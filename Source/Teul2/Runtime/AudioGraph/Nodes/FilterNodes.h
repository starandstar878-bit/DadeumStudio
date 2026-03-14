#pragma once

#include "Teul2/Runtime/AudioGraph/TNodeInstance.h"
#include "Teul2/Runtime/AudioGraph/TNodeSDK.h"
#include <juce_dsp/juce_dsp.h>

namespace Teul::Nodes {

class LowPassFilterNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Filter.LowPass";
        desc.displayName = "LowPass Filter";
        desc.category = "Filter";

        auto cutoff = makeFloatParamSpec("cutoff", "Cutoff", 1000.0f, 20.0f, 20000.0f, 0.01f, "Hz");
        cutoff.showInNodeBody = true;

        auto resonance = makeFloatParamSpec("resonance", "Resonance", 0.707f, 0.1f, 12.0f, 0.001f);

        desc.paramSpecs = { cutoff, resonance };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "Freq CV"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            Implementation() {
                filters[0].reset();
                filters[1].reset();
            }

            void prepareToPlay(double sr, int) override {
                sampleRate = sr;
                updateCoefficients(cutoffFreq);
            }

            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "cutoff") {
                    cutoffFreq = juce::jlimit(20.0f, 20000.0f, newValue);
                    mustUpdateCoefficients = true;
                } else if (paramKey == "resonance") {
                    resonance = juce::jlimit(0.1f, 12.0f, newValue);
                    mustUpdateCoefficients = true;
                }
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int lInCh = -1, rInCh = -1, cvCh = -1, lOutCh = -1, rOutCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "L In")    lInCh = it->second;
                    else if (port.name == "R In")    rInCh = it->second;
                    else if (port.name == "Freq CV") cvCh = it->second;
                    else if (port.name == "L Out")   lOutCh = it->second;
                    else if (port.name == "R Out")   rOutCh = it->second;
                }

                if (lOutCh < 0 && rOutCh < 0) return;

                if (mustUpdateCoefficients) {
                    updateCoefficients(cutoffFreq);
                    mustUpdateCoefficients = false;
                }

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* lIn = lInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(lInCh) : nullptr;
                const float* rIn = rInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(rInCh) : lIn;
                const float* cv = cvCh  >= 0 ? ctx.globalPortBuffer->getReadPointer(cvCh)  : nullptr;
                
                float* lOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                float* rOut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;

                for (int i = 0; i < numSamples; ++i) {
                    if (cv != nullptr) {
                        const float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, cutoffFreq * std::pow(2.0f, cv[i]));
                        updateCoefficients(modulatedCutoff);
                    }

                    if (lOut != nullptr) lOut[i] = filters[0].processSample(lIn != nullptr ? lIn[i] : 0.0f);
                    if (rOut != nullptr) rOut[i] = filters[1].processSample(rIn != nullptr ? rIn[i] : 0.0f);
                }
            }

        private:
            void updateCoefficients(float freq) {
                if (sampleRate <= 0.0) return;
                auto coefs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, freq, resonance);
                filters[0].coefficients = coefs;
                filters[1].coefficients = coefs;
            }

            double sampleRate = 48000.0;
            float cutoffFreq = 1000.0f;
            float resonance = 0.707f;
            bool mustUpdateCoefficients = true;
            juce::dsp::IIR::Filter<float> filters[2];
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(LowPassFilterNode);

class HighPassFilterNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Filter.HighPass";
        desc.displayName = "HighPass Filter";
        desc.category = "Filter";

        desc.paramSpecs = { 
            makeFloatParamSpec("cutoff", "Cutoff", 1000.0f, 20.0f, 20000.0f, 0.01f, "Hz"),
            makeFloatParamSpec("resonance", "Resonance", 0.707f, 0.1f, 12.0f, 0.001f)
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "Freq CV"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            Implementation() {
                filters[0].reset();
                filters[1].reset();
            }

            void prepareToPlay(double sr, int) override {
                sampleRate = sr;
                updateCoefficients(cutoffFreq);
            }

            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "cutoff") {
                    cutoffFreq = juce::jlimit(20.0f, 20000.0f, newValue);
                    mustUpdateCoefficients = true;
                } else if (paramKey == "resonance") {
                    resonance = juce::jlimit(0.1f, 12.0f, newValue);
                    mustUpdateCoefficients = true;
                }
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int lInCh = -1, rInCh = -1, cvCh = -1, lOutCh = -1, rOutCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "L In")    lInCh = it->second;
                    else if (port.name == "R In")    rInCh = it->second;
                    else if (port.name == "Freq CV") cvCh = it->second;
                    else if (port.name == "L Out")   lOutCh = it->second;
                    else if (port.name == "R Out")   rOutCh = it->second;
                }

                if (lOutCh < 0 && rOutCh < 0) return;

                if (mustUpdateCoefficients) {
                    updateCoefficients(cutoffFreq);
                    mustUpdateCoefficients = false;
                }

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* lIn = lInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(lInCh) : nullptr;
                const float* rIn = rInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(rInCh) : lIn;
                const float* cv = cvCh  >= 0 ? ctx.globalPortBuffer->getReadPointer(cvCh)  : nullptr;
                
                float* lOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                float* rOut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;

                for (int i = 0; i < numSamples; ++i) {
                    if (cv != nullptr) {
                        const float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, cutoffFreq * std::pow(2.0f, cv[i]));
                        updateCoefficients(modulatedCutoff);
                    }

                    if (lOut != nullptr) lOut[i] = filters[0].processSample(lIn != nullptr ? lIn[i] : 0.0f);
                    if (rOut != nullptr) rOut[i] = filters[1].processSample(rIn != nullptr ? rIn[i] : 0.0f);
                }
            }

        private:
            void updateCoefficients(float freq) {
                if (sampleRate <= 0.0) return;
                auto coefs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, freq, resonance);
                filters[0].coefficients = coefs;
                filters[1].coefficients = coefs;
            }

            double sampleRate = 48000.0;
            float cutoffFreq = 1000.0f;
            float resonance = 0.707f;
            bool mustUpdateCoefficients = true;
            juce::dsp::IIR::Filter<float> filters[2];
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(HighPassFilterNode);

class BandPassFilterNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Filter.BandPass";
        desc.displayName = "BandPass Filter";
        desc.category = "Filter";

        desc.paramSpecs = { 
            makeFloatParamSpec("cutoff", "Cutoff", 1000.0f, 20.0f, 20000.0f, 0.01f, "Hz"),
            makeFloatParamSpec("q", "Q Factor", 1.0f, 0.1f, 12.0f)
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "Freq CV"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            Implementation() {
                filters[0].reset();
                filters[1].reset();
            }

            void prepareToPlay(double sr, int) override {
                sampleRate = sr;
                updateCoefficients(cutoffFreq);
            }

            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "cutoff") {
                    cutoffFreq = juce::jlimit(20.0f, 20000.0f, newValue);
                    mustUpdateCoefficients = true;
                } else if (paramKey == "q") {
                    qFactor = juce::jlimit(0.1f, 12.0f, newValue);
                    mustUpdateCoefficients = true;
                }
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int lInCh = -1, rInCh = -1, cvCh = -1, lOutCh = -1, rOutCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "L In")    lInCh = it->second;
                    else if (port.name == "R In")    rInCh = it->second;
                    else if (port.name == "Freq CV") cvCh = it->second;
                    else if (port.name == "L Out")   lOutCh = it->second;
                    else if (port.name == "R Out")   rOutCh = it->second;
                }

                if (lOutCh < 0 && rOutCh < 0) return;

                if (mustUpdateCoefficients) {
                    updateCoefficients(cutoffFreq);
                    mustUpdateCoefficients = false;
                }

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* lIn = lInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(lInCh) : nullptr;
                const float* rIn = rInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(rInCh) : lIn;
                const float* cv = cvCh  >= 0 ? ctx.globalPortBuffer->getReadPointer(cvCh)  : nullptr;
                
                float* lOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                float* rOut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;

                for (int i = 0; i < numSamples; ++i) {
                    if (cv != nullptr) {
                        const float modulatedCutoff = juce::jlimit(20.0f, 20000.0f, cutoffFreq * std::pow(2.0f, cv[i]));
                        updateCoefficients(modulatedCutoff);
                    }

                    if (lOut != nullptr) lOut[i] = filters[0].processSample(lIn != nullptr ? lIn[i] : 0.0f);
                    if (rOut != nullptr) rOut[i] = filters[1].processSample(rIn != nullptr ? rIn[i] : 0.0f);
                }
            }

        private:
            void updateCoefficients(float freq) {
                if (sampleRate <= 0.0) return;
                auto coefs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, freq, qFactor);
                filters[0].coefficients = coefs;
                filters[1].coefficients = coefs;
            }

            double sampleRate = 48000.0;
            float cutoffFreq = 1000.0f;
            float qFactor = 1.0f;
            bool mustUpdateCoefficients = true;
            juce::dsp::IIR::Filter<float> filters[2];
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(BandPassFilterNode);

} // namespace Teul::Nodes
