#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"

#include "Teul2/Document/TDocumentSerializer.h"
#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Runtime/AudioGraph/TGraphProcessor.h"
#include "Teul2/Runtime/AudioGraph/Nodes/CoreNodes.h"

#include <set>
#include <queue>

namespace Teul {

namespace {

// =============================================================================
//  컴파일러 내부 유틸리티
// =============================================================================

float paramValueToFloat(const juce::var &value) {
  if (value.isBool())
    return static_cast<float>((bool)value ? 1.0 : 0.0);
  if (value.isInt() || value.isInt64())
    return static_cast<float>(static_cast<double>(value));
  if (value.isDouble())
    return static_cast<float>((double)value);
  if (value.isString())
    return value.toString().getFloatValue();
  return static_cast<float>(value);
}

bool shouldSmoothParam(const TParamSpec *paramSpec,
                       const juce::var &initialValue) noexcept {
  if (paramSpec == nullptr)
    return initialValue.isDouble() || (!initialValue.isInt() &&
                                       !initialValue.isInt64() &&
                                       !initialValue.isBool());

  if (paramSpec->isReadOnly || paramSpec->isDiscrete ||
      paramSpec->valueType == TParamValueType::Bool ||
      paramSpec->valueType == TParamValueType::Enum ||
      paramSpec->preferredWidget == TParamWidgetHint::Toggle ||
      paramSpec->preferredWidget == TParamWidgetHint::Combo) {
    return false;
  }

  return true;
}

void writeParamKey(char *dest, std::size_t destSize, const juce::String &paramKey) {
  if (dest == nullptr || destSize == 0)
    return;
  std::memset(dest, 0, destSize);
  paramKey.copyToUTF8(dest, static_cast<int>(destSize));
}

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
    const TNode &node, const juce::String &displayName,
    const juce::String &typeKey, const std::vector<TParamSpec> &paramSpecs) {
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

juce::String makeControlSourcePortKey(const juce::String &sourceId,
                                      const juce::String &portId) {
  const auto s = sourceId.trim();
  const auto p = portId.trim();
  if (s.isEmpty() || p.isEmpty())
    return {};
  return s + "::" + p;
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

std::vector<TTeulExposedParam>
TNodeRegistry::listExposedParamsForNode(const TNode &node) const {
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

TCompiledGraph::Ptr TGraphCompiler::compileDocument(const TTeulDocument &document,
                                                   const TNodeRegistry &registry,
                                                   double sampleRate,
                                                   int blockSize) {
  const auto buildStartTicks = juce::Time::getHighResolutionTicks();

  // 1. Topological Sort (Kahn's Algorithm)
  std::map<NodeId, int> inDegree;
  std::map<NodeId, std::vector<NodeId>> adj;

  for (const auto &node : document.nodes)
    inDegree[node.nodeId] = 0;

  for (const auto &conn : document.connections) {
    if (!conn.isValid())
      continue;
    if (conn.from.isNodePort() && conn.to.isNodePort()) {
      adj[conn.from.nodeId].push_back(conn.to.nodeId);
      inDegree[conn.to.nodeId]++;
    }
  }

  std::queue<NodeId> q;
  for (const auto &pair : inDegree) {
    if (pair.second == 0)
      q.push(pair.first);
  }

  std::vector<NodeId> sortedIds;
  while (!q.empty()) {
    const NodeId curr = q.front();
    q.pop();
    sortedIds.push_back(curr);
    for (const NodeId neighbor : adj[curr]) {
      if (--inDegree[neighbor] == 0)
        q.push(neighbor);
    }
  }

  // 사이클 발견 시 빌드 실패
  if (sortedIds.size() != document.nodes.size())
    return nullptr;

  auto compiled = new TCompiledGraph();
  compiled->sortedNodes.reserve(sortedIds.size());

  int portChannelCounter = 0;
  std::map<PortId, int> globalPortIndexMap;
  std::map<juce::String, int> railInputChannelMap;

  // 2. Node Instantiation and Port Mapping
  for (const auto &id : sortedIds) {
    const TNode *node = document.findNode(id);
    if (node == nullptr)
      continue;

    TNodeEntry entry;
    entry.nodeId = node->nodeId;
    entry.nodeSnapshot = *node;

    for (const auto &port : entry.nodeSnapshot.ports) {
      entry.portChannels[port.portId] = portChannelCounter;
      globalPortIndexMap[port.portId] = portChannelCounter;
      ++portChannelCounter;

      if (port.dataType == TPortDataType::MIDI) {
        if (port.direction == TPortDirection::Input)
          entry.midiInputBuffers.emplace(port.portId, juce::MidiBuffer{});
        else
          entry.midiOutputBuffers.emplace(port.portId, juce::MidiBuffer{});
      }
    }

    const TNodeDescriptor *desc = registry.descriptorFor(entry.nodeSnapshot.typeKey);
    if (desc != nullptr && desc->instanceFactory) {
      entry.instance = desc->instanceFactory();
      if (entry.instance) {
        entry.instance->prepareToPlay(sampleRate, blockSize);
        entry.instance->reset();
        for (const auto &[key, value] : entry.nodeSnapshot.params)
          entry.instance->setParameterValue(key, paramValueToFloat(value));
      }
    }
    compiled->sortedNodes.push_back(std::move(entry));
  }

  // 포트 통계 계산
  compiled->nodeCount = static_cast<int>(compiled->sortedNodes.size());
  for (const auto& nodeEntry : compiled->sortedNodes) {
    for (const auto& port : nodeEntry.nodeSnapshot.ports) {
      if (port.dataType == TPortDataType::Audio)
        compiled->audioPortCount++;
      else if (port.dataType == TPortDataType::Control)
        compiled->controlPortCount++;
    }
  }

  // 3. Rail Setup
  for (const auto &endpoint : document.controlState.inputEndpoints) {
    for (const auto &port : endpoint.ports) {
      TRailInputSource source;
      source.endpointId = endpoint.endpointId;
      source.portId = port.portId;
      source.channelIndex = portChannelCounter;
      source.dataType = port.dataType;
      
      // 임시: 오디오 입력 채널 매핑 (추후 IOControl에서 상세화)
      source.deviceChannelIndex = -1; 
      
      railInputChannelMap[endpoint.endpointId + "::" + port.portId] = portChannelCounter;
      compiled->railInputSources.push_back(std::move(source));
      ++portChannelCounter;
    }
  }

  // 4. Connection Setup (Pre-process Mixes, MIDI Routes)
  std::map<NodeId, std::size_t> entryIndexByNodeId;
  for (std::size_t index = 0; index < compiled->sortedNodes.size(); ++index)
    entryIndexByNodeId[compiled->sortedNodes[index].nodeId] = index;

  for (const auto &conn : document.connections) {
    if (!conn.isValid())
      continue;

    if (conn.from.isNodePort() && conn.to.isNodePort()) {
      const auto srcIt = globalPortIndexMap.find(conn.from.portId);
      const auto dstEntryIt = entryIndexByNodeId.find(conn.to.nodeId);
      if (srcIt == globalPortIndexMap.end() || dstEntryIt == entryIndexByNodeId.end())
        continue;

      auto &dstEntry = compiled->sortedNodes[dstEntryIt->second];
      const auto dstPortIt = dstEntry.portChannels.find(conn.to.portId);
      if (dstPortIt == dstEntry.portChannels.end())
        continue;

      // MIDI 처리
      if (document.findNode(conn.from.nodeId)->findPort(conn.from.portId)->dataType == TPortDataType::MIDI) {
         TMidiRoute route;
         route.sourceNodeIndex = entryIndexByNodeId[conn.from.nodeId];
         route.sourcePortId = conn.from.portId;
         route.targetNodeIndex = dstEntryIt->second;
         route.targetPortId = conn.to.portId;
         compiled->midiRoutes.push_back(std::move(route));
      } else {
        dstEntry.preProcessMixes.push_back({srcIt->second, dstPortIt->second});
      }
    } else if (conn.from.isRailPort() && conn.to.isNodePort()) {
      const auto srcKey = conn.from.railEndpointId + "::" + conn.from.railPortId;
      const auto srcIt = railInputChannelMap.find(srcKey);
      const auto dstEntryIt = entryIndexByNodeId.find(conn.to.nodeId);
      if (srcIt != railInputChannelMap.end() && dstEntryIt != entryIndexByNodeId.end()) {
        auto &dstEntry = compiled->sortedNodes[dstEntryIt->second];
        const auto dstPortIt = dstEntry.portChannels.find(conn.to.portId);
        if (dstPortIt != dstEntry.portChannels.end()) {
          dstEntry.preProcessMixes.push_back({srcIt->second, dstPortIt->second});
        }
      }
    }
  }

  // 5. Finalize Compiled Graph
  compiled->totalAllocatedChannels = portChannelCounter;
  compiled->globalPortBuffer.setSize(juce::jmax(1, portChannelCounter),
                                     juce::jmax(1, blockSize));
  compiled->globalPortBuffer.clear();

  // Param Dispatch Setup
  for (auto &entry : compiled->sortedNodes) {
    const TNodeDescriptor *desc = registry.descriptorFor(entry.nodeSnapshot.typeKey);
    std::set<juce::String> keys;
    if (desc) {
      for (const auto &spec : desc->paramSpecs) keys.insert(spec.key);
    }
    for (const auto &[k, v] : entry.nodeSnapshot.params) keys.insert(k);

    for (const auto &key : keys) {
      const TParamSpec *spec = nullptr;
      if (desc) {
        for (const auto &s : desc->paramSpecs) if (s.key == key) { spec = &s; break; }
      }

      TParamDispatch d;
      d.nodeId = entry.nodeId;
      d.instance = entry.instance.get();
      d.paramKey = key;
      const auto valIt = entry.nodeSnapshot.params.find(key);
      const juce::var val = (valIt != entry.nodeSnapshot.params.end()) ? valIt->second : (spec ? spec->defaultValue : juce::var{});
      d.currentValue = paramValueToFloat(val);
      d.targetValue = d.currentValue;
      d.smoothingEnabled = shouldSmoothParam(spec, val);
      writeParamKey(d.paramKeyUtf8.data(), d.paramKeyUtf8.size(), key);
      compiled->paramDispatches.push_back(std::move(d));
    }
  }

  return compiled;
}

} // namespace Teul