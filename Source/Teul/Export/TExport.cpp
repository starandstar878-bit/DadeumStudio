#include "TExport.h"

#include <map>
#include <queue>
#include <set>

namespace Teul {
namespace {

juce::String exportModeToString(TExportMode mode) {
  switch (mode) {
  case TExportMode::EditableGraph:
    return "editable_graph";
  case TExportMode::RuntimeModule:
    return "runtime_module";
  }

  return "editable_graph";
}

juce::String exportSupportToString(TNodeExportSupport support) {
  switch (support) {
  case TNodeExportSupport::Unsupported:
    return "unsupported";
  case TNodeExportSupport::JsonOnly:
    return "json_only";
  case TNodeExportSupport::RuntimeModuleOnly:
    return "runtime_module_only";
  case TNodeExportSupport::Both:
    return "both";
  }

  return "unsupported";
}

juce::String severityToString(TExportIssueSeverity severity) {
  switch (severity) {
  case TExportIssueSeverity::Info:
    return "INFO";
  case TExportIssueSeverity::Warning:
    return "WARN";
  case TExportIssueSeverity::Error:
    return "ERROR";
  }

  return "INFO";
}

juce::String issueCodeToString(TExportIssueCode code) {
  switch (code) {
  case TExportIssueCode::GraphEmpty:
    return "graph_empty";
  case TExportIssueCode::UnknownNodeDescriptor:
    return "unknown_node_descriptor";
  case TExportIssueCode::UnsupportedNodeForMode:
    return "unsupported_node_for_mode";
  case TExportIssueCode::NodePortLayoutMismatch:
    return "node_port_layout_mismatch";
  case TExportIssueCode::DuplicatePortId:
    return "duplicate_port_id";
  case TExportIssueCode::InvalidPortOwner:
    return "invalid_port_owner";
  case TExportIssueCode::CapabilityInstanceCountViolation:
    return "capability_instance_count_violation";
  case TExportIssueCode::InvalidConnectionEndpoint:
    return "invalid_connection_endpoint";
  case TExportIssueCode::MissingNodeForConnection:
    return "missing_node_for_connection";
  case TExportIssueCode::MissingPortForConnection:
    return "missing_port_for_connection";
  case TExportIssueCode::ConnectionDirectionMismatch:
    return "connection_direction_mismatch";
  case TExportIssueCode::ConnectionTypeMismatch:
    return "connection_type_mismatch";
  case TExportIssueCode::DuplicateConnection:
    return "duplicate_connection";
  case TExportIssueCode::CycleDetected:
    return "cycle_detected";
  case TExportIssueCode::MissingPrimaryAudioOutput:
    return "missing_primary_audio_output";
  case TExportIssueCode::InvalidChannelIndex:
    return "invalid_channel_index";
  case TExportIssueCode::NodeRuntimeWarning:
    return "node_runtime_warning";
  case TExportIssueCode::ExportBackendNotImplemented:
    return "export_backend_not_implemented";
  }

  return "graph_empty";
}

juce::String portDirectionToString(TPortDirection direction) {
  return direction == TPortDirection::Output ? "output" : "input";
}

juce::String portDataTypeToString(TPortDataType type) {
  switch (type) {
  case TPortDataType::Audio:
    return "audio";
  case TPortDataType::CV:
    return "cv";
  case TPortDataType::MIDI:
    return "midi";
  case TPortDataType::Gate:
    return "gate";
  case TPortDataType::Control:
    return "control";
  }

  return "audio";
}

juce::String valueTypeToString(TParamValueType type) {
  switch (type) {
  case TParamValueType::Auto:
    return "auto";
  case TParamValueType::Float:
    return "float";
  case TParamValueType::Int:
    return "int";
  case TParamValueType::Bool:
    return "bool";
  case TParamValueType::Enum:
    return "enum";
  case TParamValueType::String:
    return "string";
  }

  return "auto";
}

juce::String widgetHintToString(TParamWidgetHint hint) {
  switch (hint) {
  case TParamWidgetHint::Auto:
    return "auto";
  case TParamWidgetHint::Slider:
    return "slider";
  case TParamWidgetHint::Combo:
    return "combo";
  case TParamWidgetHint::Toggle:
    return "toggle";
  case TParamWidgetHint::Text:
    return "text";
  }

  return "auto";
}

bool supportsMode(TNodeExportSupport support, TExportMode mode) {
  switch (support) {
  case TNodeExportSupport::Unsupported:
    return false;
  case TNodeExportSupport::JsonOnly:
    return mode == TExportMode::EditableGraph;
  case TNodeExportSupport::RuntimeModuleOnly:
    return mode == TExportMode::RuntimeModule;
  case TNodeExportSupport::Both:
    return true;
  }

  return false;
}

juce::String nodeDisplayName(const TNode &node,
                             const TNodeDescriptor *descriptor) {
  if (node.label.isNotEmpty())
    return node.label;
  if (descriptor != nullptr && descriptor->displayName.isNotEmpty())
    return descriptor->displayName;
  return node.typeKey;
}

TExportIssueLocation makeNodeLocation(const TNode &node,
                                      const TNodeDescriptor *descriptor) {
  TExportIssueLocation location;
  location.nodeId = node.nodeId;
  location.nodeTypeKey = node.typeKey;
  location.nodeDisplayName = nodeDisplayName(node, descriptor);
  location.nodeX = node.x;
  location.nodeY = node.y;
  return location;
}

TExportIssueLocation makeConnectionLocation(const TConnection &connection) {
  TExportIssueLocation location;
  location.connectionId = connection.connectionId;
  location.from = connection.from;
  location.to = connection.to;
  return location;
}

TParamValueType inferValueType(const juce::var &value) {
  if (value.isBool())
    return TParamValueType::Bool;
  if (value.isInt() || value.isInt64())
    return TParamValueType::Int;
  if (value.isString())
    return TParamValueType::String;
  return TParamValueType::Float;
}

TParamWidgetHint inferWidgetHint(TParamValueType type) {
  switch (type) {
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

  return TParamWidgetHint::Auto;
}

TExportParamIR makeParamFromSpec(const TNode &node,
                                 const TNodeDescriptor &descriptor,
                                 const TParamSpec &spec) {
  TExportParamIR param;
  param.paramId = makeTeulParamId(node.nodeId, spec.key);
  param.nodeId = node.nodeId;
  param.nodeTypeKey = descriptor.typeKey;
  param.nodeDisplayName = nodeDisplayName(node, &descriptor);
  param.paramKey = spec.key;
  param.paramLabel = spec.label.isNotEmpty() ? spec.label : spec.key;
  param.defaultValue = spec.defaultValue;
  param.currentValue = spec.defaultValue;
  if (const auto it = node.params.find(spec.key); it != node.params.end())
    param.currentValue = it->second;
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
  return param;
}

TExportParamIR makeFallbackParam(const TNode &node,
                                 const juce::String &displayName,
                                 const juce::String &typeKey,
                                 const juce::String &key,
                                 const juce::var &value) {
  TExportParamIR param;
  param.paramId = makeTeulParamId(node.nodeId, key);
  param.nodeId = node.nodeId;
  param.nodeTypeKey = typeKey;
  param.nodeDisplayName = displayName;
  param.paramKey = key;
  param.paramLabel = key;
  param.defaultValue = value;
  param.currentValue = value;
  param.valueType = inferValueType(value);
  param.preferredWidget = inferWidgetHint(param.valueType);
  param.isDiscrete = param.valueType == TParamValueType::Bool ||
                     param.valueType == TParamValueType::Int;
  param.preferredBindingKey = key;
  param.exportSymbol = key;
  return param;
}

std::vector<TExportParamIR>
buildNormalizedParams(const TNode &node, const TNodeDescriptor *descriptor) {
  std::vector<TExportParamIR> result;
  std::set<juce::String> seenKeys;
  const auto displayName = nodeDisplayName(node, descriptor);
  const auto typeKey = descriptor != nullptr ? descriptor->typeKey : node.typeKey;

  if (descriptor != nullptr) {
    result.reserve(descriptor->paramSpecs.size() + node.params.size());
    for (const auto &spec : descriptor->paramSpecs) {
      result.push_back(makeParamFromSpec(node, *descriptor, spec));
      seenKeys.insert(spec.key);
    }
  } else {
    result.reserve(node.params.size());
  }

  for (const auto &[key, value] : node.params) {
    if (seenKeys.find(key) == seenKeys.end())
      result.push_back(makeFallbackParam(node, displayName, typeKey, key, value));
  }

  return result;
}

std::vector<NodeId> buildTopologicalOrder(const TGraphDocument &document,
                                          bool &hasCycle) {
  std::map<NodeId, int> indegree;
  std::map<NodeId, std::vector<NodeId>> adjacency;

  for (const auto &node : document.nodes)
    indegree[node.nodeId] = 0;

  for (const auto &connection : document.connections) {
    if (!connection.isValid())
      continue;
    if (document.findNode(connection.from.nodeId) == nullptr ||
        document.findNode(connection.to.nodeId) == nullptr) {
      continue;
    }

    adjacency[connection.from.nodeId].push_back(connection.to.nodeId);
    ++indegree[connection.to.nodeId];
  }

  std::queue<NodeId> ready;
  for (const auto &[nodeId, degree] : indegree) {
    if (degree == 0)
      ready.push(nodeId);
  }

  std::vector<NodeId> order;
  order.reserve(document.nodes.size());
  while (!ready.empty()) {
    const NodeId current = ready.front();
    ready.pop();
    order.push_back(current);

    for (const auto neighbor : adjacency[current]) {
      auto it = indegree.find(neighbor);
      if (it == indegree.end())
        continue;
      if (--it->second == 0)
        ready.push(neighbor);
    }
  }

  hasCycle = order.size() != document.nodes.size();
  return order;
}

juce::var paramOptionToJson(const TParamOptionSpec &option) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", option.id);
  object->setProperty("label", option.label);
  object->setProperty("value", option.value);
  return juce::var(object);
}

juce::var locationToJson(const TExportIssueLocation &location) {
  auto *object = new juce::DynamicObject();
  if (location.nodeId != kInvalidNodeId)
    object->setProperty("nodeId", static_cast<int>(location.nodeId));
  if (location.connectionId != kInvalidConnectionId)
    object->setProperty("connectionId", static_cast<int>(location.connectionId));
  if (location.portId != kInvalidPortId)
    object->setProperty("portId", static_cast<int>(location.portId));
  object->setProperty("nodeTypeKey", location.nodeTypeKey);
  object->setProperty("nodeDisplayName", location.nodeDisplayName);
  object->setProperty("nodeX", location.nodeX);
  object->setProperty("nodeY", location.nodeY);
  return juce::var(object);
}

juce::var issueToJson(const TExportIssue &issue) {
  auto *object = new juce::DynamicObject();
  object->setProperty("severity", severityToString(issue.severity));
  object->setProperty("code", issueCodeToString(issue.code));
  object->setProperty("message", issue.message);
  object->setProperty("location", locationToJson(issue.location));
  return juce::var(object);
}

juce::String formatLocationSuffix(const TExportIssueLocation &location) {
  if (location.hasNode()) {
    return " [node=" + juce::String(static_cast<int>(location.nodeId)) +
           ", type=" + location.nodeTypeKey + ", pos=(" +
           juce::String(location.nodeX, 1) + ", " +
           juce::String(location.nodeY, 1) + ")]";
  }
  if (location.hasConnection())
    return " [connection=" + juce::String(static_cast<int>(location.connectionId)) + "]";
  return {};
}

TExportIssueLocation enrichConnectionLocation(const TConnection &connection,
                                             const TNode *fromNode,
                                             const TNode *toNode) {
  TExportIssueLocation location = makeConnectionLocation(connection);
  const TNode *node = fromNode != nullptr ? fromNode : toNode;
  if (node != nullptr) {
    location.nodeId = node->nodeId;
    location.nodeTypeKey = node->typeKey;
    location.nodeDisplayName = node->label.isNotEmpty() ? node->label : node->typeKey;
    location.nodeX = node->x;
    location.nodeY = node->y;
  }
  return location;
}

void validateNodePorts(const TNode &node,
                       const TNodeDescriptor *descriptor,
                       TExportReport &report) {
  std::set<PortId> seenPortIds;
  for (const auto &port : node.ports) {
    auto location = makeNodeLocation(node, descriptor);
    location.portId = port.portId;

    if (!seenPortIds.insert(port.portId).second) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::DuplicatePortId,
                      "Node contains duplicate port id "+
                          juce::String(static_cast<int>(port.portId)) + ".",
                      location);
    }

    if (port.ownerNodeId != node.nodeId) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::InvalidPortOwner,
                      "Port ownerNodeId does not match the containing node.",
                      location);
    }

    if (port.channelIndex < 0) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::InvalidChannelIndex,
                      "Port channelIndex must be zero or positive.", location);
    }
  }

  if (descriptor == nullptr)
    return;

  if (node.ports.size() != descriptor->portSpecs.size()) {
    report.addIssue(TExportIssueSeverity::Error,
                    TExportIssueCode::NodePortLayoutMismatch,
                    "Node port count differs from descriptor layout (document=" +
                        juce::String(static_cast<int>(node.ports.size())) +
                        ", descriptor=" +
                        juce::String(static_cast<int>(descriptor->portSpecs.size())) +
                        ").",
                    makeNodeLocation(node, descriptor));
    return;
  }

  for (size_t i = 0; i < node.ports.size(); ++i) {
    const auto &actual = node.ports[i];
    const auto &expected = descriptor->portSpecs[i];
    if (actual.direction != expected.direction ||
        actual.dataType != expected.dataType || actual.name != expected.name) {
      auto location = makeNodeLocation(node, descriptor);
      location.portId = actual.portId;
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::NodePortLayoutMismatch,
                      "Node port layout differs from descriptor at index " +
                          juce::String(static_cast<int>(i)) + ".",
                      location);
    }
  }
}
void validateCapabilityCounts(const TGraphDocument &document,
                              const TNodeRegistry &registry,
                              TExportReport &report) {
  std::map<juce::String, int> instanceCounts;
  for (const auto &node : document.nodes)
    ++instanceCounts[node.typeKey];

  for (const auto &[typeKey, count] : instanceCounts) {
    const auto *descriptor = registry.descriptorFor(typeKey);
    if (descriptor == nullptr)
      continue;

    if (descriptor->capabilities.maxInstances >= 0 &&
        count > descriptor->capabilities.maxInstances) {
      TExportIssueLocation location;
      for (const auto &node : document.nodes) {
        if (node.typeKey == typeKey) {
          location = makeNodeLocation(node, descriptor);
          break;
        }
      }

      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::CapabilityInstanceCountViolation,
                      "Node type '" + typeKey +
                          "' exceeds maxInstances=" +
                          juce::String(descriptor->capabilities.maxInstances) +
                          " (graph has " + juce::String(count) + ").",
                      location);
    }
  }
}

void validateConnections(const TGraphDocument &document, TExportReport &report) {
  std::set<juce::String> seenConnections;

  for (const auto &connection : document.connections) {
    if (!connection.isValid()) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::InvalidConnectionEndpoint,
                      "Connection contains an invalid endpoint id.",
                      makeConnectionLocation(connection));
      continue;
    }

    const TNode *fromNode = document.findNode(connection.from.nodeId);
    const TNode *toNode = document.findNode(connection.to.nodeId);
    const auto location = enrichConnectionLocation(connection, fromNode, toNode);

    if (fromNode == nullptr || toNode == nullptr) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::MissingNodeForConnection,
                      "Connection references a node that does not exist.",
                      location);
      continue;
    }

    const TPort *fromPort = fromNode->findPort(connection.from.portId);
    const TPort *toPort = toNode->findPort(connection.to.portId);
    if (fromPort == nullptr || toPort == nullptr) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::MissingPortForConnection,
                      "Connection references a port that does not exist.",
                      location);
      continue;
    }

    if (fromPort->direction != TPortDirection::Output ||
        toPort->direction != TPortDirection::Input) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::ConnectionDirectionMismatch,
                      "Connection direction is invalid. Source must be Output "
                      "and target must be Input.",
                      location);
    }

    if (fromPort->dataType != toPort->dataType) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::ConnectionTypeMismatch,
                      "Connection data type mismatch: source=" +
                          portDataTypeToString(fromPort->dataType) +
                          ", target=" + portDataTypeToString(toPort->dataType) +
                          ".",
                      location);
    }

    const juce::String key =
        juce::String(static_cast<int>(connection.from.nodeId)) + ":" +
        juce::String(static_cast<int>(connection.from.portId)) + "->" +
        juce::String(static_cast<int>(connection.to.nodeId)) + ":" +
        juce::String(static_cast<int>(connection.to.portId));
    if (!seenConnections.insert(key).second) {
      report.addIssue(TExportIssueSeverity::Warning,
                      TExportIssueCode::DuplicateConnection,
                      "Graph contains duplicate connection endpoints.",
                      location);
    }
  }
}

void validateCycles(const TGraphDocument &document, TExportReport &report) {
  bool hasCycle = false;
  buildTopologicalOrder(document, hasCycle);
  if (hasCycle) {
    report.addIssue(TExportIssueSeverity::Error,
                    TExportIssueCode::CycleDetected,
                    "Graph contains a cycle and cannot be scheduled "
                    "topologically for export.");
  }
}

void validatePrimaryAudioOutput(const TGraphDocument &document,
                                const TNodeRegistry &registry,
                                const TExportOptions &options,
                                TExportReport &report) {
  if (!options.requirePrimaryAudioOutput ||
      options.mode != TExportMode::RuntimeModule) {
    return;
  }

  bool graphUsesAudio = false;
  int audioOutputCount = 0;
  TExportIssueLocation firstAudioNodeLocation;

  for (const auto &node : document.nodes) {
    const auto *descriptor = registry.descriptorFor(node.typeKey);
    for (const auto &port : node.ports) {
      if (port.dataType != TPortDataType::Audio)
        continue;
      graphUsesAudio = true;
      if (!firstAudioNodeLocation.hasNode())
        firstAudioNodeLocation = makeNodeLocation(node, descriptor);
    }

    if (node.typeKey == "Teul.Routing.AudioOut")
      ++audioOutputCount;
  }

  if (graphUsesAudio && audioOutputCount == 0) {
    report.addIssue(TExportIssueSeverity::Error,
                    TExportIssueCode::MissingPrimaryAudioOutput,
                    "RuntimeModule export requires at least one Audio Output "
                    "node when the graph contains audio ports.",
                    firstAudioNodeLocation);
  }
}

std::unique_ptr<TExportGraphIR>
buildGraphIR(const TGraphDocument &document,
             const TNodeRegistry &registry,
             const TExportOptions &options,
             TExportReport &report) {
  auto graph = std::make_unique<TExportGraphIR>();
  graph->schemaVersion = document.schemaVersion;
  graph->mode = options.mode;
  graph->meta = document.meta;

  bool hasCycle = false;
  const auto order = buildTopologicalOrder(document, hasCycle);
  std::map<NodeId, const TNode *> nodeById;
  for (const auto &node : document.nodes)
    nodeById[node.nodeId] = &node;

  std::set<NodeId> emitted;
  auto appendNode = [&](const TNode &node) {
    const auto *descriptor = registry.descriptorFor(node.typeKey);
    TExportNodeIR nodeIR;
    nodeIR.nodeId = node.nodeId;
    nodeIR.typeKey = node.typeKey;
    nodeIR.displayName = nodeDisplayName(node, descriptor);
    nodeIR.category = descriptor != nullptr ? descriptor->category : juce::String();
    nodeIR.exportSupport = descriptor != nullptr
                               ? descriptor->exportSupport
                               : TNodeExportSupport::Unsupported;
    nodeIR.bypassed = node.bypassed;
    nodeIR.collapsed = node.collapsed;
    nodeIR.params = buildNormalizedParams(node, descriptor);

    for (const auto &port : node.ports) {
      TExportPortIR portIR;
      portIR.portId = port.portId;
      portIR.direction = port.direction;
      portIR.dataType = port.dataType;
      portIR.name = port.name;
      portIR.channelIndex = port.channelIndex;
      nodeIR.ports.push_back(std::move(portIR));
    }

    report.summary.normalizedPortCount += static_cast<int>(nodeIR.ports.size());
    report.summary.normalizedParamCount += static_cast<int>(nodeIR.params.size());
    for (const auto &param : nodeIR.params) {
      if (param.exposeToIeum)
        ++report.summary.exposedParamCount;
      if (param.isAutomatable)
        ++report.summary.automatableParamCount;
    }
    if (!supportsMode(nodeIR.exportSupport, options.mode))
      ++report.summary.unsupportedNodeCount;

    graph->nodes.push_back(std::move(nodeIR));
    emitted.insert(node.nodeId);
  };

  for (const auto nodeId : order) {
    const auto it = nodeById.find(nodeId);
    if (it != nodeById.end() && it->second != nullptr)
      appendNode(*it->second);
  }

  for (const auto &node : document.nodes) {
    if (emitted.find(node.nodeId) == emitted.end())
      appendNode(node);
  }

  for (const auto &connection : document.connections) {
    const auto *fromNode = document.findNode(connection.from.nodeId);
    const auto *toNode = document.findNode(connection.to.nodeId);
    const auto *fromPort = fromNode != nullptr
                               ? fromNode->findPort(connection.from.portId)
                               : nullptr;
    const auto *toPort = toNode != nullptr
                             ? toNode->findPort(connection.to.portId)
                             : nullptr;

    TExportConnectionIR connectionIR;
    connectionIR.connectionId = connection.connectionId;
    connectionIR.from = connection.from;
    connectionIR.to = connection.to;
    connectionIR.dataType = fromPort != nullptr ? fromPort->dataType
                                                : TPortDataType::Audio;
    connectionIR.sourceNodeTypeKey = fromNode != nullptr ? fromNode->typeKey : juce::String();
    connectionIR.targetNodeTypeKey = toNode != nullptr ? toNode->typeKey : juce::String();
    connectionIR.sourcePortName = fromPort != nullptr ? fromPort->name : juce::String();
    connectionIR.targetPortName = toPort != nullptr ? toPort->name : juce::String();
    graph->connections.push_back(std::move(connectionIR));
  }

  return graph;
}

void validateNodes(const TGraphDocument &document,
                   const TNodeRegistry &registry,
                   const TExportOptions &options,
                   TExportReport &report) {
  if (document.nodes.empty()) {
    report.addIssue(TExportIssueSeverity::Warning, TExportIssueCode::GraphEmpty,
                    "Graph does not contain any nodes.");
    return;
  }

  for (const auto &node : document.nodes) {
    const auto *descriptor = registry.descriptorFor(node.typeKey);
    const auto location = makeNodeLocation(node, descriptor);

    if (descriptor == nullptr) {
      report.addIssue(options.requireKnownNodeDescriptors
                          ? TExportIssueSeverity::Error
                          : TExportIssueSeverity::Warning,
                      TExportIssueCode::UnknownNodeDescriptor,
                      "Node type is not registered in TNodeRegistry: " +
                          node.typeKey + ".",
                      location);
      continue;
    }

    if (!supportsMode(descriptor->exportSupport, options.mode)) {
      report.addIssue(TExportIssueSeverity::Error,
                      TExportIssueCode::UnsupportedNodeForMode,
                      "Node type '" + descriptor->typeKey +
                          "' does not support export mode '" +
                          exportModeToString(options.mode) +
                          "' (support=" +
                          exportSupportToString(descriptor->exportSupport) +
                          ").",
                      location);
    }

    validateNodePorts(node, descriptor, report);

    if (node.hasError || node.errorMessage.isNotEmpty()) {
      report.addIssue(TExportIssueSeverity::Warning,
                      TExportIssueCode::NodeRuntimeWarning,
                      node.errorMessage.isNotEmpty()
                          ? node.errorMessage
                          : "Node reports an editor/runtime warning state.",
                      location);
    }
  }

  validateCapabilityCounts(document, registry, report);
}

} // namespace
void TExportReport::addIssue(TExportIssueSeverity severity,
                             TExportIssueCode code,
                             const juce::String &message,
                             const TExportIssueLocation &location) {
  TExportIssue issue;
  issue.severity = severity;
  issue.code = code;
  issue.message = message;
  issue.location = location;
  issues.push_back(std::move(issue));
}

int TExportReport::warningCount() const noexcept {
  int count = 0;
  for (const auto &issue : issues) {
    if (issue.severity == TExportIssueSeverity::Warning)
      ++count;
  }
  return count;
}

int TExportReport::errorCount() const noexcept {
  int count = 0;
  for (const auto &issue : issues) {
    if (issue.severity == TExportIssueSeverity::Error)
      ++count;
  }
  return count;
}

bool TExportReport::hasErrors() const noexcept { return errorCount() > 0; }

juce::var TExportReport::toJson() const {
  auto *root = new juce::DynamicObject();
  root->setProperty("mode", exportModeToString(mode));
  root->setProperty("dryRunOnly", dryRunOnly);
  root->setProperty("backendImplemented", backendImplemented);
  root->setProperty("warningCount", warningCount());
  root->setProperty("errorCount", errorCount());

  auto *summaryData = new juce::DynamicObject();
  summaryData->setProperty("nodeCount", summary.nodeCount);
  summaryData->setProperty("connectionCount", summary.connectionCount);
  summaryData->setProperty("normalizedPortCount", summary.normalizedPortCount);
  summaryData->setProperty("normalizedParamCount", summary.normalizedParamCount);
  summaryData->setProperty("exposedParamCount", summary.exposedParamCount);
  summaryData->setProperty("automatableParamCount", summary.automatableParamCount);
  summaryData->setProperty("unsupportedNodeCount", summary.unsupportedNodeCount);
  root->setProperty("summary", juce::var(summaryData));

  juce::Array<juce::var> issueArray;
  for (const auto &issue : issues)
    issueArray.add(issueToJson(issue));
  root->setProperty("issues", juce::var(issueArray));
  return juce::var(root);
}

juce::String TExportReport::toText() const {
  juce::StringArray lines;
  lines.add("Teul Export Dry Run");
  lines.add("===================");
  lines.add("Mode: " + exportModeToString(mode));
  lines.add("Dry Run: " + juce::String(dryRunOnly ? "yes" : "no"));
  lines.add("Backend Implemented: " +
            juce::String(backendImplemented ? "yes" : "no"));
  lines.add("Nodes: " + juce::String(summary.nodeCount));
  lines.add("Connections: " + juce::String(summary.connectionCount));
  lines.add("Normalized Ports: " + juce::String(summary.normalizedPortCount));
  lines.add("Normalized Params: " + juce::String(summary.normalizedParamCount));
  lines.add("Exposed Params: " + juce::String(summary.exposedParamCount));
  lines.add("Automatable Params: " + juce::String(summary.automatableParamCount));
  lines.add("Unsupported Nodes: " + juce::String(summary.unsupportedNodeCount));
  lines.add("Warnings: " + juce::String(warningCount()));
  lines.add("Errors: " + juce::String(errorCount()));
  lines.add("");
  lines.add("Issues:");

  if (issues.empty()) {
    lines.add("- INFO: no issues");
  } else {
    for (const auto &issue : issues) {
      lines.add("- " + severityToString(issue.severity) + " [" +
                issueCodeToString(issue.code) + "] " + issue.message +
                formatLocationSuffix(issue.location));
    }
  }

  return lines.joinIntoString("\n");
}

juce::var TExportGraphIR::toJson() const {
  auto *root = new juce::DynamicObject();
  root->setProperty("schemaVersion", schemaVersion);
  root->setProperty("mode", exportModeToString(mode));

  auto *metaObject = new juce::DynamicObject();
  metaObject->setProperty("name", meta.name);
  metaObject->setProperty("canvasOffsetX", meta.canvasOffsetX);
  metaObject->setProperty("canvasOffsetY", meta.canvasOffsetY);
  metaObject->setProperty("canvasZoom", meta.canvasZoom);
  metaObject->setProperty("sampleRate", meta.sampleRate);
  metaObject->setProperty("blockSize", meta.blockSize);
  root->setProperty("meta", juce::var(metaObject));

  juce::Array<juce::var> nodeArray;
  for (const auto &node : nodes) {
    auto *nodeObject = new juce::DynamicObject();
    nodeObject->setProperty("nodeId", static_cast<int>(node.nodeId));
    nodeObject->setProperty("typeKey", node.typeKey);
    nodeObject->setProperty("displayName", node.displayName);
    nodeObject->setProperty("category", node.category);
    nodeObject->setProperty("exportSupport",
                            exportSupportToString(node.exportSupport));
    nodeObject->setProperty("bypassed", node.bypassed);
    nodeObject->setProperty("collapsed", node.collapsed);

    juce::Array<juce::var> portArray;
    for (const auto &port : node.ports) {
      auto *portObject = new juce::DynamicObject();
      portObject->setProperty("portId", static_cast<int>(port.portId));
      portObject->setProperty("direction", portDirectionToString(port.direction));
      portObject->setProperty("dataType", portDataTypeToString(port.dataType));
      portObject->setProperty("name", port.name);
      portObject->setProperty("channelIndex", port.channelIndex);
      portArray.add(juce::var(portObject));
    }

    juce::Array<juce::var> paramArray;
    for (const auto &param : node.params) {
      auto *paramObject = new juce::DynamicObject();
      paramObject->setProperty("paramId", param.paramId);
      paramObject->setProperty("paramKey", param.paramKey);
      paramObject->setProperty("paramLabel", param.paramLabel);
      paramObject->setProperty("defaultValue", param.defaultValue);
      paramObject->setProperty("currentValue", param.currentValue);
      paramObject->setProperty("valueType", valueTypeToString(param.valueType));
      paramObject->setProperty("preferredWidget", widgetHintToString(param.preferredWidget));
      paramObject->setProperty("isAutomatable", param.isAutomatable);
      paramObject->setProperty("isModulatable", param.isModulatable);
      paramObject->setProperty("exposeToIeum", param.exposeToIeum);
      paramObject->setProperty("exportSymbol", param.exportSymbol);

      juce::Array<juce::var> optionArray;
      for (const auto &option : param.enumOptions)
        optionArray.add(paramOptionToJson(option));
      paramObject->setProperty("enumOptions", juce::var(optionArray));
      paramArray.add(juce::var(paramObject));
    }

    nodeObject->setProperty("ports", juce::var(portArray));
    nodeObject->setProperty("params", juce::var(paramArray));
    nodeArray.add(juce::var(nodeObject));
  }

  juce::Array<juce::var> connectionArray;
  for (const auto &connection : connections) {
    auto *connectionObject = new juce::DynamicObject();
    connectionObject->setProperty("connectionId", static_cast<int>(connection.connectionId));
    connectionObject->setProperty("dataType", portDataTypeToString(connection.dataType));
    connectionObject->setProperty("sourceNodeTypeKey", connection.sourceNodeTypeKey);
    connectionObject->setProperty("targetNodeTypeKey", connection.targetNodeTypeKey);
    connectionObject->setProperty("sourcePortName", connection.sourcePortName);
    connectionObject->setProperty("targetPortName", connection.targetPortName);

    auto *fromObject = new juce::DynamicObject();
    fromObject->setProperty("nodeId", static_cast<int>(connection.from.nodeId));
    fromObject->setProperty("portId", static_cast<int>(connection.from.portId));
    connectionObject->setProperty("from", juce::var(fromObject));

    auto *toObject = new juce::DynamicObject();
    toObject->setProperty("nodeId", static_cast<int>(connection.to.nodeId));
    toObject->setProperty("portId", static_cast<int>(connection.to.portId));
    connectionObject->setProperty("to", juce::var(toObject));

    connectionArray.add(juce::var(connectionObject));
  }

  root->setProperty("nodes", juce::var(nodeArray));
  root->setProperty("connections", juce::var(connectionArray));
  return juce::var(root);
}

TExportResult TExporter::run(const TGraphDocument &document,
                             const TNodeRegistry &registry,
                             const TExportOptions &options) {
  TExportResult result;
  result.report.mode = options.mode;
  result.report.dryRunOnly = options.dryRunOnly;
  result.report.backendImplemented = false;
  result.report.summary.nodeCount = static_cast<int>(document.nodes.size());
  result.report.summary.connectionCount = static_cast<int>(document.connections.size());

  validateNodes(document, registry, options, result.report);
  validateConnections(document, result.report);
  validateCycles(document, result.report);
  validatePrimaryAudioOutput(document, registry, options, result.report);

  result.graphIR = buildGraphIR(document, registry, options, result.report);

  if (!options.dryRunOnly) {
    result.report.addIssue(TExportIssueSeverity::Error,
                           TExportIssueCode::ExportBackendNotImplemented,
                           "Teul export foundation is currently dry-run only. "
                           "Artifact generation has not been implemented yet.");
  }

  return result;
}

} // namespace Teul