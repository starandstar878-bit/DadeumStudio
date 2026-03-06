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

    auto waveform = makeEnumParamSpec(
        "waveform", "Waveform", 0,
        {makeParamOption("sine", "Sine", 0),
         makeParamOption("triangle", "Triangle", 1),
         makeParamOption("square", "Square", 2),
         makeParamOption("saw", "Saw", 3)},
        "Tone", "Selects the base oscillator waveform.");
    waveform.showInNodeBody = true;
    waveform.preferredBindingKey = "waveform";
    waveform.categoryPath = "Oscillator/Tone";

    auto frequency = makeFloatParamSpec(
        "frequency", "Frequency", 440.0f, 20.0f, 20000.0f, 0.01f, "Hz", 2,
        "Pitch", "Base oscillator frequency before modulation.");
    frequency.showInNodeBody = true;
    frequency.preferredBindingKey = "pitch";
    frequency.exportSymbol = "oscFrequency";
    frequency.categoryPath = "Oscillator/Pitch";

    auto gain = makeFloatParamSpec(
        "gain", "Gain", 0.707f, 0.0f, 1.0f, 0.001f, {}, 3, "Output",
        "Linear output gain.");
    gain.showInNodeBody = true;
    gain.preferredBindingKey = "gain";
    gain.exportSymbol = "oscGain";
    gain.categoryPath = "Oscillator/Output";

    desc.paramSpecs = {waveform, frequency, gain};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "V/Oct"},
                      {TPortDirection::Input, TPortDataType::CV, "Sync"},
                      {TPortDirection::Output, TPortDataType::Audio, "Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(OscillatorNode);

class LFONode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Source.LFO";
    desc.displayName = "LFO";
    desc.category = "Source";

    auto waveform = makeEnumParamSpec(
        "waveform", "Waveform", 0,
        {makeParamOption("sine", "Sine", 0),
         makeParamOption("triangle", "Triangle", 1),
         makeParamOption("square", "Square", 2),
         makeParamOption("saw", "Saw", 3)},
        "Tone", "Selects the modulation waveform.");
    waveform.preferredBindingKey = "lfoWaveform";
    waveform.categoryPath = "LFO/Tone";

    auto rate = makeFloatParamSpec("rate", "Rate", 1.0f, 0.01f, 20.0f,
                                   0.01f, "Hz", 2, "Motion",
                                   "LFO rate in cycles per second.");
    rate.showInNodeBody = true;
    rate.preferredBindingKey = "lfoRate";
    rate.categoryPath = "LFO/Motion";

    desc.paramSpecs = {waveform, rate};
    desc.portSpecs = {{TPortDirection::Output, TPortDataType::CV, "Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(LFONode);

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