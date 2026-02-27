#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Ui::Interaction
{
    enum class AlignEdge
    {
        left,
        right,
        top,
        bottom,
        hCenter,
        vCenter
    };

    enum class AlignTarget
    {
        selectionBounds,
        externalBounds
    };

    enum class DistributeAxis
    {
        horizontal,
        vertical
    };

    struct AlignOptions
    {
        AlignTarget target = AlignTarget::selectionBounds;
        juce::Rectangle<float> externalBounds;
    };

    struct BoundsPatch
    {
        WidgetId id = kRootId;
        juce::Rectangle<float> bounds;
    };

    class AlignDistributeEngine
    {
    public:
        std::vector<BoundsPatch> computeAlignPatches(const std::vector<WidgetModel>& widgets,
                                                     AlignEdge edge,
                                                     const AlignOptions& options) const;
        std::vector<BoundsPatch> computeDistributePatches(const std::vector<WidgetModel>& widgets,
                                                          DistributeAxis axis) const;
        juce::Result applyBoundsPatches(DocumentHandle& document, const std::vector<BoundsPatch>& patches) const;
    };
}
