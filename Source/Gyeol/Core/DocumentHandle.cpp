#include "Gyeol/Public/DocumentHandle.h"

#include "Gyeol/Core/DocumentStore.h"
#include "Gyeol/Serialization/DocumentJson.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_set>

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

        const GroupModel* findGroupInDocument(const DocumentModel& document, const WidgetId& id) noexcept
        {
            const auto it = std::find_if(document.groups.begin(),
                                         document.groups.end(),
                                         [&id](const GroupModel& group)
                                         {
                                             return group.id == id;
                                         });

            if (it == document.groups.end())
                return nullptr;

            return &(*it);
        }

        void collectGroupWidgetIdsRecursive(const DocumentModel& document,
                                            WidgetId groupId,
                                            std::unordered_set<WidgetId>& outWidgetIds,
                                            std::unordered_set<WidgetId>& visitedGroupIds)
        {
            if (!visitedGroupIds.insert(groupId).second)
                return;

            const auto* group = findGroupInDocument(document, groupId);
            if (group == nullptr)
                return;

            for (const auto memberId : group->memberWidgetIds)
                outWidgetIds.insert(memberId);

            for (const auto& candidate : document.groups)
            {
                if (candidate.parentGroupId.has_value() && *candidate.parentGroupId == groupId)
                    collectGroupWidgetIdsRecursive(document, candidate.id, outWidgetIds, visitedGroupIds);
            }
        }

        std::unordered_set<WidgetId> collectGroupWidgetIdsRecursive(const DocumentModel& document, WidgetId groupId)
        {
            std::unordered_set<WidgetId> widgets;
            std::unordered_set<WidgetId> visitedGroups;
            collectGroupWidgetIdsRecursive(document, groupId, widgets, visitedGroups);
            return widgets;
        }

        bool hasSelectedAncestorGroup(const DocumentModel& document,
                                      WidgetId groupId,
                                      const std::unordered_set<WidgetId>& selectedGroupIds)
        {
            const auto* group = findGroupInDocument(document, groupId);
            if (group == nullptr)
                return false;

            auto parent = group->parentGroupId;
            while (parent.has_value())
            {
                if (selectedGroupIds.count(*parent) > 0)
                    return true;

                const auto* parentGroup = findGroupInDocument(document, *parent);
                if (parentGroup == nullptr)
                    break;

                parent = parentGroup->parentGroupId;
            }

            return false;
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

        return setWidgetsBounds({ { id, before->bounds.translated(delta.x, delta.y) } });
    }

    bool DocumentHandle::setWidgetBounds(const WidgetId& id, juce::Rectangle<float> bounds)
    {
        return setWidgetsBounds({ { id, bounds } });
    }

    bool DocumentHandle::setWidgetsBounds(const std::vector<WidgetBoundsUpdate>& updates)
    {
        if (updates.empty())
            return false;

        SetWidgetsBoundsAction action;
        action.items.reserve(updates.size());

        bool hasAnyChange = false;
        std::vector<WidgetId> seenIds;
        seenIds.reserve(updates.size());

        for (const auto& update : updates)
        {
            if (update.id <= kRootId)
                return false;
            if (!std::isfinite(update.bounds.getX())
                || !std::isfinite(update.bounds.getY())
                || !std::isfinite(update.bounds.getWidth())
                || !std::isfinite(update.bounds.getHeight())
                || update.bounds.getWidth() < 0.0f
                || update.bounds.getHeight() < 0.0f)
            {
                return false;
            }

            if (std::find(seenIds.begin(), seenIds.end(), update.id) != seenIds.end())
                return false;
            seenIds.push_back(update.id);

            const auto* before = findWidgetInDocument(impl->store.snapshot(), update.id);
            if (before == nullptr)
                return false;

            if (before->bounds != update.bounds)
                hasAnyChange = true;

            SetWidgetsBoundsAction::Item item;
            item.id = update.id;
            item.bounds = update.bounds;
            action.items.push_back(item);
        }

        if (!hasAnyChange)
            return false;

        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::groupSelection()
    {
        if (impl->editorState.selection.size() < 2)
            return false;

        const auto& document = impl->store.snapshot();
        std::unordered_set<WidgetId> selectionSet(impl->editorState.selection.begin(), impl->editorState.selection.end());

        std::vector<WidgetId> candidateGroupIds;
        for (const auto& group : document.groups)
        {
            const auto groupWidgets = collectGroupWidgetIdsRecursive(document, group.id);
            if (groupWidgets.empty())
                continue;

            const auto fullyCovered = std::all_of(groupWidgets.begin(),
                                                  groupWidgets.end(),
                                                  [&selectionSet](WidgetId widgetId)
                                                  {
                                                      return selectionSet.count(widgetId) > 0;
                                                  });
            if (fullyCovered)
                candidateGroupIds.push_back(group.id);
        }

        std::unordered_set<WidgetId> candidateGroupSet(candidateGroupIds.begin(), candidateGroupIds.end());
        std::vector<WidgetId> selectedGroupIds;
        selectedGroupIds.reserve(candidateGroupIds.size());
        for (const auto groupId : candidateGroupIds)
        {
            if (!hasSelectedAncestorGroup(document, groupId, candidateGroupSet))
                selectedGroupIds.push_back(groupId);
        }

        std::unordered_set<WidgetId> widgetsCoveredBySelectedGroups;
        for (const auto groupId : selectedGroupIds)
        {
            const auto coveredWidgets = collectGroupWidgetIdsRecursive(document, groupId);
            widgetsCoveredBySelectedGroups.insert(coveredWidgets.begin(), coveredWidgets.end());
        }

        std::vector<WidgetId> explicitWidgetIds;
        explicitWidgetIds.reserve(impl->editorState.selection.size());
        for (const auto widgetId : impl->editorState.selection)
        {
            if (widgetsCoveredBySelectedGroups.count(widgetId) > 0)
                continue;
            if (std::find(explicitWidgetIds.begin(), explicitWidgetIds.end(), widgetId) == explicitWidgetIds.end())
                explicitWidgetIds.push_back(widgetId);
        }

        const auto selectedUnitCount = explicitWidgetIds.size() + selectedGroupIds.size();
        const auto allowSingleGroupWrapper = explicitWidgetIds.empty() && selectedGroupIds.size() == 1;
        if (selectedUnitCount < 2 && !allowSingleGroupWrapper)
            return false;

        GroupWidgetsAction action;
        action.widgetIds = std::move(explicitWidgetIds);
        action.groupIds = std::move(selectedGroupIds);
        action.groupName = "Group";
        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::ungroupSelection()
    {
        if (impl->editorState.selection.empty())
            return false;

        const auto& document = impl->store.snapshot();
        std::unordered_set<WidgetId> selectionSet(impl->editorState.selection.begin(), impl->editorState.selection.end());

        std::vector<WidgetId> candidateGroupIds;
        for (const auto& group : document.groups)
        {
            const auto groupWidgets = collectGroupWidgetIdsRecursive(document, group.id);
            if (groupWidgets.empty())
                continue;

            const auto fullyCovered = std::all_of(groupWidgets.begin(),
                                                  groupWidgets.end(),
                                                  [&selectionSet](WidgetId widgetId)
                                                  {
                                                      return selectionSet.count(widgetId) > 0;
                                                  });
            if (fullyCovered)
                candidateGroupIds.push_back(group.id);
        }

        std::unordered_set<WidgetId> candidateGroupSet(candidateGroupIds.begin(), candidateGroupIds.end());
        std::vector<WidgetId> groupIds;
        groupIds.reserve(candidateGroupIds.size());
        for (const auto groupId : candidateGroupIds)
        {
            if (!hasSelectedAncestorGroup(document, groupId, candidateGroupSet))
                groupIds.push_back(groupId);
        }

        if (groupIds.empty())
        {
            for (const auto id : impl->editorState.selection)
            {
                if (findGroupInDocument(document, id) != nullptr)
                    groupIds.push_back(id);
            }
        }

        if (groupIds.empty())
            return false;

        std::sort(groupIds.begin(), groupIds.end());
        groupIds.erase(std::unique(groupIds.begin(), groupIds.end()), groupIds.end());

        UngroupWidgetsAction action;
        action.groupIds = std::move(groupIds);
        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::reparent(ReparentAction action)
    {
        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::reorder(ReorderAction action)
    {
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
