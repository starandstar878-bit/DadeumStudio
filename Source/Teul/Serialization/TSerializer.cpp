#include "TSerializer.h"

namespace Teul {

juce::var TSerializer::toJson(const TGraphDocument &doc) {
  auto *obj = new juce::DynamicObject();

  obj->setProperty("schema_version", doc.schemaVersion);
  obj->setProperty("next_node_id", (int64_t)doc.getNextNodeId());
  obj->setProperty("next_port_id", (int64_t)doc.getNextPortId());
  obj->setProperty("next_conn_id", (int64_t)doc.getNextConnectionId());
  obj->setProperty("next_frame_id", doc.getNextFrameId());
  obj->setProperty("next_bookmark_id", doc.getNextBookmarkId());

  auto *metaObj = new juce::DynamicObject();
  metaObj->setProperty("name", doc.meta.name);
  metaObj->setProperty("canvas_offset_x", doc.meta.canvasOffsetX);
  metaObj->setProperty("canvas_offset_y", doc.meta.canvasOffsetY);
  metaObj->setProperty("canvas_zoom", doc.meta.canvasZoom);
  metaObj->setProperty("sample_rate", doc.meta.sampleRate);
  metaObj->setProperty("block_size", doc.meta.blockSize);
  obj->setProperty("meta", metaObj);

  juce::Array<juce::var> nodesArr;
  for (const auto &node : doc.nodes)
    nodesArr.add(nodeToJson(node));
  obj->setProperty("nodes", nodesArr);

  juce::Array<juce::var> connsArr;
  for (const auto &connection : doc.connections)
    connsArr.add(connectionToJson(connection));
  obj->setProperty("connections", connsArr);

  juce::Array<juce::var> framesArr;
  for (const auto &frame : doc.frames)
    framesArr.add(frameToJson(frame));
  obj->setProperty("frames", framesArr);

  juce::Array<juce::var> bookmarksArr;
  for (const auto &bookmark : doc.bookmarks)
    bookmarksArr.add(bookmarkToJson(bookmark));
  obj->setProperty("bookmarks", bookmarksArr);

  return juce::var(obj);
}

juce::var TSerializer::nodeToJson(const TNode &node) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("id", (int64_t)node.nodeId);
  obj->setProperty("type", node.typeKey);
  obj->setProperty("x", node.x);
  obj->setProperty("y", node.y);
  obj->setProperty("collapsed", node.collapsed);
  obj->setProperty("bypassed", node.bypassed);
  obj->setProperty("label", node.label);
  obj->setProperty("color_tag", node.colorTag);

  auto *paramsObj = new juce::DynamicObject();
  for (const auto &[key, value] : node.params)
    paramsObj->setProperty(key, value);
  obj->setProperty("params", paramsObj);

  juce::Array<juce::var> portsArr;
  for (const auto &port : node.ports)
    portsArr.add(portToJson(port));
  obj->setProperty("ports", portsArr);

  return juce::var(obj);
}

juce::var TSerializer::portToJson(const TPort &port) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("id", (int64_t)port.portId);
  obj->setProperty("direction", (int)port.direction);
  obj->setProperty("type", (int)port.dataType);
  obj->setProperty("name", port.name);
  obj->setProperty("channel_index", port.channelIndex);
  return juce::var(obj);
}

juce::var TSerializer::connectionToJson(const TConnection &conn) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("id", (int64_t)conn.connectionId);

  auto *fromObj = new juce::DynamicObject();
  fromObj->setProperty("node_id", (int64_t)conn.from.nodeId);
  fromObj->setProperty("port_id", (int64_t)conn.from.portId);
  obj->setProperty("from", fromObj);

  auto *toObj = new juce::DynamicObject();
  toObj->setProperty("node_id", (int64_t)conn.to.nodeId);
  toObj->setProperty("port_id", (int64_t)conn.to.portId);
  obj->setProperty("to", toObj);

  return juce::var(obj);
}

juce::var TSerializer::frameToJson(const TFrameRegion &frame) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("id", frame.frameId);
  obj->setProperty("uuid", frame.frameUuid);
  obj->setProperty("title", frame.title);
  obj->setProperty("x", frame.x);
  obj->setProperty("y", frame.y);
  obj->setProperty("width", frame.width);
  obj->setProperty("height", frame.height);
  obj->setProperty("color_argb", (int64_t)frame.colorArgb);
  obj->setProperty("collapsed", frame.collapsed);
  obj->setProperty("locked", frame.locked);
  obj->setProperty("logical_group", frame.logicalGroup);
  obj->setProperty("membership_explicit", frame.membershipExplicit);

  juce::Array<juce::var> membersArr;
  for (const auto memberNodeId : frame.memberNodeIds)
    membersArr.add((int64_t)memberNodeId);
  obj->setProperty("member_node_ids", membersArr);
  return juce::var(obj);
}

juce::var TSerializer::bookmarkToJson(const TBookmark &bookmark) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("id", bookmark.bookmarkId);
  obj->setProperty("name", bookmark.name);
  obj->setProperty("focus_x", bookmark.focusX);
  obj->setProperty("focus_y", bookmark.focusY);
  obj->setProperty("zoom", bookmark.zoom);
  obj->setProperty("color_tag", bookmark.colorTag);
  return juce::var(obj);
}

bool TSerializer::fromJson(TGraphDocument &doc, const juce::var &json) {
  if (!json.isObject())
    return false;

  doc.nodes.clear();
  doc.connections.clear();
  doc.frames.clear();
  doc.bookmarks.clear();

  doc.schemaVersion = json.getProperty("schema_version", 1);
  doc.setNextNodeId((NodeId)(int64_t)json.getProperty("next_node_id", 1));
  doc.setNextPortId((PortId)(int64_t)json.getProperty("next_port_id", 1));
  doc.setNextConnectionId(
      (ConnectionId)(int64_t)json.getProperty("next_conn_id", 1));
  doc.setNextFrameId((int)json.getProperty("next_frame_id", 1));
  doc.setNextBookmarkId((int)json.getProperty("next_bookmark_id", 1));

  if (auto *metaObj =
          json.getProperty("meta", juce::var()).getDynamicObject()) {
    doc.meta.name = metaObj->getProperty("name").toString();
    doc.meta.canvasOffsetX = (float)metaObj->getProperty("canvas_offset_x");
    doc.meta.canvasOffsetY = (float)metaObj->getProperty("canvas_offset_y");
    doc.meta.canvasZoom = (float)metaObj->getProperty("canvas_zoom");
    doc.meta.sampleRate = (double)metaObj->getProperty("sample_rate");
    doc.meta.blockSize = (int)metaObj->getProperty("block_size");
  }

  if (auto *nodesArr = json.getProperty("nodes", juce::var()).getArray()) {
    for (auto &nodeVar : *nodesArr) {
      TNode node;
      if (jsonToNode(node, nodeVar))
        doc.nodes.push_back(std::move(node));
    }
  }

  if (auto *connsArr =
          json.getProperty("connections", juce::var()).getArray()) {
    for (auto &connVar : *connsArr) {
      TConnection connection;
      if (jsonToConnection(connection, connVar))
        doc.connections.push_back(std::move(connection));
    }
  }

  if (auto *framesArr = json.getProperty("frames", juce::var()).getArray()) {
    for (auto &frameVar : *framesArr) {
      TFrameRegion frame;
      if (jsonToFrame(frame, frameVar))
        doc.frames.push_back(std::move(frame));
    }
  }

  if (auto *bookmarksArr =
          json.getProperty("bookmarks", juce::var()).getArray()) {
    for (auto &bookmarkVar : *bookmarksArr) {
      TBookmark bookmark;
      if (jsonToBookmark(bookmark, bookmarkVar))
        doc.bookmarks.push_back(std::move(bookmark));
    }
  }

  if (doc.getNextFrameId() <= 0) {
    int maxFrameId = 0;
    for (const auto &frame : doc.frames)
      maxFrameId = juce::jmax(maxFrameId, frame.frameId);
    doc.setNextFrameId(maxFrameId + 1);
  }

  if (doc.getNextBookmarkId() <= 0) {
    int maxBookmarkId = 0;
    for (const auto &bookmark : doc.bookmarks)
      maxBookmarkId = juce::jmax(maxBookmarkId, bookmark.bookmarkId);
    doc.setNextBookmarkId(maxBookmarkId + 1);
  }

  return true;
}

bool TSerializer::jsonToNode(TNode &node, const juce::var &json) {
  if (!json.isObject())
    return false;

  node.nodeId = (NodeId)(int64_t)json.getProperty("id", 0);
  node.typeKey = json.getProperty("type", "").toString();
  node.x = (float)json.getProperty("x", 0.0f);
  node.y = (float)json.getProperty("y", 0.0f);
  node.collapsed = json.getProperty("collapsed", false);
  node.bypassed = json.getProperty("bypassed", false);
  node.label = json.getProperty("label", "").toString();
  node.colorTag = json.getProperty("color_tag", "").toString();

  if (auto *paramsObj =
          json.getProperty("params", juce::var()).getDynamicObject()) {
    for (const auto &[key, value] : paramsObj->getProperties())
      node.params[key.toString()] = value;
  }

  if (auto *portsArr = json.getProperty("ports", juce::var()).getArray()) {
    for (auto &portVar : *portsArr) {
      TPort port;
      if (jsonToPort(port, portVar)) {
        port.ownerNodeId = node.nodeId;
        node.ports.push_back(std::move(port));
      }
    }
  }

  return true;
}

bool TSerializer::jsonToPort(TPort &port, const juce::var &json) {
  if (!json.isObject())
    return false;

  port.portId = (PortId)(int64_t)json.getProperty("id", 0);
  port.direction = (TPortDirection)(int)json.getProperty("direction", 0);
  port.dataType = (TPortDataType)(int)json.getProperty("type", 0);
  port.name = json.getProperty("name", "").toString();
  port.channelIndex = json.getProperty("channel_index", 0);

  return true;
}

bool TSerializer::jsonToConnection(TConnection &conn, const juce::var &json) {
  if (!json.isObject())
    return false;

  conn.connectionId = (ConnectionId)(int64_t)json.getProperty("id", 0);

  if (auto *fromObj =
          json.getProperty("from", juce::var()).getDynamicObject()) {
    conn.from.nodeId = (NodeId)(int64_t)fromObj->getProperty("node_id");
    conn.from.portId = (PortId)(int64_t)fromObj->getProperty("port_id");
  }

  if (auto *toObj = json.getProperty("to", juce::var()).getDynamicObject()) {
    conn.to.nodeId = (NodeId)(int64_t)toObj->getProperty("node_id");
    conn.to.portId = (PortId)(int64_t)toObj->getProperty("port_id");
  }

  return conn.isValid();
}

bool TSerializer::jsonToFrame(TFrameRegion &frame, const juce::var &json) {
  if (!json.isObject())
    return false;

  frame.frameId = (int)json.getProperty("id", 0);
  frame.frameUuid = json.getProperty("uuid", "").toString();
  if (frame.frameUuid.isEmpty())
    frame.frameUuid = juce::Uuid().toString();
  frame.title = json.getProperty("title", "Frame").toString();
  frame.x = (float)json.getProperty("x", 0.0f);
  frame.y = (float)json.getProperty("y", 0.0f);
  frame.width = (float)json.getProperty("width", 360.0f);
  frame.height = (float)json.getProperty("height", 220.0f);
  frame.colorArgb = (juce::uint32)(int64_t)json.getProperty("color_argb", 0x334d8bf7);
  frame.collapsed = json.getProperty("collapsed", false);
  frame.locked = json.getProperty("locked", false);
  frame.logicalGroup = json.getProperty("logical_group", true);
  frame.membershipExplicit = json.getProperty("membership_explicit", false);
  frame.memberNodeIds.clear();

  if (auto *membersArr = json.getProperty("member_node_ids", juce::var()).getArray()) {
    for (const auto &memberVar : *membersArr)
      frame.memberNodeIds.push_back((NodeId)(int64_t)memberVar);
    if (!membersArr->isEmpty())
      frame.membershipExplicit = true;
  }

  return frame.frameId > 0;
}

bool TSerializer::jsonToBookmark(TBookmark &bookmark, const juce::var &json) {
  if (!json.isObject())
    return false;

  bookmark.bookmarkId = (int)json.getProperty("id", 0);
  bookmark.name = json.getProperty("name", "Bookmark").toString();
  bookmark.focusX = (float)json.getProperty("focus_x", 0.0f);
  bookmark.focusY = (float)json.getProperty("focus_y", 0.0f);
  bookmark.zoom = (float)json.getProperty("zoom", 1.0f);
  bookmark.colorTag = json.getProperty("color_tag", "").toString();
  return bookmark.bookmarkId > 0;
}

} // namespace Teul