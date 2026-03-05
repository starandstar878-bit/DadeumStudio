#pragma once

#include "Gyeol/Editor/Canvas/CanvasComponent.h"
#include "Gyeol/Editor/Panels/AssetsPanel.h"
#include "Gyeol/Editor/Panels/EventActionPanel.h"
#include "Gyeol/Editor/Panels/ExportPreviewPanel.h"
#include "Gyeol/Editor/Panels/HistoryPanel.h"
#include "Gyeol/Editor/Panels/LayerTreePanel.h"
#include "Gyeol/Editor/Panels/NavigatorPanel.h"
#include "Gyeol/Editor/Panels/PropertyPanel.h"
#include "Gyeol/Editor/Panels/ValidationPanel.h"
#include "Gyeol/Editor/Panels/WidgetLibraryPanel.h"
#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Gyeol::Shell
{
    class EditorPanelMediator
    {
    public:
        struct State
        {
            std::optional<WidgetId> activeLayerOverrideId;
            bool pendingLayerTreeRefresh = false;
            bool pendingInspectorSync = false;
            bool pendingEventActionSync = false;
            bool pendingAssetsSync = false;
            uint64_t deferredRefreshRequestCount = 0;
            uint64_t deferredRefreshCoalescedCount = 0;
            uint64_t deferredRefreshFlushCount = 0;
            std::uint64_t lastDocumentDigest = 0;
        };

        struct Components
        {
            DocumentHandle& document;
            Ui::Canvas::CanvasComponent& canvas;
            Ui::Panels::LayerTreePanel& layerTreePanel;
            Ui::Panels::WidgetLibraryPanel& widgetLibraryPanel;
            Ui::Panels::AssetsPanel& assetsPanel;
            Ui::Panels::EventActionPanel& eventActionPanel;
            Ui::Panels::ValidationPanel& validationPanel;
            Ui::Panels::ExportPreviewPanel& exportPreviewPanel;
            Ui::Panels::PropertyPanel& propertyPanel;
            Ui::Panels::HistoryPanel& historyPanel;
            Ui::Panels::NavigatorPanel& navigatorPanel;
        };

        struct ToolbarComponents
        {
            std::vector<juce::Button*> createButtons;
            Ui::Panels::HistoryPanel& historyPanel;
            juce::Button& deleteSelected;
            juce::Button& groupSelected;
            juce::Button& ungroupSelected;
            juce::Button& arrangeMenuButton;
            juce::Button& dumpJsonButton;
            juce::Button& exportJuceButton;
            juce::Button& undoButton;
            juce::Button& redoButton;
            juce::ToggleButton& previewBindingSimToggle;
            juce::Button& gridSnapMenuButton;
            juce::Button& leftDockButton;
            juce::Button& rightDockButton;
            juce::Button& uiMenuButton;
        };

        std::optional<WidgetId> resolveActiveLayerId(const State& state,
                                                     const DocumentModel& snapshot) const;

        Ui::Panels::PropertyPanel::InspectorTarget
        resolveInspectorTarget(const Components& components) const;

        void syncInspectorTargetFromState(const Components& components) const;

        void refreshNavigatorSceneFromDocument(const Components& components) const;

        void refreshAllPanelsFromDocument(State& state,
                                          const Components& components,
                                          const std::function<void()>& refreshViewDiagnosticsPanels,
                                          const std::function<void()>& refreshToolbarState) const;

        bool requestDeferredUiRefresh(State& state,
                                      bool refreshLayerTree,
                                      bool refreshInspector,
                                      bool asyncUpdatePending) const;

        void refreshCanvasAndRequestPanels(
            State& state,
            const Components& components,
            bool refreshCanvas,
            bool refreshLayerTree,
            bool refreshInspector,
            bool& suppressNextCanvasMutationHistory,
            const std::function<void()>& refreshToolbarState,
            bool asyncUpdatePending,
            const std::function<void()>& triggerAsyncUpdate) const;

        void flushDeferredUiRefresh(State& state,
                                    const Components& components,
                                    const std::function<void()>& refreshViewDiagnosticsPanels) const;

        void refreshToolbarState(const ToolbarComponents& toolbar,
                                 const DocumentHandle& document,
                                 const Ui::Canvas::CanvasComponent& canvas,
                                 bool previewBindingSimulationActive,
                                 bool& suppressPreviewBindingToggleCallbacks) const;

        std::uint64_t computeDocumentDigest(const DocumentHandle& document) const;
        bool updateDocumentDigest(State& state, const DocumentHandle& document) const;

        void rememberRecentSelection(const DocumentHandle& document,
                                     std::vector<WidgetId>& recentWidgetIds,
                                     std::size_t maxCount = 20) const;
    };
}
