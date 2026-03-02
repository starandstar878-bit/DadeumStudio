#pragma once

#include <JuceHeader.h>

class GyeolExportedComponent : public juce::Component
{
public:
    GyeolExportedComponent();
    ~GyeolExportedComponent() override = default;

    void resized() override;

private:
    juce::TextButton button_2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GyeolExportedComponent)
};
