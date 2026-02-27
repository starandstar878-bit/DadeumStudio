#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>

namespace Gyeol::Ui::Interaction
{
    class SelectionEngine;
    class LayerOrderEngine;
    class AlignDistributeEngine;
    class SnapEngine;

    class EditorInteractionEngine
    {
    public:
        EditorInteractionEngine(SelectionEngine& selectionEngineIn,
                                LayerOrderEngine& layerOrderEngineIn,
                                AlignDistributeEngine& alignDistributeEngineIn,
                                SnapEngine& snapEngineIn) noexcept;

        void beginFrame(juce::Rectangle<float> canvasBounds) noexcept;
        juce::Result handleMouseDown(DocumentHandle& document, WidgetId targetId, const juce::MouseEvent& event);
        juce::Result handleMouseDrag(DocumentHandle& document, WidgetId targetId, const juce::MouseEvent& event);
        juce::Result handleMouseUp(DocumentHandle& document, WidgetId targetId, const juce::MouseEvent& event);

    private:
        SelectionEngine& selectionEngine;
        LayerOrderEngine& layerOrderEngine;
        AlignDistributeEngine& alignDistributeEngine;
        SnapEngine& snapEngine;
        juce::Rectangle<float> currentCanvasBounds;
    };
}
