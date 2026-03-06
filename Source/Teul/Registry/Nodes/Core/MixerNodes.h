#pragma once
#include "../TNodeSDK.h"

namespace Teul::Nodes {

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

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "In"},
                      {TPortDirection::Input, TPortDataType::CV, "CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "Out"}};
    return desc;
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
};
TEUL_NODE_AUTOREGISTER(Mixer2Node);

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
};
TEUL_NODE_AUTOREGISTER(AudioOutputNode);

} // namespace Teul::Nodes