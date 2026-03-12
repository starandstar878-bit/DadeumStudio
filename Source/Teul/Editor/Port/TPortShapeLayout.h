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
