#include "TDocumentSerializer.h"

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

int TDocumentSerializer::currentSchemaVersion() noexcept { return 4; }

juce::var TDocumentSerializer::toJson(const TTeulDocument &doc) {
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

juce::var TDocumentSerializer::nodeToJson(const TNode &node) {
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

juce::var TDocumentSerializer::portToJson(const TPort &port) {
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

juce::var TDocumentSerializer::connectionToJson(const TConnection &conn) {
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

juce::var TDocumentSerializer::frameToJson(const TFrameRegion &frame) {
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

juce::var TDocumentSerializer::bookmarkToJson(const TBookmark &bookmark) {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("id", bookmark.bookmarkId);
  obj->setProperty("name", bookmark.name);
  obj->setProperty("focus_x", bookmark.focusX);
  obj->setProperty("focus_y", bookmark.focusY);
  obj->setProperty("zoom", bookmark.zoom);
  obj->setProperty("color_tag", bookmark.colorTag);
  return juce::var(obj);
}

bool TDocumentSerializer::fromJson(TTeulDocument &doc,
                           const juce::var &json,
                           TSchemaMigrationReport *migrationReportOut) {
  const auto *sourceRoot = json.getDynamicObject();

  TSchemaMigrationReport migrationReport;
  migrationReport.sourceSchemaVersion =
      (int)propertyOrAlias(sourceRoot, {"schema_version", "schemaVersion"}, 1);
  migrationReport.targetSchemaVersion = currentSchemaVersion();
  migrationReport.usedLegacyAliases = TDocumentMigration::usesLegacyDocumentAliases(json);
  migrationReport.migrated =
      migrationReport.usedLegacyAliases ||
      migrationReport.sourceSchemaVersion != migrationReport.targetSchemaVersion;

  if (migrationReport.usedLegacyAliases) {
    TDocumentMigration::appendWarning(migrationReport.warnings,
                  "Document used legacy field aliases during restore.");
  }

  if (migrationReport.sourceSchemaVersion < migrationReport.targetSchemaVersion) {
    TDocumentMigration::appendWarning(migrationReport.warnings,
                  "Document schema upgraded from v" +
                      juce::String(migrationReport.sourceSchemaVersion) + " to v" +
                      juce::String(migrationReport.targetSchemaVersion) + ".");
  } else if (migrationReport.sourceSchemaVersion >
             migrationReport.targetSchemaVersion) {
    migrationReport.degraded = true;
    TDocumentMigration::appendWarning(
        migrationReport.warnings,
        "Document schema is newer than this build supports; using best-effort restore.");
  }

  if (sourceRoot == nullptr)
    return false;

  if (!propertyOrAlias(sourceRoot, {"meta", "graphMeta"}).isObject()) {
    migrationReport.degraded = true;
    TDocumentMigration::appendWarning(migrationReport.warnings,
                  "Document meta missing; defaults were applied.");
  }

  const auto migratedJson = TDocumentMigration::migrateDocumentJson(
      json, migrationReport.sourceSchemaVersion,
      migrationReport.targetSchemaVersion, migrateControlStateJson,
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
  TDocumentMigration::normalizeLegacyRailBridgeNodes(doc, migrationReport);
  doc.controlState.reconcileDeviceProfilesAndSources();

  if (doc.getNextFrameId() <= 0) {
    int maxFrameId = 0;
    for (const auto &frame : doc.frames)
      maxFrameId = juce::jmax(maxFrameId, frame.frameId);
    doc.setNextFrameId(maxFrameId + 1);
    TDocumentMigration::appendWarning(migrationReport.warnings,
                  "Document frame id sequence was repaired from frame contents.");
  }

  if (doc.getNextBookmarkId() <= 0) {
    int maxBookmarkId = 0;
    for (const auto &bookmark : doc.bookmarks)
      maxBookmarkId = juce::jmax(maxBookmarkId, bookmark.bookmarkId);
    doc.setNextBookmarkId(maxBookmarkId + 1);
    TDocumentMigration::appendWarning(
        migrationReport.warnings,
        "Document bookmark id sequence was repaired from bookmark contents.");
  }

  if (migrationReportOut != nullptr)
    *migrationReportOut = migrationReport;

  return true;
}

bool TDocumentSerializer::jsonToNode(TNode &node, const juce::var &json) {
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

bool TDocumentSerializer::jsonToPort(TPort &port, const juce::var &json) {
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

bool TDocumentSerializer::jsonToConnection(TConnection &conn, const juce::var &json) {
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

bool TDocumentSerializer::jsonToFrame(TFrameRegion &frame, const juce::var &json) {
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

bool TDocumentSerializer::jsonToBookmark(TBookmark &bookmark, const juce::var &json) {
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
