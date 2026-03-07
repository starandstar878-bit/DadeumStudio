#pragma once

#include "Teul/Bridge/ITeulParamProvider.h"
#include "Teul/Registry/TNodeRegistry.h"

#include <JuceHeader.h>

namespace Teul {

bool varEquals(const juce::var &lhs, const juce::var &rhs);
TParamValueType inferValueType(const juce::var &value);
bool isNumericValue(const juce::var &value);
double numericValue(const juce::var &value);
bool hasNumericRange(const TParamSpec &spec);
double sliderIntervalFor(const TParamSpec &spec);
TParamSpec makeFallbackParamSpec(const juce::String &key,
                                 const juce::var &value);
TParamSpec makeSpecFromExposedParam(const TTeulExposedParam &param);
juce::String formatValueForDisplay(const juce::var &value,
                                   const TParamSpec &spec);
juce::var parseTextValue(const juce::String &text,
                         const TParamSpec &spec,
                         const juce::var &prototype);

} // namespace Teul
