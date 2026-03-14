#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"

#include "Teul2/Document/TDocumentSerializer.h"
#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Runtime/AudioGraph/Nodes/CoreNodes.h"

#include <set>

namespace Teul {

namespace {

TParamValueType inferParamValueType(const TParamSpec &spec) {
  if (!spec.enumOptions.empty())
    return TParamValueType::Enum;
  if (spec.defaultValue.isBool())
    return TParamValueType::Bool;
  if (spec.defaultValue.isInt() || spec.defaultValue.isInt64())
    return TParamValueType::Int;
  if (spec.defaultValue.isString())
    return TParamValueType::String;
  return TParamValueType::Float;
}

TParamWidgetHint inferPreferredWidget(const TParamSpec &spec) {
  switch (spec.valueType) {
  case TParamValueType::Bool:
    return TParamWidgetHint::Toggle;
  case TParamValueType::Enum:
    return TParamWidgetHint::Combo;
  case TParamValueType::String:
    return TParamWidgetHint::Text;
  case TParamValueType::Float:
  case TParamValueType::Int:
    return TParamWidgetHint::Slider;
  case TParamValueType::Auto:
    break;
  }

  return TParamWidgetHint::Text;
}

void normalizeParamSpec(TParamSpec &spec) {
  if (spec.valueType == TParamValueType::Auto)
    spec.valueType = inferParamValueType(spec);

  if (spec.preferredWidget == TParamWidgetHint::Auto)
    spec.preferredWidget = inferPreferredWidget(spec);

  if (spec.label.isEmpty())
    spec.label = spec.key;

  if (spec.preferredBindingKey.isEmpty())
    spec.preferredBindingKey = spec.key;

  if (spec.exportSymbol.isEmpty())
    spec.exportSymbol = spec.key;

  if (spec.categoryPath.isEmpty() && spec.group.isNotEmpty())
    spec.categoryPath = spec.group;

  if (spec.valueType == TParamValueType::Bool ||
      spec.valueType == TParamValueType::Enum)
    spec.isDiscrete = true;
}

void applySpecMetadata(TTeulExposedParam &param, const TParamSpec &spec) {
  param.valueType = spec.valueType;
  param.minValue = spec.minValue;
  param.maxValue = spec.maxValue;
  param.step = spec.step;
  param.unitLabel = spec.unitLabel;
  param.displayPrecision = spec.displayPrecision;
  param.group = spec.group;
  param.description = spec.description;
  param.enumOptions = spec.enumOptions;
  param.preferredWidget = spec.preferredWidget;
  param.showInNodeBody = spec.showInNodeBody;
  param.showInPropertyPanel = spec.showInPropertyPanel;
  param.isReadOnly = spec.isReadOnly;
  param.isAutomatable = spec.isAutomatable;
  param.isModulatable = spec.isModulatable;
  param.isDiscrete = spec.isDiscrete;
  param.exposeToIeum = spec.exposeToIeum;
  param.preferredBindingKey = spec.preferredBindingKey;
  param.exportSymbol = spec.exportSymbol;
  param.categoryPath = spec.categoryPath;
}

void applyFallbackMetadata(TTeulExposedParam &param, const juce::var &value,
                           const juce::String &key) {
  param.valueType = value.isBool() ? TParamValueType::Bool
                                   : (value.isInt() || value.isInt64())
                                         ? TParamValueType::Int
                                         : value.isString() ? TParamValueType::String
                                                            : TParamValueType::Float;
  param.preferredWidget = param.valueType == TParamValueType::Bool
                              ? TParamWidgetHint::Toggle
                              : param.valueType == TParamValueType::String
                                    ? TParamWidgetHint::Text
                                    : TParamWidgetHint::Slider;
  param.isDiscrete = param.valueType == TParamValueType::Bool ||
                     param.valueType == TParamValueType::Int;
  param.preferredBindingKey = key;
  param.exportSymbol = key;
}

std::vector<TTeulExposedParam> buildFallbackExposedParams(
    const TNode &node,
    const juce::String &displayName,
    const juce::String &typeKey,
    const std::vector<TParamSpec> &paramSpecs) {
  std::vector<TTeulExposedParam> result;
  std::set<juce::String> seenKeys;

  const juce::String nodeName =
      node.label.isNotEmpty() ? node.label : displayName;

  result.reserve(paramSpecs.size() + node.params.size());
  for (const auto &spec : paramSpecs) {
    if (!spec.exposeToIeum)
      continue;

    TTeulExposedParam param;
    param.paramId = makeTeulParamId(node.nodeId, spec.key);
    param.nodeId = node.nodeId;
    param.nodeTypeKey = typeKey;
    param.nodeDisplayName = nodeName;
    param.paramKey = spec.key;
    param.paramLabel = spec.label.isNotEmpty() ? spec.label : spec.key;
    param.defaultValue = spec.defaultValue;

    if (const auto it = node.params.find(spec.key); it != node.params.end())
      param.currentValue = it->second;
    else
      param.currentValue = spec.defaultValue;

    applySpecMetadata(param, spec);
    result.push_back(std::move(param));
    seenKeys.insert(spec.key);
  }

  for (const auto &[key, value] : node.params) {
    if (seenKeys.find(key) != seenKeys.end())
      continue;

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

} // namespace

// =============================================================================
//  TNodeRegistry Implementation
// =============================================================================

void TNodeRegistry::registerNode(const TNodeDescriptor &desc) {
  TNodeDescriptor next = desc;
  for (auto &spec : next.paramSpecs)
    normalizeParamSpec(spec);

  if (!next.exposedParamFactory) {
    const auto displayName = next.displayName;
    const auto typeKey = next.typeKey;
    const auto specs = next.paramSpecs;
    next.exposedParamFactory =
        [displayName, typeKey, specs](const TNode &node) {
          return buildFallbackExposedParams(node, displayName, typeKey, specs);
        };
  }

  for (auto &existing : descriptors) {
    if (existing.typeKey == next.typeKey) {
      existing = std::move(next);
      return;
    }
  }
  descriptors.push_back(std::move(next));
}

const TNodeDescriptor *
TNodeRegistry::descriptorFor(const juce::String &typeKey) const {
  for (const auto &desc : descriptors) {
    if (desc.typeKey == typeKey)
      return &desc;
  }
  return nullptr;
}

const std::vector<TNodeDescriptor> &TNodeRegistry::getAllDescriptors() const {
  return descriptors;
}

std::vector<TTeulExposedParam> TNodeRegistry::listExposedParamsForNode(
    const TNode &node) const {
  if (const auto *desc = descriptorFor(node.typeKey)) {
    if (desc->exposedParamFactory)
      return desc->exposedParamFactory(node);

    return buildFallbackExposedParams(node, desc->displayName, desc->typeKey,
                                      desc->paramSpecs);
  }

  return buildFallbackExposedParams(node, node.typeKey, node.typeKey, {});
}

std::unique_ptr<TNodeRegistry> makeDefaultNodeRegistry() {
  auto registry = std::make_unique<TNodeRegistry>();

  for (const auto &factory : TNodeClassCatalog::allFactories()) {
    if (!factory)
      continue;

    auto nodeClass = factory();
    if (nodeClass) {
      TNodeDescriptor desc = nodeClass->makeDescriptor();
      desc.instanceFactory = [factory]() {
        auto nc = factory();
        return nc ? nc->createInstance() : nullptr;
      };
      registry->registerNode(desc);
    }
  }

  return registry;
}

// =============================================================================
//  TGraphCompiler Implementation
// =============================================================================

juce::var TGraphCompiler::compileDocumentJson(const TTeulDocument &document) {
  return TDocumentSerializer::toJson(document);
}

bool TGraphCompiler::compileLegacyDocument(TGraphDocument &legacyOut,
                                           const TTeulDocument &document) {
  // 새로운 규격의 도큐먼트를 레거시 런타임이 이해할 수 있는 구조로 복사/변환합니다.
  legacyOut.meta = document.meta;
  legacyOut.nodes = document.nodes;
  legacyOut.connections = document.connections;
  legacyOut.frames = document.frames;
  legacyOut.bookmarks = document.bookmarks;
  legacyOut.controlState = document.controlState;

  // 나머지 내부 인덱스 정보 등은 도큐먼트 모델에서 직접 접근 가능하다고 가정합니다.
  return true;
}

} // namespace Teul