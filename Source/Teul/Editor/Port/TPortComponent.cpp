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
  if (isBus()) {
    const int width = juce::jmax(8, juce::roundToInt(18.0f * scaleFactor));
    const int height = juce::jmax(18, juce::roundToInt((14.0f + 10.0f * (float)(portGroup.size() - 1)) * scaleFactor));
    setSize(width, height);
  } else {
    const int size = juce::jmax(4, juce::roundToInt(14.0f * scaleFactor));
    setSize(size, size);
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

juce::Rectangle<float> TPortComponent::monoBounds() const {
  const auto bounds = getLocalBounds().toFloat();
  const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
  return juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());
}

juce::Rectangle<float> TPortComponent::busOuterBounds() const {
  return getLocalBounds().toFloat().reduced(0.5f, 0.5f);
}

std::vector<juce::Rectangle<float>> TPortComponent::channelBounds() const {
  std::vector<juce::Rectangle<float>> bounds;
  if (!isBus()) {
    bounds.push_back(monoBounds());
    return bounds;
  }

  const auto outer = busOuterBounds();
  const int channelCount = (int)portGroup.size();
  const float diameter = juce::jmax(6.0f, outer.getWidth() - 4.0f);
  const float radius = diameter * 0.5f;
  const float firstCentreY = outer.getY() + radius + 2.0f;
  const float lastCentreY = outer.getBottom() - radius - 2.0f;
  const float step = channelCount > 1 ? (lastCentreY - firstCentreY) / (float)(channelCount - 1) : 0.0f;
  const float centreX = outer.getCentreX();
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

  if (isBus() && busOuterBounds().contains(point)) {
    result.hit = true;
    result.bundle = true;
    result.portId = portGroup.front().portId;
    result.channelCount = (int)portGroup.size();
    return result;
  }

  if (!isBus() && containsEllipse(monoBounds(), point)) {
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
      g.setColour(targetColor.withAlpha(0.22f));
      g.fillEllipse(circle.expanded(2.5f * scaleFactor));
      g.setColour(targetColor.withAlpha(0.9f));
      g.drawEllipse(circle.expanded(1.2f * scaleFactor), juce::jmax(1.0f, 1.8f * scaleFactor));
    }

    if (hoveredPortId != kInvalidPortId) {
      g.setColour(baseColor.brighter(0.2f));
      g.fillEllipse(circle.reduced(0.9f));
      g.setColour(baseColor.withAlpha(0.45f));
      g.drawEllipse(circle, juce::jmax(1.0f, 1.2f * scaleFactor));
      return;
    }

    g.setColour(baseColor);
    g.fillEllipse(circle.reduced(1.4f));
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(circle.reduced(1.4f), juce::jmax(0.8f, scaleFactor));
    return;
  }

  const auto outer = busOuterBounds();
  const float radius = outer.getWidth() * 0.5f;
  g.setColour(juce::Colour(0xff0f172a));
  g.fillRoundedRectangle(outer, radius);
  g.setColour(baseColor.withAlpha(0.92f));
  g.drawRoundedRectangle(outer, radius, 1.0f);

  if (isDragTargetHighlighted && highlightBundle) {
    g.setColour(targetColor.withAlpha(0.18f));
    g.fillRoundedRectangle(outer.expanded(1.5f), radius);
    g.setColour(targetColor.withAlpha(0.9f));
    g.drawRoundedRectangle(outer.expanded(1.0f), radius, juce::jmax(1.2f, 1.8f * scaleFactor));
  } else if (hoveredBundle) {
    g.setColour(baseColor.withAlpha(0.16f));
    g.fillRoundedRectangle(outer, radius);
  }

  const auto circles = channelBounds();
  for (size_t index = 0; index < circles.size() && index < portGroup.size(); ++index) {
    const auto &circle = circles[index];
    const bool channelHovered = hoveredPortId == portGroup[index].portId;
    const bool channelTarget = isDragTargetHighlighted && highlightedPortId == portGroup[index].portId;
    if (channelHovered || channelTarget) {
      const auto overlay = channelTarget ? targetColor : baseColor;
      g.setColour(overlay.withAlpha(channelTarget ? 0.24f : 0.16f));
      g.fillEllipse(circle);
      g.setColour(overlay.withAlpha(channelTarget ? 0.9f : 0.72f));
      g.drawEllipse(circle, channelTarget ? juce::jmax(1.2f, 1.8f * scaleFactor)
                                          : juce::jmax(1.0f, 1.2f * scaleFactor));
    }

    const auto lane = circle.reduced(circle.getWidth() * 0.16f, circle.getHeight() * 0.16f);
    g.setColour(baseColor.withAlpha(channelHovered ? 0.20f : 0.10f));
    g.fillEllipse(lane);
    g.setColour(baseColor.withAlpha(0.32f));
    g.drawEllipse(lane, 1.0f);

    const float dotSize = juce::jmax(3.5f, lane.getWidth() * 0.24f);
    g.setColour(baseColor.withAlpha(0.95f));
    g.fillEllipse(lane.getCentreX() - dotSize * 0.5f,
                  lane.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
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
