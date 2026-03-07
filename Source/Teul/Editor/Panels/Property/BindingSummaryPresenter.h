#pragma once

#include "Teul/Public/EditorHandle.h"
#include "Teul/Registry/TNodeRegistry.h"

#include <JuceHeader.h>

namespace Teul {

struct GyeolBindingPresentation {
  juce::String text;
  juce::Colour colour;
};

juce::String makeIeumBindingText(const TParamSpec &spec);
GyeolBindingPresentation makeGyeolBindingPresentation(
    const TParamSpec &spec, const juce::String &paramId,
    ParamBindingSummaryResolver bindingSummaryResolver);

} // namespace Teul
