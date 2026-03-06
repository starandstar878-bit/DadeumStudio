#pragma once
#include "../TNodeSDK.h"

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
};

TEUL_NODE_AUTOREGISTER(MidiInputNode);

} // namespace Teul::Nodes
