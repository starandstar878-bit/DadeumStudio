#include "Gyeol/Editor/Panels/GridSnapPanel.h"

#include <array>

namespace Gyeol::Ui::Panels
{
    GridSnapPanel::GridSnapPanel()
    {
        addAndMakeVisible(snapEnabledToggle);
        addAndMakeVisible(gridToggle);
        addAndMakeVisible(gridSnapToggle);
        addAndMakeVisible(smartSnapToggle);
        addAndMakeVisible(gridSizeLabel);
        addAndMakeVisible(gridSizeSlider);
        addAndMakeVisible(toleranceLabel);
        addAndMakeVisible(toleranceSlider);

        for (auto* toggle : { &snapEnabledToggle, &gridToggle, &gridSnapToggle, &smartSnapToggle })
            toggle->setClickingTogglesState(true);

        gridSizeLabel.setText("Grid", juce::dontSendNotification);
        gridSizeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(178, 186, 200));
        gridSizeLabel.setJustificationType(juce::Justification::centredLeft);

        toleranceLabel.setText("Tol", juce::dontSendNotification);
        toleranceLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(178, 186, 200));
        toleranceLabel.setJustificationType(juce::Justification::centredLeft);

        auto configureSlider = [](juce::Slider& slider, double min, double max, double step)
        {
            slider.setSliderStyle(juce::Slider::LinearBar);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 20);
            slider.setRange(min, max, step);
        };

        configureSlider(gridSizeSlider, 2.0, 96.0, 1.0);
        configureSlider(toleranceSlider, 1.0, 24.0, 1.0);

        const auto onUiChanged = [this]
        {
            applySettingsFromUi();
        };

        snapEnabledToggle.onClick = onUiChanged;
        gridToggle.onClick = onUiChanged;
        gridSnapToggle.onClick = onUiChanged;
        smartSnapToggle.onClick = onUiChanged;
        gridSizeSlider.onValueChange = onUiChanged;
        toleranceSlider.onValueChange = onUiChanged;

        syncUiFromSettings();
    }

    void GridSnapPanel::setSettings(const Interaction::SnapSettings& settingsIn)
    {
        snapSettings = settingsIn;
        syncUiFromSettings();
        repaint();
    }

    const Interaction::SnapSettings& GridSnapPanel::settings() const noexcept
    {
        return snapSettings;
    }

    void GridSnapPanel::setSettingsChangedCallback(std::function<void(const Interaction::SnapSettings&)> callback)
    {
        onSettingsChanged = std::move(callback);
    }

    void GridSnapPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(40, 46, 56));
        g.drawRect(getLocalBounds(), 1);
        g.setColour(juce::Colour::fromRGB(184, 189, 200));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Grid / Snap", getLocalBounds().reduced(8), juce::Justification::topLeft, true);
    }

    void GridSnapPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22);

        const auto toggleStartX = area.getX();
        auto toggleRow = area.removeFromTop(24);
        const auto nextToggleRow = [&area, &toggleRow]()
        {
            area.removeFromTop(4);
            toggleRow = area.removeFromTop(24);
        };
        const auto toggleWidth = [](const juce::ToggleButton& toggle)
        {
            const auto text = toggle.getButtonText();
            if (text == "Snap")
                return 86;
            if (text == "Grid View")
                return 112;
            if (text == "Grid Snap")
                return 112;
            if (text == "Smart Snap")
                return 120;
            return 110;
        };
        const auto placeToggle = [&](juce::ToggleButton& toggle)
        {
            auto width = toggleWidth(toggle);
            const auto rowUsed = toggleRow.getX() > toggleStartX;
            if (rowUsed && toggleRow.getWidth() < width)
                nextToggleRow();

            if (toggleRow.getWidth() < width)
                width = toggleRow.getWidth();

            toggle.setBounds(toggleRow.removeFromLeft(width));
            toggleRow.removeFromLeft(8);
        };

        for (auto* toggle : std::array<juce::ToggleButton*, 4> { &snapEnabledToggle, &gridToggle, &gridSnapToggle, &smartSnapToggle })
            placeToggle(*toggle);

        area.removeFromTop(6);
        constexpr int kLabelWidth = 48;

        auto row2 = area.removeFromTop(24);
        gridSizeLabel.setBounds(row2.removeFromLeft(kLabelWidth));
        gridSizeSlider.setBounds(row2);

        area.removeFromTop(3);
        auto row3 = area.removeFromTop(24);
        toleranceLabel.setBounds(row3.removeFromLeft(kLabelWidth));
        toleranceSlider.setBounds(row3);
    }

    void GridSnapPanel::syncUiFromSettings()
    {
        snapEnabledToggle.setToggleState(snapSettings.snapEnabled, juce::dontSendNotification);
        gridToggle.setToggleState(snapSettings.enableGrid, juce::dontSendNotification);
        gridSnapToggle.setToggleState(snapSettings.enableGridSnap, juce::dontSendNotification);
        smartSnapToggle.setToggleState(snapSettings.enableSmartSnap, juce::dontSendNotification);
        gridSizeSlider.setValue(snapSettings.gridSize, juce::dontSendNotification);
        toleranceSlider.setValue(snapSettings.tolerance, juce::dontSendNotification);
    }

    void GridSnapPanel::applySettingsFromUi()
    {
        snapSettings.snapEnabled = snapEnabledToggle.getToggleState();
        snapSettings.enableGrid = gridToggle.getToggleState();
        snapSettings.enableGridSnap = gridSnapToggle.getToggleState();
        snapSettings.enableSmartSnap = smartSnapToggle.getToggleState();
        snapSettings.gridSize = static_cast<float>(gridSizeSlider.getValue());
        snapSettings.tolerance = static_cast<float>(toleranceSlider.getValue());
        notifySettingsChanged();
        repaint();
    }

    void GridSnapPanel::notifySettingsChanged() const
    {
        if (onSettingsChanged != nullptr)
            onSettingsChanged(snapSettings);
    }
}
