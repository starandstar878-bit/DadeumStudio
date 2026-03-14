#pragma once

#include "Teul2/Runtime/AudioGraph/TGraphProcessor.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
#include <array>
#include <cmath>

namespace Teul::Nodes {

namespace MixerNodeHelpers {

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

static int findNthAudioInputChannel(const TProcessContext& ctx, int inputIndex) {
    if (ctx.nodeData == nullptr || ctx.portToChannel == nullptr) return -1;
    int currentIndex = 0;
    for (const auto& port : ctx.nodeData->ports) {
        if (port.direction != TPortDirection::Input || port.dataType != TPortDataType::Audio) continue;
        if (currentIndex == inputIndex) {
            const auto it = ctx.portToChannel->find(port.portId);
            return it != ctx.portToChannel->end() ? it->second : -1;
        }
        ++currentIndex;
    }
    return -1;
}

} // namespace MixerNodeHelpers

class VCANode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Mixer.VCA";
        desc.displayName = "VCA";
        desc.category = "Mixer";
        desc.capabilities.canMute = true;

        auto gain = makeFloatParamSpec("gain", "Gain", 1.0f, 0.0f, 2.0f, 0.001f);
        gain.showInNodeBody = true;
        desc.paramSpecs = { gain };

        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "CV"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "gain") gain = juce::jlimit(0.0f, 2.0f, newValue);
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;

                int lInCh = MixerNodeHelpers::findPortChannel(ctx, "L In");
                int rInCh = MixerNodeHelpers::findPortChannel(ctx, "R In");
                const int cvCh = MixerNodeHelpers::findPortChannel(ctx, "CV");
                const int lOutCh = MixerNodeHelpers::findPortChannel(ctx, "L Out");
                const int rOutCh = MixerNodeHelpers::findPortChannel(ctx, "R Out");

                if (lOutCh < 0 && rOutCh < 0) return;

                if (lInCh < 0) lInCh = MixerNodeHelpers::findNthAudioInputChannel(ctx, 0);
                if (rInCh < 0) rInCh = MixerNodeHelpers::findNthAudioInputChannel(ctx, 1);

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                auto* lOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                auto* rOut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;
                const float* lIn = lInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(lInCh) : nullptr;
                const float* rIn = rInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(rInCh) : lIn;
                const float* cv = cvCh >= 0 ? ctx.globalPortBuffer->getReadPointer(cvCh) : nullptr;

                for (int i = 0; i < numSamples; ++i) {
                    const float mod = cv ? juce::jmax(0.0f, cv[i]) : 1.0f;
                    const float lSrc = lIn ? lIn[i] : 0.0f;
                    const float rSrc = rIn ? rIn[i] : lSrc;
                    if (lOut) lOut[i] = lSrc * gain * mod;
                    if (rOut) rOut[i] = rSrc * gain * mod;
                }
            }
        private:
            float gain = 1.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(VCANode);

class StereoPannerNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Mixer.StereoPanner";
        desc.displayName = "Stereo Panner";
        desc.category = "Mixer";
        desc.capabilities.canMute = true;

        auto pan = makeFloatParamSpec("pan", "Pan", 0.0f, -1.0f, 1.0f, 0.001f);
        pan.showInNodeBody = true;
        desc.paramSpecs = { pan };

        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In"),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "Pan CV"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "pan") pan = juce::jlimit(-1.0f, 1.0f, newValue);
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;

                const int inCh = MixerNodeHelpers::findPortChannel(ctx, "In");
                const int cvCh = MixerNodeHelpers::findPortChannel(ctx, "Pan CV");
                const int lCh = MixerNodeHelpers::findPortChannel(ctx, "L Out");
                const int rCh = MixerNodeHelpers::findPortChannel(ctx, "R Out");

                if (lCh < 0 || rCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* input = inCh >= 0 ? ctx.globalPortBuffer->getReadPointer(inCh) : nullptr;
                const float* cv = cvCh >= 0 ? ctx.globalPortBuffer->getReadPointer(cvCh) : nullptr;
                auto* left = ctx.globalPortBuffer->getWritePointer(lCh);
                auto* right = ctx.globalPortBuffer->getWritePointer(rCh);

                for (int i = 0; i < numSamples; ++i) {
                    const float src = input ? input[i] : 0.0f;
                    const float cPan = juce::jlimit(-1.0f, 1.0f, pan + (cv ? cv[i] : 0.0f));
                    const float angle = (cPan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
                    left[i] = src * std::cos(angle);
                    right[i] = src * std::sin(angle);
                }
            }
        private:
            float pan = 0.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(StereoPannerNode);

class Mixer2Node final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Mixer.Mixer2";
        desc.displayName = "Mixer (2-Ch)";
        desc.category = "Mixer";
        desc.capabilities.canMute = true;
        desc.paramSpecs = { 
            makeFloatParamSpec("gain1", "Gain 1", 1.0f, 0.0f, 2.0f),
            makeFloatParamSpec("gain2", "Gain 2", 1.0f, 0.0f, 2.0f)
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 1"),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 2"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if      (paramKey == "gain1") gain1 = juce::jlimit(0.0f, 2.0f, newValue);
                else if (paramKey == "gain2") gain2 = juce::jlimit(0.0f, 2.0f, newValue);
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int in1Ch = MixerNodeHelpers::findPortChannel(ctx, "In 1");
                const int in2Ch = MixerNodeHelpers::findPortChannel(ctx, "In 2");
                const int outCh = MixerNodeHelpers::findPortChannel(ctx, "Out");
                if (outCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* in1 = in1Ch >= 0 ? ctx.globalPortBuffer->getReadPointer(in1Ch) : nullptr;
                const float* in2 = in2Ch >= 0 ? ctx.globalPortBuffer->getReadPointer(in2Ch) : nullptr;
                auto* output = ctx.globalPortBuffer->getWritePointer(outCh);

                for (int i = 0; i < numSamples; ++i) {
                    const float a = in1 ? in1[i] : 0.0f;
                    const float b = in2 ? in2[i] : 0.0f;
                    output[i] = a * gain1 + b * gain2;
                }
            }
        private:
            float gain1 = 1.0f, gain2 = 1.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(Mixer2Node);

class MonoMixer4Node final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Mixer.MonoMixer4";
        desc.displayName = "Mono Mixer (4-In)";
        desc.category = "Mixer";
        desc.capabilities.canMute = true;

        for (int i = 0; i < 4; ++i) {
            desc.paramSpecs.push_back(makeFloatParamSpec("gain" + juce::String(i + 1), "Gain " + juce::String(i + 1), 1.0f, 0.0f, 2.0f));
        }
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 1"),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 2"),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 3"),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 4"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                for (int i = 0; i < 4; ++i) {
                    if (paramKey == "gain" + juce::String(i + 1)) {
                        gains[(size_t)i] = juce::jlimit(0.0f, 2.0f, newValue);
                        return;
                    }
                }
            }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int outCh = MixerNodeHelpers::findPortChannel(ctx, "Out");
                if (outCh < 0) return;

                std::array<const float*, 4> ins{};
                for (int i = 0; i < 4; ++i) {
                    const int ch = MixerNodeHelpers::findPortChannel(ctx, "In " + juce::String(i + 1));
                    ins[(size_t)i] = ch >= 0 ? ctx.globalPortBuffer->getReadPointer(ch) : nullptr;
                }

                auto* output = ctx.globalPortBuffer->getWritePointer(outCh);
                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                for (int i = 0; i < numSamples; ++i) {
                    float mix = 0.0f;
                    for (size_t k = 0; k < 4; ++k) if (ins[k]) mix += ins[k][i] * gains[k];
                    output[i] = mix;
                }
            }
        private:
            std::array<float, 4> gains{{1.0f, 1.0f, 1.0f, 1.0f}};
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(MonoMixer4Node);

class StereoMixer4Node final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Mixer.StereoMixer4";
        desc.displayName = "Stereo Mixer (4-Bus)";
        desc.category = "Mixer";
        desc.capabilities.canMute = true;

        for (int i = 0; i < 4; ++i) {
            desc.paramSpecs.push_back(makeFloatParamSpec("gain" + juce::String(i + 1), "Gain " + juce::String(i + 1), 1.0f, 0.0f, 2.0f));
        }
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 1", 2, {"L In 1", "R In 1"}),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 2", 2, {"L In 2", "R In 2"}),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 3", 2, {"L In 3", "R In 3"}),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In 4", 2, {"L In 4", "R In 4"}),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                for (int i = 0; i < 4; ++i) {
                    if (paramKey == "gain" + juce::String(i + 1)) {
                        gains[(size_t)i] = juce::jlimit(0.0f, 2.0f, newValue);
                        return;
                    }
                }
            }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int lOutCh = MixerNodeHelpers::findPortChannel(ctx, "L Out");
                const int rOutCh = MixerNodeHelpers::findPortChannel(ctx, "R Out");
                if (lOutCh < 0 && rOutCh < 0) return;

                std::array<const float*, 4> lIns{}, rIns{};
                for (int i = 0; i < 4; ++i) {
                    lIns[(size_t)i] = ctx.globalPortBuffer->getReadPointer(MixerNodeHelpers::findPortChannel(ctx, "L In " + juce::String(i + 1)));
                    rIns[(size_t)i] = ctx.globalPortBuffer->getReadPointer(MixerNodeHelpers::findPortChannel(ctx, "R In " + juce::String(i + 1)));
                }

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                auto* lOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                auto* rOut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;

                for (int i = 0; i < numSamples; ++i) {
                    float lMix = 0, rMix = 0;
                    for (size_t k = 0; k < 4; ++k) {
                        if (lIns[k]) lMix += lIns[k][i] * gains[k];
                        if (rIns[k]) rMix += rIns[k][i] * gains[k];
                    }
                    if (lOut) lOut[i] = lMix;
                    if (rOut) rOut[i] = rMix;
                }
            }
        private:
            std::array<float, 4> gains{{1.0f, 1.0f, 1.0f, 1.0f}};
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(StereoMixer4Node);

class StereoSplitterNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Routing.StereoSplitter";
        desc.displayName = "Stereo Splitter";
        desc.category = "Routing";
        desc.capabilities.canMute = true;
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "L Out"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "R Out")
        };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int lInCh = MixerNodeHelpers::findPortChannel(ctx, "L In");
                const int rInCh = MixerNodeHelpers::findPortChannel(ctx, "R In");
                const int lOutCh = MixerNodeHelpers::findPortChannel(ctx, "L Out");
                const int rOutCh = MixerNodeHelpers::findPortChannel(ctx, "R Out");
                if (lOutCh < 0 && rOutCh < 0) return;
                const int n = ctx.globalPortBuffer->getNumSamples();
                const float* lIn = lInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(lInCh) : nullptr;
                const float* rIn = rInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(rInCh) : lIn;
                float* lOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                float* rOut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;
                for (int i = 0; i < n; ++i) {
                    if (lOut) lOut[i] = lIn ? lIn[i] : 0;
                    if (rOut) rOut[i] = rIn ? rIn[i] : 0;
                }
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(StereoSplitterNode);

class StereoMergerNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Routing.StereoMerger";
        desc.displayName = "Stereo Merger";
        desc.category = "Routing";
        desc.capabilities.canMute = true;
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "L In"),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "R In"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int lInCh = MixerNodeHelpers::findPortChannel(ctx, "L In");
                const int rInCh = MixerNodeHelpers::findPortChannel(ctx, "R In");
                const int lOutCh = MixerNodeHelpers::findPortChannel(ctx, "L Out");
                const int rOutCh = MixerNodeHelpers::findPortChannel(ctx, "R Out");
                if (lOutCh < 0 && rOutCh < 0) return;
                const int n = ctx.globalPortBuffer->getNumSamples();
                const float* lIn = lInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(lInCh) : nullptr;
                const float* rIn = rInCh >= 0 ? ctx.globalPortBuffer->getReadPointer(rInCh) : nullptr;
                float* lOut = lOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(lOutCh) : nullptr;
                float* rOut = rOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rOutCh) : nullptr;
                for (int i = 0; i < n; ++i) {
                    float sL = lIn ? lIn[i] : 0;
                    float sR = rIn ? rIn[i] : sL;
                    if (lOut) lOut[i] = sL;
                    if (rOut) rOut[i] = sR;
                }
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(StereoMergerNode);

class CrossfaderNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Mixer.Crossfader";
        desc.displayName = "Crossfader";
        desc.category = "Mixer";
        desc.capabilities.canMute = true;
        auto mixParam = makeFloatParamSpec("mix", "Mix", 0.5f, 0.0f, 1.0f, 0.001f);
        mixParam.showInNodeBody = true;
        desc.paramSpecs = { mixParam };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "A", 2, {"L In A", "R In A"}),
            makePortSpec(TPortDirection::Input, TPortDataType::Audio, "B", 2, {"L In B", "R In B"}),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "Mix CV"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})
        };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& k, float v) override {
                if (k == "mix") m = juce::jlimit(0.f, 1.f, v);
            }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int lA = MixerNodeHelpers::findPortChannel(ctx, "L In A");
                const int rA = MixerNodeHelpers::findPortChannel(ctx, "R In A");
                const int lB = MixerNodeHelpers::findPortChannel(ctx, "L In B");
                const int rB = MixerNodeHelpers::findPortChannel(ctx, "R In B");
                const int cvCh = MixerNodeHelpers::findPortChannel(ctx, "Mix CV");
                const int lO = MixerNodeHelpers::findPortChannel(ctx, "L Out");
                const int rO = MixerNodeHelpers::findPortChannel(ctx, "R Out");
                if (lO < 0 && rO < 0) return;
                const int n = ctx.globalPortBuffer->getNumSamples();
                const float* pCv = cvCh >= 0 ? ctx.globalPortBuffer->getReadPointer(cvCh) : nullptr;
                for (int i = 0; i < n; ++i) {
                    float curM = juce::jlimit(0.f, 1.f, m + (pCv ? pCv[i] : 0));
                    float ang = curM * juce::MathConstants<float>::halfPi;
                    float gA = std::cos(ang), gB = std::sin(ang);
                    if (lO >= 0) {
                        float sA = lA >= 0 ? ctx.globalPortBuffer->getReadPointer(lA)[i] : 0;
                        float sB = lB >= 0 ? ctx.globalPortBuffer->getReadPointer(lB)[i] : 0;
                        ctx.globalPortBuffer->getWritePointer(lO)[i] = sA * gA + sB * gB;
                    }
                    if (rO >= 0) {
                        float sA = rA >= 0 ? ctx.globalPortBuffer->getReadPointer(rA)[i] : 0;
                        float sB = rB >= 0 ? ctx.globalPortBuffer->getReadPointer(rB)[i] : 0;
                        ctx.globalPortBuffer->getWritePointer(rO)[i] = sA * gA + sB * gB;
                    }
                }
            }
        private:
            float m = 0.5f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(CrossfaderNode);

class AudioInputNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Source.AudioInput";
        desc.displayName = "Audio Input";
        desc.category = "Source";
        desc.capabilities.canMute = false;
        desc.portSpecs = { makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"}) };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.inputAudioBuffer == nullptr) return;
                int lCh = MixerNodeHelpers::findPortChannel(ctx, "L Out");
                int rCh = MixerNodeHelpers::findPortChannel(ctx, "R Out");
                int inCount = ctx.inputAudioBuffer->getNumChannels();
                int n = juce::jmin(ctx.globalPortBuffer->getNumSamples(), ctx.inputAudioBuffer->getNumSamples());
                if (n <= 0 || inCount <= 0) return;
                const float* pL = ctx.inputAudioBuffer->getReadPointer(0);
                const float* pR = inCount > 1 ? ctx.inputAudioBuffer->getReadPointer(1) : pL;
                if (lCh >= 0) juce::FloatVectorOperations::copy(ctx.globalPortBuffer->getWritePointer(lCh), pL, n);
                if (rCh >= 0) juce::FloatVectorOperations::copy(ctx.globalPortBuffer->getWritePointer(rCh), pR, n);
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(AudioInputNode);

class AudioOutputNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Routing.AudioOut";
        desc.displayName = "Audio Output";
        desc.category = "Routing";
        desc.capabilities.canMute = true;
        desc.paramSpecs = { makeFloatParamSpec("volume", "Volume", 1.0f, 0.0f, 2.0f) };
        desc.portSpecs = { makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}) };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& k, float v) override { if (k == "volume") vol = juce::jlimit(0.f, 2.f, v); }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.deviceAudioBuffer == nullptr) return;
                int lIn = MixerNodeHelpers::findPortChannel(ctx, "L In");
                int rIn = MixerNodeHelpers::findPortChannel(ctx, "R In");
                if (lIn < 0) lIn = MixerNodeHelpers::findNthAudioInputChannel(ctx, 0);
                if (rIn < 0) rIn = MixerNodeHelpers::findNthAudioInputChannel(ctx, 1);
                int n = juce::jmin(ctx.globalPortBuffer->getNumSamples(), ctx.deviceAudioBuffer->getNumSamples());
                if (n <= 0 || ctx.deviceAudioBuffer->getNumChannels() <= 0) return;
                const float* pL = lIn >= 0 ? ctx.globalPortBuffer->getReadPointer(lIn) : nullptr;
                const float* pR = rIn >= 0 ? ctx.globalPortBuffer->getReadPointer(rIn) : pL;
                float* outL = ctx.deviceAudioBuffer->getWritePointer(0);
                float* outR = ctx.deviceAudioBuffer->getNumChannels() > 1 ? ctx.deviceAudioBuffer->getWritePointer(1) : nullptr;
                for (int i = 0; i < n; ++i) {
                    float sL = (pL ? pL[i] : 0) * vol;
                    float sR = (pR ? pR[i] : sL) * vol;
                    outL[i] += sL;
                    if (outR) outR[i] += sR;
                }
            }
        private:
            float vol = 1.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(AudioOutputNode);

} // namespace Teul::Nodes
