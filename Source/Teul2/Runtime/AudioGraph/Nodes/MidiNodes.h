#pragma once

#include "Teul2/Runtime/AudioGraph/TNodeInstance.h"
#include "Teul2/Runtime/AudioGraph/TNodeSDK.h"
#include <limits>

namespace Teul::Nodes {

class PitchToCVNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Midi.MidiToCV";
        desc.displayName = "MIDI to CV";
        desc.category = "MIDI";
        desc.capabilities.maxPolyphony = 1;
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::MIDI, "MIDI In"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "V/Oct"),
            makePortSpec(TPortDirection::Output, TPortDataType::Gate, "Gate"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "Velocity")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (!ctx.midiMessages || !ctx.globalPortBuffer || !ctx.portToChannel || !ctx.nodeData) return;

                auto findCh = [&](juce::StringRef n) -> int {
                    for (const auto& p : ctx.nodeData->ports) {
                        if (p.name == n) {
                            auto it = ctx.portToChannel->find(p.portId);
                            return (it != ctx.portToChannel->end()) ? it->second : -1;
                        }
                    }
                    return -1;
                };

                const int gateCh = findCh("Gate"), pctCh = findCh("V/Oct"), velCh = findCh("Velocity");
                const int numSamples = ctx.globalPortBuffer->getNumSamples();

                float cGate = lGate, cPct = lPct, cVel = lVel;
                for (const auto meta : *ctx.midiMessages) {
                    const auto msg = meta.getMessage();
                    if (msg.isNoteOn()) {
                        cGate = 1.0f; cPct = msg.getNoteNumber() / 12.0f; cVel = msg.getFloatVelocity();
                    } else if (msg.isNoteOff() && std::abs(cPct - (msg.getNoteNumber() / 12.0f)) < 0.01f) {
                        cGate = 0.0f;
                    }
                }

                if (gateCh >= 0) {
                    float* o = ctx.globalPortBuffer->getWritePointer(gateCh);
                    for (int i = 0; i < numSamples; ++i) o[i] = cGate;
                }
                if (pctCh >= 0) {
                    float* o = ctx.globalPortBuffer->getWritePointer(pctCh);
                    for (int i = 0; i < numSamples; ++i) o[i] = cPct;
                }
                if (velCh >= 0) {
                    float* o = ctx.globalPortBuffer->getWritePointer(velCh);
                    for (int i = 0; i < numSamples; ++i) o[i] = cVel;
                }
                lGate = cGate; lPct = cPct; lVel = cVel;
            }
        private:
            float lGate = 0.0f, lPct = 5.0f, lVel = 0.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(PitchToCVNode);

class MidiOutputNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Midi.MidiOut";
        desc.displayName = "MIDI Output";
        desc.category = "MIDI";
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::MIDI, "MIDI In"),
            makePortSpec(TPortDirection::Output, TPortDataType::MIDI, "MIDI Out")
        };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.midiOutputMessages == nullptr) return;
                ctx.midiOutputMessages->clear();
                if (ctx.midiMessages == nullptr || ctx.midiMessages->isEmpty()) return;
                int n = ctx.globalPortBuffer ? ctx.globalPortBuffer->getNumSamples() : std::numeric_limits<int>::max();
                ctx.midiOutputMessages->addEvents(*ctx.midiMessages, 0, n, 0);
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(MidiOutputNode);

class MidiInputNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Source.MidiInput";
        desc.displayName = "MIDI Input";
        desc.category = "MIDI";
        desc.portSpecs = { makePortSpec(TPortDirection::Output, TPortDataType::MIDI, "MIDI Out") };
        return desc;
    }
    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.midiOutputMessages == nullptr) return;
                ctx.midiOutputMessages->clear();
                const juce::MidiBuffer* src = ctx.deviceMidiMessages ? ctx.deviceMidiMessages : ctx.midiMessages;
                if (!src || src->isEmpty()) return;
                int n = ctx.globalPortBuffer ? ctx.globalPortBuffer->getNumSamples() : std::numeric_limits<int>::max();
                ctx.midiOutputMessages->addEvents(*src, 0, n, 0);
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(MidiInputNode);

} // namespace Teul::Nodes
