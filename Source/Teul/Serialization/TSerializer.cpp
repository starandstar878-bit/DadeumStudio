#include "TSerializer.h"

#include <functional>

namespace Teul {
namespace {

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

bool hasProperty(const juce::DynamicObject *object, const char *key) {
  return object != nullptr && object->hasProperty(juce::Identifier(key));
}

bool hasAnyProperty(const juce::DynamicObject *object,
                    std::initializer_list<const char *> keys) {
  if (object == nullptr)
    return false;

  for (const auto *key : keys) {
    if (hasProperty(object, key))
      return true;
  }

  return false;
}

void appendWarning(juce::StringArray &warnings, const juce::String &warning) {
  const auto normalized = warning.trim();
  if (normalized.isEmpty())
    return;
  if (!warnings.contains(normalized))
    warnings.add(normalized);
}

void appendMigrationStep(TSchemaMigrationReport *report,
                         const juce::String &stepName) {
  if (report == nullptr)
    return;

  const auto normalized = stepName.trim();
  if (normalized.isEmpty())
    return;
  if (!report->appliedSteps.contains(normalized))
    report->appliedSteps.add(normalized);
}

juce::Array<juce::var> migrateArray(
    const juce::var &source,
    const std::function<juce::var(const juce::var &)> &migrateItem) {
  juce::Array<juce::var> migrated;
  if (auto *array = source.getArray()) {
    for (const auto &item : *array)
      migrated.add(migrateItem(item));
  }
  return migrated;
}

} // namespace

int TSerializer::currentSchemaVersion() noexcept { return 3; }

bool TSerializer::usesLegacyDocumentAliases(const juce::var &json) {
  const auto *root = json.getDynamicObject();
  if (root == nullptr)
    return false;

  if (hasAnyProperty(root, {"schemaVersion", "nextNodeId", "nextPortId",
                            "nextConnectionId", "nextFrameId",
                            "nextBookmarkId", "graphMeta", "node_list",
                            "connection_list", "frame_regions",
                            "bookmark_list"})) {
    return true;
  }

  if (const auto *meta =
          propertyOrAlias(root, {"meta", "graphMeta"}).getDynamicObject()) {
    if (hasAnyProperty(meta, {"canvasOffsetX", "canvasOffsetY", "canvasZoom",
                              "sampleRate", "blockSize"})) {
      return true;
    }
  }

  if (const auto *nodes =
          propertyOrAlias(root, {"nodes", "node_list"}).getArray()) {
    for (const auto &nodeVar : *nodes) {
      const auto *node = nodeVar.getDynamicObject();
      if (hasAnyProperty(node, {"nodeId", "typeKey", "posX", "posY",
                                "isCollapsed", "isBypassed", "colorTag",
                                "paramValues", "param_values", "port_list"})) {
        return true;
      }

      if (const auto *ports =
              propertyOrAlias(node, {"ports", "port_list"}).getArray()) {
        for (const auto &portVar : *ports) {
          const auto *port = portVar.getDynamicObject();
          if (hasAnyProperty(port, {"portId", "portDirection", "dataType",
                                    "portName", "channelIndex"})) {
            return true;
          }
        }
      }
    }
  }

  if (const auto *connections =
          propertyOrAlias(root, {"connections", "connection_list"}).getArray()) {
    for (const auto &connectionVar : *connections) {
      const auto *connection = connectionVar.getDynamicObject();
      if (hasAnyProperty(connection, {"connectionId", "source", "target"}))
        return true;
    }
  }

  if (const auto *frames =
          propertyOrAlias(root, {"frames", "frame_regions"}).getArray()) {
    for (const auto &frameVar : *frames) {
      const auto *frame = frameVar.getDynamicObject();
      if (hasAnyProperty(frame, {"frameId", "frameUuid", "posX", "posY",
                                 "colorArgb", "isCollapsed", "isLocked",
                                 "logicalGroup", "membershipExplicit",
                                 "memberNodeIds"})) {
        return true;
      }
    }
  }

  if (const auto *bookmarks =
          propertyOrAlias(root, {"bookmarks", "bookmark_list"}).getArray()) {
    for (const auto &bookmarkVar : *bookmarks) {
      const auto *bookmark = bookmarkVar.getDynamicObject();
      if (hasAnyProperty(bookmark, {"bookmarkId", "focusX", "focusY",
                                    "colorTag"})) {
        return true;
      }
    }
  }

  return false;
}

juce::var TSerializer::migrateDocumentJson(
    const juce::var &json,
    int sourceSchemaVersion,
    TSchemaMigrationReport *migrationReportOut) {
  if (sourceSchemaVersion <= 1) {
    auto migrated = migrateDocumentV1ToV2(json);
    appendMigrationStep(migrationReportOut, "document:v1->v2");
    migrated = migrateDocumentV2ToV3(migrated);
    appendMigrationStep(migrationReportOut, "document:v2->v3");
    return migrated;
  }

  if (sourceSchemaVersion == 2) {
    appendMigrationStep(migrationReportOut, "document:v2->v3");
    return migrateDocumentV2ToV3(json);
  }

  return normalizeDocumentJson(json);
}

juce::var TSerializer::normalizeDocumentJson(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty(
      "schema_version",
      propertyOrAlias(source, {"schema_version", "schemaVersion"},
                      currentSchemaVersion()));
  object->setProperty("next_node_id",
                      propertyOrAlias(source, {"next_node_id", "nextNodeId"},
                                      1));
  object->setProperty("next_port_id",
                      propertyOrAlias(source, {"next_port_id", "nextPortId"},
                                      1));
  object->setProperty(
      "next_conn_id",
      propertyOrAlias(source, {"next_conn_id", "next_connection_id",
                               "nextConnectionId"},
                      1));
  object->setProperty("next_frame_id",
                      propertyOrAlias(source, {"next_frame_id", "nextFrameId"},
                                      1));
  object->setProperty(
      "next_bookmark_id",
      propertyOrAlias(source, {"next_bookmark_id", "nextBookmarkId"}, 1));
  object->setProperty(
      "meta", migrateMetaJson(propertyOrAlias(source, {"meta", "graphMeta"})));
  object->setProperty(
      "nodes",
      migrateArray(propertyOrAlias(source, {"nodes", "node_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateNodeJson(item);
                   }));
  object->setProperty(
      "connections",
      migrateArray(propertyOrAlias(source, {"connections", "connection_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateConnectionJson(item);
                   }));
  object->setProperty(
      "frames",
      migrateArray(propertyOrAlias(source, {"frames", "frame_regions"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateFrameJson(item);
                   }));
  object->setProperty(
      "bookmarks",
      migrateArray(propertyOrAlias(source, {"bookmarks", "bookmark_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateBookmarkJson(item);
                   }));
  return juce::var(object);
}

juce::var TSerializer::migrateDocumentV1ToV2(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("schema_version", 2);
  object->setProperty("next_node_id",
                      propertyOrAlias(source, {"next_node_id", "nextNodeId"},
                                      1));
  object->setProperty("next_port_id",
                      propertyOrAlias(source, {"next_port_id", "nextPortId"},
                                      1));
  object->setProperty(
      "next_connection_id",
      propertyOrAlias(source, {"next_connection_id", "next_conn_id",
                               "nextConnectionId"},
                      1));
  object->setProperty("next_frame_id",
                      propertyOrAlias(source, {"next_frame_id", "nextFrameId"},
                                      1));
  object->setProperty(
      "next_bookmark_id",
      propertyOrAlias(source, {"next_bookmark_id", "nextBookmarkId"}, 1));
  object->setProperty(
      "meta", migrateMetaJson(propertyOrAlias(source, {"meta", "graphMeta"})));
  object->setProperty(
      "nodes",
      migrateArray(propertyOrAlias(source, {"nodes", "node_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateNodeJson(item);
                   }));
  object->setProperty(
      "connections",
      migrateArray(propertyOrAlias(source, {"connections", "connection_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateConnectionJson(item);
                   }));
  object->setProperty(
      "frames",
      migrateArray(propertyOrAlias(source, {"frames", "frame_regions"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateFrameJsonV2(item);
                   }));
  object->setProperty(
      "bookmarks",
      migrateArray(propertyOrAlias(source, {"bookmarks", "bookmark_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateBookmarkJson(item);
                   }));
  return juce::var(object);
}

juce::var TSerializer::migrateDocumentV2ToV3(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("schema_version", currentSchemaVersion());
  object->setProperty("next_node_id",
                      propertyOrAlias(source, {"next_node_id", "nextNodeId"},
                                      1));
  object->setProperty("next_port_id",
                      propertyOrAlias(source, {"next_port_id", "nextPortId"},
                                      1));
  object->setProperty(
      "next_conn_id",
      propertyOrAlias(source, {"next_conn_id", "next_connection_id",
                               "nextConnectionId"},
                      1));
  object->setProperty("next_frame_id",
                      propertyOrAlias(source, {"next_frame_id", "nextFrameId"},
                                      1));
  object->setProperty(
      "next_bookmark_id",
      propertyOrAlias(source, {"next_bookmark_id", "nextBookmarkId"}, 1));
  object->setProperty(
      "meta", migrateMetaJson(propertyOrAlias(source, {"meta", "graphMeta"})));
  object->setProperty(
      "nodes",
      migrateArray(propertyOrAlias(source, {"nodes", "node_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateNodeJson(item);
                   }));
  object->setProperty(
      "connections",
      migrateArray(propertyOrAlias(source, {"connections", "connection_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateConnectionJson(item);
                   }));
  object->setProperty(
      "frames",
      migrateArray(propertyOrAlias(source, {"frames", "frame_regions"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateFrameJson(item);
                   }));
  object->setProperty(
      "bookmarks",
      migrateArray(propertyOrAlias(source, {"bookmarks", "bookmark_list"}),
                   [](const juce::var &item) {
                     return TSerializer::migrateBookmarkJson(item);
                   }));
  return juce::var(object);
}

juce::var TSerializer::migrateMetaJson(const juce::var &json) {
  if (!json.isObject())
    return juce::var(new juce::DynamicObject());

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("name", propertyOrAlias(source, {"name"}, "Untitled"));
  object->setProperty(
      "canvas_offset_x",
      propertyOrAlias(source, {"canvas_offset_x", "canvasOffsetX"}, 0.0f));
  object->setProperty(
      "canvas_offset_y",
      propertyOrAlias(source, {"canvas_offset_y", "canvasOffsetY"}, 0.0f));
  object->setProperty(
      "canvas_zoom",
      propertyOrAlias(source, {"canvas_zoom", "canvasZoom"}, 1.0f));
  object->setProperty(
      "sample_rate",
      propertyOrAlias(source, {"sample_rate", "sampleRate"}, 48000.0));
  object->setProperty(
      "block_size",
      propertyOrAlias(source, {"block_size", "blockSize"}, 256));
  return juce::var(object);
}

juce::var TSerializer::migrateNodeJson(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("id", propertyOrAlias(source, {"id", "node_id", "nodeId"}, 0));
  object->setProperty("type",
                      propertyOrAlias(source, {"type", "type_key", "typeKey"}, ""));
  object->setProperty("x", propertyOrAlias(source, {"x", "pos_x", "posX"}, 0.0f));
  object->setProperty("y", propertyOrAlias(source, {"y", "pos_y", "posY"}, 0.0f));
  object->setProperty("collapsed",
                      propertyOrAlias(source, {"collapsed", "isCollapsed"}, false));
  object->setProperty("bypassed",
                      propertyOrAlias(source, {"bypassed", "isBypassed"}, false));
  object->setProperty("label", propertyOrAlias(source, {"label", "name"}, ""));
  object->setProperty(
      "color_tag",
      propertyOrAlias(source, {"color_tag", "colorTag"}, juce::String()));

  const auto params = propertyOrAlias(source, {"params", "param_values", "paramValues"});
  if (auto *paramsObject = params.getDynamicObject()) {
    auto *normalizedParams = new juce::DynamicObject();
    for (const auto &[key, value] : paramsObject->getProperties())
      normalizedParams->setProperty(key, value);
    object->setProperty("params", normalizedParams);
  } else {
    object->setProperty("params", juce::var(new juce::DynamicObject()));
  }

  object->setProperty(
      "ports",
      migrateArray(propertyOrAlias(source, {"ports", "port_list"}),
                   [](const juce::var &item) { return TSerializer::migratePortJson(item); }));
  return juce::var(object);
}

juce::var TSerializer::migratePortJson(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("id", propertyOrAlias(source, {"id", "port_id", "portId"}, 0));
  object->setProperty(
      "direction",
      propertyOrAlias(source, {"direction", "port_direction", "portDirection"}, 0));
  object->setProperty("type",
                      propertyOrAlias(source, {"type", "data_type", "dataType"}, 0));
  object->setProperty("name", propertyOrAlias(source, {"name", "port_name", "portName"}, ""));
  object->setProperty(
      "channel_index",
      propertyOrAlias(source, {"channel_index", "channelIndex"}, 0));
  return juce::var(object);
}

juce::var TSerializer::migrateConnectionJson(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty(
      "id", propertyOrAlias(source, {"id", "connection_id", "connectionId"}, 0));

  auto migrateEndpoint = [](const juce::var &endpointVar) {
    auto *endpoint = new juce::DynamicObject();
    if (auto *sourceEndpoint = endpointVar.getDynamicObject()) {
      endpoint->setProperty(
          "node_id",
          propertyOrAlias(sourceEndpoint, {"node_id", "nodeId"}, 0));
      endpoint->setProperty(
          "port_id",
          propertyOrAlias(sourceEndpoint, {"port_id", "portId"}, 0));
    }
    return juce::var(endpoint);
  };

  object->setProperty(
      "from",
      migrateEndpoint(propertyOrAlias(source, {"from", "source"}, juce::var())));
  object->setProperty(
      "to",
      migrateEndpoint(propertyOrAlias(source, {"to", "target"}, juce::var())));
  return juce::var(object);
}

juce::var TSerializer::migrateFrameJsonV2(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("id", propertyOrAlias(source, {"id", "frame_id", "frameId"}, 0));
  object->setProperty("uuid", propertyOrAlias(source, {"uuid", "frame_uuid", "frameUuid"}, juce::String()));
  object->setProperty("title", propertyOrAlias(source, {"title", "name"}, "Frame"));
  object->setProperty("x", propertyOrAlias(source, {"x", "pos_x", "posX"}, 0.0f));
  object->setProperty("y", propertyOrAlias(source, {"y", "pos_y", "posY"}, 0.0f));
  object->setProperty("width", propertyOrAlias(source, {"width"}, 360.0f));
  object->setProperty("height", propertyOrAlias(source, {"height"}, 220.0f));
  object->setProperty(
      "color_argb",
      propertyOrAlias(source, {"color_argb", "colorArgb"}, (int64_t)0x334d8bf7));
  object->setProperty("collapsed",
                      propertyOrAlias(source, {"collapsed", "isCollapsed"}, false));
  object->setProperty("locked",
                      propertyOrAlias(source, {"locked", "isLocked"}, false));
  return juce::var(object);
}

juce::var TSerializer::migrateFrameJson(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("id", propertyOrAlias(source, {"id", "frame_id", "frameId"}, 0));
  object->setProperty("uuid", propertyOrAlias(source, {"uuid", "frame_uuid", "frameUuid"}, juce::String()));
  object->setProperty("title", propertyOrAlias(source, {"title", "name"}, "Frame"));
  object->setProperty("x", propertyOrAlias(source, {"x", "pos_x", "posX"}, 0.0f));
  object->setProperty("y", propertyOrAlias(source, {"y", "pos_y", "posY"}, 0.0f));
  object->setProperty("width", propertyOrAlias(source, {"width"}, 360.0f));
  object->setProperty("height", propertyOrAlias(source, {"height"}, 220.0f));
  object->setProperty(
      "color_argb",
      propertyOrAlias(source, {"color_argb", "colorArgb"}, (int64_t)0x334d8bf7));
  object->setProperty("collapsed",
                      propertyOrAlias(source, {"collapsed", "isCollapsed"}, false));
  object->setProperty("locked",
                      propertyOrAlias(source, {"locked", "isLocked"}, false));
  object->setProperty(
      "logical_group",
      propertyOrAlias(source, {"logical_group", "logicalGroup"}, true));
  object->setProperty(
      "membership_explicit",
      propertyOrAlias(source, {"membership_explicit", "membershipExplicit"}, false));

  juce::Array<juce::var> members;
  if (auto *membersArray =
          propertyOrAlias(source, {"member_node_ids", "memberNodeIds"}).getArray()) {
    for (const auto &member : *membersArray)
      members.add(member);
  }
  object->setProperty("member_node_ids", members);
  return juce::var(object);
}

juce::var TSerializer::migrateBookmarkJson(const juce::var &json) {
  if (!json.isObject())
    return {};

  const auto *source = json.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("id",
                      propertyOrAlias(source, {"id", "bookmark_id", "bookmarkId"}, 0));
  object->setProperty("name", propertyOrAlias(source, {"name"}, "Bookmark"));
  object->setProperty(
      "focus_x",
      propertyOrAlias(source, {"focus_x", "focusX"}, 0.0f));
  object->setProperty(
      "focus_y",
      propertyOrAlias(source, {"focus_y", "focusY"}, 0.0f));
  object->setProperty("zoom", propertyOrAlias(source, {"zoom"}, 1.0f));
  object->setProperty(
      "color_tag",
      propertyOrAlias(source, {"color_tag", "colorTag"}, juce::String()));
  return juce::var(object);
}

juce::var TSerializer::toJson(const TGraphDocument &doc) {
  auto *obj = new juce::DynamicObject();

  obj->setProperty("schema_version", currentSchemaVersion());
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

bool TSerializer::fromJson(TGraphDocument &doc,
                           const juce::var &json,
                           TSchemaMigrationReport *migrationReportOut) {
  const auto *sourceRoot = json.getDynamicObject();

  TSchemaMigrationReport migrationReport;
  migrationReport.sourceSchemaVersion =
      (int)propertyOrAlias(sourceRoot, {"schema_version", "schemaVersion"}, 1);
  migrationReport.targetSchemaVersion = currentSchemaVersion();
  migrationReport.usedLegacyAliases = usesLegacyDocumentAliases(json);
  migrationReport.migrated =
      migrationReport.usedLegacyAliases ||
      migrationReport.sourceSchemaVersion != migrationReport.targetSchemaVersion;

  if (migrationReport.usedLegacyAliases) {
    appendWarning(migrationReport.warnings,
                  "Document used legacy field aliases during restore.");
  }

  if (migrationReport.sourceSchemaVersion < migrationReport.targetSchemaVersion) {
    appendWarning(migrationReport.warnings,
                  "Document schema upgraded from v" +
                      juce::String(migrationReport.sourceSchemaVersion) + " to v" +
                      juce::String(migrationReport.targetSchemaVersion) + ".");
  } else if (migrationReport.sourceSchemaVersion >
             migrationReport.targetSchemaVersion) {
    migrationReport.degraded = true;
    appendWarning(
        migrationReport.warnings,
        "Document schema is newer than this build supports; using best-effort restore.");
  }

  if (sourceRoot == nullptr)
    return false;

  if (!propertyOrAlias(sourceRoot, {"meta", "graphMeta"}).isObject()) {
    migrationReport.degraded = true;
    appendWarning(migrationReport.warnings,
                  "Document meta missing; defaults were applied.");
  }

  const auto migratedJson =
      migrateDocumentJson(json, migrationReport.sourceSchemaVersion,
                          &migrationReport);
  if (!migratedJson.isObject())
    return false;

  migrationReport.migrated =
      migrationReport.migrated || !migrationReport.appliedSteps.isEmpty();

  doc.nodes.clear();
  doc.connections.clear();
  doc.frames.clear();
  doc.bookmarks.clear();

  doc.schemaVersion = currentSchemaVersion();
  doc.setNextNodeId(
      (NodeId)(int64_t)migratedJson.getProperty("next_node_id", 1));
  doc.setNextPortId(
      (PortId)(int64_t)migratedJson.getProperty("next_port_id", 1));
  doc.setNextConnectionId(
      (ConnectionId)(int64_t)migratedJson.getProperty("next_conn_id", 1));
  doc.setNextFrameId((int)migratedJson.getProperty("next_frame_id", 1));
  doc.setNextBookmarkId((int)migratedJson.getProperty("next_bookmark_id", 1));

  if (auto *metaObj =
          migratedJson.getProperty("meta", juce::var()).getDynamicObject()) {
    doc.meta.name = metaObj->getProperty("name").toString();
    doc.meta.canvasOffsetX = (float)metaObj->getProperty("canvas_offset_x");
    doc.meta.canvasOffsetY = (float)metaObj->getProperty("canvas_offset_y");
    doc.meta.canvasZoom = (float)metaObj->getProperty("canvas_zoom");
    doc.meta.sampleRate = (double)metaObj->getProperty("sample_rate");
    doc.meta.blockSize = (int)metaObj->getProperty("block_size");
  }

  if (auto *nodesArr = migratedJson.getProperty("nodes", juce::var()).getArray()) {
    for (auto &nodeVar : *nodesArr) {
      TNode node;
      if (jsonToNode(node, nodeVar))
        doc.nodes.push_back(std::move(node));
    }
  }

  if (auto *connsArr =
          migratedJson.getProperty("connections", juce::var()).getArray()) {
    for (auto &connVar : *connsArr) {
      TConnection connection;
      if (jsonToConnection(connection, connVar))
        doc.connections.push_back(std::move(connection));
    }
  }

  if (auto *framesArr = migratedJson.getProperty("frames", juce::var()).getArray()) {
    for (auto &frameVar : *framesArr) {
      TFrameRegion frame;
      if (jsonToFrame(frame, frameVar))
        doc.frames.push_back(std::move(frame));
    }
  }

  if (auto *bookmarksArr =
          migratedJson.getProperty("bookmarks", juce::var()).getArray()) {
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
    appendWarning(migrationReport.warnings,
                  "Document frame id sequence was repaired from frame contents.");
  }

  if (doc.getNextBookmarkId() <= 0) {
    int maxBookmarkId = 0;
    for (const auto &bookmark : doc.bookmarks)
      maxBookmarkId = juce::jmax(maxBookmarkId, bookmark.bookmarkId);
    doc.setNextBookmarkId(maxBookmarkId + 1);
    appendWarning(
        migrationReport.warnings,
        "Document bookmark id sequence was repaired from bookmark contents.");
  }

  if (migrationReportOut != nullptr)
    *migrationReportOut = migrationReport;

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
  node.params.clear();
  node.ports.clear();

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
  frame.colorArgb =
      (juce::uint32)(int64_t)json.getProperty("color_argb", 0x334d8bf7);
  frame.collapsed = json.getProperty("collapsed", false);
  frame.locked = json.getProperty("locked", false);
  frame.logicalGroup = json.getProperty("logical_group", true);
  frame.membershipExplicit = json.getProperty("membership_explicit", false);
  frame.memberNodeIds.clear();

  if (auto *membersArr =
          json.getProperty("member_node_ids", juce::var()).getArray()) {
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
