#include "TSerializer.h"

namespace Teul {

// =============================================================================
//  직렬화 (Document -> JSON)
// =============================================================================

juce::var TSerializer::toJson(const TGraphDocument &doc) {
  auto *obj = new juce::DynamicObject();

  obj->setProperty("schema_version", doc.schemaVersion);
  obj->setProperty("next_node_id", (int64_t)doc.getNextNodeId());
  obj->setProperty("next_port_id", (int64_t)doc.getNextPortId());
  obj->setProperty("next_conn_id", (int64_t)doc.getNextConnectionId());

  // Meta
  auto *metaObj = new juce::DynamicObject();
  metaObj->setProperty("name", doc.meta.name);
  metaObj->setProperty("canvas_offset_x", doc.meta.canvasOffsetX);
  metaObj->setProperty("canvas_offset_y", doc.meta.canvasOffsetY);
  metaObj->setProperty("canvas_zoom", doc.meta.canvasZoom);
  metaObj->setProperty("sample_rate", doc.meta.sampleRate);
  metaObj->setProperty("block_size", doc.meta.blockSize);
  obj->setProperty("meta", metaObj);

  // Nodes
  juce::Array<juce::var> nodesArr;
  for (const auto &n : doc.nodes)
    nodesArr.add(nodeToJson(n));
  obj->setProperty("nodes", nodesArr);

  // Connections
  juce::Array<juce::var> connsArr;
  for (const auto &c : doc.connections)
    connsArr.add(connectionToJson(c));
  obj->setProperty("connections", connsArr);

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

  // Params
  auto *paramsObj = new juce::DynamicObject();
  for (auto const &[key, val] : node.params)
    paramsObj->setProperty(key, val);
  obj->setProperty("params", paramsObj);

  // Ports
  juce::Array<juce::var> portsArr;
  for (const auto &p : node.ports)
    portsArr.add(portToJson(p));
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

// =============================================================================
//  역직렬화 (JSON -> Document)
// =============================================================================

bool TSerializer::fromJson(TGraphDocument &doc, const juce::var &json) {
  if (!json.isObject())
    return false;

  doc.nodes.clear();
  doc.connections.clear();

  doc.schemaVersion = json.getProperty("schema_version", 1);
  doc.setNextNodeId((NodeId)(int64_t)json.getProperty("next_node_id", 1));
  doc.setNextPortId((PortId)(int64_t)json.getProperty("next_port_id", 1));
  doc.setNextConnectionId(
      (ConnectionId)(int64_t)json.getProperty("next_conn_id", 1));

  // Meta
  if (auto *metaObj =
          json.getProperty("meta", juce::var()).getDynamicObject()) {
    doc.meta.name = metaObj->getProperty("name").toString();
    doc.meta.canvasOffsetX = (float)metaObj->getProperty("canvas_offset_x");
    doc.meta.canvasOffsetY = (float)metaObj->getProperty("canvas_offset_y");
    doc.meta.canvasZoom = (float)metaObj->getProperty("canvas_zoom");
    doc.meta.sampleRate = (double)metaObj->getProperty("sample_rate");
    doc.meta.blockSize = (int)metaObj->getProperty("block_size");
  }

  // Nodes
  if (auto *nodesArr = json.getProperty("nodes", juce::var()).getArray()) {
    for (auto &nVar : *nodesArr) {
      TNode node;
      if (jsonToNode(node, nVar))
        doc.nodes.push_back(std::move(node));
    }
  }

  // Connections
  if (auto *connsArr =
          json.getProperty("connections", juce::var()).getArray()) {
    for (auto &cVar : *connsArr) {
      TConnection conn;
      if (jsonToConnection(conn, cVar))
        doc.connections.push_back(std::move(conn));
    }
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

  // Params
  if (auto *paramsObj =
          json.getProperty("params", juce::var()).getDynamicObject()) {
    for (auto const &[key, val] : paramsObj->getProperties())
      node.params[key.toString()] = val;
  }

  // Ports
  if (auto *portsArr = json.getProperty("ports", juce::var()).getArray()) {
    for (auto &pVar : *portsArr) {
      TPort port;
      if (jsonToPort(port, pVar)) {
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

} // namespace Teul
