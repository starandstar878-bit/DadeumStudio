#pragma once
#include "../../../Runtime/TNodeInstance.h"
#include "../TNodeSDK.h"

#include <array>
#include <cmath>

namespace Teul::Nodes {
namespace MixerNodeHelpers {

static int findPortChannel(const TProcessContext &ctx,
                           juce::StringRef portName) {
  if (ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
    return -1;

  for (const auto &port : ctx.nodeData->ports) {
    if (port.name == portName) {
      const auto it = ctx.portToChannel->find(port.portId);
      return it != ctx.portToChannel->end() ? it->second : -1;
    }
  }

  return -1;
}

static int findNthAudioInputChannel(const TProcessContext &ctx, int inputIndex) {
  if (ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
    return -1;

  int currentIndex = 0;
  for (const auto &port : ctx.nodeData->ports) {
    if (port.direction != TPortDirection::Input ||
        port.dataType != TPortDataType::Audio) {
      continue;
    }

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

    auto gain = makeFloatParamSpec("gain", "Gain", 1.0f, 0.0f, 2.0f, 0.001f,
                                   {}, 3, "Level",
                                   "Linear gain applied before CV modulation.");
    gain.showInNodeBody = true;
    gain.preferredBindingKey = "vcaGain";
    gain.categoryPath = "VCA/Level";
    desc.paramSpecs = {gain};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "L In"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In"},
                      {TPortDirection::Input, TPortDataType::CV, "CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "L Out"},
                      {TPortDirection::Output, TPortDataType::Audio, "R Out"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        if (paramKey == "gain")
          gain = juce::jlimit(0.0f, 2.0f, newValue);
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr)
          return;

        int leftInputChannel = MixerNodeHelpers::findPortChannel(ctx, "L In");
        int rightInputChannel = MixerNodeHelpers::findPortChannel(ctx, "R In");
        const int cvChannel = MixerNodeHelpers::findPortChannel(ctx, "CV");
        const int leftOutputChannel = MixerNodeHelpers::findPortChannel(ctx, "L Out");
        const int rightOutputChannel = MixerNodeHelpers::findPortChannel(ctx, "R Out");
        if (leftOutputChannel < 0 && rightOutputChannel < 0)
          return;

        if (leftInputChannel < 0)
          leftInputChannel = MixerNodeHelpers::findNthAudioInputChannel(ctx, 0);
        if (rightInputChannel < 0)
          rightInputChannel = MixerNodeHelpers::findNthAudioInputChannel(ctx, 1);

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        auto *leftOutput = leftOutputChannel >= 0
                               ? ctx.globalPortBuffer->getWritePointer(leftOutputChannel)
                               : nullptr;
        auto *rightOutput = rightOutputChannel >= 0
                                ? ctx.globalPortBuffer->getWritePointer(rightOutputChannel)
                                : nullptr;
        const float *leftInput =
            leftInputChannel >= 0 ? ctx.globalPortBuffer->getReadPointer(leftInputChannel)
                                  : nullptr;
        const float *rightInput = rightInputChannel >= 0
                                      ? ctx.globalPortBuffer->getReadPointer(rightInputChannel)
                                      : leftInput;
        const float *cv =
            cvChannel >= 0 ? ctx.globalPortBuffer->getReadPointer(cvChannel)
                           : nullptr;

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
          const float modulation =
              cv != nullptr ? juce::jmax(0.0f, cv[sampleIndex]) : 1.0f;
          const float leftSource =
              leftInput != nullptr ? leftInput[sampleIndex] : 0.0f;
          const float rightSource =
              rightInput != nullptr ? rightInput[sampleIndex] : leftSource;
          if (leftOutput != nullptr)
            leftOutput[sampleIndex] = leftSource * gain * modulation;
          if (rightOutput != nullptr)
            rightOutput[sampleIndex] = rightSource * gain * modulation;
        }
      }

    private:
      float gain = 1.0f;
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(VCANode);

class StereoPannerNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Mixer.StereoPanner";
    desc.displayName = "Stereo Panner";
    desc.category = "Mixer";
    desc.capabilities.canMute = true;

    auto pan = makeFloatParamSpec("pan", "Pan", 0.0f, -1.0f, 1.0f, 0.001f,
                                  {}, 3, "Placement",
                                  "-1 is left, +1 is right.");
    pan.showInNodeBody = true;
    pan.preferredBindingKey = "pan";
    pan.categoryPath = "StereoPanner/Placement";
    desc.paramSpecs = {pan};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "In"},
                      {TPortDirection::Input, TPortDataType::CV, "Pan CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "L Out"},
                      {TPortDirection::Output, TPortDataType::Audio, "R Out"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        if (paramKey == "pan")
          pan = juce::jlimit(-1.0f, 1.0f, newValue);
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr)
          return;

        const int inputChannel = MixerNodeHelpers::findPortChannel(ctx, "In");
        const int cvChannel = MixerNodeHelpers::findPortChannel(ctx, "Pan CV");
        const int leftChannel = MixerNodeHelpers::findPortChannel(ctx, "L Out");
        const int rightChannel = MixerNodeHelpers::findPortChannel(ctx, "R Out");
        if (leftChannel < 0 || rightChannel < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *input =
            inputChannel >= 0 ? ctx.globalPortBuffer->getReadPointer(inputChannel)
                              : nullptr;
        const float *cv =
            cvChannel >= 0 ? ctx.globalPortBuffer->getReadPointer(cvChannel)
                           : nullptr;
        auto *left = ctx.globalPortBuffer->getWritePointer(leftChannel);
        auto *right = ctx.globalPortBuffer->getWritePointer(rightChannel);

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
          const float source = input != nullptr ? input[sampleIndex] : 0.0f;
          const float currentPan = juce::jlimit(
              -1.0f, 1.0f, pan + (cv != nullptr ? cv[sampleIndex] : 0.0f));
          const float angle =
              (currentPan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
          left[sampleIndex] = source * std::cos(angle);
          right[sampleIndex] = source * std::sin(angle);
        }
      }

    private:
      float pan = 0.0f;
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(StereoPannerNode);

class Mixer2Node final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Mixer.Mixer2";
    desc.displayName = "Mixer (2-Ch)";
    desc.category = "Mixer";
    desc.capabilities.canMute = true;

    desc.paramSpecs = {{"gain1", "Gain 1", 1.0f}, {"gain2", "Gain 2", 1.0f}};
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "In 1"},
                      {TPortDirection::Input, TPortDataType::Audio, "In 2"},
                      {TPortDirection::Output, TPortDataType::Audio, "Out"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        if (paramKey == "gain1")
          gain1 = juce::jlimit(0.0f, 2.0f, newValue);
        else if (paramKey == "gain2")
          gain2 = juce::jlimit(0.0f, 2.0f, newValue);
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr)
          return;

        const int input1Channel = MixerNodeHelpers::findPortChannel(ctx, "In 1");
        const int input2Channel = MixerNodeHelpers::findPortChannel(ctx, "In 2");
        const int outputChannel = MixerNodeHelpers::findPortChannel(ctx, "Out");
        if (outputChannel < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *input1 = input1Channel >= 0
                                  ? ctx.globalPortBuffer->getReadPointer(input1Channel)
                                  : nullptr;
        const float *input2 = input2Channel >= 0
                                  ? ctx.globalPortBuffer->getReadPointer(input2Channel)
                                  : nullptr;
        auto *output = ctx.globalPortBuffer->getWritePointer(outputChannel);

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
          const float a = input1 != nullptr ? input1[sampleIndex] : 0.0f;
          const float b = input2 != nullptr ? input2[sampleIndex] : 0.0f;
          output[sampleIndex] = a * gain1 + b * gain2;
        }
      }

    private:
      float gain1 = 1.0f;
      float gain2 = 1.0f;
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(Mixer2Node);

class StereoMixer4Node final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Mixer.StereoMixer4";
    desc.displayName = "Stereo Mixer (4-Bus)";
    desc.category = "Mixer";
    desc.capabilities.canMute = true;

    std::vector<TParamSpec> params;
    params.reserve(4);
    for (int busIndex = 0; busIndex < 4; ++busIndex) {
      const juce::String suffix(busIndex + 1);
      auto gain = makeFloatParamSpec("gain" + suffix, "Gain " + suffix, 1.0f,
                                     0.0f, 2.0f, 0.001f, {}, 3, "Levels",
                                     "Linear gain applied to the stereo input bus.");
      gain.categoryPath = "StereoMixer/Level " + suffix;
      params.push_back(std::move(gain));
    }
    desc.paramSpecs = std::move(params);

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "L In 1"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In 1"},
                      {TPortDirection::Input, TPortDataType::Audio, "L In 2"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In 2"},
                      {TPortDirection::Input, TPortDataType::Audio, "L In 3"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In 3"},
                      {TPortDirection::Input, TPortDataType::Audio, "L In 4"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In 4"},
                      {TPortDirection::Output, TPortDataType::Audio, "L Out"},
                      {TPortDirection::Output, TPortDataType::Audio, "R Out"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        for (int busIndex = 0; busIndex < 4; ++busIndex) {
          if (paramKey == "gain" + juce::String(busIndex + 1)) {
            gains[(size_t)busIndex] = juce::jlimit(0.0f, 2.0f, newValue);
            return;
          }
        }
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr)
          return;

        const int leftOutputChannel = MixerNodeHelpers::findPortChannel(ctx, "L Out");
        const int rightOutputChannel = MixerNodeHelpers::findPortChannel(ctx, "R Out");
        if (leftOutputChannel < 0 && rightOutputChannel < 0)
          return;

        std::array<const float *, 4> leftInputs{};
        std::array<const float *, 4> rightInputs{};
        for (int busIndex = 0; busIndex < 4; ++busIndex) {
          const juce::String suffix(busIndex + 1);
          const int leftInputChannel =
              MixerNodeHelpers::findPortChannel(ctx, "L In " + suffix);
          const int rightInputChannel =
              MixerNodeHelpers::findPortChannel(ctx, "R In " + suffix);

          leftInputs[(size_t)busIndex] =
              leftInputChannel >= 0
                  ? ctx.globalPortBuffer->getReadPointer(leftInputChannel)
                  : nullptr;
          rightInputs[(size_t)busIndex] =
              rightInputChannel >= 0
                  ? ctx.globalPortBuffer->getReadPointer(rightInputChannel)
                  : nullptr;
        }

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        auto *leftOutput = leftOutputChannel >= 0
                               ? ctx.globalPortBuffer->getWritePointer(leftOutputChannel)
                               : nullptr;
        auto *rightOutput = rightOutputChannel >= 0
                                ? ctx.globalPortBuffer->getWritePointer(rightOutputChannel)
                                : nullptr;

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
          float leftMix = 0.0f;
          float rightMix = 0.0f;

          for (size_t busIndex = 0; busIndex < gains.size(); ++busIndex) {
            if (leftInputs[busIndex] != nullptr)
              leftMix += leftInputs[busIndex][sampleIndex] * gains[busIndex];
            if (rightInputs[busIndex] != nullptr)
              rightMix += rightInputs[busIndex][sampleIndex] * gains[busIndex];
          }

          if (leftOutput != nullptr)
            leftOutput[sampleIndex] = leftMix;
          if (rightOutput != nullptr)
            rightOutput[sampleIndex] = rightMix;
        }
      }

    private:
      std::array<float, 4> gains{{1.0f, 1.0f, 1.0f, 1.0f}};
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(StereoMixer4Node);

class AudioInputNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Source.AudioInput";
    desc.displayName = "Audio Input";
    desc.category = "Source";

    desc.capabilities.canMute = false;
    desc.capabilities.canBypass = false;

    desc.portSpecs = {{TPortDirection::Output, TPortDataType::Audio, "L Out"},
                      {TPortDirection::Output, TPortDataType::Audio, "R Out"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.inputAudioBuffer == nullptr ||
            ctx.nodeData == nullptr || ctx.portToChannel == nullptr) {
          return;
        }

        const int leftChannel = MixerNodeHelpers::findPortChannel(ctx, "L Out");
        const int rightChannel = MixerNodeHelpers::findPortChannel(ctx, "R Out");
        if (leftChannel < 0 && rightChannel < 0)
          return;

        const int availableInputChannels = ctx.inputAudioBuffer->getNumChannels();
        const int numSamples = juce::jmin(ctx.globalPortBuffer->getNumSamples(),
                                          ctx.inputAudioBuffer->getNumSamples());
        if (numSamples <= 0 || availableInputChannels <= 0)
          return;

        const float *leftInput = availableInputChannels > 0
                                     ? ctx.inputAudioBuffer->getReadPointer(0)
                                     : nullptr;
        const float *rightInput = availableInputChannels > 1
                                      ? ctx.inputAudioBuffer->getReadPointer(1)
                                      : leftInput;

        if (leftChannel >= 0) {
          auto *leftOutput = ctx.globalPortBuffer->getWritePointer(leftChannel);
          if (leftInput != nullptr)
            juce::FloatVectorOperations::copy(leftOutput, leftInput, numSamples);
          else
            juce::FloatVectorOperations::clear(leftOutput, numSamples);
        }

        if (rightChannel >= 0) {
          auto *rightOutput = ctx.globalPortBuffer->getWritePointer(rightChannel);
          if (rightInput != nullptr)
            juce::FloatVectorOperations::copy(rightOutput, rightInput, numSamples);
          else
            juce::FloatVectorOperations::clear(rightOutput, numSamples);
        }
      }
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(AudioInputNode);

class AudioOutputNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Routing.AudioOut";
    desc.displayName = "Audio Output";
    desc.category = "Routing";

    desc.capabilities.canMute = true;
    desc.capabilities.canBypass = false;
    desc.capabilities.minInstances = 1;
    desc.capabilities.maxInstances = 1;

    desc.paramSpecs = {{"volume", "Volume", 1.0f}};
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "L In"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        if (paramKey == "volume")
          volume = juce::jlimit(0.0f, 2.0f, newValue);
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.deviceAudioBuffer == nullptr)
          return;

        int leftInputChannel = MixerNodeHelpers::findPortChannel(ctx, "L In");
        int rightInputChannel = MixerNodeHelpers::findPortChannel(ctx, "R In");
        if (leftInputChannel < 0)
          leftInputChannel = MixerNodeHelpers::findNthAudioInputChannel(ctx, 0);
        if (rightInputChannel < 0)
          rightInputChannel = MixerNodeHelpers::findNthAudioInputChannel(ctx, 1);

        const int numSamples = juce::jmin(ctx.globalPortBuffer->getNumSamples(),
                                          ctx.deviceAudioBuffer->getNumSamples());
        if (numSamples <= 0 || ctx.deviceAudioBuffer->getNumChannels() <= 0)
          return;

        const float *leftInput = leftInputChannel >= 0
                                     ? ctx.globalPortBuffer->getReadPointer(leftInputChannel)
                                     : nullptr;
        const float *rightInput = rightInputChannel >= 0
                                      ? ctx.globalPortBuffer->getReadPointer(rightInputChannel)
                                      : leftInput;

        auto *leftOutput = ctx.deviceAudioBuffer->getWritePointer(0);
        float *rightOutput = ctx.deviceAudioBuffer->getNumChannels() > 1
                                 ? ctx.deviceAudioBuffer->getWritePointer(1)
                                 : nullptr;

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
          const float leftSample =
              (leftInput != nullptr ? leftInput[sampleIndex] : 0.0f) * volume;
          const float rightSample =
              (rightInput != nullptr ? rightInput[sampleIndex] : leftSample) * volume;

          leftOutput[sampleIndex] += leftSample;
          if (rightOutput != nullptr)
            rightOutput[sampleIndex] += rightSample;
        }
      }

    private:
      float volume = 1.0f;
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(AudioOutputNode);

} // namespace Teul::Nodes
