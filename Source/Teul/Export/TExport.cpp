#include "TExport.h"

#include "../Serialization/TSerializer.h"

#include <cmath>
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
  case TExportIssueCode::DeadNodePruned:
    return "dead_node_pruned";
  case TExportIssueCode::AssetReferenceMissing:
    return "asset_reference_missing";
  case TExportIssueCode::AssetReferenceCopied:
    return "asset_reference_copied";
  case TExportIssueCode::ParamSkippedFromAPVTS:
    return "param_skipped_from_apvts";
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

juce::String sanitizeIdentifier(const juce::String &raw) {
  juce::String sanitized;
  sanitized.preallocateBytes(raw.length() + 8);

  auto isAsciiAlpha = [](auto c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  };
  auto isAsciiDigit = [](auto c) noexcept { return c >= '0' && c <= '9'; };

  for (int i = 0; i < raw.length(); ++i) {
    const auto c = raw[i];
    const bool keep = isAsciiAlpha(c) || isAsciiDigit(c) || c == '_';
    if (i == 0 && isAsciiDigit(c))
      sanitized << "_";
    sanitized << (keep ? juce::String::charToString(c) : juce::String("_"));
  }

  if (sanitized.isEmpty())
    sanitized = "GeneratedModule";

  return sanitized;
}

juce::String makeUniqueMemberName(const juce::String &preferredBase,
                                  std::set<juce::String> &usedNames) {
  const auto base = sanitizeIdentifier(preferredBase);
  if (usedNames.insert(base).second)
    return base;

  for (int suffix = 2; suffix < 10000; ++suffix) {
    const auto candidate = base + "_" + juce::String(suffix);
    if (usedNames.insert(candidate).second)
      return candidate;
  }

  const auto fallback = base + "_fallback";
  usedNames.insert(fallback);
  return fallback;
}

juce::String toCppStringLiteral(const juce::String &text) {
  return juce::JSON::toString(juce::var(text), false);
}

juce::String generatedAtUtcIsoString() {
  return juce::Time::getCurrentTime().toISO8601(true);
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

juce::Result ensureDirectory(const juce::File &directory) {
  if (directory.getFullPathName().isEmpty())
    return juce::Result::fail("Export output directory is empty");

  if (directory.exists()) {
    if (!directory.isDirectory())
      return juce::Result::fail("Export output path is not a directory: " +
                                directory.getFullPathName());
    return juce::Result::ok();
  }

  if (!directory.createDirectory())
    return juce::Result::fail("Failed to create directory: " +
                              directory.getFullPathName());

  return juce::Result::ok();
}

juce::Result writeTextFile(const juce::File &file,
                           const juce::String &text,
                           bool overwriteExisting) {
  const auto directoryResult = ensureDirectory(file.getParentDirectory());
  if (directoryResult.failed())
    return directoryResult;

  if (file.existsAsFile() && !overwriteExisting) {
    return juce::Result::fail("Refusing to overwrite existing file: " +
                              file.getFullPathName());
  }

  if (!file.replaceWithText(text, false, false, "\r\n")) {
    return juce::Result::fail("Failed to write file: " + file.getFullPathName());
  }

  return juce::Result::ok();
}

juce::String relativePathOrAbsolute(const juce::File &file,
                                    const juce::File &baseDirectory) {
  const auto relative = file.getRelativePathFrom(baseDirectory);
  return relative.isNotEmpty() ? relative.replaceCharacter('\\', '/')
                               : file.getFullPathName().replaceCharacter('\\', '/');
}

juce::File resolveInputFilePath(const juce::String &pathText,
                                const TExportOptions &options) {
  juce::File file(pathText.trim());
  if (juce::File::isAbsolutePath(pathText.trim()))
    return file;
  if (options.projectRootDirectory.exists())
    return options.projectRootDirectory.getChildFile(pathText.trim());
  return juce::File::getCurrentWorkingDirectory().getChildFile(pathText.trim());
}

bool looksLikeAssetPath(const juce::String &text) {
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return false;

  const auto lowered = trimmed.toLowerCase();
  if (lowered.contains("\\") || lowered.contains("/"))
    return true;

  for (const auto &ext : {".wav", ".aiff", ".aif", ".png", ".jpg", ".jpeg",
                          ".bmp", ".json", ".txt", ".bin", ".ttf"}) {
    if (lowered.endsWith(ext))
      return true;
  }

  return false;
}

juce::String preferredAssetRelativePath(const TExportAssetIR &asset) {
  const juce::File source(asset.sourcePath);
  const auto fileName = source.getFileName();
  return juce::String("Assets/") +
         sanitizeIdentifier(asset.nodeTypeKey.replaceCharacter('.', '_')) +
         "/" + fileName;
}

juce::File uniqueDestinationPath(const juce::File &baseDirectory,
                                 const juce::String &relativePath) {
  juce::File destination = baseDirectory.getChildFile(relativePath);
  if (!destination.exists())
    return destination;

  const auto extension = destination.getFileExtension();
  const auto stem = destination.getFileNameWithoutExtension();
  auto parent = destination.getParentDirectory();
  for (int suffix = 2; suffix < 10000; ++suffix) {
    const auto candidate = parent.getChildFile(stem + "_" + juce::String(suffix) + extension);
    if (!candidate.exists())
      return candidate;
  }

  return destination;
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

float varToFloat(const juce::var &value, float fallback = 0.0f) {
  if (value.isBool())
    return static_cast<bool>(value) ? 1.0f : 0.0f;
  if (value.isInt())
    return static_cast<float>((int)value);
  if (value.isInt64())
    return static_cast<float>((juce::int64)value);
  if (value.isDouble())
    return static_cast<float>((double)value);
  if (value.isString())
    return value.toString().getFloatValue();
  return fallback;
}

int varToInt(const juce::var &value, int fallback = 0) {
  if (value.isBool())
    return static_cast<bool>(value) ? 1 : 0;
  if (value.isInt())
    return (int)value;
  if (value.isInt64())
    return static_cast<int>((juce::int64)value);
  if (value.isDouble())
    return juce::roundToInt((double)value);
  if (value.isString())
    return value.toString().getIntValue();
  return fallback;
}

bool varToBool(const juce::var &value, bool fallback = false) {
  if (value.isBool())
    return static_cast<bool>(value);
  if (value.isInt() || value.isInt64() || value.isDouble())
    return std::abs((double)value) > 0.0000001;
  if (value.isString()) {
    const auto lowered = value.toString().trim().toLowerCase();
    return lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
  }
  return fallback;
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

std::vector<NodeId> collectLiveNodeIds(const TGraphDocument &document,
                                       TExportMode mode,
                                       bool optimizeForRuntimeModule) {
  std::set<NodeId> liveNodeIds;
  if (mode != TExportMode::RuntimeModule || !optimizeForRuntimeModule) {
    for (const auto &node : document.nodes)
      liveNodeIds.insert(node.nodeId);
    return {liveNodeIds.begin(), liveNodeIds.end()};
  }

  std::map<NodeId, std::vector<NodeId>> reverseAdjacency;
  for (const auto &connection : document.connections) {
    if (!connection.isValid())
      continue;
    reverseAdjacency[connection.to.nodeId].push_back(connection.from.nodeId);
  }

  std::queue<NodeId> queue;
  for (const auto &node : document.nodes) {
    if (node.typeKey == "Teul.Routing.AudioOut" ||
        node.typeKey == "Teul.Midi.MidiOut") {
      queue.push(node.nodeId);
      liveNodeIds.insert(node.nodeId);
    }
  }

  if (queue.empty()) {
    for (const auto &node : document.nodes)
      liveNodeIds.insert(node.nodeId);
    return {liveNodeIds.begin(), liveNodeIds.end()};
  }

  while (!queue.empty()) {
    const NodeId current = queue.front();
    queue.pop();
    for (const auto predecessor : reverseAdjacency[current]) {
      if (liveNodeIds.insert(predecessor).second)
        queue.push(predecessor);
    }
  }

  return {liveNodeIds.begin(), liveNodeIds.end()};
}

std::vector<NodeId> buildTopologicalOrder(const TGraphDocument &document,
                                          const std::set<NodeId> &includedNodeIds,
                                          bool &hasCycle) {
  std::map<NodeId, int> indegree;
  std::map<NodeId, std::vector<NodeId>> adjacency;

  for (const auto &node : document.nodes) {
    if (includedNodeIds.find(node.nodeId) != includedNodeIds.end())
      indegree[node.nodeId] = 0;
  }

  for (const auto &connection : document.connections) {
    if (!connection.isValid())
      continue;
    if (includedNodeIds.find(connection.from.nodeId) == includedNodeIds.end() ||
        includedNodeIds.find(connection.to.nodeId) == includedNodeIds.end()) {
      continue;
    }
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
  order.reserve(indegree.size());
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

  hasCycle = order.size() != indegree.size();
  return order;
}

void scanAssetReferences(const TGraphDocument &document,
                         const TNodeRegistry &registry,
                         const TExportOptions &options,
                         TExportReport &report,
                         std::vector<TExportAssetIR> &assetsOut) {
  std::set<juce::String> seenParamIds;
  for (const auto &node : document.nodes) {
    const auto *descriptor = registry.descriptorFor(node.typeKey);
    const auto params = buildNormalizedParams(node, descriptor);

    for (const auto &param : params) {
      if (!param.currentValue.isString())
        continue;

      const auto valueText = param.currentValue.toString().trim();
      if (!looksLikeAssetPath(valueText))
        continue;
      if (!seenParamIds.insert(param.paramId).second)
        continue;

      TExportAssetIR asset;
      asset.refKey = param.exportSymbol.isNotEmpty() ? param.exportSymbol : param.paramKey;
      asset.paramId = param.paramId;
      asset.nodeTypeKey = param.nodeTypeKey;
      asset.paramKey = param.paramKey;
      asset.sourcePath = resolveInputFilePath(valueText, options)
                             .getFullPathName()
                             .replaceCharacter('\\', '/');
      asset.exists = juce::File(asset.sourcePath).existsAsFile();
      assetsOut.push_back(asset);

      if (!asset.exists) {
        report.addIssue(TExportIssueSeverity::Warning,
                        TExportIssueCode::AssetReferenceMissing,
                        "Asset reference does not resolve to a file: " + valueText,
                        makeNodeLocation(node, descriptor));
      }
    }
  }
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
                      "Node contains duplicate port id " +
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
                      "Connection direction is invalid. Source must be Output and target must be Input.",
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
                    "RuntimeModule export requires at least one Audio Output node when the graph contains audio ports.",
                    firstAudioNodeLocation);
  }
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
                      "Node type is not registered in TNodeRegistry: " + node.typeKey + ".",
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

std::unique_ptr<TExportGraphIR>
buildGraphIR(const TGraphDocument &document,
             const TNodeRegistry &registry,
             const TExportOptions &options,
             TExportReport &report) {
  auto graph = std::make_unique<TExportGraphIR>();
  graph->schemaVersion = document.schemaVersion;
  graph->mode = options.mode;
  graph->generatedAtUtc = generatedAtUtcIsoString();
  graph->meta = document.meta;
  graph->assets = report.assets;

  const auto liveNodeList = collectLiveNodeIds(document, options.mode,
                                               options.optimizeForRuntimeModule);
  std::set<NodeId> liveNodeIds(liveNodeList.begin(), liveNodeList.end());
  report.summary.liveNodeCount = static_cast<int>(liveNodeIds.size());
  report.summary.prunedNodeCount =
      static_cast<int>(document.nodes.size()) - report.summary.liveNodeCount;

  for (const auto &node : document.nodes) {
    const auto *descriptor = registry.descriptorFor(node.typeKey);
    if (liveNodeIds.find(node.nodeId) == liveNodeIds.end()) {
      report.addIssue(TExportIssueSeverity::Info,
                      TExportIssueCode::DeadNodePruned,
                      "Node is not connected to a terminal output and was pruned from the RuntimeModule IR.",
                      makeNodeLocation(node, descriptor));
    }
  }

  bool hasCycle = false;
  const auto order = buildTopologicalOrder(document, liveNodeIds, hasCycle);
  if (hasCycle) {
    report.addIssue(TExportIssueSeverity::Error,
                    TExportIssueCode::CycleDetected,
                    "Graph contains a cycle and cannot be scheduled topologically for export.");
  }

  std::map<NodeId, const TNode *> nodeById;
  for (const auto &node : document.nodes)
    nodeById[node.nodeId] = &node;

  for (int index = 0; index < (int)order.size(); ++index) {
    const auto *node = nodeById[order[(size_t)index]];
    if (node == nullptr)
      continue;

    const auto *descriptor = registry.descriptorFor(node->typeKey);
    TExportNodeIR nodeIR;
    nodeIR.nodeId = node->nodeId;
    nodeIR.typeKey = node->typeKey;
    nodeIR.displayName = nodeDisplayName(*node, descriptor);
    nodeIR.category = descriptor != nullptr ? descriptor->category : juce::String();
    nodeIR.exportSupport = descriptor != nullptr
                               ? descriptor->exportSupport
                               : TNodeExportSupport::Unsupported;
    nodeIR.bypassed = node->bypassed;
    nodeIR.collapsed = node->collapsed;
    nodeIR.live = true;
    nodeIR.params = buildNormalizedParams(*node, descriptor);

    for (const auto &port : node->ports) {
      TExportPortIR portIR;
      portIR.portId = port.portId;
      portIR.direction = port.direction;
      portIR.dataType = port.dataType;
      portIR.name = port.name;
      portIR.channelIndex = port.channelIndex;
      nodeIR.ports.push_back(std::move(portIR));

      TExportBufferPlanEntry bufferEntry;
      bufferEntry.bufferIndex = report.summary.bufferEntryCount;
      bufferEntry.nodeId = node->nodeId;
      bufferEntry.portId = port.portId;
      bufferEntry.dataType = port.dataType;
      bufferEntry.portName = port.name;
      bufferEntry.channelIndex = port.channelIndex;
      graph->bufferPlan.push_back(std::move(bufferEntry));
      ++report.summary.bufferEntryCount;
      ++report.summary.normalizedPortCount;
    }

    for (const auto &param : nodeIR.params) {
      ++report.summary.normalizedParamCount;
      if (param.exposeToIeum)
        ++report.summary.exposedParamCount;
      if (param.isAutomatable)
        ++report.summary.automatableParamCount;
    }
    if (!supportsMode(nodeIR.exportSupport, options.mode))
      ++report.summary.unsupportedNodeCount;

    TExportScheduleEntry scheduleEntry;
    scheduleEntry.orderIndex = index;
    scheduleEntry.nodeId = node->nodeId;
    scheduleEntry.typeKey = node->typeKey;
    scheduleEntry.displayName = nodeIR.displayName;
    graph->schedule.push_back(std::move(scheduleEntry));
    ++report.summary.scheduleEntryCount;

    graph->nodes.push_back(std::move(nodeIR));
  }

  for (const auto &connection : document.connections) {
    if (liveNodeIds.find(connection.from.nodeId) == liveNodeIds.end() ||
        liveNodeIds.find(connection.to.nodeId) == liveNodeIds.end()) {
      continue;
    }

    const auto *fromNode = document.findNode(connection.from.nodeId);
    const auto *toNode = document.findNode(connection.to.nodeId);
    const auto *fromPort = fromNode != nullptr ? fromNode->findPort(connection.from.portId) : nullptr;
    const auto *toPort = toNode != nullptr ? toNode->findPort(connection.to.portId) : nullptr;

    TExportConnectionIR connectionIR;
    connectionIR.connectionId = connection.connectionId;
    connectionIR.from = connection.from;
    connectionIR.to = connection.to;
    connectionIR.dataType = fromPort != nullptr ? fromPort->dataType : TPortDataType::Audio;
    connectionIR.sourceNodeTypeKey = fromNode != nullptr ? fromNode->typeKey : juce::String();
    connectionIR.targetNodeTypeKey = toNode != nullptr ? toNode->typeKey : juce::String();
    connectionIR.sourcePortName = fromPort != nullptr ? fromPort->name : juce::String();
    connectionIR.targetPortName = toPort != nullptr ? toPort->name : juce::String();
    graph->connections.push_back(std::move(connectionIR));
  }

  return graph;
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

juce::Array<juce::var> assetArrayToJson(const std::vector<TExportAssetIR> &assets) {
  juce::Array<juce::var> array;
  for (const auto &asset : assets) {
    auto *object = new juce::DynamicObject();
    object->setProperty("refKey", asset.refKey);
    object->setProperty("paramId", asset.paramId);
    object->setProperty("nodeTypeKey", asset.nodeTypeKey);
    object->setProperty("paramKey", asset.paramKey);
    object->setProperty("sourcePath", asset.sourcePath);
    object->setProperty("destinationPath", asset.destinationRelativePath);
    object->setProperty("exists", asset.exists);
    object->setProperty("copied", asset.copied);
    object->setProperty("reused", asset.reused);
    array.add(juce::var(object));
  }
  return array;
}

std::vector<TExportParamIR> collectAPVTSParams(const TExportGraphIR &graph,
                                               TExportReport *report) {
  std::vector<TExportParamIR> params;
  std::set<juce::String> seenIds;

  for (const auto &node : graph.nodes) {
    for (const auto &param : node.params) {
      const bool supportsType = param.valueType == TParamValueType::Float ||
                                param.valueType == TParamValueType::Int ||
                                param.valueType == TParamValueType::Bool ||
                                param.valueType == TParamValueType::Enum;
      const bool shouldExport = param.exposeToIeum && param.isAutomatable &&
                                !param.isReadOnly && supportsType;
      if (!shouldExport) {
        if (report != nullptr && param.exposeToIeum && param.isAutomatable) {
          report->addIssue(TExportIssueSeverity::Info,
                           TExportIssueCode::ParamSkippedFromAPVTS,
                           "Parameter skipped from APVTS layout: " + param.paramId,
                           {});
          ++report->summary.skippedApvtsParamCount;
        }
        continue;
      }
      if (!seenIds.insert(param.paramId).second)
        continue;
      params.push_back(param);
    }
  }

  if (report != nullptr)
    report->summary.apvtsParamCount = static_cast<int>(params.size());
  return params;
}

juce::String buildRuntimeJsonText(const TExportGraphIR &graph) {
  return juce::JSON::toString(graph.toJson(), true);
}

juce::String buildManifestJsonText(const TExportGraphIR &graph,
                                   const TExportOptions &options,
                                   const TExportReport &report) {
  auto *root = new juce::DynamicObject();
  root->setProperty("manifestVersion", "1.0");
  root->setProperty("mode", exportModeToString(graph.mode));
  root->setProperty("schemaVersion", graph.schemaVersion);
  root->setProperty("generatedAtUtc", graph.generatedAtUtc);
  root->setProperty("runtimeClassName", report.runtimeClassName);
  root->setProperty("graphFile", report.graphFile.getFileName());
  root->setProperty("runtimeDataFile", report.runtimeDataFile.getFileName());
  root->setProperty("generatedHeaderFile", report.generatedHeaderFile.getFileName());
  root->setProperty("generatedSourceFile", report.generatedSourceFile.getFileName());
  root->setProperty("cmakeHintsFile", report.cmakeHintsFile.getFileName());
  root->setProperty("jucerHintsFile", report.jucerHintsFile.getFileName());
  root->setProperty("dryRunOnly", options.dryRunOnly);

  auto *summary = new juce::DynamicObject();
  summary->setProperty("nodeCount", report.summary.nodeCount);
  summary->setProperty("connectionCount", report.summary.connectionCount);
  summary->setProperty("liveNodeCount", report.summary.liveNodeCount);
  summary->setProperty("prunedNodeCount", report.summary.prunedNodeCount);
  summary->setProperty("scheduleEntryCount", report.summary.scheduleEntryCount);
  summary->setProperty("bufferEntryCount", report.summary.bufferEntryCount);
  summary->setProperty("normalizedPortCount", report.summary.normalizedPortCount);
  summary->setProperty("normalizedParamCount", report.summary.normalizedParamCount);
  summary->setProperty("exposedParamCount", report.summary.exposedParamCount);
  summary->setProperty("automatableParamCount", report.summary.automatableParamCount);
  summary->setProperty("apvtsParamCount", report.summary.apvtsParamCount);
  summary->setProperty("skippedApvtsParamCount", report.summary.skippedApvtsParamCount);
  summary->setProperty("unsupportedNodeCount", report.summary.unsupportedNodeCount);
  summary->setProperty("assetReferenceCount", report.summary.assetReferenceCount);
  summary->setProperty("copiedAssetCount", report.summary.copiedAssetCount);
  summary->setProperty("missingAssetCount", report.summary.missingAssetCount);
  root->setProperty("summary", juce::var(summary));
  root->setProperty("assets", juce::var(assetArrayToJson(report.assets)));

  juce::Array<juce::var> issueArray;
  for (const auto &issue : report.issues)
    issueArray.add(issueToJson(issue));
  root->setProperty("issues", juce::var(issueArray));
  root->setProperty("graph", graph.toJson());
  return juce::JSON::toString(juce::var(root), true);
}

juce::String generateHeaderCode(const juce::String &className) {
  juce::StringArray lines;
  lines.add("#pragma once");
  lines.add("");
  lines.add("#include <JuceHeader.h>");
  lines.add("#include \"Teul/Runtime/TGraphRuntime.h\"");
  lines.add("#include \"Teul/Registry/TNodeRegistry.h\"");
  lines.add("#include \"Teul/Serialization/TSerializer.h\"");
  lines.add("#include <cmath>");
  lines.add("#include <map>");
  lines.add("");
  lines.add("class " + className);
  lines.add("{");
  lines.add("public:");
  lines.add("    " + className + "();");
  lines.add("    ~" + className + "() = default;");
  lines.add("");
  lines.add("    void prepare(double sampleRate, int maximumExpectedSamplesPerBlock);");
  lines.add("    void reset();");
  lines.add("    void process(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiBuffer);");
  lines.add("");
  lines.add("    bool setParamById(const juce::String& paramId, const juce::var& value);");
  lines.add("    juce::var getParamById(const juce::String& paramId) const;");
  lines.add("    bool setParamByIndex(int index, const juce::var& value);");
  lines.add("    juce::var getParamByIndex(int index) const;");
  lines.add("    int paramCount() const noexcept;");
  lines.add("    int paramIndexForId(const juce::String& paramId) const noexcept;");
  lines.add("    juce::String paramIdForIndex(int index) const;");
  lines.add("    const juce::StringArray& nodeSchedule() const noexcept;");
  lines.add("");
  lines.add("    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();");
  lines.add("    static juce::String embeddedGraphJson();");
  lines.add("    static juce::String embeddedRuntimeJson();");
  lines.add("    static juce::String embeddedManifestJson();");
  lines.add("");
  lines.add("private:");
  lines.add("    void rebuildRuntime();");
  lines.add("");
  lines.add("    Teul::TGraphDocument document;");
  lines.add("    std::unique_ptr<Teul::TNodeRegistry> registry;");
  lines.add("    Teul::TGraphRuntime runtime;");
  lines.add("    juce::StringArray paramIds;");
  lines.add("    juce::StringArray scheduleEntries;");
  lines.add("    std::map<juce::String, int> paramIndexById;");
  lines.add("    double currentSampleRate = 48000.0;");
  lines.add("    int currentBlockSize = 256;");
  lines.add("};");
  lines.add("");
  return lines.joinIntoString("\n");
}

juce::String generateSourceCode(const juce::String &className,
                                const juce::String &graphJson,
                                const juce::String &runtimeJson,
                                const juce::String &manifestJson,
                                const TExportGraphIR &graph,
                                const std::vector<TExportParamIR> &apvtsParams) {
  juce::StringArray lines;
  lines.add("#include \"" + className + ".h\"");
  lines.add("");
  lines.add("namespace");
  lines.add("{");
  lines.add("    Teul::TGraphDocument parseEmbeddedDocument(const juce::String& jsonText)");
  lines.add("    {");
  lines.add("        Teul::TGraphDocument document;");
  lines.add("        auto parsed = juce::JSON::parse(jsonText);");
  lines.add("        if (!parsed.isVoid())");
  lines.add("            Teul::TSerializer::fromJson(document, parsed);");
  lines.add("        return document;");
  lines.add("    }");
  lines.add("}");
  lines.add("");
  lines.add(className + "::" + className + "() : registry(Teul::makeDefaultNodeRegistry()), runtime(registry.get())");
  lines.add("{");
  lines.add("    document = parseEmbeddedDocument(embeddedGraphJson());");
  lines.add("    rebuildRuntime();");
  for (int index = 0; index < (int)apvtsParams.size(); ++index) {
    lines.add("    paramIds.add(" + toCppStringLiteral(apvtsParams[(size_t)index].paramId) + ");");
    lines.add("    paramIndexById[paramIds[" + juce::String(index) + "]] = " + juce::String(index) + ";");
  }
  for (const auto &schedule : graph.schedule) {
    lines.add("    scheduleEntries.add(" +
              toCppStringLiteral(juce::String(schedule.orderIndex) + ":" + schedule.typeKey + ":" + schedule.displayName) +
              ");");
  }
  lines.add("}");
  lines.add("");
  lines.add("void " + className + "::rebuildRuntime()");
  lines.add("{");
  lines.add("    runtime.buildGraph(document);");
  lines.add("    runtime.prepareToPlay(currentSampleRate, currentBlockSize);");
  lines.add("}");
  lines.add("");
  lines.add("void " + className + "::prepare(double sampleRate, int maximumExpectedSamplesPerBlock)");
  lines.add("{");
  lines.add("    currentSampleRate = sampleRate;");
  lines.add("    currentBlockSize = maximumExpectedSamplesPerBlock;");
  lines.add("    runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);");
  lines.add("}");
  lines.add("");
  lines.add("void " + className + "::reset()");
  lines.add("{");
  lines.add("    runtime.releaseResources();");
  lines.add("    rebuildRuntime();");
  lines.add("}");
  lines.add("");
  lines.add("void " + className + "::process(juce::AudioBuffer<float>& audioBuffer, juce::MidiBuffer& midiBuffer)");
  lines.add("{");
  lines.add("    runtime.processBlock(audioBuffer, midiBuffer);");
  lines.add("}");
  lines.add("");
  lines.add("bool " + className + "::setParamById(const juce::String& paramId, const juce::var& value)");
  lines.add("{");
  lines.add("    return paramIndexForId(paramId) >= 0 && runtime.setParam(paramId, value);");
  lines.add("}");
  lines.add("");
  lines.add("juce::var " + className + "::getParamById(const juce::String& paramId) const");
  lines.add("{");
  lines.add("    return paramIndexForId(paramId) >= 0 ? runtime.getParam(paramId) : juce::var();");
  lines.add("}");
  lines.add("");
  lines.add("bool " + className + "::setParamByIndex(int index, const juce::var& value)");
  lines.add("{");
  lines.add("    if (index < 0 || index >= paramIds.size())");
  lines.add("        return false;");
  lines.add("    return setParamById(paramIds[index], value);");
  lines.add("}");
  lines.add("");
  lines.add("juce::var " + className + "::getParamByIndex(int index) const");
  lines.add("{");
  lines.add("    if (index < 0 || index >= paramIds.size())");
  lines.add("        return {};");
  lines.add("    return getParamById(paramIds[index]);");
  lines.add("}");
  lines.add("");
  lines.add("int " + className + "::paramCount() const noexcept");
  lines.add("{");
  lines.add("    return paramIds.size();");
  lines.add("}");
  lines.add("");
  lines.add("int " + className + "::paramIndexForId(const juce::String& paramId) const noexcept");
  lines.add("{");
  lines.add("    const auto it = paramIndexById.find(paramId);");
  lines.add("    return it != paramIndexById.end() ? it->second : -1;");
  lines.add("}");
  lines.add("");
  lines.add("juce::String " + className + "::paramIdForIndex(int index) const");
  lines.add("{");
  lines.add("    if (index < 0 || index >= paramIds.size())");
  lines.add("        return {};");
  lines.add("    return paramIds[index];");
  lines.add("}");
  lines.add("");
  lines.add("const juce::StringArray& " + className + "::nodeSchedule() const noexcept");
  lines.add("{");
  lines.add("    return scheduleEntries;");
  lines.add("}");
  lines.add("");
  lines.add("juce::AudioProcessorValueTreeState::ParameterLayout " + className + "::createParameterLayout()");
  lines.add("{");
  lines.add("    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;");
  for (const auto &param : apvtsParams) {
    if (param.valueType == TParamValueType::Bool) {
      lines.add("    params.push_back(std::make_unique<juce::AudioParameterBool>(" +
                toCppStringLiteral(param.paramId) + ", " +
                toCppStringLiteral(param.paramLabel) + ", " +
                juce::String(varToBool(param.defaultValue) ? "true" : "false") + "));");
    } else if (param.valueType == TParamValueType::Enum) {
      lines.add("    {");
      lines.add("        juce::StringArray choices;");
      for (const auto &option : param.enumOptions)
        lines.add("        choices.add(" + toCppStringLiteral(option.label) + ");");
      lines.add("        params.push_back(std::make_unique<juce::AudioParameterChoice>(" +
                toCppStringLiteral(param.paramId) + ", " +
                toCppStringLiteral(param.paramLabel) + ", choices, " +
                juce::String(juce::jlimit(0, juce::jmax(0, (int)param.enumOptions.size() - 1), varToInt(param.defaultValue))) + "));");
      lines.add("    }");
    } else if (param.valueType == TParamValueType::Int) {
      const int minValue = varToInt(param.minValue, 0);
      int maxValue = varToInt(param.maxValue, juce::jmax(minValue + 1, minValue));
      if (maxValue <= minValue)
        maxValue = minValue + 1;
      lines.add("    params.push_back(std::make_unique<juce::AudioParameterInt>(" +
                toCppStringLiteral(param.paramId) + ", " +
                toCppStringLiteral(param.paramLabel) + ", " +
                juce::String(minValue) + ", " + juce::String(maxValue) + ", " +
                juce::String(juce::jlimit(minValue, maxValue, varToInt(param.defaultValue, minValue))) + "));");
    } else {
      float minValue = varToFloat(param.minValue, 0.0f);
      float maxValue = varToFloat(param.maxValue, 1.0f);
      if (maxValue <= minValue)
        maxValue = minValue + 1.0f;
      const float stepValue = juce::jmax(0.0f, varToFloat(param.step, 0.0f));
      lines.add("    params.push_back(std::make_unique<juce::AudioParameterFloat>(" +
                toCppStringLiteral(param.paramId) + ", " +
                toCppStringLiteral(param.paramLabel) + ", " +
                "juce::NormalisableRange<float>(" + juce::String(minValue, 6) + "f, " +
                juce::String(maxValue, 6) + "f, " + juce::String(stepValue, 6) + "f), " +
                juce::String(varToFloat(param.defaultValue, minValue), 6) + "f));");
    }
  }
  lines.add("    return { params.begin(), params.end() };");
  lines.add("}");
  lines.add("");
  lines.add("juce::String " + className + "::embeddedGraphJson()");
  lines.add("{");
  lines.add("    return " + toCppStringLiteral(graphJson) + ";");
  lines.add("}");
  lines.add("");
  lines.add("juce::String " + className + "::embeddedRuntimeJson()");
  lines.add("{");
  lines.add("    return " + toCppStringLiteral(runtimeJson) + ";");
  lines.add("}");
  lines.add("");
  lines.add("juce::String " + className + "::embeddedManifestJson()");
  lines.add("{");
  lines.add("    return " + toCppStringLiteral(manifestJson) + ";");
  lines.add("}");
  lines.add("");
  return lines.joinIntoString("\n");
}

juce::String generateCMakeHintsSnippet(const TExportOptions &options,
                                       const TExportReport &report) {
  juce::StringArray lines;
  const auto projectRoot = options.projectRootDirectory.exists()
                               ? options.projectRootDirectory.getFullPathName()
                               : juce::String("<repo-root>");
  lines.add("# Auto-generated by Teul export.");
  lines.add("set(TEUL_EXPORT_GENERATED_HEADER " + toCppStringLiteral(report.generatedHeaderFile.getFileName()) + ")");
  lines.add("set(TEUL_EXPORT_GENERATED_SOURCE " + toCppStringLiteral(report.generatedSourceFile.getFileName()) + ")");
  lines.add("set(TEUL_EXPORT_REQUIRED_INCLUDE_DIRS");
  lines.add("    " + toCppStringLiteral(projectRoot + "/Source"));
  lines.add("    " + toCppStringLiteral(projectRoot + "/JuceLibraryCode"));
  lines.add("    " + toCppStringLiteral("C:/JUCE/modules"));
  lines.add(")");
  lines.add("# Example:");
  lines.add("# target_sources(<target> PRIVATE ${TEUL_EXPORT_GENERATED_HEADER} ${TEUL_EXPORT_GENERATED_SOURCE})");
  lines.add("# target_include_directories(<target> PRIVATE ${TEUL_EXPORT_REQUIRED_INCLUDE_DIRS})");
  return lines.joinIntoString("\n");
}

juce::String generateJucerHintsSnippet(const TExportOptions &options,
                                       const TExportReport &report) {
  juce::StringArray lines;
  const auto projectRoot = options.projectRootDirectory.exists()
                               ? options.projectRootDirectory.getFullPathName()
                               : juce::String("<repo-root>");
  lines.add("# Auto-generated by Teul export.");
  lines.add("Generated Header: " + report.generatedHeaderFile.getFileName());
  lines.add("Generated Source: " + report.generatedSourceFile.getFileName());
  lines.add("Header Search Paths: " + projectRoot + "/Source; " + projectRoot + "/JuceLibraryCode; C:/JUCE/modules");
  lines.add("Recommended: add generated files to the current target and keep Source in include search paths.");
  return lines.joinIntoString("\n");
}

juce::String buildEditableGraphJsonText(const TGraphDocument &document) {
  return juce::JSON::toString(TSerializer::toJson(document), true);
}

juce::File defaultOutputDirectoryFor(const TExportOptions &options) {
  const auto modeFolder = options.mode == TExportMode::RuntimeModule
                              ? juce::String("RuntimeModule")
                              : juce::String("EditableGraph");
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulExport")
      .getChildFile(modeFolder + "_" +
                    juce::String(juce::Time::currentTimeMillis()));
}

void assignOutputPaths(TExportReport &report, const TExportOptions &options) {
  const auto requestedClassName = options.runtimeClassName.trim();
  report.runtimeClassName = sanitizeIdentifier(
      requestedClassName.isNotEmpty() ? requestedClassName
                                      : juce::String("TeulExportedRuntimeModule"));

  report.outputDirectory = options.outputDirectory.getFullPathName().isNotEmpty()
                               ? options.outputDirectory
                               : defaultOutputDirectoryFor(options);
  report.graphFile = report.outputDirectory.getChildFile("editable-graph.teul");
  report.manifestFile =
      report.outputDirectory.getChildFile("export-manifest.json");
  report.runtimeDataFile =
      report.outputDirectory.getChildFile("export-runtime.json");
  report.generatedHeaderFile =
      report.outputDirectory.getChildFile(report.runtimeClassName + ".h");
  report.generatedSourceFile =
      report.outputDirectory.getChildFile(report.runtimeClassName + ".cpp");
  report.cmakeHintsFile =
      report.outputDirectory.getChildFile("import-into-cmake.cmake");
  report.jucerHintsFile =
      report.outputDirectory.getChildFile("import-into-projucer.txt");
  report.reportFile = report.outputDirectory.getChildFile("export-report.txt");
}

void collectAssetReferences(const TExportGraphIR &graph,
                           const TExportOptions &options,
                           TExportReport &report) {
  report.assets.clear();
  report.summary.assetReferenceCount = 0;
  report.summary.copiedAssetCount = 0;
  report.summary.missingAssetCount = 0;

  std::set<juce::String> seenParamIds;
  for (const auto &node : graph.nodes) {
    for (const auto &param : node.params) {
      if (!param.currentValue.isString())
        continue;

      const auto valueText = param.currentValue.toString().trim();
      if (!looksLikeAssetPath(valueText))
        continue;
      if (!seenParamIds.insert(param.paramId).second)
        continue;

      TExportAssetIR asset;
      asset.refKey = param.exportSymbol.isNotEmpty() ? param.exportSymbol
                                                     : param.paramKey;
      asset.paramId = param.paramId;
      asset.nodeTypeKey = param.nodeTypeKey;
      asset.paramKey = param.paramKey;

      const auto resolvedFile = resolveInputFilePath(valueText, options);
      asset.sourcePath =
          resolvedFile.getFullPathName().replaceCharacter('\\', '/');
      asset.destinationRelativePath =
          preferredAssetRelativePath(asset).replaceCharacter('\\', '/');
      asset.exists = resolvedFile.existsAsFile();

      report.assets.push_back(asset);
      ++report.summary.assetReferenceCount;

      if (!asset.exists) {
        ++report.summary.missingAssetCount;
        TExportIssueLocation location;
        location.nodeId = param.nodeId;
        location.nodeTypeKey = param.nodeTypeKey;
        location.nodeDisplayName = param.nodeDisplayName;
        report.addIssue(TExportIssueSeverity::Warning,
                        TExportIssueCode::AssetReferenceMissing,
                        "Asset reference does not resolve to a file: " +
                            valueText,
                        location);
      }
    }
  }
}

juce::Result packageAssetsIntoOutput(TExportGraphIR &graph,
                                     const TExportOptions &options,
                                     TExportReport &report) {
  if (!options.packageAssets || report.assets.empty()) {
    graph.assets = report.assets;
    return juce::Result::ok();
  }

  std::map<juce::String, juce::String> destinationBySource;
  for (auto &asset : report.assets) {
    if (!asset.exists)
      continue;

    const juce::File sourceFile(asset.sourcePath);
    auto existing = destinationBySource.find(asset.sourcePath);
    if (existing != destinationBySource.end()) {
      asset.destinationRelativePath = existing->second;
      asset.reused = true;
      continue;
    }

    juce::File destination =
        uniqueDestinationPath(report.outputDirectory, asset.destinationRelativePath);
    const auto ensureResult = ensureDirectory(destination.getParentDirectory());
    if (ensureResult.failed())
      return ensureResult;

    const auto samePath = destination.getFullPathName()
                              .equalsIgnoreCase(sourceFile.getFullPathName());
    if (!samePath) {
      if (destination.existsAsFile() && !options.overwriteExistingFiles) {
        return juce::Result::fail("Refusing to overwrite packaged asset: " +
                                  destination.getFullPathName());
      }

      if (destination.existsAsFile() && !destination.deleteFile()) {
        return juce::Result::fail("Failed to replace packaged asset: " +
                                  destination.getFullPathName());
      }

      if (!sourceFile.copyFileTo(destination)) {
        return juce::Result::fail("Failed to copy asset to export package: " +
                                  sourceFile.getFullPathName());
      }
    }

    asset.destinationRelativePath =
        relativePathOrAbsolute(destination, report.outputDirectory);
    asset.copied = true;
    destinationBySource[asset.sourcePath] = asset.destinationRelativePath;
    ++report.summary.copiedAssetCount;
    report.addIssue(TExportIssueSeverity::Info,
                    TExportIssueCode::AssetReferenceCopied,
                    "Packaged asset: " + asset.destinationRelativePath);
  }

  graph.assets = report.assets;
  return juce::Result::ok();
}

juce::Result parseJsonFile(const juce::File &file,
                           juce::var &jsonOut,
                           const juce::String &label) {
  if (!file.existsAsFile())
    return juce::Result::fail(label + " file does not exist: " +
                              file.getFullPathName());

  auto parseResult = juce::JSON::parse(file.loadFileAsString(), jsonOut);
  if (parseResult.failed()) {
    return juce::Result::fail("Failed to parse " + label + ": " +
                              parseResult.getErrorMessage());
  }

  return juce::Result::ok();
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
  root->setProperty("runtimeClassName", runtimeClassName);
  root->setProperty("outputDirectory", outputDirectory.getFullPathName());
  root->setProperty("graphFile", graphFile.getFullPathName());
  root->setProperty("manifestFile", manifestFile.getFullPathName());
  root->setProperty("runtimeDataFile", runtimeDataFile.getFullPathName());
  root->setProperty("generatedHeaderFile", generatedHeaderFile.getFullPathName());
  root->setProperty("generatedSourceFile", generatedSourceFile.getFullPathName());
  root->setProperty("cmakeHintsFile", cmakeHintsFile.getFullPathName());
  root->setProperty("jucerHintsFile", jucerHintsFile.getFullPathName());
  root->setProperty("reportFile", reportFile.getFullPathName());
  root->setProperty("warningCount", warningCount());
  root->setProperty("errorCount", errorCount());

  auto *summaryData = new juce::DynamicObject();
  summaryData->setProperty("nodeCount", summary.nodeCount);
  summaryData->setProperty("connectionCount", summary.connectionCount);
  summaryData->setProperty("liveNodeCount", summary.liveNodeCount);
  summaryData->setProperty("prunedNodeCount", summary.prunedNodeCount);
  summaryData->setProperty("scheduleEntryCount", summary.scheduleEntryCount);
  summaryData->setProperty("bufferEntryCount", summary.bufferEntryCount);
  summaryData->setProperty("normalizedPortCount", summary.normalizedPortCount);
  summaryData->setProperty("normalizedParamCount", summary.normalizedParamCount);
  summaryData->setProperty("exposedParamCount", summary.exposedParamCount);
  summaryData->setProperty("automatableParamCount", summary.automatableParamCount);
  summaryData->setProperty("apvtsParamCount", summary.apvtsParamCount);
  summaryData->setProperty("skippedApvtsParamCount", summary.skippedApvtsParamCount);
  summaryData->setProperty("unsupportedNodeCount", summary.unsupportedNodeCount);
  summaryData->setProperty("assetReferenceCount", summary.assetReferenceCount);
  summaryData->setProperty("copiedAssetCount", summary.copiedAssetCount);
  summaryData->setProperty("missingAssetCount", summary.missingAssetCount);
  root->setProperty("summary", juce::var(summaryData));
  root->setProperty("assets", juce::var(assetArrayToJson(assets)));

  juce::Array<juce::var> issueArray;
  for (const auto &issue : issues)
    issueArray.add(issueToJson(issue));
  root->setProperty("issues", juce::var(issueArray));
  return juce::var(root);
}

juce::String TExportReport::toText() const {
  juce::StringArray lines;
  lines.add("Teul Export Report");
  lines.add("==================");
  lines.add("Mode: " + exportModeToString(mode));
  lines.add("Dry Run: " + juce::String(dryRunOnly ? "yes" : "no"));
  lines.add("Backend Implemented: " +
            juce::String(backendImplemented ? "yes" : "no"));
  lines.add("Runtime Class: " + runtimeClassName);
  if (outputDirectory.getFullPathName().isNotEmpty())
    lines.add("Output Directory: " + outputDirectory.getFullPathName());
  lines.add("Nodes: " + juce::String(summary.nodeCount));
  lines.add("Connections: " + juce::String(summary.connectionCount));
  lines.add("Live Nodes: " + juce::String(summary.liveNodeCount));
  lines.add("Pruned Nodes: " + juce::String(summary.prunedNodeCount));
  lines.add("Schedule Entries: " + juce::String(summary.scheduleEntryCount));
  lines.add("Buffer Entries: " + juce::String(summary.bufferEntryCount));
  lines.add("Normalized Ports: " + juce::String(summary.normalizedPortCount));
  lines.add("Normalized Params: " + juce::String(summary.normalizedParamCount));
  lines.add("Exposed Params: " + juce::String(summary.exposedParamCount));
  lines.add("Automatable Params: " + juce::String(summary.automatableParamCount));
  lines.add("APVTS Params: " + juce::String(summary.apvtsParamCount));
  lines.add("APVTS Skipped: " + juce::String(summary.skippedApvtsParamCount));
  lines.add("Unsupported Nodes: " + juce::String(summary.unsupportedNodeCount));
  lines.add("Asset References: " + juce::String(summary.assetReferenceCount));
  lines.add("Copied Assets: " + juce::String(summary.copiedAssetCount));
  lines.add("Missing Assets: " + juce::String(summary.missingAssetCount));
  lines.add("Warnings: " + juce::String(warningCount()));
  lines.add("Errors: " + juce::String(errorCount()));
  if (graphFile.getFullPathName().isNotEmpty())
    lines.add("Graph File: " + graphFile.getFileName());
  if (runtimeDataFile.getFullPathName().isNotEmpty())
    lines.add("Runtime Data File: " + runtimeDataFile.getFileName());
  if (manifestFile.getFullPathName().isNotEmpty())
    lines.add("Manifest File: " + manifestFile.getFileName());
  lines.add("");
  lines.add("Issues:");

  if (issues.empty()) {
    lines.add("- INFO [ok] no issues");
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
  root->setProperty("generatedAtUtc", generatedAtUtc);

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
    nodeObject->setProperty("live", node.live);

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
      paramObject->setProperty("nodeId", static_cast<int>(param.nodeId));
      paramObject->setProperty("nodeTypeKey", param.nodeTypeKey);
      paramObject->setProperty("nodeDisplayName", param.nodeDisplayName);
      paramObject->setProperty("paramKey", param.paramKey);
      paramObject->setProperty("paramLabel", param.paramLabel);
      paramObject->setProperty("defaultValue", param.defaultValue);
      paramObject->setProperty("currentValue", param.currentValue);
      paramObject->setProperty("valueType", valueTypeToString(param.valueType));
      paramObject->setProperty("minValue", param.minValue);
      paramObject->setProperty("maxValue", param.maxValue);
      paramObject->setProperty("step", param.step);
      paramObject->setProperty("unitLabel", param.unitLabel);
      paramObject->setProperty("displayPrecision", param.displayPrecision);
      paramObject->setProperty("group", param.group);
      paramObject->setProperty("description", param.description);
      paramObject->setProperty("preferredWidget",
                               widgetHintToString(param.preferredWidget));
      paramObject->setProperty("showInNodeBody", param.showInNodeBody);
      paramObject->setProperty("showInPropertyPanel", param.showInPropertyPanel);
      paramObject->setProperty("isReadOnly", param.isReadOnly);
      paramObject->setProperty("isAutomatable", param.isAutomatable);
      paramObject->setProperty("isModulatable", param.isModulatable);
      paramObject->setProperty("isDiscrete", param.isDiscrete);
      paramObject->setProperty("exposeToIeum", param.exposeToIeum);
      paramObject->setProperty("preferredBindingKey", param.preferredBindingKey);
      paramObject->setProperty("exportSymbol", param.exportSymbol);
      paramObject->setProperty("categoryPath", param.categoryPath);

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
    connectionObject->setProperty("connectionId",
                                  static_cast<int>(connection.connectionId));
    connectionObject->setProperty("dataType",
                                  portDataTypeToString(connection.dataType));
    connectionObject->setProperty("sourceNodeTypeKey",
                                  connection.sourceNodeTypeKey);
    connectionObject->setProperty("targetNodeTypeKey",
                                  connection.targetNodeTypeKey);
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

  juce::Array<juce::var> scheduleArray;
  for (const auto &entry : schedule) {
    auto *entryObject = new juce::DynamicObject();
    entryObject->setProperty("orderIndex", entry.orderIndex);
    entryObject->setProperty("nodeId", static_cast<int>(entry.nodeId));
    entryObject->setProperty("typeKey", entry.typeKey);
    entryObject->setProperty("displayName", entry.displayName);
    scheduleArray.add(juce::var(entryObject));
  }

  juce::Array<juce::var> bufferArray;
  for (const auto &entry : bufferPlan) {
    auto *entryObject = new juce::DynamicObject();
    entryObject->setProperty("bufferIndex", entry.bufferIndex);
    entryObject->setProperty("nodeId", static_cast<int>(entry.nodeId));
    entryObject->setProperty("portId", static_cast<int>(entry.portId));
    entryObject->setProperty("dataType", portDataTypeToString(entry.dataType));
    entryObject->setProperty("portName", entry.portName);
    entryObject->setProperty("channelIndex", entry.channelIndex);
    bufferArray.add(juce::var(entryObject));
  }

  root->setProperty("nodes", juce::var(nodeArray));
  root->setProperty("connections", juce::var(connectionArray));
  root->setProperty("schedule", juce::var(scheduleArray));
  root->setProperty("bufferPlan", juce::var(bufferArray));
  root->setProperty("assets", juce::var(assetArrayToJson(assets)));
  return juce::var(root);
}

TExportResult TExporter::run(const TGraphDocument &document,
                             const TNodeRegistry &registry,
                             const TExportOptions &options) {
  TExportResult result;
  result.report.mode = options.mode;
  result.report.dryRunOnly = options.dryRunOnly;
  result.report.backendImplemented = true;
  result.report.runtimeClassName = sanitizeIdentifier(
      options.runtimeClassName.trim().isNotEmpty()
          ? options.runtimeClassName
          : juce::String("TeulExportedRuntimeModule"));
  result.report.outputDirectory = options.outputDirectory;
  result.report.summary.nodeCount = static_cast<int>(document.nodes.size());
  result.report.summary.connectionCount =
      static_cast<int>(document.connections.size());

  if (options.outputDirectory.getFullPathName().isNotEmpty())
    assignOutputPaths(result.report, options);

  validateNodes(document, registry, options, result.report);
  validateConnections(document, result.report);
  validatePrimaryAudioOutput(document, registry, options, result.report);

  result.graphIR = buildGraphIR(document, registry, options, result.report);
  if (result.graphIR != nullptr) {
    collectAssetReferences(*result.graphIR, options, result.report);
    result.graphIR->assets = result.report.assets;
    juce::ignoreUnused(collectAPVTSParams(*result.graphIR, &result.report));
  }

  return result;
}
juce::Result TExporter::exportToDirectory(const TGraphDocument &document,
                                          const TNodeRegistry &registry,
                                          const TExportOptions &options,
                                          TExportReport &reportOut) {
  auto effectiveOptions = options;
  effectiveOptions.dryRunOnly = false;

  auto result = run(document, registry, effectiveOptions);
  assignOutputPaths(result.report, effectiveOptions);
  reportOut = result.report;

  const auto directoryResult = ensureDirectory(reportOut.outputDirectory);
  if (directoryResult.failed())
    return directoryResult;

  auto writeReportAndFail = [&](const juce::String &message) {
    juce::ignoreUnused(
        writeTextFile(reportOut.reportFile, reportOut.toText(), true));
    return juce::Result::fail(message);
  };

  if (result.graphIR == nullptr)
    return writeReportAndFail("Export IR generation failed.");

  auto &graph = *result.graphIR;
  const auto packageResult =
      packageAssetsIntoOutput(graph, effectiveOptions, reportOut);
  if (packageResult.failed())
    return writeReportAndFail(packageResult.getErrorMessage());

  if (reportOut.hasErrors())
    return writeReportAndFail("Export validation failed.");

  const bool mustWriteGraph = effectiveOptions.writeGraphJson ||
                              effectiveOptions.mode == TExportMode::EditableGraph;
  if (mustWriteGraph) {
    const auto graphWriteResult = writeTextFile(
        reportOut.graphFile, buildEditableGraphJsonText(document),
        effectiveOptions.overwriteExistingFiles);
    if (graphWriteResult.failed())
      return writeReportAndFail(graphWriteResult.getErrorMessage());
  }

  if (effectiveOptions.writeRuntimeDataJson) {
    const auto runtimeWriteResult =
        writeTextFile(reportOut.runtimeDataFile, buildRuntimeJsonText(graph),
                      effectiveOptions.overwriteExistingFiles);
    if (runtimeWriteResult.failed())
      return writeReportAndFail(runtimeWriteResult.getErrorMessage());
  }

  if (effectiveOptions.mode == TExportMode::RuntimeModule &&
      effectiveOptions.writeRuntimeModuleFiles) {
    const auto apvtsParams = collectAPVTSParams(graph, nullptr);
    const auto graphJson = buildEditableGraphJsonText(document);
    const auto runtimeJson = buildRuntimeJsonText(graph);
    const auto manifestJson =
        buildManifestJsonText(graph, effectiveOptions, reportOut);

    const auto headerWriteResult =
        writeTextFile(reportOut.generatedHeaderFile,
                      generateHeaderCode(reportOut.runtimeClassName),
                      effectiveOptions.overwriteExistingFiles);
    if (headerWriteResult.failed())
      return writeReportAndFail(headerWriteResult.getErrorMessage());

    const auto sourceWriteResult = writeTextFile(
        reportOut.generatedSourceFile,
        generateSourceCode(reportOut.runtimeClassName, graphJson, runtimeJson,
                           manifestJson, graph, apvtsParams),
        effectiveOptions.overwriteExistingFiles);
    if (sourceWriteResult.failed())
      return writeReportAndFail(sourceWriteResult.getErrorMessage());

    if (effectiveOptions.writeBuildHints) {
      const auto cmakeWriteResult = writeTextFile(
          reportOut.cmakeHintsFile,
          generateCMakeHintsSnippet(effectiveOptions, reportOut),
          effectiveOptions.overwriteExistingFiles);
      if (cmakeWriteResult.failed())
        return writeReportAndFail(cmakeWriteResult.getErrorMessage());

      const auto jucerWriteResult = writeTextFile(
          reportOut.jucerHintsFile,
          generateJucerHintsSnippet(effectiveOptions, reportOut),
          effectiveOptions.overwriteExistingFiles);
      if (jucerWriteResult.failed())
        return writeReportAndFail(jucerWriteResult.getErrorMessage());
    }
  }

  if (effectiveOptions.writeManifestJson) {
    const auto manifestWriteResult = writeTextFile(
        reportOut.manifestFile,
        buildManifestJsonText(graph, effectiveOptions, reportOut),
        effectiveOptions.overwriteExistingFiles);
    if (manifestWriteResult.failed())
      return writeReportAndFail(manifestWriteResult.getErrorMessage());
  }

  const auto reportWriteResult =
      writeTextFile(reportOut.reportFile, reportOut.toText(), true);
  if (reportWriteResult.failed())
    return reportWriteResult;

  return juce::Result::ok();
}

juce::Result TExporter::importEditableGraphPackage(const juce::File &path,
                                                   TGraphDocument &documentOut) {
  juce::File graphFile;

  if (path.isDirectory()) {
    const auto manifestFile = path.getChildFile("export-manifest.json");
    if (manifestFile.existsAsFile()) {
      juce::var manifestJson;
      const auto parseResult = parseJsonFile(manifestFile, manifestJson, "manifest");
      if (parseResult.failed())
        return parseResult;

      if (auto *manifestObject = manifestJson.getDynamicObject()) {
        const auto graphFileName =
            manifestObject->getProperty("graphFile").toString().trim();
        if (graphFileName.isNotEmpty())
          graphFile = path.getChildFile(graphFileName);
      }
    }

    if (!graphFile.existsAsFile())
      graphFile = path.getChildFile("editable-graph.teul");
  } else if (path.existsAsFile()) {
    if (path.getFileName().equalsIgnoreCase("export-manifest.json")) {
      juce::var manifestJson;
      const auto parseResult = parseJsonFile(path, manifestJson, "manifest");
      if (parseResult.failed())
        return parseResult;

      if (auto *manifestObject = manifestJson.getDynamicObject()) {
        const auto graphFileName =
            manifestObject->getProperty("graphFile").toString().trim();
        if (graphFileName.isNotEmpty())
          graphFile = path.getParentDirectory().getChildFile(graphFileName);
      }
    } else {
      graphFile = path;
    }
  }

  if (!graphFile.existsAsFile()) {
    return juce::Result::fail(
        "EditableGraph package is missing editable-graph.teul.");
  }

  juce::var graphJson;
  const auto parseResult = parseJsonFile(graphFile, graphJson, "graph");
  if (parseResult.failed())
    return parseResult;

  if (!TSerializer::fromJson(documentOut, graphJson)) {
    return juce::Result::fail(
        "Failed to deserialize editable graph package: " +
        graphFile.getFullPathName());
  }

  return juce::Result::ok();
}

} // namespace Teul
