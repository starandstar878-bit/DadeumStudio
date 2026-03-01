#pragma once

#include <JuceHeader.h>
#include <optional>
#include <vector>

namespace Gyeol::Ui::Interaction
{
    enum class SnapKind
    {
        none,
        grid,
        smartAlign,
        smartSpacing
    };

    struct SmartSpacingHint
    {
        // true: horizontal equal-gap (X-axis), false: vertical equal-gap (Y-axis)
        bool horizontal = true;
        float axisPosition = 0.0f;
        float firstStart = 0.0f;
        float firstEnd = 0.0f;
        float secondStart = 0.0f;
        float secondEnd = 0.0f;
        float gap = 0.0f;
    };

    struct SnapSettings
    {
        bool snapEnabled = true;
        bool enableGridSnap = false;
        bool enableSmartSnap = true;
        bool enableGrid = true;
        float gridSize = 8.0f;
        float tolerance = 4.0f;
    };

    struct SnapRequest
    {
        juce::Rectangle<float> proposedBounds;
        std::vector<juce::Rectangle<float>> nearbyBounds;
        std::vector<float> verticalGuides;
        std::vector<float> horizontalGuides;
        SnapSettings settings;
    };

    struct SnapResult
    {
        juce::Rectangle<float> snappedBounds;
        bool snappedX = false;
        bool snappedY = false;
        SnapKind snapKindX = SnapKind::none;
        SnapKind snapKindY = SnapKind::none;
        std::optional<float> guideX;
        std::optional<float> guideY;
        std::vector<SmartSpacingHint> spacingHints;
    };

    class SnapEngine
    {
    public:
        SnapResult compute(const SnapRequest& request) const;
    };
}
