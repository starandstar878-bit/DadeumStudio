#pragma once

#include <JuceHeader.h>

class GyeolExportedComponent : public juce::Component
{
public:
    GyeolExportedComponent();
    ~GyeolExportedComponent() override = default;

    void resized() override;

private:
    juce::Label label_1;
    juce::Label label_2;
    juce::Label label_3;
    juce::Label label_4;
    juce::Label label_5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GyeolExportedComponent)
};
