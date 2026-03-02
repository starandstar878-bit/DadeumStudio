#include "GyeolExportedComponent.h"

namespace
{
    juce::File resolveExportAssetFile(const juce::String& relativePath)
    {
        auto baseDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
        return baseDir.getChildFile(relativePath);
    }

    juce::Image preloadExportAssetImage(const juce::String& relativePath)
    {
        const auto file = resolveExportAssetFile(relativePath);
        if (!file.existsAsFile())
            return {};
        return juce::ImageFileFormat::loadFrom(file);
    }
}

GyeolExportedComponent::GyeolExportedComponent()
{
    // Widget id=2, type=slider, target=juce::Slider::LinearHorizontal, codegen=juce_slider_dynamic
    slider_2.setSliderStyle(juce::Slider::LinearHorizontal);
    slider_2.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider_2.setRange(0.00000000, 1.00000000, 0.00000000);
    slider_2.setValue(0.50000000, juce::dontSendNotification);
    addAndMakeVisible(slider_2);

    // Widget id=3, type=comboBox, target=juce::ComboBox, codegen=juce_combo_box
    combobox_3.addItem("Item 1", 1);
    combobox_3.addItem("Item 2", 2);
    combobox_3.addItem("Item 3", 3);
    combobox_3.setEditableText(false);
    combobox_3.setTextWhenNothingSelected("Select...");
    combobox_3.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(combobox_3);

    // Widget id=4, type=comboBox, target=juce::ComboBox, codegen=juce_combo_box
    combobox_4.addItem("Item 1", 1);
    combobox_4.addItem("Item 2", 2);
    combobox_4.addItem("Item 3", 3);
    combobox_4.setEditableText(false);
    combobox_4.setTextWhenNothingSelected("Select...");
    combobox_4.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(combobox_4);

    // Widget id=5, type=meter, target=gyeol::Meter, codegen=custom_meter_slider_dynamic
    meter_5.setSliderStyle(juce::Slider::LinearVertical);
    meter_5.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    meter_5.setRange(0.00000000, 1.00000000, 0.0);
    meter_5.setValue(0.62000000, juce::dontSendNotification);
    meter_5.setEnabled(false);
    meter_5.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(meter_5);

    // Widget id=6, type=slider, target=juce::Slider::LinearHorizontal, codegen=juce_slider_dynamic
    slider_6.setSliderStyle(juce::Slider::LinearHorizontal);
    slider_6.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider_6.setRange(0.00000000, 1.00000000, 0.00000000);
    slider_6.setValue(0.50000000, juce::dontSendNotification);
    addAndMakeVisible(slider_6);

    // Widget id=7, type=label, target=juce::Label, codegen=juce_label
    label_7.setText("Label", juce::dontSendNotification);
    label_7.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label_7);

    // Widget id=8, type=button, target=juce::TextButton, codegen=juce_text_button
    button_8.setButtonText("Button");
    addAndMakeVisible(button_8);

}

void GyeolExportedComponent::resized()
{
    slider_2.setBounds(663, 354, 171, 35);
    combobox_3.setBounds(457, 377, 150, 29);
    combobox_4.setBounds(413, 237, 151, 29);
    meter_5.setBounds(307, 453, 36, 120);
    slider_6.setBounds(173, 371, 170, 35);
    label_7.setBounds(506, 476, 120, 28);
    button_8.setBounds(530, 587, 96, 30);
}
