#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/Editor/Theme/TeulPalette.h"
#include "Teul/Editor/Canvas/TGraphCanvas.h"
#include "Teul/Editor/Node/TNodeComponent.h"

namespace Teul {

TPortComponent::TPortComponent(TNodeComponent &owner, const TPort &port)
    : ownerNode(owner), portData(port) {
  setScaleFactor(1.0f);
}

TPortComponent::~TPortComponent() = default;

void TPortComponent::setScaleFactor(float newScale) {
  scaleFactor = juce::jmax(0.1f, newScale);
  const int size = juce::jmax(4, juce::roundToInt(14.0f * scaleFactor));
  setSize(size, size);
  repaint();
}

juce::Colour TPortComponent::getPortColor() const {
  switch (portData.dataType) {
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

void TPortComponent::setDragTargetHighlight(bool enabled, bool validType) {
  if (isDragTargetHighlighted == enabled && isDragTargetTypeValid == validType)
    return;

  isDragTargetHighlighted = enabled;
  isDragTargetTypeValid = validType;
  repaint();
}

void TPortComponent::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();
  const juce::Colour baseColor = getPortColor();
  const float haloExpand = juce::jmax(1.0f, 3.0f * scaleFactor);
  const float outlineExpand = juce::jmax(0.8f, 1.5f * scaleFactor);
  const float reducedInner = juce::jmax(0.75f, 1.0f * scaleFactor);
  const float reducedCore = juce::jmax(1.0f, 2.0f * scaleFactor);
  const float outlineThickness = juce::jmax(1.0f, 1.5f * scaleFactor);
  const float coreOutlineThickness = juce::jmax(0.8f, 1.0f * scaleFactor);

  if (isDragTargetHighlighted) {
    const juce::Colour targetColor =
        isDragTargetTypeValid ? baseColor.brighter(0.35f) : juce::Colours::red;
    g.setColour(targetColor.withAlpha(0.22f));
    g.fillEllipse(bounds.expanded(haloExpand));
    g.setColour(targetColor.withAlpha(0.9f));
    g.drawEllipse(bounds.expanded(outlineExpand),
                  juce::jmax(1.0f, 2.0f * scaleFactor));
  }

  if (isHovered || isDragTargetHighlighted) {
    g.setColour(baseColor.brighter(0.2f));
    g.fillEllipse(bounds.reduced(reducedInner));
    g.setColour(baseColor.withAlpha(0.45f));
    g.drawEllipse(bounds, outlineThickness);
    return;
  }

  g.setColour(baseColor);
  g.fillEllipse(bounds.reduced(reducedCore));
  g.setColour(juce::Colours::black.withAlpha(0.6f));
  g.drawEllipse(bounds.reduced(reducedCore), coreOutlineThickness);
}

void TPortComponent::mouseEnter(const juce::MouseEvent &) {
  isHovered = true;
  repaint();
}

void TPortComponent::mouseExit(const juce::MouseEvent &) {
  isHovered = false;
  repaint();
}

void TPortComponent::mouseDown(const juce::MouseEvent &e) {
  if (!e.mods.isLeftButtonDown())
    return;

  if (portData.direction != TPortDirection::Output)
    return;

  auto &canvas = ownerNode.getOwnerCanvas();
  const auto posInCanvas = e.getEventRelativeTo(&canvas).position;
  canvas.beginConnectionDrag(portData, posInCanvas);
}

void TPortComponent::mouseDrag(const juce::MouseEvent &e) {
  if (!e.mods.isLeftButtonDown())
    return;

  if (portData.direction != TPortDirection::Output)
    return;

  auto &canvas = ownerNode.getOwnerCanvas();
  canvas.updateConnectionDrag(e.getEventRelativeTo(&canvas).position);
}

void TPortComponent::mouseUp(const juce::MouseEvent &e) {
  if (portData.direction != TPortDirection::Output)
    return;

  auto &canvas = ownerNode.getOwnerCanvas();
  canvas.endConnectionDrag(e.getEventRelativeTo(&canvas).position);
}

} // namespace Teul
