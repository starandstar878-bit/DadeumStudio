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

const TSystemRailEndpoint *findRailEndpointByKind(const TGraphDocument &doc,
                                                  TSystemRailEndpointKind kind) {
  for (const auto &endpoint : doc.controlState.inputEndpoints) {
    if (endpoint.kind == kind)
      return &endpoint;
  }

  for (const auto &endpoint : doc.controlState.outputEndpoints) {
    if (endpoint.kind == kind)
      return &endpoint;
  }

  return nullptr;
}

const TSystemRailPort *findRailPortByDisplayName(const TSystemRailEndpoint *endpoint,
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

bool endpointsEqual(const TEndpoint &a, const TEndpoint &b) {
  return a.ownerKind == b.ownerKind && a.nodeId == b.nodeId &&
         a.portId == b.portId && a.railEndpointId == b.railEndpointId &&
         a.railPortId == b.railPortId;
}

bool hasConnectionWithEndpoints(const std::vector<TConnection> &connections,
                                const TConnection &candidate) {
  return std::any_of(connections.begin(), connections.end(),
                     [&](const TConnection &existing) {
                       return endpointsEqual(existing.from, candidate.from) &&
                              endpointsEqual(existing.to, candidate.to);
                     });
}

bool tryMapLegacySourcePort(const TGraphDocument &doc,
                            const TNode &node,
                            PortId portId,
                            TEndpoint &mappedOut) {
  const auto *port = node.findPort(portId);
  if (port == nullptr)
    return false;

  if (node.typeKey == "Teul.Source.AudioInput") {
    const auto *endpoint =
        findRailEndpointByKind(doc, TSystemRailEndpointKind::audioInput);
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
        findRailEndpointByKind(doc, TSystemRailEndpointKind::midiInput);
    if (endpoint == nullptr || endpoint->ports.empty())
      return false;

    mappedOut =
        TEndpoint::makeRailPort(endpoint->endpointId, endpoint->ports.front().portId);
    return true;
  }

  return false;
}

bool tryMapLegacySinkPort(const TGraphDocument &doc,
                          const TNode &node,
                          PortId portId,
                          TEndpoint &mappedOut) {
  const auto *port = node.findPort(portId);
  if (port == nullptr)
    return false;

  if (node.typeKey == "Teul.Routing.AudioOut") {
    const auto *endpoint =
        findRailEndpointByKind(doc, TSystemRailEndpointKind::audioOutput);
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
        findRailEndpointByKind(doc, TSystemRailEndpointKind::midiOutput);
    if (endpoint == nullptr || endpoint->ports.empty())
      return false;

    mappedOut =
        TEndpoint::makeRailPort(endpoint->endpointId, endpoint->ports.front().portId);
    return true;
  }

  return false;
}

void normalizeLegacyRailBridgeNodes(TGraphDocument &doc,
                                    TSchemaMigrationReport &migrationReport) {
  doc.controlState.ensurePreviewDataIfEmpty();

  std::vector<bool> removeConnections(doc.connections.size(), false);
  std::vector<TConnection> migratedConnections;
  std::vector<NodeId> nodesToRemove;

  for (const auto &node : doc.nodes) {
    const bool isSource = isLegacyRailSourceNode(node.typeKey);
    const bool isSink = isLegacyRailSinkNode(node.typeKey);
    if (!isSource && !isSink)
      continue;

    std::vector<size_t> referencedConnectionIndexes;
    for (size_t connectionIndex = 0; connectionIndex < doc.connections.size();
         ++connectionIndex) {
      const auto &connection = doc.connections[connectionIndex];
      if (connection.from.referencesNode(node.nodeId) ||
          connection.to.referencesNode(node.nodeId)) {
        referencedConnectionIndexes.push_back(connectionIndex);
      }
    }

    bool migratable = true;
    std::vector<TConnection> localMigrations;
    for (const auto connectionIndex : referencedConnectionIndexes) {
      const auto &connection = doc.connections[connectionIndex];
      if (isSource) {
        if (!connection.from.referencesNode(node.nodeId) ||
            connection.to.referencesNode(node.nodeId)) {
          migratable = false;
          break;
        }

        TEndpoint mappedFrom;
        if (!tryMapLegacySourcePort(doc, node, connection.from.portId, mappedFrom)) {
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
      if (!tryMapLegacySinkPort(doc, node, connection.to.portId, mappedTo)) {
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
  normalizedConnections.reserve(doc.connections.size());
  for (size_t connectionIndex = 0; connectionIndex < doc.connections.size();
       ++connectionIndex) {
    if (!removeConnections[connectionIndex])
      normalizedConnections.push_back(doc.connections[connectionIndex]);
  }

  for (const auto &connection : migratedConnections) {
    if (!hasConnectionWithEndpoints(normalizedConnections, connection))
      normalizedConnections.push_back(connection);
  }

  doc.connections = std::move(normalizedConnections);
  doc.nodes.erase(std::remove_if(doc.nodes.begin(), doc.nodes.end(),
                                 [&](const TNode &node) {
                                   return std::find(nodesToRemove.begin(),
                                                    nodesToRemove.end(),
                                                    node.nodeId) !=
                                          nodesToRemove.end();
                                 }),
                  doc.nodes.end());

  for (auto &frame : doc.frames) {
    frame.memberNodeIds.erase(
        std::remove_if(frame.memberNodeIds.begin(), frame.memberNodeIds.end(),
                       [&](NodeId nodeId) {
                         return std::find(nodesToRemove.begin(), nodesToRemove.end(),
                                          nodeId) != nodesToRemove.end();
                       }),
        frame.memberNodeIds.end());
  }

  appendMigrationStep(&migrationReport, "document:normalize-legacy-rail-io");
  appendWarning(migrationReport.warnings,
                "Legacy Audio Input / MIDI Input / Audio Output / MIDI Output nodes were moved to rail endpoints.");
  migrationReport.migrated = true;
}

juce::String railKindToString(TRailKind kind) {
  switch (kind) {
  case TRailKind::input:
    return "input";
  case TRailKind::output:
    return "output";
  case TRailKind::controlSource:
    return "control_source";
  }

  return "control_source";
}

TRailKind railKindFromVar(const juce::var &value) {
  if (value.isInt() || value.isInt64())
    return static_cast<TRailKind>((int)value);

  const auto text = value.toString().trim().toLowerCase();
  if (text == "input")
    return TRailKind::input;
  if (text == "output")
    return TRailKind::output;
  return TRailKind::controlSource;
}

juce::String systemRailEndpointKindToString(TSystemRailEndpointKind kind) {
  switch (kind) {
  case TSystemRailEndpointKind::audioInput:
    return "audio_input";
  case TSystemRailEndpointKind::audioOutput:
    return "audio_output";
  case TSystemRailEndpointKind::midiInput:
    return "midi_input";
  case TSystemRailEndpointKind::midiOutput:
    return "midi_output";
  }

  return "audio_input";
}

TSystemRailEndpointKind systemRailEndpointKindFromVar(const juce::var &value) {
  if (value.isInt() || value.isInt64())
    return static_cast<TSystemRailEndpointKind>((int)value);

  const auto text = value.toString().trim().toLowerCase();
  if (text == "audio_output")
    return TSystemRailEndpointKind::audioOutput;
  if (text == "midi_input")
    return TSystemRailEndpointKind::midiInput;
  if (text == "midi_output")
    return TSystemRailEndpointKind::midiOutput;
  return TSystemRailEndpointKind::audioInput;
}

juce::String controlSourceKindToString(TControlSourceKind kind) {
  switch (kind) {
  case TControlSourceKind::expression:
    return "expression";
  case TControlSourceKind::footswitch:
    return "footswitch";
  case TControlSourceKind::trigger:
    return "trigger";
  case TControlSourceKind::midiCc:
    return "midi_cc";
  case TControlSourceKind::midiNote:
    return "midi_note";
  case TControlSourceKind::macro:
    return "macro";
  }

  return "expression";
}

TControlSourceKind controlSourceKindFromVar(const juce::var &value) {
  if (value.isInt() || value.isInt64())
    return static_cast<TControlSourceKind>((int)value);

  const auto text = value.toString().trim().toLowerCase();
  if (text == "footswitch")
    return TControlSourceKind::footswitch;
  if (text == "trigger")
    return TControlSourceKind::trigger;
  if (text == "midi_cc")
    return TControlSourceKind::midiCc;
  if (text == "midi_note")
    return TControlSourceKind::midiNote;
  if (text == "macro")
    return TControlSourceKind::macro;
  return TControlSourceKind::expression;
}

juce::String controlSourceModeToString(TControlSourceMode mode) {
  switch (mode) {
  case TControlSourceMode::continuous:
    return "continuous";
  case TControlSourceMode::momentary:
    return "momentary";
  case TControlSourceMode::toggle:
    return "toggle";
  }

  return "continuous";
}

TControlSourceMode controlSourceModeFromVar(const juce::var &value) {
  if (value.isInt() || value.isInt64())
    return static_cast<TControlSourceMode>((int)value);

  const auto text = value.toString().trim().toLowerCase();
  if (text == "momentary")
    return TControlSourceMode::momentary;
  if (text == "toggle")
    return TControlSourceMode::toggle;
  return TControlSourceMode::continuous;
}

juce::String controlPortKindToString(TControlPortKind kind) {
  switch (kind) {
  case TControlPortKind::value:
    return "value";
  case TControlPortKind::gate:
    return "gate";
  case TControlPortKind::trigger:
    return "trigger";
  }

  return "value";
}

TControlPortKind controlPortKindFromVar(const juce::var &value) {
  if (value.isInt() || value.isInt64())
    return static_cast<TControlPortKind>((int)value);

  const auto text = value.toString().trim().toLowerCase();
  if (text == "gate")
    return TControlPortKind::gate;
  if (text == "trigger")
    return TControlPortKind::trigger;
  return TControlPortKind::value;
}

juce::var controlRailToJson(const TControlRailLayout &rail) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", rail.railId);
  object->setProperty("title", rail.title);
  object->setProperty("kind", railKindToString(rail.kind));
  object->setProperty("collapsed", rail.collapsed);
  object->setProperty("order", rail.order);
  return juce::var(object);
}

bool jsonToControlRail(TControlRailLayout &rail, const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  rail.railId = propertyOrAlias(object, {"id", "rail_id", "railId"}).toString();
  if (rail.railId.isEmpty())
    return false;

  rail.title =
      propertyOrAlias(object, {"title", "display_name", "displayName"}, rail.railId)
          .toString();
  rail.kind = railKindFromVar(
      propertyOrAlias(object, {"kind", "rail_kind", "railKind"}, "control_source"));
  rail.collapsed = (bool)propertyOrAlias(object, {"collapsed"}, false);
  rail.order = (int)propertyOrAlias(object, {"order"}, 0);
  return true;
}

juce::var systemRailPortToJson(const TSystemRailPort &port) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", port.portId);
  object->setProperty("display_name", port.displayName);
  object->setProperty("type", (int)port.dataType);
  return juce::var(object);
}

bool jsonToSystemRailPort(TSystemRailPort &port, const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  port.portId = propertyOrAlias(object, {"id", "port_id", "portId"}).toString();
  if (port.portId.isEmpty())
    return false;

  port.displayName =
      propertyOrAlias(object, {"display_name", "displayName", "name"}, port.portId)
          .toString();
  port.dataType = (TPortDataType)(int)propertyOrAlias(object, {"type", "data_type", "dataType"}, 0);
  return true;
}

juce::var systemRailEndpointToJson(const TSystemRailEndpoint &endpoint) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", endpoint.endpointId);
  object->setProperty("rail_id", endpoint.railId);
  object->setProperty("display_name", endpoint.displayName);
  object->setProperty("subtitle", endpoint.subtitle);
  object->setProperty("kind", systemRailEndpointKindToString(endpoint.kind));
  object->setProperty("stereo", endpoint.stereo);
  object->setProperty("missing", endpoint.missing);
  object->setProperty("degraded", endpoint.degraded);
  object->setProperty("order", endpoint.order);

  juce::Array<juce::var> portsArray;
  for (const auto &port : endpoint.ports)
    portsArray.add(systemRailPortToJson(port));
  object->setProperty("ports", portsArray);
  return juce::var(object);
}

bool jsonToSystemRailEndpoint(TSystemRailEndpoint &endpoint,
                              const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  endpoint.endpointId = propertyOrAlias(object, {"id", "endpoint_id", "endpointId"}).toString();
  if (endpoint.endpointId.isEmpty())
    return false;

  endpoint.railId = propertyOrAlias(object, {"rail_id", "railId"}).toString();
  endpoint.displayName =
      propertyOrAlias(object, {"display_name", "displayName", "name"}, endpoint.endpointId)
          .toString();
  endpoint.subtitle = propertyOrAlias(object, {"subtitle", "detail"}).toString();
  endpoint.kind = systemRailEndpointKindFromVar(
      propertyOrAlias(object, {"kind", "endpoint_kind", "endpointKind"}, "audio_input"));
  endpoint.stereo = (bool)propertyOrAlias(object, {"stereo"}, false);
  endpoint.missing = (bool)propertyOrAlias(object, {"missing"}, false);
  endpoint.degraded = (bool)propertyOrAlias(object, {"degraded"}, false);
  endpoint.order = (int)propertyOrAlias(object, {"order"}, 0);
  endpoint.ports.clear();

  if (auto *portsArray = propertyOrAlias(object, {"ports"}).getArray()) {
    for (const auto &portVar : *portsArray) {
      TSystemRailPort port;
      if (jsonToSystemRailPort(port, portVar))
        endpoint.ports.push_back(std::move(port));
    }
  }

  return true;
}

juce::var controlSourcePortToJson(const TControlSourcePort &port) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", port.portId);
  object->setProperty("display_name", port.displayName);
  object->setProperty("kind", controlPortKindToString(port.kind));
  return juce::var(object);
}

bool jsonToControlSourcePort(TControlSourcePort &port, const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  port.portId = propertyOrAlias(object, {"id", "port_id", "portId"}).toString();
  if (port.portId.isEmpty())
    return false;

  port.displayName =
      propertyOrAlias(object, {"display_name", "displayName", "name"}, port.portId)
          .toString();
  port.kind = controlPortKindFromVar(
      propertyOrAlias(object, {"kind", "port_kind", "portKind"}, "value"));
  return true;
}

juce::var deviceBindingSignatureToJson(const TDeviceBindingSignature &binding) {
  auto *object = new juce::DynamicObject();
  object->setProperty("midi_device_name", binding.midiDeviceName);
  object->setProperty("hardware_id", binding.hardwareId);
  object->setProperty("midi_channel", binding.midiChannel);
  object->setProperty("controller_number", binding.controllerNumber);
  object->setProperty("note_number", binding.noteNumber);
  return juce::var(object);
}

bool jsonToDeviceBindingSignature(TDeviceBindingSignature &binding,
                                  const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  binding.midiDeviceName =
      propertyOrAlias(object, {"midi_device_name", "midiDeviceName"}).toString();
  binding.hardwareId = propertyOrAlias(object, {"hardware_id", "hardwareId"}).toString();
  binding.midiChannel = (int)propertyOrAlias(object, {"midi_channel", "midiChannel"}, 0);
  binding.controllerNumber =
      (int)propertyOrAlias(object, {"controller_number", "controllerNumber"}, -1);
  binding.noteNumber = (int)propertyOrAlias(object, {"note_number", "noteNumber"}, -1);
  return true;
}

juce::var deviceSourceProfileToJson(const TDeviceSourceProfile &source) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", source.sourceId);
  object->setProperty("display_name", source.displayName);
  object->setProperty("kind", controlSourceKindToString(source.kind));
  object->setProperty("mode", controlSourceModeToString(source.mode));

  juce::Array<juce::var> portsArray;
  for (const auto &port : source.ports)
    portsArray.add(controlSourcePortToJson(port));
  object->setProperty("ports", portsArray);

  juce::Array<juce::var> bindingsArray;
  for (const auto &binding : source.bindings)
    bindingsArray.add(deviceBindingSignatureToJson(binding));
  object->setProperty("bindings", bindingsArray);
  return juce::var(object);
}

bool jsonToDeviceSourceProfile(TDeviceSourceProfile &source, const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  source.sourceId = propertyOrAlias(object, {"id", "source_id", "sourceId"}).toString();
  if (source.sourceId.isEmpty())
    return false;

  source.displayName =
      propertyOrAlias(object, {"display_name", "displayName"}, source.sourceId).toString();
  source.kind = controlSourceKindFromVar(
      propertyOrAlias(object, {"kind", "source_kind", "sourceKind"}, "expression"));
  source.mode = controlSourceModeFromVar(
      propertyOrAlias(object, {"mode", "source_mode", "sourceMode"}, "continuous"));
  source.ports.clear();
  source.bindings.clear();

  if (auto *portsArray = propertyOrAlias(object, {"ports"}).getArray()) {
    for (const auto &portVar : *portsArray) {
      TControlSourcePort port;
      if (jsonToControlSourcePort(port, portVar))
        source.ports.push_back(std::move(port));
    }
  }

  if (auto *bindingsArray = propertyOrAlias(object, {"bindings"}).getArray()) {
    for (const auto &bindingVar : *bindingsArray) {
      TDeviceBindingSignature binding;
      if (jsonToDeviceBindingSignature(binding, bindingVar))
        source.bindings.push_back(std::move(binding));
    }
  }

  return true;
}

juce::var deviceProfileToJson(const TDeviceProfile &profile) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", profile.profileId);
  object->setProperty("device_id", profile.deviceId);
  object->setProperty("display_name", profile.displayName);
  object->setProperty("auto_detected", profile.autoDetected);

  juce::Array<juce::var> sourcesArray;
  for (const auto &source : profile.sources)
    sourcesArray.add(deviceSourceProfileToJson(source));
  object->setProperty("sources", sourcesArray);
  return juce::var(object);
}

bool jsonToDeviceProfile(TDeviceProfile &profile, const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  profile.profileId = propertyOrAlias(object, {"id", "profile_id", "profileId"}).toString();
  if (profile.profileId.isEmpty())
    return false;

  profile.deviceId = propertyOrAlias(object, {"device_id", "deviceId"}).toString();
  profile.displayName =
      propertyOrAlias(object, {"display_name", "displayName"}, profile.profileId).toString();
  profile.autoDetected = (bool)propertyOrAlias(object, {"auto_detected", "autoDetected"}, false);
  profile.sources.clear();

  if (auto *sourcesArray = propertyOrAlias(object, {"sources"}).getArray()) {
    for (const auto &sourceVar : *sourcesArray) {
      TDeviceSourceProfile source;
      if (jsonToDeviceSourceProfile(source, sourceVar))
        profile.sources.push_back(std::move(source));
    }
  }

  return true;
}

juce::var controlSourceToJson(const TControlSource &source) {
  auto *object = new juce::DynamicObject();
  object->setProperty("id", source.sourceId);
  object->setProperty("device_profile_id", source.deviceProfileId);
  object->setProperty("rail_id", source.railId);
  object->setProperty("display_name", source.displayName);
  object->setProperty("kind", controlSourceKindToString(source.kind));
  object->setProperty("mode", controlSourceModeToString(source.mode));
  object->setProperty("auto_detected", source.autoDetected);
  object->setProperty("confirmed", source.confirmed);
  object->setProperty("missing", source.missing);
  object->setProperty("degraded", source.degraded);

  juce::Array<juce::var> portsArray;
  for (const auto &port : source.ports)
    portsArray.add(controlSourcePortToJson(port));
  object->setProperty("ports", portsArray);
  return juce::var(object);
}

bool jsonToControlSource(TControlSource &source, const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  source.sourceId = propertyOrAlias(object, {"id", "source_id", "sourceId"}).toString();
  if (source.sourceId.isEmpty())
    return false;

  source.deviceProfileId =
      propertyOrAlias(object, {"device_profile_id", "deviceProfileId"}).toString();
  source.railId = propertyOrAlias(object, {"rail_id", "railId"}, "control-rail").toString();
  source.displayName =
      propertyOrAlias(object, {"display_name", "displayName"}, source.sourceId).toString();
  source.kind = controlSourceKindFromVar(
      propertyOrAlias(object, {"kind", "source_kind", "sourceKind"}, "expression"));
  source.mode = controlSourceModeFromVar(
      propertyOrAlias(object, {"mode", "source_mode", "sourceMode"}, "continuous"));
  source.autoDetected = (bool)propertyOrAlias(object, {"auto_detected", "autoDetected"}, false);
  source.confirmed = (bool)propertyOrAlias(object, {"confirmed"}, false);
  source.missing = (bool)propertyOrAlias(object, {"missing"}, false);
  source.degraded = (bool)propertyOrAlias(object, {"degraded"}, false);
  source.ports.clear();

  if (auto *portsArray = propertyOrAlias(object, {"ports"}).getArray()) {
    for (const auto &portVar : *portsArray) {
      TControlSourcePort port;
      if (jsonToControlSourcePort(port, portVar))
        source.ports.push_back(std::move(port));
    }
  }

  return true;
}

juce::var controlAssignmentToJson(const TControlSourceAssignment &assignment) {
  auto *object = new juce::DynamicObject();
  object->setProperty("source_id", assignment.sourceId);
  object->setProperty("port_id", assignment.portId);
  object->setProperty("target_node_id", (int64_t)assignment.targetNodeId);
  object->setProperty("target_param_id", assignment.targetParamId);
  object->setProperty("enabled", assignment.enabled);
  object->setProperty("inverted", assignment.inverted);
  object->setProperty("range_min", assignment.rangeMin);
  object->setProperty("range_max", assignment.rangeMax);
  return juce::var(object);
}

bool jsonToControlAssignment(TControlSourceAssignment &assignment,
                             const juce::var &json) {
  const auto *object = json.getDynamicObject();
  if (object == nullptr)
    return false;

  assignment.sourceId = propertyOrAlias(object, {"source_id", "sourceId"}).toString();
  assignment.portId = propertyOrAlias(object, {"port_id", "portId"}).toString();
  assignment.targetNodeId =
      (NodeId)(int64_t)propertyOrAlias(object, {"target_node_id", "targetNodeId"}, 0);
  assignment.targetParamId =
      propertyOrAlias(object, {"target_param_id", "targetParamId"}).toString();
  assignment.enabled = (bool)propertyOrAlias(object, {"enabled"}, true);
  assignment.inverted = (bool)propertyOrAlias(object, {"inverted"}, false);
  assignment.rangeMin = (float)(double)propertyOrAlias(object, {"range_min", "rangeMin"}, 0.0);
  assignment.rangeMax = (float)(double)propertyOrAlias(object, {"range_max", "rangeMax"}, 1.0);
  return assignment.sourceId.isNotEmpty() && assignment.portId.isNotEmpty();
}

juce::var controlStateToJson(const TControlSourceState &state) {
  auto *object = new juce::DynamicObject();

  juce::Array<juce::var> railsArray;
  for (const auto &rail : state.rails)
    railsArray.add(controlRailToJson(rail));
  object->setProperty("rails", railsArray);

  juce::Array<juce::var> inputEndpointsArray;
  for (const auto &endpoint : state.inputEndpoints)
    inputEndpointsArray.add(systemRailEndpointToJson(endpoint));
  object->setProperty("input_endpoints", inputEndpointsArray);

  juce::Array<juce::var> outputEndpointsArray;
  for (const auto &endpoint : state.outputEndpoints)
    outputEndpointsArray.add(systemRailEndpointToJson(endpoint));
  object->setProperty("output_endpoints", outputEndpointsArray);

  juce::Array<juce::var> sourcesArray;
  for (const auto &source : state.sources)
    sourcesArray.add(controlSourceToJson(source));
  object->setProperty("sources", sourcesArray);

  juce::Array<juce::var> profilesArray;
  for (const auto &profile : state.deviceProfiles)
    profilesArray.add(deviceProfileToJson(profile));
  object->setProperty("device_profiles", profilesArray);

  juce::Array<juce::var> assignmentsArray;
  for (const auto &assignment : state.assignments)
    assignmentsArray.add(controlAssignmentToJson(assignment));
  object->setProperty("assignments", assignmentsArray);

  juce::Array<juce::var> missingProfilesArray;
  for (const auto &profileId : state.missingDeviceProfileIds)
    missingProfilesArray.add(profileId);
  object->setProperty("missing_device_profile_ids", missingProfilesArray);
  return juce::var(object);
}

void jsonToControlState(TControlSourceState &state, const juce::var &json) {
  state = {};

  const auto *object = json.getDynamicObject();
  if (object == nullptr) {
    state.ensureDefaultRails();
    state.ensurePreviewDataIfEmpty();
    return;
  }

  state.rails.clear();
  state.inputEndpoints.clear();
  state.outputEndpoints.clear();
  state.sources.clear();
  state.deviceProfiles.clear();
  state.assignments.clear();
  state.missingDeviceProfileIds.clear();

  if (auto *railsArray = propertyOrAlias(object, {"rails", "rail_layouts", "railLayouts"}).getArray()) {
    for (const auto &railVar : *railsArray) {
      TControlRailLayout rail;
      if (jsonToControlRail(rail, railVar))
        state.rails.push_back(std::move(rail));
    }
  }

  if (auto *inputEndpointsArray = propertyOrAlias(object, {"input_endpoints", "inputEndpoints"}).getArray()) {
    for (const auto &endpointVar : *inputEndpointsArray) {
      TSystemRailEndpoint endpoint;
      if (jsonToSystemRailEndpoint(endpoint, endpointVar))
        state.inputEndpoints.push_back(std::move(endpoint));
    }
  }

  if (auto *outputEndpointsArray = propertyOrAlias(object, {"output_endpoints", "outputEndpoints"}).getArray()) {
    for (const auto &endpointVar : *outputEndpointsArray) {
      TSystemRailEndpoint endpoint;
      if (jsonToSystemRailEndpoint(endpoint, endpointVar))
        state.outputEndpoints.push_back(std::move(endpoint));
    }
  }

  if (auto *sourcesArray = propertyOrAlias(object, {"sources", "control_sources", "controlSources"}).getArray()) {
    for (const auto &sourceVar : *sourcesArray) {
      TControlSource source;
      if (jsonToControlSource(source, sourceVar))
        state.sources.push_back(std::move(source));
    }
  }

  if (auto *profilesArray = propertyOrAlias(object, {"device_profiles", "deviceProfiles"}).getArray()) {
    for (const auto &profileVar : *profilesArray) {
      TDeviceProfile profile;
      if (jsonToDeviceProfile(profile, profileVar))
        state.deviceProfiles.push_back(std::move(profile));
    }
  }

  if (auto *assignmentsArray = propertyOrAlias(object, {"assignments", "control_assignments", "controlAssignments"}).getArray()) {
    for (const auto &assignmentVar : *assignmentsArray) {
      TControlSourceAssignment assignment;
      if (jsonToControlAssignment(assignment, assignmentVar))
        state.assignments.push_back(std::move(assignment));
    }
  }

  if (auto *missingProfilesArray =
          propertyOrAlias(object, {"missing_device_profile_ids", "missingDeviceProfileIds"}).getArray()) {
    for (const auto &profileVar : *missingProfilesArray) {
      const auto profileId = profileVar.toString().trim();
      if (profileId.isNotEmpty())
        state.missingDeviceProfileIds.push_back(profileId);
    }
  }

  state.ensureDefaultRails();
  state.ensurePreviewDataIfEmpty();
  state.reconcileDeviceProfilesAndSources();
}

juce::var migrateControlStateJson(const juce::var &json) {
  TControlSourceState state;
  jsonToControlState(state, json);
  return controlStateToJson(state);
}

} // namespace

int TSerializer::currentSchemaVersion() noexcept { return 4; }

bool TSerializer::usesLegacyDocumentAliases(const juce::var &json) {
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
  object->setProperty(
      "control_state",
      migrateControlStateJson(
          propertyOrAlias(source, {"control_state", "controlState"})));
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
  object->setProperty(
      "control_state",
      migrateControlStateJson(
          propertyOrAlias(source, {"control_state", "controlState"})));
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
  object->setProperty(
      "control_state",
      migrateControlStateJson(
          propertyOrAlias(source, {"control_state", "controlState"})));
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
      endpoint->setProperty("owner_kind",
                            propertyOrAlias(sourceEndpoint,
                                            {"owner_kind", "ownerKind"},
                                            "node"));
      endpoint->setProperty(
          "node_id",
          propertyOrAlias(sourceEndpoint, {"node_id", "nodeId"}, 0));
      endpoint->setProperty(
          "port_id",
          propertyOrAlias(sourceEndpoint, {"port_id", "portId"}, 0));
      endpoint->setProperty(
          "rail_endpoint_id",
          propertyOrAlias(sourceEndpoint,
                          {"rail_endpoint_id", "railEndpointId",
                           "endpoint_id", "endpointId"},
                          juce::String()));
      endpoint->setProperty(
          "rail_port_id",
          propertyOrAlias(sourceEndpoint, {"rail_port_id", "railPortId"},
                          juce::String()));
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
  obj->setProperty("control_state", controlStateToJson(doc.controlState));

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
  obj->setProperty("max_incoming_connections", port.maxIncomingConnections);
  obj->setProperty("max_outgoing_connections", port.maxOutgoingConnections);
  return juce::var(obj);
}

juce::var TSerializer::connectionToJson(const TConnection &conn) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("id", (int64_t)conn.connectionId);

  auto writeEndpoint = [](const TEndpoint &endpoint) {
    auto *endpointObj = new juce::DynamicObject();
    endpointObj->setProperty("owner_kind",
                             endpoint.isRailPort() ? "rail" : "node");
    endpointObj->setProperty("node_id", (int64_t)endpoint.nodeId);
    endpointObj->setProperty("port_id", (int64_t)endpoint.portId);
    endpointObj->setProperty("rail_endpoint_id", endpoint.railEndpointId);
    endpointObj->setProperty("rail_port_id", endpoint.railPortId);
    return juce::var(endpointObj);
  };

  obj->setProperty("from", writeEndpoint(conn.from));
  obj->setProperty("to", writeEndpoint(conn.to));

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
  doc.controlState = {};

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

  jsonToControlState(
      doc.controlState,
      migratedJson.getProperty("control_state", juce::var()));
  normalizeLegacyRailBridgeNodes(doc, migrationReport);
  doc.controlState.reconcileDeviceProfilesAndSources();

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
  port.maxIncomingConnections =
      (int)json.getProperty("max_incoming_connections", 1);
  port.maxOutgoingConnections =
      (int)json.getProperty("max_outgoing_connections", -1);

  return true;
}

bool TSerializer::jsonToConnection(TConnection &conn, const juce::var &json) {
  if (!json.isObject())
    return false;

  conn = {};
  conn.connectionId = (ConnectionId)(int64_t)json.getProperty("id", 0);

  auto readEndpoint = [](TEndpoint &endpoint, const juce::var &endpointVar) {
    auto *endpointObj = endpointVar.getDynamicObject();
    if (endpointObj == nullptr)
      return;

    const auto ownerKind =
        propertyOrAlias(endpointObj, {"owner_kind", "ownerKind"}, "node")
            .toString()
            .trim()
            .toLowerCase();

    endpoint.nodeId =
        (NodeId)(int64_t)propertyOrAlias(endpointObj, {"node_id", "nodeId"}, 0);
    endpoint.portId =
        (PortId)(int64_t)propertyOrAlias(endpointObj, {"port_id", "portId"}, 0);
    endpoint.railEndpointId = propertyOrAlias(
        endpointObj, {"rail_endpoint_id", "railEndpointId", "endpoint_id",
                      "endpointId"},
        juce::String())
                                  .toString();
    endpoint.railPortId =
        propertyOrAlias(endpointObj, {"rail_port_id", "railPortId"},
                        juce::String())
            .toString();

    endpoint.ownerKind =
        ownerKind == "rail" ? TEndpointOwnerKind::RailPort
                              : TEndpointOwnerKind::NodePort;

    if (endpoint.ownerKind == TEndpointOwnerKind::NodePort &&
        endpoint.nodeId == kInvalidNodeId && endpoint.portId == kInvalidPortId &&
        endpoint.railEndpointId.isNotEmpty() && endpoint.railPortId.isNotEmpty()) {
      endpoint.ownerKind = TEndpointOwnerKind::RailPort;
    }
  };

  readEndpoint(conn.from, json.getProperty("from", juce::var()));
  readEndpoint(conn.to, json.getProperty("to", juce::var()));

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
