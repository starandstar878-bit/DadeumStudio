#include "Teul/Editor/Panels/Property/ParamValueFormatter.h"

namespace Teul {
namespace {

static juce::var parseEditedValue(const juce::String &text,
                                  const juce::var &prototype) {
  if (prototype.isBool()) {
    const juce::String lowered = text.trim().toLowerCase();
    return lowered == "1" || lowered == "true" || lowered == "yes" ||
           lowered == "on";
  }

  if (prototype.isInt())
    return text.getIntValue();

  if (prototype.isInt64())
    return static_cast<juce::int64>(text.getLargeIntValue());

  if (prototype.isDouble())
    return text.getDoubleValue();

  return text;
}

} // namespace

bool varEquals(const juce::var &lhs, const juce::var &rhs) {
  return lhs.toString() == rhs.toString() && lhs.isBool() == rhs.isBool() &&
         lhs.isInt() == rhs.isInt() && lhs.isInt64() == rhs.isInt64() &&
         lhs.isDouble() == rhs.isDouble();
}

TParamValueType inferValueType(const juce::var &value) {
  if (value.isBool())
    return TParamValueType::Bool;
  if (value.isInt() || value.isInt64())
    return TParamValueType::Int;
  if (value.isString())
    return TParamValueType::String;
  return TParamValueType::Float;
}

bool isNumericValue(const juce::var &value) {
  return value.isBool() || value.isInt() || value.isInt64() ||
         value.isDouble();
}

double numericValue(const juce::var &value) {
  if (value.isBool())
    return (bool)value ? 1.0 : 0.0;
  if (value.isInt())
    return (double)(int)value;
  if (value.isInt64())
    return (double)(juce::int64)value;
  if (value.isDouble())
    return (double)value;
  if (value.isString())
    return value.toString().getDoubleValue();
  return 0.0;
}

bool hasNumericRange(const TParamSpec &spec) {
  return isNumericValue(spec.minValue) && isNumericValue(spec.maxValue);
}

double sliderIntervalFor(const TParamSpec &spec) {
  if (isNumericValue(spec.step) && numericValue(spec.step) > 0.0)
    return numericValue(spec.step);

  if (spec.valueType == TParamValueType::Int ||
      spec.valueType == TParamValueType::Bool ||
      spec.valueType == TParamValueType::Enum || spec.isDiscrete)
    return 1.0;

  return 0.0;
}

TParamSpec makeFallbackParamSpec(const juce::String &key,
                                 const juce::var &value) {
  TParamSpec spec;
  spec.key = key;
  spec.label = key;
  spec.defaultValue = value;
  spec.valueType = inferValueType(value);
  spec.preferredWidget = spec.valueType == TParamValueType::Bool
                             ? TParamWidgetHint::Toggle
                             : TParamWidgetHint::Text;
  spec.showInPropertyPanel = true;
  spec.preferredBindingKey = key;
  spec.exportSymbol = key;
  return spec;
}

TParamSpec makeSpecFromExposedParam(const TTeulExposedParam &param) {
  TParamSpec spec;
  spec.key = param.paramKey;
  spec.label = param.paramLabel;
  spec.defaultValue = param.defaultValue;
  spec.valueType = param.valueType;
  spec.minValue = param.minValue;
  spec.maxValue = param.maxValue;
  spec.step = param.step;
  spec.unitLabel = param.unitLabel;
  spec.displayPrecision = param.displayPrecision;
  spec.group = param.group;
  spec.description = param.description;
  spec.enumOptions = param.enumOptions;
  spec.preferredWidget = param.preferredWidget;
  spec.showInNodeBody = param.showInNodeBody;
  spec.showInPropertyPanel = param.showInPropertyPanel;
  spec.isReadOnly = param.isReadOnly;
  spec.isAutomatable = param.isAutomatable;
  spec.isModulatable = param.isModulatable;
  spec.isDiscrete = param.isDiscrete;
  spec.exposeToIeum = param.exposeToIeum;
  spec.preferredBindingKey = param.preferredBindingKey;
  spec.exportSymbol = param.exportSymbol;
  spec.categoryPath = param.categoryPath;
  return spec;
}

juce::String formatValueForDisplay(const juce::var &value,
                                   const TParamSpec &spec) {
  if (value.isVoid())
    return "-";

  switch (spec.valueType) {
  case TParamValueType::Bool:
    return (bool)value ? "On" : "Off";
  case TParamValueType::Enum:
    for (const auto &option : spec.enumOptions) {
      if (varEquals(option.value, value))
        return option.label.isNotEmpty() ? option.label : option.id;
    }
    return value.toString();
  case TParamValueType::Int: {
    juce::String text((int)juce::roundToInt(numericValue(value)));
    if (spec.unitLabel.isNotEmpty())
      text << " " << spec.unitLabel;
    return text;
  }
  case TParamValueType::Float: {
    juce::String text(numericValue(value),
                      juce::jlimit(0, 6, spec.displayPrecision));
    if (spec.unitLabel.isNotEmpty())
      text << " " << spec.unitLabel;
    return text;
  }
  case TParamValueType::String:
  case TParamValueType::Auto:
    break;
  }

  return value.toString();
}

juce::var parseTextValue(const juce::String &text,
                         const TParamSpec &spec,
                         const juce::var &prototype) {
  if (spec.valueType == TParamValueType::Bool)
    return parseEditedValue(text, prototype.isVoid() ? juce::var(false)
                                                     : prototype);

  if (spec.valueType == TParamValueType::Int)
    return juce::roundToInt(text.getDoubleValue());

  if (spec.valueType == TParamValueType::Float)
    return text.getDoubleValue();

  if (spec.valueType == TParamValueType::Enum) {
    for (const auto &option : spec.enumOptions) {
      if (option.id.equalsIgnoreCase(text) ||
          option.label.equalsIgnoreCase(text) ||
          option.value.toString().equalsIgnoreCase(text)) {
        return option.value;
      }
    }
  }

  return parseEditedValue(text, prototype);
}

} // namespace Teul
