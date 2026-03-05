#include "Gyeol/Editor/Shell/EditorPanelMediator.h"

#include <algorithm>
#include <cmath>

namespace Gyeol::Shell
{
    std::optional<WidgetId> EditorPanelMediator::resolveActiveLayerId(
        const State& state,
        const DocumentModel& snapshot) const
    {
        const auto& layers = snapshot.layers;
        if (layers.empty())
            return std::nullopt;

        if (state.activeLayerOverrideId.has_value())
        {
            const auto it = std::find_if(layers.begin(),
                                         layers.end(),
                                         [layerId = *state.activeLayerOverrideId](const LayerModel& layer)
                                         {
                                             return layer.id == layerId;
                                         });
            if (it != layers.end())
                return it->id;
        }

        const auto topIt = std::max_element(
            layers.begin(),
            layers.end(),
            [](const LayerModel& lhs, const LayerModel& rhs)
            {
                if (lhs.order != rhs.order)
                    return lhs.order < rhs.order;
                return lhs.id < rhs.id;
            });
        return topIt != layers.end()
            ? std::optional<WidgetId>{ topIt->id }
            : std::optional<WidgetId>{};
    }

    Ui::Panels::PropertyPanel::InspectorTarget
    EditorPanelMediator::resolveInspectorTarget(const Components& components) const
    {
        Ui::Panels::PropertyPanel::InspectorTarget target;

        if (const auto selectedNode = components.layerTreePanel.selectedNode();
            selectedNode.has_value())
        {
            if (selectedNode->kind == NodeKind::layer)
            {
                target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::layer;
                target.nodeId = selectedNode->id;
                return target;
            }

            if (selectedNode->kind == NodeKind::group)
            {
                target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::group;
                target.nodeId = selectedNode->id;
                return target;
            }

            const auto& selection = components.document.editorState().selection;
            if (selection.size() > 1
                && std::find(selection.begin(), selection.end(), selectedNode->id)
                    != selection.end())
            {
                target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::widgetMulti;
                target.widgetIds = selection;
                return target;
            }

            target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::widgetSingle;
            target.nodeId = selectedNode->id;
            target.widgetIds = { selectedNode->id };
            return target;
        }

        const auto& selection = components.document.editorState().selection;
        if (selection.empty())
        {
            target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::none;
            return target;
        }

        if (selection.size() == 1)
        {
            target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::widgetSingle;
            target.nodeId = selection.front();
            target.widgetIds = selection;
            return target;
        }

        target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::widgetMulti;
        target.widgetIds = selection;
        return target;
    }

    void EditorPanelMediator::syncInspectorTargetFromState(
        const Components& components) const
    {
        components.propertyPanel.setInspectorTarget(resolveInspectorTarget(components));
        components.propertyPanel.refreshFromDocument();
    }

    void EditorPanelMediator::refreshNavigatorSceneFromDocument(
        const Components& components) const
    {
        const auto& snapshot = components.document.snapshot();
        const auto& selection = components.document.editorState().selection;
        std::unordered_set<WidgetId> selectedWidgetIds(selection.begin(), selection.end());

        std::vector<Ui::Panels::NavigatorPanel::SceneItem> items;
        items.reserve(snapshot.widgets.size());

        for (const auto& widget : snapshot.widgets)
        {
            Ui::Panels::NavigatorPanel::SceneItem item;
            item.bounds = widget.bounds;
            item.selected = selectedWidgetIds.count(widget.id) > 0;
            item.visible = widget.visible;
            item.locked = widget.locked;
            items.push_back(std::move(item));
        }

        components.navigatorPanel.setScene(components.canvas.canvasWorldBounds(),
                                           std::move(items));
    }

    void EditorPanelMediator::refreshAllPanelsFromDocument(
        State& state,
        const Components& components,
        const std::function<void()>& refreshViewDiagnosticsPanels,
        const std::function<void()>& refreshToolbarStateCb) const
    {
        components.canvas.refreshFromDocument();
        components.layerTreePanel.refreshFromDocument();
        components.widgetLibraryPanel.refreshFromRegistry();
        components.assetsPanel.refreshFromDocument();
        refreshNavigatorSceneFromDocument(components);
        components.eventActionPanel.refreshFromDocument();
        components.validationPanel.refreshValidation();
        components.exportPreviewPanel.markDirty();

        if (refreshViewDiagnosticsPanels)
            refreshViewDiagnosticsPanels();
        if (refreshToolbarStateCb)
            refreshToolbarStateCb();

        syncInspectorTargetFromState(components);
        components.historyPanel.setStackState(components.document.undoDepth(),
                                              components.document.redoDepth(),
                                              components.document.historySerial());
        state.lastDocumentDigest = computeDocumentDigest(components.document);
    }

    bool EditorPanelMediator::requestDeferredUiRefresh(State& state,
                                                       bool refreshLayerTree,
                                                       bool refreshInspector,
                                                       bool asyncUpdatePending) const
    {
        state.deferredRefreshRequestCount += 1;
        const auto alreadyPending = state.pendingLayerTreeRefresh
            || state.pendingInspectorSync
            || state.pendingEventActionSync
            || state.pendingAssetsSync;

        state.pendingLayerTreeRefresh = state.pendingLayerTreeRefresh || refreshLayerTree;
        state.pendingInspectorSync = state.pendingInspectorSync || refreshInspector;
        state.pendingEventActionSync = state.pendingEventActionSync || refreshLayerTree;
        state.pendingAssetsSync = state.pendingAssetsSync || refreshLayerTree;

        if (alreadyPending)
            state.deferredRefreshCoalescedCount += 1;

        const auto anyPending = state.pendingLayerTreeRefresh
            || state.pendingInspectorSync
            || state.pendingEventActionSync
            || state.pendingAssetsSync;
        return anyPending && !asyncUpdatePending;
    }

    void EditorPanelMediator::refreshCanvasAndRequestPanels(
        State& state,
        const Components& components,
        bool refreshCanvas,
        bool refreshLayerTree,
        bool refreshInspector,
        bool& suppressNextCanvasMutationHistory,
        const std::function<void()>& refreshToolbarStateCb,
        bool asyncUpdatePending,
        const std::function<void()>& triggerAsyncUpdate) const
    {
        if (refreshCanvas)
        {
            suppressNextCanvasMutationHistory = true;
            components.canvas.refreshFromDocument();
        }

        if (refreshToolbarStateCb)
            refreshToolbarStateCb();

        const auto shouldTrigger = requestDeferredUiRefresh(
            state,
            refreshLayerTree,
            refreshInspector,
            asyncUpdatePending);

        if (shouldTrigger && triggerAsyncUpdate)
            triggerAsyncUpdate();
    }

    void EditorPanelMediator::flushDeferredUiRefresh(
        State& state,
        const Components& components,
        const std::function<void()>& refreshViewDiagnosticsPanels) const
    {
        state.deferredRefreshFlushCount += 1;

        const auto shouldRefreshLayerTree = state.pendingLayerTreeRefresh;
        const auto shouldSyncInspector = state.pendingInspectorSync;
        const auto shouldSyncEventAction = state.pendingEventActionSync;
        const auto shouldSyncAssets = state.pendingAssetsSync;

        state.pendingLayerTreeRefresh = false;
        state.pendingInspectorSync = false;
        state.pendingEventActionSync = false;
        state.pendingAssetsSync = false;

        if (shouldRefreshLayerTree)
            components.layerTreePanel.refreshFromDocument();
        if (shouldSyncInspector)
            syncInspectorTargetFromState(components);
        if (shouldSyncEventAction)
            components.eventActionPanel.refreshFromDocument();
        if (shouldSyncAssets)
            components.assetsPanel.refreshFromDocument();

        if (shouldRefreshLayerTree || shouldSyncInspector || shouldSyncEventAction
            || shouldSyncAssets)
            refreshNavigatorSceneFromDocument(components);

        if (refreshViewDiagnosticsPanels)
            refreshViewDiagnosticsPanels();

        if (state.deferredRefreshFlushCount % 120 == 0)
        {
            DBG(juce::String("[Gyeol][Editor][Perf] deferredRefresh flush#")
                + juce::String(static_cast<int64_t>(state.deferredRefreshFlushCount))
                + " requests="
                + juce::String(static_cast<int64_t>(state.deferredRefreshRequestCount))
                + " coalesced="
                + juce::String(static_cast<int64_t>(state.deferredRefreshCoalescedCount)));
        }
    }

    void EditorPanelMediator::refreshToolbarState(
        const ToolbarComponents& toolbar,
        const DocumentHandle& document,
        const Ui::Canvas::CanvasComponent& canvas,
        bool previewBindingSimulationActive,
        bool& suppressPreviewBindingToggleCallbacks) const
    {
        const auto runMode = canvas.isRunMode();
        const auto interactionLocked = runMode || previewBindingSimulationActive;

        for (auto* const button : toolbar.createButtons)
        {
            if (button != nullptr)
                button->setEnabled(!interactionLocked);
        }

        toolbar.deleteSelected.setEnabled(!interactionLocked
                                         && !document.editorState().selection.empty());
        toolbar.groupSelected.setEnabled(!interactionLocked && canvas.canGroupSelection());
        toolbar.ungroupSelected.setEnabled(!interactionLocked && canvas.canUngroupSelection());
        toolbar.arrangeMenuButton.setEnabled(
            !interactionLocked && document.editorState().selection.size() >= 2);
        toolbar.dumpJsonButton.setEnabled(!interactionLocked);
        toolbar.exportJuceButton.setEnabled(!interactionLocked);
        toolbar.undoButton.setEnabled(!interactionLocked && document.canUndo());
        toolbar.redoButton.setEnabled(!interactionLocked && document.canRedo());
        toolbar.historyPanel.setCanUndoRedo(!interactionLocked && document.canUndo(),
                                            !interactionLocked && document.canRedo());
        toolbar.previewBindingSimToggle.setEnabled(!runMode);
        toolbar.gridSnapMenuButton.setEnabled(!interactionLocked);
        toolbar.leftDockButton.setEnabled(true);
        toolbar.rightDockButton.setEnabled(true);
        toolbar.uiMenuButton.setEnabled(true);

        suppressPreviewBindingToggleCallbacks = true;
        toolbar.previewBindingSimToggle.setToggleState(previewBindingSimulationActive,
                                                       juce::dontSendNotification);
        suppressPreviewBindingToggleCallbacks = false;

        toolbar.historyPanel.setStackState(document.undoDepth(),
                                           document.redoDepth(),
                                           document.historySerial());
    }

    std::uint64_t EditorPanelMediator::computeDocumentDigest(
        const DocumentHandle& document) const
    {
        auto hash = static_cast<std::uint64_t>(1469598103934665603ULL);

        const auto mix = [&hash](std::uint64_t value)
        {
            hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        };

        const auto mixFloat = [&mix](float value)
        {
            const auto quantized = static_cast<std::int64_t>(
                std::llround(static_cast<double>(value) * 1000.0));
            mix(static_cast<std::uint64_t>(quantized));
        };

        const auto mixBool = [&mix](bool value)
        {
            mix(value ? 1ULL : 0ULL);
        };

        const auto mixString = [&mix](const juce::String& value)
        {
            mix(static_cast<std::uint64_t>(value.hashCode64()));
        };

        const auto& snapshot = document.snapshot();
        mix(static_cast<std::uint64_t>(snapshot.widgets.size()));
        mix(static_cast<std::uint64_t>(snapshot.groups.size()));
        mix(static_cast<std::uint64_t>(snapshot.layers.size()));

        for (const auto& widget : snapshot.widgets)
        {
            mix(static_cast<std::uint64_t>(widget.id));
            mix(static_cast<std::uint64_t>(widget.type));
            mixFloat(widget.bounds.getX());
            mixFloat(widget.bounds.getY());
            mixFloat(widget.bounds.getWidth());
            mixFloat(widget.bounds.getHeight());
            mixBool(widget.visible);
            mixBool(widget.locked);
            mixFloat(widget.opacity);

            mix(static_cast<std::uint64_t>(widget.properties.size()));
            for (int i = 0; i < widget.properties.size(); ++i)
            {
                mixString(widget.properties.getName(i).toString());
                mixString(widget.properties.getValueAt(i).toString());
            }
        }

        for (const auto& group : snapshot.groups)
        {
            mix(static_cast<std::uint64_t>(group.id));
            mixString(group.name);
            mixBool(group.visible);
            mixBool(group.locked);
            mixFloat(group.opacity);
            mix(static_cast<std::uint64_t>(group.parentGroupId.value_or(kRootId)));
            for (const auto memberId : group.memberWidgetIds)
                mix(static_cast<std::uint64_t>(memberId));
            for (const auto childGroupId : group.memberGroupIds)
                mix(static_cast<std::uint64_t>(childGroupId));
        }

        for (const auto& layer : snapshot.layers)
        {
            mix(static_cast<std::uint64_t>(layer.id));
            mixString(layer.name);
            mix(static_cast<std::uint64_t>(layer.order));
            mixBool(layer.visible);
            mixBool(layer.locked);
            for (const auto memberId : layer.memberWidgetIds)
                mix(static_cast<std::uint64_t>(memberId));
            for (const auto groupId : layer.memberGroupIds)
                mix(static_cast<std::uint64_t>(groupId));
        }

        return hash;
    }

    bool EditorPanelMediator::updateDocumentDigest(State& state,
                                                   const DocumentHandle& document) const
    {
        const auto nextDigest = computeDocumentDigest(document);
        const auto changed = nextDigest != state.lastDocumentDigest;
        state.lastDocumentDigest = nextDigest;
        return changed;
    }

    void EditorPanelMediator::rememberRecentSelection(const DocumentHandle& document,
                                                      std::vector<WidgetId>& recentWidgetIds,
                                                      std::size_t maxCount) const
    {
        const auto& selection = document.editorState().selection;
        for (const auto id : selection)
        {
            recentWidgetIds.erase(
                std::remove(recentWidgetIds.begin(), recentWidgetIds.end(), id),
                recentWidgetIds.end());
            recentWidgetIds.insert(recentWidgetIds.begin(), id);
        }

        if (recentWidgetIds.size() > maxCount)
            recentWidgetIds.resize(maxCount);
    }
}
