#pragma once



#include "Teul/Registry/TNodeRegistry.h"



#include <JuceHeader.h>

#include <memory>



namespace Teul {



std::unique_ptr<juce::Component> createParamEditor(const TParamSpec &spec,

                                                   const juce::var &value);

int editorHeightFor(const juce::Component &editor);

juce::var readEditorValue(const juce::Component &editor,

                          const TParamSpec &spec,

                          const juce::var &originalValue);



} // namespace Teul

