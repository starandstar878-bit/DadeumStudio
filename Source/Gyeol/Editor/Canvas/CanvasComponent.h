#pragma once

#include "Gyeol/Editor/Canvas/CanvasRenderer.h"
#include "Gyeol/Editor/Canvas/MarqueeSelectionOverlay.h"
#include "Gyeol/Editor/Canvas/SnapGuideOverlay.h"
#include "Gyeol/Editor/Canvas/WidgetComponent.h"
#include "Gyeol/Editor/Interaction/SnapEngine.h"
#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Gyeol::Ui::Canvas
{
    class CanvasComponent : public juce::Component,
                            public juce::DragAndDropTarget
    {
    public:
        enum class InteractionMode
        {
            preview,
            run
        };

        struct PerfStats
        {
            uint64_t refreshCount = 0;
            uint64_t paintCount = 0;
            uint64_t selectionSyncCount = 0;
            uint64_t dragPreviewUpdateCount = 0;
            uint64_t refreshRequestedPartialRepaintCount = 0;
            uint64_t refreshRequestedFullRepaintCount = 0;
            uint64_t selectionSyncRequestedPartialRepaintCount = 0;
            double lastRefreshMs = 0.0;
            double maxRefreshMs = 0.0;
            double lastPaintMs = 0.0;
            double maxPaintMs = 0.0;
            double lastSelectionSyncMs = 0.0;
            double maxSelectionSyncMs = 0.0;
            float lastDirtyAreaPx = 0.0f;
            int lastWidgetViewCount = 0;
            int lastSelectionCount = 0;
        };

        explicit CanvasComponent(DocumentHandle& documentIn);
        CanvasComponent(DocumentHandle& documentIn,
                        const Widgets::WidgetFactory& widgetFactoryIn);
        ~CanvasComponent() override;

        void setInteractionMode(InteractionMode nextMode);
        InteractionMode interactionMode() const noexcept;
        bool isRunMode() const noexcept;

        void setPreviewBindingSimulationEnabled(bool enabled);
        bool isPreviewBindingSimulationEnabled() const noexcept;

        void setStateChangedCallback(std::function<void()> callback);
        void setViewportChangedCallback(std::function<void()> callback);
        void setRuntimeLogCallback(
            std::function<void(const juce::String&, const juce::String&)> callback);
        void setActiveLayerResolver(std::function<std::optional<WidgetId>()> resolver);
        void setWidgetLibraryDropCallback(
            std::function<void(const juce::String&, juce::Point<float>)> callback);

        void setSnapSettings(const Interaction::SnapSettings& settingsIn);
        const Interaction::SnapSettings& currentSnapSettings() const noexcept;

        float currentZoomLevel() const noexcept;
        juce::Point<float> currentViewOriginWorld() const noexcept;
        juce::Rectangle<float> visibleWorldBounds() const noexcept;

        juce::Rectangle<int> viewportBounds() const noexcept;
        juce::Rectangle<float> canvasWorldBounds() const;

        juce::Point<float> worldToView(juce::Point<float> worldPoint) const noexcept;
        juce::Point<float> viewToWorld(juce::Point<float> viewPoint) const noexcept;
        juce::Rectangle<float> worldToViewRect(const juce::Rectangle<float>& worldRect) const noexcept;
        juce::Rectangle<float> viewToWorldRect(const juce::Rectangle<float>& viewRect) const noexcept;

        bool focusWidget(WidgetId widgetId);
        bool focusWorldPoint(juce::Point<float> worldPoint);
        juce::Point<float> snapCreateOrigin(WidgetType type,
                                            juce::Point<float> worldOrigin) const;

        void refreshFromDocument();
        void syncSelectionFromDocument();

        bool deleteSelection();
        bool performUndo();
        bool performRedo();
        bool groupSelection();
        bool ungroupSelection();
        bool canGroupSelection() const noexcept;
        bool canUngroupSelection() const noexcept;

        const PerfStats& performanceStats() const noexcept;

        bool isInterestedInDragSource(
            const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDragEnter(
            const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDragMove(
            const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDragExit(
            const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDropped(
            const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;

        void paint(juce::Graphics& g) override;
        void paintOverChildren(juce::Graphics& g) override;
        void resized() override;

        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void mouseWheelMove(const juce::MouseEvent& event,
                            const juce::MouseWheelDetails& wheel) override;

        bool keyPressed(const juce::KeyPress& key) override;

    private:
        struct DragItemState
        {
            WidgetId widgetId = kRootId;
            juce::Rectangle<float> startBounds;
            juce::Rectangle<float> currentBounds;
        };

        struct DragState
        {
            bool active = false;
            WidgetId anchorWidgetId = kRootId;
            juce::Point<float> startMouseView;
            juce::Rectangle<float> startSelectionBounds;
            std::vector<DragItemState> items;
        };

        struct MarqueeState
        {
            bool active = false;
            bool additive = false;
            bool toggle = false;
            juce::Point<float> startMouse;
            juce::Rectangle<float> bounds;
        };

        struct PanState
        {
            bool active = false;
            juce::Point<float> startMouse;
            juce::Point<float> startViewOriginWorld;
        };

        struct Guide
        {
            bool vertical = false;
            float worldPosition = 0.0f;
        };

        struct GuideDragState
        {
            bool active = false;
            bool vertical = true;
            juce::Point<float> startMouse;
            float worldPosition = 0.0f;
            bool previewInViewport = false;
        };

        void rebuildWidgetViews();
        void updateAllWidgetViewBounds();
        void syncSelectionToViews();

        void notifyStateChanged();
        void notifyViewportChanged();
        void emitRuntimeLog(const juce::String& action,
                            const juce::String& detail);

        void setZoomAtPoint(float nextZoom, juce::Point<float> localAnchor);
        void clampViewOriginToCanvas();

        juce::Rectangle<int> topRulerBounds() const noexcept;
        juce::Rectangle<int> leftRulerBounds() const noexcept;
        bool isPointInTopRuler(juce::Point<float> localPoint) const noexcept;
        bool isPointInLeftRuler(juce::Point<float> localPoint) const noexcept;
        bool isPointInCanvasView(juce::Point<float> localPoint) const noexcept;

        bool beginDragForSelection(WidgetId anchorId,
                                   juce::Point<float> startMouseView);
        void updateDragPreview(juce::Point<float> currentMouseView);
        void commitDrag();

        std::vector<WidgetId> collectMarqueeHitIds(
            const juce::Rectangle<float>& marqueeBounds) const;
        void applyMarqueeSelection();
        bool nudgeSelection(juce::Point<float> deltaWorld);

        std::vector<WidgetId> orderedWidgetIdsForCanvas() const;
        std::vector<juce::Rectangle<float>> collectNearbyBoundsForSnap(
            const std::vector<WidgetId>& excludedWidgetIds) const;
        std::vector<float> collectRulerGuidePositions(bool vertical) const;
        Interaction::SnapRequest makeSnapRequest(
            const juce::Rectangle<float>& proposedBounds,
            const std::vector<WidgetId>& excludedWidgetIds) const;

        void clearTransientSnapGuides();
        void updateTransientSnapGuides(const Interaction::SnapResult& snapResult);
        bool removeGuideNearPoint(juce::Point<float> localPoint,
                                  std::optional<bool> verticalOnly);

        WidgetComponent* findWidgetView(WidgetId id) noexcept;
        const WidgetComponent* findWidgetView(WidgetId id) const noexcept;
        const WidgetModel* findWidgetModel(WidgetId id) const noexcept;

        bool isWidgetVisibleForCanvas(WidgetId id) const noexcept;
        bool isWidgetLockedForCanvas(WidgetId id) const noexcept;
        float effectiveOpacityForWidget(WidgetId id) const noexcept;
        std::optional<juce::String> extractWidgetLibraryTypeKey(
            const juce::var& description) const;

        std::optional<WidgetId> resolveActiveLayerId() const;

        void updateMouseTrackerFromChild(juce::Point<float> localPoint);
        void clearMouseTrackerFromChild();
        void applyWheelZoomAtPoint(juce::Point<float> localPoint,
                                   const juce::MouseWheelDetails& wheel);
        void setMouseTrackerPoint(juce::Point<float> localPoint);
        void clearMouseTrackerPoint();

        void handleWidgetMouseDown(WidgetId id,
                                   bool resizeHit,
                                   const juce::MouseEvent& event);
        void handleWidgetMouseDrag(WidgetId id,
                                   const juce::MouseEvent& event);
        void handleWidgetMouseUp(WidgetId id,
                                 const juce::MouseEvent* event = nullptr);
        void handleCanvasDoubleClick(juce::Point<float> localPoint);

        static bool containsWidgetId(const std::vector<WidgetId>& ids,
                                     WidgetId id);
        static bool areClose(float lhs, float rhs) noexcept;
        static bool areRectsEqual(const juce::Rectangle<float>& lhs,
                                  const juce::Rectangle<float>& rhs) noexcept;
        static juce::Rectangle<float> unionRect(
            const juce::Rectangle<float>& lhs,
            const juce::Rectangle<float>& rhs) noexcept;
        static juce::Rectangle<float> makeNormalizedRect(
            juce::Point<float> a,
            juce::Point<float> b) noexcept;

        static constexpr int rulerThicknessPx = 20;
        static constexpr float guideRemoveThresholdPx = 8.0f;
        static constexpr float minCanvasZoom = 0.2f;
        static constexpr float maxCanvasZoom = 4.0f;
        static constexpr float canvasWorldWidth = 1600.0f;
        static constexpr float canvasWorldHeight = 1000.0f;

        DocumentHandle& document;
        const Widgets::WidgetFactory& widgetFactory;
        CanvasRenderer renderer;

        std::vector<std::unique_ptr<WidgetComponent>> widgetViews;
        MarqueeSelectionOverlay marqueeOverlay;
        SnapGuideOverlay snapGuideOverlay;

        std::function<void()> onStateChanged;
        std::function<void()> onViewportChanged;
        std::function<void(const juce::String&, const juce::String&)> onRuntimeLog;
        std::function<std::optional<WidgetId>()> activeLayerResolver;
        std::function<void(const juce::String&, juce::Point<float>)> onWidgetLibraryDrop;

        InteractionMode interactionModeValue = InteractionMode::preview;
        bool previewBindingSimulationEnabled = false;

        Interaction::SnapEngine snapEngine;
        Interaction::SnapSettings snapSettings;
        std::vector<Guide> guides;
        std::vector<Guide> transientSnapGuides;
        std::vector<Interaction::SmartSpacingHint> transientSmartSpacingHints;

        float zoomLevel = 1.0f;
        juce::Point<float> viewOriginWorld;
        DragState dragState;
        MarqueeState marqueeState;
        PanState panState;
        GuideDragState guideDragState;

        bool hasMouseLocalPoint = false;
        juce::Point<float> lastMouseLocalPoint;
        bool widgetLibraryDropPreviewActive = false;
        juce::Point<float> widgetLibraryDropPreviewView;

        std::vector<WidgetId> lastSelectionSnapshot;
        PerfStats perf;

        friend class WidgetComponent;
    };
}
