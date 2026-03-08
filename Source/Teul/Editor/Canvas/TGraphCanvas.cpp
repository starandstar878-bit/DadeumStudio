#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Search/SearchController.h"
#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/Editor/Theme/TeulPalette.h"
#include "Teul/History/TCommands.h"
#include "Teul/Registry/TNodeRegistry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace Teul {

TGraphCanvas::TGraphCanvas(TGraphDocument &doc) : document(doc) {
  setWantsKeyboardFocus(true);

  nodeRegistry = getSharedRegistry();

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
  pushStatusHint(enabled ? "Heatmap visible" : "Heatmap hidden");
}

void TGraphCanvas::setLiveProbeEnabled(bool enabled) {
  if (runtimeViewOptions.liveProbeEnabled == enabled)
    return;

  runtimeViewOptions.liveProbeEnabled = enabled;
  pushStatusHint(enabled ? "Live probe visible" : "Live probe hidden");
}

void TGraphCanvas::setDebugOverlayEnabled(bool enabled) {
  if (runtimeViewOptions.debugOverlayEnabled == enabled)
    return;

  runtimeViewOptions.debugOverlayEnabled = enabled;
  pushStatusHint(enabled ? "Runtime overlay visible"
                         : "Runtime overlay hidden");
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

void TGraphCanvas::rebuildNodeComponents() {
  nodeComponents.clear();

  for (const auto &node : document.nodes) {
    const TNodeDescriptor *desc = nullptr;
    if (nodeRegistry)
      desc = nodeRegistry->descriptorFor(node.typeKey);

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
  g.fillAll(TeulPalette::CanvasBackground);
  drawInfiniteGrid(g);
  drawFrames(g);
  drawConnections(g);
  drawSelectionOverlay(g);
  drawRuntimeOverlay(g);
  drawMiniMap(g);
  drawLibraryDropPreview(g);
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
    if (frameId != 0 && isPointInFrameTitle(frameId, event.position)) {
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
    marqueeState.active = true;
    marqueeState.additive = event.mods.isShiftDown() || event.mods.isCtrlDown() ||
                            event.mods.isCommandDown();
    marqueeState.startView = event.position;
    marqueeState.rectView = {event.position.x, event.position.y, 0.0f, 0.0f};
    marqueeState.baseSelection =
        marqueeState.additive ? selectedNodeIds : std::vector<NodeId>{};

    if (!marqueeState.additive)
      clearNodeSelection();
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
    updateMarqueeSelection();
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
} // namespace Teul
