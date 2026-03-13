#pragma once

#include <JuceHeader.h>
#include <vector>

namespace Teul {

struct TPortShapeHitResult {
  bool hit = false;
  bool bundle = false;
  int channelIndex = -1;
  int channelCount = 1;
};

namespace TPortShapeLayout {

inline juce::Rectangle<float> monoCircleBounds(juce::Rectangle<float> area) {
  const auto outer = area.reduced(0.5f, 0.5f);
  const float diameter = juce::jmax(8.0f,
                                    juce::jmin(outer.getWidth(), outer.getHeight()));
  return juce::Rectangle<float>(diameter, diameter).withCentre(outer.getCentre());
}

inline juce::Rectangle<float> busOuterBounds(juce::Rectangle<float> area,
                                             int channelCount) {
  if (channelCount <= 1)
    return monoCircleBounds(area);
  return area.reduced(0.5f, 0.5f);
}

inline float busChannelPaddingY(int channelCount) {
  if (channelCount <= 2)
    return 2.0f;
  if (channelCount <= 6)
    return 1.6f;
  return 1.0f;
}

inline float busChannelDiameter(juce::Rectangle<float> outer, int channelCount) {
  const float widthLimited =
      juce::jmax(channelCount > 6 ? 4.5f : 8.0f, outer.getWidth() - 4.0f);
  if (channelCount <= 6)
    return widthLimited;

  const float padding = busChannelPaddingY(channelCount);
  const float verticalLimited =
      ((outer.getHeight() - padding * 2.0f) / (float)channelCount) - 0.8f;
  return juce::jlimit(4.5f, widthLimited, juce::jmax(4.5f, verticalLimited));
}

inline std::vector<juce::Rectangle<float>> busChannelBounds(
    juce::Rectangle<float> area, int channelCount) {
  std::vector<juce::Rectangle<float>> bounds;
  if (channelCount <= 0)
    return bounds;

  if (channelCount == 1) {
    bounds.push_back(monoCircleBounds(area));
    return bounds;
  }

  const auto outer = busOuterBounds(area, channelCount);
  const float circleDiameter = busChannelDiameter(outer, channelCount);
  const float radius = circleDiameter * 0.5f;
  const float padding = busChannelPaddingY(channelCount);
  const float firstCentreY = outer.getY() + radius + padding;
  const float lastCentreY = outer.getBottom() - radius - padding;
  const float step = channelCount > 1
                         ? (lastCentreY - firstCentreY) /
                               (float)(channelCount - 1)
                         : 0.0f;
  const float centreX = outer.getCentreX();
  bounds.reserve((size_t)channelCount);
  for (int index = 0; index < channelCount; ++index) {
    const float centreY = firstCentreY + step * (float)index;
    bounds.push_back(juce::Rectangle<float>(circleDiameter, circleDiameter)
                         .withCentre({centreX, centreY}));
  }
  return bounds;
}

inline bool containsEllipse(juce::Rectangle<float> bounds,
                            juce::Point<float> point) {
  const auto radiusX = bounds.getWidth() * 0.5f;
  const auto radiusY = bounds.getHeight() * 0.5f;
  if (radiusX <= 0.0f || radiusY <= 0.0f)
    return false;

  const auto dx = (point.x - bounds.getCentreX()) / radiusX;
  const auto dy = (point.y - bounds.getCentreY()) / radiusY;
  return dx * dx + dy * dy <= 1.0f;
}

inline TPortShapeHitResult hitTest(juce::Rectangle<float> area, int channelCount,
                                   juce::Point<float> point) {
  TPortShapeHitResult result;
  const auto channelRects = busChannelBounds(area, channelCount);
  for (size_t index = 0; index < channelRects.size(); ++index) {
    if (!containsEllipse(channelRects[index], point))
      continue;

    result.hit = true;
    result.bundle = false;
    result.channelIndex = (int)index;
    result.channelCount = 1;
    return result;
  }

  if (channelCount > 1) {
    const auto outer = busOuterBounds(area, channelCount);
    if (!outer.contains(point))
      return result;

    result.hit = true;
    result.bundle = true;
    result.channelIndex = 0;
    result.channelCount = channelCount;
    return result;
  }

  if (containsEllipse(monoCircleBounds(area), point)) {
    result.hit = true;
    result.bundle = false;
    result.channelIndex = 0;
    result.channelCount = 1;
  }

  return result;
}

} // namespace TPortShapeLayout

} // namespace Teul

#include "Teul2/Editor/TIssueState.h"
#include "Teul2/Editor/Theme/TeulPalette.h"
#include <JuceHeader.h>

namespace Teul::TPortVisuals {

inline void drawStateOverlay(juce::Graphics &g, juce::Rectangle<float> bounds,
                             juce::Colour accent, bool hovered, bool active,
                             bool elliptical = false) {
  if (!hovered && !active)
    return;

  const auto overlayBounds = bounds.reduced(active ? 0.4f : 0.8f,
                                            active ? 0.4f : 0.8f);
  const auto radius = juce::jmax(
      3.0f,
      juce::jmin(overlayBounds.getWidth(), overlayBounds.getHeight()) * 0.45f);
  const auto fill = active ? accent.withAlpha(0.28f)
                           : accent.withAlpha(0.16f);
  const auto outline = active ? accent.brighter(0.3f).withAlpha(0.98f)
                              : accent.withAlpha(0.72f);
  g.setColour(fill);
  if (elliptical)
    g.fillEllipse(overlayBounds);
  else
    g.fillRoundedRectangle(overlayBounds, radius);
  g.setColour(outline);
  if (elliptical)
    g.drawEllipse(overlayBounds, active ? 1.6f : 1.1f);
  else
    g.drawRoundedRectangle(overlayBounds, radius, active ? 1.6f : 1.1f);
}

inline juce::Colour dropTargetColour(juce::Colour accent, bool canConnect) {
  return canConnect ? accent.brighter(0.45f).interpolatedWith(
                          TeulPalette::AccentGreen(), 0.35f)
                    : TeulPalette::AccentOrange();
}

inline void drawDropTargetOverlay(juce::Graphics &g,
                                  juce::Rectangle<float> bounds,
                                  juce::Colour accent, bool canConnect,
                                  bool elliptical = false) {
  const auto overlay = dropTargetColour(accent, canConnect);
  const auto expanded = bounds.expanded(1.5f);
  const auto rounded = juce::jmax(
      3.0f,
      juce::jmin(expanded.getWidth(), expanded.getHeight()) * 0.45f);
  g.setColour(overlay.withAlpha(canConnect ? 0.20f : 0.18f));
  if (elliptical)
    g.fillEllipse(expanded);
  else
    g.fillRoundedRectangle(expanded, rounded);
  g.setColour(overlay.withAlpha(0.96f));
  if (elliptical)
    g.drawEllipse(expanded, canConnect ? 2.0f : 1.6f);
  else
    g.drawRoundedRectangle(expanded, rounded, canConnect ? 2.0f : 1.6f);
}

inline void drawIssueRing(juce::Graphics &g, juce::Rectangle<float> bounds,
                          TIssueState issueState, bool elliptical = false) {
  const auto accent = issueStateAccent(issueState);
  if (!hasIssueState(issueState))
    return;

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
  juce::PathStrokeType(1.25f, juce::PathStrokeType::curved,
                       juce::PathStrokeType::rounded)
      .createDashedStroke(dashed, outline, dashes, 2);
  g.setColour(accent.withAlpha(0.92f));
  g.fillPath(dashed);
}

} // namespace Teul::TPortVisuals

#include "Teul2/Editor/TIssueState.h"
#include "Teul2/Document/TDocumentTypes.h"
#include <JuceHeader.h>
#include <vector>

namespace Teul {

class TNodeComponent;

class TPortComponent : public juce::Component {
public:
  struct HitResult {
    bool hit = false;
    bool bundle = false;
    PortId portId = kInvalidPortId;
    int channelCount = 1;
  };

  struct PortIssueState {
    PortId portId = kInvalidPortId;
    TIssueState state = TIssueState::none;

    bool operator==(const PortIssueState &other) const noexcept {
      return portId == other.portId && state == other.state;
    }
  };

  TPortComponent(TNodeComponent &owner, const TPort &port);
  TPortComponent(TNodeComponent &owner, std::vector<TPort> ports);
  ~TPortComponent() override;

  void paint(juce::Graphics &g) override;

  void mouseEnter(const juce::MouseEvent &e) override;
  void mouseMove(const juce::MouseEvent &e) override;
  void mouseExit(const juce::MouseEvent &e) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  const TPort &getPortData() const { return portGroup.front(); }
  const std::vector<TPort> &getPortGroup() const noexcept { return portGroup; }
  bool containsPort(PortId portId) const noexcept;
  juce::Point<float> localAnchorForPort(PortId portId) const;
  juce::Rectangle<float> visualBoundsForPort(PortId portId) const;
  juce::Rectangle<float> visualBoundsForBundle() const;
  juce::String getDisplayName() const;
  HitResult hitTestLocal(juce::Point<float> point) const;

  void setDragTargetHighlight(bool enabled, bool validType,
                              PortId highlightedPortId = kInvalidPortId,
                              bool highlightBundle = false);
  void setIssueState(std::vector<PortIssueState> issueStates,
                     TIssueState bundleIssueState = TIssueState::none);
  void setScaleFactor(float newScale);

private:
  TNodeComponent &ownerNode;
  std::vector<TPort> portGroup;

  float scaleFactor = 1.0f;
  PortId hoveredPortId = kInvalidPortId;
  bool hoveredBundle = false;
  bool isDragTargetHighlighted = false;
  bool isDragTargetTypeValid = true;
  PortId highlightedPortId = kInvalidPortId;
  bool highlightBundle = false;
  std::vector<PortIssueState> issueStates;
  TIssueState bundleIssueState = TIssueState::none;
  bool dragActive = false;
  PortId dragSourcePortId = kInvalidPortId;
  bool dragSourceBundle = false;

  bool isBus() const noexcept { return portGroup.size() > 1; }
  bool isInput() const noexcept {
    return getPortData().direction == TPortDirection::Input;
  }
  juce::Colour getPortColor() const;
  juce::Rectangle<float> interactionBounds() const;
  juce::Rectangle<float> socketArea() const;
  juce::Rectangle<float> monoBounds() const;
  juce::Rectangle<float> busOuterBounds() const;
  std::vector<juce::Rectangle<float>> channelBounds() const;
  TIssueState issueStateForPort(PortId portId) const noexcept;
  void updateHoverState(juce::Point<float> point);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TPortComponent)
};

} // namespace Teul
