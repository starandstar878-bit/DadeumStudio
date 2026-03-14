#pragma once
#include "Teul2/Runtime/AudioGraph/TGraphProcessor.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
#include <cmath>

namespace Teul::Nodes {

namespace SourceNodeHelpers {

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

static float sampleWaveform(int waveform, float phase) {
    switch (waveform) {
        case 1:  return 1.0f - 4.0f * std::abs(phase - 0.5f); // Triangle
        case 2:  return phase < 0.5f ? 1.0f : -1.0f;         // Square
        case 3:  return 1.0f - 2.0f * phase;                 // Saw
        case 0:
        default: return std::sin(phase * juce::MathConstants<float>::twoPi); // Sine
    }
}

} // namespace SourceNodeHelpers

class OscillatorNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Source.Oscillator";
        desc.displayName = "Oscillator";
        desc.category = "Source";
        desc.capabilities.canMute = true;
        desc.capabilities.maxPolyphony = 8;

        auto waveform = makeEnumParamSpec("waveform", "Waveform", 0,
            { makeParamOption("sine", "Sine", 0),
              makeParamOption("triangle", "Triangle", 1),
              makeParamOption("square", "Square", 2),
              makeParamOption("saw", "Saw", 3) },
            "Tone");
        waveform.showInNodeBody = true;

        auto frequency = makeFloatParamSpec("frequency", "Frequency", 440.0f, 20.0f, 20000.0f, 0.01f, "Hz");
        frequency.showInNodeBody = true;

        auto gain = makeFloatParamSpec("gain", "Gain", 0.707f, 0.0f, 1.0f, 0.001f);
        gain.showInNodeBody = true;

        desc.paramSpecs = { waveform, frequency, gain };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "V/Oct"),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "Sync"),
            makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void prepareToPlay(double newSampleRate, int) override {
                sampleRate = juce::jmax(1.0, newSampleRate);
            }

            void reset() override { phase = 0.0f; }

            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "waveform")      waveform = juce::jlimit(0, 3, juce::roundToInt(newValue));
                else if (paramKey == "frequency") frequency = juce::jlimit(20.0f, 20000.0f, newValue);
                else if (paramKey == "gain")      gain = juce::jlimit(0.0f, 1.0f, newValue);
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;

                const int outputChannel = SourceNodeHelpers::findPortChannel(ctx, "Out");
                if (outputChannel < 0) return;

                const int pitchChannel = SourceNodeHelpers::findPortChannel(ctx, "V/Oct");
                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                auto* output = ctx.globalPortBuffer->getWritePointer(outputChannel);
                const float* pitch = pitchChannel >= 0 ? ctx.globalPortBuffer->getReadPointer(pitchChannel) : nullptr;

                for (int i = 0; i < numSamples; ++i) {
                    float currentFreq = frequency;
                    if (pitch) currentFreq *= std::pow(2.0f, pitch[i]);
                    currentFreq = juce::jlimit(0.0f, (float)(sampleRate * 0.45), currentFreq);
                    
                    output[i] = SourceNodeHelpers::sampleWaveform(waveform, phase) * gain;
                    phase += currentFreq / (float)sampleRate;
                    phase -= std::floor(phase);
                }
            }

        private:
            double sampleRate = 48000.0;
            int waveform = 0;
            float frequency = 440.0f;
            float gain = 0.707f;
            float phase = 0.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(OscillatorNode);

class LFONode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Source.LFO";
        desc.displayName = "LFO";
        desc.category = "Source";

        auto waveform = makeEnumParamSpec("waveform", "Waveform", 0,
            { makeParamOption("sine", "Sine", 0),
              makeParamOption("triangle", "Triangle", 1),
              makeParamOption("square", "Square", 2),
              makeParamOption("saw", "Saw", 3) },
            "Tone");

        auto rate = makeFloatParamSpec("rate", "Rate", 1.0f, 0.01f, 20.0f, 0.01f, "Hz");
        rate.showInNodeBody = true;

        desc.paramSpecs = { waveform, rate };
        desc.portSpecs = { makePortSpec(TPortDirection::Output, TPortDataType::CV, "Out") };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void prepareToPlay(double newSampleRate, int) override {
                sampleRate = juce::jmax(1.0, newSampleRate);
            }
            void reset() override { phase = 0.0f; }
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "waveform")      waveform = juce::jlimit(0, 3, juce::roundToInt(newValue));
                else if (paramKey == "rate")      rate = juce::jlimit(0.01f, 20.0f, newValue);
            }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int outputChannel = SourceNodeHelpers::findPortChannel(ctx, "Out");
                if (outputChannel < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                auto* output = ctx.globalPortBuffer->getWritePointer(outputChannel);
                for (int i = 0; i < numSamples; ++i) {
                    output[i] = SourceNodeHelpers::sampleWaveform(waveform, phase);
                    phase += rate / (float)sampleRate;
                    phase -= std::floor(phase);
                }
            }
        private:
            double sampleRate = 48000.0;
            int waveform = 0;
            float rate = 1.0f;
            float phase = 0.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(LFONode);

class NoiseGeneratorNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Source.Noise";
    desc.displayName = "Noise Generator";
    desc.category = "Source";
    desc.capabilities.canMute = true;
    desc.capabilities.isTimeDependent = true;
    desc.portSpecs = { makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"}) };
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr) return;

        int leftOutCh = SourceNodeHelpers::findPortChannel(ctx, "L Out");
        int rightOutCh = SourceNodeHelpers::findPortChannel(ctx, "R Out");

        if (leftOutCh < 0 && rightOutCh < 0) return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        float *leftOut = leftOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(leftOutCh) : nullptr;
        float *rightOut = rightOutCh >= 0 ? ctx.globalPortBuffer->getWritePointer(rightOutCh) : nullptr;

        for (int i = 0; i < numSamples; ++i) {
          if (leftOut) leftOut[i] = random.nextFloat() * 2.0f - 1.0f;
          if (rightOut) rightOut[i] = random.nextFloat() * 2.0f - 1.0f;
        }
      }
    private:
      juce::Random random;
    };
    return std::make_unique<Implementation>();
  }
};

TEUL2_NODE_REGISTER(NoiseGeneratorNode);

class ConstantNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Source.Constant";
        desc.displayName = "Constant";
        desc.category = "Source";
        desc.paramSpecs = { makeFloatParamSpec("value", "Value", 1.0f, -100.0f, 100.0f) };
        desc.portSpecs = { makePortSpec(TPortDirection::Output, TPortDataType::CV, "Value") };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if (paramKey == "value") value = newValue;
            }
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr) return;
                const int outputChannel = SourceNodeHelpers::findPortChannel(ctx, "Value");
                if (outputChannel < 0) return;
                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                auto* output = ctx.globalPortBuffer->getWritePointer(outputChannel);
                for (int i = 0; i < numSamples; ++i) output[i] = value;
            }
        private:
            float value = 1.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(ConstantNode);

} // namespace Teul::Nodes
