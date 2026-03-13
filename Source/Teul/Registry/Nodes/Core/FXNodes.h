#pragma once
#include "../../../Runtime/TNodeInstance.h"
#include "../../TNodeSDK.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Teul::Nodes {
namespace FXNodeHelpers {

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
    desc.capabilities.estimatedCpuCost = 8;

    desc.paramSpecs = {{"roomSize", "Room Size", 0.5f},
                       {"damping", "Damping", 0.5f},
                       {"width", "Width", 1.0f},
                       {"mix", "Mix", 0.3f}};
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::Audio,
                                   "In", 2, {"L In", "R In"}),
                      makePortSpec(TPortDirection::Output,
                                   TPortDataType::Audio, "Out", 2,
                                   {"L Out", "R Out"})};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void prepareToPlay(double sr, int maximumExpectedSamplesPerBlock) override {
        wetLeft.resize((size_t)juce::jmax(1, maximumExpectedSamplesPerBlock));
        wetRight.resize((size_t)juce::jmax(1, maximumExpectedSamplesPerBlock));
        reverb.setSampleRate(sr);
        parametersDirty = true;
      }

      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        if (paramKey == "roomSize")
          roomSize = juce::jlimit(0.0f, 1.0f, newValue);
        else if (paramKey == "damping")
          damping = juce::jlimit(0.0f, 1.0f, newValue);
        else if (paramKey == "width")
          width = juce::jlimit(0.0f, 1.0f, newValue);
        else if (paramKey == "mix")
          mix = juce::jlimit(0.0f, 1.0f, newValue);
        else
          return;

        parametersDirty = true;
      }

      void reset() override { reverb.reset(); }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr)
          return;

        const int leftInCh = FXNodeHelpers::findPortChannel(ctx, "L In");
        const int rightInCh = FXNodeHelpers::findPortChannel(ctx, "R In");
        const int leftOutCh = FXNodeHelpers::findPortChannel(ctx, "L Out");
        const int rightOutCh = FXNodeHelpers::findPortChannel(ctx, "R Out");
        if (leftOutCh < 0 && rightOutCh < 0)
          return;

        if (parametersDirty) {
          juce::Reverb::Parameters params;
          params.roomSize = roomSize;
          params.damping = damping;
          params.width = width;
          params.wetLevel = 1.0f;
          params.dryLevel = 0.0f;
          params.freezeMode = 0.0f;
          reverb.setParameters(params);
          parametersDirty = false;
        }

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        if ((int)wetLeft.size() < numSamples) {
          wetLeft.resize((size_t)numSamples);
          wetRight.resize((size_t)numSamples);
        }

        const float *leftIn = leftInCh >= 0
                                  ? ctx.globalPortBuffer->getReadPointer(leftInCh)
                                  : nullptr;
        const float *rightIn = rightInCh >= 0
                                   ? ctx.globalPortBuffer->getReadPointer(rightInCh)
                                   : leftIn;
        float *leftOut = leftOutCh >= 0
                             ? ctx.globalPortBuffer->getWritePointer(leftOutCh)
                             : nullptr;
        float *rightOut = rightOutCh >= 0
                              ? ctx.globalPortBuffer->getWritePointer(rightOutCh)
                              : nullptr;

        for (int i = 0; i < numSamples; ++i) {
          wetLeft[(size_t)i] = leftIn != nullptr ? leftIn[i] : 0.0f;
          wetRight[(size_t)i] =
              rightIn != nullptr ? rightIn[i] : wetLeft[(size_t)i];
        }

        reverb.processStereo(wetLeft.data(), wetRight.data(), numSamples);

        const float dryMix = 1.0f - mix;
        for (int i = 0; i < numSamples; ++i) {
          const float leftDry = leftIn != nullptr ? leftIn[i] : 0.0f;
          const float rightDry = rightIn != nullptr ? rightIn[i] : leftDry;
          if (leftOut != nullptr)
            leftOut[i] = leftDry * dryMix + wetLeft[(size_t)i] * mix;
          if (rightOut != nullptr)
            rightOut[i] = rightDry * dryMix + wetRight[(size_t)i] * mix;
        }
      }

    private:
      juce::Reverb reverb;
      std::vector<float> wetLeft;
      std::vector<float> wetRight;
      float roomSize = 0.5f;
      float damping = 0.5f;
      float width = 1.0f;
      float mix = 0.3f;
      bool parametersDirty = true;
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(ReverbNode);

class DelayNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.FX.Delay";
    desc.displayName = "Delay";
    desc.category = "FX";

    desc.capabilities.isTimeDependent = true;
    desc.capabilities.canBypass = true;
    desc.capabilities.estimatedCpuCost = 4;

    desc.paramSpecs = {{"time", "Time (ms)", 250.0f},
                       {"feedback", "Feedback", 0.3f},
                       {"mix", "Mix", 0.5f}};
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::Audio,
                                   "In", 2, {"L In", "R In"}),
                      makePortSpec(TPortDirection::Output,
                                   TPortDataType::Audio, "Out", 2,
                                   {"L Out", "R Out"})};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void prepareToPlay(double sr, int maximumExpectedSamplesPerBlock) override {
        sampleRate = sr;
        const int minimumDelaySamples = juce::jmax(1, maximumExpectedSamplesPerBlock);
        const int maximumDelaySamples =
            juce::jmax(minimumDelaySamples + 1,
                       (int)std::ceil(sampleRate * maximumDelaySeconds()) + 1);
        delayLeft.assign((size_t)maximumDelaySamples, 0.0f);
        delayRight.assign((size_t)maximumDelaySamples, 0.0f);
        writeIndex = 0;
      }

      void reset() override {
        std::fill(delayLeft.begin(), delayLeft.end(), 0.0f);
        std::fill(delayRight.begin(), delayRight.end(), 0.0f);
        writeIndex = 0;
      }

      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        if (paramKey == "time")
          timeMs = juce::jlimit(1.0f, maximumDelaySeconds() * 1000.0f, newValue);
        else if (paramKey == "feedback")
          feedback = juce::jlimit(0.0f, 0.99f, newValue);
        else if (paramKey == "mix")
          mix = juce::jlimit(0.0f, 1.0f, newValue);
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || delayLeft.empty() || delayRight.empty())
          return;

        const int leftInCh = FXNodeHelpers::findPortChannel(ctx, "L In");
        const int rightInCh = FXNodeHelpers::findPortChannel(ctx, "R In");
        const int leftOutCh = FXNodeHelpers::findPortChannel(ctx, "L Out");
        const int rightOutCh = FXNodeHelpers::findPortChannel(ctx, "R Out");
        if (leftOutCh < 0 && rightOutCh < 0)
          return;

        const float *leftIn = leftInCh >= 0
                                  ? ctx.globalPortBuffer->getReadPointer(leftInCh)
                                  : nullptr;
        const float *rightIn = rightInCh >= 0
                                   ? ctx.globalPortBuffer->getReadPointer(rightInCh)
                                   : leftIn;
        float *leftOut = leftOutCh >= 0
                             ? ctx.globalPortBuffer->getWritePointer(leftOutCh)
                             : nullptr;
        float *rightOut = rightOutCh >= 0
                              ? ctx.globalPortBuffer->getWritePointer(rightOutCh)
                              : nullptr;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const int delaySamples = juce::jlimit(
            1, (int)delayLeft.size() - 1,
            (int)std::round((timeMs / 1000.0f) * sampleRate));
        const float dryMix = 1.0f - mix;

        for (int i = 0; i < numSamples; ++i) {
          const float inputLeft = leftIn != nullptr ? leftIn[i] : 0.0f;
          const float inputRight = rightIn != nullptr ? rightIn[i] : inputLeft;
          const int readIndex =
              (writeIndex + (int)delayLeft.size() - delaySamples) % (int)delayLeft.size();

          const float delayedLeft = delayLeft[(size_t)readIndex];
          const float delayedRight = delayRight[(size_t)readIndex];

          delayLeft[(size_t)writeIndex] = inputLeft + delayedLeft * feedback;
          delayRight[(size_t)writeIndex] = inputRight + delayedRight * feedback;

          if (leftOut != nullptr)
            leftOut[i] = inputLeft * dryMix + delayedLeft * mix;
          if (rightOut != nullptr)
            rightOut[i] = inputRight * dryMix + delayedRight * mix;

          writeIndex = (writeIndex + 1) % (int)delayLeft.size();
        }
      }

    private:
      static float maximumDelaySeconds() { return 5.0f; }

      std::vector<float> delayLeft;
      std::vector<float> delayRight;
      double sampleRate = 48000.0;
      float timeMs = 250.0f;
      float feedback = 0.3f;
      float mix = 0.5f;
      int writeIndex = 0;
    };

    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(DelayNode);

} // namespace Teul::Nodes
