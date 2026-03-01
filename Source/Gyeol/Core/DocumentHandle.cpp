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

        bool runtimeActionEquals(const RuntimeActionModel& lhs, const RuntimeActionModel& rhs) noexcept
        {
            return lhs.kind == rhs.kind
                && lhs.paramKey == rhs.paramKey
                && lhs.value == rhs.value
                && lhs.delta == rhs.delta
                && lhs.target.kind == rhs.target.kind
                && lhs.target.id == rhs.target.id
                && lhs.visible == rhs.visible
                && lhs.locked == rhs.locked
                && lhs.opacity == rhs.opacity
                && lhs.patch == rhs.patch
                && lhs.targetWidgetId == rhs.targetWidgetId
                && lhs.bounds == rhs.bounds;
        }

        bool runtimeBindingEquals(const RuntimeBindingModel& lhs, const RuntimeBindingModel& rhs) noexcept
        {
            if (lhs.id != rhs.id
                || lhs.name != rhs.name
                || lhs.enabled != rhs.enabled
                || lhs.sourceWidgetId != rhs.sourceWidgetId
                || lhs.eventKey != rhs.eventKey
                || lhs.actions.size() != rhs.actions.size())
            {
                return false;
            }

            for (size_t i = 0; i < lhs.actions.size(); ++i)
            {
                if (!runtimeActionEquals(lhs.actions[i], rhs.actions[i]))
                    return false;
            }

            return true;
        }

        bool runtimeBindingsEqual(const std::vector<RuntimeBindingModel>& lhs,
                                  const std::vector<RuntimeBindingModel>& rhs) noexcept
        {
            if (lhs.size() != rhs.size())
                return false;

            for (size_t i = 0; i < lhs.size(); ++i)
            {
                if (!runtimeBindingEquals(lhs[i], rhs[i]))
                    return false;
            }

            return true;
        }

        bool assetModelEquals(const AssetModel& lhs, const AssetModel& rhs) noexcept
        {
            return lhs.id == rhs.id
                && lhs.name == rhs.name
                && lhs.kind == rhs.kind
                && lhs.refKey == rhs.refKey
                && lhs.relativePath == rhs.relativePath
                && lhs.mimeType == rhs.mimeType
                && lhs.meta == rhs.meta;
        }

        bool assetsEqual(const std::vector<AssetModel>& lhs,
                         const std::vector<AssetModel>& rhs) noexcept
        {
            if (lhs.size() != rhs.size())
                return false;

            for (size_t i = 0; i < lhs.size(); ++i)
            {
                if (!assetModelEquals(lhs[i], rhs[i]))
                    return false;
            }

            return true;
        }

        bool replaceStringInVarRecursive(juce::var& value,
                                         const juce::String& oldRefKey,
                                         const juce::String& newRefKey)
        {
            if (value.isString())
            {
                if (value.toString() == oldRefKey)
                {
                    value = newRefKey;
                    return true;
                }
                return false;
            }

            if (auto* object = value.getDynamicObject(); object != nullptr)
            {
                bool changed = false;
                auto& props = object->getProperties();
                for (int i = 0; i < props.size(); ++i)
                {
                    const auto key = props.getName(i);
                    auto nested = props.getValueAt(i);
                    if (!replaceStringInVarRecursive(nested, oldRefKey, newRefKey))
                        continue;

                    object->setProperty(key, nested);
                    changed = true;
                }
                return changed;
            }

            if (auto* array = value.getArray(); array != nullptr)
            {
                bool changed = false;
                for (auto& item : *array)
                    changed = replaceStringInVarRecursive(item, oldRefKey, newRefKey) || changed;
                return changed;
            }

            return false;
        }

        bool replaceStringInPropertyBag(PropertyBag& bag,
                                        const juce::String& oldRefKey,
                                        const juce::String& newRefKey)
        {
            bool changed = false;
            for (int i = 0; i < bag.size(); ++i)
            {
                const auto key = bag.getName(i);
                auto value = bag.getValueAt(i);
                if (!replaceStringInVarRecursive(value, oldRefKey, newRefKey))
                    continue;

                bag.set(key, value);
                changed = true;
            }

            return changed;
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

        DocumentModel makeInitialDocumentModel()
        {
            DocumentModel document;
            LayerModel layer;
            layer.id = 1;
            layer.name = "Layer 1";
            layer.order = 0;
            document.layers.push_back(std::move(layer));
            return document;
        }
    }

    class DocumentHandle::Impl
    {
    public:
        Impl()
            : store(makeInitialDocumentModel())
        {
        }

        struct Snapshot
        {
            DocumentModel document;
            EditorStateModel editorState;
        };

        struct CoalescedEditState
        {
            juce::String key;
            Snapshot baseline;
            bool dirty = false;
        };

        Core::DocumentStore store;
        EditorStateModel editorState;
        std::vector<Snapshot> undoStack;
        std::vector<Snapshot> redoStack;
        uint64_t historySerial = 1;
        size_t maxHistory = 256;
        std::optional<CoalescedEditState> coalescedEdit;

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
            coalescedEdit.reset();
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

        bool beginCoalescedEdit(const juce::String& key)
        {
            const auto normalizedKey = key.trim();
            if (normalizedKey.isEmpty())
                return false;

            if (coalescedEdit.has_value())
            {
                if (coalescedEdit->key == normalizedKey)
                    return true;

                const auto currentKey = coalescedEdit->key;
                if (!endCoalescedEdit(currentKey, true))
                    return false;
            }

            CoalescedEditState state;
            state.key = normalizedKey;
            state.baseline = snapshot();
            coalescedEdit = std::move(state);
            return true;
        }

        bool previewAction(const Action& action)
        {
            if (!coalescedEdit.has_value())
                return false;

            const auto result = store.apply(action, nullptr, false);
            if (result.failed())
                return false;

            coalescedEdit->dirty = true;
            return true;
        }

        bool endCoalescedEdit(const juce::String& key, bool commit)
        {
            const auto normalizedKey = key.trim();
            if (!coalescedEdit.has_value())
                return false;
            if (coalescedEdit->key != normalizedKey)
                return false;

            auto state = std::move(*coalescedEdit);
            coalescedEdit.reset();

            if (commit)
            {
                if (state.dirty)
                {
                    pushUndoState(std::move(state.baseline));
                    clearRedo();
                    ++historySerial;
                }
                return true;
            }

            restore(std::move(state.baseline));
            return true;
        }

        bool finalizeActiveCoalescedEdit(bool commit)
        {
            if (!coalescedEdit.has_value())
                return true;

            const auto key = coalescedEdit->key;
            return endCoalescedEdit(key, commit);
        }

        bool commitDocumentAction(const Action& action,
                                  std::vector<WidgetId>* createdIdsOut = nullptr)
        {
            if (!finalizeActiveCoalescedEdit(true))
                return false;

            auto previous = snapshot();

            const auto result = store.apply(action, createdIdsOut, false);
            if (result.failed())
                return false;

            pushUndoState(std::move(previous));
            clearRedo();
            ++historySerial;
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

    WidgetId DocumentHandle::createNode(const CreateAction& action)
    {
        std::vector<WidgetId> createdIds;
        if (!impl->commitDocumentAction(action, &createdIds))
            return 0;

        return createdIds.empty() ? 0 : createdIds.front();
    }

    bool DocumentHandle::deleteNodes(const DeleteAction& action)
    {
        if (!impl->commitDocumentAction(action, nullptr))
            return false;

        // Selection stores widget ids only; keep only still-existing widgets.
        std::vector<WidgetId> filtered;
        filtered.reserve(impl->editorState.selection.size());
        for (const auto id : impl->editorState.selection)
        {
            if (impl->hasWidget(id))
                filtered.push_back(id);
        }
        impl->editorState.selection = std::move(filtered);
        return true;
    }

    bool DocumentHandle::setProps(const SetPropsAction& action)
    {
        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::setBounds(const SetBoundsAction& action)
    {
        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::beginCoalescedEdit(const juce::String& key)
    {
        return impl->beginCoalescedEdit(key);
    }

    bool DocumentHandle::previewSetProps(const SetPropsAction& action)
    {
        return impl->previewAction(action);
    }

    bool DocumentHandle::previewSetBounds(const SetBoundsAction& action)
    {
        return impl->previewAction(action);
    }

    bool DocumentHandle::endCoalescedEdit(const juce::String& key, bool commit)
    {
        return impl->endCoalescedEdit(key, commit);
    }

    WidgetId DocumentHandle::addWidget(WidgetType type,
                                       juce::Rectangle<float> bounds,
                                       const PropertyBag& properties,
                                       std::optional<WidgetId> layerId)
    {
        CreateWidgetPayload payload;
        payload.type = type;
        payload.bounds = bounds;
        payload.properties = properties;
        if (layerId.has_value())
        {
            payload.parent.kind = ParentKind::layer;
            payload.parent.id = *layerId;
        }
        else
        {
            payload.parent.kind = ParentKind::root;
            payload.parent.id = kRootId;
        }

        CreateAction action;
        action.kind = NodeKind::widget;
        action.payload = std::move(payload);
        return createNode(action);
    }

    bool DocumentHandle::removeWidget(const WidgetId& id)
    {
        DeleteAction action;
        action.kind = NodeKind::widget;
        action.ids = { id };
        return deleteNodes(action);
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

        SetBoundsAction action;
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

            SetBoundsAction::Item item;
            item.id = update.id;
            item.bounds = update.bounds;
            action.items.push_back(item);
        }

        if (!hasAnyChange)
            return false;

        return setBounds(action);
    }

    bool DocumentHandle::groupSelection(std::optional<WidgetId> layerId)
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

        CreateGroupPayload payload;
        payload.name = "Group";
        payload.members.reserve(explicitWidgetIds.size() + selectedGroupIds.size());
        for (const auto widgetId : explicitWidgetIds)
            payload.members.push_back({ NodeKind::widget, widgetId });
        for (const auto groupId : selectedGroupIds)
            payload.members.push_back({ NodeKind::group, groupId });

        if (layerId.has_value())
        {
            payload.parent.kind = ParentKind::layer;
            payload.parent.id = *layerId;
        }
        else
        {
            payload.parent.kind = ParentKind::root;
            payload.parent.id = kRootId;
        }

        CreateAction action;
        action.kind = NodeKind::group;
        action.payload = std::move(payload);
        return createNode(action) > kRootId;
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

        DeleteAction action;
        action.kind = NodeKind::group;
        action.ids = std::move(groupIds);
        action.policy = DeleteGroupPolicy {};
        return deleteNodes(action);
    }

    bool DocumentHandle::reparent(ReparentAction action)
    {
        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::reorder(ReorderAction action)
    {
        return impl->commitDocumentAction(action, nullptr);
    }

    bool DocumentHandle::setRuntimeBindings(std::vector<RuntimeBindingModel> bindings)
    {
        if (!impl->finalizeActiveCoalescedEdit(true))
            return false;

        const auto& currentDocument = impl->store.snapshot();
        if (runtimeBindingsEqual(currentDocument.runtimeBindings, bindings))
            return false;

        auto nextDocument = currentDocument;
        nextDocument.runtimeBindings = std::move(bindings);

        const auto previous = impl->snapshot();
        impl->store.reset(std::move(nextDocument));
        impl->editorState = previous.editorState;
        impl->pushUndoState(previous);
        impl->clearRedo();
        ++impl->historySerial;
        return true;
    }

    bool DocumentHandle::setAssets(std::vector<AssetModel> assets)
    {
        if (!impl->finalizeActiveCoalescedEdit(true))
            return false;

        const auto& currentDocument = impl->store.snapshot();
        if (assetsEqual(currentDocument.assets, assets))
            return false;

        auto nextDocument = currentDocument;
        nextDocument.assets = std::move(assets);

        const auto previous = impl->snapshot();
        impl->store.reset(std::move(nextDocument));
        impl->editorState = previous.editorState;
        impl->pushUndoState(previous);
        impl->clearRedo();
        ++impl->historySerial;
        return true;
    }

    bool DocumentHandle::replaceAssetRefKey(const juce::String& oldRefKey, const juce::String& newRefKey)
    {
        if (!impl->finalizeActiveCoalescedEdit(true))
            return false;

        const auto oldKey = oldRefKey.trim();
        const auto newKey = newRefKey.trim();
        if (oldKey.isEmpty() || newKey.isEmpty() || oldKey == newKey)
            return false;

        const auto& currentDocument = impl->store.snapshot();
        auto nextDocument = currentDocument;

        bool changed = false;
        bool hasSourceAsset = false;

        for (const auto& asset : nextDocument.assets)
        {
            if (asset.refKey.trim().equalsIgnoreCase(newKey) && asset.refKey.trim() != oldKey)
                return false;
        }

        for (auto& asset : nextDocument.assets)
        {
            if (asset.refKey.trim() != oldKey)
                continue;

            asset.refKey = newKey;
            hasSourceAsset = true;
            changed = true;
        }

        if (!hasSourceAsset)
            return false;

        for (auto& widget : nextDocument.widgets)
            changed = replaceStringInPropertyBag(widget.properties, oldKey, newKey) || changed;

        for (auto& binding : nextDocument.runtimeBindings)
        {
            for (auto& action : binding.actions)
                changed = replaceStringInPropertyBag(action.patch, oldKey, newKey) || changed;
        }

        if (!changed)
            return false;

        const auto previous = impl->snapshot();
        impl->store.reset(std::move(nextDocument));
        impl->editorState = previous.editorState;
        impl->pushUndoState(previous);
        impl->clearRedo();
        ++impl->historySerial;
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

        impl->pushUndoState(impl->snapshot());
        impl->editorState.selection = std::move(normalized);
        impl->clearRedo();
        ++impl->historySerial;
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

    int DocumentHandle::undoDepth() const noexcept
    {
        return static_cast<int>(impl->undoStack.size());
    }

    int DocumentHandle::redoDepth() const noexcept
    {
        return static_cast<int>(impl->redoStack.size());
    }

    uint64_t DocumentHandle::historySerial() const noexcept
    {
        return impl->historySerial;
    }

    bool DocumentHandle::undo()
    {
        impl->finalizeActiveCoalescedEdit(false);

        if (impl->undoStack.empty())
            return false;

        impl->redoStack.push_back(impl->snapshot());
        auto previous = std::move(impl->undoStack.back());
        impl->undoStack.pop_back();
        impl->restore(std::move(previous));
        ++impl->historySerial;
        return true;
    }

    bool DocumentHandle::redo()
    {
        impl->finalizeActiveCoalescedEdit(false);

        if (impl->redoStack.empty())
            return false;

        impl->pushUndoState(impl->snapshot());
        auto next = std::move(impl->redoStack.back());
        impl->redoStack.pop_back();
        impl->restore(std::move(next));
        ++impl->historySerial;
        return true;
    }

    juce::Result DocumentHandle::saveToFile(const juce::File& file) const
    {
        return Serialization::saveDocumentToFile(file, snapshot(), editorState());
    }

    juce::Result DocumentHandle::loadFromFile(const juce::File& file)
    {
        impl->finalizeActiveCoalescedEdit(false);

        DocumentModel loadedDocument;
        EditorStateModel loadedEditorState;
        const auto result = Serialization::loadDocumentFromFile(file, loadedDocument, loadedEditorState);
        if (result.failed())
            return result;

        impl->store.reset(std::move(loadedDocument));
        impl->editorState = std::move(loadedEditorState);
        impl->undoStack.clear();
        impl->redoStack.clear();
        ++impl->historySerial;
        return juce::Result::ok();
    }
}
