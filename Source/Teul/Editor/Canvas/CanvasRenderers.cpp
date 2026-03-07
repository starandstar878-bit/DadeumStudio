#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/Editor/Theme/TeulPalette.h"

#include <algorithm>
#include <cmath>

namespace Teul {

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
                            juce::AffineTransform(),
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


void TGraphCanvas::drawMiniMap(juce::Graphics &g) {
  const float miniW = 220.0f;
  const float miniH = 140.0f;
  const float margin = 12.0f;

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

  g.setColour(juce::Colour(0xe0121212));
  g.fillRoundedRectangle(miniMapRectView, 6.0f);
  g.setColour(juce::Colour(0xff3b3b3b));
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
    g.setColour(juce::Colour(frame.colorArgb).withAlpha(0.35f));
    g.fillRect(rect);
  }

  for (const auto &node : document.nodes) {
    auto topLeft = worldToMini({node.x, node.y});
    g.setColour(isNodeSelected(node.nodeId) ? juce::Colours::white
                                            : juce::Colour(0xff9ca3af));
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

  g.setColour(juce::Colours::white.withAlpha(0.9f));
  g.drawRect(miniMapViewportRectView, 1.2f);
  g.restoreState();
}

void TGraphCanvas::pushStatusHint(const juce::String &text) {
  statusHintText = text;
  statusHintAlpha = 1.0f;
  repaint();
}
bool TGraphCanvas::isNodeInsideFrame(const TNode &node,
                                     const TFrameRegion &frame) const {
  const juce::Rectangle<float> frameRect(frame.x, frame.y, frame.width,
                                         frame.height);
  const juce::Rectangle<float> nodeRect(node.x, node.y, 160.0f, 90.0f);
  return frameRect.intersects(nodeRect);
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
  const float titleHWorld = 28.0f / zoomLevel;

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
      base = juce::Colour(0x334d8bf7);

    g.setColour(base.withMultipliedAlpha(frame.locked ? 0.45f : 0.62f));
    g.fillRoundedRectangle(rect, 8.0f);

    g.setColour(base.withMultipliedAlpha(0.95f));
    g.drawRoundedRectangle(rect, 8.0f, 1.0f);

    auto titleRect = rect.removeFromTop(26.0f);
    g.setColour(base.brighter(0.2f).withAlpha(0.55f));
    g.fillRoundedRectangle(titleRect, 7.0f);

    juce::String title = frame.title;
    if (frame.locked)
      title += " [L]";
    if (frame.collapsed)
      title += " [C]";

    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(12.0f);
    g.drawText(title, titleRect.toNearestInt().reduced(8, 0),
               juce::Justification::centredLeft, false);
  }
}

void TGraphCanvas::drawLibraryDropPreview(juce::Graphics &g) {
  if (!libraryDropPreview.active)
    return;

  const auto *desc =
      nodeRegistry ? nodeRegistry->descriptorFor(libraryDropPreview.typeKey) : nullptr;
  const juce::String title =
      desc != nullptr ? desc->displayName : libraryDropPreview.typeKey;
  const juce::String subtitle =
      desc != nullptr ? desc->category + " / " + desc->typeKey
                      : libraryDropPreview.typeKey;

  auto bubble = juce::Rectangle<float>(libraryDropPreview.pointView.x + 14.0f,
                                       libraryDropPreview.pointView.y - 10.0f,
                                       240.0f, 44.0f);
  const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
  bubble.setPosition(
      juce::jlimit(bounds.getX(), bounds.getRight() - bubble.getWidth(),
                   bubble.getX()),
      juce::jlimit(bounds.getY(), bounds.getBottom() - bubble.getHeight(),
                   bubble.getY()));

  g.setColour(juce::Colour(0xdd0f172a));
  g.fillRoundedRectangle(bubble, 10.0f);
  g.setColour(juce::Colour(0xff60a5fa));
  g.drawRoundedRectangle(bubble, 10.0f, 1.2f);

  g.setColour(juce::Colour(0xff93c5fd));
  g.drawEllipse(libraryDropPreview.pointView.x - 6.0f,
                libraryDropPreview.pointView.y - 6.0f, 12.0f, 12.0f, 1.5f);
  g.fillEllipse(libraryDropPreview.pointView.x - 2.5f,
                libraryDropPreview.pointView.y - 2.5f, 5.0f, 5.0f);

  auto textArea = bubble.toNearestInt().reduced(12, 7);
  g.setColour(juce::Colours::white.withAlpha(0.95f));
  g.setFont(13.0f);
  g.drawText(title, textArea.removeFromTop(16), juce::Justification::centredLeft,
             false);
  g.setColour(juce::Colours::white.withAlpha(0.55f));
  g.setFont(11.0f);
  g.drawText(subtitle, textArea, juce::Justification::centredLeft, false);
}

void TGraphCanvas::drawSelectionOverlay(juce::Graphics &g) {
  if (marqueeState.active) {
    g.setColour(juce::Colour(0x5560a5fa));
    g.fillRect(marqueeState.rectView);
    g.setColour(juce::Colour(0xff60a5fa));
    g.drawRect(marqueeState.rectView, 1.2f);
  }

  if (snapGuideState.xActive) {
    const float x = worldToView({snapGuideState.worldX, 0.0f}).x;
    g.setColour(juce::Colour(0x88fbbf24));
    g.drawLine(x, 0.0f, x, (float)getHeight(), 1.0f);
  }

  if (snapGuideState.yActive) {
    const float y = worldToView({0.0f, snapGuideState.worldY}).y;
    g.setColour(juce::Colour(0x88fbbf24));
    g.drawLine(0.0f, y, (float)getWidth(), y, 1.0f);
  }
}

void TGraphCanvas::drawStatusHint(juce::Graphics &g) {
  if (statusHintAlpha <= 0.01f || statusHintText.isEmpty())
    return;

  auto area = getLocalBounds().removeFromTop(28).reduced(10, 4);
  area.setWidth(juce::jmin(460, area.getWidth()));

  g.setColour(juce::Colour(0xcc111111).withAlpha(statusHintAlpha));
  g.fillRoundedRectangle(area.toFloat(), 5.0f);

  g.setColour(juce::Colours::white.withAlpha(statusHintAlpha));
  g.setFont(13.0f);
  g.drawText(statusHintText, area.reduced(8, 0), juce::Justification::centredLeft,
             false);
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
