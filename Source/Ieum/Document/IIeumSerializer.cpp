#include "IIeumSerializer.h"

namespace Ieum {

namespace {
    juce::String modeToString(IBindingMode mode) {
        switch (mode) {
            case IBindingMode::Continuous: return "continuous";
            case IBindingMode::Toggle:     return "toggle";
            case IBindingMode::Trigger:    return "trigger";
            case IBindingMode::Relative:   return "relative";
            case IBindingMode::Momentary:  return "momentary";
        }
        return "continuous";
    }

    IBindingMode stringToMode(const juce::String& s) {
        if (s == "toggle")    return IBindingMode::Toggle;
        if (s == "trigger")   return IBindingMode::Trigger;
        if (s == "relative")  return IBindingMode::Relative;
        if (s == "momentary") return IBindingMode::Momentary;
        return IBindingMode::Continuous;
    }

    juce::String typeToString(IValueType type) {
        switch (type) {
            case IValueType::Floating: return "floating";
            case IValueType::Integer:  return "integer";
            case IValueType::Boolean:  return "boolean";
            case IValueType::Trigger:  return "trigger";
            case IValueType::Color:    return "color";
            case IValueType::String:   return "string";
            default:                   return "unknown";
        }
    }

    IValueType stringToType(const juce::String& s) {
        if (s == "floating") return IValueType::Floating;
        if (s == "integer")  return IValueType::Integer;
        if (s == "boolean")  return IValueType::Boolean;
        if (s == "trigger")  return IValueType::Trigger;
        if (s == "color")    return IValueType::Color;
        if (s == "string")   return IValueType::String;
        return IValueType::Unknown;
    }
}

juce::var IIeumSerializer::toJson(const IIeumDocument& doc)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("schemaVersion", currentSchemaVersion());
    
    juce::Array bindingsArray;
    for (const auto& b : doc.bindings)
        bindingsArray.add(bindingToJson(b));
        
    obj->setProperty("bindings", bindingsArray);
    return juce::var(obj.get());
}

bool IIeumSerializer::fromJson(IIeumDocument& doc, const juce::var& json)
{
    if (!json.isObject()) return false;
    
    doc.bindings.clear();
    const auto* bindingsArray = json.getProperty("bindings", juce::var()).getArray();
    if (bindingsArray != nullptr)
    {
        for (const auto& bVar : *bindingsArray)
        {
            IBindingSpec spec;
            if (jsonToBinding(spec, bVar))
                doc.bindings.push_back(spec);
        }
    }
    
    return true;
}

juce::var IIeumSerializer::bindingToJson(const IBindingSpec& spec)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", spec.id);
    obj->setProperty("name", spec.name);
    obj->setProperty("enabled", spec.enabled);
    obj->setProperty("mode", modeToString(spec.mode));
    obj->setProperty("valueType", typeToString(spec.valueType));
    obj->setProperty("transformId", spec.transformId.toString());
    
    obj->setProperty("source", endpointToJson(spec.source));
    obj->setProperty("target", endpointToJson(spec.target));
    
    return juce::var(obj.get());
}

juce::var IIeumSerializer::endpointToJson(const IEndpoint& endpoint)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("providerId", endpoint.providerId.toString());
    obj->setProperty("entityId", endpoint.entityId);
    obj->setProperty("paramId", endpoint.paramId);
    return juce::var(obj.get());
}

bool IIeumSerializer::jsonToBinding(IBindingSpec& spec, const juce::var& json)
{
    if (!json.isObject()) return false;
    
    spec.id = json.getProperty("id", "").toString();
    spec.name = json.getProperty("name", "").toString();
    spec.enabled = json.getProperty("enabled", true);
    spec.mode = stringToMode(json.getProperty("mode", "").toString());
    spec.valueType = stringToType(json.getProperty("valueType", "").toString());
    spec.transformId = json.getProperty("transformId", "").toString();
    
    jsonToEndpoint(spec.source, json.getProperty("source", juce::var()));
    jsonToEndpoint(spec.target, json.getProperty("target", juce::var()));
    
    return true;
}

bool IIeumSerializer::jsonToEndpoint(IEndpoint& endpoint, const juce::var& json)
{
    if (!json.isObject()) return false;
    endpoint.providerId = json.getProperty("providerId", "").toString();
    endpoint.entityId = json.getProperty("entityId", "").toString();
    endpoint.paramId = json.getProperty("paramId", "").toString();
    return true;
}

} // namespace Ieum
