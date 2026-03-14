#include "Teul2/Bridge/TIeumBridge.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"

namespace Teul {

namespace {

TParamValueType inferParamValueType(const TParamSpec& spec) {
  if (!spec.enumOptions.empty()) return TParamValueType::Enum;
  if (spec.defaultValue.isBool()) return TParamValueType::Bool;
  if (spec.defaultValue.isInt() || spec.defaultValue.isInt64()) return TParamValueType::Int;
  if (spec.defaultValue.isString()) return TParamValueType::String;
  return TParamValueType::Float;
}

TParamWidgetHint inferPreferredWidget(TParamValueType type) {
  switch (type) {
    case TParamValueType::Bool: return TParamWidgetHint::Toggle;
    case TParamValueType::Enum: return TParamWidgetHint::Combo;
    case TParamValueType::String: return TParamWidgetHint::Text;
    case TParamValueType::Float:
    case TParamValueType::Int: return TParamWidgetHint::Slider;
    case TParamValueType::Auto: break;
  }
  return TParamWidgetHint::Slider;
}

void applySpecMetadata(TTeulExposedParam& param, const TParamSpec& spec) {
  param.valueType = spec.valueType == TParamValueType::Auto ? inferParamValueType(spec) : spec.valueType;
  param.minValue = spec.minValue;
  param.maxValue = spec.maxValue;
  param.step = spec.step;
  param.unitLabel = spec.unitLabel;
  param.displayPrecision = spec.displayPrecision;
  param.group = spec.group;
  param.description = spec.description;
  param.enumOptions = spec.enumOptions;
  param.preferredWidget = spec.preferredWidget == TParamWidgetHint::Auto ? inferPreferredWidget(param.valueType) : spec.preferredWidget;
  param.showInNodeBody = spec.showInNodeBody;
  param.showInPropertyPanel = spec.showInPropertyPanel;
  param.isReadOnly = spec.isReadOnly;
  param.isAutomatable = spec.isAutomatable;
  param.isModulatable = spec.isModulatable;
  param.isDiscrete = spec.isDiscrete || (param.valueType == TParamValueType::Bool || param.valueType == TParamValueType::Enum);
  param.exposeToIeum = spec.exposeToIeum;
  param.preferredBindingKey = spec.preferredBindingKey.isNotEmpty() ? spec.preferredBindingKey : spec.key;
  param.exportSymbol = spec.exportSymbol.isNotEmpty() ? spec.exportSymbol : spec.key;
  param.categoryPath = spec.categoryPath.isNotEmpty() ? spec.categoryPath : spec.group;
}

void applyFallbackMetadata(TTeulExposedParam& param, const juce::var& value, const juce::String& key) {
  if (value.isBool()) param.valueType = TParamValueType::Bool;
  else if (value.isInt() || value.isInt64()) param.valueType = TParamValueType::Int;
  else if (value.isString()) param.valueType = TParamValueType::String;
  else param.valueType = TParamValueType::Float;

  param.preferredWidget = inferPreferredWidget(param.valueType);
  param.isDiscrete = (param.valueType == TParamValueType::Bool || param.valueType == TParamValueType::Int);
  param.preferredBindingKey = key;
  param.exportSymbol = key;
}

} // namespace

std::vector<TTeulExposedParam> TNodeRegistry::listExposedParamsForNode(const TNode& node) const {
  const auto* desc = descriptorFor(node.typeKey);
  std::vector<TTeulExposedParam> result;
  std::set<juce::String> seenKeys;

  const juce::String nodeName = node.label.isNotEmpty() ? node.label : (desc ? desc->displayName : node.typeKey);
  const juce::String typeKey = node.typeKey;

  if (desc) {
    result.reserve(desc->paramSpecs.size() + node.params.size());
    for (const auto& spec : desc->paramSpecs) {
      if (!spec.exposeToIeum) continue;

      TTeulExposedParam param;
      param.paramId = makeTeulParamId(node.nodeId, spec.key);
      param.nodeId = node.nodeId;
      param.nodeTypeKey = typeKey;
      param.nodeDisplayName = nodeName;
      param.paramKey = spec.key;
      param.paramLabel = spec.label.isNotEmpty() ? spec.label : spec.key;
      param.defaultValue = spec.defaultValue;

      if (auto it = node.params.find(spec.key); it != node.params.end())
        param.currentValue = it->second;
      else
        param.currentValue = spec.defaultValue;

      applySpecMetadata(param, spec);
      result.push_back(std::move(param));
      seenKeys.insert(spec.key);
    }
  }

  for (const auto& [key, value] : node.params) {
    if (seenKeys.find(key) != seenKeys.end()) continue;

    TTeulExposedParam param;
    param.paramId = makeTeulParamId(node.nodeId, key);
    param.nodeId = node.nodeId;
    param.nodeTypeKey = typeKey;
    param.nodeDisplayName = nodeName;
    param.paramKey = key;
    param.paramLabel = key;
    param.defaultValue = value;
    param.currentValue = value;
    applyFallbackMetadata(param, value, key);
    result.push_back(std::move(param));
  }

  return result;
}

} // namespace Teul
