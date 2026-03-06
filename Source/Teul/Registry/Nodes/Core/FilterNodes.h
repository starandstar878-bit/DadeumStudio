#pragma once
#include "../TNodeSDK.h"

namespace Teul::Nodes {

class LowPassFilterNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Filter.LowPass";
    desc.displayName = "LowPass Filter";
    desc.category = "Filter";

    desc.paramSpecs = {{"cutoff", "Cutoff", 1000.0f},
                       {"resonance", "Resonance", 0.707f}};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "In"},
                      {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "Out"}};
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
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "In"},
                      {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "Out"}};
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
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Audio, "In"},
                      {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
                      {TPortDirection::Output, TPortDataType::Audio, "Out"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(BandPassFilterNode);

} // namespace Teul::Nodes
