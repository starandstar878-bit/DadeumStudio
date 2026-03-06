#include "TGraphCanvas.h"
#include "../TeulPalette.h"

#include "../../History/TCommands.h"
#include "../../Registry/TNodeRegistry.h"
#include "../Node/TNodeComponent.h"
#include "../Port/TPortComponent.h"

#include <algorithm>
#include <cmath>`r`n#include <utility>

namespace Teul {

static const TNodeRegistry *getSharedRegistry() {
  static auto reg = makeDefaultNodeRegistry();
  return reg.get();
}

TGraphCanvas::TGraphCanvas(TGraphDocument &doc) : document(doc) {
  setWantsKeyboardFocus(true);

  nodeRegistry = getSharedRegistry();

  viewOriginWorld = {doc.meta.canvasOffsetX, doc.meta.canvasOffsetY};
  zoomLevel = doc.meta.canvasZoom;

  if (zoomLevel < 0.1f)
    zoomLevel = 1.0f;

  rebuildNodeComponents();
  startTimerHz(30);
}

TGraphCanvas::~TGraphCanvas() {
  stopTimer();
  document.meta.canvasOffsetX = viewOriginWorld.x;
  document.meta.canvasOffsetY = viewOriginWorld.y;
  document.meta.canvasZoom = zoomLevel;
}

void TGraphCanvas::setConnectionLevelProvider(ConnectionLevelProvider provider) {
  connectionLevelProvider = std::move(provider);
}

void TGraphCanvas::rebuildNodeComponents() {
  nodeComponents.clear();

  for (const auto &n : document.nodes) {
    const TNodeDescriptor *desc = nullptr;
    if (nodeRegistry)
      desc = nodeRegistry->descriptorFor(n.typeKey);

    auto comp = std::make_unique<TNodeComponent>(*this, n.nodeId, desc);
    addAndMakeVisible(comp.get());
    nodeComponents.push_back(std::move(comp));
  }

  updateChildPositions();
  repaint();
}

void TGraphCanvas::updateChildPositions() {
  for (auto &tnc : nodeComponents) {
    const TNode *nodeData = document.findNode(tnc->getNodeId());
    if (!nodeData)
      continue;

    tnc->setTopLeftPosition(
        worldToView(juce::Point<float>(nodeData->x, nodeData->y)).roundToInt());
    tnc->setTransform(juce::AffineTransform::scale(zoomLevel));
  }
}

void TGraphCanvas::paint(juce::Graphics &g) {
  g.fillAll(TeulPalette::CanvasBackground);
  drawInfiniteGrid(g);
  drawConnections(g);
  drawZoomIndicator(g);
}

void TGraphCanvas::resized() { updateChildPositions(); }

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

  g.setColour(TeulPalette::GridDot);

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
  juce::String zoomText =
      juce::String(juce::roundToInt(zoomLevel * 100.0f)) + "%";

  auto area =
      getLocalBounds().removeFromBottom(30).removeFromRight(80).reduced(5);
  g.setColour(juce::Colour(0x80000000));
  g.fillRoundedRectangle(area.toFloat(), 4.0f);

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(zoomText, area, juce::Justification::centred, false);
}

juce::Colour TGraphCanvas::colorForPortType(TPortDataType type) const {
  switch (type) {
  case TPortDataType::Audio:
    return TeulPalette::PortAudio;
  case TPortDataType::CV:
    return TeulPalette::PortCV;
  case TPortDataType::Gate:
    return TeulPalette::PortGate;
  case TPortDataType::MIDI:
    return TeulPalette::PortMIDI;
  case TPortDataType::Control:
    return TeulPalette::PortControl;
  default:
    return TeulPalette::WireNormal;
  }
}

juce::Path TGraphCanvas::makeWirePath(juce::Point<float> from,
                                      juce::Point<float> to) const {
  juce::Path path;
  path.startNewSubPath(from);

  const float dx = std::abs(to.x - from.x);
  const float tangent = juce::jlimit(40.0f, 220.0f, dx * 0.5f);

  path.cubicTo(from.x + tangent, from.y, to.x - tangent, to.y, to.x, to.y);
  return path;
}

const TPort *TGraphCanvas::findPortModel(NodeId nodeId, PortId portId) const {
  if (const TNode *node = document.findNode(nodeId))
    return node->findPort(portId);

  return nullptr;
}

TNodeComponent *TGraphCanvas::findNodeComponent(NodeId nodeId) noexcept {
  for (auto &n : nodeComponents) {
    if (n->getNodeId() == nodeId)
      return n.get();
  }
  return nullptr;
}

const TNodeComponent *
TGraphCanvas::findNodeComponent(NodeId nodeId) const noexcept {
  for (const auto &n : nodeComponents) {
    if (n->getNodeId() == nodeId)
      return n.get();
  }
  return nullptr;
}

TPortComponent *TGraphCanvas::findPortComponent(NodeId nodeId,
                                                PortId portId) noexcept {
  if (auto *nodeComp = findNodeComponent(nodeId))
    return nodeComp->findPortComponent(portId);

  return nullptr;
}

const TPortComponent *
TGraphCanvas::findPortComponent(NodeId nodeId, PortId portId) const noexcept {
  if (const auto *nodeComp = findNodeComponent(nodeId))
    return nodeComp->findPortComponent(portId);

  return nullptr;
}

juce::Point<float> TGraphCanvas::portCentreInCanvas(NodeId nodeId,
                                                     PortId portId) const {
  if (const auto *portComp = findPortComponent(nodeId, portId)) {
    const auto localCentre = portComp->getLocalBounds().getCentre();
    return getLocalPoint(portComp, localCentre).toFloat();
  }

  if (const auto *node = document.findNode(nodeId))
    return worldToView({node->x, node->y});

  return {};
}

ConnectionId TGraphCanvas::hitTestConnection(juce::Point<float> pointView,
                                             float hitThickness) const {
  for (auto it = document.connections.rbegin(); it != document.connections.rend();
       ++it) {
    const auto fromPos = portCentreInCanvas(it->from.nodeId, it->from.portId);
    const auto toPos = portCentreInCanvas(it->to.nodeId, it->to.portId);

    juce::Path hitPath;
    juce::PathStrokeType(hitThickness).createStrokedPath(
        hitPath, makeWirePath(fromPos, toPos));

    if (hitPath.contains(pointView.x, pointView.y))
      return it->connectionId;
  }

  return kInvalidConnectionId;
}

void TGraphCanvas::drawConnections(juce::Graphics &g) {
  for (const auto &conn : document.connections) {
    const auto fromPos = portCentreInCanvas(conn.from.nodeId, conn.from.portId);
    const auto toPos = portCentreInCanvas(conn.to.nodeId, conn.to.portId);

    const juce::Path wirePath = makeWirePath(fromPos, toPos);
    const TPort *sourcePort = findPortModel(conn.from.nodeId, conn.from.portId);
    const TPortDataType sourceType =
        sourcePort ? sourcePort->dataType : TPortDataType::Audio;

    const float level =
        connectionLevelProvider
            ? juce::jlimit(0.0f, 1.0f, connectionLevelProvider(conn))
            : 0.0f;

    const bool isSelected = (conn.connectionId == selectedConnectionId);
    const float alpha = isSelected
                            ? 1.0f
                            : juce::jlimit(0.55f, 0.95f, 0.75f + level * 0.2f);

    juce::Colour wireColor = colorForPortType(sourceType).withAlpha(alpha);
    if (isSelected)
      wireColor = wireColor.brighter(0.2f);

    g.setColour(wireColor);
    g.strokePath(
        wirePath,
        juce::PathStrokeType(isSelected ? 3.0f : 2.0f,
                             juce::PathStrokeType::curved,
                             juce::PathStrokeType::rounded));

    juce::Path pulsePath;
    const float dashLengths[2] = {7.0f, 34.0f};
    juce::PathStrokeType(2.8f, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded)
        .createDashedStroke(pulsePath, wirePath, dashLengths, 2,
                            flowPhase * (dashLengths[0] + dashLengths[1]));

    const float pulseAlpha = juce::jlimit(0.15f, 0.85f, 0.25f + level * 0.55f);
    g.setColour(wireColor.brighter(0.65f).withAlpha(pulseAlpha));
    g.fillPath(pulsePath);
  }

  if (wireDragState.active) {
    const juce::Path previewPath =
        makeWirePath(wireDragState.sourcePosView, wireDragState.mousePosView);

    const bool hasTarget = wireDragState.targetNodeId != kInvalidNodeId &&
                           wireDragState.targetPortId != kInvalidPortId;

    const bool connectable = hasTarget && isCurrentDragTargetConnectable();

    juce::Colour previewColor =
        colorForPortType(wireDragState.sourceType).withAlpha(0.9f);

    if (hasTarget && !connectable)
      previewColor = juce::Colours::red.withAlpha(0.9f);

    juce::Path dashed;
    const float dashes[2] = {9.0f, 6.0f};
    juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded)
        .createDashedStroke(dashed, previewPath, dashes, 2);

    g.setColour(previewColor);
    g.fillPath(dashed);

    g.setColour(previewColor.withAlpha(0.65f));
    g.fillEllipse(juce::Rectangle<float>(wireDragState.mousePosView.x - 3.0f,
                                         wireDragState.mousePosView.y - 3.0f,
                                         6.0f, 6.0f));
  }

  if (disconnectAnimation.active) {
    juce::Path hanging = makeWirePath(disconnectAnimation.anchorPosView,
                                      disconnectAnimation.loosePosView);

    juce::Colour animColor =
        colorForPortType(disconnectAnimation.type)
            .withAlpha(0.75f * disconnectAnimation.alpha);

    g.setColour(animColor);
    g.strokePath(hanging,
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    g.setColour(animColor.brighter(0.3f).withAlpha(disconnectAnimation.alpha));
    g.fillEllipse(juce::Rectangle<float>(disconnectAnimation.loosePosView.x - 4.0f,
                                         disconnectAnimation.loosePosView.y - 4.0f,
                                         8.0f, 8.0f));
  }
}

void TGraphCanvas::clearDragTargetHighlight() {
  for (auto &node : nodeComponents) {
    for (const auto &port : node->getInputPorts()) {
      port->setDragTargetHighlight(false, true);
    }
  }
}

void TGraphCanvas::updateDragTargetFromMouse(juce::Point<float> mousePosView) {
  clearDragTargetHighlight();

  wireDragState.targetNodeId = kInvalidNodeId;
  wireDragState.targetPortId = kInvalidPortId;
  wireDragState.targetTypeMatch = false;
  wireDragState.targetCycleFree = false;

  if (!wireDragState.active)
    return;

  const auto mousePosInt = mousePosView.roundToInt();

  for (auto &node : nodeComponents) {
    for (const auto &inputPort : node->getInputPorts()) {
      const auto hitArea =
          getLocalArea(inputPort.get(), inputPort->getLocalBounds()).expanded(4);

      if (!hitArea.contains(mousePosInt))
        continue;

      const auto &candidatePort = inputPort->getPortData();
      wireDragState.targetNodeId = candidatePort.ownerNodeId;
      wireDragState.targetPortId = candidatePort.portId;
      wireDragState.targetTypeMatch =
          (candidatePort.dataType == wireDragState.sourceType);
      wireDragState.targetCycleFree =
          !document.wouldCreateCycle(wireDragState.sourceNodeId,
                                     candidatePort.ownerNodeId);

      inputPort->setDragTargetHighlight(true, isCurrentDragTargetConnectable());
      return;
    }
  }
}

bool TGraphCanvas::isCurrentDragTargetConnectable() const {
  if (!wireDragState.active)
    return false;

  if (wireDragState.sourceNodeId == kInvalidNodeId ||
      wireDragState.sourcePortId == kInvalidPortId ||
      wireDragState.targetNodeId == kInvalidNodeId ||
      wireDragState.targetPortId == kInvalidPortId)
    return false;

  if (!wireDragState.targetTypeMatch || !wireDragState.targetCycleFree)
    return false;

  if (wireDragState.sourceNodeId == wireDragState.targetNodeId &&
      wireDragState.sourcePortId == wireDragState.targetPortId)
    return false;

  const TPort *sourcePort =
      findPortModel(wireDragState.sourceNodeId, wireDragState.sourcePortId);
  const TPort *targetPort =
      findPortModel(wireDragState.targetNodeId, wireDragState.targetPortId);

  if (!sourcePort || !targetPort)
    return false;

  if (sourcePort->direction != TPortDirection::Output ||
      targetPort->direction != TPortDirection::Input)
    return false;

  const bool duplicateExists =
      std::any_of(document.connections.begin(), document.connections.end(),
                  [&](const TConnection &conn) {
                    return conn.from.nodeId == wireDragState.sourceNodeId &&
                           conn.from.portId == wireDragState.sourcePortId &&
                           conn.to.nodeId == wireDragState.targetNodeId &&
                           conn.to.portId == wireDragState.targetPortId;
                  });

  return !duplicateExists;
}

void TGraphCanvas::tryCreateConnectionFromDrag() {
  if (!isCurrentDragTargetConnectable())
    return;

  TConnection conn;
  conn.connectionId = document.allocConnectionId();
  conn.from.nodeId = wireDragState.sourceNodeId;
  conn.from.portId = wireDragState.sourcePortId;
  conn.to.nodeId = wireDragState.targetNodeId;
  conn.to.portId = wireDragState.targetPortId;

  document.executeCommand(std::make_unique<AddConnectionCommand>(conn));
  selectedConnectionId = conn.connectionId;
}

void TGraphCanvas::beginConnectionDrag(const TPort &sourcePort,
                                       juce::Point<float> mousePosView) {
  if (sourcePort.direction != TPortDirection::Output)
    return;

  wireDragState = WireDragState{};
  wireDragState.active = true;
  wireDragState.sourceNodeId = sourcePort.ownerNodeId;
  wireDragState.sourcePortId = sourcePort.portId;
  wireDragState.sourceType = sourcePort.dataType;
  wireDragState.mousePosView = mousePosView;

  if (findPortComponent(sourcePort.ownerNodeId, sourcePort.portId) != nullptr)
    wireDragState.sourcePosView =
        portCentreInCanvas(sourcePort.ownerNodeId, sourcePort.portId);
  else
    wireDragState.sourcePosView = mousePosView;

  selectedConnectionId = kInvalidConnectionId;

  updateDragTargetFromMouse(mousePosView);
  repaint();
}

void TGraphCanvas::updateConnectionDrag(juce::Point<float> mousePosView) {
  if (!wireDragState.active)
    return;

  wireDragState.mousePosView = mousePosView;
  updateDragTargetFromMouse(mousePosView);
  repaint();
}

void TGraphCanvas::endConnectionDrag(juce::Point<float> mousePosView) {
  if (!wireDragState.active)
    return;

  wireDragState.mousePosView = mousePosView;
  updateDragTargetFromMouse(mousePosView);
  tryCreateConnectionFromDrag();
  cancelConnectionDrag();
}

void TGraphCanvas::cancelConnectionDrag() {
  clearDragTargetHighlight();
  wireDragState = WireDragState{};
  repaint();
}

void TGraphCanvas::startDisconnectAnimation(juce::Point<float> anchorPosView,
                                            juce::Point<float> loosePosView,
                                            juce::Point<float> impulseView,
                                            TPortDataType type) {
  disconnectAnimation.active = true;
  disconnectAnimation.anchorPosView = anchorPosView;
  disconnectAnimation.loosePosView = loosePosView;
  disconnectAnimation.velocityViewPerSecond = impulseView +
                                              juce::Point<float>(0.0f, 120.0f);
  disconnectAnimation.alpha = 1.0f;
  disconnectAnimation.type = type;
}

void TGraphCanvas::deleteConnectionWithAnimation(ConnectionId connectionId,
                                                 juce::Point<float> breakPointView,
                                                 juce::Point<float> impulseView) {
  const TConnection *conn = document.findConnection(connectionId);
  if (!conn)
    return;

  const auto fromPos = portCentreInCanvas(conn->from.nodeId, conn->from.portId);
  const auto toPos = portCentreInCanvas(conn->to.nodeId, conn->to.portId);

  const float fromDistSq = fromPos.getDistanceSquaredFrom(breakPointView);
  const float toDistSq = toPos.getDistanceSquaredFrom(breakPointView);

  const juce::Point<float> anchorPos = (fromDistSq <= toDistSq) ? toPos : fromPos;

  const TPort *sourcePort = findPortModel(conn->from.nodeId, conn->from.portId);
  const TPortDataType sourceType =
      sourcePort ? sourcePort->dataType : TPortDataType::Audio;

  startDisconnectAnimation(anchorPos, breakPointView, impulseView, sourceType);

  document.executeCommand(
      std::make_unique<DeleteConnectionCommand>(connectionId));

  if (selectedConnectionId == connectionId)
    selectedConnectionId = kInvalidConnectionId;

  repaint();
}

void TGraphCanvas::mouseDown(const juce::MouseEvent &event) {
  grabKeyboardFocus();

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
  }

  const bool isPanTrigger =
      event.mods.isMiddleButtonDown() || event.mods.isAltDown() ||
      juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);

  if (isPanTrigger) {
    panState.active = true;
    panState.startMouseView = event.position;
    panState.startViewOriginWorld = viewOriginWorld;
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
  }
}

void TGraphCanvas::mouseDrag(const juce::MouseEvent &event) {
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
  }
}

void TGraphCanvas::mouseUp(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);

  connectionBreakDragArmed = false;
  pressedConnectionId = kInvalidConnectionId;

  if (panState.active) {
    panState.active = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
  }
}

void TGraphCanvas::mouseWheelMove(const juce::MouseEvent &event,
                                  const juce::MouseWheelDetails &wheel) {
  if (event.mods.isCtrlDown() || event.mods.isCommandDown() ||
      event.mods.isAltDown()) {
    const float zoomDelta = wheel.deltaY * 2.0f;
    const float nextZoom = juce::jlimit(0.1f, 5.0f, zoomLevel + zoomDelta);
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

void TGraphCanvas::timerCallback() {
  flowPhase += 0.04f;
  if (flowPhase >= 1.0f)
    flowPhase -= std::floor(flowPhase);

  if (disconnectAnimation.active) {
    const float dt = 1.0f / 30.0f;
    disconnectAnimation.velocityViewPerSecond.y += 1500.0f * dt;
    disconnectAnimation.loosePosView +=
        disconnectAnimation.velocityViewPerSecond * dt;

    disconnectAnimation.alpha = juce::jmax(0.0f, disconnectAnimation.alpha - 2.4f * dt);
    if (disconnectAnimation.alpha <= 0.01f)
      disconnectAnimation.active = false;
  }

  if (!document.connections.empty() || wireDragState.active ||
      disconnectAnimation.active) {
    repaint();
  }
}

} // namespace Teul