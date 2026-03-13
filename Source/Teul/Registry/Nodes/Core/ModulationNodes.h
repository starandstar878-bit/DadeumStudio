#pragma once
#include "../../../Runtime/TNodeInstance.h"
#include "../../TNodeSDK.h"

namespace Teul::Nodes {

class ADSRNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Mod.ADSR";
    desc.displayName = "ADSR Envelope";
    desc.category = "Modulation";

    auto attack =
        makeFloatParamSpec("attack", "Attack", 10.0f, 0.0f, 5000.0f, 0.1f, "ms",
                           1, "Time", "Attack time in milliseconds.");
    attack.categoryPath = "ADSR/Time";

    auto decay =
        makeFloatParamSpec("decay", "Decay", 100.0f, 0.0f, 5000.0f, 0.1f, "ms",
                           1, "Time", "Decay time in milliseconds.");
    decay.categoryPath = "ADSR/Time";

    auto sustain =
        makeFloatParamSpec("sustain", "Sustain", 0.5f, 0.0f, 1.0f, 0.001f, {},
                           3, "Level", "Sustain level as a linear value.");
    sustain.categoryPath = "ADSR/Level";

    auto release =
        makeFloatParamSpec("release", "Release", 300.0f, 0.0f, 5000.0f, 0.1f,
                           "ms", 1, "Time", "Release time in milliseconds.");
    release.categoryPath = "ADSR/Time";

    desc.paramSpecs = {attack, decay, sustain, release};

    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::Gate, "Gate"),
                      makePortSpec(TPortDirection::Output, TPortDataType::CV, "Env")};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void prepareToPlay(double sr, int) override {
        adsr.setSampleRate(sr);
        updateParameters();
      }

      void reset() override {
        adsr.reset();
        gateHigh = false;
      }

      void setParameterValue(const juce::String &paramKey,
                             float newValue) override {
        if (paramKey == "attack")
          attackMs = juce::jlimit(0.0f, 5000.0f, newValue);
        else if (paramKey == "decay")
          decayMs = juce::jlimit(0.0f, 5000.0f, newValue);
        else if (paramKey == "sustain")
          sustain = juce::jlimit(0.0f, 1.0f, newValue);
        else if (paramKey == "release")
          releaseMs = juce::jlimit(0.0f, 5000.0f, newValue);
        else
          return;

        updateParameters();
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr ||
            ctx.portToChannel == nullptr)
          return;

        int gateChannel = -1;
        int envChannel = -1;
        for (const auto &port : ctx.nodeData->ports) {
          const auto it = ctx.portToChannel->find(port.portId);
          if (it == ctx.portToChannel->end())
            continue;
          if (port.name == "Gate")
            gateChannel = it->second;
          else if (port.name == "Env")
            envChannel = it->second;
        }

        if (envChannel < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *gate = gateChannel >= 0
                                ? ctx.globalPortBuffer->getReadPointer(gateChannel)
                                : nullptr;
        float *env = ctx.globalPortBuffer->getWritePointer(envChannel);

        for (int i = 0; i < numSamples; ++i) {
          const bool nextGateHigh = gate != nullptr ? gate[i] > 0.5f : false;
          if (nextGateHigh && !gateHigh)
            adsr.noteOn();
          else if (!nextGateHigh && gateHigh)
            adsr.noteOff();

          gateHigh = nextGateHigh;
          env[i] = adsr.getNextSample();
        }
      }

    private:
      void updateParameters() {
        juce::ADSR::Parameters params;
        params.attack = attackMs / 1000.0f;
        params.decay = decayMs / 1000.0f;
        params.sustain = sustain;
        params.release = releaseMs / 1000.0f;
        adsr.setParameters(params);
      }

      juce::ADSR adsr;
      float attackMs = 10.0f;
      float decayMs = 100.0f;
      float sustain = 0.5f;
      float releaseMs = 300.0f;
      bool gateHigh = false;
    };

    return std::make_unique<Implementation>();
  }
};

TEUL_NODE_AUTOREGISTER(ADSRNode);

} // namespace Teul::Nodes
