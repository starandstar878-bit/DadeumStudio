#pragma once
#include "../../../Runtime/TNodeInstance.h"
#include "../../TNodeSDK.h"

namespace Teul::Nodes {

class PitchToCVNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Midi.MidiToCV";
    desc.displayName = "MIDI to CV";
    desc.category = "MIDI";

    desc.capabilities.maxPolyphony = 1;

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::MIDI, "MIDI In"},
                      {TPortDirection::Output, TPortDataType::CV, "V/Oct"},
                      {TPortDirection::Output, TPortDataType::Gate, "Gate"},
                      {TPortDirection::Output, TPortDataType::CV, "Velocity"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        if (!ctx.midiMessages || !ctx.globalPortBuffer || !ctx.portToChannel ||
            !ctx.nodeData)
          return;

        auto findChannelByPortName = [&](juce::StringRef portName) -> int {
          for (const auto &port : ctx.nodeData->ports) {
            if (port.name == portName) {
              auto it = ctx.portToChannel->find(port.portId);
              return (it != ctx.portToChannel->end()) ? it->second : -1;
            }
          }
          return -1;
        };

        const int gateCh = findChannelByPortName("Gate");
        const int pctCh = findChannelByPortName("V/Oct");
        const int velCh = findChannelByPortName("Velocity");

        int numSamples = ctx.globalPortBuffer->getNumSamples();

        float currentGate = lastGate;
        float currentPct = lastPct;
        float currentVel = lastVel;

        // 임시: 전역 MIDI 버퍼에서 가장 최근 이벤트를 모노포닉으로 추적
        for (const auto meta : *ctx.midiMessages) {
          const auto msg = meta.getMessage();
          if (msg.isNoteOn()) {
            currentGate = 1.0f;
            currentPct =
                msg.getNoteNumber() / 12.0f; // 1V/Oct 방식 근사 (C0 = 0V)
            currentVel = msg.getFloatVelocity();
          } else if (msg.isNoteOff() &&
                     std::abs(currentPct - (msg.getNoteNumber() / 12.0f)) <
                         0.01f) {
            currentGate = 0.0f;
          }
        }

        if (gateCh >= 0) {
          auto *out = ctx.globalPortBuffer->getWritePointer(gateCh);
          for (int i = 0; i < numSamples; ++i)
            out[i] = currentGate;
        }
        if (pctCh >= 0) {
          auto *out = ctx.globalPortBuffer->getWritePointer(pctCh);
          for (int i = 0; i < numSamples; ++i)
            out[i] = currentPct;
        }
        if (velCh >= 0) {
          auto *out = ctx.globalPortBuffer->getWritePointer(velCh);
          for (int i = 0; i < numSamples; ++i)
            out[i] = currentVel;
        }

        lastGate = currentGate;
        lastPct = currentPct;
        lastVel = currentVel;
      }

    private:
      float lastGate = 0.0f;
      float lastPct = 5.0f; // 60 / 12 (C4)
      float lastVel = 0.0f;
    };
    return std::make_unique<Implementation>();
  }
};

TEUL_NODE_AUTOREGISTER(PitchToCVNode);

class MidiOutputNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Midi.MidiOut";
    desc.displayName = "MIDI Output";
    desc.category = "MIDI";
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::MIDI, "MIDI In"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        // Output 은 나중에 호스트로 전송할 때 처리
      }
    };
    return std::make_unique<Implementation>();
  }
};

TEUL_NODE_AUTOREGISTER(MidiOutputNode);

class MidiInputNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Source.MidiInput";
    desc.displayName = "MIDI Input";
    desc.category = "MIDI";
    desc.portSpecs = {
        {TPortDirection::Output, TPortDataType::MIDI, "MIDI Out"}};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        // Input 은 외부에서 ctx.midiMessages 에 넣어진 상태라고 간주
      }
    };
    return std::make_unique<Implementation>();
  }
};

TEUL_NODE_AUTOREGISTER(MidiInputNode);

} // namespace Teul::Nodes
