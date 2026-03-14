#include "TBridgeJsonCodec.h"

namespace Teul {

juce::var TBridgeJsonCodec::encodeReport(const TExportReport &report) {
  auto *root = new juce::DynamicObject();
  root->setProperty("mode", exportModeToString(report.mode));
  root->setProperty("dryRunOnly", report.dryRunOnly);
  root->setProperty("runtimeClassName", report.runtimeClassName);
  root->setProperty("outputDirectory", report.outputDirectory.getFullPathName());
  root->setProperty("graphFile", report.graphFile.getFileName());
  root->setProperty("manifestFile", report.manifestFile.getFileName());
  root->setProperty("runtimeDataFile", report.runtimeDataFile.getFileName());
  root->setProperty("warningCount", report.summary.warningCount);
  root->setProperty("errorCount", report.summary.errorCount);

  auto *summary = new juce::DynamicObject();
  summary->setProperty("nodeCount", report.summary.nodeCount);
  summary->setProperty("connectionCount", report.summary.connectionCount);
  summary->setProperty("liveNodeCount", report.summary.liveNodeCount);
  summary->setProperty("prunedNodeCount", report.summary.prunedNodeCount);
  summary->setProperty("exposedParamCount", report.summary.exposedParamCount);
  summary->setProperty("assetReferenceCount", report.summary.assetReferenceCount);
  summary->setProperty("copiedAssetCount", report.summary.copiedAssetCount);
  summary->setProperty("missingAssetCount", report.summary.missingAssetCount);
  root->setProperty("summary", juce::var(summary));

  juce::Array<juce::var> assetArray;
  for (const auto &asset : report.assets)
    assetArray.add(encodeAsset(asset));
  root->setProperty("assets", juce::var(assetArray));

  juce::Array<juce::var> issueArray;
  for (const auto &issue : report.issues)
    issueArray.add(encodeIssue(issue));
  root->setProperty("issues", juce::var(issueArray));

  return juce::var(root);
}

juce::var TBridgeJsonCodec::encodeGraphIR(const TExportGraphIR &graph) {
  auto *root = new juce::DynamicObject();
  root->setProperty("schemaVersion", graph.schemaVersion);
  root->setProperty("mode", exportModeToString(graph.mode));
  root->setProperty("generatedAtUtc", graph.generatedAtUtc);

  auto *meta = new juce::DynamicObject();
  meta->setProperty("name", graph.meta.name);
  meta->setProperty("canvasOffsetX", graph.meta.canvasOffsetX);
  meta->setProperty("canvasOffsetY", graph.meta.canvasOffsetY);
  meta->setProperty("canvasZoom", graph.meta.canvasZoom);
  meta->setProperty("sampleRate", graph.meta.sampleRate);
  meta->setProperty("blockSize", graph.meta.blockSize);
  root->setProperty("meta", juce::var(meta));

  juce::Array<juce::var> nodeArray;
  for (const auto &node : graph.nodes) {
    auto *nodeObj = new juce::DynamicObject();
    nodeObj->setProperty("nodeId", static_cast<int>(node.nodeId));
    nodeObj->setProperty("typeKey", node.typeKey);
    nodeObj->setProperty("displayName", node.displayName);
    nodeObj->setProperty("category", node.category);
    nodeObj->setProperty("bypassed", node.bypassed);
    nodeObj->setProperty("collapsed", node.collapsed);
    nodeObj->setProperty("live", node.live);

    juce::Array<juce::var> portArray;
    for (const auto &port : node.ports) {
      auto *portObj = new juce::DynamicObject();
      portObj->setProperty("portId", static_cast<int>(port.portId));
      portObj->setProperty("direction", portDirectionToString(port.direction));
      portObj->setProperty("dataType", portDataTypeToString(port.dataType));
      portObj->setProperty("name", port.name);
      portObj->setProperty("channelIndex", port.channelIndex);
      portArray.add(juce::var(portObj));
    }
    nodeObj->setProperty("ports", juce::var(portArray));

    juce::Array<juce::var> paramArray;
    for (const auto &param : node.params) {
      auto *paramObj = new juce::DynamicObject();
      paramObj->setProperty("paramId", param.paramId);
      paramObj->setProperty("nodeId", static_cast<int>(param.nodeId));
      paramObj->setProperty("paramKey", param.paramKey);
      paramObj->setProperty("paramLabel", param.paramLabel);
      paramObj->setProperty("defaultValue", param.defaultValue);
      paramObj->setProperty("currentValue", param.currentValue);
      paramObj->setProperty("valueType", valueTypeToString(param.valueType));
      paramObj->setProperty("minValue", param.minValue);
      paramObj->setProperty("maxValue", param.maxValue);
      paramObj->setProperty("step", param.step);
      paramObj->setProperty("unitLabel", param.unitLabel);
      paramObj->setProperty("displayPrecision", param.displayPrecision);
      paramObj->setProperty("group", param.group);
      paramObj->setProperty("preferredWidget", widgetHintToString(param.preferredWidget));
      paramObj->setProperty("isReadOnly", param.isReadOnly);
      paramObj->setProperty("isAutomatable", param.isAutomatable);
      paramObj->setProperty("isDiscrete", param.isDiscrete);
      paramObj->setProperty("exposeToIeum", param.exposeToIeum);
      paramArray.add(juce::var(paramObj));
    }
    nodeObj->setProperty("params", juce::var(paramArray));
    
    nodeArray.add(juce::var(nodeObj));
  }
  root->setProperty("nodes", juce::var(nodeArray));

  juce::Array<juce::var> connArray;
  for (const auto &conn : graph.connections) {
    auto *connObj = new juce::DynamicObject();
    connObj->setProperty("connectionId", static_cast<int>(conn.connectionId));
    connObj->setProperty("dataType", portDataTypeToString(conn.dataType));
    connObj->setProperty("sourcePortName", conn.sourcePortName);
    connObj->setProperty("targetPortName", conn.targetPortName);

    auto *from = new juce::DynamicObject();
    from->setProperty("nodeId", static_cast<int>(conn.from.nodeId));
    from->setProperty("portId", static_cast<int>(conn.from.portId));
    connObj->setProperty("from", juce::var(from));

    auto *to = new juce::DynamicObject();
    to->setProperty("nodeId", static_cast<int>(conn.to.nodeId));
    to->setProperty("portId", static_cast<int>(conn.to.portId));
    connObj->setProperty("to", juce::var(to));

    connArray.add(juce::var(connObj));
  }
  root->setProperty("connections", juce::var(connArray));

  juce::Array<juce::var> assetArray;
  for (const auto &asset : graph.assets)
    assetArray.add(encodeAsset(asset));
  root->setProperty("assets", juce::var(assetArray));

  return juce::var(root);
}

juce::var TBridgeJsonCodec::encodeIssue(const TExportIssue &issue) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("severity", severityToString(issue.severity));
  obj->setProperty("code", issueCodeToString(issue.code));
  obj->setProperty("message", issue.message);

  auto *loc = new juce::DynamicObject();
  loc->setProperty("nodeId", static_cast<int>(issue.location.nodeId));
  loc->setProperty("connectionId", static_cast<int>(issue.location.connectionId));
  loc->setProperty("portId", static_cast<int>(issue.location.portId));
  loc->setProperty("nodeTypeKey", issue.location.nodeTypeKey);
  loc->setProperty("nodeDisplayName", issue.location.nodeDisplayName);
  loc->setProperty("nodeX", issue.location.nodeX);
  loc->setProperty("nodeY", issue.location.nodeY);
  obj->setProperty("location", juce::var(loc));

  return juce::var(obj);
}

juce::var TBridgeJsonCodec::encodeAsset(const TExportAssetIR &asset) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("refKey", asset.refKey);
  obj->setProperty("paramId", asset.paramId);
  obj->setProperty("nodeTypeKey", asset.nodeTypeKey);
  obj->setProperty("paramKey", asset.paramKey);
  obj->setProperty("sourcePath", asset.sourcePath);
  obj->setProperty("destinationRelativePath", asset.destinationRelativePath);
  obj->setProperty("exists", asset.exists);
  obj->setProperty("copied", asset.copied);
  obj->setProperty("reused", asset.reused);
  return juce::var(obj);
}

juce::String TBridgeJsonCodec::exportModeToString(TExportMode mode) {
  return mode == TExportMode::RuntimeModule ? "runtime_module" : "editable_graph";
}

juce::String TBridgeJsonCodec::severityToString(TExportIssueSeverity severity) {
  switch (severity) {
    case TExportIssueSeverity::Info: return "INFO";
    case TExportIssueSeverity::Warning: return "WARN";
    case TExportIssueSeverity::Error: return "ERROR";
  }
  return "INFO";
}

juce::String TBridgeJsonCodec::issueCodeToString(TExportIssueCode code) {
  switch (code) {
    case TExportIssueCode::GraphEmpty: return "graph_empty";
    case TExportIssueCode::UnknownNodeDescriptor: return "unknown_node_descriptor";
    case TExportIssueCode::UnsupportedNodeForMode: return "unsupported_node_for_mode";
    case TExportIssueCode::NodePortLayoutMismatch: return "node_port_layout_mismatch";
    case TExportIssueCode::DuplicatePortId: return "duplicate_port_id";
    case TExportIssueCode::InvalidPortOwner: return "invalid_port_owner";
    case TExportIssueCode::CapabilityInstanceCountViolation: return "capability_instance_count_violation";
    case TExportIssueCode::InvalidConnectionEndpoint: return "invalid_connection_endpoint";
    case TExportIssueCode::MissingNodeForConnection: return "missing_node_for_connection";
    case TExportIssueCode::MissingPortForConnection: return "missing_port_for_connection";
    case TExportIssueCode::ConnectionDirectionMismatch: return "connection_direction_mismatch";
    case TExportIssueCode::ConnectionTypeMismatch: return "connection_type_mismatch";
    case TExportIssueCode::DuplicateConnection: return "duplicate_connection";
    case TExportIssueCode::CycleDetected: return "cycle_detected";
    case TExportIssueCode::MissingPrimaryAudioOutput: return "missing_primary_audio_output";
    case TExportIssueCode::InvalidChannelIndex: return "invalid_channel_index";
    case TExportIssueCode::NodeRuntimeWarning: return "node_runtime_warning";
    case TExportIssueCode::DeadNodePruned: return "dead_node_pruned";
    case TExportIssueCode::AssetReferenceMissing: return "asset_reference_missing";
    case TExportIssueCode::AssetReferenceCopied: return "asset_reference_copied";
    case TExportIssueCode::ParamSkippedFromAPVTS: return "param_skipped_from_apvts";
  }
  return "unknown";
}

juce::String TBridgeJsonCodec::portDataTypeToString(TPortDataType type) {
  switch (type) {
    case TPortDataType::Audio: return "audio";
    case TPortDataType::CV: return "cv";
    case TPortDataType::MIDI: return "midi";
    case TPortDataType::Gate: return "gate";
    case TPortDataType::Control: return "control";
  }
  return "audio";
}

juce::String TBridgeJsonCodec::portDirectionToString(TPortDirection direction) {
  return direction == TPortDirection::Output ? "output" : "input";
}

juce::String TBridgeJsonCodec::valueTypeToString(TParamValueType type) {
  switch (type) {
    case TParamValueType::Auto: return "auto";
    case TParamValueType::Float: return "float";
    case TParamValueType::Int: return "int";
    case TParamValueType::Bool: return "bool";
    case TParamValueType::Enum: return "enum";
    case TParamValueType::String: return "string";
  }
  return "auto";
}

juce::String TBridgeJsonCodec::widgetHintToString(TParamWidgetHint hint) {
  switch (hint) {
    case TParamWidgetHint::Auto: return "auto";
    case TParamWidgetHint::Slider: return "slider";
    case TParamWidgetHint::Combo: return "combo";
    case TParamWidgetHint::Toggle: return "toggle";
    case TParamWidgetHint::Text: return "text";
  }
  return "auto";
}

} // namespace Teul
