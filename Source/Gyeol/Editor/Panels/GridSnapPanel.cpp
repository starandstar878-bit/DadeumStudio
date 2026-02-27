#include "Gyeol/Editor/Panels/GridSnapPanel.h"

namespace Gyeol::Ui::Panels
{
    GridSnapPanel::GridSnapPanel() = default;

    void GridSnapPanel::setSettings(const Interaction::SnapSettings& settingsIn)
    {
        snapSettings = settingsIn;
        notifySettingsChanged();
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
        g.setColour(juce::Colour::fromRGB(184, 189, 200));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Grid/Snap (Phase 3)", getLocalBounds().reduced(8), juce::Justification::topLeft, true);

        const auto lines = "Grid: " + juce::String(snapSettings.enableGrid ? "on" : "off")
                           + " / Size: " + juce::String(snapSettings.gridSize)
                           + " / Guides: " + juce::String(snapSettings.enableWidgetGuides ? "on" : "off");
        g.drawText(lines, getLocalBounds().reduced(8).withTrimmedTop(20), juce::Justification::topLeft, true);
    }

    void GridSnapPanel::notifySettingsChanged() const
    {
        if (onSettingsChanged != nullptr)
            onSettingsChanged(snapSettings);
    }
}
