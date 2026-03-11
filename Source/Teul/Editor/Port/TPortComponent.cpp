#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/Editor/Theme/TeulPalette.h"
#include "Teul/Editor/Canvas/TGraphCanvas.h"
#include "Teul/Editor/Node/TNodeComponent.h"

#include <algorithm>

namespace Teul {
namespace {

juce::String groupedDisplayName(const std::vector<TPort> &ports) {
  if (ports.empty())
    return {};
  if (ports.size() == 1)
    return ports.front().name;

  auto trimPrefix = [](const juce::String &name) {
    const juce::String trimmed = name.trim();
    if (trimmed.startsWithIgnoreCase("L ") || trimmed.startsWithIgnoreCase("R "))
      return trimmed.substring(2).trim();
    return trimmed;
  };

  const auto label = trimPrefix(ports.front().name);
  return label.isNotEmpty() ? label : ports.front().name;
}

void drawWarningRing(juce::Graphics &g, juce::Rectangle<float> bounds,
                     juce::Colour accent, bool elliptical = false) {
  const auto ring = bounds.expanded(2.0f);
  const auto rounded = juce::jmax(
      3.0f, juce::jmin(ring.getWidth(), ring.getHeight()) * 0.46f);
  g.setColour(accent.withAlpha(0.16f));
  if (elliptical)
    g.fillEllipse(ring);
  else
    g.fillRoundedRectangle(ring, rounded);

  juce::Path outline;
  if (elliptical)
    outline.addEllipse(ring);
  else
    outline.addRoundedRectangle(ring, rounded);

  juce::Path dashed;
  const float dashes[2] = {3.5f, 3.0f};
  juce::PathStrokeType(1.2f, juce::PathStrokeType::curved,
                       juce::PathStrokeType::rounded)
      .createDashedStroke(dashed, outline, dashes, 2);
  g.setColour(accent.withAlpha(0.92f));
  g.fillPath(dashed);
}

} // namespace

TPortComponent::TPortComponent(TNodeComponent &owner, const TPort &port)
    : TPortComponent(owner, std::vector<TPort>{port}) {}

TPortComponent::TPortComponent(TNodeComponent &owner, std::vector<TPort> ports)
    : ownerNode(owner), portGroup(std::move(ports)) {
  jassert(!portGroup.empty());
  setScaleFactor(1.0f);
}

TPortComponent::~TPortComponent() = default;

void TPortComponent::setScaleFactor(float newScale) {
  scaleFactor = juce::jmax(0.1f, newScale);
  const int laneWidth = juce::jmax(20, juce::roundToInt(34.0f * scaleFactor));
  const int monoHeight = juce::jmax(18, juce::roundToInt(22.0f * scaleFactor));
  if (isBus()) {
    const int height = juce::jmax(
        monoHeight + juce::roundToInt(8.0f * scaleFactor),
        juce::roundToInt((26.0f + 15.0f * (float)(portGroup.size() - 1)) * scaleFactor));
    setSize(laneWidth, height);
  } else {
    setSize(laneWidth, monoHeight);
  }
  repaint();
}

juce::Colour TPortComponent::getPortColor() const {
  switch (getPortData().dataType) {
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
    return juce::Colours::grey;
  }
}

bool TPortComponent::containsPort(PortId portId) const noexcept {
  return std::any_of(portGroup.begin(), portGroup.end(),
                     [portId](const TPort &port) { return port.portId == portId; });
}

juce::String TPortComponent::getDisplayName() const {
  return groupedDisplayName(portGroup);
}

juce::Rectangle<float> TPortComponent::interactionBounds() const {
  return getLocalBounds().toFloat().reduced(1.0f, 1.0f);
}

juce::Rectangle<float> TPortComponent::monoBounds() const {
  const auto bounds = interactionBounds();
  const float diameter = juce::jmax(
      8.0f, juce::jmin(bounds.getHeight() - 4.0f, 14.0f * scaleFactor));
  const float radius = diameter * 0.5f;
  const float centreX = isInput() ? bounds.getX() + radius + 3.0f
                                  : bounds.getRight() - radius - 3.0f;
  return juce::Rectangle<float>(diameter, diameter)
      .withCentre(juce::Point<float>(centreX, bounds.getCentreY()));
}

juce::Rectangle<float> TPortComponent::busOuterBounds() const {
  const auto lane = interactionBounds();
  const float width = juce::jmax(
      10.0f, juce::jmin(lane.getWidth() - 10.0f, 15.0f * scaleFactor));
  const float height = juce::jmax(
      18.0f, juce::jmin(lane.getHeight() - 2.0f, lane.getHeight()));
  const float centreX = isInput() ? lane.getX() + width * 0.5f + 4.0f
                                  : lane.getRight() - width * 0.5f - 4.0f;
  return juce::Rectangle<float>(width, height)
      .withCentre(juce::Point<float>(centreX, lane.getCentreY()));
}

std::vector<juce::Rectangle<float>> TPortComponent::channelBounds() const {
  std::vector<juce::Rectangle<float>> bounds;
  if (!isBus()) {
    bounds.push_back(monoBounds());
    return bounds;
  }

  const auto outer = busOuterBounds();
  const int channelCount = (int)portGroup.size();
  const float diameter = juce::jmax(
      7.0f, juce::jmin(outer.getWidth() - 4.0f, 10.0f * scaleFactor));
  const float radius = diameter * 0.5f;
  const float firstCentreY = outer.getY() + radius + 3.0f;
  const float lastCentreY = outer.getBottom() - radius - 3.0f;
  const float step = channelCount > 1
                         ? (lastCentreY - firstCentreY) / (float)(channelCount - 1)
                         : 0.0f;
  const float centreX = isInput() ? outer.getX() + radius + 3.0f
                                  : outer.getRight() - radius - 3.0f;
  bounds.reserve((size_t)channelCount);
  for (int index = 0; index < channelCount; ++index) {
    bounds.push_back(juce::Rectangle<float>(diameter, diameter)
                         .withCentre(juce::Point<float>(centreX, firstCentreY + step * (float)index)));
  }
  return bounds;
}

TPortComponent::HitResult TPortComponent::hitTestLocal(juce::Point<float> point) const {
  const auto containsEllipse = [](juce::Rectangle<float> bounds,
                                  juce::Point<float> p) {
    const auto radiusX = bounds.getWidth() * 0.5f;
    const auto radiusY = bounds.getHeight() * 0.5f;
    if (radiusX <= 0.0f || radiusY <= 0.0f)
      return false;

    const auto dx = (p.x - bounds.getCentreX()) / radiusX;
    const auto dy = (p.y - bounds.getCentreY()) / radiusY;
    return dx * dx + dy * dy <= 1.0f;
  };

  HitResult result;
  const auto bounds = channelBounds();
  for (size_t index = 0; index < bounds.size() && index < portGroup.size(); ++index) {
    const auto &circle = bounds[index];
    if (containsEllipse(circle, point)) {
      result.hit = true;
      result.bundle = false;
      result.portId = portGroup[index].portId;
      result.channelCount = 1;
      return result;
    }
  }

  if (isBus() && interactionBounds().contains(point)) {
    result.hit = true;
    result.bundle = true;
    result.portId = portGroup.front().portId;
    result.channelCount = (int)portGroup.size();
    return result;
  }

  if (!isBus() && interactionBounds().contains(point)) {
    result.hit = true;
    result.bundle = false;
    result.portId = portGroup.front().portId;
    result.channelCount = 1;
  }

  return result;
}

juce::Point<float> TPortComponent::localAnchorForPort(PortId portId) const {
  const auto bounds = channelBounds();
  for (size_t index = 0; index < portGroup.size() && index < bounds.size(); ++index) {
    if (portGroup[index].portId == portId)
      return bounds[index].getCentre();
  }
  return isBus() ? busOuterBounds().getCentre() : monoBounds().getCentre();
}

void TPortComponent::setDragTargetHighlight(bool enabled, bool validType,
                                            PortId newHighlightedPortId,
                                            bool newHighlightBundle) {
  if (isDragTargetHighlighted == enabled && isDragTargetTypeValid == validType &&
      highlightedPortId == newHighlightedPortId &&
      highlightBundle == newHighlightBundle)
    return;

  isDragTargetHighlighted = enabled;
  isDragTargetTypeValid = validType;
  highlightedPortId = newHighlightedPortId;
  highlightBundle = newHighlightBundle;
  repaint();
}

void TPortComponent::setWarningState(std::vector<PortId> newWarningPortIds,
                                     bool newWarningBundle) {
  std::sort(newWarningPortIds.begin(), newWarningPortIds.end());
  if (warningPortIds == newWarningPortIds && warningBundle == newWarningBundle)
    return;

  warningPortIds = std::move(newWarningPortIds);
  warningBundle = newWarningBundle;
  repaint();
}

bool TPortComponent::hasWarningForPort(PortId portId) const noexcept {
  return std::find(warningPortIds.begin(), warningPortIds.end(), portId) !=
         warningPortIds.end();
}

void TPortComponent::updateHoverState(juce::Point<float> point) {
  const auto hit = hitTestLocal(point);
  const PortId newHoveredPortId = hit.hit && !hit.bundle ? hit.portId : kInvalidPortId;
  const bool newHoveredBundle = hit.hit && hit.bundle;
  if (hoveredPortId == newHoveredPortId && hoveredBundle == newHoveredBundle)
    return;

  hoveredPortId = newHoveredPortId;
  hoveredBundle = newHoveredBundle;
  repaint();
}

void TPortComponent::paint(juce::Graphics &g) {
  const auto baseColor = getPortColor();
  const juce::Colour targetColor =
      isDragTargetTypeValid ? baseColor.brighter(0.35f) : juce::Colours::red;

  if (!isBus()) {
    const auto circle = monoBounds();
    if (isDragTargetHighlighted) {
      g.setColour(targetColor.withAlpha(0.16f));
      g.fillEllipse(circle);
      g.setColour(targetColor.withAlpha(0.92f));
      g.drawEllipse(circle, juce::jmax(1.0f, 1.6f * scaleFactor));
    } else if (hoveredPortId != kInvalidPortId) {
      g.setColour(baseColor.withAlpha(0.10f));
      g.fillEllipse(circle);
      g.setColour(baseColor.withAlpha(0.42f));
      g.drawEllipse(circle, juce::jmax(1.0f, 1.1f * scaleFactor));
    }

    if (warningBundle || hasWarningForPort(getPortData().portId))
      drawWarningRing(g, circle, juce::Colour(0xfff59e0b), true);

    g.setColour(baseColor.withAlpha(0.95f));
    g.fillEllipse(circle.reduced(1.0f));
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.drawEllipse(circle.reduced(1.0f), juce::jmax(0.8f, scaleFactor));
    return;
  }

  const auto outer = busOuterBounds();
  const float radius = juce::jmin(outer.getWidth(), outer.getHeight()) * 0.42f;
  const bool bundleHover = !isDragTargetHighlighted && hoveredBundle;
  const bool channelHover = !isDragTargetHighlighted && hoveredPortId != kInvalidPortId;
  const bool bundleTarget = isDragTargetHighlighted &&
                            (highlightBundle || highlightedPortId == kInvalidPortId);
  const bool channelTarget = isDragTargetHighlighted && !highlightBundle &&
                             highlightedPortId != kInvalidPortId;

  if (bundleTarget) {
    g.setColour(targetColor.withAlpha(0.14f));
    g.fillRoundedRectangle(outer, radius);
    g.setColour(targetColor.withAlpha(0.92f));
    g.drawRoundedRectangle(outer, radius,
                           juce::jmax(1.2f, 1.8f * scaleFactor));
  } else if (bundleHover) {
    g.setColour(baseColor.withAlpha(0.10f));
    g.fillRoundedRectangle(outer, radius);
    g.setColour(baseColor.withAlpha(0.42f));
    g.drawRoundedRectangle(outer, radius,
                           juce::jmax(1.0f, 1.2f * scaleFactor));
  }

  if (warningBundle)
    drawWarningRing(g, outer, juce::Colour(0xfff59e0b));

  g.setColour(juce::Colour(0xff0f172a));
  g.fillRoundedRectangle(outer, radius);
  g.setColour(baseColor.withAlpha(0.88f));
  g.drawRoundedRectangle(outer, radius, 1.0f);

  const auto circles = channelBounds();
  for (size_t index = 0; index < circles.size() && index < portGroup.size(); ++index) {
    const auto &circle = circles[index];
    const bool channelWarning = hasWarningForPort(portGroup[index].portId);
    if (channelWarning)
      drawWarningRing(g, circle, juce::Colour(0xfff59e0b), true);

    const bool activeChannel = (channelTarget && highlightedPortId == portGroup[index].portId) ||
                               (channelHover && hoveredPortId == portGroup[index].portId);
    if (activeChannel) {
      const auto overlay = channelTarget ? targetColor : baseColor;
      g.setColour(overlay.withAlpha(channelTarget ? 0.16f : 0.10f));
      g.fillEllipse(circle);
      g.setColour(overlay.withAlpha(channelTarget ? 0.92f : 0.42f));
      g.drawEllipse(circle,
                    channelTarget ? juce::jmax(1.2f, 1.8f * scaleFactor)
                                  : juce::jmax(1.0f, 1.2f * scaleFactor));
    }

    const auto channelLane = circle.reduced(circle.getWidth() * 0.16f,
                                            circle.getHeight() * 0.16f);
    g.setColour(baseColor.withAlpha(0.12f));
    g.fillEllipse(channelLane);
    g.setColour(baseColor.withAlpha(0.36f));
    g.drawEllipse(channelLane, 1.0f);

    const float dotSize = juce::jmax(3.5f, channelLane.getWidth() * 0.24f);
    g.setColour(baseColor.withAlpha(0.95f));
    g.fillEllipse(channelLane.getCentreX() - dotSize * 0.5f,
                  channelLane.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
  }
}

void TPortComponent::mouseEnter(const juce::MouseEvent &e) {
  updateHoverState(e.position);
}

void TPortComponent::mouseMove(const juce::MouseEvent &e) {
  updateHoverState(e.position);
}

void TPortComponent::mouseExit(const juce::MouseEvent &) {
  hoveredPortId = kInvalidPortId;
  hoveredBundle = false;
  repaint();
}

void TPortComponent::mouseDown(const juce::MouseEvent &e) {
  if (!e.mods.isLeftButtonDown())
    return;

  if (getPortData().direction != TPortDirection::Output)
    return;

  const auto hit = hitTestLocal(e.position);
  if (!hit.hit)
    return;

  dragActive = true;
  auto &canvas = ownerNode.getOwnerCanvas();
  const auto posInCanvas = e.getEventRelativeTo(&canvas).position;
  auto it = std::find_if(portGroup.begin(), portGroup.end(),
                         [&](const TPort &port) { return port.portId == hit.portId; });
  if (it == portGroup.end())
    it = portGroup.begin();
  canvas.beginConnectionDrag(*it, posInCanvas, hit.bundle ? hit.channelCount : 1);
}

void TPortComponent::mouseDrag(const juce::MouseEvent &e) {
  if (!e.mods.isLeftButtonDown() || !dragActive)
    return;

  auto &canvas = ownerNode.getOwnerCanvas();
  canvas.updateConnectionDrag(e.getEventRelativeTo(&canvas).position);
}

void TPortComponent::mouseUp(const juce::MouseEvent &e) {
  if (getPortData().direction != TPortDirection::Output)
    return;

  if (!dragActive)
    return;

  dragActive = false;
  auto &canvas = ownerNode.getOwnerCanvas();
  canvas.endConnectionDrag(e.getEventRelativeTo(&canvas).position);
}

} // namespace Teul
