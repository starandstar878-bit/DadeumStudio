#pragma once

#include "Teul2/Runtime/AudioGraph/TNodeInstance.h"
#include "Teul2/Runtime/AudioGraph/TNodeSDK.h"

namespace Teul::Nodes {

class ADSRNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Mod.ADSR";
        desc.displayName = "ADSR Envelope";
        desc.category = "Modulation";

        desc.paramSpecs = {
            makeFloatParamSpec("attack",  "Attack",  10.0f,  0.0f, 5000.0f, 0.1f, "ms"),
            makeFloatParamSpec("decay",   "Decay",   100.0f, 0.0f, 5000.0f, 0.1f, "ms"),
            makeFloatParamSpec("sustain", "Sustain", 0.5f,   0.0f, 1.0f,   0.001f),
            makeFloatParamSpec("release", "Release", 300.0f, 0.0f, 5000.0f, 0.1f, "ms")
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Gate, "Gate"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "Env")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void prepareToPlay(double sr, int) override {
                adsr.setSampleRate(sr);
                updateParams();
            }
            void reset() override { adsr.reset(); gateActive = false; }
            void setParameterValue(const juce::String& k, float v) override {
                if      (k == "attack")  aMs = juce::jlimit(0.f, 5000.f, v);
                else if (k == "decay")   dMs = juce::jlimit(0.f, 5000.f, v);
                else if (k == "sustain") sus = juce::jlimit(0.f, 1.f, v);
                else if (k == "release") rMs = juce::jlimit(0.f, 5000.f, v);
                updateParams();
            }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr) return;
                int gCh = -1, eCh = -1;
                for (const auto& p : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(p.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (p.name == "Gate") gCh = it->second;
                    else if (p.name == "Env")  eCh = it->second;
                }
                if (eCh < 0) return;
                const int n = ctx.globalPortBuffer->getNumSamples();
                const float* gIn = gCh >= 0 ? ctx.globalPortBuffer->getReadPointer(gCh) : nullptr;
                float* eOut = ctx.globalPortBuffer->getWritePointer(eCh);
                for (int i = 0; i < n; ++i) {
                    bool curG = gIn ? gIn[i] > 0.5f : false;
                    if (curG && !gateActive) adsr.noteOn(); else if (!curG && gateActive) adsr.noteOff();
                    gateActive = curG;
                    eOut[i] = adsr.getNextSample();
                }
            }
        private:
            void updateParams() {
                juce::ADSR::Parameters p;
                p.attack = aMs / 1000.f; p.decay = dMs / 1000.f; p.sustain = sus; p.release = rMs / 1000.f;
                adsr.setParameters(p);
            }
            juce::ADSR adsr;
            float aMs = 10.f, dMs = 100.f, sus = 0.5f, rMs = 300.f;
            bool gateActive = false;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(ADSRNode);

} // namespace Teul::Nodes
