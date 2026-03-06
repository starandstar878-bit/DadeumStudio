#pragma once
#include "../TNodeSDK.h"

namespace Teul::Nodes {

class AddNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Math.Add";
    desc.displayName = "Add";
    desc.category = "Math";
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "A"},
                      {TPortDirection::Input, TPortDataType::CV, "B"},
                      {TPortDirection::Output, TPortDataType::CV, "A+B"}};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(AddNode);

class SubtractNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Math.Subtract";
    desc.displayName = "Subtract";
    desc.category = "Math";
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "A"},
                      {TPortDirection::Input, TPortDataType::CV, "B"},
                      {TPortDirection::Output, TPortDataType::CV, "A-B"}};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(SubtractNode);

class MultiplyNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Math.Multiply";
    desc.displayName = "Multiply";
    desc.category = "Math";
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "A"},
                      {TPortDirection::Input, TPortDataType::CV, "B"},
                      {TPortDirection::Output, TPortDataType::CV, "A*B"}};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(MultiplyNode);

class DivideNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Math.Divide";
    desc.displayName = "Divide";
    desc.category = "Math";
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "A"},
                      {TPortDirection::Input, TPortDataType::CV, "B"},
                      {TPortDirection::Output, TPortDataType::CV, "A/B"}};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(DivideNode);

class ClampNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Math.Clamp";
    desc.displayName = "Clamp";
    desc.category = "Math";
    desc.paramSpecs = {{"min", "Min", 0.0f}, {"max", "Max", 1.0f}};
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "In"},
                      {TPortDirection::Output, TPortDataType::CV, "Out"}};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(ClampNode);

class ValueMapNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Math.ValueMap";
    desc.displayName = "Value Map";
    desc.category = "Math";
    desc.paramSpecs = {{"inMin", "In Min", 0.0f},
                       {"inMax", "In Max", 1.0f},
                       {"outMin", "Out Min", -1.0f},
                       {"outMax", "Out Max", 1.0f}};
    desc.portSpecs = {{TPortDirection::Input, TPortDataType::CV, "In"},
                      {TPortDirection::Output, TPortDataType::CV, "Out"}};
    return desc;
  }
};
TEUL_NODE_AUTOREGISTER(ValueMapNode);

} // namespace Teul::Nodes
