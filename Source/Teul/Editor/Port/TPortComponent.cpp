#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/Editor/Port/TPortShapeLayout.h"
#include "Teul/Editor/Port/TPortVisuals.h"
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

void drawIssueRing(juce::Graphics &g, juce::Rectangle<float> bounds,
                   TIssueState issueState, bool elliptical = false) {
  TPortVisuals::drawIssueRing(g, bounds, issueState, elliptical);
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
juce::Rectangle<float> TPortComponent::socketArea() const {
  const auto lane = interactionBounds();
  const float width = juce::jmax(
      14.0f, juce::jmin(lane.getWidth() - 6.0f, 18.0f * scaleFactor));
  auto area = juce::Rectangle<float>(width, lane.getHeight());
  area.setY(lane.getY());
  area.setX(isInput() ? lane.getX() + 2.0f : lane.getRight() - width - 2.0f);
  return area.reduced(0.5f, 0.0f);
}

juce::Rectangle<float> TPortComponent::monoBounds() const {
  return TPortShapeLayout::monoCircleBounds(socketArea());
}

juce::Rectangle<float> TPortComponent::busOuterBounds() const {
  return TPortShapeLayout::busOuterBounds(socketArea(), (int)portGroup.size());
}

std::vector<juce::Rectangle<float>> TPortComponent::channelBounds() const {
  return TPortShapeLayout::busChannelBounds(socketArea(), (int)portGroup.size());
}

TPortComponent::HitResult TPortComponent::hitTestLocal(juce::Point<float> point) const {
  HitResult result;
  const auto hit =
      TPortShapeLayout::hitTest(socketArea(), (int)portGroup.size(), point);
  if (!hit.hit)
    return result;
  result.hit = true;
  result.bundle = hit.bundle;
  result.channelCount = hit.channelCount;
  if (hit.channelIndex >= 0 && hit.channelIndex < (int)portGroup.size())
    result.portId = portGroup[(size_t)hit.channelIndex].portId;
  else if (!portGroup.empty())
    result.portId = portGroup.front().portId;
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

void TPortComponent::setIssueState(std::vector<PortIssueState> newIssueStates,
                                  TIssueState newBundleIssueState) {
  std::sort(newIssueStates.begin(), newIssueStates.end(),
            [](const PortIssueState &lhs, const PortIssueState &rhs) {
              return lhs.portId < rhs.portId;
            });
  if (issueStates == newIssueStates && bundleIssueState == newBundleIssueState)
    return;

  issueStates = std::move(newIssueStates);
  bundleIssueState = newBundleIssueState;
  repaint();
}

TIssueState TPortComponent::issueStateForPort(PortId portId) const noexcept {
  const auto it = std::find_if(issueStates.begin(), issueStates.end(),
                               [portId](const PortIssueState &issue) {
                                 return issue.portId == portId;
                               });
  return it != issueStates.end() ? it->state : TIssueState::none;
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
  const bool sourceChannelActive = dragActive && !dragSourceBundle &&
                                   dragSourcePortId != kInvalidPortId;
  const bool sourceBundleActive = dragActive && dragSourceBundle;

  if (!isBus()) {
    const auto circle = monoBounds();
    g.setColour(baseColor.withAlpha(0.95f));
    g.fillEllipse(circle.reduced(1.0f));
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.drawEllipse(circle.reduced(1.0f), juce::jmax(0.8f, scaleFactor));

    const auto issueState =
        mergeIssueState(bundleIssueState, issueStateForPort(getPortData().portId));
    if (hasIssueState(issueState))
      drawIssueRing(g, circle, issueState, true);

    if (hoveredPortId != kInvalidPortId || sourceChannelActive) {
      TPortVisuals::drawStateOverlay(g, circle, baseColor, true,
                                     sourceChannelActive, true);
    }
    if (isDragTargetHighlighted) {
      TPortVisuals::drawDropTargetOverlay(g, circle, baseColor,
                                          isDragTargetTypeValid, true);
    }
    return;
  }

  const auto outer = busOuterBounds();
  const float radius = juce::jmin(outer.getWidth(), outer.getHeight()) * 0.42f;
  const bool bundleHover = hoveredBundle || sourceBundleActive;
  const bool channelHover = hoveredPortId != kInvalidPortId || sourceChannelActive;
  const bool bundleTarget = isDragTargetHighlighted &&
                            (highlightBundle || highlightedPortId == kInvalidPortId);
  const bool channelTarget = isDragTargetHighlighted && !highlightBundle &&
                             highlightedPortId != kInvalidPortId;

  g.setColour(juce::Colour(0xff0f172a));
  g.fillRoundedRectangle(outer, radius);
  g.setColour(baseColor.withAlpha(0.88f));
  g.drawRoundedRectangle(outer, radius, 1.0f);

  if (hasIssueState(bundleIssueState))
    drawIssueRing(g, outer, bundleIssueState);

  if (bundleHover) {
    TPortVisuals::drawStateOverlay(g, outer, baseColor, true,
                                   sourceBundleActive, false);
  }
  if (bundleTarget) {
    TPortVisuals::drawDropTargetOverlay(g, outer, baseColor,
                                        isDragTargetTypeValid, false);
  }

  const auto circles = channelBounds();
  for (size_t index = 0; index < circles.size() && index < portGroup.size(); ++index) {
    const auto &circle = circles[index];
    const auto channelIssue = issueStateForPort(portGroup[index].portId);
    if (hasIssueState(channelIssue))
      drawIssueRing(g, circle, channelIssue, true);

    const bool activeChannel =
        !bundleHover && !bundleTarget &&
        ((channelTarget && highlightedPortId == portGroup[index].portId) ||
         (channelHover && hoveredPortId == portGroup[index].portId));
    if (activeChannel) {
      TPortVisuals::drawStateOverlay(g, circle, baseColor,
                                     !channelTarget, sourceChannelActive, true);
      if (channelTarget) {
        TPortVisuals::drawDropTargetOverlay(g, circle, baseColor,
                                            isDragTargetTypeValid, true);
      }
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
  if (dragActive)
    return;
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
  dragSourcePortId = hit.bundle ? kInvalidPortId : hit.portId;
  dragSourceBundle = hit.bundle;
  hoveredPortId = hit.bundle ? kInvalidPortId : hit.portId;
  hoveredBundle = hit.bundle;
  repaint();
  auto &canvas = ownerNode.getOwnerCanvas();
  const auto posInCanvas = e.getEventRelativeTo(&canvas).position;
  const auto sourceAnchorLocal =
      hit.bundle ? busOuterBounds().getCentre() : localAnchorForPort(hit.portId);
  const auto sourceAnchorCanvas =
      canvas.getLocalPoint(this, sourceAnchorLocal.roundToInt()).toFloat();
  auto it = std::find_if(portGroup.begin(), portGroup.end(),
                         [&](const TPort &port) { return port.portId == hit.portId; });
  if (it == portGroup.end())
    it = portGroup.begin();
  canvas.beginConnectionDragFromPoint(*it, sourceAnchorCanvas, posInCanvas,
                                      hit.bundle ? hit.channelCount : 1);
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
  dragSourcePortId = kInvalidPortId;
  dragSourceBundle = false;
  auto &canvas = ownerNode.getOwnerCanvas();
  canvas.endConnectionDrag(e.getEventRelativeTo(&canvas).position);
  updateHoverState(e.position);
}

} // namespace Teul
