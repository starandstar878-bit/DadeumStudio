#pragma once
#include "../TNodeSDK.h"

namespace Teul::Nodes {

class OscillatorNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Source.Oscillator";
    desc.displayName = "Oscillator";
    desc.category = "Source";

    desc.capabilities.canMute = true;
    desc.capabilities.maxPolyphony = 8;

    desc.paramSpecs = {
        {"waveform", "Waveform", 0}, // 0:Sine, 1:Triangle, 2:Square, 3:Saw
        {"frequency", "Frequency", 440.0f},
        {"gain", "Gain", 0.707f}};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "V/Oct"},
                      {TPortDirection::Input, TPortDataType::CV, "Sync"},
                      {TPortDirection::Output, TPortDataType::Audio, "Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(OscillatorNode);

// -----------------------------------------------------------------------------

class LFONode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Source.LFO";
    desc.displayName = "LFO";
    desc.category = "Source";
    desc.paramSpecs = {{"waveform", "Waveform", 0}, {"rate", "Rate", 1.0f}};
    desc.portSpecs = {{TPortDirection::Output, TPortDataType::CV, "Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(LFONode);

// -----------------------------------------------------------------------------

class ConstantNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Source.Constant";
    desc.displayName = "Constant";
    desc.category = "Source";
    desc.capabilities.canMute = true;
    desc.paramSpecs = {{"value", "Value", 1.0f}};
    desc.portSpecs = {{TPortDirection::Output, TPortDataType::CV, "Value"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(ConstantNode);

} // namespace Teul::Nodes
