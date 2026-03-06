#include "TNodeRegistry.h"
#include "TNodeSDK.h"

#include <set>

#include "Nodes/CoreNodes.h"

namespace Teul {
namespace {

std::vector<TTeulExposedParam> buildFallbackExposedParams(
    const TNode &node,
    const juce::String &displayName,
    const juce::String &typeKey,
    const std::vector<TParamSpec> &paramSpecs) {
  std::vector<TTeulExposedParam> result;
  std::set<juce::String> seenKeys;

  const juce::String nodeName = node.label.isNotEmpty() ? node.label : displayName;

  result.reserve(paramSpecs.size() + node.params.size());
  for (const auto &spec : paramSpecs) {
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
    result.push_back(std::move(param));
  }

  return result;
}

} // namespace

void TNodeRegistry::registerNode(const TNodeDescriptor &desc) {
  TNodeDescriptor next = desc;
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

} // namespace Teul