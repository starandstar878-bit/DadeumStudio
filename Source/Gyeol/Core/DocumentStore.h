#pragma once

#include "Gyeol/Core/Reducer.h"
#include <algorithm>
#include <vector>

namespace Gyeol::Core
{
    class DocumentStore
    {
    public:
        DocumentStore() = default;
        explicit DocumentStore(DocumentModel initialDocument)
            : documentState(std::move(initialDocument))
        {
        }

        const DocumentModel& snapshot() const noexcept
        {
            return documentState;
        }

        juce::Result apply(const Action& action,
                           std::vector<WidgetId>* createdIdsOut = nullptr,
                           bool recordHistory = true)
        {
            DocumentModel next = documentState;
            std::vector<WidgetId> createdIds;

            const auto result = Reducer::apply(next, action, &createdIds);
            if (result.failed())
                return result;

            if (recordHistory)
                pushUndoState();

            documentState = std::move(next);
            if (recordHistory)
                redoStack.clear();

            if (createdIdsOut != nullptr)
                *createdIdsOut = std::move(createdIds);

            return juce::Result::ok();
        }

        bool canUndo() const noexcept
        {
            return !undoStack.empty();
        }

        bool canRedo() const noexcept
        {
            return !redoStack.empty();
        }

        bool undo()
        {
            if (!canUndo())
                return false;

            redoStack.push_back(documentState);
            documentState = std::move(undoStack.back());
            undoStack.pop_back();
            return true;
        }

        bool redo()
        {
            if (!canRedo())
                return false;

            undoStack.push_back(documentState);
            trimUndoHistory();

            documentState = std::move(redoStack.back());
            redoStack.pop_back();
            return true;
        }

        void reset(DocumentModel document)
        {
            documentState = std::move(document);
            clearHistory();
        }

        void clearHistory()
        {
            undoStack.clear();
            redoStack.clear();
        }

        void setHistoryLimit(size_t limit) noexcept
        {
            historyLimit = std::max<size_t>(1, limit);
            trimUndoHistory();
        }

        size_t getHistoryLimit() const noexcept
        {
            return historyLimit;
        }

        size_t undoDepth() const noexcept
        {
            return undoStack.size();
        }

        size_t redoDepth() const noexcept
        {
            return redoStack.size();
        }

    private:
        void pushUndoState()
        {
            undoStack.push_back(documentState);
            trimUndoHistory();
        }

        void trimUndoHistory()
        {
            if (undoStack.size() <= historyLimit)
                return;

            const auto overflow = undoStack.size() - historyLimit;
            undoStack.erase(undoStack.begin(), undoStack.begin() + static_cast<std::ptrdiff_t>(overflow));
        }

        DocumentModel documentState;
        std::vector<DocumentModel> undoStack;
        std::vector<DocumentModel> redoStack;
        size_t historyLimit = 256;
    };
}
