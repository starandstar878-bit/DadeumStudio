#pragma once

#include "../Bridge/ITeulParamProvider.h"
#include "../Model/TNode.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Teul {

struct TParamSpec {
  juce::String key;
  juce::String label;
  juce::var defaultValue;

  TParamValueType valueType = TParamValueType::Auto;
  juce::var minValue;
  juce::var maxValue;
  juce::var step;
  juce::String unitLabel;
  int displayPrecision = 2;
  juce::String group;
  juce::String description;
  std::vector<TParamOptionSpec> enumOptions;
  TParamWidgetHint preferredWidget = TParamWidgetHint::Auto;
  bool showInNodeBody = false;
  bool showInPropertyPanel = true;
  bool isReadOnly = false;
  bool isAutomatable = true;
  bool isModulatable = true;
  bool isDiscrete = false;
  bool exposeToIeum = true;
  juce::String preferredBindingKey;
  juce::String exportSymbol;
  juce::String categoryPath;
};

inline TParamOptionSpec makeParamOption(const juce::String &id,
                                        const juce::String &label,
                                        const juce::var &value) {
  TParamOptionSpec option;
  option.id = id;
  option.label = label;
  option.value = value;
  return option;
}

inline TParamSpec makeFloatParamSpec(const juce::String &key,
                                     const juce::String &label,
                                     float defaultValue,
                                     float minValue,
                                     float maxValue,
                                     float step = 0.0f,
                                     const juce::String &unitLabel = {},
                                     int displayPrecision = 2,
                                     const juce::String &group = {},
                                     const juce::String &description = {}) {
  TParamSpec spec;
  spec.key = key;
  spec.label = label;
  spec.defaultValue = defaultValue;
  spec.valueType = TParamValueType::Float;
  spec.minValue = minValue;
  spec.maxValue = maxValue;
  spec.step = step;
  spec.unitLabel = unitLabel;
  spec.displayPrecision = displayPrecision;
  spec.group = group;
  spec.description = description;
  spec.preferredWidget = TParamWidgetHint::Slider;
  return spec;
}

inline TParamSpec makeEnumParamSpec(const juce::String &key,
                                    const juce::String &label,
                                    int defaultValue,
                                    std::vector<TParamOptionSpec> enumOptions,
                                    const juce::String &group = {},
                                    const juce::String &description = {}) {
  TParamSpec spec;
  spec.key = key;
  spec.label = label;
  spec.defaultValue = defaultValue;
  spec.valueType = TParamValueType::Enum;
  spec.group = group;
  spec.description = description;
  spec.enumOptions = std::move(enumOptions);
  spec.preferredWidget = TParamWidgetHint::Combo;
  spec.isDiscrete = true;
  return spec;
}

struct TPortSpec {
  TPortDirection direction;
  TPortDataType dataType;
  juce::String name;
};

struct TNodeCapabilities {
  bool canBypass = true;
  bool canMute = false;
  bool canSolo = false;
  bool isTimeDependent = false;

  int minInstances = 0;
  int maxInstances = -1;
  int maxPolyphony = 1;
  float processingLatencyMs = 0.0f;
  int estimatedCpuCost = 1;
};

class TNodeInstance;

struct TNodeDescriptor {
  juce::String typeKey;
  juce::String displayName;
  juce::String category;

  TNodeCapabilities capabilities;

  std::vector<TParamSpec> paramSpecs;
  std::vector<TPortSpec> portSpecs;

  std::function<std::unique_ptr<TNodeInstance>()> instanceFactory;
  std::function<std::vector<TTeulExposedParam>(const TNode &node)>
      exposedParamFactory;
};

class TNodeRegistry {
public:
  TNodeRegistry() = default;

  void registerNode(const TNodeDescriptor &desc);
  const TNodeDescriptor *descriptorFor(const juce::String &typeKey) const;
  const std::vector<TNodeDescriptor> &getAllDescriptors() const;
  std::vector<TTeulExposedParam> listExposedParamsForNode(
      const TNode &node) const;

private:
  std::vector<TNodeDescriptor> descriptors;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNodeRegistry)
};

std::unique_ptr<TNodeRegistry> makeDefaultNodeRegistry();

} // namespace Teul