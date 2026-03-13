#include "TDocumentStore.h"
#include "TDocumentSerializer.h"

namespace Teul {

bool TDocumentStore::saveToFile(const TTeulDocument &document,
                                const juce::File &file) {
  const auto json = TDocumentSerializer::toJson(document);
  const auto jsonText = juce::JSON::toString(json);
  return file.replaceWithText(jsonText, false, false, "\r\n");
}

bool TDocumentStore::loadFromFile(
    TTeulDocument &document,
    const juce::File &file,
    TSchemaMigrationReport *migrationReportOut) {
  if (!file.existsAsFile())
    return false;

  juce::var json;
  const auto parseResult = juce::JSON::parse(file.loadFileAsString(), json);
  if (parseResult.failed())
    return false;

  TSchemaMigrationReport migrationReport;
  if (!TDocumentSerializer::fromJson(document, json, &migrationReport))
    return false;

  if (migrationReport.degraded || !migrationReport.warnings.isEmpty()) {
    const auto level = migrationReport.degraded
                           ? TDocumentNoticeLevel::degraded
                           : TDocumentNoticeLevel::warning;
    const auto title = migrationReport.degraded
                           ? juce::String("Document restored in degraded mode")
                           : juce::String("Document compatibility warnings");
    document.setTransientNotice(
        level, title,
        TDocumentMigration::joinWarnings(migrationReport.warnings));
  } else {
    document.clearTransientNotice();
  }

  if (migrationReportOut != nullptr)
    *migrationReportOut = migrationReport;

  return true;
}

juce::Result TDocumentStore::importEditableGraphPackage(
    const juce::File &path,
    TTeulDocument &documentOut) {
  constexpr auto kGraphFileName = "editable-graph.teul";
  constexpr auto kManifestFileName = "export-manifest.json";

  juce::File graphFile;
  if (path.isDirectory()) {
    const auto manifestFile = path.getChildFile(kManifestFileName);
    if (manifestFile.existsAsFile()) {
      juce::var manifestJson;
      const auto parseResult =
          TDocumentMigration::parseJsonFile(manifestFile, manifestJson,
                                            "manifest");
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
      graphFile = path.getChildFile(kGraphFileName);
  } else if (path.existsAsFile()) {
    if (path.getFileName().equalsIgnoreCase(kManifestFileName)) {
      juce::var manifestJson;
      const auto parseResult =
          TDocumentMigration::parseJsonFile(path, manifestJson, "manifest");
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
  const auto parseResult =
      TDocumentMigration::parseJsonFile(graphFile, graphJson, "graph");
  if (parseResult.failed())
    return parseResult;

  TSchemaMigrationReport migrationReport;
  if (!TDocumentSerializer::fromJson(documentOut, graphJson, &migrationReport)) {
    return juce::Result::fail("Failed to deserialize editable graph package: " +
                              graphFile.getFullPathName());
  }

  if (migrationReport.degraded || !migrationReport.warnings.isEmpty()) {
    const auto level = migrationReport.degraded
                           ? TDocumentNoticeLevel::degraded
                           : TDocumentNoticeLevel::warning;
    const auto title = migrationReport.degraded
                           ? juce::String("Editable graph restored in degraded mode")
                           : juce::String("Editable graph compatibility warnings");
    documentOut.setTransientNotice(
        level, title,
        TDocumentMigration::joinWarnings(migrationReport.warnings));
  } else {
    documentOut.clearTransientNotice();
  }

  return juce::Result::ok();
}

bool TFileIo::saveToFile(const TTeulDocument &document,
                         const juce::File &file) {
  return TDocumentStore::saveToFile(document, file);
}

bool TFileIo::loadFromFile(TTeulDocument &document,
                           const juce::File &file,
                           TSchemaMigrationReport *migrationReportOut) {
  return TDocumentStore::loadFromFile(document, file, migrationReportOut);
}

namespace {
constexpr float kNodeWidth = 160.0f;
constexpr float kNodeHeight = 90.0f;

constexpr int kPatchPresetSchemaVersion = 2;

juce::var propertyOrAlias(const juce::DynamicObject *object,
                          std::initializer_list<const char *> keys,
                          const juce::var &fallback = juce::var()) {
  if (object == nullptr)
    return fallback;

  for (const auto *key : keys) {
    const juce::Identifier id(key);
    if (object->hasProperty(id))
      return object->getProperty(id);
  }

  return fallback;
}

bool writeJsonFile(const juce::File &file, const juce::var &json) {
  return file.replaceWithText(juce::JSON::toString(json, true), false, false,
                              "\r\n");
}

juce::Rectangle<float> nodeRectForPreset(const TNode &node) {
  return {node.x, node.y, kNodeWidth, kNodeHeight};
}

std::vector<NodeId> collectFrameMembers(const TTeulDocument &document,
                                        const TFrameRegion &frame) {
  std::vector<NodeId> memberNodeIds;

  if (frame.membershipExplicit && !frame.memberNodeIds.empty()) {
    for (const auto nodeId : frame.memberNodeIds) {
      if (document.findNode(nodeId) != nullptr)
        memberNodeIds.push_back(nodeId);
    }

    return memberNodeIds;
  }

  const juce::Rectangle<float> frameRect(frame.x, frame.y, frame.width,
                                         frame.height);
  for (const auto &node : document.nodes) {
    if (frameRect.intersects(nodeRectForPreset(node)))
      memberNodeIds.push_back(node.nodeId);
  }

  return memberNodeIds;
}

juce::Rectangle<float> computeDocumentBounds(const TTeulDocument &document) {
  juce::Rectangle<float> bounds;
  bool hasBounds = false;

  for (const auto &frame : document.frames) {
    const juce::Rectangle<float> frameRect(frame.x, frame.y, frame.width,
                                           frame.height);
    bounds = hasBounds ? bounds.getUnion(frameRect) : frameRect;
    hasBounds = true;
  }

  for (const auto &node : document.nodes) {
    const auto nodeRect = nodeRectForPreset(node);
    bounds = hasBounds ? bounds.getUnion(nodeRect) : nodeRect;
    hasBounds = true;
  }

  return hasBounds ? bounds : juce::Rectangle<float>();
}

void updateDocumentNextIds(TTeulDocument &document) {
  NodeId maxNodeId = 0;
  PortId maxPortId = 0;
  ConnectionId maxConnectionId = 0;
  int maxFrameId = 0;
  int maxBookmarkId = 0;

  for (const auto &node : document.nodes) {
    maxNodeId = juce::jmax(maxNodeId, node.nodeId);
    for (const auto &port : node.ports)
      maxPortId = juce::jmax(maxPortId, port.portId);
  }

  for (const auto &connection : document.connections)
    maxConnectionId = juce::jmax(maxConnectionId, connection.connectionId);

  for (const auto &frame : document.frames)
    maxFrameId = juce::jmax(maxFrameId, frame.frameId);

  for (const auto &bookmark : document.bookmarks)
    maxBookmarkId = juce::jmax(maxBookmarkId, bookmark.bookmarkId);

  document.setNextNodeId(maxNodeId + 1);
  document.setNextPortId(maxPortId + 1);
  document.setNextConnectionId(maxConnectionId + 1);
  document.setNextFrameId(maxFrameId + 1);
  document.setNextBookmarkId(maxBookmarkId + 1);
}

juce::File withPatchPresetExtension(const juce::File &file) {
  const auto extension = TPatchPresetIO::fileExtension();
  if (file.hasFileExtension(extension))
    return file;

  return file.withFileExtension(extension);
}



juce::String sanitizePatchPresetName(const juce::String &rawName) {
  juce::String text = rawName.trim();
  if (text.isEmpty())
    text = "PatchPreset";

  for (const auto character : {'<', '>', ':', '\"', '/', '\\', '|', '?', '*'})
    text = text.replaceCharacter(character, '_');

  return text;
}

juce::var summaryToJson(const TPatchPresetSummary &summary) {
  auto *object = new juce::DynamicObject();
  object->setProperty("preset_name", summary.presetName);
  object->setProperty("source_frame_uuid", summary.sourceFrameUuid);
  object->setProperty("node_count", summary.nodeCount);
  object->setProperty("connection_count", summary.connectionCount);
  object->setProperty("frame_count", summary.frameCount);
  object->setProperty("bounds_x", summary.bounds.getX());
  object->setProperty("bounds_y", summary.bounds.getY());
  object->setProperty("bounds_width", summary.bounds.getWidth());
  object->setProperty("bounds_height", summary.bounds.getHeight());
  return juce::var(object);
}

void hydrateSummaryFromJson(TPatchPresetSummary &summary,
                            const juce::var &summaryVar) {
  if (auto *object = summaryVar.getDynamicObject()) {
    const auto readInt = [&](std::initializer_list<const char *> keys) {
      const auto value = propertyOrAlias(object, keys);
      return value.isVoid() ? 0 : (int)value;
    };
    const auto readFloat = [&](std::initializer_list<const char *> keys) {
      const auto value = propertyOrAlias(object, keys);
      return value.isVoid() ? 0.0f : (float)value;
    };

    summary.presetName =
        propertyOrAlias(object, {"preset_name", "presetName"}).toString();
    summary.sourceFrameUuid =
        propertyOrAlias(object, {"source_frame_uuid", "sourceFrameUuid"})
            .toString();
    summary.nodeCount = readInt({"node_count", "nodeCount"});
    summary.connectionCount =
        readInt({"connection_count", "connectionCount"});
    summary.frameCount = readInt({"frame_count", "frameCount"});
    summary.bounds = {readFloat({"bounds_x", "boundsX"}),
                      readFloat({"bounds_y", "boundsY"}),
                      readFloat({"bounds_width", "boundsWidth"}),
                      readFloat({"bounds_height", "boundsHeight"})};
  }
}

} // namespace

juce::String TPatchPresetIO::fileExtension() { return ".teulpatch"; }

juce::File TPatchPresetIO::defaultPresetDirectory() {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulPatchPresets");
}

juce::Result TPatchPresetIO::saveFrameToFile(const TTeulDocument &document,
                                             int frameId,
                                             const juce::File &file,
                                             TPatchPresetSummary *summaryOut) {
  const auto *frame = document.findFrame(frameId);
  if (frame == nullptr)
    return juce::Result::fail("Patch preset save failed: frame not found.");

  const auto memberNodeIds = collectFrameMembers(document, *frame);
  if (memberNodeIds.empty()) {
    return juce::Result::fail(
        "Patch preset save failed: frame has no member nodes.");
  }

  const std::unordered_set<NodeId> memberSet(memberNodeIds.begin(),
                                             memberNodeIds.end());

  TTeulDocument presetDocument;
  presetDocument.schemaVersion = document.schemaVersion;
  presetDocument.meta.name = frame->title;
  presetDocument.meta.canvasOffsetX = 0.0f;
  presetDocument.meta.canvasOffsetY = 0.0f;
  presetDocument.meta.canvasZoom = 1.0f;
  presetDocument.meta.sampleRate = document.meta.sampleRate;
  presetDocument.meta.blockSize = document.meta.blockSize;

  const float originX = frame->x;
  const float originY = frame->y;

  for (const auto &node : document.nodes) {
    if (memberSet.find(node.nodeId) == memberSet.end())
      continue;

    auto presetNode = node;
    presetNode.x -= originX;
    presetNode.y -= originY;
    presetDocument.nodes.push_back(std::move(presetNode));
  }

  for (const auto &connection : document.connections) {
    if (memberSet.find(connection.from.nodeId) == memberSet.end() ||
        memberSet.find(connection.to.nodeId) == memberSet.end()) {
      continue;
    }

    presetDocument.connections.push_back(connection);
  }

  auto presetFrame = *frame;
  presetFrame.x -= originX;
  presetFrame.y -= originY;
  presetFrame.membershipExplicit = true;
  presetFrame.memberNodeIds = memberNodeIds;
  presetDocument.frames.push_back(std::move(presetFrame));

  updateDocumentNextIds(presetDocument);

  TPatchPresetSummary summary;
  summary.presetName = frame->title.isNotEmpty() ? frame->title
                                                 : sanitizePatchPresetName(file.getFileNameWithoutExtension());
  summary.sourceFrameUuid = frame->frameUuid;
  summary.nodeCount = (int)presetDocument.nodes.size();
  summary.connectionCount = (int)presetDocument.connections.size();
  summary.frameCount = (int)presetDocument.frames.size();
  summary.bounds = computeDocumentBounds(presetDocument);

  auto *root = new juce::DynamicObject();
  root->setProperty("format", "teul.patch_preset");
  root->setProperty("schema_version", kPatchPresetSchemaVersion);
  root->setProperty("preset_name", summary.presetName);
  root->setProperty("source_frame_uuid", summary.sourceFrameUuid);
  root->setProperty("saved_at", juce::Time::getCurrentTime().toISO8601(true));
  root->setProperty("summary", summaryToJson(summary));
  root->setProperty("graph", TDocumentSerializer::toJson(presetDocument));

  const auto targetFile = withPatchPresetExtension(file);
  if (!targetFile.getParentDirectory().createDirectory() &&
      !targetFile.getParentDirectory().exists()) {
    return juce::Result::fail(
        "Patch preset save failed: output directory could not be created.");
  }

  if (!writeJsonFile(targetFile, juce::var(root)))
    return juce::Result::fail("Patch preset save failed: file write failed.");

  if (summaryOut != nullptr)
    *summaryOut = summary;

  return juce::Result::ok();
}

juce::Result TPatchPresetIO::loadFromFile(
    TTeulDocument &presetDocumentOut,
    TPatchPresetSummary &summaryOut,
    const juce::File &file,
    TPatchPresetLoadReport *loadReportOut) {
  if (!file.existsAsFile())
    return juce::Result::fail("Patch preset load failed: file not found.");

  juce::var rootVar;
  const auto parseResult = juce::JSON::parse(file.loadFileAsString(), rootVar);
  if (parseResult.failed())
    return juce::Result::fail("Patch preset load failed: invalid JSON.");

  auto *sourceRoot = rootVar.getDynamicObject();
  if (sourceRoot == nullptr)
    return juce::Result::fail("Patch preset load failed: invalid preset root.");

  if (propertyOrAlias(sourceRoot, {"format"}).toString() != "teul.patch_preset") {
    return juce::Result::fail(
        "Patch preset load failed: unsupported preset format.");
  }

  TPatchPresetLoadReport loadReport;
  loadReport.sourceSchemaVersion =
      (int)propertyOrAlias(sourceRoot, {"schema_version", "schemaVersion"}, 1);
  loadReport.targetSchemaVersion = kPatchPresetSchemaVersion;
  loadReport.usedLegacyAliases = TDocumentMigration::usesLegacyPatchAliases(rootVar);

  if (loadReport.usedLegacyAliases) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "Patch preset used legacy root field aliases during load.");
  }

  if (loadReport.sourceSchemaVersion < loadReport.targetSchemaVersion) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "Patch preset schema upgraded from v" +
                      juce::String(loadReport.sourceSchemaVersion) + " to v" +
                      juce::String(loadReport.targetSchemaVersion) + ".");
  } else if (loadReport.sourceSchemaVersion > loadReport.targetSchemaVersion) {
    loadReport.degraded = true;
    TDocumentMigration::appendWarning(
        loadReport.warnings,
        "Patch preset schema is newer than this build supports; using best-effort load.");
  }

  const auto migratedRootVar = TDocumentMigration::migratePatchPresetJson(
      rootVar, loadReport.sourceSchemaVersion,
      loadReport.targetSchemaVersion, &loadReport);
  auto *root = migratedRootVar.getDynamicObject();
  if (root == nullptr) {
    return juce::Result::fail(
        "Patch preset load failed: migrated preset root is invalid.");
  }

  const auto summaryVar = propertyOrAlias(root, {"summary", "presetSummary"});
  const bool hasSummaryObject = summaryVar.isObject();

  summaryOut = {};
  summaryOut.presetName =
      propertyOrAlias(root, {"preset_name", "presetName"}).toString();
  summaryOut.sourceFrameUuid =
      propertyOrAlias(root, {"source_frame_uuid", "sourceFrameUuid"})
          .toString();
  hydrateSummaryFromJson(summaryOut, summaryVar);
  if (summaryOut.presetName.isEmpty())
    summaryOut.presetName = file.getFileNameWithoutExtension();

  presetDocumentOut = TTeulDocument();
  const auto graphVar =
      propertyOrAlias(root, {"graph", "graphPayload", "graph_json"});
  if (!TDocumentSerializer::fromJson(presetDocumentOut, graphVar,
                             &loadReport.graphMigration)) {
    return juce::Result::fail(
        "Patch preset load failed: graph payload could not be restored.");
  }

  for (const auto &warning : loadReport.graphMigration.warnings)
    TDocumentMigration::appendWarning(loadReport.warnings, "Graph: " + warning);

  bool derivedSummaryField = false;
  if (summaryOut.nodeCount <= 0) {
    summaryOut.nodeCount = (int)presetDocumentOut.nodes.size();
    derivedSummaryField = true;
  }
  if (summaryOut.connectionCount <= 0) {
    summaryOut.connectionCount = (int)presetDocumentOut.connections.size();
    derivedSummaryField = true;
  }
  if (summaryOut.frameCount <= 0) {
    summaryOut.frameCount = (int)presetDocumentOut.frames.size();
    derivedSummaryField = true;
  }
  if (summaryOut.bounds.isEmpty()) {
    summaryOut.bounds = computeDocumentBounds(presetDocumentOut);
    derivedSummaryField = true;
  }

  if (!hasSummaryObject) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "Patch preset summary missing; summary fields were derived from the graph payload.");
  } else if (derivedSummaryField) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "Patch preset summary incomplete; missing fields were derived from the graph payload.");
  }

  loadReport.migrated =
      loadReport.usedLegacyAliases ||
      loadReport.sourceSchemaVersion != loadReport.targetSchemaVersion ||
      loadReport.graphMigration.migrated || !loadReport.appliedSteps.isEmpty();
  loadReport.degraded = loadReport.degraded || loadReport.graphMigration.degraded;
  if (loadReportOut != nullptr)
    *loadReportOut = loadReport;

  return juce::Result::ok();
}

juce::Result TPatchPresetIO::insertFromFile(

    TTeulDocument &targetDocument,
    const juce::File &file,
    juce::Point<float> originWorld,
    std::vector<NodeId> *insertedNodeIdsOut,
    int *insertedFrameIdOut,
    TPatchPresetSummary *summaryOut) {
  TTeulDocument presetDocument;
  TPatchPresetSummary summary;
  TPatchPresetLoadReport loadReport;
  const auto loadResult =
      loadFromFile(presetDocument, summary, file, &loadReport);
  if (loadResult.failed())
    return loadResult;

  if (loadReport.degraded || !loadReport.warnings.isEmpty()) {
    const auto level = loadReport.degraded ? TDocumentNoticeLevel::degraded
                                           : TDocumentNoticeLevel::warning;
    const auto title =
        loadReport.degraded ? juce::String("Patch preset inserted with fallback")
                            : juce::String("Patch preset compatibility warning");
    auto detail = summary.presetName.trim();
    if (detail.isNotEmpty())
      detail << ": ";
    detail << TDocumentMigration::joinWarnings(loadReport.warnings);
    targetDocument.setTransientNotice(level, title, detail);
  }

  std::unordered_map<NodeId, NodeId> nodeIdMap;
  std::unordered_map<PortId, PortId> portIdMap;
  std::vector<NodeId> insertedNodeIds;

  for (const auto &presetNode : presetDocument.nodes) {
    auto insertedNode = presetNode;
    const auto nextNodeId = targetDocument.allocNodeId();
    nodeIdMap[presetNode.nodeId] = nextNodeId;
    insertedNode.nodeId = nextNodeId;
    insertedNode.x += originWorld.x;
    insertedNode.y += originWorld.y;

    for (auto &port : insertedNode.ports) {
      const auto previousPortId = port.portId;
      const auto nextPortId = targetDocument.allocPortId();
      portIdMap[previousPortId] = nextPortId;
      port.portId = nextPortId;
      port.ownerNodeId = nextNodeId;
    }

    insertedNodeIds.push_back(nextNodeId);
    targetDocument.nodes.push_back(std::move(insertedNode));
  }

  for (const auto &presetConnection : presetDocument.connections) {
    auto insertedConnection = presetConnection;
    insertedConnection.connectionId = targetDocument.allocConnectionId();
    insertedConnection.from.nodeId = nodeIdMap[presetConnection.from.nodeId];
    insertedConnection.to.nodeId = nodeIdMap[presetConnection.to.nodeId];
    insertedConnection.from.portId = portIdMap[presetConnection.from.portId];
    insertedConnection.to.portId = portIdMap[presetConnection.to.portId];
    targetDocument.connections.push_back(std::move(insertedConnection));
  }

  int insertedFrameId = 0;
  for (const auto &presetFrame : presetDocument.frames) {
    auto insertedFrame = presetFrame;
    insertedFrame.frameId = targetDocument.allocFrameId();
    insertedFrame.frameUuid = juce::Uuid().toString();
    insertedFrame.x += originWorld.x;
    insertedFrame.y += originWorld.y;
    insertedFrame.membershipExplicit = true;
    insertedFrame.memberNodeIds.clear();
    for (const auto presetMemberId : presetFrame.memberNodeIds) {
      const auto mappedIt = nodeIdMap.find(presetMemberId);
      if (mappedIt != nodeIdMap.end())
        insertedFrame.memberNodeIds.push_back(mappedIt->second);
    }

    if (insertedFrameId == 0)
      insertedFrameId = insertedFrame.frameId;

    targetDocument.frames.push_back(std::move(insertedFrame));
  }

  if (insertedNodeIdsOut != nullptr)
    *insertedNodeIdsOut = insertedNodeIds;
  if (insertedFrameIdOut != nullptr)
    *insertedFrameIdOut = insertedFrameId;
  if (summaryOut != nullptr) {
    *summaryOut = summary;
    summaryOut->bounds = summary.bounds.translated(originWorld.x, originWorld.y);
  }

  return juce::Result::ok();
}






namespace {

constexpr int kStatePresetSchemaVersion = 2;



juce::File withStatePresetExtension(const juce::File &file) {
  const auto extension = TStatePresetIO::fileExtension();
  if (file.hasFileExtension(extension))
    return file;

  return file.withFileExtension(extension);
}



juce::String sanitizeStatePresetName(const juce::String &rawName) {
  juce::String text = rawName.trim();
  if (text.isEmpty())
    text = "StatePreset";

  for (const auto character : {'<', '>', ':', '"', '/', '\\', '|', '?', '*'})
    text = text.replaceCharacter(character, '_');

  return text;
}

int countParamValues(const std::vector<TStatePresetNodeState> &nodeStates) {
  int count = 0;
  for (const auto &nodeState : nodeStates)
    count += (int)nodeState.params.size();
  return count;
}

juce::var nodeStateToJson(const TStatePresetNodeState &nodeState) {
  auto *object = new juce::DynamicObject();
  object->setProperty("node_id", (int64_t)nodeState.nodeId);
  object->setProperty("type_key", nodeState.typeKey);
  object->setProperty("label", nodeState.label);
  object->setProperty("bypassed", nodeState.bypassed);

  auto *paramsObject = new juce::DynamicObject();
  for (const auto &[key, value] : nodeState.params)
    paramsObject->setProperty(key, value);
  object->setProperty("params", paramsObject);
  return juce::var(object);
}

bool jsonToNodeState(TStatePresetNodeState &nodeState, const juce::var &json) {
  if (!json.isObject())
    return false;

  const auto *object = json.getDynamicObject();
  nodeState.nodeId =
      (NodeId)(int64_t)propertyOrAlias(object, {"node_id", "nodeId"}, 0);
  nodeState.typeKey =
      propertyOrAlias(object, {"type_key", "typeKey", "type"}).toString();
  nodeState.label = propertyOrAlias(object, {"label", "name"}).toString();
  nodeState.bypassed =
      (bool)propertyOrAlias(object, {"bypassed", "isBypassed"}, false);
  nodeState.params.clear();

  if (auto *paramsObject =
          propertyOrAlias(object, {"params", "paramValues", "param_values"},
                          juce::var())
              .getDynamicObject()) {
    for (const auto &[key, value] : paramsObject->getProperties())
      nodeState.params[key.toString()] = value;
  }

  return nodeState.nodeId != kInvalidNodeId && nodeState.typeKey.isNotEmpty();
}

juce::var summaryToJson(const TStatePresetSummary &summary) {
  auto *object = new juce::DynamicObject();
  object->setProperty("preset_name", summary.presetName);
  object->setProperty("target_graph_name", summary.targetGraphName);
  object->setProperty("node_state_count", summary.nodeStateCount);
  object->setProperty("param_value_count", summary.paramValueCount);
  return juce::var(object);
}

void hydrateSummaryFromJson(TStatePresetSummary &summary,
                            const juce::var &summaryVar) {
  if (auto *object = summaryVar.getDynamicObject()) {
    summary.presetName =
        propertyOrAlias(object, {"preset_name", "presetName"}).toString();
    summary.targetGraphName =
        propertyOrAlias(object, {"target_graph_name", "targetGraphName"})
            .toString();
    summary.nodeStateCount =
        object->hasProperty("node_state_count")
            ? (int)object->getProperty("node_state_count")
            : (object->hasProperty("nodeStateCount")
                   ? (int)object->getProperty("nodeStateCount")
                   : 0);
    summary.paramValueCount =
        object->hasProperty("param_value_count")
            ? (int)object->getProperty("param_value_count")
            : (object->hasProperty("paramValueCount")
                   ? (int)object->getProperty("paramValueCount")
                   : 0);
  }
}

const TNode *findFallbackNode(const TTeulDocument &document,
                              const TStatePresetNodeState &nodeState) {
  const TNode *match = nullptr;
  for (const auto &node : document.nodes) {
    if (node.typeKey != nodeState.typeKey)
      continue;
    if (nodeState.label.isNotEmpty() && node.label != nodeState.label)
      continue;

    if (match != nullptr)
      return nullptr;

    match = &node;
  }

  return match;
}

TNode *findFallbackNode(TTeulDocument &document,
                        const TStatePresetNodeState &nodeState) {
  return const_cast<TNode *>(findFallbackNode(
      const_cast<const TTeulDocument &>(document), nodeState));
}

const TNode *findTargetNode(const TTeulDocument &document,
                            const TStatePresetNodeState &nodeState) {
  if (const auto *node = document.findNode(nodeState.nodeId)) {
    if (node->typeKey == nodeState.typeKey)
      return node;
  }

  return findFallbackNode(document, nodeState);
}

TNode *findTargetNode(TTeulDocument &document,
                      const TStatePresetNodeState &nodeState) {
  return const_cast<TNode *>(findTargetNode(
      const_cast<const TTeulDocument &>(document), nodeState));
}

bool varsDiffer(const juce::var &lhs, const juce::var &rhs) {
  if (lhs.isVoid() != rhs.isVoid())
    return true;
  if (lhs.isBool() != rhs.isBool())
    return true;
  if (lhs.isInt() != rhs.isInt())
    return true;
  if (lhs.isInt64() != rhs.isInt64())
    return true;
  if (lhs.isDouble() != rhs.isDouble())
    return true;
  if (lhs.isString() != rhs.isString())
    return true;
  return juce::JSON::toString(lhs) != juce::JSON::toString(rhs);
}

void appendPreviewWarning(juce::StringArray &warnings, const juce::String &warning) {
  TDocumentMigration::appendWarning(warnings, warning);
}

} // namespace

juce::String TStatePresetIO::fileExtension() { return ".teulstate"; }

juce::File TStatePresetIO::defaultPresetDirectory() {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulStatePresets");
}

juce::Result TStatePresetIO::saveDocumentToFile(const TTeulDocument &document,
                                                const juce::File &file,
                                                TStatePresetSummary *summaryOut) {
  std::vector<TStatePresetNodeState> nodeStates;
  nodeStates.reserve(document.nodes.size());

  for (const auto &node : document.nodes) {
    TStatePresetNodeState nodeState;
    nodeState.nodeId = node.nodeId;
    nodeState.typeKey = node.typeKey;
    nodeState.label = node.label;
    nodeState.bypassed = node.bypassed;
    nodeState.params = node.params;
    nodeStates.push_back(std::move(nodeState));
  }

  TStatePresetSummary summary;
  summary.presetName = sanitizeStatePresetName(file.getFileNameWithoutExtension());
  if (summary.presetName == "StatePreset" && document.meta.name.isNotEmpty())
    summary.presetName = sanitizeStatePresetName(document.meta.name + " State");
  summary.targetGraphName = document.meta.name;
  summary.nodeStateCount = (int)nodeStates.size();
  summary.paramValueCount = countParamValues(nodeStates);

  auto *root = new juce::DynamicObject();
  root->setProperty("format", "teul.state_preset");
  root->setProperty("schema_version", kStatePresetSchemaVersion);
  root->setProperty("preset_name", summary.presetName);
  root->setProperty("target_graph_name", summary.targetGraphName);
  root->setProperty("summary", summaryToJson(summary));

  juce::Array<juce::var> nodeStatesArray;
  for (const auto &nodeState : nodeStates)
    nodeStatesArray.add(nodeStateToJson(nodeState));
  root->setProperty("node_states", nodeStatesArray);

  const auto targetFile = withStatePresetExtension(file);
  if (!targetFile.getParentDirectory().createDirectory() &&
      !targetFile.getParentDirectory().isDirectory()) {
    return juce::Result::fail(
        "State preset save failed: output directory could not be created.");
  }

  if (!writeJsonFile(targetFile, juce::var(root)))
    return juce::Result::fail("State preset save failed: file write failed.");

  if (summaryOut != nullptr)
    *summaryOut = summary;

  return juce::Result::ok();
}

juce::Result TStatePresetIO::loadFromFile(
    std::vector<TStatePresetNodeState> &nodeStatesOut,
    TStatePresetSummary &summaryOut,
    const juce::File &file,
    TStatePresetLoadReport *loadReportOut) {
  if (!file.existsAsFile())
    return juce::Result::fail("State preset load failed: file not found.");

  const auto rootVar = juce::JSON::parse(file);
  if (rootVar.isVoid())
    return juce::Result::fail("State preset load failed: invalid JSON.");

  auto *sourceRoot = rootVar.getDynamicObject();
  if (sourceRoot == nullptr)
    return juce::Result::fail("State preset load failed: invalid preset root.");

  if (propertyOrAlias(sourceRoot, {"format"}).toString() != "teul.state_preset") {
    return juce::Result::fail(
        "State preset load failed: unsupported preset format.");
  }

  TStatePresetLoadReport loadReport;
  loadReport.sourceSchemaVersion =
      (int)propertyOrAlias(sourceRoot, {"schema_version", "schemaVersion"}, 1);
  loadReport.targetSchemaVersion = kStatePresetSchemaVersion;
  loadReport.usedLegacyAliases = TDocumentMigration::usesLegacyStateAliases(rootVar);

  if (loadReport.usedLegacyAliases) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "State preset used legacy root field aliases during load.");
  }

  if (loadReport.sourceSchemaVersion < loadReport.targetSchemaVersion) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "State preset schema upgraded from v" +
                      juce::String(loadReport.sourceSchemaVersion) + " to v" +
                      juce::String(loadReport.targetSchemaVersion) + ".");
  } else if (loadReport.sourceSchemaVersion > loadReport.targetSchemaVersion) {
    loadReport.degraded = true;
    TDocumentMigration::appendWarning(
        loadReport.warnings,
        "State preset schema is newer than this build supports; using best-effort load.");
  }

  const auto migratedRootVar = TDocumentMigration::migrateStatePresetJson(
      rootVar, loadReport.sourceSchemaVersion,
      loadReport.targetSchemaVersion, &loadReport);
  auto *root = migratedRootVar.getDynamicObject();
  if (root == nullptr)
    return juce::Result::fail("State preset load failed: migrated preset root is invalid.");

  const auto summaryVar = propertyOrAlias(root, {"summary", "presetSummary"});
  const bool hasSummaryObject = summaryVar.isObject();

  nodeStatesOut.clear();
  summaryOut = {};
  summaryOut.presetName =
      propertyOrAlias(root, {"preset_name", "presetName"}).toString();
  summaryOut.targetGraphName =
      propertyOrAlias(root, {"target_graph_name", "targetGraphName"})
          .toString();
  hydrateSummaryFromJson(summaryOut, summaryVar);
  if (summaryOut.presetName.isEmpty())
    summaryOut.presetName = file.getFileNameWithoutExtension();

  if (auto *nodeStatesArray =
          propertyOrAlias(root, {"node_states", "nodeStates"}, juce::var())
              .getArray()) {
    for (const auto &nodeStateVar : *nodeStatesArray) {
      TStatePresetNodeState nodeState;
      if (jsonToNodeState(nodeState, nodeStateVar))
        nodeStatesOut.push_back(std::move(nodeState));
    }
  }

  bool derivedSummaryField = false;
  if (summaryOut.nodeStateCount <= 0) {
    summaryOut.nodeStateCount = (int)nodeStatesOut.size();
    derivedSummaryField = true;
  }
  if (summaryOut.paramValueCount <= 0) {
    summaryOut.paramValueCount = countParamValues(nodeStatesOut);
    derivedSummaryField = true;
  }

  if (!hasSummaryObject) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "State preset summary missing; summary fields were derived from node states.");
  } else if (derivedSummaryField) {
    TDocumentMigration::appendWarning(loadReport.warnings,
                  "State preset summary incomplete; missing fields were derived from node states.");
  }

  loadReport.migrated =
      loadReport.usedLegacyAliases ||
      loadReport.sourceSchemaVersion != loadReport.targetSchemaVersion ||
      !loadReport.appliedSteps.isEmpty();
  if (loadReportOut != nullptr)
    *loadReportOut = loadReport;

  return juce::Result::ok();
}

juce::Result TStatePresetIO::previewAgainstDocument(

    const TTeulDocument &document,
    const juce::File &file,
    TStatePresetDiffPreview *previewOut) {
  std::vector<TStatePresetNodeState> nodeStates;
  TStatePresetSummary summary;
  TStatePresetLoadReport loadReport;
  const auto loadResult = loadFromFile(nodeStates, summary, file, &loadReport);
  if (loadResult.failed())
    return loadResult;

  TStatePresetDiffPreview preview;
  preview.summary = summary;
  preview.loadReport = loadReport;
  preview.degraded = loadReport.degraded;
  preview.warnings = loadReport.warnings;

  for (const auto &nodeState : nodeStates) {
    const auto *node = findTargetNode(document, nodeState);
    if (node == nullptr) {
      ++preview.missingNodeCount;
      preview.degraded = true;
      appendPreviewWarning(
          preview.warnings,
          "Missing target node for state preset entry: " +
              (nodeState.label.isNotEmpty() ? nodeState.label : nodeState.typeKey));
      continue;
    }

    ++preview.matchedNodeCount;
    bool nodeChanged = false;

    if (node->bypassed != nodeState.bypassed) {
      ++preview.changedBypassCount;
      nodeChanged = true;
    }

    for (const auto &[key, value] : nodeState.params) {
      const auto currentIt = node->params.find(key);
      if (currentIt == node->params.end() || varsDiffer(currentIt->second, value)) {
        ++preview.changedParamValueCount;
        nodeChanged = true;
      }
    }

    if (nodeChanged) {
      ++preview.changedNodeCount;
      const auto label = node->label.isNotEmpty() ? node->label : node->typeKey;
      if (!preview.changedNodeLabels.contains(label))
        preview.changedNodeLabels.add(label);
    }
  }

  if (previewOut != nullptr)
    *previewOut = preview;

  return juce::Result::ok();
}

juce::Result TStatePresetIO::applyToDocument(TTeulDocument &document,
                                             const juce::File &file,
                                             TStatePresetApplyReport *reportOut) {
  std::vector<TStatePresetNodeState> nodeStates;
  TStatePresetSummary summary;
  TStatePresetLoadReport loadReport;
  const auto loadResult = loadFromFile(nodeStates, summary, file, &loadReport);
  if (loadResult.failed())
    return loadResult;

  TStatePresetApplyReport report;
  report.summary = summary;
  report.loadReport = loadReport;
  report.degraded = loadReport.degraded;
  report.warnings = loadReport.warnings;

  for (const auto &nodeState : nodeStates) {
    auto *node = findTargetNode(document, nodeState);
    if (node == nullptr) {
      ++report.skippedNodeCount;
      continue;
    }

    node->bypassed = nodeState.bypassed;
    for (const auto &[key, value] : nodeState.params) {
      node->params[key] = value;
      ++report.appliedParamValueCount;
    }

    report.appliedNodeIds.push_back(node->nodeId);
    ++report.appliedNodeCount;
  }

  if (report.skippedNodeCount > 0) {
    report.degraded = true;
    TDocumentMigration::appendWarning(report.warnings,
                  "State preset skipped " + juce::String(report.skippedNodeCount) +
                      " nodes while applying to the current document.");
  }

  if (report.appliedNodeCount <= 0) {
    if (reportOut != nullptr)
      *reportOut = report;
    return juce::Result::fail(
        "State preset apply failed: no matching nodes were found.");
  }

  if (report.degraded || !report.warnings.isEmpty()) {
    const auto level = report.degraded ? TDocumentNoticeLevel::degraded
                                       : TDocumentNoticeLevel::warning;
    const auto title =
        report.degraded ? juce::String("State preset applied with fallback")
                        : juce::String("State preset compatibility warning");
    auto detail = summary.presetName.trim();
    if (detail.isNotEmpty())
      detail << ": ";
    detail << TDocumentMigration::joinWarnings(report.warnings);
    document.setTransientNotice(level, title, detail);
  }

  document.touch(false);

  if (reportOut != nullptr)
    *reportOut = report;

  return juce::Result::ok();
}



} // namespace Teul
