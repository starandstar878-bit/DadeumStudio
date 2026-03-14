#pragma once

#include "Teul2/Runtime/AudioGraph/TNodeInstance.h"
#include "Teul2/Runtime/AudioGraph/TNodeSDK.h"

namespace Teul::Nodes {

class AddNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Math.Add";
        desc.displayName = "Add";
        desc.category = "Math";
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "A+B")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int aCh = -1, bCh = -1, outCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "A")   aCh = it->second;
                    else if (port.name == "B")   bCh = it->second;
                    else if (port.name == "A+B") outCh = it->second;
                }

                if (outCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
                const float* inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
                float* out = ctx.globalPortBuffer->getWritePointer(outCh);

                for (int i = 0; i < numSamples; ++i) {
                    const float valA = inA ? inA[i] : 0.0f;
                    const float valB = inB ? inB[i] : 0.0f;
                    out[i] = valA + valB;
                }
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(AddNode);

class SubtractNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Math.Subtract";
        desc.displayName = "Subtract";
        desc.category = "Math";
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "A-B")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int aCh = -1, bCh = -1, outCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "A")   aCh = it->second;
                    else if (port.name == "B")   bCh = it->second;
                    else if (port.name == "A-B") outCh = it->second;
                }

                if (outCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
                const float* inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
                float* out = ctx.globalPortBuffer->getWritePointer(outCh);

                for (int i = 0; i < numSamples; ++i) {
                    const float valA = inA ? inA[i] : 0.0f;
                    const float valB = inB ? inB[i] : 0.0f;
                    out[i] = valA - valB;
                }
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(SubtractNode);

class MultiplyNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Math.Multiply";
        desc.displayName = "Multiply";
        desc.category = "Math";
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "A*B")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int aCh = -1, bCh = -1, outCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "A")   aCh = it->second;
                    else if (port.name == "B")   bCh = it->second;
                    else if (port.name == "A*B") outCh = it->second;
                }

                if (outCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
                const float* inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
                float* out = ctx.globalPortBuffer->getWritePointer(outCh);

                for (int i = 0; i < numSamples; ++i) {
                    const float valA = inA ? inA[i] : 0.0f;
                    const float valB = inB ? inB[i] : 0.0f;
                    out[i] = valA * valB;
                }
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(MultiplyNode);

class DivideNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Math.Divide";
        desc.displayName = "Divide";
        desc.category = "Math";
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "A"),
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "B"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "A/B")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int aCh = -1, bCh = -1, outCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "A")   aCh = it->second;
                    else if (port.name == "B")   bCh = it->second;
                    else if (port.name == "A/B") outCh = it->second;
                }

                if (outCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* inA = aCh >= 0 ? ctx.globalPortBuffer->getReadPointer(aCh) : nullptr;
                const float* inB = bCh >= 0 ? ctx.globalPortBuffer->getReadPointer(bCh) : nullptr;
                float* out = ctx.globalPortBuffer->getWritePointer(outCh);

                for (int i = 0; i < numSamples; ++i) {
                    const float valA = inA ? inA[i] : 0.0f;
                    const float valB = inB ? inB[i] : 1.0f;
                    out[i] = (valB != 0.0f) ? (valA / valB) : 0.0f;
                }
            }
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(DivideNode);

class ClampNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Math.Clamp";
        desc.displayName = "Clamp";
        desc.category = "Math";
        desc.paramSpecs = { 
            makeFloatParamSpec("min", "Min", 0.0f, -100.0f, 100.0f),
            makeFloatParamSpec("max", "Max", 1.0f, -100.0f, 100.0f)
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "In"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "Out")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if      (paramKey == "min") minVal = newValue;
                else if (paramKey == "max") maxVal = newValue;
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int inCh = -1, outCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "In")  inCh = it->second;
                    else if (port.name == "Out") outCh = it->second;
                }

                if (outCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* in = inCh >= 0 ? ctx.globalPortBuffer->getReadPointer(inCh) : nullptr;
                float* out = ctx.globalPortBuffer->getWritePointer(outCh);

                const float sMin = std::min(minVal, maxVal);
                const float sMax = std::max(minVal, maxVal);

                for (int i = 0; i < numSamples; ++i) {
                    const float val = in ? in[i] : 0.0f;
                    out[i] = juce::jlimit(sMin, sMax, val);
                }
            }

        private:
            float minVal = 0.0f;
            float maxVal = 1.0f;
        };
        return std::make_unique<Implementation>();
    }
};

TEUL2_NODE_REGISTER(ClampNode);

class ValueMapNode final : public TNodeClass {
public:
    TNodeDescriptor makeDescriptor() const override {
        TNodeDescriptor desc;
        desc.typeKey = "Teul.Math.ValueMap";
        desc.displayName = "Value Map";
        desc.category = "Math";
        desc.paramSpecs = { 
            makeFloatParamSpec("inMin", "In Min", 0.0f, -100.0f, 100.0f),
            makeFloatParamSpec("inMax", "In Max", 1.0f, -100.0f, 100.0f),
            makeFloatParamSpec("outMin", "Out Min", -1.0f, -100.0f, 100.0f),
            makeFloatParamSpec("outMax", "Out Max", 1.0f, -100.0f, 100.0f)
        };
        desc.portSpecs = {
            makePortSpec(TPortDirection::Input, TPortDataType::CV, "In"),
            makePortSpec(TPortDirection::Output, TPortDataType::CV, "Out")
        };
        return desc;
    }

    std::unique_ptr<TNodeInstance> createInstance() const override {
        class Implementation final : public TNodeInstance {
        public:
            void setParameterValue(const juce::String& paramKey, float newValue) override {
                if      (paramKey == "inMin")  inMin = newValue;
                else if (paramKey == "inMax")  inMax = newValue;
                else if (paramKey == "outMin") outMin = newValue;
                else if (paramKey == "outMax") outMax = newValue;
            }

            void processSamples(const TProcessContext& ctx) override {
                if (ctx.globalPortBuffer == nullptr || ctx.nodeData == nullptr || ctx.portToChannel == nullptr)
                    return;

                int inCh = -1, outCh = -1;
                for (const auto& port : ctx.nodeData->ports) {
                    auto it = ctx.portToChannel->find(port.portId);
                    if (it == ctx.portToChannel->end()) continue;
                    if      (port.name == "In")  inCh = it->second;
                    else if (port.name == "Out") outCh = it->second;
                }

                if (outCh < 0) return;

                const int numSamples = ctx.globalPortBuffer->getNumSamples();
                const float* in = inCh >= 0 ? ctx.globalPortBuffer->getReadPointer(inCh) : nullptr;
                float* out = ctx.globalPortBuffer->getWritePointer(outCh);

                const float inRange = inMax - inMin;
                const float outRange = outMax - outMin;

                for (int i = 0; i < numSamples; ++i) {
                    const float val = in ? in[i] : 0.0f;
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

TEUL2_NODE_REGISTER(ValueMapNode);

} // namespace Teul::Nodes
