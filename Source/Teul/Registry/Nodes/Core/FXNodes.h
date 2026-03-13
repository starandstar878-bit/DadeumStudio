#pragma once
#include "../../TNodeSDK.h"

namespace Teul::Nodes {

class ReverbNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.FX.Reverb";
    desc.displayName = "Reverb";
    desc.category = "FX";

    desc.capabilities.isTimeDependent = true; // 내부 버퍼 꼬임 방지 힌트
    desc.capabilities.canBypass = true;
    desc.capabilities.estimatedCpuCost = 8;

    desc.paramSpecs = {{"roomSize", "Room Size", 0.5f},
                       {"damping", "Damping", 0.5f},
                       {"width", "Width", 1.0f},
                       {"mix", "Mix", 0.3f}};
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
                      makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(ReverbNode);

class DelayNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.FX.Delay";
    desc.displayName = "Delay";
    desc.category = "FX";

    desc.capabilities.isTimeDependent = true;
    desc.capabilities.canBypass = true;
    desc.capabilities.estimatedCpuCost = 4;

    desc.paramSpecs = {{"time", "Time (ms)", 250.0f},
                       {"feedback", "Feedback", 0.3f},
                       {"mix", "Mix", 0.5f}};
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::Audio, "In", 2, {"L In", "R In"}),
                      makePortSpec(TPortDirection::Output, TPortDataType::Audio, "Out", 2, {"L Out", "R Out"})};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(DelayNode);

} // namespace Teul::Nodes
