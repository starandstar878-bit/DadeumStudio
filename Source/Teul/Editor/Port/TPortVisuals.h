#pragma once

#include "Teul/Editor/TIssueState.h"
#include "Teul/Editor/Theme/TeulPalette.h"
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
