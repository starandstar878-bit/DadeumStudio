#pragma once

#include "Teul/Model/TNode.h"
#include "Teul/Registry/TNodeRegistry.h"

#include <JuceHeader.h>

#include <functional>

namespace Teul {

using PortLevelReader = std::function<float(PortId)>;

enum class InlinePreviewKind {
  none,
  oscillator,
  adsr,
  meter,
};

InlinePreviewKind inlinePreviewKindFor(const TNodeDescriptor *descriptor);
int previewHeightForKind(InlinePreviewKind kind);
int minWidthForKind(InlinePreviewKind kind);
juce::Point<int> measureNodeSize(const TNodeDescriptor *descriptor,
                                 int inputPortCount,
                                 int outputPortCount,
                                 bool collapsed = false);
juce::Rectangle<float> makePreviewBounds(const juce::Rectangle<float> &bounds,
                                         InlinePreviewKind kind);
void drawOscillatorPreview(juce::Graphics &g,
                           const juce::Rectangle<float> &bounds,
                           const TNode &node);
void drawAdsrPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                     const TNode &node);
void drawMeterPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                      const TNodeDescriptor *descriptor, const TNode &node,
                      const PortLevelReader &portLevelReader = {});

} // namespace Teul
