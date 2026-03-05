#pragma once

#include "Gyeol/Editor/Canvas/CanvasComponent.h"
#include "Gyeol/Editor/Panels/HistoryPanel.h"
#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>

namespace Gyeol::Shell
{
    class EditorHistoryBridge
    {
    public:
        struct State
        {
            bool suppressNextCanvasMutationHistory = false;
        };

        struct Context
        {
            DocumentHandle& document;
            Ui::Canvas::CanvasComponent& canvas;
            Ui::Panels::HistoryPanel& historyPanel;
        };

        void appendHistoryEntry(const Context& context,
                                const juce::String& action,
                                const juce::String& detail) const;

        bool performUndoFromToolbar(State& state, const Context& context) const;
        bool performRedoFromToolbar(State& state, const Context& context) const;
        bool performUndoFromHistoryPanel(State& state, const Context& context) const;
        bool performRedoFromHistoryPanel(State& state, const Context& context) const;
    };
}
