#pragma once
#include "../../../Runtime/TNodeInstance.h"
#include "../../TNodeSDK.h"

namespace Teul::Nodes {

class AddNode final : public TNodeClass {
public:
  TNodeDescriptor makeDescriptor() const override {
    TNodeDescriptor desc;
    desc.typeKey = "Teul.Math.Add";
    desc.displayName = "Add";
    desc.category = "Math";
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
                      makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
                      makePortSpec(TPortDirection::Output, TPortDataType::CV, "A+B")};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr ||
            ctx.portToChannel == nullptr)
          return;

        int aCh = -1, bCh = -1, outCh = -1;
        for (const auto &port : ctx.nodeData->ports) {
          auto it = ctx.portToChannel->find(port.portId);
          if (it == ctx.portToChannel->end())
            continue;
          if (port.name == "A")
            aCh = it->second;
          else if (port.name == "B")
            bCh = it->second;
          else if (port.name == "A+B")
            outCh = it->second;
        }

        if (outCh < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
        const float *inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
        float *out = ctx.globalPortBuffer->getWritePointer(outCh);

        for (int i = 0; i < numSamples; ++i) {
          const float valA = inA != nullptr ? inA[i] : 0.0f;
          const float valB = inB != nullptr ? inB[i] : 0.0f;
          out[i] = valA + valB;
        }
      }
    };
    return std::make_unique<Implementation>();
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
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
                      makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
                      makePortSpec(TPortDirection::Output, TPortDataType::CV, "A-B")};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr ||
            ctx.portToChannel == nullptr)
          return;

        int aCh = -1, bCh = -1, outCh = -1;
        for (const auto &port : ctx.nodeData->ports) {
          auto it = ctx.portToChannel->find(port.portId);
          if (it == ctx.portToChannel->end())
            continue;
          if (port.name == "A")
            aCh = it->second;
          else if (port.name == "B")
            bCh = it->second;
          else if (port.name == "A-B")
            outCh = it->second;
        }

        if (outCh < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
        const float *inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
        float *out = ctx.globalPortBuffer->getWritePointer(outCh);

        for (int i = 0; i < numSamples; ++i) {
          const float valA = inA != nullptr ? inA[i] : 0.0f;
          const float valB = inB != nullptr ? inB[i] : 0.0f;
          out[i] = valA - valB;
        }
      }
    };
    return std::make_unique<Implementation>();
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
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
                      makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
                      makePortSpec(TPortDirection::Output, TPortDataType::CV, "A*B")};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr ||
            ctx.portToChannel == nullptr)
          return;

        int aCh = -1, bCh = -1, outCh = -1;
        for (const auto &port : ctx.nodeData->ports) {
          auto it = ctx.portToChannel->find(port.portId);
          if (it == ctx.portToChannel->end())
            continue;
          if (port.name == "A")
            aCh = it->second;
          else if (port.name == "B")
            bCh = it->second;
          else if (port.name == "A*B")
            outCh = it->second;
        }

        if (outCh < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
        const float *inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
        float *out = ctx.globalPortBuffer->getWritePointer(outCh);

        for (int i = 0; i < numSamples; ++i) {
          const float valA = inA != nullptr ? inA[i] : 0.0f;
          const float valB = inB != nullptr ? inB[i] : 0.0f;
          out[i] = valA * valB;
        }
      }
    };
    return std::make_unique<Implementation>();
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
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
                      makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
                      makePortSpec(TPortDirection::Output, TPortDataType::CV, "A/B")};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr ||
            ctx.portToChannel == nullptr)
          return;

        int aCh = -1, bCh = -1, outCh = -1;
        for (const auto &port : ctx.nodeData->ports) {
          auto it = ctx.portToChannel->find(port.portId);
          if (it == ctx.portToChannel->end())
            continue;
          if (port.name == "A")
            aCh = it->second;
          else if (port.name == "B")
            bCh = it->second;
          else if (port.name == "A/B")
            outCh = it->second;
        }

        if (outCh < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
        const float *inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
        float *out = ctx.globalPortBuffer->getWritePointer(outCh);

        for (int i = 0; i < numSamples; ++i) {
          const float valA = inA != nullptr ? inA[i] : 0.0f;
          const float valB = inB != nullptr ? inB[i] : 1.0f;
          out[i] = (valB != 0.0f) ? (valA / valB) : 0.0f;
        }
      }
    };
    return std::make_unique<Implementation>();
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
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::CV, "In"),
                      makePortSpec(TPortDirection::Output, TPortDataType::CV, "Out")};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void setParameterValue(const juce::String &paramKey, float newValue) override {
        if (paramKey == "min")
          minVal = newValue;
        else if (paramKey == "max")
          maxVal = newValue;
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr ||
            ctx.portToChannel == nullptr)
          return;

        int inCh = -1, outCh = -1;
        for (const auto &port : ctx.nodeData->ports) {
          auto it = ctx.portToChannel->find(port.portId);
          if (it == ctx.portToChannel->end())
            continue;
          if (port.name == "In")
            inCh = it->second;
          else if (port.name == "Out")
            outCh = it->second;
        }

        if (outCh < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *in = inCh >= 0 ? ctx.globalPortBuffer->getReadPointer(inCh) : nullptr;
        float *out = ctx.globalPortBuffer->getWritePointer(outCh);

        const float safeMin = std::min(minVal, maxVal);
        const float safeMax = std::max(minVal, maxVal);

        for (int i = 0; i < numSamples; ++i) {
          const float val = in != nullptr ? in[i] : 0.0f;
          out[i] = juce::jlimit(safeMin, safeMax, val);
        }
      }

    private:
      float minVal = 0.0f;
      float maxVal = 1.0f;
    };
    return std::make_unique<Implementation>();
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
    desc.portSpecs = {makePortSpec(TPortDirection::Input, TPortDataType::CV, "In"),
                      makePortSpec(TPortDirection::Output, TPortDataType::CV, "Out")};
    return desc;
  }

  std::unique_ptr<TNodeInstance> createInstance() const override {
    class Implementation final : public TNodeInstance {
    public:
      void setParameterValue(const juce::String &paramKey, float newValue) override {
        if (paramKey == "inMin")
          inMin = newValue;
        else if (paramKey == "inMax")
          inMax = newValue;
        else if (paramKey == "outMin")
          outMin = newValue;
        else if (paramKey == "outMax")
          outMax = newValue;
      }

      void processSamples(const TProcessContext &ctx) override {
        if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr ||
            ctx.portToChannel == nullptr)
          return;

        int inCh = -1, outCh = -1;
        for (const auto &port : ctx.nodeData->ports) {
          auto it = ctx.portToChannel->find(port.portId);
          if (it == ctx.portToChannel->end())
            continue;
          if (port.name == "In")
            inCh = it->second;
          else if (port.name == "Out")
            outCh = it->second;
        }

        if (outCh < 0)
          return;

        const int numSamples = ctx.globalPortBuffer->getNumSamples();
        const float *in = inCh >= 0 ? ctx.globalPortBuffer->getReadPointer(inCh) : nullptr;
        float *out = ctx.globalPortBuffer->getWritePointer(outCh);

        const float inRange = inMax - inMin;
        const float outRange = outMax - outMin;

        for (int i = 0; i < numSamples; ++i) {
          const float val = in != nullptr ? in[i] : 0.0f;
          if (std::abs(inRange) < 0.00001f) {
             out[i] = outMin;
          } else {
             const float normalized = (val - inMin) / inRange;
             out[i] = outMin + normalized * outRange;
          }
        }
      }

    private:
      float inMin = 0.0f;
      float inMax = 1.0f;
      float outMin = -1.0f;
      float outMax = 1.0f;
    };
    return std::make_unique<Implementation>();
  }
};
TEUL_NODE_AUTOREGISTER(ValueMapNode);

} // namespace Teul::Nodes
