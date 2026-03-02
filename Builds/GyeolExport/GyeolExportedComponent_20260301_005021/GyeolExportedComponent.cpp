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
    // Widget id=2, type=button, target=juce::TextButton, codegen=juce_text_button
    button_2.setButtonText("Button");
    addAndMakeVisible(button_2);

}

void GyeolExportedComponent::resized()
{
    button_2.setBounds(506, 476, 96, 30);
}
