#include "Gyeol/Editor/Panels/PropertyPanel.h"

namespace Gyeol::Ui::Panels
{
    PropertyPanel::PropertyPanel(DocumentHandle& documentIn)
        : document(documentIn)
    {
    }

    void PropertyPanel::setSelection(std::vector<WidgetId> selectionIds)
    {
        selectedIds = std::move(selectionIds);
        repaint();
    }

    void PropertyPanel::refreshFromDocument()
    {
        repaint();
    }

    void PropertyPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(184, 189, 200));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Property Panel (Phase 3)", getLocalBounds().reduced(8), juce::Justification::topLeft, true);

        const auto details = "Selected: " + juce::String(static_cast<int>(selectedIds.size()))
                             + " / Widgets: " + juce::String(static_cast<int>(document.snapshot().widgets.size()));
        g.drawText(details, getLocalBounds().reduced(8).withTrimmedTop(20), juce::Justification::topLeft, true);
    }
}
