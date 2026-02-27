#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Ui::Interaction
{
    enum class MarqueeSelectionMode
    {
        contains,
        intersects
    };

    struct MarqueeSelectionRequest
    {
        std::vector<WidgetId> hitIds;
        MarqueeSelectionMode mode = MarqueeSelectionMode::intersects;
        bool additive = false;
        bool toggle = false;
    };

    class SelectionEngine
    {
    public:
        juce::Result selectSingle(DocumentHandle& document, WidgetId id);
        juce::Result setSelection(DocumentHandle& document, std::vector<WidgetId> ids);
        juce::Result toggleSelection(DocumentHandle& document, WidgetId id);
        juce::Result applyMarquee(DocumentHandle& document, const MarqueeSelectionRequest& request);
        juce::Result clear(DocumentHandle& document);
    };
}
