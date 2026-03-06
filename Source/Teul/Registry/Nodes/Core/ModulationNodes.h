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

    auto attack = makeFloatParamSpec("attack", "Attack", 10.0f, 0.0f, 5000.0f,
                                     0.1f, "ms", 1, "Time",
                                     "Attack time in milliseconds.");
    attack.categoryPath = "ADSR/Time";

    auto decay = makeFloatParamSpec("decay", "Decay", 100.0f, 0.0f, 5000.0f,
                                    0.1f, "ms", 1, "Time",
                                    "Decay time in milliseconds.");
    decay.categoryPath = "ADSR/Time";

    auto sustain = makeFloatParamSpec("sustain", "Sustain", 0.5f, 0.0f, 1.0f,
                                      0.001f, {}, 3, "Level",
                                      "Sustain level as a linear value.");
    sustain.categoryPath = "ADSR/Level";

    auto release = makeFloatParamSpec("release", "Release", 300.0f, 0.0f,
                                      5000.0f, 0.1f, "ms", 1, "Time",
                                      "Release time in milliseconds.");
    release.categoryPath = "ADSR/Time";

    desc.paramSpecs = {attack, decay, sustain, release};

    desc.portSpecs = {{TPortDirection::Input, TPortDataType::Gate, "Gate"},
                      {TPortDirection::Output, TPortDataType::CV, "Env"}};
    return desc;
  }
};

TEUL_NODE_AUTOREGISTER(ADSRNode);

} // namespace Teul::Nodes