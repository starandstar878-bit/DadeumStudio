#include "Gyeol/Editor/Canvas/CanvasComponent.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Gyeol::Ui::Canvas
{
    namespace
    {
        const Widgets::WidgetFactory& fallbackWidgetFactory()
        {
            static Widgets::WidgetRegistry registry = Widgets::makeDefaultWidgetRegistry();
            static Widgets::WidgetFactory factory(registry);
            return factory;
        }
    }

    bool CanvasComponent::containsWidgetId(const std::vector<WidgetId>& ids,
                                           WidgetId id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    bool CanvasComponent::areClose(float lhs, float rhs) noexcept
    {
        return std::abs(lhs - rhs) <= 0.0001f;
    }

    bool CanvasComponent::areRectsEqual(const juce::Rectangle<float>& lhs,
                                        const juce::Rectangle<float>& rhs) noexcept
    {
        return areClose(lhs.getX(), rhs.getX())
            && areClose(lhs.getY(), rhs.getY())
            && areClose(lhs.getWidth(), rhs.getWidth())
            && areClose(lhs.getHeight(), rhs.getHeight());
    }

    juce::Rectangle<float> CanvasComponent::unionRect(
        const juce::Rectangle<float>& lhs,
        const juce::Rectangle<float>& rhs) noexcept
    {
        const auto left = std::min(lhs.getX(), rhs.getX());
        const auto top = std::min(lhs.getY(), rhs.getY());
        const auto right = std::max(lhs.getRight(), rhs.getRight());
        const auto bottom = std::max(lhs.getBottom(), rhs.getBottom());
        return { left, top, right - left, bottom - top };
    }

    juce::Rectangle<float> CanvasComponent::makeNormalizedRect(
        juce::Point<float> a,
        juce::Point<float> b) noexcept
    {
        const auto left = std::min(a.x, b.x);
        const auto top = std::min(a.y, b.y);
        const auto right = std::max(a.x, b.x);
        const auto bottom = std::max(a.y, b.y);
        return { left, top, right - left, bottom - top };
    }

    CanvasComponent::CanvasComponent(DocumentHandle& documentIn)
        : CanvasComponent(documentIn, fallbackWidgetFactory())
    {
    }

    CanvasComponent::CanvasComponent(DocumentHandle& documentIn,
                                     const Widgets::WidgetFactory& widgetFactoryIn)
        : document(documentIn),
          widgetFactory(widgetFactoryIn),
          renderer(widgetFactoryIn)
    {
        setWantsKeyboardFocus(true);
        addAndMakeVisible(marqueeOverlay);
        addAndMakeVisible(snapGuideOverlay);
        marqueeOverlay.setInterceptsMouseClicks(false, false);
        snapGuideOverlay.setInterceptsMouseClicks(false, false);
        refreshFromDocument();
    }

    CanvasComponent::~CanvasComponent() = default;

    void CanvasComponent::setInteractionMode(InteractionMode nextMode)
    {
        if (interactionModeValue == nextMode)
            return;

        interactionModeValue = nextMode;
        dragState = {};
        marqueeState = {};
        panState = {};
        clearTransientSnapGuides();
        syncSelectionToViews();
        repaint();
    }

    CanvasComponent::InteractionMode CanvasComponent::interactionMode() const noexcept
    {
        return interactionModeValue;
    }

    bool CanvasComponent::isRunMode() const noexcept
    {
        return interactionModeValue == InteractionMode::run;
    }

    void CanvasComponent::setPreviewBindingSimulationEnabled(bool enabled)
    {
        if (previewBindingSimulationEnabled == enabled)
            return;

        previewBindingSimulationEnabled = enabled;
        syncSelectionToViews();
        repaint();
    }

    bool CanvasComponent::isPreviewBindingSimulationEnabled() const noexcept
    {
        return previewBindingSimulationEnabled;
    }

    void CanvasComponent::setStateChangedCallback(std::function<void()> callback)
    {
        onStateChanged = std::move(callback);
    }

    void CanvasComponent::setViewportChangedCallback(std::function<void()> callback)
    {
        onViewportChanged = std::move(callback);
    }

    void CanvasComponent::setRuntimeLogCallback(
        std::function<void(const juce::String&, const juce::String&)> callback)
    {
        onRuntimeLog = std::move(callback);
    }

    void CanvasComponent::setActiveLayerResolver(
        std::function<std::optional<WidgetId>()> resolver)
    {
        activeLayerResolver = std::move(resolver);
    }

    void CanvasComponent::setWidgetLibraryDropCallback(
        std::function<void(const juce::String&, juce::Point<float>)> callback)
    {
        onWidgetLibraryDrop = std::move(callback);
    }

    void CanvasComponent::setSnapSettings(const Interaction::SnapSettings& settingsIn)
    {
        snapSettings = settingsIn;
        repaint();
    }

    const Interaction::SnapSettings& CanvasComponent::currentSnapSettings() const noexcept
    {
        return snapSettings;
    }

    float CanvasComponent::currentZoomLevel() const noexcept { return zoomLevel; }

    juce::Point<float> CanvasComponent::currentViewOriginWorld() const noexcept
    {
        return viewOriginWorld;
    }

    juce::Rectangle<int> CanvasComponent::viewportBounds() const noexcept
    {
        auto area = getLocalBounds();
        area.removeFromTop(rulerThicknessPx);
        area.removeFromLeft(rulerThicknessPx);
        return area;
    }

    juce::Rectangle<float> CanvasComponent::canvasWorldBounds() const
    {
        return { 0.0f, 0.0f, canvasWorldWidth, canvasWorldHeight };
    }

    juce::Point<float> CanvasComponent::worldToView(juce::Point<float> worldPoint) const noexcept
    {
        const auto viewport = viewportBounds();
        return { static_cast<float>(viewport.getX()) + (worldPoint.x - viewOriginWorld.x) * zoomLevel,
                 static_cast<float>(viewport.getY()) + (worldPoint.y - viewOriginWorld.y) * zoomLevel };
    }

    juce::Point<float> CanvasComponent::viewToWorld(juce::Point<float> viewPoint) const noexcept
    {
        const auto viewport = viewportBounds();
        return { viewOriginWorld.x + (viewPoint.x - static_cast<float>(viewport.getX())) / zoomLevel,
                 viewOriginWorld.y + (viewPoint.y - static_cast<float>(viewport.getY())) / zoomLevel };
    }

    juce::Rectangle<float> CanvasComponent::worldToViewRect(const juce::Rectangle<float>& worldRect) const noexcept
    {
        const auto topLeft = worldToView(worldRect.getTopLeft());
        return { topLeft.x, topLeft.y, worldRect.getWidth() * zoomLevel, worldRect.getHeight() * zoomLevel };
    }

    juce::Rectangle<float> CanvasComponent::viewToWorldRect(const juce::Rectangle<float>& viewRect) const noexcept
    {
        const auto topLeft = viewToWorld(viewRect.getTopLeft());
        return { topLeft.x, topLeft.y, viewRect.getWidth() / zoomLevel, viewRect.getHeight() / zoomLevel };
    }

    juce::Rectangle<float> CanvasComponent::visibleWorldBounds() const noexcept
    {
        const auto viewport = viewportBounds();
        if (viewport.isEmpty())
            return {};
        return viewToWorldRect(viewport.toFloat());
    }

    bool CanvasComponent::focusWidget(WidgetId widgetId)
    {
        if (const auto* widget = findWidgetModel(widgetId); widget != nullptr)
            return focusWorldPoint(widget->bounds.getCentre());
        return false;
    }

    bool CanvasComponent::focusWorldPoint(juce::Point<float> worldPoint)
    {
        const auto viewport = viewportBounds();
        if (viewport.isEmpty())
            return false;

        const auto visibleW = static_cast<float>(viewport.getWidth()) / zoomLevel;
        const auto visibleH = static_cast<float>(viewport.getHeight()) / zoomLevel;
        viewOriginWorld = { worldPoint.x - visibleW * 0.5f, worldPoint.y - visibleH * 0.5f };
        clampViewOriginToCanvas();
        updateAllWidgetViewBounds();
        repaint();
        notifyViewportChanged();
        return true;
    }

    juce::Point<float> CanvasComponent::snapCreateOrigin(WidgetType type,
                                                          juce::Point<float> worldOrigin) const
    {
        if (!snapSettings.snapEnabled)
            return worldOrigin;

        const auto* descriptor = widgetFactory.descriptorFor(type);
        if (descriptor == nullptr)
            return worldOrigin;

        const auto proposed = descriptor->defaultBounds.withPosition(worldOrigin);
        const auto snapResult = snapEngine.compute(makeSnapRequest(proposed, {}));
        return snapResult.snappedBounds.getPosition();
    }

    void CanvasComponent::refreshFromDocument()
    {
        const auto started = std::chrono::steady_clock::now();
        dragState = {};
        marqueeState = {};
        panState = {};
        marqueeOverlay.clearMarquee();
        clearTransientSnapGuides();
        rebuildWidgetViews();
        repaint();

        perf.refreshCount += 1;
        perf.refreshRequestedFullRepaintCount += 1;
        perf.lastWidgetViewCount = static_cast<int>(widgetViews.size());
        perf.lastSelectionCount = static_cast<int>(document.editorState().selection.size());
        perf.lastRefreshMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        perf.maxRefreshMs = std::max(perf.maxRefreshMs, perf.lastRefreshMs);

        notifyStateChanged();
        notifyViewportChanged();
    }

    void CanvasComponent::syncSelectionFromDocument() { syncSelectionToViews(); }

    bool CanvasComponent::deleteSelection()
    {
        if (isRunMode())
            return false;
        auto ids = document.editorState().selection;
        if (ids.empty())
            return false;

        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        DeleteAction action;
        action.kind = NodeKind::widget;
        action.ids = std::move(ids);
        if (!document.deleteNodes(action))
            return false;

        emitRuntimeLog("Canvas", "Delete selection");
        refreshFromDocument();
        return true;
    }

    bool CanvasComponent::performUndo() { if (!document.undo()) return false; refreshFromDocument(); return true; }
    bool CanvasComponent::performRedo() { if (!document.redo()) return false; refreshFromDocument(); return true; }

    bool CanvasComponent::groupSelection()
    {
        if (isRunMode())
            return false;
        if (!document.groupSelection(resolveActiveLayerId()))
            return false;
        refreshFromDocument();
        return true;
    }

    bool CanvasComponent::ungroupSelection()
    {
        if (isRunMode())
            return false;
        if (!document.ungroupSelection())
            return false;
        refreshFromDocument();
        return true;
    }

    bool CanvasComponent::canGroupSelection() const noexcept { return document.editorState().selection.size() >= 2; }

    bool CanvasComponent::canUngroupSelection() const noexcept
    {
        for (const auto id : document.editorState().selection)
            for (const auto& group : document.snapshot().groups)
                if (containsWidgetId(group.memberWidgetIds, id))
                    return true;
        return false;
    }

    const CanvasComponent::PerfStats& CanvasComponent::performanceStats() const noexcept
    {
        return perf;
    }
    bool CanvasComponent::isInterestedInDragSource(
        const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        if (isRunMode())
            return false;
        return extractWidgetLibraryTypeKey(dragSourceDetails.description).has_value();
    }

    void CanvasComponent::itemDragEnter(
        const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        itemDragMove(dragSourceDetails);
    }

    void CanvasComponent::itemDragMove(
        const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        if (isRunMode())
        {
            widgetLibraryDropPreviewActive = false;
            repaint();
            return;
        }

        if (!extractWidgetLibraryTypeKey(dragSourceDetails.description).has_value())
        {
            if (widgetLibraryDropPreviewActive)
            {
                widgetLibraryDropPreviewActive = false;
                repaint();
            }
            return;
        }

        const auto point = dragSourceDetails.localPosition.toFloat();
        if (!viewportBounds().toFloat().contains(point))
        {
            if (widgetLibraryDropPreviewActive)
            {
                widgetLibraryDropPreviewActive = false;
                repaint();
            }
            return;
        }

        widgetLibraryDropPreviewActive = true;
        widgetLibraryDropPreviewView = point;
        repaint();
    }

    void CanvasComponent::itemDragExit(
        const juce::DragAndDropTarget::SourceDetails&)
    {
        if (!widgetLibraryDropPreviewActive)
            return;
        widgetLibraryDropPreviewActive = false;
        repaint();
    }

    void CanvasComponent::itemDropped(
        const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        const auto typeKey = extractWidgetLibraryTypeKey(dragSourceDetails.description);
        const auto point = dragSourceDetails.localPosition.toFloat();
        widgetLibraryDropPreviewActive = false;
        repaint();

        if (!typeKey.has_value())
            return;
        if (!viewportBounds().toFloat().contains(point))
            return;
        if (onWidgetLibraryDrop == nullptr)
            return;

        onWidgetLibraryDrop(*typeKey, viewToWorld(point));
    }

    void CanvasComponent::paint(juce::Graphics& g)
    {
        const auto started = std::chrono::steady_clock::now();
        renderer.paintCanvas(g,
                             viewportBounds(),
                             visibleWorldBounds(),
                             canvasWorldBounds(),
                             zoomLevel,
                             snapSettings);

        perf.paintCount += 1;
        perf.lastPaintMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        perf.maxPaintMs = std::max(perf.maxPaintMs, perf.lastPaintMs);
    }

    void CanvasComponent::paintOverChildren(juce::Graphics& g)
    {
        if (widgetLibraryDropPreviewActive)
        {
            const auto p = widgetLibraryDropPreviewView;
            g.setColour(juce::Colour::fromRGBA(84, 212, 255, 220));
            g.drawLine(p.x - 10.0f, p.y, p.x + 10.0f, p.y, 1.4f);
            g.drawLine(p.x, p.y - 10.0f, p.x, p.y + 10.0f, 1.4f);
        }

        if (hasMouseLocalPoint)
        {
            const auto viewport = viewportBounds();
            const auto topRuler = juce::Rectangle<int>(viewport.getX(), 0, viewport.getWidth(), viewport.getY());
            const auto leftRuler = juce::Rectangle<int>(0, viewport.getY(), viewport.getX(), viewport.getHeight());
            g.setColour(juce::Colour::fromRGBA(120, 170, 245, 220));
            if (!topRuler.isEmpty())
            {
                const auto x = juce::jlimit(static_cast<float>(topRuler.getX()),
                                            static_cast<float>(topRuler.getRight()),
                                            lastMouseLocalPoint.x);
                g.drawLine(x,
                           static_cast<float>(topRuler.getY()),
                           x,
                           static_cast<float>(topRuler.getBottom()));
            }
            if (!leftRuler.isEmpty())
            {
                const auto y = juce::jlimit(static_cast<float>(leftRuler.getY()),
                                            static_cast<float>(leftRuler.getBottom()),
                                            lastMouseLocalPoint.y);
                g.drawLine(static_cast<float>(leftRuler.getX()),
                           y,
                           static_cast<float>(leftRuler.getRight()),
                           y);
            }
        }
    }

    void CanvasComponent::resized()
    {
        clampViewOriginToCanvas();
        updateAllWidgetViewBounds();
        marqueeOverlay.setBounds(getLocalBounds());
        snapGuideOverlay.setBounds(getLocalBounds());
        marqueeOverlay.toFront(false);
        snapGuideOverlay.toFront(false);
        notifyViewportChanged();
    }

    void CanvasComponent::mouseDown(const juce::MouseEvent& event)
    {
        setMouseTrackerPoint(event.position);
        clearTransientSnapGuides();

        if (!event.mods.isLeftButtonDown())
            return;

        grabKeyboardFocus();
        if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
        {
            panState.active = true;
            panState.startMouse = event.position;
            panState.startViewOriginWorld = viewOriginWorld;
            return;
        }

        if (isRunMode())
            return;

        if (!viewportBounds().toFloat().contains(event.position))
            return;

        dragState = {};
        marqueeState.active = true;
        marqueeState.additive = event.mods.isShiftDown();
        marqueeState.toggle = event.mods.isCommandDown();
        marqueeState.startMouse = event.position;
        marqueeState.bounds = makeNormalizedRect(event.position, event.position);

        if (!marqueeState.additive && !marqueeState.toggle)
        {
            document.clearSelection();
            syncSelectionToViews();
        }

        marqueeOverlay.setMarquee(marqueeState.bounds);
    }

    void CanvasComponent::mouseDrag(const juce::MouseEvent& event)
    {
        setMouseTrackerPoint(event.position);

        if (panState.active)
        {
            const auto delta = event.position - panState.startMouse;
            viewOriginWorld = panState.startViewOriginWorld
                - juce::Point<float>(delta.x / zoomLevel, delta.y / zoomLevel);
            clampViewOriginToCanvas();
            updateAllWidgetViewBounds();
            repaint();
            notifyViewportChanged();
            return;
        }

        if (isRunMode())
            return;

        if (dragState.active)
        {
            updateDragPreview(event.position);
            return;
        }

        if (!marqueeState.active)
            return;

        marqueeState.bounds = makeNormalizedRect(marqueeState.startMouse, event.position);
        marqueeOverlay.setMarquee(marqueeState.bounds);
        repaint(marqueeState.bounds.expanded(2.0f).toNearestInt());
    }

    void CanvasComponent::mouseUp(const juce::MouseEvent& event)
    {
        setMouseTrackerPoint(event.position);

        if (panState.active)
        {
            panState = {};
            return;
        }

        if (isRunMode())
            return;

        if (dragState.active)
        {
            commitDrag();
            return;
        }

        if (!marqueeState.active)
            return;

        marqueeState.bounds = makeNormalizedRect(marqueeState.startMouse, event.position);
        applyMarqueeSelection();
        marqueeState = {};
        marqueeOverlay.clearMarquee();
    }

    void CanvasComponent::mouseDoubleClick(const juce::MouseEvent& event)
    {
        if (isRunMode())
            return;
        handleCanvasDoubleClick(event.position);
    }

    void CanvasComponent::mouseMove(const juce::MouseEvent& event) { setMouseTrackerPoint(event.position); }
    void CanvasComponent::mouseExit(const juce::MouseEvent&) { clearMouseTrackerPoint(); }

    void CanvasComponent::mouseWheelMove(const juce::MouseEvent& event,
                                         const juce::MouseWheelDetails& wheel)
    {
        applyWheelZoomAtPoint(event.position, wheel);
    }

    bool CanvasComponent::keyPressed(const juce::KeyPress& key)
    {
        if (isRunMode())
            return false;

        const auto mods = key.getModifiers();
        const auto keyCode = key.getKeyCode();
        const auto isZ = keyCode == 'z' || keyCode == 'Z';
        const auto isY = keyCode == 'y' || keyCode == 'Y';
        const auto isG = keyCode == 'g' || keyCode == 'G';

        if (mods.isCommandDown() && isZ)
            return mods.isShiftDown() ? performRedo() : performUndo();
        if (mods.isCommandDown() && isY)
            return performRedo();
        if (mods.isCommandDown() && isG)
            return mods.isShiftDown() ? ungroupSelection() : groupSelection();

        if (!mods.isAnyModifierKeyDown()
            && (keyCode == juce::KeyPress::deleteKey
                || keyCode == juce::KeyPress::backspaceKey))
            return deleteSelection();

        if (!mods.isCommandDown())
        {
            const auto step = mods.isShiftDown() ? 10.0f : 1.0f;
            juce::Point<float> delta;
            if (keyCode == juce::KeyPress::leftKey) delta.x = -step;
            else if (keyCode == juce::KeyPress::rightKey) delta.x = step;
            else if (keyCode == juce::KeyPress::upKey) delta.y = -step;
            else if (keyCode == juce::KeyPress::downKey) delta.y = step;
            if (!areClose(delta.x, 0.0f) || !areClose(delta.y, 0.0f))
                return nudgeSelection(delta);
        }

        return false;
    }
    void CanvasComponent::rebuildWidgetViews()
    {
        for (auto& view : widgetViews)
            if (view != nullptr)
                removeChildComponent(view.get());

        widgetViews.clear();
        const auto orderedIds = orderedWidgetIdsForCanvas();
        widgetViews.reserve(orderedIds.size());

        const auto& selection = document.editorState().selection;
        const auto showSelection = !isRunMode();
        const auto showHandle = showSelection && selection.size() == 1;

        for (const auto id : orderedIds)
        {
            const auto* widget = findWidgetModel(id);
            if (widget == nullptr || !isWidgetVisibleForCanvas(id))
                continue;

            auto view = std::make_unique<WidgetComponent>(*this, renderer);
            const auto selected = showSelection && containsWidgetId(selection, id);
            view->setModel(*widget,
                           worldToViewRect(widget->bounds),
                           selected,
                           effectiveOpacityForWidget(id),
                           selected && showHandle);
            addAndMakeVisible(*view);
            widgetViews.push_back(std::move(view));
        }

        marqueeOverlay.toFront(false);
        snapGuideOverlay.toFront(false);
    }

    void CanvasComponent::updateAllWidgetViewBounds()
    {
        for (auto& view : widgetViews)
        {
            const auto* widget = findWidgetModel(view->widgetId());
            if (widget != nullptr)
                view->setViewBounds(worldToViewRect(widget->bounds));
        }
    }

    void CanvasComponent::syncSelectionToViews()
    {
        const auto started = std::chrono::steady_clock::now();
        const auto& selection = document.editorState().selection;
        const auto showSelection = !isRunMode();
        const auto showHandle = showSelection && selection.size() == 1;

        bool repainted = false;
        for (auto& view : widgetViews)
        {
            const auto selected = showSelection && containsWidgetId(selection, view->widgetId());
            repainted = view->setSelectionVisual(selected, selected && showHandle) || repainted;
        }
        if (repainted)
        {
            repaint();
            perf.selectionSyncRequestedPartialRepaintCount += 1;
        }

        perf.selectionSyncCount += 1;
        perf.lastSelectionCount = static_cast<int>(selection.size());
        perf.lastSelectionSyncMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        perf.maxSelectionSyncMs = std::max(perf.maxSelectionSyncMs, perf.lastSelectionSyncMs);

        notifyStateChanged();
    }

    void CanvasComponent::notifyStateChanged() { if (onStateChanged) onStateChanged(); }
    void CanvasComponent::notifyViewportChanged() { if (onViewportChanged) onViewportChanged(); }
    void CanvasComponent::emitRuntimeLog(const juce::String& action, const juce::String& detail) { if (onRuntimeLog) onRuntimeLog(action, detail); }

    void CanvasComponent::setZoomAtPoint(float nextZoom, juce::Point<float> localAnchor)
    {
        nextZoom = juce::jlimit(minCanvasZoom, maxCanvasZoom, nextZoom);
        if (std::abs(nextZoom - zoomLevel) <= 0.0001f)
            return;

        const auto worldAnchor = viewToWorld(localAnchor);
        zoomLevel = nextZoom;
        const auto viewport = viewportBounds();
        viewOriginWorld.x = worldAnchor.x - (localAnchor.x - static_cast<float>(viewport.getX())) / zoomLevel;
        viewOriginWorld.y = worldAnchor.y - (localAnchor.y - static_cast<float>(viewport.getY())) / zoomLevel;
        clampViewOriginToCanvas();
        updateAllWidgetViewBounds();
        repaint();
        notifyViewportChanged();
    }

    void CanvasComponent::clampViewOriginToCanvas()
    {
        const auto viewport = viewportBounds();
        if (viewport.isEmpty())
            return;

        const auto canvas = canvasWorldBounds();
        const auto visibleW = static_cast<float>(viewport.getWidth()) / zoomLevel;
        const auto visibleH = static_cast<float>(viewport.getHeight()) / zoomLevel;

        if (canvas.getWidth() * zoomLevel <= static_cast<float>(viewport.getWidth()))
            viewOriginWorld.x = canvas.getCentreX() - visibleW * 0.5f;
        else
            viewOriginWorld.x = juce::jlimit(canvas.getX(), canvas.getRight() - visibleW, viewOriginWorld.x);

        if (canvas.getHeight() * zoomLevel <= static_cast<float>(viewport.getHeight()))
            viewOriginWorld.y = canvas.getCentreY() - visibleH * 0.5f;
        else
            viewOriginWorld.y = juce::jlimit(canvas.getY(), canvas.getBottom() - visibleH, viewOriginWorld.y);
    }

    bool CanvasComponent::beginDragForSelection(WidgetId anchorId, juce::Point<float> startMouseView)
    {
        auto ids = document.editorState().selection;
        if (ids.empty() && anchorId > kRootId)
            ids.push_back(anchorId);

        DragState next;
        next.active = true;
        next.anchorWidgetId = anchorId;
        next.startMouseView = startMouseView;

        for (const auto id : ids)
        {
            const auto* widget = findWidgetModel(id);
            if (widget == nullptr || !isWidgetVisibleForCanvas(id) || isWidgetLockedForCanvas(id))
                continue;
            next.items.push_back({ id, widget->bounds, widget->bounds });
        }
        if (next.items.empty())
            return false;

        next.startSelectionBounds = next.items.front().startBounds;
        for (size_t i = 1; i < next.items.size(); ++i)
            next.startSelectionBounds = unionRect(next.startSelectionBounds, next.items[i].startBounds);

        dragState = std::move(next);
        return true;
    }

    void CanvasComponent::updateDragPreview(juce::Point<float> currentMouseView)
    {
        if (!dragState.active)
            return;

        perf.dragPreviewUpdateCount += 1;
        auto deltaPx = currentMouseView - dragState.startMouseView;
        auto delta = juce::Point<float>(deltaPx.x / zoomLevel, deltaPx.y / zoomLevel);

        std::vector<WidgetId> ids;
        for (const auto& item : dragState.items)
            ids.push_back(item.widgetId);

        if (snapSettings.snapEnabled)
        {
            const auto snapped = snapEngine.compute(makeSnapRequest(dragState.startSelectionBounds.translated(delta.x, delta.y), ids));
            delta.x = snapped.snappedBounds.getX() - dragState.startSelectionBounds.getX();
            delta.y = snapped.snappedBounds.getY() - dragState.startSelectionBounds.getY();
            updateTransientSnapGuides(snapped);
        }
        else
        {
            clearTransientSnapGuides();
        }

        for (auto& item : dragState.items)
        {
            item.currentBounds = item.startBounds.translated(delta.x, delta.y);
            if (auto* view = findWidgetView(item.widgetId))
                view->setViewBounds(worldToViewRect(item.currentBounds));
        }
        repaint();
    }

    void CanvasComponent::commitDrag()
    {
        std::vector<WidgetBoundsUpdate> updates;
        for (const auto& item : dragState.items)
            if (!areRectsEqual(item.startBounds, item.currentBounds))
                updates.push_back({ item.widgetId, item.currentBounds });
        dragState = {};
        clearTransientSnapGuides();
        if (!updates.empty())
            document.setWidgetsBounds(updates);
        refreshFromDocument();
    }

    std::vector<WidgetId> CanvasComponent::collectMarqueeHitIds(
        const juce::Rectangle<float>& marqueeBounds) const
    {
        std::vector<WidgetId> hits;
        if (marqueeBounds.isEmpty())
            return hits;

        for (const auto& widget : document.snapshot().widgets)
            if (isWidgetVisibleForCanvas(widget.id) && widget.bounds.intersects(marqueeBounds))
                hits.push_back(widget.id);

        return hits;
    }

    void CanvasComponent::applyMarqueeSelection()
    {
        auto hits = collectMarqueeHitIds(viewToWorldRect(marqueeState.bounds));
        auto next = marqueeState.additive || marqueeState.toggle
            ? document.editorState().selection
            : std::vector<WidgetId>{};

        if (marqueeState.toggle)
        {
            for (const auto id : hits)
            {
                if (const auto it = std::find(next.begin(), next.end(), id); it != next.end())
                    next.erase(it);
                else
                    next.push_back(id);
            }
        }
        else
        {
            for (const auto id : hits)
                if (!containsWidgetId(next, id))
                    next.push_back(id);
        }

        document.setSelection(std::move(next));
        syncSelectionToViews();
    }

    bool CanvasComponent::nudgeSelection(juce::Point<float> deltaWorld)
    {
        std::vector<WidgetBoundsUpdate> updates;
        for (const auto id : document.editorState().selection)
        {
            const auto* widget = findWidgetModel(id);
            if (widget == nullptr || !isWidgetVisibleForCanvas(id) || isWidgetLockedForCanvas(id))
                continue;
            updates.push_back({ id, widget->bounds.translated(deltaWorld.x, deltaWorld.y) });
        }

        if (updates.empty() || !document.setWidgetsBounds(updates))
            return false;
        refreshFromDocument();
        return true;
    }

    std::vector<WidgetId> CanvasComponent::orderedWidgetIdsForCanvas() const
    {
        std::vector<WidgetId> ids;
        ids.reserve(document.snapshot().widgets.size());
        for (const auto& widget : document.snapshot().widgets)
            ids.push_back(widget.id);
        return ids;
    }

    std::vector<juce::Rectangle<float>> CanvasComponent::collectNearbyBoundsForSnap(
        const std::vector<WidgetId>& excludedWidgetIds) const
    {
        std::vector<juce::Rectangle<float>> bounds;
        for (const auto& widget : document.snapshot().widgets)
            if (!containsWidgetId(excludedWidgetIds, widget.id) && isWidgetVisibleForCanvas(widget.id))
                bounds.push_back(widget.bounds);
        return bounds;
    }

    Interaction::SnapRequest CanvasComponent::makeSnapRequest(
        const juce::Rectangle<float>& proposedBounds,
        const std::vector<WidgetId>& excludedWidgetIds) const
    {
        Interaction::SnapRequest req;
        req.proposedBounds = proposedBounds;
        req.nearbyBounds = collectNearbyBoundsForSnap(excludedWidgetIds);
        req.settings = snapSettings;
        return req;
    }

    void CanvasComponent::clearTransientSnapGuides()
    {
        transientSnapGuides.clear();
        snapGuideOverlay.clearGuides();
    }

    void CanvasComponent::updateTransientSnapGuides(const Interaction::SnapResult& snapResult)
    {
        transientSnapGuides.clear();
        std::vector<juce::Line<float>> lines;
        const auto viewport = viewportBounds().toFloat();

        if (snapResult.snapKindX == Interaction::SnapKind::smartAlign && snapResult.guideX.has_value())
        {
            transientSnapGuides.push_back({ true, *snapResult.guideX });
            const auto x = worldToView({ *snapResult.guideX, 0.0f }).x;
            lines.emplace_back(x, viewport.getY(), x, viewport.getBottom());
        }
        if (snapResult.snapKindY == Interaction::SnapKind::smartAlign && snapResult.guideY.has_value())
        {
            transientSnapGuides.push_back({ false, *snapResult.guideY });
            const auto y = worldToView({ 0.0f, *snapResult.guideY }).y;
            lines.emplace_back(viewport.getX(), y, viewport.getRight(), y);
        }

        snapGuideOverlay.setGuides(std::move(lines));
    }

    WidgetComponent* CanvasComponent::findWidgetView(WidgetId id) noexcept
    {
        const auto it = std::find_if(widgetViews.begin(), widgetViews.end(), [id](const auto& view) { return view && view->widgetId() == id; });
        return it == widgetViews.end() ? nullptr : it->get();
    }

    const WidgetComponent* CanvasComponent::findWidgetView(WidgetId id) const noexcept
    {
        const auto it = std::find_if(widgetViews.begin(), widgetViews.end(), [id](const auto& view) { return view && view->widgetId() == id; });
        return it == widgetViews.end() ? nullptr : it->get();
    }

    const WidgetModel* CanvasComponent::findWidgetModel(WidgetId id) const noexcept
    {
        const auto& widgets = document.snapshot().widgets;
        const auto it = std::find_if(widgets.begin(), widgets.end(), [id](const auto& w) { return w.id == id; });
        return it == widgets.end() ? nullptr : &(*it);
    }

    bool CanvasComponent::isWidgetVisibleForCanvas(WidgetId id) const noexcept
    {
        if (const auto* widget = findWidgetModel(id); widget != nullptr)
            return widget->visible;
        return false;
    }

    bool CanvasComponent::isWidgetLockedForCanvas(WidgetId id) const noexcept
    {
        if (const auto* widget = findWidgetModel(id); widget != nullptr)
            return widget->locked;
        return true;
    }

    float CanvasComponent::effectiveOpacityForWidget(WidgetId id) const noexcept
    {
        if (const auto* widget = findWidgetModel(id); widget != nullptr)
            return juce::jlimit(0.0f, 1.0f, widget->opacity);
        return 1.0f;
    }

    std::optional<juce::String> CanvasComponent::extractWidgetLibraryTypeKey(const juce::var& description) const
    {
        const auto* obj = description.getDynamicObject();
        if (obj == nullptr)
            return std::nullopt;

        const auto& props = obj->getProperties();
        if (!props.contains("kind") || props["kind"].toString() != "widgetLibraryType")
            return std::nullopt;
        if (!props.contains("typeKey") || !props["typeKey"].isString())
            return std::nullopt;

        const auto key = props["typeKey"].toString().trim();
        return key.isEmpty() ? std::optional<juce::String>{} : std::optional<juce::String>{ key };
    }

    std::optional<WidgetId> CanvasComponent::resolveActiveLayerId() const
    {
        return activeLayerResolver ? activeLayerResolver() : std::optional<WidgetId>{};
    }

    void CanvasComponent::updateMouseTrackerFromChild(juce::Point<float> localPoint) { setMouseTrackerPoint(localPoint); }
    void CanvasComponent::clearMouseTrackerFromChild() { clearMouseTrackerPoint(); }

    void CanvasComponent::applyWheelZoomAtPoint(juce::Point<float> localPoint,
                                                const juce::MouseWheelDetails& wheel)
    {
        if (!viewportBounds().toFloat().contains(localPoint))
            return;
        const auto delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
        if (std::abs(delta) <= 0.0001f)
            return;
        setZoomAtPoint(zoomLevel * static_cast<float>(std::pow(1.1f, delta * 4.0f)), localPoint);
    }

    void CanvasComponent::handleWidgetMouseDown(WidgetId id,
                                                bool,
                                                const juce::MouseEvent& event)
    {
        setMouseTrackerPoint(event.getEventRelativeTo(this).position);
        clearTransientSnapGuides();
        if (!event.mods.isLeftButtonDown() || isRunMode())
            return;
        if (!isWidgetVisibleForCanvas(id) || isWidgetLockedForCanvas(id))
            return;

        marqueeState = {};
        const auto point = event.getEventRelativeTo(this).position;

        if (event.mods.isCommandDown())
        {
            auto sel = document.editorState().selection;
            if (const auto it = std::find(sel.begin(), sel.end(), id); it != sel.end()) sel.erase(it); else sel.push_back(id);
            document.setSelection(std::move(sel));
            syncSelectionToViews();
            return;
        }

        if (event.mods.isShiftDown())
        {
            auto sel = document.editorState().selection;
            if (!containsWidgetId(sel, id)) sel.push_back(id);
            document.setSelection(std::move(sel));
            syncSelectionToViews();
            beginDragForSelection(id, point);
            return;
        }

        const auto& sel = document.editorState().selection;
        if (sel.size() != 1 || sel.front() != id)
        {
            document.selectSingle(id);
            syncSelectionToViews();
        }

        beginDragForSelection(id, point);
    }

    void CanvasComponent::handleWidgetMouseDrag(WidgetId id, const juce::MouseEvent& event)
    {
        if (dragState.active && dragState.anchorWidgetId == id)
            updateDragPreview(event.getEventRelativeTo(this).position);
    }

    void CanvasComponent::handleWidgetMouseUp(WidgetId id, const juce::MouseEvent*)
    {
        if (dragState.active && dragState.anchorWidgetId == id)
            commitDrag();
    }

    void CanvasComponent::handleCanvasDoubleClick(juce::Point<float>) { clearTransientSnapGuides(); }

    void CanvasComponent::setMouseTrackerPoint(juce::Point<float> localPoint)
    {
        hasMouseLocalPoint = true;
        lastMouseLocalPoint = localPoint;
        repaint();
    }

    void CanvasComponent::clearMouseTrackerPoint()
    {
        if (!hasMouseLocalPoint)
            return;
        hasMouseLocalPoint = false;
        repaint();
    }
}
