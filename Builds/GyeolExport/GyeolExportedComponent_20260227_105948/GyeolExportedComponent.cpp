#include "GyeolExportedComponent.h"

GyeolExportedComponent::GyeolExportedComponent()
{
    // Widget id=1, type=label, target=juce::Label, codegen=juce_label
    label_1.setText("Label", juce::dontSendNotification);
    label_1.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label_1);

    // Widget id=2, type=label, target=juce::Label, codegen=juce_label
    label_2.setText("Label", juce::dontSendNotification);
    label_2.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label_2);

    // Widget id=3, type=label, target=juce::Label, codegen=juce_label
    label_3.setText("Label", juce::dontSendNotification);
    label_3.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label_3);

    // Widget id=4, type=label, target=juce::Label, codegen=juce_label
    label_4.setText("Label", juce::dontSendNotification);
    label_4.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label_4);

    // Widget id=5, type=label, target=juce::Label, codegen=juce_label
    label_5.setText("Label", juce::dontSendNotification);
    label_5.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label_5);

}

void GyeolExportedComponent::resized()
{
    label_1.setBounds(24, 24, 120, 28);
    label_2.setBounds(44, 24, 120, 28);
    label_3.setBounds(64, 24, 120, 28);
    label_4.setBounds(84, 24, 120, 28);
    label_5.setBounds(104, 24, 120, 28);
}
