
#include "Gyeol/Editor/Panels/NavigatorPanel.h"

#include "Gyeol/Editor/Theme/GyeolCustomLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace Gyeol::Ui::Panels {
namespace {
using Gyeol::GyeolPalette;

juce::Colour palette(GyeolPalette id, float alpha = 1.0f) {
  return Gyeol::getGyeolColor(id).withAlpha(alpha);
}

juce::Font makePanelFont(const juce::Component &component, float height,
                         bool bold) {
  if (auto *lf = dynamic_cast<const Gyeol::GyeolCustomLookAndFeel *>(
          &component.getLookAndFeel());
      lf != nullptr)
    return lf->makeFont(height, bold);

  auto fallback = juce::Font(juce::FontOptions(height));
  return bold ? fallback.boldened() : fallback;
}

juce::String formatZoomBadgeText(float zoomLevel) {
  return "x" + juce::String(zoomLevel, 2);
}

juce::Path makeArrowPath(juce::Point<float> center, float angle,
                         float size) {
  juce::Path path;
  const auto tip = juce::Point<float>(center.x + std::cos(angle) * size,
                                      center.y + std::sin(angle) * size);
  const auto left = juce::Point<float>(
      center.x + std::cos(angle + juce::MathConstants<float>::pi * 0.75f) *
                     size * 0.62f,
      center.y + std::sin(angle + juce::MathConstants<float>::pi * 0.75f) *
                     size * 0.62f);
  const auto right = juce::Point<float>(
      center.x + std::cos(angle - juce::MathConstants<float>::pi * 0.75f) *
                     size * 0.62f,
      center.y + std::sin(angle - juce::MathConstants<float>::pi * 0.75f) *
                     size * 0.62f);

  path.startNewSubPath(tip);
  path.lineTo(left);
  path.lineTo(right);
  path.closeSubPath();
  return path;
}
} // namespace

NavigatorPanel::NavigatorPanel() {
  setWantsKeyboardFocus(false);
  setInterceptsMouseClicks(true, false);
  setBufferedToImage(true);
  startTimerHz(60);
}

void NavigatorPanel::lookAndFeelChanged() { repaint(); }

void NavigatorPanel::setScene(juce::Rectangle<float> worldBoundsIn,
                              std::vector<SceneItem> itemsIn) {
  if (worldBoundsIn.getWidth() > 0.0f && worldBoundsIn.getHeight() > 0.0f)
    worldBounds = worldBoundsIn;
  sceneItems = std::move(itemsIn);
  repaint();
}

void NavigatorPanel::setViewState(juce::Rectangle<float> visibleWorldBoundsIn,
                                  float zoomLevelIn) {
  visibleWorldBounds = visibleWorldBoundsIn;
  zoomLevel = zoomLevelIn;
  repaint();
}

void NavigatorPanel::setNavigateRequestedCallback(
    std::function<void(juce::Point<float>)> callback) {
  onNavigateRequested = std::move(callback);
}

void NavigatorPanel::setZoomRequestedCallback(
    std::function<void(float, juce::Point<float>)> callback) {
  onZoomRequested = std::move(callback);
}

void NavigatorPanel::setPanelBoundsRequestedCallback(
    std::function<void(juce::Rectangle<int>)> callback) {
  onPanelBoundsRequested = std::move(callback);
}

void NavigatorPanel::enqueueDirtyRect(juce::Rectangle<float> worldDirtyRect) {
  if (worldDirtyRect.isEmpty())
    return;

  dirtyRectPulses.push_back({worldDirtyRect, 0.52f});
  if (dirtyRectPulses.size() > 24)
    dirtyRectPulses.erase(dirtyRectPulses.begin());
  repaint();
}

void NavigatorPanel::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();
  const auto panel = bounds.reduced(0.8f);

  Gyeol::GyeolCustomLookAndFeel::drawSoftDropShadow(g, panel, 10.0f,
                                                     0.32f * shellOpacity);

  g.setColour(palette(GyeolPalette::OverlayBackground, 0.78f * shellOpacity + 0.12f));
  g.fillRoundedRectangle(panel, 10.0f);

  g.setColour(palette(GyeolPalette::BorderDefault, 0.95f));
  g.drawRoundedRectangle(panel, 10.0f, 1.0f);

  auto handle = dragHandleBounds();
  g.setColour(palette(GyeolPalette::HeaderBackground, 0.86f));
  g.fillRoundedRectangle(handle, 8.0f);

  g.setColour(palette(GyeolPalette::TextSecondary, 0.62f));
  g.setFont(makePanelFont(*this, 9.0f, true));
  g.drawText("Navigator", handle.toNearestInt(), juce::Justification::centred,
             false);

  const auto transform = computeMapTransform();
  const auto mapBounds = transform.mapBounds;
  if (!mapBounds.isEmpty()) {
    g.setColour(palette(GyeolPalette::ControlDown, 0.84f));
    g.fillRoundedRectangle(mapBounds, 8.0f);
    g.setColour(palette(GyeolPalette::BorderDefault, 0.92f));
    g.drawRoundedRectangle(mapBounds, 8.0f, 1.0f);
  }

  if (transform.isValid()) {
    g.saveState();
    g.reduceClipRegion(mapBounds.toNearestInt());

    const auto lodMode = transform.scale <= 0.10f;
    for (const auto &item : sceneItems) {
      if (!item.visible)
        continue;

      auto itemMapBounds = transform.worldToMap(item.bounds);
      if (itemMapBounds.getWidth() <= 0.0f || itemMapBounds.getHeight() <= 0.0f)
        continue;

      if (itemMapBounds.getWidth() < 1.0f || itemMapBounds.getHeight() < 1.0f) {
        itemMapBounds = itemMapBounds.withSizeKeepingCentre(
            juce::jmax(1.0f, itemMapBounds.getWidth()),
            juce::jmax(1.0f, itemMapBounds.getHeight()));
      }

      juce::Colour fill = palette(GyeolPalette::ControlHover, 0.35f);
      if (item.locked)
        fill = palette(GyeolPalette::ValidWarning, 0.36f);
      if (item.selected)
        fill = palette(GyeolPalette::AccentPrimary, 0.78f);

      g.setColour(fill);
      if (lodMode)
        g.fillRect(itemMapBounds);
      else
        g.fillRoundedRectangle(itemMapBounds, 2.0f);

      if (!lodMode) {
        g.setColour(item.selected ? palette(GyeolPalette::BorderActive, 0.94f)
                                  : palette(GyeolPalette::BorderDefault, 0.72f));
        g.drawRoundedRectangle(itemMapBounds, 2.0f, 0.8f);
      }
    }

    for (auto &pulse : dirtyRectPulses) {
      auto dirtyMap = transform.worldToMap(pulse.worldRect).getIntersection(mapBounds);
      if (dirtyMap.isEmpty())
        continue;
      g.setColour(palette(GyeolPalette::AccentPrimary, pulse.alpha));
      g.fillRoundedRectangle(dirtyMap.expanded(1.0f), 2.0f);
    }

    auto viewMapBounds = transform.worldToMap(visibleWorldBounds).getIntersection(mapBounds);
    if (!viewMapBounds.isEmpty()) {
      g.setColour(palette(GyeolPalette::AccentPrimary, 0.16f));
      g.fillRoundedRectangle(viewMapBounds, 4.0f);

      g.setColour(palette(GyeolPalette::AccentPrimary, 0.25f + 0.25f * shellOpacity));
      g.drawRoundedRectangle(viewMapBounds.expanded(1.2f), 4.0f, 2.0f);

      g.setColour(palette(GyeolPalette::AccentPrimary, 0.96f));
      g.drawRoundedRectangle(viewMapBounds, 4.0f, 1.3f);
    }

    drawOffscreenIndicators(g, transform, mapBounds);
    g.restoreState();
  }

  const auto zoomText = formatZoomBadgeText(zoomLevel);
  const auto badgeFont = makePanelFont(*this, 9.5f, true);
  const auto badgeWidth = juce::jmax(44.0f, badgeFont.getStringWidthFloat(zoomText) + 12.0f);
  juce::Rectangle<float> badgeBounds(bounds.getCentreX() - badgeWidth * 0.5f,
                                     bounds.getBottom() - 20.0f,
                                     badgeWidth,
                                     13.0f);

  g.setColour(palette(GyeolPalette::HeaderBackground, 0.92f));
  g.fillRoundedRectangle(badgeBounds, 6.0f);
  g.setColour(palette(GyeolPalette::BorderDefault, 0.92f));
  g.drawRoundedRectangle(badgeBounds, 6.0f, 1.0f);
  g.setColour(palette(GyeolPalette::TextSecondary, 0.96f));
  g.setFont(badgeFont);
  g.drawText(zoomText, badgeBounds.toNearestInt(), juce::Justification::centred,
             true);
}

void NavigatorPanel::resized() { repaint(); }

bool NavigatorPanel::hitTest(int x, int y) {
  return getLocalBounds().contains(x, y);
}

NavigatorPanel::MapTransform NavigatorPanel::computeMapTransform() const noexcept {
  MapTransform transform;
  transform.worldBounds = worldBounds;

  auto mapArea = computeMapContainer();
  if (mapArea.isEmpty() || worldBounds.getWidth() <= 0.0f ||
      worldBounds.getHeight() <= 0.0f)
    return transform;

  const auto scaleX = mapArea.getWidth() / worldBounds.getWidth();
  const auto scaleY = mapArea.getHeight() / worldBounds.getHeight();
  const auto scale = std::min(scaleX, scaleY);
  if (scale <= 0.0f)
    return transform;

  mapArea.setSize(worldBounds.getWidth() * scale, worldBounds.getHeight() * scale);
  mapArea.setCentre(computeMapContainer().getCentre());

  transform.mapBounds = mapArea;
  transform.scale = scale;
  return transform;
}

juce::Rectangle<float> NavigatorPanel::computeMapContainer() const noexcept {
  auto area = getLocalBounds().toFloat().reduced(10.0f);
  area.removeFromTop(22.0f);
  area.removeFromBottom(14.0f);
  return area;
}

juce::Rectangle<float> NavigatorPanel::dragHandleBounds() const noexcept {
  auto area = getLocalBounds().toFloat().reduced(10.0f);
  return area.removeFromTop(16.0f);
}

void NavigatorPanel::requestNavigateAt(juce::Point<float> localPoint) {
  const auto transform = computeMapTransform();
  if (!transform.isValid() || onNavigateRequested == nullptr)
    return;

  juce::Point<float> mapPoint = localPoint;
  if (!transform.mapBounds.contains(localPoint)) {
    mapPoint.x = juce::jlimit(transform.mapBounds.getX(),
                              transform.mapBounds.getRight(), localPoint.x);
    mapPoint.y = juce::jlimit(transform.mapBounds.getY(),
                              transform.mapBounds.getBottom(), localPoint.y);
  }

  auto worldPoint = transform.mapToWorld(mapPoint);
  worldPoint.x =
      juce::jlimit(worldBounds.getX(), worldBounds.getRight(), worldPoint.x);
  worldPoint.y =
      juce::jlimit(worldBounds.getY(), worldBounds.getBottom(), worldPoint.y);
  onNavigateRequested(worldPoint);
}

NavigatorPanel::ResizeEdge
NavigatorPanel::hitResizeEdge(juce::Point<float> localPoint,
                              const MapTransform &transform,
                              juce::Rectangle<float> &ioViewRect) const {
  if (!transform.isValid())
    return ResizeEdge::none;

  ioViewRect = transform.worldToMap(visibleWorldBounds).getIntersection(transform.mapBounds);
  if (ioViewRect.isEmpty())
    return ResizeEdge::none;

  constexpr float hitPad = 5.0f;
  const auto left = std::abs(localPoint.x - ioViewRect.getX()) <= hitPad;
  const auto right = std::abs(localPoint.x - ioViewRect.getRight()) <= hitPad;
  const auto top = std::abs(localPoint.y - ioViewRect.getY()) <= hitPad;
  const auto bottom = std::abs(localPoint.y - ioViewRect.getBottom()) <= hitPad;

  if (left && top) return ResizeEdge::topLeft;
  if (right && top) return ResizeEdge::topRight;
  if (left && bottom) return ResizeEdge::bottomLeft;
  if (right && bottom) return ResizeEdge::bottomRight;
  if (left) return ResizeEdge::left;
  if (right) return ResizeEdge::right;
  if (top) return ResizeEdge::top;
  if (bottom) return ResizeEdge::bottom;
  return ResizeEdge::none;
}

void NavigatorPanel::requestZoomFromResizedView(
    const MapTransform &transform,
    const juce::Rectangle<float> &resizedViewRect) {
  if (!transform.isValid() || onZoomRequested == nullptr || zoomLevel <= 0.0f)
    return;

  const auto viewportPixelWidth = visibleWorldBounds.getWidth() * zoomLevel;
  const auto viewportPixelHeight = visibleWorldBounds.getHeight() * zoomLevel;

  auto worldRect = resizedViewRect;
  worldRect.setX(transform.worldBounds.getX() +
                 (resizedViewRect.getX() - transform.mapBounds.getX()) /
                     transform.scale);
  worldRect.setY(transform.worldBounds.getY() +
                 (resizedViewRect.getY() - transform.mapBounds.getY()) /
                     transform.scale);
  worldRect.setWidth(resizedViewRect.getWidth() / transform.scale);
  worldRect.setHeight(resizedViewRect.getHeight() / transform.scale);

  const auto zoomFromWidth = viewportPixelWidth / juce::jmax(1.0f, worldRect.getWidth());
  const auto zoomFromHeight = viewportPixelHeight / juce::jmax(1.0f, worldRect.getHeight());
  const auto requestedZoom = juce::jlimit(0.15f, 8.0f,
                                          std::min(zoomFromWidth, zoomFromHeight));

  onZoomRequested(requestedZoom, worldRect.getCentre());
}

void NavigatorPanel::drawOffscreenIndicators(juce::Graphics &g,
                                             const MapTransform &transform,
                                             juce::Rectangle<float> mapBounds) const {
  const auto centerWorld = visibleWorldBounds.getCentre();

  for (const auto &item : sceneItems) {
    if (!item.selected)
      continue;

    if (visibleWorldBounds.intersects(item.bounds))
      continue;

    const auto target = item.bounds.getCentre();
    const auto direction = target - centerWorld;
    const auto distance = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (distance <= 0.001f)
      continue;

    const auto unit = juce::Point<float>(direction.x / distance, direction.y / distance);
    const auto radiusX = mapBounds.getWidth() * 0.5f - 6.0f;
    const auto radiusY = mapBounds.getHeight() * 0.5f - 6.0f;
    const auto denom = std::max(std::abs(unit.x) / juce::jmax(radiusX, 1.0f),
                                std::abs(unit.y) / juce::jmax(radiusY, 1.0f));
    if (denom <= 0.0f)
      continue;

    const auto edgePoint = mapBounds.getCentre() + unit * (1.0f / denom);
    const auto angle = std::atan2(unit.y, unit.x);

    g.setColour(palette(GyeolPalette::ValidWarning, 0.92f));
    g.fillPath(makeArrowPath(edgePoint, angle, 6.5f));

    auto textArea = juce::Rectangle<int>(juce::roundToInt(edgePoint.x - 24.0f),
                                         juce::roundToInt(edgePoint.y - 17.0f),
                                         48,
                                         10);
    g.setColour(palette(GyeolPalette::HeaderBackground, 0.90f));
    g.fillRoundedRectangle(textArea.toFloat(), 3.0f);
    g.setColour(palette(GyeolPalette::TextSecondary, 0.92f));
    g.setFont(makePanelFont(*this, 8.2f, true));
    g.drawText(juce::String(distance, 0), textArea, juce::Justification::centred,
               false);
  }
}

void NavigatorPanel::mouseEnter(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);
  targetOpacity = 1.0f;
}

void NavigatorPanel::mouseExit(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);
  if (!panelDragActive && !viewResizeActive && !navigateDragActive)
    targetOpacity = 0.30f;
}

void NavigatorPanel::mouseDown(const juce::MouseEvent &event) {
  const auto local = event.position;
  const auto transform = computeMapTransform();

  if (dragHandleBounds().contains(local)) {
    panelDragActive = true;
    panelDragStart = event.getScreenPosition();
    panelBoundsStart = getBounds();
    targetOpacity = 1.0f;
    return;
  }

  juce::Rectangle<float> viewMapBounds;
  activeResizeEdge = hitResizeEdge(local, transform, viewMapBounds);
  if (activeResizeEdge != ResizeEdge::none) {
    viewResizeActive = true;
    resizeStartViewMapBounds = viewMapBounds;
    targetOpacity = 1.0f;
    return;
  }

  navigateDragActive = true;
  targetOpacity = 1.0f;
  requestNavigateAt(local);
}

void NavigatorPanel::mouseDrag(const juce::MouseEvent &event) {
  const auto local = event.position;

  if (panelDragActive) {
    if (onPanelBoundsRequested != nullptr) {
      const auto delta = event.getScreenPosition() - panelDragStart;
      onPanelBoundsRequested(panelBoundsStart.translated(delta.x, delta.y));
    }
    return;
  }

  if (viewResizeActive) {
    const auto transform = computeMapTransform();
    if (!transform.isValid())
      return;

    auto resized = resizeStartViewMapBounds;
    switch (activeResizeEdge) {
    case ResizeEdge::left:
    case ResizeEdge::topLeft:
    case ResizeEdge::bottomLeft:
      resized.setX(local.x);
      resized.setWidth(resizeStartViewMapBounds.getRight() - local.x);
      break;
    default:
      break;
    }

    switch (activeResizeEdge) {
    case ResizeEdge::right:
    case ResizeEdge::topRight:
    case ResizeEdge::bottomRight:
      resized.setWidth(local.x - resizeStartViewMapBounds.getX());
      break;
    default:
      break;
    }

    switch (activeResizeEdge) {
    case ResizeEdge::top:
    case ResizeEdge::topLeft:
    case ResizeEdge::topRight:
      resized.setY(local.y);
      resized.setHeight(resizeStartViewMapBounds.getBottom() - local.y);
      break;
    default:
      break;
    }

    switch (activeResizeEdge) {
    case ResizeEdge::bottom:
    case ResizeEdge::bottomLeft:
    case ResizeEdge::bottomRight:
      resized.setHeight(local.y - resizeStartViewMapBounds.getY());
      break;
    default:
      break;
    }

    resized = resized.getIntersection(transform.mapBounds);
    resized.setWidth(juce::jmax(10.0f, resized.getWidth()));
    resized.setHeight(juce::jmax(10.0f, resized.getHeight()));
    requestZoomFromResizedView(transform, resized);
    return;
  }

  if (navigateDragActive)
    requestNavigateAt(local);
}

void NavigatorPanel::mouseUp(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);
  panelDragActive = false;
  viewResizeActive = false;
  activeResizeEdge = ResizeEdge::none;
  navigateDragActive = false;
  targetOpacity = isMouseOver() ? 1.0f : 0.30f;
}

void NavigatorPanel::mouseMove(const juce::MouseEvent &event) {
  juce::Rectangle<float> viewMapBounds;
  const auto edge = hitResizeEdge(event.position, computeMapTransform(), viewMapBounds);

  switch (edge) {
  case ResizeEdge::left:
  case ResizeEdge::right:
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    break;
  case ResizeEdge::top:
  case ResizeEdge::bottom:
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    break;
  case ResizeEdge::topLeft:
  case ResizeEdge::bottomRight:
    setMouseCursor(juce::MouseCursor::TopLeftCornerResizeCursor);
    break;
  case ResizeEdge::topRight:
  case ResizeEdge::bottomLeft:
    setMouseCursor(juce::MouseCursor::TopRightCornerResizeCursor);
    break;
  default:
    if (dragHandleBounds().contains(event.position))
      setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
      setMouseCursor(juce::MouseCursor::NormalCursor);
    break;
  }
}

void NavigatorPanel::timerCallback() {
  const auto alphaDelta = targetOpacity - shellOpacity;
  if (std::abs(alphaDelta) > 0.001f)
    shellOpacity += alphaDelta * 0.14f;

  for (auto &pulse : dirtyRectPulses)
    pulse.alpha = juce::jmax(0.0f, pulse.alpha - 0.013f);
  dirtyRectPulses.erase(
      std::remove_if(dirtyRectPulses.begin(), dirtyRectPulses.end(),
                     [](const DirtyRectPulse &pulse) { return pulse.alpha <= 0.01f; }),
      dirtyRectPulses.end());

  if (std::abs(alphaDelta) > 0.001f || !dirtyRectPulses.empty())
    repaint();
}
} // namespace Gyeol::Ui::Panels
