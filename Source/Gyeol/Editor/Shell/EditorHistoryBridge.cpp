#include "Gyeol/Editor/Shell/EditorHistoryBridge.h"

namespace Gyeol::Shell
{
    void EditorHistoryBridge::appendHistoryEntry(const Context& context,
                                                 const juce::String& action,
                                                 const juce::String& detail) const
    {
        context.historyPanel.setStackState(context.document.undoDepth(),
                                           context.document.redoDepth(),
                                           context.document.historySerial());
        context.historyPanel.appendEntry(action, detail);
        context.historyPanel.setCanUndoRedo(context.document.canUndo(),
                                            context.document.canRedo());
    }

    bool EditorHistoryBridge::performUndoFromToolbar(State& state,
                                                     const Context& context) const
    {
        if (context.canvas.isRunMode())
            return false;

        state.suppressNextCanvasMutationHistory = true;
        if (context.canvas.performUndo())
        {
            appendHistoryEntry(context, "Undo", "Toolbar");
            return true;
        }

        state.suppressNextCanvasMutationHistory = false;
        return false;
    }

    bool EditorHistoryBridge::performRedoFromToolbar(State& state,
                                                     const Context& context) const
    {
        if (context.canvas.isRunMode())
            return false;

        state.suppressNextCanvasMutationHistory = true;
        if (context.canvas.performRedo())
        {
            appendHistoryEntry(context, "Redo", "Toolbar");
            return true;
        }

        state.suppressNextCanvasMutationHistory = false;
        return false;
    }

    bool EditorHistoryBridge::performUndoFromHistoryPanel(State& state,
                                                          const Context& context) const
    {
        if (context.canvas.isRunMode())
            return false;

        state.suppressNextCanvasMutationHistory = true;
        if (context.canvas.performUndo())
        {
            appendHistoryEntry(context, "Undo", "History panel");
            return true;
        }

        state.suppressNextCanvasMutationHistory = false;
        return false;
    }

    bool EditorHistoryBridge::performRedoFromHistoryPanel(State& state,
                                                          const Context& context) const
    {
        if (context.canvas.isRunMode())
            return false;

        state.suppressNextCanvasMutationHistory = true;
        if (context.canvas.performRedo())
        {
            appendHistoryEntry(context, "Redo", "History panel");
            return true;
        }

        state.suppressNextCanvasMutationHistory = false;
        return false;
    }
}
