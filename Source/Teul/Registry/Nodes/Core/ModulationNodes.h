#pragma once
#include "../TNodeSDK.h"

namespace Teul::Nodes {

class ADSRNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Mod.ADSR";
    desc.displayName = "ADSR Envelope";
    desc.category = "Modulation";

    desc.paramSpecs = {{"attack", "Attack", 10.0f},
                       {"decay", "Decay", 100.0f},
                       {"sustain", "Sustain", 0.5f},
                       {"release", "Release", 300.0f}};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Gate, "Gate"},
                      {TPortDirection::Output, TPortDataType::CV, "Env"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(ADSRNode);

} // namespace Teul::Nodes
