#include "Gyeol/Public/DocumentHandle.h"

#include "Gyeol/Core/Document.h"
#include "Gyeol/Serialization/DocumentJson.h"
#include <algorithm>
#include <memory>

namespace Gyeol
{
    class DocumentHandle::Impl
    {
    public:
        struct Snapshot
        {
            Core::Document document;
            EditorStateModel editorState;
        };

        Core::Document document;
        EditorStateModel editorState;
        std::vector<Snapshot> undoStack;
        std::vector<Snapshot> redoStack;
        size_t maxHistory = 256;

        bool hasWidget(const WidgetId& id) const noexcept
        {
            return document.findWidget(id) != nullptr;
        }

        Snapshot snapshot() const
        {
            return { document, editorState };
        }

        void restore(Snapshot state)
        {
            document = std::move(state.document);
            editorState = std::move(state.editorState);
        }

        void pushUndo()
        {
            undoStack.push_back(snapshot());
            if (undoStack.size() > maxHistory)
                undoStack.erase(undoStack.begin());
        }

        void clearRedo()
        {
            redoStack.clear();
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
        return impl->document.model();
    }

    const EditorStateModel& DocumentHandle::editorState() const noexcept
    {
        return impl->editorState;
    }

    WidgetId DocumentHandle::addWidget(WidgetType type,
                                       juce::Rectangle<float> bounds,
                                       const PropertyBag& properties)
    {
        const auto previousSize = impl->document.model().widgets.size();
        const auto nextDocument = impl->document.withWidgetAdded(type, bounds, properties);
        if (nextDocument.model().widgets.size() == previousSize)
            return 0;

        impl->pushUndo();
        impl->document = nextDocument;
        impl->clearRedo();
        const auto& widgets = impl->document.model().widgets;
        return widgets.empty() ? 0 : widgets.back().id;
    }

    bool DocumentHandle::removeWidget(const WidgetId& id)
    {
        const auto previousSize = impl->document.model().widgets.size();
        const auto nextDocument = impl->document.withWidgetRemoved(id);
        if (nextDocument.model().widgets.size() == previousSize)
            return false;

        impl->pushUndo();
        impl->document = nextDocument;
        impl->editorState.selection.erase(std::remove(impl->editorState.selection.begin(),
                                                      impl->editorState.selection.end(),
                                                      id),
                                          impl->editorState.selection.end());
        impl->clearRedo();
        return true;
    }

    bool DocumentHandle::moveWidget(const WidgetId& id, juce::Point<float> delta)
    {
        const auto* before = impl->document.findWidget(id);
        if (before == nullptr)
            return false;

        const auto oldBounds = before->bounds;
        const auto nextDocument = impl->document.withWidgetMoved(id, delta);
        const auto* after = nextDocument.findWidget(id);
        if (after == nullptr || after->bounds == oldBounds)
            return false;

        impl->pushUndo();
        impl->document = nextDocument;
        impl->clearRedo();
        return true;
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

        impl->pushUndo();
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

        impl->undoStack.push_back(impl->snapshot());
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

        impl->document = Core::Document(std::make_shared<const DocumentModel>(std::move(loadedDocument)));
        impl->editorState = std::move(loadedEditorState);
        impl->undoStack.clear();
        impl->redoStack.clear();
        return juce::Result::ok();
    }
}
