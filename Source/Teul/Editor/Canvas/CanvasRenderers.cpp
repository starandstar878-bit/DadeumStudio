#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/Editor/Theme/TeulPalette.h"

#include <algorithm>
#include <cmath>

namespace Teul {
namespace {

juce::String railZoneIdForEndpoint(const TEndpoint &endpoint) {
  if (!endpoint.isRailPort())
    return {};

  return endpoint.railEndpointId + "::" + endpoint.railPortId;
}

bool endpointsEqual(const TEndpoint &a, const TEndpoint &b) {
  return a.ownerKind == b.ownerKind && a.nodeId == b.nodeId &&
         a.portId == b.portId && a.railEndpointId == b.railEndpointId &&
         a.railPortId == b.railPortId;
}

bool splitStereoLabel(juce::StringRef name, bool &isLeft, juce::String &suffix) {
  const juce::String trimmed = juce::String(name).trim();
  if (trimmed.equalsIgnoreCase("L")) {
    isLeft = true;
    suffix.clear();
    return true;
  }
  if (trimmed.equalsIgnoreCase("R")) {
    isLeft = false;
    suffix.clear();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("L ")) {
    isLeft = true;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("R ")) {
    isLeft = false;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  return false;
}

bool findNodeStereoSibling(const TGraphDocument &document,
                           const TEndpoint &endpoint,
                           TEndpoint &siblingOut) {
  if (!endpoint.isNodePort())
    return false;

  const auto *node = document.findNode(endpoint.nodeId);
  const auto *port = node != nullptr ? node->findPort(endpoint.portId) : nullptr;
  if (port == nullptr || port->dataType != TPortDataType::Audio)
    return false;

  bool isLeft = false;
  juce::String suffix;
  if (!splitStereoLabel(port->name, isLeft, suffix))
    return false;

  for (const auto &candidate : node->ports) {
    if (candidate.portId == port->portId ||
        candidate.direction != port->direction ||
        candidate.dataType != port->dataType) {
      continue;
    }

    bool candidateIsLeft = false;
    juce::String candidateSuffix;
    if (!splitStereoLabel(candidate.name, candidateIsLeft, candidateSuffix) ||
        candidateIsLeft == isLeft || candidateSuffix != suffix) {
      continue;
    }

    siblingOut = TEndpoint::makeNodePort(endpoint.nodeId, candidate.portId);
    return true;
  }

  return false;
}

bool findRailStereoSibling(const TGraphDocument &document,
                           const TEndpoint &endpoint,
                           TEndpoint &siblingOut) {
  if (!endpoint.isRailPort())
    return false;

  const auto *railEndpoint = document.controlState.findEndpoint(endpoint.railEndpointId);
  if (railEndpoint == nullptr || !railEndpoint->stereo ||
      railEndpoint->ports.size() < 2) {
    return false;
  }

  if (railEndpoint->ports[0].portId == endpoint.railPortId) {
    siblingOut = TEndpoint::makeRailPort(endpoint.railEndpointId,
                                         railEndpoint->ports[1].portId);
    return true;
  }

  if (railEndpoint->ports[1].portId == endpoint.railPortId) {
    siblingOut = TEndpoint::makeRailPort(endpoint.railEndpointId,
                                         railEndpoint->ports[0].portId);
    return true;
  }

  return false;
}

bool findStereoSiblingEndpoint(const TGraphDocument &document,
                               const TEndpoint &endpoint,
                               TEndpoint &siblingOut) {
  if (endpoint.isRailPort())
    return findRailStereoSibling(document, endpoint, siblingOut);
  return findNodeStereoSibling(document, endpoint, siblingOut);
}

bool tryFindBundleCompanionIndex(const TGraphDocument &document,
                                 const TConnection &connection,
                                 int currentIndex,
                                 int &companionIndexOut) {
  TEndpoint fromSibling;
  TEndpoint toSibling;
  if (!findStereoSiblingEndpoint(document, connection.from, fromSibling) ||
      !findStereoSiblingEndpoint(document, connection.to, toSibling)) {
    return false;
  }

  for (int index = 0; index < (int)document.connections.size(); ++index) {
    if (index == currentIndex)
      continue;

    const auto &candidate = document.connections[(size_t)index];
    if (endpointsEqual(candidate.from, fromSibling) &&
        endpointsEqual(candidate.to, toSibling)) {
      companionIndexOut = index;
      return true;
    }
  }

  return false;
}


bool connectionHasBundleCompanion(const TGraphDocument &document,
                                  const TConnection &connection) {
  TEndpoint fromSibling;
  TEndpoint toSibling;
  if (!findStereoSiblingEndpoint(document, connection.from, fromSibling) ||
      !findStereoSiblingEndpoint(document, connection.to, toSibling)) {
    return false;
  }

  for (const auto &candidate : document.connections) {
    if (candidate.connectionId == connection.connectionId)
      continue;

    if (endpointsEqual(candidate.from, fromSibling) &&
        endpointsEqual(candidate.to, toSibling)) {
      return true;
    }
  }

  return false;
}

juce::Point<float> midpoint(juce::Point<float> a, juce::Point<float> b) {
  return {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
}

juce::Rectangle<float> makeBundleCap(juce::Point<float> a,
                                     juce::Point<float> b,
                                     float width) {
  if (a.y > b.y)
    std::swap(a, b);

  const float centreX = (a.x + b.x) * 0.5f;
  return juce::Rectangle<float>(centreX - width * 0.5f, a.y - width * 0.5f,
                                width,
                                juce::jmax(width, b.y - a.y + width));
}

} // namespace

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
juce::Colour TGraphCanvas::colorForPortType(TPortDataType type) const {
  switch (type) {
  case TPortDataType::Audio:
    return TeulPalette::PortAudio();
  case TPortDataType::CV:
    return TeulPalette::PortCV();
  case TPortDataType::Gate:
    return TeulPalette::PortGate();
  case TPortDataType::MIDI:
    return TeulPalette::PortMIDI();
  case TPortDataType::Control:
    return TeulPalette::PortControl();
  default:
    return TeulPalette::WireNormal();
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

const TPort *TGraphCanvas::findPortModel(const TEndpoint &endpoint) const {
  if (!endpoint.isNodePort())
    return nullptr;

  return findPortModel(endpoint.nodeId, endpoint.portId);
}

const TSystemRailPort *
TGraphCanvas::findRailPortModel(const TEndpoint &endpoint) const {
  if (!endpoint.isRailPort())
    return nullptr;

  return document.findSystemRailPort(endpoint.railEndpointId,
                                     endpoint.railPortId);
}

TPortDataType TGraphCanvas::dataTypeForEndpoint(const TEndpoint &endpoint) const {
  if (const auto *port = findPortModel(endpoint))
    return port->dataType;
  if (const auto *railPort = findRailPortModel(endpoint))
    return railPort->dataType;
  return TPortDataType::Audio;
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
    const auto localCentre = portComp->localAnchorForPort(portId);
    return getLocalPoint(portComp, localCentre.roundToInt()).toFloat();
  }

  if (const auto *node = document.findNode(nodeId))
    return worldToView({node->x, node->y});

  return {};
}

juce::Point<float> TGraphCanvas::portCentreInCanvas(const TEndpoint &endpoint) const {
  if (endpoint.isNodePort())
    return portCentreInCanvas(endpoint.nodeId, endpoint.portId);

  if (!endpoint.isRailPort() || externalEndpointAnchorProvider == nullptr)
    return {};

  const auto zoneId = railZoneIdForEndpoint(endpoint);
  for (const auto &zone : externalEndpointAnchorProvider()) {
    if (zone.zoneId == zoneId)
      return zone.boundsView.getCentre();
  }

  return {};
}


juce::Rectangle<float> TGraphCanvas::canvasViewportBounds() const {
  return getLocalBounds().toFloat();
}

juce::Rectangle<float>
TGraphCanvas::endpointVisualBoundsInCanvas(const TEndpoint &endpoint) const {
  if (!endpoint.isNodePort())
    return {};

  const auto *portComp = findPortComponent(endpoint.nodeId, endpoint.portId);
  if (portComp == nullptr)
    return {};

  const auto localBounds = portComp->visualBoundsForPort(endpoint.portId);
  if (localBounds.isEmpty())
    return {};

  return getLocalArea(portComp, localBounds.getSmallestIntegerContainer()).toFloat();
}

juce::Rectangle<float>
TGraphCanvas::endpointBundleVisualBoundsInCanvas(const TEndpoint &endpoint) const {
  if (!endpoint.isNodePort())
    return {};

  const auto *portComp = findPortComponent(endpoint.nodeId, endpoint.portId);
  if (portComp == nullptr)
    return {};

  const auto localBounds = portComp->visualBoundsForBundle();
  if (localBounds.isEmpty())
    return {};

  return getLocalArea(portComp, localBounds.getSmallestIntegerContainer()).toFloat();
}

bool TGraphCanvas::isNodeEndpointVisibleInViewport(const TEndpoint &endpoint) const {
  const auto visualBounds = endpointVisualBoundsInCanvas(endpoint);
  return !visualBounds.isEmpty() &&
         visualBounds.intersects(canvasViewportBounds());
}

bool TGraphCanvas::shouldRenderRailNodeConnection(const TConnection &connection) const {
  const bool railToNode = connection.from.isRailPort() && connection.to.isNodePort();
  const bool nodeToRail = connection.from.isNodePort() && connection.to.isRailPort();
  if (!railToNode && !nodeToRail)
    return true;

  const auto &nodeEndpoint = nodeToRail ? connection.from : connection.to;
  const bool isBundle = dataTypeForEndpoint(connection.from) == TPortDataType::Audio &&
                        dataTypeForEndpoint(connection.to) == TPortDataType::Audio &&
                        connectionHasBundleCompanion(document, connection);
  const auto visualBounds = isBundle
                                ? endpointBundleVisualBoundsInCanvas(nodeEndpoint)
                                : endpointVisualBoundsInCanvas(nodeEndpoint);
  return !visualBounds.isEmpty() &&
         visualBounds.intersects(canvasViewportBounds());
}

ConnectionId TGraphCanvas::hitTestConnection(juce::Point<float> pointView,
                                             float hitThickness) const {
  std::vector<bool> consumed(document.connections.size(), false);

  for (int index = (int)document.connections.size() - 1; index >= 0; --index) {
    if (consumed[(size_t)index])
      continue;

    const auto &connection = document.connections[(size_t)index];
    int companionIndex = -1;
    if (dataTypeForEndpoint(connection.from) == TPortDataType::Audio &&
        dataTypeForEndpoint(connection.to) == TPortDataType::Audio &&
        tryFindBundleCompanionIndex(document, connection, index,
                                    companionIndex)) {
      consumed[(size_t)index] = true;
      consumed[(size_t)companionIndex] = true;
      if (!shouldRenderRailNodeConnection(connection))
        continue;

      const auto &companion = document.connections[(size_t)companionIndex];
      const auto fromA = portCentreInCanvas(connection.from);
      const auto toA = portCentreInCanvas(connection.to);
      const auto fromB = portCentreInCanvas(companion.from);
      const auto toB = portCentreInCanvas(companion.to);
      const auto bundlePath = makeWirePath(midpoint(fromA, fromB), midpoint(toA, toB));

      juce::Path hitPath;
      juce::PathStrokeType(hitThickness + 1.5f).createStrokedPath(hitPath,
                                                                  bundlePath);
      if (hitPath.contains(pointView.x, pointView.y))
        return connection.connectionId;

      const auto fromCap = makeBundleCap(fromA, fromB, hitThickness + 1.5f);
      const auto toCap = makeBundleCap(toA, toB, hitThickness + 1.5f);
      juce::Path capPath;
      capPath.addRoundedRectangle(fromCap, juce::jmax(3.0f, fromCap.getWidth() * 0.5f));
      capPath.addRoundedRectangle(toCap, juce::jmax(3.0f, toCap.getWidth() * 0.5f));
      if (capPath.contains(pointView.x, pointView.y))
        return connection.connectionId;

      continue;
    }

    consumed[(size_t)index] = true;
    if (!shouldRenderRailNodeConnection(connection))
      continue;
    const auto fromPos = portCentreInCanvas(connection.from);
    const auto toPos = portCentreInCanvas(connection.to);

    juce::Path hitPath;
    juce::PathStrokeType(hitThickness).createStrokedPath(
        hitPath, makeWirePath(fromPos, toPos));

    if (hitPath.contains(pointView.x, pointView.y))
      return connection.connectionId;
  }

  return kInvalidConnectionId;
}

void TGraphCanvas::drawConnections(juce::Graphics &g) {
  std::vector<bool> consumed(document.connections.size(), false);

  for (size_t index = 0; index < document.connections.size(); ++index) {
    if (consumed[index])
      continue;

    const auto &conn = document.connections[index];
    const TPortDataType sourceType = dataTypeForEndpoint(conn.from);
    const float level = connectionLevelProvider
                            ? juce::jlimit(0.0f, 1.0f,
                                           connectionLevelProvider(conn))
                            : 0.0f;

    int companionIndex = -1;
    const bool hasBundle =
        sourceType == TPortDataType::Audio &&
        dataTypeForEndpoint(conn.to) == TPortDataType::Audio &&
        tryFindBundleCompanionIndex(document, conn, (int)index,
                                    companionIndex);

    if (hasBundle) {
      consumed[index] = true;
      if (companionIndex < (int)index)
        continue;

      consumed[(size_t)companionIndex] = true;
      if (!shouldRenderRailNodeConnection(conn))
        continue;
      const auto &companion = document.connections[(size_t)companionIndex];
      const auto fromA = portCentreInCanvas(conn.from);
      const auto toA = portCentreInCanvas(conn.to);
      const auto fromB = portCentreInCanvas(companion.from);
      const auto toB = portCentreInCanvas(companion.to);
      const auto fromMid = midpoint(fromA, fromB);
      const auto toMid = midpoint(toA, toB);
      const auto wirePath = makeWirePath(fromMid, toMid);
      const float companionLevel = connectionLevelProvider
                                       ? juce::jlimit(0.0f, 1.0f,
                                                      connectionLevelProvider(
                                                          companion))
                                       : 0.0f;
      const float bundleLevel = juce::jmax(level, companionLevel);
      const bool isSelected = (conn.connectionId == selectedConnectionId) ||
                              (companion.connectionId == selectedConnectionId);
      const float alpha = isSelected
                              ? 1.0f
                              : juce::jlimit(0.55f, 0.95f,
                                             0.75f + bundleLevel * 0.2f);
      juce::Colour wireColor = colorForPortType(sourceType).withAlpha(alpha);
      if (isSelected)
        wireColor = wireColor.brighter(0.2f);

      const float capWidth = isSelected ? 10.4f : 8.2f;
      const auto fromCap = makeBundleCap(fromA, fromB, capWidth);
      const auto toCap = makeBundleCap(toA, toB, capWidth);
      const float capRadius = juce::jmax(3.2f, capWidth * 0.5f);
      g.setColour(wireColor.withAlpha(isSelected ? 0.38f : 0.28f));
      g.fillRoundedRectangle(fromCap, capRadius);
      g.fillRoundedRectangle(toCap, capRadius);
      g.setColour(wireColor.withAlpha(isSelected ? 0.92f : 0.78f));
      g.drawRoundedRectangle(fromCap, capRadius, isSelected ? 1.8f : 1.2f);
      g.drawRoundedRectangle(toCap, capRadius, isSelected ? 1.8f : 1.2f);

      const auto underlayColor = wireColor.darker(0.70f).withAlpha(
          isSelected ? 0.50f : 0.36f);
      g.setColour(underlayColor);
      g.strokePath(
          wirePath,
          juce::PathStrokeType(isSelected ? 6.2f : 5.0f,
                               juce::PathStrokeType::curved,
                               juce::PathStrokeType::rounded));

      g.setColour(wireColor.withAlpha(isSelected ? 0.98f : 0.92f));
      g.strokePath(
          wirePath,
          juce::PathStrokeType(isSelected ? 4.1f : 3.2f,
                               juce::PathStrokeType::curved,
                               juce::PathStrokeType::rounded));

      g.setColour(wireColor.brighter(0.52f).withAlpha(isSelected ? 0.74f : 0.52f));
      g.strokePath(
          wirePath,
          juce::PathStrokeType(isSelected ? 1.85f : 1.35f,
                               juce::PathStrokeType::curved,
                               juce::PathStrokeType::rounded));

      juce::Path pulsePath;
      const float dashLengths[2] = {7.0f, 34.0f};
      juce::PathStrokeType(isSelected ? 3.0f : 2.6f,
                           juce::PathStrokeType::curved,
                           juce::PathStrokeType::rounded)
          .createDashedStroke(pulsePath, wirePath, dashLengths, 2,
                              juce::AffineTransform(),
                              flowPhase * (dashLengths[0] + dashLengths[1]));

      const float pulseAlpha =
          juce::jlimit(0.18f, 0.88f, 0.30f + bundleLevel * 0.55f);
      g.setColour(wireColor.brighter(0.65f).withAlpha(pulseAlpha));
      g.fillPath(pulsePath);
      continue;
    }

    consumed[index] = true;
    if (!shouldRenderRailNodeConnection(conn))
      continue;
    const auto fromPos = portCentreInCanvas(conn.from);
    const auto toPos = portCentreInCanvas(conn.to);
    if (fromPos.isOrigin() && toPos.isOrigin())
      continue;

    const juce::Path wirePath = makeWirePath(fromPos, toPos);
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
                            juce::AffineTransform(),
                            flowPhase * (dashLengths[0] + dashLengths[1]));

    const float pulseAlpha = juce::jlimit(0.15f, 0.85f, 0.25f + level * 0.55f);
    g.setColour(wireColor.brighter(0.65f).withAlpha(pulseAlpha));
    g.fillPath(pulsePath);
  }

  if (wireDragState.active) {
    const juce::Path previewPath =
        makeWirePath(wireDragState.sourcePosView, wireDragState.mousePosView);

    const bool hasTarget =
        (wireDragState.targetNodeId != kInvalidNodeId &&
         wireDragState.targetPortId != kInvalidPortId) ||
        wireDragState.targetExternalZoneId.isNotEmpty();

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

juce::Rectangle<float>
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
