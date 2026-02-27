#pragma once

#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Ui::Interaction
{
    struct SnapSettings
    {
        bool enableGrid = true;
        float gridSize = 8.0f;
        bool enableWidgetGuides = false;
        float tolerance = 4.0f;
    };

    struct SnapRequest
    {
        juce::Rectangle<float> proposedBounds;
        std::vector<juce::Rectangle<float>> nearbyBounds;
        SnapSettings settings;
    };

    struct SnapResult
    {
        juce::Rectangle<float> snappedBounds;
        bool snappedX = false;
        bool snappedY = false;
    };

    class SnapEngine
    {
    public:
        SnapResult compute(const SnapRequest& request) const;
    };
}
