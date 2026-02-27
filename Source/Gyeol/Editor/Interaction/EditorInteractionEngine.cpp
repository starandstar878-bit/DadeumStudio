#include "Gyeol/Editor/Interaction/EditorInteractionEngine.h"

#include "Gyeol/Editor/Interaction/AlignDistributeEngine.h"
#include "Gyeol/Editor/Interaction/LayerOrderEngine.h"
#include "Gyeol/Editor/Interaction/SelectionEngine.h"
#include "Gyeol/Editor/Interaction/SnapEngine.h"

namespace Gyeol::Ui::Interaction
{
    EditorInteractionEngine::EditorInteractionEngine(SelectionEngine& selectionEngineIn,
                                                     LayerOrderEngine& layerOrderEngineIn,
                                                     AlignDistributeEngine& alignDistributeEngineIn,
                                                     SnapEngine& snapEngineIn) noexcept
        : selectionEngine(selectionEngineIn),
          layerOrderEngine(layerOrderEngineIn),
          alignDistributeEngine(alignDistributeEngineIn),
          snapEngine(snapEngineIn)
    {
    }

    void EditorInteractionEngine::beginFrame(juce::Rectangle<float> canvasBounds) noexcept
    {
        currentCanvasBounds = canvasBounds;
    }

    juce::Result EditorInteractionEngine::handleMouseDown(DocumentHandle& document,
                                                          WidgetId targetId,
                                                          const juce::MouseEvent& event)
    {
        (void) layerOrderEngine;
        (void) alignDistributeEngine;
        (void) snapEngine;
        (void) currentCanvasBounds;

        if (!event.mods.isLeftButtonDown())
            return juce::Result::ok();

        if (targetId > kRootId)
            return selectionEngine.selectSingle(document, targetId);

        return selectionEngine.clear(document);
    }

    juce::Result EditorInteractionEngine::handleMouseDrag(DocumentHandle& document,
                                                          WidgetId targetId,
                                                          const juce::MouseEvent& event)
    {
        (void) document;
        (void) targetId;
        (void) event;
        return juce::Result::ok();
    }

    juce::Result EditorInteractionEngine::handleMouseUp(DocumentHandle& document,
                                                        WidgetId targetId,
                                                        const juce::MouseEvent& event)
    {
        (void) document;
        (void) targetId;
        (void) event;
        return juce::Result::ok();
    }
}
