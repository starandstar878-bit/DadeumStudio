#include "Gyeol/Public/DocumentHandle.h"

#include "Gyeol/Core/DocumentStore.h"
#include "Gyeol/Serialization/DocumentJson.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace Gyeol
{
    namespace
    {
        const WidgetModel* findWidgetInDocument(const DocumentModel& document, const WidgetId& id) noexcept
        {
            const auto it = std::find_if(document.widgets.begin(),
                                         document.widgets.end(),
                                         [&id](const WidgetModel& widget)
                                         {
                                             return widget.id == id;
                                         });

            if (it == document.widgets.end())
                return nullptr;

            return &(*it);
        }
    }

    class DocumentHandle::Impl
    {
    public:
        struct Snapshot
        {
            DocumentModel document;
            EditorStateModel editorState;
        };

        Core::DocumentStore store;
        EditorStateModel editorState;
        std::vector<Snapshot> undoStack;
        std::vector<Snapshot> redoStack;
        size_t maxHistory = 256;

        bool hasWidget(const WidgetId& id) const noexcept
        {
            return findWidgetInDocument(store.snapshot(), id) != nullptr;
        }

        Snapshot snapshot() const
        {
            return { store.snapshot(), editorState };
        }

        void restore(Snapshot state)
        {
            store.reset(std::move(state.document));
            editorState = std::move(state.editorState);
        }

        void pushUndoState(Snapshot snapshotState)
        {
            undoStack.push_back(std::move(snapshotState));
            if (undoStack.size() > maxHistory)
                undoStack.erase(undoStack.begin());
        }

        void clearRedo()
        {
            redoStack.clear();
        }

        bool commitDocumentAction(const Action& action,
                                  std::vector<WidgetId>* createdIdsOut = nullptr)
        {
            auto previous = snapshot();

            const auto result = store.apply(action, createdIdsOut, false);
            if (result.failed())
                return false;

            pushUndoState(std::move(previous));
            clearRedo();
            return true;
        }
    };

    DocumentHandle::DocumentHandle()
        : impl(std::make_unique<Impl>())
    {
    }

    DocumentHandle::~DocumentHandle() = default;
    DocumentHandle::DocumentHandle(DocumentHandle&&) noexcept = default;
    DocumentHandle& DocumentHandle::operator=(DocumentHandle&&) noexcept = default;

    const DocumentModel& DocumentHandle::snapshot() const noexcept
    {
        return impl->store.snapshot();
    }

    const EditorStateModel& DocumentHandle::editorState() const noexcept
    {
        return impl->editorState;
    }

    WidgetId DocumentHandle::addWidget(WidgetType type,
                                       juce::Rectangle<float> bounds,
                                       const PropertyBag& properties)
    {
        CreateWidgetAction action;
        action.type = type;
        action.bounds = bounds;
        action.properties = properties;

        std::vector<WidgetId> createdIds;
        if (!impl->commitDocumentAction(action, &createdIds))
            return 0;

        return createdIds.size() == 1 ? createdIds.front() : 0;
    }

    bool DocumentHandle::removeWidget(const WidgetId& id)
    {
        DeleteWidgetsAction action;
        action.ids = { id };

        if (!impl->commitDocumentAction(action, nullptr))
            return false;

        impl->editorState.selection.erase(std::remove(impl->editorState.selection.begin(),
                                                      impl->editorState.selection.end(),
                                                      id),
                                          impl->editorState.selection.end());
        return true;
    }

    bool DocumentHandle::moveWidget(const WidgetId& id, juce::Point<float> delta)
    {
        if (!std::isfinite(delta.x) || !std::isfinite(delta.y))
            return false;

        const auto* before = findWidgetInDocument(impl->store.snapshot(), id);
        if (before == nullptr)
            return false;

        return setWidgetBounds(id, before->bounds.translated(delta.x, delta.y));
    }

    bool DocumentHandle::setWidgetBounds(const WidgetId& id, juce::Rectangle<float> bounds)
    {
        if (!std::isfinite(bounds.getX())
            || !std::isfinite(bounds.getY())
            || !std::isfinite(bounds.getWidth())
            || !std::isfinite(bounds.getHeight())
            || bounds.getWidth() < 0.0f
            || bounds.getHeight() < 0.0f)
        {
            return false;
        }

        const auto* before = findWidgetInDocument(impl->store.snapshot(), id);
        if (before == nullptr || before->bounds == bounds)
            return false;

        SetWidgetPropsAction action;
        action.ids = { id };
        action.bounds = bounds;
        return impl->commitDocumentAction(action, nullptr);
    }

    void DocumentHandle::selectSingle(const WidgetId& id)
    {
        setSelection({ id });
    }

    void DocumentHandle::setSelection(std::vector<WidgetId> selection)
    {
        std::vector<WidgetId> normalized;
        normalized.reserve(selection.size());

        for (const auto id : selection)
        {
            if (!impl->hasWidget(id))
                continue;

            if (std::find(normalized.begin(), normalized.end(), id) == normalized.end())
                normalized.push_back(id);
        }

        if (normalized == impl->editorState.selection)
            return;

        impl->pushUndoState(impl->snapshot());
        impl->editorState.selection = std::move(normalized);
        impl->clearRedo();
    }

    void DocumentHandle::clearSelection()
    {
        setSelection({});
    }

    bool DocumentHandle::canUndo() const noexcept
    {
        return !impl->undoStack.empty();
    }

    bool DocumentHandle::canRedo() const noexcept
    {
        return !impl->redoStack.empty();
    }

    bool DocumentHandle::undo()
    {
        if (impl->undoStack.empty())
            return false;

        impl->redoStack.push_back(impl->snapshot());
        auto previous = std::move(impl->undoStack.back());
        impl->undoStack.pop_back();
        impl->restore(std::move(previous));
        return true;
    }

    bool DocumentHandle::redo()
    {
        if (impl->redoStack.empty())
            return false;

        impl->pushUndoState(impl->snapshot());
        auto next = std::move(impl->redoStack.back());
        impl->redoStack.pop_back();
        impl->restore(std::move(next));
        return true;
    }

    juce::Result DocumentHandle::saveToFile(const juce::File& file) const
    {
        return Serialization::saveDocumentToFile(file, snapshot(), editorState());
    }

    juce::Result DocumentHandle::loadFromFile(const juce::File& file)
    {
        DocumentModel loadedDocument;
        EditorStateModel loadedEditorState;
        const auto result = Serialization::loadDocumentFromFile(file, loadedDocument, loadedEditorState);
        if (result.failed())
            return result;

        impl->store.reset(std::move(loadedDocument));
        impl->editorState = std::move(loadedEditorState);
        impl->undoStack.clear();
        impl->redoStack.clear();
        return juce::Result::ok();
    }
}
