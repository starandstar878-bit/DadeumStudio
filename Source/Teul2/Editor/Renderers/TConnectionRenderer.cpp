#include "Teul2/Editor/Renderers/TConnectionRenderer.h"

#include "Teul2/Editor/Renderers/TNodeRenderer.h"
#include "Teul2/Editor/Renderers/TPortRenderer.h"
#include "Teul2/Editor/Theme/TeulPalette.h"

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

bool getBundleEndpointsFor(const TTeulDocument &document, const TEndpoint &endpoint,
                           std::vector<TEndpoint> &endpointsOut) {
  endpointsOut.clear();
  if (endpoint.isNodePort()) {
    const auto *node = document.findNode(endpoint.nodeId);
    if (node == nullptr) return false;

    int anchorIndex = -1;
    for (int i = 0; i < (int)node->ports.size(); ++i) {
      if (node->ports[i].portId == endpoint.portId) {
        anchorIndex = i;
        break;
      }
    }
    if (anchorIndex < 0) return false;

    const auto &anchorPort = node->ports[anchorIndex];
    int startIndex = anchorIndex;
    while (startIndex > 0 &&
           node->ports[startIndex - 1].channelIndex == node->ports[startIndex].channelIndex - 1 &&
           node->ports[startIndex - 1].direction == anchorPort.direction &&
           node->ports[startIndex - 1].dataType == anchorPort.dataType) {
      --startIndex;
    }

    int count = 1;
    while (startIndex + count < (int)node->ports.size() &&
           node->ports[startIndex + count].channelIndex == node->ports[startIndex + count - 1].channelIndex + 1 &&
           node->ports[startIndex + count].direction == anchorPort.direction &&
           node->ports[startIndex + count].dataType == anchorPort.dataType) {
      ++count;
    }

    for (int i = 0; i < count; ++i)
      endpointsOut.push_back(TEndpoint::makeNodePort(endpoint.nodeId, node->ports[startIndex + i].portId));

    return true;
  }

  const auto *railEndpoint = document.controlState.findEndpoint(endpoint.railEndpointId);
  const auto *railPort = document.findSystemRailPort(endpoint.railEndpointId, endpoint.railPortId);
  if (railEndpoint == nullptr || railPort == nullptr)
    return false;

  for (const auto &p : railEndpoint->ports)
    endpointsOut.push_back(TEndpoint::makeRailPort(endpoint.railEndpointId, p.portId));
  return true;
}

bool tryFindBundleConnections(const TTeulDocument &document,
                              const TConnection &connection,
                              std::vector<int> &bundleIndicesOut) {
  bundleIndicesOut.clear();
  std::vector<TEndpoint> fromBundle;
  std::vector<TEndpoint> toBundle;
  if (!getBundleEndpointsFor(document, connection.from, fromBundle) ||
      !getBundleEndpointsFor(document, connection.to, toBundle)) {
    return false;
  }

  if (fromBundle.size() < 2 || fromBundle.size() != toBundle.size()) return false;

  bool isInBundle = false;
  for (size_t i = 0; i < fromBundle.size(); ++i) {
      if (endpointsEqual(connection.from, fromBundle[i]) && endpointsEqual(connection.to, toBundle[i])) {
          isInBundle = true; break;
      }
  }
  if (!isInBundle) return false;

  bundleIndicesOut.resize(fromBundle.size(), -1);
  for (int index = 0; index < (int)document.connections.size(); ++index) {
      const auto& cand = document.connections[index];
      for (size_t b = 0; b < fromBundle.size(); ++b) {
          if (endpointsEqual(cand.from, fromBundle[b]) && endpointsEqual(cand.to, toBundle[b])) {
              bundleIndicesOut[b] = index;
              break;
          }
      }
  }

  for (int idx : bundleIndicesOut) {
      if (idx == -1) return false;
  }

  return true;
}

bool connectionHasBundleCompanion(const TTeulDocument &document,
                                  const TConnection &connection) {
  std::vector<int> dummy;
  return tryFindBundleConnections(document, connection, dummy);
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
    std::vector<int> bundleIndices;
    if (dataTypeForEndpoint(connection.from) == TPortDataType::Audio &&
        dataTypeForEndpoint(connection.to) == TPortDataType::Audio &&
        tryFindBundleConnections(document, connection, bundleIndices)) {

      consumed[(size_t)index] = true;
      for (int bi : bundleIndices) consumed[(size_t)bi] = true;

      if (!shouldRenderRailNodeConnection(connection))
        continue;

      const auto &companion = document.connections[(size_t)bundleIndices.back()];
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

    std::vector<int> bundleIndices;
    const bool hasBundle =
        sourceType == TPortDataType::Audio &&
        dataTypeForEndpoint(conn.to) == TPortDataType::Audio &&
        tryFindBundleConnections(document, conn, bundleIndices);

    if (hasBundle) {
      if (bundleIndices.front() != (int)index) {
        consumed[index] = true;
        continue;
      }
      for (int bi : bundleIndices) consumed[(size_t)bi] = true;

      if (!shouldRenderRailNodeConnection(conn))
        continue;
      const auto &companion = document.connections[(size_t)bundleIndices.back()];
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
      const bool isSelected = std::any_of(bundleIndices.begin(), bundleIndices.end(), [&](int bIdx) { return document.connections[bIdx].connectionId == selectedConnectionId; });
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


juce::Rectangle<float>
} // namespace Teul
