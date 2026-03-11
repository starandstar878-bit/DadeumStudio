#pragma once
#include "../../TNodeSDK.h"

namespace Teul::Nodes {

class LowPassFilterNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Filter.LowPass";
    desc.displayName = "LowPass Filter";
    desc.category = "Filter";

    auto cutoff =
        makeFloatParamSpec("cutoff", "Cutoff", 1000.0f, 20.0f, 20000.0f, 0.01f,
                           "Hz", 2, "Frequency", "Low-pass cutoff frequency.");
    cutoff.showInNodeBody = true;
    cutoff.preferredBindingKey = "cutoff";
    cutoff.categoryPath = "Filter/Frequency";

    auto resonance = makeFloatParamSpec("resonance", "Resonance", 0.707f, 0.1f,
                                        12.0f, 0.001f, {}, 3, "Response",
                                        "Filter resonance / Q response.");
    resonance.preferredBindingKey = "resonance";
    resonance.categoryPath = "Filter/Response";

    desc.paramSpecs = {cutoff, resonance};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "L In"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In"},
                      {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "L Out"},
                      {TPortDirection::Output, TPortDataType::Audio, "R Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(LowPassFilterNode);

class HighPassFilterNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Filter.HighPass";
    desc.displayName = "HighPass Filter";
    desc.category = "Filter";

    desc.paramSpecs = {{"cutoff", "Cutoff", 1000.0f},
                       {"resonance", "Resonance", 0.707f}};
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "L In"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In"},
                      {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "L Out"},
                      {TPortDirection::Output, TPortDataType::Audio, "R Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(HighPassFilterNode);

class BandPassFilterNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Filter.BandPass";
    desc.displayName = "BandPass Filter";
    desc.category = "Filter";

    desc.paramSpecs = {{"cutoff", "Cutoff", 1000.0f}, {"q", "Q Factor", 1.0f}};
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "L In"},
                      {TPortDirection::Input, TPortDataType::Audio, "R In"},
                      {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "L Out"},
                      {TPortDirection::Output, TPortDataType::Audio, "R Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(BandPassFilterNode);

} // namespace Teul::Nodes