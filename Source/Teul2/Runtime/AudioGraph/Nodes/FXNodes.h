#pragma once

#include "Teul2/Runtime/AudioGraph/TGraphProcessor.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Teul::Nodes {

namespace FXNodeHelpers {

static int findPortChannel(const TProcessContext& ctx, juce::StringRef portName) {
    if (ctx.nodeData == nullptr || ctx.portToChannel == nullptr) return -1;
    for (const auto& port : ctx.nodeData->ports) {
        if (port.name == portName) {
            const auto it = ctx.portToChannel->find(port.portId);
            return it != ctx.portToChannel->end() ? it->second : -1;
        }
    }
    return -1;
}

} // namespace FXNodeHelpers

class ReverbNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.FX.Reverb";
        desc.displayName = "Reverb";
        desc.category = "FX";
        desc.capabilities.isTimeDependent = true;
        desc.capabilities.canBypass = true;
        desc.paramSpecs = { 
            makeFloatParamSpec("roomSize", "Room Size", 0.5f, 0.0f, 1.0f),
            makeFloatParamSpec("damping", "Damping", 0.5f, 0.0f, 1.0f),
            makeFloatParamSpec("width", "Width", 1.0f, 0.0f, 1.0f),
            makeFloatParamSpec("mix", "Mix", 0.3f, 0.0f, 1.0f)
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void prepareToPlay(double sr, int maxSamples) override {
                wetL.resize((size_t)juce::jmax(1, maxSamples));
                wetR.resize((size_t)juce::jmax(1, maxSamples));
                reverb.setSampleRate(sr);
                dirty = true;
            }

            void setParameterValue(const juce::String& k, float v) override {
                if      (k == "roomSize") roomSize = juce::jlimit(0.f, 1.f, v);
                else if (k == "damping")  damping  = juce::jlimit(0.f, 1.f, v);
                else if (k == "width")    width    = juce::jlimit(0.f, 1.f, v);
                else if (k == "mix")      mix      = juce::jlimit(0.f, 1.f, v);
                dirty = true;
            }

            void reset() override { reverb.reset(); }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int lInCh = FXNodeHelpers::findPortChannel(ctx, "L In");
                const int rInCh = FXNodeHelpers::findPortChannel(ctx, "R In");
                const int lOutCh = FXNodeHelpers::findPortChannel(ctx, "L Out");
                const int rOutCh = FXNodeHelpers::findPortChannel(ctx, "R Out");
                if (lOutCh < 0 && rOutCh < 0) return;

                if (dirty) {
                    juce::Reverb::Parameters p;
                    p.roomSize = roomSize; p.damping = damping; p.width = width;
                    p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.freezeMode = 0.0f;
                    reverb.setParameters(p);
                    dirty = false;
                }

                const int n = ctx.globalPortBuffer->getNumSamples();
                if ((int)wetL.size() < n) { wetL.resize((size_t)n); wetR.resize((size_t)n); }

                const float* pLIn = lInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(lInCh) : nullptr;
                const float* pRIn = rInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(rInCh) : pLIn;

                for (int i = 0; i < n; ++i) {
                    wetL[(size_t)i] = pLIn ? pLIn[i] : 0.0f;
                    wetR[(size_t)i] = pRIn ? pRIn[i] : wetL[(size_t)i];
                }

                reverb.processStereo(wetL.data(), wetR.data(), n);

                const float dry = 1.0f - mix;
                float* pLOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                float* pROut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;

                for (int i = 0; i < n; ++i) {
                    float sLDr = pLIn ? pLIn[i] : 0.0f;
                    float sRDr = pRIn ? pRIn[i] : sLDr;
                    if (pLOut) pLOut[i] = sLDr * dry + wetL[(size_t)i] * mix;
                    if (pROut) pROut[i] = sRDr * dry + wetR[(size_t)i] * mix;
                }
            }
        private:
            juce::Reverb reverb;
            std::vector<float> wetL, wetR;
            float roomSize = 0.5f, damping = 0.5f, width = 1.0f, mix = 0.3f;
            bool dirty = true;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(ReverbNode);

class DelayNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.FX.Delay";
        desc.displayName = "Delay";
        desc.category = "FX";
        desc.capabilities.isTimeDependent = true;
        desc.capabilities.canBypass = true;
        desc.paramSpecs = { 
            makeFloatParamSpec("time", "Time (ms)", 250.0f, 1.0f, 5000.0f),
            makeFloatParamSpec("feedback", "Feedback", 0.3f, 0.0f, 0.99f),
            makeFloatParamSpec("mix", "Mix", 0.5f, 0.0f, 1.0f)
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void prepareToPlay(double sr, int maxBlock) override {
                sampleRate = sr;
                int maxD = (int)std::ceil(sr * 5.0) + maxBlock + 1;
                dL.assign((size_t)maxD, 0.f); dR.assign((size_t)maxD, 0.f);
                wIdx = 0;
            }
            void reset() override { std::fill(dL.begin(), dL.end(), 0.f); std::fill(dR.begin(), dR.end(), 0.f); wIdx = 0; }
            void setParameterValue(const juce::String& k, float v) override {
                if      (k == "time")     tMs = juce::jlimit(1.f, 5000.f, v);
                else if (k == "feedback") fb  = juce::jlimit(0.f, 0.99f, v);
                else if (k == "mix")      mix = juce::jlimit(0.f, 1.f, v);
            }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || dL.empty()) return;
                int lIn = FXNodeHelpers::findPortChannel(ctx, "L In");
                int rIn = FXNodeHelpers::findPortChannel(ctx, "R In");
                int lOut = FXNodeHelpers::findPortChannel(ctx, "L Out");
                int rOut = FXNodeHelpers::findPortChannel(ctx, "R Out");
                if (lOut < 0 && rOut < 0) return;
                const int n = ctx.globalPortBuffer->getNumSamples();
                const int dSamples = juce::jlimit(1, (int)dL.size() - 1, (int)std::round((tMs / 1000.f) * sampleRate));
                const float dry = 1.0f - mix;
                for (int i = 0; i < n; ++i) {
                    float sL = lIn >= 0 ? ctx.globalPortBuffer->getReadPointer(lIn)[i] : 0;
                    float sR = rIn >= 0 ? ctx.globalPortBuffer->getReadPointer(rIn)[i] : sL;
                    int rIdx = (wIdx + (int)dL.size() - dSamples) % (int)dL.size();
                    float dLS = dL[(size_t)rIdx], dRS = dR[(size_t)rIdx];
                    dL[(size_t)wIdx] = sL + dLS * fb; dR[(size_t)wIdx] = sR + dRS * fb;
                    if (lOut >= 0) ctx.globalPortBuffer->getWritePointer(lOut)[i] = sL * dry + dLS * mix;
                    if (rOut >= 0) ctx.globalPortBuffer->getWritePointer(rOut)[i] = sR * dry + dRS * mix;
                    wIdx = (wIdx + 1) % (int)dL.size();
                }
            }
        private:
            std::vector<float> dL, dR;
            double sampleRate = 48000.0;
            float tMs = 250.f, fb = 0.3f, mix = 0.5f;
            int wIdx = 0;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(DelayNode);

} // namespace Teul::Nodes
