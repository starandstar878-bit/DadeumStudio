#include "Teul2/Editor/Canvas/TGraphCanvas.h"

#include "Teul2/Editor/Search/SearchController.h"
#include "Teul2/Editor/Renderers/TNodeRenderer.h"
#include "Teul2/Editor/Renderers/TPortRenderer.h"
#include "Teul2/Editor/Theme/TeulPalette.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace Teul {

TGraphCanvas::TGraphCanvas(TTeulDocument &doc, const TNodeRegistry &registry)
    : document(doc), nodeDescriptors(registry.getAllDescriptors()) {
  setWantsKeyboardFocus(true);

  viewOriginWorld = {doc.meta.canvasOffsetX, doc.meta.canvasOffsetY};
  zoomLevel = doc.meta.canvasZoom;

  if (!std::isfinite(zoomLevel))
    zoomLevel = 1.0f;

  zoomLevel = juce::jlimit(0.1f, 5.0f, zoomLevel);

  rebuildNodeComponents();

  searchOverlay = std::make_unique<SearchOverlay>(*this);
  addAndMakeVisible(searchOverlay.get());
  searchOverlay->setBounds(getLocalBounds());
  searchOverlay->setVisible(false);

  startTimerHz(30);
}

TGraphCanvas::~TGraphCanvas() {
  stopTimer();
  nodeSelectionChangedHandler = {};
  nodePropertiesRequestHandler = {};
  connectionLevelProvider = {};
  portLevelProvider = {};
  bindingSummaryResolver = {};
  externalEndpointAnchorProvider = {};
  document.meta.canvasOffsetX = viewOriginWorld.x;
  document.meta.canvasOffsetY = viewOriginWorld.y;
  document.meta.canvasZoom = zoomLevel;
}

void TGraphCanvas::setConnectionLevelProvider(ConnectionLevelProvider provider) {
  connectionLevelProvider = std::move(provider);
}

void TGraphCanvas::setPortLevelProvider(PortLevelProvider provider) {
  portLevelProvider = std::move(provider);
}

void TGraphCanvas::setBindingSummaryResolver(BindingSummaryResolver resolver) {
  bindingSummaryResolver = std::move(resolver);
}

void TGraphCanvas::setExternalDropZoneProvider(
    ExternalDropZoneProvider provider) {
  externalDropZoneProvider = std::move(provider);
}

void TGraphCanvas::setExternalEndpointAnchorProvider(
    ExternalDropZoneProvider provider) {
  externalEndpointAnchorProvider = std::move(provider);
}

void TGraphCanvas::setExternalConnectionCommitHandler(
    ExternalConnectionCommitHandler handler) {
  externalConnectionCommitHandler = std::move(handler);
}

void TGraphCanvas::setExternalDragTargetChangedHandler(
    ExternalDragTargetChangedHandler handler) {
  externalDragTargetChangedHandler = std::move(handler);
}

void TGraphCanvas::setRuntimeOverlayState(const RuntimeOverlayState &state) {
  runtimeOverlayState = state;
  repaint();
}

void TGraphCanvas::setRuntimeViewOptions(const RuntimeViewOptions &options) {
  runtimeViewOptions = options;
  repaint();
}

void TGraphCanvas::setRuntimeHeatmapEnabled(bool enabled) {
  if (runtimeViewOptions.heatmapEnabled == enabled)
    return;

  runtimeViewOptions.heatmapEnabled = enabled;
  pushStatusHint(enabled ? "Heatmap on: node cost accents visible"
                         : "Heatmap off");
}
void TGraphCanvas::setLiveProbeEnabled(bool enabled) {
  if (runtimeViewOptions.liveProbeEnabled == enabled)
    return;

  runtimeViewOptions.liveProbeEnabled = enabled;
  pushStatusHint(enabled ? "Probe on: edge meters and selected readouts visible"
                         : "Probe off");
}
void TGraphCanvas::setDebugOverlayEnabled(bool enabled) {
  if (runtimeViewOptions.debugOverlayEnabled == enabled)
    return;

  runtimeViewOptions.debugOverlayEnabled = enabled;
  pushStatusHint(enabled ? "Overlay on: runtime card visible"
                         : "Overlay off");
}
float TGraphCanvas::getPortLevel(PortId portId) const {
  if (!portLevelProvider)
    return 0.0f;

  return juce::jlimit(0.0f, 1.0f, portLevelProvider(portId));
}

void TGraphCanvas::setNodePropertiesRequestHandler(
    NodePropertiesRequestHandler handler) {
  nodePropertiesRequestHandler = std::move(handler);
}

void TGraphCanvas::setNodeSelectionChangedHandler(
    NodeSelectionChangedHandler handler) {
  nodeSelectionChangedHandler = std::move(handler);
}

void TGraphCanvas::setFrameSelectionChangedHandler(
    FrameSelectionChangedHandler handler) {
  frameSelectionChangedHandler = std::move(handler);
}

juce::Point<float> TGraphCanvas::getViewCenter() const {
  return {getWidth() * 0.5f, getHeight() * 0.5f};
}

void TGraphCanvas::focusNode(NodeId nodeId) {
  const TNode *node = document.findNode(nodeId);
  if (node == nullptr)
    return;

  viewOriginWorld.x = node->x - (getWidth() * 0.5f) / zoomLevel;
  viewOriginWorld.y = node->y - (getHeight() * 0.5f) / zoomLevel;
  updateChildPositions();
  repaint();
}

bool TGraphCanvas::ensureNodeVisible(NodeId nodeId, float paddingView) {
  auto *nodeComponent = findNodeComponent(nodeId);
  if (nodeComponent == nullptr || !nodeComponent->isVisible())
    return false;

  const auto safeArea = getLocalBounds().toFloat().reduced(paddingView);
  if (safeArea.getWidth() <= 0.0f || safeArea.getHeight() <= 0.0f)
    return false;

  const auto nodeBounds = getNodeBoundsInView(*nodeComponent);
  juce::Point<float> deltaView;

  if (nodeBounds.getRight() > safeArea.getRight())
    deltaView.x = nodeBounds.getRight() - safeArea.getRight();
  else if (nodeBounds.getX() < safeArea.getX())
    deltaView.x = nodeBounds.getX() - safeArea.getX();

  if (nodeBounds.getBottom() > safeArea.getBottom())
    deltaView.y = nodeBounds.getBottom() - safeArea.getBottom();
  else if (nodeBounds.getY() < safeArea.getY())
    deltaView.y = nodeBounds.getY() - safeArea.getY();

  if (deltaView.x == 0.0f && deltaView.y == 0.0f)
    return false;

  viewOriginWorld += deltaView / zoomLevel;
  updateChildPositions();
  repaint();
  return true;
}

void TGraphCanvas::rebuildNodeComponents() {
  nodeComponents.clear();

  for (const auto &node : document.nodes) {
    const TNodeDescriptor *desc = findDescriptorByTypeKey(node.typeKey);

    auto comp = std::make_unique<TNodeComponent>(*this, node.nodeId, desc);
    addAndMakeVisible(comp.get());
    nodeComponents.push_back(std::move(comp));
  }

  if (searchOverlay != nullptr)
    searchOverlay->toFront(false);

  updateChildPositions();
  syncNodeSelectionToComponents();
  repaint();
}

void TGraphCanvas::updateChildPositions() {
  for (auto &nodeComponent : nodeComponents) {
    const TNode *nodeData = document.findNode(nodeComponent->getNodeId());
    if (!nodeData)
      continue;

    const bool hidden = isNodeHiddenByCollapsedFrame(*nodeData);
    nodeComponent->setVisible(!hidden);
    if (hidden)
      continue;

    nodeComponent->setViewScale(zoomLevel);
    nodeComponent->setTopLeftPosition(
        worldToView(juce::Point<float>(nodeData->x, nodeData->y)).roundToInt());
    nodeComponent->setTransform(juce::AffineTransform());
  }

  recalcMiniMapCache();
}

void TGraphCanvas::paint(juce::Graphics &g) {
  g.fillAll(TeulPalette::CanvasBackground());
  drawInfiniteGrid(g);
  drawFrames(g);
  drawLibraryDropPreview(g);
}

void TGraphCanvas::paintOverChildren(juce::Graphics &g) {
  drawSelectionOverlay(g);
}

void TGraphCanvas::paintConnectionLayer(juce::Graphics &g) {
  drawConnections(g);
}

void TGraphCanvas::paintHudLayer(juce::Graphics &g) {
  drawRuntimeOverlay(g);
  drawMiniMap(g);
  drawZoomIndicator(g);
  drawStatusHint(g);
}

void TGraphCanvas::resized() {
  updateChildPositions();
  if (searchOverlay != nullptr)
    searchOverlay->setBounds(getLocalBounds());
}

void TGraphCanvas::mouseDown(const juce::MouseEvent &event) {
  grabKeyboardFocus();

  if (event.mods.isRightButtonDown()) {
    const int frameId = hitTestFrame(event.position);
    if (frameId != 0) {
      selectOnlyFrame(frameId);
      showFrameContextMenu(
          frameId, event.position,
          localPointToGlobal(event.position.roundToInt()).toFloat());
      return;
    }

    showCanvasContextMenu(event.position,
                          localPointToGlobal(event.position.roundToInt()).toFloat());
    return;
  }

  if (miniMapRectView.contains(event.position)) {
    const auto world = miniMapToWorld(event.position);

    if (miniMapViewportRectView.contains(event.position)) {
      miniMapDragState.active = true;
      miniMapDragState.worldOffset = world - viewOriginWorld;
    } else {
      viewOriginWorld = {world.x - (getWidth() * 0.5f) / zoomLevel,
                         world.y - (getHeight() * 0.5f) / zoomLevel};
      updateChildPositions();
      repaint();
    }
    return;
  }

  const bool isPrimaryCanvasInteraction =
      event.mods.isLeftButtonDown() && !event.mods.isMiddleButtonDown();
  if (isPrimaryCanvasInteraction && canvasPrimaryInteractionHandler != nullptr)
    canvasPrimaryInteractionHandler();

  if (event.mods.isAltDown() && event.mods.isLeftButtonDown()) {
    const ConnectionId hit = hitTestConnection(event.position);
    if (hit != kInvalidConnectionId) {
      deleteConnectionWithAnimation(hit, event.position, {0.0f, 140.0f});
      connectionBreakDragArmed = false;
      pressedConnectionId = kInvalidConnectionId;
      return;
    }
  }

  if (event.mods.isLeftButtonDown() && !event.mods.isMiddleButtonDown()) {
    const ConnectionId hit = hitTestConnection(event.position);
    if (hit != kInvalidConnectionId) {
      clearFrameSelection();
      clearNodeSelection();
      selectedConnectionId = hit;
      pressedConnectionId = hit;
      pressedConnectionPointView = event.position;
      connectionBreakDragArmed = true;
      repaint();
      return;
    }

    if (selectedConnectionId != kInvalidConnectionId) {
      selectedConnectionId = kInvalidConnectionId;
      repaint();
    }

    const int frameId = hitTestFrame(event.position);
    if (frameId != 0) {
      selectOnlyFrame(frameId);
      if (isPointInFrameTitle(frameId, event.position))
        startFrameDrag(frameId, event.position);
      return;
    }
  }

  const bool isPanTrigger =
      event.mods.isMiddleButtonDown() || event.mods.isAltDown() ||
      juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);

  if (isPanTrigger) {
    panState.active = true;
    panState.startMouseView = event.position;
    panState.startViewOriginWorld = viewOriginWorld;
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    return;
  }

  if (event.mods.isLeftButtonDown()) {
    const bool additiveSelection = event.mods.isShiftDown() ||
                                   event.mods.isCtrlDown() ||
                                   event.mods.isCommandDown();
    if (additiveSelection)
      clearFrameSelection();
    else
      clearAllSelection();

    marqueeState.active = true;
    marqueeState.additive = additiveSelection;
    marqueeState.startView = event.position;
    marqueeState.rectView = {event.position.x, event.position.y, 0.0f, 0.0f};
    marqueeState.baseSelection =
        marqueeState.additive ? selectedNodeIds : std::vector<NodeId>{};
  }
}

void TGraphCanvas::mouseDrag(const juce::MouseEvent &event) {
  if (miniMapDragState.active) {
    const auto world = miniMapToWorld(event.position);
    viewOriginWorld = world - miniMapDragState.worldOffset;
    updateChildPositions();
    repaint();
    return;
  }

  if (connectionBreakDragArmed && pressedConnectionId != kInvalidConnectionId) {
    const juce::Point<float> delta = event.position - pressedConnectionPointView;
    if (delta.getDistanceFromOrigin() > 6.0f) {
      deleteConnectionWithAnimation(pressedConnectionId, event.position,
                                    delta * 6.0f);
      connectionBreakDragArmed = false;
      pressedConnectionId = kInvalidConnectionId;
      return;
    }
  }

  if (panState.active) {
    const juce::Point<float> deltaView = event.position - panState.startMouseView;
    const juce::Point<float> deltaWorld = deltaView / zoomLevel;

    viewOriginWorld = panState.startViewOriginWorld - deltaWorld;
    updateChildPositions();
    repaint();
    return;
  }

  if (frameDragState.active) {
    updateFrameDrag(event.position);
    return;
  }

  if (marqueeState.active) {
    marqueeState.rectView = juce::Rectangle<float>::leftTopRightBottom(
        juce::jmin(marqueeState.startView.x, event.position.x),
        juce::jmin(marqueeState.startView.y, event.position.y),
        juce::jmax(marqueeState.startView.x, event.position.x),
        juce::jmax(marqueeState.startView.y, event.position.y));
    repaint();
  }
}

void TGraphCanvas::mouseUp(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);

  connectionBreakDragArmed = false;
  pressedConnectionId = kInvalidConnectionId;

  if (miniMapDragState.active)
    miniMapDragState = MiniMapDragState{};

  if (panState.active) {
    panState.active = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
  }

  if (frameDragState.active)
    endFrameDrag();

  if (marqueeState.active) {
    applyMarqueeSelection();
    marqueeState.active = false;
    repaint();
  }
}

void TGraphCanvas::mouseWheelMove(const juce::MouseEvent &event,
                                  const juce::MouseWheelDetails &wheel) {
  if (event.mods.isCtrlDown() || event.mods.isCommandDown() ||
      event.mods.isAltDown()) {
    const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (std::abs(delta) <= 0.0001f)
      return;

    const float nextZoom =
        zoomLevel * static_cast<float>(std::pow(1.1f, delta * 4.0f));
    setZoomLevel(nextZoom, event.position);
  } else {
    const float panSpeedPixels = 50.0f;
    const float deltaXWorld = (wheel.deltaX * panSpeedPixels) / zoomLevel;
    const float deltaYWorld = (wheel.deltaY * panSpeedPixels) / zoomLevel;

    viewOriginWorld.x -= deltaXWorld;
    viewOriginWorld.y -= deltaYWorld;
    updateChildPositions();
    repaint();
  }
}

bool TGraphCanvas::keyPressed(const juce::KeyPress &key) {
  if (key == juce::KeyPress::spaceKey) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    return true;
  }

  const bool ctrlOrCmd = key.getModifiers().isCtrlDown() ||
                         key.getModifiers().isCommandDown();
  const bool alt = key.getModifiers().isAltDown();
  const auto ch =
      juce::CharacterFunctions::toLowerCase(key.getTextCharacter());

  if (ctrlOrCmd && ch == 'p') {
    showCommandPaletteOverlay();
    return true;
  }

  if (ctrlOrCmd && ch == 'f') {
    showNodeSearchOverlay();
    return true;
  }

  if (key == juce::KeyPress::tabKey) {
    showQuickAddPrompt(getViewCenter());
    return true;
  }

  if (key == juce::KeyPress::escapeKey) {
    if (wireDragState.active)
      cancelConnectionDrag();
    return true;
  }

  if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
    if (selectedConnectionId != kInvalidConnectionId) {
      const bool confirmed = juce::AlertWindow::showOkCancelBox(
          juce::MessageBoxIconType::WarningIcon, "Delete Connection",
          "Delete selected connection? Ctrl+Z will undo.", "OK", "Cancel", nullptr,
          nullptr);
      if (confirmed)
        deleteConnectionWithAnimation(selectedConnectionId, getViewCenter(),
                                      {0.0f, 120.0f});
      return true;
    }

    deleteSelectionWithPrompt();
    return true;
  }

  if (ctrlOrCmd && !key.getModifiers().isShiftDown() && ch == 'z') {
    if (document.undo()) {
      rebuildNodeComponents();
      pushStatusHint("Undo");
    }
    return true;
  }

  if ((ctrlOrCmd && key.getModifiers().isShiftDown() && ch == 'z') ||
      (ctrlOrCmd && ch == 'y')) {
    if (document.redo()) {
      rebuildNodeComponents();
      pushStatusHint("Redo");
    }
    return true;
  }

  if (ctrlOrCmd && ch == 'd') {
    duplicateSelection();
    return true;
  }

  if (ch == 'b') {
    toggleBypassSelection();
    return true;
  }

  if (ctrlOrCmd && ch == 'l') {
    alignSelectionLeft();
    return true;
  }

  if (ctrlOrCmd && ch == 't') {
    alignSelectionTop();
    return true;
  }

  if (ctrlOrCmd && alt && ch == 'h') {
    distributeSelectionHorizontally();
    return true;
  }

  if (ctrlOrCmd && alt && ch == 'v') {
    distributeSelectionVertically();
    return true;
  }

  return false;
}

bool TGraphCanvas::keyStateChanged(bool isKeyDown) {
  juce::ignoreUnused(isKeyDown);

  if (!juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)) {
    if (!panState.active)
      setMouseCursor(juce::MouseCursor::NormalCursor);
  } else {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
  }

  return false;
}

void TGraphCanvas::setZoomLevel(float newZoom,
                                juce::Point<float> anchorPosView) {
  newZoom = juce::jlimit(0.1f, 5.0f, newZoom);

  if (juce::exactlyEqual(newZoom, zoomLevel))
    return;

  const juce::Point<float> anchorWorld = viewToWorld(anchorPosView);

  zoomLevel = newZoom;

  viewOriginWorld.x = anchorWorld.x - (anchorPosView.x / zoomLevel);
  viewOriginWorld.y = anchorWorld.y - (anchorPosView.y / zoomLevel);

  updateChildPositions();
  repaint();
}

juce::Point<float> TGraphCanvas::viewToWorld(juce::Point<float> viewPos) const {
  return viewOriginWorld + (viewPos / zoomLevel);
}

juce::Point<float> TGraphCanvas::worldToView(juce::Point<float> worldPos) const {
  return (worldPos - viewOriginWorld) * zoomLevel;
}

juce::Rectangle<float>
TGraphCanvas::getNodeBoundsInView(const TNodeComponent &nodeComponent) const {
  return nodeComponent.getBounds().toFloat();
}

void TGraphCanvas::timerCallback() {
  flowPhase += 0.04f;
  if (flowPhase >= 1.0f)
    flowPhase -= std::floor(flowPhase);

  if (disconnectAnimation.active) {
    const float dt = 1.0f / 30.0f;
    disconnectAnimation.velocityViewPerSecond.y += 1500.0f * dt;
    disconnectAnimation.loosePosView +=
        disconnectAnimation.velocityViewPerSecond * dt;

    disconnectAnimation.alpha =
        juce::jmax(0.0f, disconnectAnimation.alpha - 2.4f * dt);
    if (disconnectAnimation.alpha <= 0.01f)
      disconnectAnimation.active = false;
  }

  if (portLevelProvider != nullptr) {
    for (auto &nodeComponent : nodeComponents) {
      if (nodeComponent != nullptr && nodeComponent->isVisible())
        nodeComponent->repaint();
    }
  }

  if (connectionLevelProvider != nullptr || !document.connections.empty() ||
      wireDragState.active || disconnectAnimation.active) {
    repaint();
  }
}

void TGraphCanvas::openQuickAddAt(juce::Point<float> pointView) {
  showQuickAddPrompt(pointView);
}

void TGraphCanvas::openNodeSearchPrompt() { showNodeSearchOverlay(); }

void TGraphCanvas::openCommandPalette() { showCommandPaletteOverlay(); }

void TGraphCanvas::mouseDoubleClick(const juce::MouseEvent &event) {
  if (event.mods.isLeftButtonDown())
    showQuickAddPrompt(event.position);
}

void TGraphCanvas::drawInfiniteGrid(juce::Graphics &g) {
  g.saveState();

  const float baseGridSize = 40.0f;
  const float scaledGridSize = baseGridSize * zoomLevel;

  if (scaledGridSize < 5.0f) {
    g.restoreState();
    return;
  }

  const auto localBounds = getLocalBounds().toFloat();
  juce::Rectangle<float> worldBounds(viewOriginWorld.x, viewOriginWorld.y,
                                     localBounds.getWidth() / zoomLevel,
                                     localBounds.getHeight() / zoomLevel);

  const float startX =
      std::floor(worldBounds.getX() / baseGridSize) * baseGridSize;
  const float startY =
      std::floor(worldBounds.getY() / baseGridSize) * baseGridSize;

  g.setColour(TeulPalette::GridDot());

  for (float wx = startX; wx < worldBounds.getRight(); wx += baseGridSize) {
    for (float wy = startY; wy < worldBounds.getBottom(); wy += baseGridSize) {
      const float vx = (wx - viewOriginWorld.x) * zoomLevel;
      const float vy = (wy - viewOriginWorld.y) * zoomLevel;
      g.fillRect(vx - 1.0f, vy - 1.0f, 2.0f, 2.0f);
    }
  }

  g.restoreState();
}

void TGraphCanvas::drawZoomIndicator(juce::Graphics &g) {
  const juce::String zoomText =
      juce::String(juce::roundToInt(zoomLevel * 100.0f)) + "%";

  auto area =
      getLocalBounds().removeFromBottom(32).removeFromRight(96).reduced(8, 6);
  g.setColour(TeulPalette::HudBackgroundAlt().withAlpha(0.72f));
  g.fillRoundedRectangle(area.toFloat(), 7.0f);
  g.setColour(TeulPalette::HudStroke().withAlpha(0.36f));
  g.drawRoundedRectangle(area.toFloat(), 7.0f, 1.0f);

  auto content = area.reduced(8, 4);
  g.setColour(TeulPalette::PanelTextMuted().withAlpha(0.48f));
  g.setFont(9.0f);
  g.drawText("Zoom", content.removeFromTop(10),
             juce::Justification::centredLeft, false);
  g.setColour(TeulPalette::PanelTextStrong().withAlpha(0.94f));
  g.setFont(juce::FontOptions(11.8f, juce::Font::bold));
  g.drawText(zoomText, content, juce::Justification::centredLeft, false);
}

void TGraphCanvas::drawMiniMap(juce::Graphics &g) {
  const float miniW = 172.0f;
  const float miniH = 104.0f;
  const float margin = 10.0f;

  miniMapRectView = {getWidth() - miniW - margin, getHeight() - miniH - margin,
                     miniW, miniH};
  recalcMiniMapCache();

  const auto miniMapInnerRect = miniMapRectView.reduced(1.0f);
  const float sx = miniMapInnerRect.getWidth() / miniMapWorldBounds.getWidth();
  const float sy = miniMapInnerRect.getHeight() / miniMapWorldBounds.getHeight();

  auto worldToMini = [&](juce::Point<float> p) {
    return juce::Point<float>(
        miniMapInnerRect.getX() + (p.x - miniMapWorldBounds.getX()) * sx,
        miniMapInnerRect.getY() + (p.y - miniMapWorldBounds.getY()) * sy);
  };

  g.setColour(TeulPalette::HudBackgroundAlt().withAlpha(0.82f));
  g.fillRoundedRectangle(miniMapRectView, 6.0f);
  g.setColour(TeulPalette::HudStroke().withAlpha(0.60f));
  g.drawRoundedRectangle(miniMapRectView, 6.0f, 1.0f);

  g.saveState();
  g.reduceClipRegion(miniMapInnerRect.toNearestInt());

  for (const auto &frame : document.frames) {
    auto topLeft = worldToMini({frame.x, frame.y});
    auto bottomRight = worldToMini({frame.x + frame.width, frame.y + frame.height});
    juce::Rectangle<float> rect(
        juce::jmin(topLeft.x, bottomRight.x),
        juce::jmin(topLeft.y, bottomRight.y),
        std::abs(bottomRight.x - topLeft.x),
        std::abs(bottomRight.y - topLeft.y));
    g.setColour(juce::Colour(frame.colorArgb).withAlpha(0.24f));
    g.fillRect(rect);
  }

  for (const auto &node : document.nodes) {
    auto topLeft = worldToMini({node.x, node.y});
    g.setColour(isNodeSelected(node.nodeId)
                    ? TeulPalette::PanelTextStrong().withAlpha(0.82f)
                    : TeulPalette::AccentSlate().withAlpha(0.72f));
    g.fillRect(topLeft.x, topLeft.y, 160.0f * sx, 90.0f * sy);
  }

  const float safeZoom = juce::jmax(zoomLevel, 0.0001f);
  const juce::Rectangle<float> viewportWorld(viewOriginWorld.x, viewOriginWorld.y,
                                             getWidth() / safeZoom,
                                             getHeight() / safeZoom);
  const auto viewTopLeft = worldToMini(viewportWorld.getTopLeft());
  const auto viewBottomRight = worldToMini(viewportWorld.getBottomRight());
  miniMapViewportRectView =
      juce::Rectangle<float>(juce::jmin(viewTopLeft.x, viewBottomRight.x),
                             juce::jmin(viewTopLeft.y, viewBottomRight.y),
                             std::abs(viewBottomRight.x - viewTopLeft.x),
                             std::abs(viewBottomRight.y - viewTopLeft.y))
          .getIntersection(miniMapInnerRect);

  g.setColour(TeulPalette::PanelText().withAlpha(0.78f));
  g.drawRect(miniMapViewportRectView, 1.1f);
  g.restoreState();
}

void TGraphCanvas::pushStatusHint(const juce::String &text) {
  statusHintText = text;
  statusHintAlpha = 1.0f;
  repaint();
}

bool TGraphCanvas::isNodeInsideFrame(const TNode &node,
                                     const TFrameRegion &frame) const {
  if (frame.membershipExplicit)
    return frame.containsNode(node.nodeId);

  const juce::Rectangle<float> frameRect(frame.x, frame.y, frame.width,
                                         frame.height);
  const juce::Rectangle<float> nodeRect(node.x, node.y, 160.0f, 90.0f);
  return frameRect.intersects(nodeRect);
}

TGraphCanvas::getFrameMemberBoundsWorld(const TFrameRegion &frame,
                                        float paddingWorld) const {
  juce::Rectangle<float> bounds;
  bool hasBounds = false;

  for (const auto memberNodeId : frame.memberNodeIds) {
    const auto *node = document.findNode(memberNodeId);
    if (node == nullptr)
      continue;

    const juce::Rectangle<float> nodeRect(node->x, node->y, 160.0f, 90.0f);
    bounds = hasBounds ? bounds.getUnion(nodeRect) : nodeRect;
    hasBounds = true;
  }

  if (!hasBounds)
    return {frame.x, frame.y, frame.width, frame.height};

  return bounds.expanded(paddingWorld, paddingWorld);
}

bool TGraphCanvas::isNodeHiddenByCollapsedFrame(const TNode &node) const {
  for (const auto &frame : document.frames) {
    if (frame.collapsed && isNodeInsideFrame(node, frame))
      return true;
  }
  return false;
}

bool TGraphCanvas::isNodeMoveLocked(NodeId nodeId) const {
  const TNode *node = document.findNode(nodeId);
  if (node == nullptr)
    return false;

  for (const auto &frame : document.frames) {
    if (frame.locked && isNodeInsideFrame(*node, frame))
      return true;
  }

  return false;
}

void TGraphCanvas::drawFrames(juce::Graphics &g) {
  const float titleHWorld = 24.0f / zoomLevel;

  for (const auto &frame : document.frames) {
    const float drawH = frame.collapsed ? titleHWorld : frame.height;

    const auto topLeft = worldToView({frame.x, frame.y});
    const auto bottomRight = worldToView({frame.x + frame.width,
                                          frame.y + drawH});

    juce::Rectangle<float> rect(
        juce::jmin(topLeft.x, bottomRight.x),
        juce::jmin(topLeft.y, bottomRight.y),
        std::abs(bottomRight.x - topLeft.x),
        std::abs(bottomRight.y - topLeft.y));

    juce::Colour base = juce::Colour(frame.colorArgb);
    if (base.isTransparent())
      base = TeulPalette::AccentBlue().withAlpha(0.20f);

    g.setColour(base.withMultipliedAlpha(frame.locked ? 0.4f : 0.54f));
    g.fillRoundedRectangle(rect, 7.0f);

    g.setColour(base.withMultipliedAlpha(0.88f));
    g.drawRoundedRectangle(rect, 7.0f, 1.0f);

    if (frame.frameId == selectedFrameId) {
      g.setColour(base.brighter(0.55f).withAlpha(0.98f));
      g.drawRoundedRectangle(rect.expanded(1.0f), 8.0f, 1.8f);
    }

    auto titleRect = rect.removeFromTop(22.0f);
    g.setColour(base.brighter(0.18f).withAlpha(0.46f));
    g.fillRoundedRectangle(titleRect, 6.0f);

    juce::String title = frame.title;
    if (frame.logicalGroup) {
      title += " [G";
      if (frame.membershipExplicit)
        title += ":" + juce::String((int)frame.memberNodeIds.size());
      title += "]";
    }
    if (frame.locked)
      title += " [L]";
    if (frame.collapsed)
      title += " [C]";

    g.setColour(TeulPalette::PanelTextStrong().withAlpha(0.86f));
    g.setFont(10.0f);
    g.drawText(title, titleRect.toNearestInt().reduced(6, 0),
               juce::Justification::centredLeft, false);
  }
}

void TGraphCanvas::drawLibraryDropPreview(juce::Graphics &g) {
  if (!libraryDropPreview.active)
    return;

  const auto previewTypeKey =
      juce::String::fromUTF8(libraryDropPreview.typeKeyUtf8.c_str());
  const auto *desc = findDescriptorByTypeKey(previewTypeKey);
  const juce::String title =
      desc != nullptr ? desc->displayName : previewTypeKey;
  const juce::String subtitle =
      desc != nullptr ? desc->category + " / " + desc->typeKey
                      : previewTypeKey;

  auto bubble = juce::Rectangle<float>(libraryDropPreview.pointView.x + 10.0f,
                                       libraryDropPreview.pointView.y - 6.0f,
                                       204.0f, 36.0f);
  const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
  bubble.setPosition(
      juce::jlimit(bounds.getX(), bounds.getRight() - bubble.getWidth(),
                   bubble.getX()),
      juce::jlimit(bounds.getY(), bounds.getBottom() - bubble.getHeight(),
                   bubble.getY()));

  g.setColour(TeulPalette::PanelBackgroundRaised().withAlpha(0.92f));
  g.fillRoundedRectangle(bubble, 7.0f);
  g.setColour(TeulPalette::AccentSky().withAlpha(0.88f));
  g.drawRoundedRectangle(bubble, 7.0f, 1.0f);

  g.setColour(TeulPalette::AccentBlue().brighter(0.18f).withAlpha(0.88f));
  g.drawEllipse(libraryDropPreview.pointView.x - 4.5f,
                libraryDropPreview.pointView.y - 4.5f, 9.0f, 9.0f, 1.15f);
  g.fillEllipse(libraryDropPreview.pointView.x - 1.75f,
                libraryDropPreview.pointView.y - 1.75f, 3.5f, 3.5f);

  auto textArea = bubble.toNearestInt().reduced(9, 5);
  g.setColour(TeulPalette::PanelTextStrong().withAlpha(0.94f));
  g.setFont(11.0f);
  g.drawText(title, textArea.removeFromTop(13), juce::Justification::centredLeft,
             false);
  g.setColour(TeulPalette::PanelTextMuted().withAlpha(0.50f));
  g.setFont(9.2f);
  g.drawText(subtitle, textArea, juce::Justification::centredLeft, false);
}

void TGraphCanvas::drawSelectionOverlay(juce::Graphics &g) {
  if (marqueeState.active) {
    g.setColour(TeulPalette::AccentSky().withAlpha(0.34f));
    g.fillRect(marqueeState.rectView);
    g.setColour(TeulPalette::AccentSky());
    g.drawRect(marqueeState.rectView, 1.2f);
  }

  if (snapGuideState.xActive) {
    const float x = worldToView({snapGuideState.worldX, 0.0f}).x;
    g.setColour(TeulPalette::AccentGold().withAlpha(0.54f));
    g.drawLine(x, 0.0f, x, (float)getHeight(), 1.0f);
  }

  if (snapGuideState.yActive) {
    const float y = worldToView({0.0f, snapGuideState.worldY}).y;
    g.setColour(TeulPalette::AccentGold().withAlpha(0.54f));
    g.drawLine(0.0f, y, (float)getWidth(), y, 1.0f);
  }
}

void TGraphCanvas::drawRuntimeOverlay(juce::Graphics &g) {
  if (!runtimeViewOptions.debugOverlayEnabled)
    return;

  const bool hasRuntimeData = runtimeOverlayState.blockSize > 0 ||
                              runtimeOverlayState.activeGeneration > 0 ||
                              runtimeOverlayState.activeNodeCount > 0;
  if (!hasRuntimeData)
    return;

  auto area =
      getLocalBounds().removeFromBottom(74).removeFromLeft(288).reduced(10);
  if (area.getWidth() < 196 || area.getHeight() < 56)
    return;

  g.setGradientFill(juce::ColourGradient(TeulPalette::HudBackground().withAlpha(0.78f),
                                         (float)area.getCentreX(),
                                         (float)area.getY(),
                                         TeulPalette::HudBackgroundAlt().withAlpha(0.68f),
                                         (float)area.getCentreX(),
                                         (float)area.getBottom(), false));
  g.fillRoundedRectangle(area.toFloat(), 10.0f);
  g.setColour(TeulPalette::HudStroke().withAlpha(0.42f));
  g.drawRoundedRectangle(area.toFloat(), 10.0f, 1.0f);

  auto content = area.reduced(9, 7);
  auto titleRow = content.removeFromTop(10);
  auto primaryRow = content.removeFromTop(13);
  auto detailRow = content.removeFromTop(12);
  auto viewRow = content.removeFromTop(11);
  content.removeFromTop(1);
  auto badgeRow = content.removeFromTop(14);

  const juce::String primaryText = juce::String::formatted(
      "%.1f kHz  |  %d blk  |  %d in / %d out  |  CPU %.1f%%",
      runtimeOverlayState.sampleRate * 0.001,
      runtimeOverlayState.blockSize,
      runtimeOverlayState.inputChannels,
      runtimeOverlayState.outputChannels,
      runtimeOverlayState.cpuLoadPercent);
  const juce::String detailText = juce::String::formatted(
      "Nodes %d  |  Buffers %d  |  Gen %llu  |  Pending %llu",
      runtimeOverlayState.activeNodeCount,
      runtimeOverlayState.allocatedPortChannels,
      static_cast<unsigned long long>(runtimeOverlayState.activeGeneration),
      static_cast<unsigned long long>(runtimeOverlayState.pendingGeneration));
  const juce::String viewText =
      "Views  " + juce::String(runtimeViewOptions.heatmapEnabled ? "Heat on" : "Heat off") +
      "  |  " +
      juce::String(runtimeViewOptions.liveProbeEnabled ? "Probe on" : "Probe off") +
      "  |  Overlay on";

  g.setColour(TeulPalette::PanelTextFaint().withAlpha(0.44f));
  g.setFont(8.8f);
  g.drawText("Runtime Overlay", titleRow, juce::Justification::centredLeft,
             false);

  g.setColour(TeulPalette::PanelTextStrong().withAlpha(0.91f));
  g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
  g.drawText(primaryText, primaryRow, juce::Justification::centredLeft, false);

  g.setColour(TeulPalette::PanelTextMuted().withAlpha(0.50f));
  g.setFont(8.9f);
  g.drawText(detailText, detailRow, juce::Justification::centredLeft, false);
  g.setColour(TeulPalette::PanelTextFaint().withAlpha(0.44f));
  g.drawText(viewText, viewRow, juce::Justification::centredLeft, false);

  int badgeX = badgeRow.getX();
  auto drawBadge = [&](const juce::String &text, juce::Colour colour) {
    if (text.isEmpty())
      return;

    const int width = juce::jlimit(56, 148, 20 + text.length() * 6);
    if (badgeX + width > badgeRow.getRight())
      return;

    juce::Rectangle<int> badge(badgeX, badgeRow.getY(), width,
                               badgeRow.getHeight());
    badgeX += width + 5;

    g.setColour(colour.withAlpha(0.14f));
    g.fillRoundedRectangle(badge.toFloat(), 7.0f);
    g.setColour(colour.withAlpha(0.70f));
    g.drawRoundedRectangle(badge.toFloat(), 7.0f, 1.0f);
    g.setColour(colour.brighter(0.10f));
    g.setFont(8.8f);
    g.drawText(text, badge, juce::Justification::centred, false);
  };

  bool drewBadge = false;
  if (runtimeOverlayState.rebuildPending) {
    drawBadge("Deferred Apply", juce::Colour(0xfff59e0b));
    drewBadge = true;
  }
  if (runtimeOverlayState.smoothingActiveCount > 0) {
    drawBadge("Smooth " + juce::String(runtimeOverlayState.smoothingActiveCount),
              TeulPalette::AccentSky());
    drewBadge = true;
  }
  if (runtimeOverlayState.xrunDetected) {
    drawBadge("XRUN", juce::Colour(0xffef4444));
    drewBadge = true;
  }
  if (runtimeOverlayState.clipDetected) {
    drawBadge("Clip", juce::Colour(0xfff97316));
    drewBadge = true;
  }
  if (runtimeOverlayState.denormalDetected) {
    drawBadge("Denormal", juce::Colour(0xffeab308));
    drewBadge = true;
  }
  if (runtimeOverlayState.mutedFallbackActive) {
    drawBadge("Muted Fallback", juce::Colour(0xff94a3b8));
    drewBadge = true;
  }
  if (!drewBadge)
    drawBadge("Stable", juce::Colour(0xff22c55e));
}

void TGraphCanvas::drawStatusHint(juce::Graphics &g) {
  const auto dragHint = currentDragStatusHint();
  const bool showDragHint = dragHint.isNotEmpty();
  const auto &text = showDragHint ? dragHint : statusHintText;
  const float alpha = showDragHint ? 1.0f : statusHintAlpha;

  if (alpha <= 0.01f || text.isEmpty())
    return;

  auto area = getLocalBounds().removeFromTop(26).reduced(10, 4);
  area.setWidth(juce::jmin(332, area.getWidth()));

  g.setColour(TeulPalette::HudBackgroundAlt().withAlpha(0.78f).withAlpha(alpha));
  g.fillRoundedRectangle(area.toFloat(), 6.0f);
  g.setColour(TeulPalette::HudStroke().withAlpha(0.42f).withAlpha(alpha));
  g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);

  g.setColour(TeulPalette::PanelTextStrong().withAlpha(alpha * 0.94f));
  g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  g.drawText(text, area.reduced(8, 0),
             juce::Justification::centredLeft, false);
}

void TGraphCanvas::recalcMiniMapCache() {
  const float safeZoom = juce::jmax(zoomLevel, 0.0001f);
  const float viewportWidthWorld =
      getWidth() > 0 ? (float)getWidth() / safeZoom : 800.0f;
  const float viewportHeightWorld =
      getHeight() > 0 ? (float)getHeight() / safeZoom : 600.0f;
  const juce::Rectangle<float> viewportWorld(viewOriginWorld.x, viewOriginWorld.y,
                                             viewportWidthWorld,
                                             viewportHeightWorld);

  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = -std::numeric_limits<float>::max();
  float maxY = -std::numeric_limits<float>::max();

  for (const auto &node : document.nodes) {
    minX = juce::jmin(minX, node.x);
    minY = juce::jmin(minY, node.y);
    maxX = juce::jmax(maxX, node.x + 160.0f);
    maxY = juce::jmax(maxY, node.y + 90.0f);
  }

  for (const auto &frame : document.frames) {
    minX = juce::jmin(minX, frame.x);
    minY = juce::jmin(minY, frame.y);
    maxX = juce::jmax(maxX, frame.x + frame.width);
    maxY = juce::jmax(maxY, frame.y + frame.height);
  }

  juce::Rectangle<float> contentBounds;
  if (minX == std::numeric_limits<float>::max()) {
    contentBounds = viewportWorld;
  } else {
    contentBounds = juce::Rectangle<float>(
        minX, minY, juce::jmax(1.0f, maxX - minX), juce::jmax(1.0f, maxY - minY));
  }

  if (contentBounds.getWidth() < viewportWorld.getWidth()) {
    const float grow = (viewportWorld.getWidth() - contentBounds.getWidth()) * 0.5f;
    contentBounds = contentBounds.expanded(grow, 0.0f);
  }

  if (contentBounds.getHeight() < viewportWorld.getHeight()) {
    const float grow =
        (viewportWorld.getHeight() - contentBounds.getHeight()) * 0.5f;
    contentBounds = contentBounds.expanded(0.0f, grow);
  }

  const float paddingX = juce::jmax(80.0f, contentBounds.getWidth() * 0.08f);
  const float paddingY = juce::jmax(60.0f, contentBounds.getHeight() * 0.08f);

  miniMapWorldBounds = contentBounds.expanded(paddingX, paddingY);
  miniMapCacheValid = true;
}

juce::Point<float> TGraphCanvas::miniMapToWorld(
    juce::Point<float> miniPoint) const {
  if (!miniMapCacheValid || miniMapRectView.getWidth() <= 0.0f ||
      miniMapRectView.getHeight() <= 0.0f) {
    return viewOriginWorld;
  }

  const float nx = (miniPoint.x - miniMapRectView.getX()) /
                   miniMapRectView.getWidth();
  const float ny = (miniPoint.y - miniMapRectView.getY()) /
                   miniMapRectView.getHeight();

  return {miniMapWorldBounds.getX() + nx * miniMapWorldBounds.getWidth(),
          miniMapWorldBounds.getY() + ny * miniMapWorldBounds.getHeight()};
}

int TGraphCanvas::hitTestFrame(juce::Point<float> pointView) const {
  const juce::Point<float> world = viewToWorld(pointView);

  for (auto it = document.frames.rbegin(); it != document.frames.rend(); ++it) {
    const float titleH = 28.0f / zoomLevel;
    const float drawH = it->collapsed ? titleH : it->height;

    juce::Rectangle<float> rect(it->x, it->y, it->width, drawH);
    if (rect.contains(world))
      return it->frameId;
  }

  return 0;
}

bool TGraphCanvas::isPointInFrameTitle(int frameId,
                                       juce::Point<float> pointView) const {
  for (const auto &frame : document.frames) {
    if (frame.frameId != frameId)
      continue;

    const juce::Point<float> world = viewToWorld(pointView);
    const float titleH = 28.0f / zoomLevel;
    juce::Rectangle<float> title(frame.x, frame.y, frame.width, titleH);
    return title.contains(world);
  }

  return false;
}
} // namespace Teul
