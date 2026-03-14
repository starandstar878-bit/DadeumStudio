#include "IeumSerializer.h"

namespace Ieum {

namespace {
int currentSchemaVersion() { return 110; } // v1.1.0

juce::String modeToString(IeumBindingMode mode) {
  switch (mode) {
  case IeumBindingMode::Continuous:
    return "continuous";
  case IeumBindingMode::Toggle:
    return "toggle";
  case IeumBindingMode::Trigger:
    return "trigger";
  case IeumBindingMode::Relative:
    return "relative";
  case IeumBindingMode::Momentary:
    return "momentary";
  }
  return "continuous";
}

IeumBindingMode stringToMode(const juce::String &s) {
  if (s == "toggle")
    return IeumBindingMode::Toggle;
  if (s == "trigger")
    return IeumBindingMode::Trigger;
  if (s == "relative")
    return IeumBindingMode::Relative;
  if (s == "momentary")
    return IeumBindingMode::Momentary;
  return IeumBindingMode::Continuous;
}

juce::String typeToString(IeumValueType type) {
  switch (type) {
  case IeumValueType::Floating:
    return "floating";
  case IeumValueType::Integer:
    return "integer";
  case IeumValueType::Boolean:
    return "boolean";
  case IeumValueType::Trigger:
    return "trigger";
  case IeumValueType::Color:
    return "color";
  case IeumValueType::String:
    return "string";
  default:
    return "unknown";
  }
}

IeumValueType stringToType(const juce::String &s) {
  if (s == "floating")
    return IeumValueType::Floating;
  if (s == "integer")
    return IeumValueType::Integer;
  if (s == "boolean")
    return IeumValueType::Boolean;
  if (s == "trigger")
    return IeumValueType::Trigger;
  if (s == "color")
    return IeumValueType::Color;
  if (s == "string")
    return IeumValueType::String;
  return IeumValueType::Unknown;
}

juce::var rangeToJson(const IeumRange &range) {
  juce::DynamicObject::Ptr obj = new juce::DynamicObject();
  obj->setProperty("min", range.min);
  obj->setProperty("max", range.max);
  return juce::var(obj.get());
}

IeumRange jsonToRange(const juce::var &json) {
  if (!json.isObject())
    return {0.0, 1.0};
  return {(double)json.getProperty("min", 0.0),
          (double)json.getProperty("max", 1.0)};
}
} // namespace

juce::var IeumSerializer::toJson(const IeumDocument &doc) {
  juce::DynamicObject::Ptr obj = new juce::DynamicObject();
  obj->setProperty("schemaVersion", currentSchemaVersion());

  juce::Array<juce::var> bindingsArray;
  for (const auto &b : doc.getBindings())
    bindingsArray.add(bindingToJson(b));

  obj->setProperty("bindings", bindingsArray);
  return juce::var(obj.get());
}

bool IeumSerializer::fromJson(IeumDocument &doc, const juce::var &json) {
  if (!json.isObject())
    return false;

  doc.clear();
  const auto *bindingsArray =
      json.getProperty("bindings", juce::var()).getArray();
  if (bindingsArray != nullptr) {
    for (const auto &bVar : *bindingsArray) {
      IeumBindingSpec spec;
      if (jsonToBinding(spec, bVar))
        doc.addBinding(spec);
    }
  }

  return true;
}

juce::var IeumSerializer::bindingToJson(const IeumBindingSpec &spec) {
  juce::DynamicObject::Ptr obj = new juce::DynamicObject();
  obj->setProperty("id", spec.id);
  obj->setProperty("name", spec.name);
  obj->setProperty("enabled", spec.enabled);
  obj->setProperty("mode", modeToString(spec.mode));
  obj->setProperty("valueType", typeToString(spec.valueType));
  obj->setProperty("transformId", spec.transformId.toString());

  obj->setProperty("source", endpointToJson(spec.source));
  obj->setProperty("target", endpointToJson(spec.target));

  obj->setProperty("groupId", spec.groupId);
  obj->setProperty("sourceRange", rangeToJson(spec.sourceRange));
  obj->setProperty("targetRange", rangeToJson(spec.targetRange));

  return juce::var(obj.get());
}

juce::var IeumSerializer::endpointToJson(const IeumEndpoint &endpoint) {
  juce::DynamicObject::Ptr obj = new juce::DynamicObject();
  obj->setProperty("providerId", endpoint.providerId.toString());
  obj->setProperty("entityId", endpoint.entityId);
  obj->setProperty("paramId", endpoint.paramId);
  return juce::var(obj.get());
}

bool IeumSerializer::jsonToBinding(IeumBindingSpec &spec,
                                   const juce::var &json) {
  if (!json.isObject())
    return false;

  spec.id = json.getProperty("id", "").toString();
  spec.name = json.getProperty("name", "").toString();
  spec.enabled = json.getProperty("enabled", true);
  spec.mode = stringToMode(json.getProperty("mode", "").toString());
  spec.valueType = stringToType(json.getProperty("valueType", "").toString());
  spec.transformId = json.getProperty("transformId", "").toString();

  jsonToEndpoint(spec.source, json.getProperty("source", juce::var()));
  jsonToEndpoint(spec.target, json.getProperty("target", juce::var()));

  spec.groupId = json.getProperty("groupId", "").toString();
  spec.sourceRange = jsonToRange(json.getProperty("sourceRange", juce::var()));
  spec.targetRange = jsonToRange(json.getProperty("targetRange", juce::var()));

  return true;
}

bool IeumSerializer::jsonToEndpoint(IeumEndpoint &endpoint,
                                    const juce::var &json) {
  if (!json.isObject())
    return false;
  endpoint.providerId = json.getProperty("providerId", "").toString();
  endpoint.entityId = json.getProperty("entityId", "").toString();
  endpoint.paramId = json.getProperty("paramId", "").toString();
  return true;
}

} // namespace Ieum
