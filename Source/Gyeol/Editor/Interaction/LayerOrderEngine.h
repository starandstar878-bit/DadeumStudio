#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <optional>
#include <vector>

namespace Gyeol::Ui::Interaction
{
    enum class LayerMoveCommand
    {
        bringForward,
        sendBackward,
        bringToFront,
        sendToBack
    };

    enum class LayerDropPlacement
    {
        before,
        after,
        into
    };

    struct LayerTreeDropRequest
    {
        std::vector<LayerNodeRef> dragged;
        std::optional<LayerNodeRef> target;
        LayerDropPlacement placement = LayerDropPlacement::before;
        WidgetId parentId = kRootId;
        int insertIndex = -1; // back-to-front order index
    };

    class LayerOrderEngine
    {
    public:
        juce::Result moveSelection(DocumentHandle& document, LayerMoveCommand command);
        juce::Result reorder(DocumentHandle& document,
                             const std::vector<WidgetId>& ids,
                             WidgetId parentId,
                             int insertIndex);
        juce::Result applyTreeDrop(DocumentHandle& document, const LayerTreeDropRequest& request);
    };
}
