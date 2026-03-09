#include "TPatchPresetIO.h"

#include "TSerializer.h"

#include <unordered_map>
#include <unordered_set>

namespace Teul {
namespace {

constexpr float kNodeWidth = 160.0f;
constexpr float kNodeHeight = 90.0f;

juce::Rectangle<float> nodeRectForPreset(const TNode &node) {
  return {node.x, node.y, kNodeWidth, kNodeHeight};
}

std::vector<NodeId> collectFrameMembers(const TGraphDocument &document,
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

juce::Rectangle<float> computeDocumentBounds(const TGraphDocument &document) {
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

void updateDocumentNextIds(TGraphDocument &document) {
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

bool writeJsonFile(const juce::File &file, const juce::var &json) {
  return file.replaceWithText(juce::JSON::toString(json, true), false, false,
                              "\r\n");
}

juce::String sanitizePresetName(const juce::String &rawName) {
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
    const auto readInt = [&](const juce::Identifier &key) {
      const auto value = object->getProperty(key);
      return value.isVoid() ? 0 : (int)value;
    };
    const auto readFloat = [&](const juce::Identifier &key) {
      const auto value = object->getProperty(key);
      return value.isVoid() ? 0.0f : (float)value;
    };

    summary.presetName = object->getProperty("preset_name").toString();
    summary.sourceFrameUuid = object->getProperty("source_frame_uuid").toString();
    summary.nodeCount = readInt("node_count");
    summary.connectionCount = readInt("connection_count");
    summary.frameCount = readInt("frame_count");
    summary.bounds = {readFloat("bounds_x"), readFloat("bounds_y"),
                      readFloat("bounds_width"), readFloat("bounds_height")};
  }
}

} // namespace

juce::String TPatchPresetIO::fileExtension() { return ".teulpatch"; }

juce::File TPatchPresetIO::defaultPresetDirectory() {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulPatchPresets");
}

juce::Result TPatchPresetIO::saveFrameToFile(const TGraphDocument &document,
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

  TGraphDocument presetDocument;
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
                                                 : sanitizePresetName(file.getFileNameWithoutExtension());
  summary.sourceFrameUuid = frame->frameUuid;
  summary.nodeCount = (int)presetDocument.nodes.size();
  summary.connectionCount = (int)presetDocument.connections.size();
  summary.frameCount = (int)presetDocument.frames.size();
  summary.bounds = computeDocumentBounds(presetDocument);

  auto *root = new juce::DynamicObject();
  root->setProperty("format", "teul.patch_preset");
  root->setProperty("schema_version", 1);
  root->setProperty("preset_name", summary.presetName);
  root->setProperty("source_frame_uuid", summary.sourceFrameUuid);
  root->setProperty("saved_at", juce::Time::getCurrentTime().toISO8601(true));
  root->setProperty("summary", summaryToJson(summary));
  root->setProperty("graph", TSerializer::toJson(presetDocument));

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

juce::Result TPatchPresetIO::loadFromFile(TGraphDocument &presetDocumentOut,
                                          TPatchPresetSummary &summaryOut,
                                          const juce::File &file) {
  if (!file.existsAsFile())
    return juce::Result::fail("Patch preset load failed: file not found.");

  juce::var rootVar;
  const auto parseResult = juce::JSON::parse(file.loadFileAsString(), rootVar);
  if (parseResult.failed())
    return juce::Result::fail("Patch preset load failed: invalid JSON.");

  auto *root = rootVar.getDynamicObject();
  if (root == nullptr)
    return juce::Result::fail("Patch preset load failed: invalid preset root.");

  if (root->getProperty("format").toString() != "teul.patch_preset") {
    return juce::Result::fail(
        "Patch preset load failed: unsupported preset format.");
  }

  summaryOut = {};
  summaryOut.presetName = root->getProperty("preset_name").toString();
  summaryOut.sourceFrameUuid = root->getProperty("source_frame_uuid").toString();
  hydrateSummaryFromJson(summaryOut, root->getProperty("summary"));
  if (summaryOut.presetName.isEmpty())
    summaryOut.presetName = file.getFileNameWithoutExtension();

  presetDocumentOut = TGraphDocument();
  const auto graphVar = root->getProperty("graph");
  if (!TSerializer::fromJson(presetDocumentOut, graphVar)) {
    return juce::Result::fail(
        "Patch preset load failed: graph payload could not be restored.");
  }

  if (summaryOut.nodeCount <= 0)
    summaryOut.nodeCount = (int)presetDocumentOut.nodes.size();
  if (summaryOut.connectionCount < 0)
    summaryOut.connectionCount = (int)presetDocumentOut.connections.size();
  if (summaryOut.frameCount <= 0)
    summaryOut.frameCount = (int)presetDocumentOut.frames.size();
  if (summaryOut.bounds.isEmpty())
    summaryOut.bounds = computeDocumentBounds(presetDocumentOut);

  return juce::Result::ok();
}

juce::Result TPatchPresetIO::insertFromFile(
    TGraphDocument &targetDocument,
    const juce::File &file,
    juce::Point<float> originWorld,
    std::vector<NodeId> *insertedNodeIdsOut,
    int *insertedFrameIdOut,
    TPatchPresetSummary *summaryOut) {
  TGraphDocument presetDocument;
  TPatchPresetSummary summary;
  const auto loadResult = loadFromFile(presetDocument, summary, file);
  if (loadResult.failed())
    return loadResult;

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

} // namespace Teul
