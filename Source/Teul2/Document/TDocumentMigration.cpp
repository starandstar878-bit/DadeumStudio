#include "TDocumentMigration.h"

#include "TTeulDocument.h"

#include <algorithm>

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

template <typename TReport>
void appendMigrationStepImpl(TReport *report, const juce::String &stepName) {
  if (report == nullptr)
    return;

  const auto normalized = stepName.trim();
  if (normalized.isEmpty())
    return;
  if (!report->appliedSteps.contains(normalized))
    report->appliedSteps.add(normalized);
}

juce::var normalizePatchPresetJson(const juce::var &rootVar,
                                   int targetSchemaVersion) {
  if (!rootVar.isObject())
    return {};

  const auto *root = rootVar.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("format",
                      propertyOrAlias(root, {"format"}, "teul.patch_preset"));
  object->setProperty(
      "schema_version",
      propertyOrAlias(root, {"schema_version", "schemaVersion"},
                      targetSchemaVersion));
  object->setProperty("preset_name",
                      propertyOrAlias(root, {"preset_name", "presetName"}));
  object->setProperty("source_frame_uuid",
                      propertyOrAlias(root, {"source_frame_uuid", "sourceFrameUuid"}));
  object->setProperty("saved_at",
                      propertyOrAlias(root, {"saved_at", "savedAt"}));
  object->setProperty("summary",
                      propertyOrAlias(root, {"summary", "presetSummary"}));
  object->setProperty(
      "graph",
      propertyOrAlias(root, {"graph", "graphPayload", "graph_json"}));
  return juce::var(object);
}

juce::var normalizeStatePresetJson(const juce::var &rootVar,
                                   int targetSchemaVersion) {
  if (!rootVar.isObject())
    return {};

  const auto *root = rootVar.getDynamicObject();
  auto *object = new juce::DynamicObject();
  object->setProperty("format",
                      propertyOrAlias(root, {"format"}, "teul.state_preset"));
  object->setProperty(
      "schema_version",
      propertyOrAlias(root, {"schema_version", "schemaVersion"},
                      targetSchemaVersion));
  object->setProperty("preset_name",
                      propertyOrAlias(root, {"preset_name", "presetName"}));
  object->setProperty("target_graph_name",
                      propertyOrAlias(root, {"target_graph_name", "targetGraphName"}));
  object->setProperty("summary",
                      propertyOrAlias(root, {"summary", "presetSummary"}));
  object->setProperty("node_states",
                      propertyOrAlias(root, {"node_states", "nodeStates"}));
  return juce::var(object);
}

} // namespace

void TDocumentMigration::appendWarning(juce::StringArray &warnings,
                                       const juce::String &warning) {
  const auto normalized = warning.trim();
  if (normalized.isEmpty())
    return;
  if (!warnings.contains(normalized))
    warnings.add(normalized);
}

void TDocumentMigration::appendMigrationStep(TSchemaMigrationReport *report,
                                             const juce::String &stepName) {
  appendMigrationStepImpl(report, stepName);
}

void TDocumentMigration::appendMigrationStep(TPatchPresetLoadReport *report,
                                             const juce::String &stepName) {
  appendMigrationStepImpl(report, stepName);
}

void TDocumentMigration::appendMigrationStep(TStatePresetLoadReport *report,
                                             const juce::String &stepName) {
  appendMigrationStepImpl(report, stepName);
}

juce::String TDocumentMigration::joinWarnings(
    const juce::StringArray &warnings) {
  juce::StringArray normalizedWarnings;
  for (const auto &warning : warnings) {
    const auto normalized = warning.trim();
    if (normalized.isNotEmpty() && !normalizedWarnings.contains(normalized))
      normalizedWarnings.add(normalized);
  }

  return normalizedWarnings.joinIntoString(" | ");
}

juce::Result TDocumentMigration::parseJsonFile(const juce::File &file,
                                               juce::var &jsonOut,
                                               const juce::String &label) {
  if (!file.existsAsFile()) {
    return juce::Result::fail(label + " file does not exist: " +
                              file.getFullPathName());
  }

  const auto parseResult = juce::JSON::parse(file.loadFileAsString(), jsonOut);
  if (parseResult.failed()) {
    return juce::Result::fail("Failed to parse " + label + ": " +
                              parseResult.getErrorMessage());
  }

  return juce::Result::ok();
}
namespace {

const TSystemRailEndpoint *findRailEndpointByKind(const TTeulDocument &document,
                                                  TSystemRailEndpointKind kind) {
  for (const auto &endpoint : document.controlState.inputEndpoints) {
    if (endpoint.kind == kind)
      return &endpoint;
  }

  for (const auto &endpoint : document.controlState.outputEndpoints) {
    if (endpoint.kind == kind)
      return &endpoint;
  }

  return nullptr;
}

const TSystemRailPort *findRailPortByDisplayName(
    const TSystemRailEndpoint *endpoint,
    juce::StringRef displayName) {
  if (endpoint == nullptr)
    return nullptr;

  for (const auto &port : endpoint->ports) {
    if (port.displayName.equalsIgnoreCase(displayName))
      return &port;
  }

  return nullptr;
}

bool isLegacyRailSourceNode(const juce::String &typeKey) {
  return typeKey == "Teul.Source.AudioInput" ||
         typeKey == "Teul.Source.MidiInput";
}

bool isLegacyRailSinkNode(const juce::String &typeKey) {
  return typeKey == "Teul.Routing.AudioOut" ||
         typeKey == "Teul.Midi.MidiOut";
}

bool isLegacyMidiOutputBridgeNode(const TNode &node) {
  if (node.typeKey != "Teul.Midi.MidiOut")
    return false;

  if (node.ports.size() != 1)
    return false;

  const auto &port = node.ports.front();
  return port.direction == TPortDirection::Input &&
         port.dataType == TPortDataType::MIDI;
}

bool endpointsEqual(const TEndpoint &lhs, const TEndpoint &rhs) {
  return lhs.ownerKind == rhs.ownerKind && lhs.nodeId == rhs.nodeId &&
         lhs.portId == rhs.portId && lhs.railEndpointId == rhs.railEndpointId &&
         lhs.railPortId == rhs.railPortId;
}

bool hasConnectionWithEndpoints(const std::vector<TConnection> &connections,
                                const TConnection &candidate) {
  return std::any_of(connections.begin(), connections.end(),
                     [&](const TConnection &existing) {
                       return endpointsEqual(existing.from, candidate.from) &&
                              endpointsEqual(existing.to, candidate.to);
                     });
}

bool tryMapLegacySourcePort(const TTeulDocument &document,
                            const TNode &node,
                            PortId portId,
                            TEndpoint &mappedOut) {
  const auto *port = node.findPort(portId);
  if (port == nullptr)
    return false;

  if (node.typeKey == "Teul.Source.AudioInput") {
    const auto *endpoint =
        findRailEndpointByKind(document, TSystemRailEndpointKind::audioInput);
    if (endpoint == nullptr)
      return false;

    const TSystemRailPort *railPort = nullptr;
    if (port->name.startsWithIgnoreCase("L "))
      railPort = findRailPortByDisplayName(endpoint, "L");
    else if (port->name.startsWithIgnoreCase("R "))
      railPort = findRailPortByDisplayName(endpoint, "R");

    if (railPort == nullptr)
      return false;

    mappedOut = TEndpoint::makeRailPort(endpoint->endpointId, railPort->portId);
    return true;
  }

  if (node.typeKey == "Teul.Source.MidiInput") {
    const auto *endpoint =
        findRailEndpointByKind(document, TSystemRailEndpointKind::midiInput);
    if (endpoint == nullptr || endpoint->ports.empty())
      return false;

    mappedOut =
        TEndpoint::makeRailPort(endpoint->endpointId, endpoint->ports.front().portId);
    return true;
  }

  return false;
}

bool tryMapLegacySinkPort(const TTeulDocument &document,
                          const TNode &node,
                          PortId portId,
                          TEndpoint &mappedOut) {
  const auto *port = node.findPort(portId);
  if (port == nullptr)
    return false;

  if (node.typeKey == "Teul.Routing.AudioOut") {
    const auto *endpoint =
        findRailEndpointByKind(document, TSystemRailEndpointKind::audioOutput);
    if (endpoint == nullptr)
      return false;

    const TSystemRailPort *railPort = nullptr;
    if (port->name.startsWithIgnoreCase("L "))
      railPort = findRailPortByDisplayName(endpoint, "L");
    else if (port->name.startsWithIgnoreCase("R "))
      railPort = findRailPortByDisplayName(endpoint, "R");

    if (railPort == nullptr)
      return false;

    mappedOut = TEndpoint::makeRailPort(endpoint->endpointId, railPort->portId);
    return true;
  }

  if (node.typeKey == "Teul.Midi.MidiOut") {
    const auto *endpoint =
        findRailEndpointByKind(document, TSystemRailEndpointKind::midiOutput);
    if (endpoint == nullptr || endpoint->ports.empty())
      return false;

    mappedOut =
        TEndpoint::makeRailPort(endpoint->endpointId, endpoint->ports.front().portId);
    return true;
  }

  return false;
}

} // namespace
bool TDocumentMigration::usesLegacyDocumentAliases(const juce::var &json) {
  const auto *root = json.getDynamicObject();
  if (root == nullptr)
    return false;

  if (hasAnyProperty(root, {"schemaVersion", "nextNodeId", "nextPortId",
                            "nextConnectionId", "nextFrameId",
                            "nextBookmarkId", "graphMeta", "controlState", "node_list",
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

void TDocumentMigration::normalizeLegacyRailBridgeNodes(
    TTeulDocument &document,
    TSchemaMigrationReport &migrationReport) {
  document.controlState.ensurePreviewDataIfEmpty();

  std::vector<bool> removeConnections(document.connections.size(), false);
  std::vector<TConnection> migratedConnections;
  std::vector<NodeId> nodesToRemove;

  for (const auto &node : document.nodes) {
    const bool isSource = isLegacyRailSourceNode(node.typeKey);
    bool isSink = isLegacyRailSinkNode(node.typeKey);
    if (node.typeKey == "Teul.Midi.MidiOut")
      isSink = isLegacyMidiOutputBridgeNode(node);
    if (!isSource && !isSink)
      continue;

    std::vector<size_t> referencedConnectionIndexes;
    for (size_t connectionIndex = 0; connectionIndex < document.connections.size();
         ++connectionIndex) {
      const auto &connection = document.connections[connectionIndex];
      if (connection.from.referencesNode(node.nodeId) ||
          connection.to.referencesNode(node.nodeId)) {
        referencedConnectionIndexes.push_back(connectionIndex);
      }
    }

    bool migratable = true;
    std::vector<TConnection> localMigrations;
    for (const auto connectionIndex : referencedConnectionIndexes) {
      const auto &connection = document.connections[connectionIndex];
      if (isSource) {
        if (!connection.from.referencesNode(node.nodeId) ||
            connection.to.referencesNode(node.nodeId)) {
          migratable = false;
          break;
        }

        TEndpoint mappedFrom;
        if (!tryMapLegacySourcePort(document, node, connection.from.portId,
                                    mappedFrom)) {
          migratable = false;
          break;
        }

        auto migrated = connection;
        migrated.from = mappedFrom;
        localMigrations.push_back(std::move(migrated));
        continue;
      }

      if (!connection.to.referencesNode(node.nodeId) ||
          connection.from.referencesNode(node.nodeId)) {
        migratable = false;
        break;
      }

      TEndpoint mappedTo;
      if (!tryMapLegacySinkPort(document, node, connection.to.portId,
                                mappedTo)) {
        migratable = false;
        break;
      }

      auto migrated = connection;
      migrated.to = mappedTo;
      localMigrations.push_back(std::move(migrated));
    }

    if (!migratable)
      continue;

    nodesToRemove.push_back(node.nodeId);
    for (const auto connectionIndex : referencedConnectionIndexes)
      removeConnections[connectionIndex] = true;
    migratedConnections.insert(migratedConnections.end(), localMigrations.begin(),
                               localMigrations.end());
  }

  if (nodesToRemove.empty())
    return;

  std::vector<TConnection> normalizedConnections;
  normalizedConnections.reserve(document.connections.size());
  for (size_t connectionIndex = 0; connectionIndex < document.connections.size();
       ++connectionIndex) {
    if (!removeConnections[connectionIndex])
      normalizedConnections.push_back(document.connections[connectionIndex]);
  }

  for (const auto &connection : migratedConnections) {
    if (!hasConnectionWithEndpoints(normalizedConnections, connection))
      normalizedConnections.push_back(connection);
  }

  document.connections = std::move(normalizedConnections);
  document.nodes.erase(
      std::remove_if(document.nodes.begin(), document.nodes.end(),
                     [&](const TNode &node) {
                       return std::find(nodesToRemove.begin(), nodesToRemove.end(),
                                        node.nodeId) != nodesToRemove.end();
                     }),
      document.nodes.end());

  for (auto &frame : document.frames) {
    frame.memberNodeIds.erase(
        std::remove_if(frame.memberNodeIds.begin(), frame.memberNodeIds.end(),
                       [&](NodeId nodeId) {
                         return std::find(nodesToRemove.begin(), nodesToRemove.end(),
                                          nodeId) != nodesToRemove.end();
                       }),
        frame.memberNodeIds.end());
  }

  appendMigrationStep(&migrationReport, "document:normalize-legacy-rail-io");
  appendWarning(
      migrationReport.warnings,
      "Legacy Audio Input / MIDI Input / Audio Output / MIDI Output nodes were moved to rail endpoints.");
  migrationReport.migrated = true;
}
bool TDocumentMigration::usesLegacyPatchAliases(const juce::var &json) {
  const auto *root = json.getDynamicObject();
  return root != nullptr &&
         (root->hasProperty("schemaVersion") ||
          root->hasProperty("presetName") ||
          root->hasProperty("sourceFrameUuid") ||
          root->hasProperty("presetSummary") ||
          root->hasProperty("graphPayload") ||
          root->hasProperty("graph_json"));
}

juce::var TDocumentMigration::migratePatchPresetJson(
    const juce::var &json,
    int sourceSchemaVersion,
    int targetSchemaVersion,
    TPatchPresetLoadReport *report) {
  if (sourceSchemaVersion <= 1) {
    appendMigrationStep(report, "patch:v1->v2");
    return normalizePatchPresetJson(json, targetSchemaVersion);
  }

  return normalizePatchPresetJson(json, targetSchemaVersion);
}

bool TDocumentMigration::usesLegacyStateAliases(const juce::var &json) {
  const auto *root = json.getDynamicObject();
  return root != nullptr &&
         (root->hasProperty("schemaVersion") ||
          root->hasProperty("presetName") ||
          root->hasProperty("targetGraphName") ||
          root->hasProperty("presetSummary") ||
          root->hasProperty("nodeStates"));
}

juce::var TDocumentMigration::migrateStatePresetJson(
    const juce::var &json,
    int sourceSchemaVersion,
    int targetSchemaVersion,
    TStatePresetLoadReport *report) {
  if (sourceSchemaVersion <= 1) {
    appendMigrationStep(report, "state:v1->v2");
    return normalizeStatePresetJson(json, targetSchemaVersion);
  }

  return normalizeStatePresetJson(json, targetSchemaVersion);
}

} // namespace Teul
