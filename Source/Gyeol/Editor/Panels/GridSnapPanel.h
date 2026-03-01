#pragma once

#include "Gyeol/Editor/Interaction/SnapEngine.h"
#include <JuceHeader.h>
#include <functional>

namespace Gyeol::Ui::Panels
{
    class GridSnapPanel : public juce::Component
    {
    public:
        GridSnapPanel();

        void setSettings(const Interaction::SnapSettings& settingsIn);
        const Interaction::SnapSettings& settings() const noexcept;

        void setSettingsChangedCallback(std::function<void(const Interaction::SnapSettings&)> callback);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void syncUiFromSettings();
        void applySettingsFromUi();
        void notifySettingsChanged() const;

        Interaction::SnapSettings snapSettings;
        std::function<void(const Interaction::SnapSettings&)> onSettingsChanged;
        juce::ToggleButton snapEnabledToggle { "Snap" };
        juce::ToggleButton gridToggle { "Grid View" };
        juce::ToggleButton gridSnapToggle { "Grid Snap" };
        juce::ToggleButton smartSnapToggle { "Smart Snap" };
        juce::Label gridSizeLabel;
        juce::Slider gridSizeSlider;
        juce::Label toleranceLabel;
        juce::Slider toleranceSlider;
    };
}
