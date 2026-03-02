#pragma once

#include <JuceHeader.h>

class GyeolExportedComponent : public juce::Component
{
public:
    GyeolExportedComponent();
    ~GyeolExportedComponent() override = default;

    void resized() override;

private:
    juce::Slider slider_2;
    juce::ComboBox combobox_3;
    juce::ComboBox combobox_4;
    juce::Slider meter_5;
    juce::Slider slider_6;
    juce::Label label_7;
    juce::TextButton button_8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GyeolExportedComponent)
};
