#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/Editor/Node/NodePreviewRenderer.h"
#include "Teul/Editor/Theme/TeulPalette.h"
#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include <algorithm>
#include <cmath>

namespace Teul {
namespace {

static juce::Colour colorFromTag(const juce::String &tagRaw) {
  const juce::String tag = tagRaw.trim().toLowerCase();
  if (tag == "red")
    return juce::Colour(0xffef4444);
  if (tag == "orange")
    return juce::Colour(0xfff59e0b);
  if (tag == "green")
    return juce::Colour(0xff22c55e);
  if (tag == "blue")
    return juce::Colour(0xff3b82f6);
  return juce::Colours::transparentBlack;
}

} // namespace

TNodeComponent::TNodeComponent(TGraphCanvas &canvas, NodeId id,
                               const TNodeDescriptor *desc)
    : ownerCanvas(canvas), nodeId(id), descriptor(desc) {
  setRepaintsOnMouseActivity(true);
  updateFromModel();
}

TNodeComponent::~TNodeComponent() = default;

void TNodeComponent::updateFromModel() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  if (!nodePtr)
    return;

  inPorts.clear();
  outPorts.clear();

  for (const auto &portData : nodePtr->ports) {
    auto comp = std::make_unique<TPortComponent>(*this, portData);
    comp->setScaleFactor(viewScale);
    addAndMakeVisible(comp.get());

    if (portData.direction == TPortDirection::Input)
      inPorts.push_back(std::move(comp));
    else
      outPorts.push_back(std::move(comp));
  }

  recalculateHeight();

}

void TNodeComponent::recalculateHeight() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  logicalSize = measureNodeSize(descriptor, (int)inPorts.size(),
                                (int)outPorts.size(), collapsed);
  applyViewScale();
}

void TNodeComponent::setViewScale(float newScale) {
  newScale = juce::jmax(0.1f, newScale);
  if (std::abs(viewScale - newScale) <= 0.0001f)
    return;

  viewScale = newScale;
  for (auto &port : inPorts)
    port->setScaleFactor(viewScale);
  for (auto &port : outPorts)
    port->setScaleFactor(viewScale);

  applyViewScale();
  resized();
}

void TNodeComponent::applyViewScale() {
  const int width = juce::jmax(1, scaledInt(logicalSize.x));
  const int height = juce::jmax(1, scaledInt(logicalSize.y));
  setSize(width, height);
}

int TNodeComponent::scaledInt(int value) const noexcept {
  return juce::roundToInt((float)value * viewScale);
}

float TNodeComponent::scaledFloat(float value) const noexcept {
  return value * viewScale;
}

void TNodeComponent::resized() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  const int headerHeightPx = scaledInt(headerHeight);
  const int portRowHeightPx = scaledInt(portRowHeight);
  const int portSizePx = juce::jmax(4, scaledInt(14));
  const int portHalf = portSizePx / 2;
  const int portYInset = scaledInt(8);

  int y = headerHeightPx + portYInset;
  for (auto &p : inPorts) {
    p->setBounds(-portHalf, y, portSizePx, portSizePx);
    p->setVisible(!collapsed);
    y += portRowHeightPx;
  }

  y = headerHeightPx + portYInset;
  for (auto &p : outPorts) {
    p->setBounds(getWidth() - portHalf, y, portSizePx, portSizePx);
    p->setVisible(!collapsed);
    y += portRowHeightPx;
  }
}

TPortComponent *TNodeComponent::findPortComponent(PortId portId) noexcept {
  for (auto &p : inPorts) {
    if (p->getPortData().portId == portId)
      return p.get();
  }

  for (auto &p : outPorts) {
    if (p->getPortData().portId == portId)
      return p.get();
  }

  return nullptr;
}

const TPortComponent *
TNodeComponent::findPortComponent(PortId portId) const noexcept {
  for (const auto &p : inPorts) {
    if (p->getPortData().portId == portId)
      return p.get();
  }

  for (const auto &p : outPorts) {
    if (p->getPortData().portId == portId)
      return p.get();
  }

  return nullptr;
}

void TNodeComponent::setPortDragHighlight(PortId portId, bool enabled,
                                          bool validType) {
  if (auto *portComp = findPortComponent(portId)) {
    portComp->setDragTargetHighlight(enabled, validType);
  }
}

void TNodeComponent::paint(juce::Graphics &g) {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool bypassed = nodePtr ? nodePtr->bypassed : false;
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  const bool hasError = nodePtr ? nodePtr->hasError : false;
  const auto previewKind = inlinePreviewKindFor(descriptor);
  const float cornerRadiusPx = scaledFloat((float)cornerRadius);
  const float headerHeightPx = scaledFloat((float)headerHeight);
  const float titleFontPx = juce::jmax(7.0f, scaledFloat(14.0f));
  const float bodyFontPx = juce::jmax(6.0f, scaledFloat(11.0f));
  const int labelInsetPx = juce::jmax(4, scaledInt(12));
  const int labelWidthPx = juce::jmax(24, scaledInt(72));
  const int portTextHeightPx = juce::jmax(8, scaledInt(14));
  const int portTextStartYPx = scaledInt(headerHeight + 8) - scaledInt(1);
  const int portRowHeightPx = scaledInt(portRowHeight);
  const float borderThicknessPx = juce::jmax(1.0f, 1.0f * viewScale);
  const float borderSelectedThicknessPx = juce::jmax(1.2f, 1.5f * viewScale);
  const float errorGlowExpandPx = juce::jmax(1.0f, 2.0f * viewScale);
  const float errorGlowThicknessPx = juce::jmax(1.2f, 2.0f * viewScale);

  const auto bounds = getLocalBounds().toFloat();

  if (bypassed)
    g.setOpacity(0.4f);

  g.setColour(TeulPalette::NodeBackground);
  g.fillRoundedRectangle(bounds, cornerRadiusPx);

  juce::Rectangle<float> headerBounds = bounds.withHeight(headerHeightPx);
  juce::Colour headerColor = TeulPalette::NodeHeader;
  if (nodePtr != nullptr && nodePtr->colorTag.isNotEmpty())
    headerColor = colorFromTag(nodePtr->colorTag).withAlpha(0.75f);
  g.setColour(headerColor);

  if (collapsed) {
    g.fillRoundedRectangle(headerBounds, cornerRadiusPx);
  } else {
    g.fillRoundedRectangle(headerBounds, cornerRadiusPx);
    g.fillRect(headerBounds.withTop(headerBounds.getBottom() - cornerRadiusPx));
  }

  juce::String title = descriptor ? descriptor->displayName : "Unknown";
  g.setFont(juce::FontOptions(titleFontPx, juce::Font::bold));

  if (hasError) {
    g.setColour(juce::Colour(0xfff87171));
    title = juce::String::fromUTF8("\xe2\x9a\xa0 ") + title;
  } else {
    g.setColour(juce::Colours::white);
  }

  g.drawText(title, headerBounds.reduced(scaledFloat(8.0f), 0).toNearestInt(),
             juce::Justification::centredLeft);

  const auto colBtn = getCollapseButtonBounds().toFloat();
  g.setColour(isHoveringCollapse ? juce::Colours::white
                                 : juce::Colours::lightgrey);
  g.drawText(collapsed ? "+" : "-", colBtn.toNearestInt(),
             juce::Justification::centred);

  if (!collapsed) {
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(bodyFontPx));

    int yIn = portTextStartYPx;
    for (auto &p : inPorts) {
      g.drawText(p->getPortData().name, labelInsetPx, yIn, labelWidthPx,
                 portTextHeightPx, juce::Justification::centredLeft);
      yIn += portRowHeightPx;
    }

    int yOut = portTextStartYPx;
    for (auto &p : outPorts) {
      g.drawText(p->getPortData().name,
                 getWidth() - labelInsetPx - labelWidthPx, yOut, labelWidthPx,
                 portTextHeightPx, juce::Justification::centredRight);
      yOut += portRowHeightPx;
    }

    if (nodePtr != nullptr) {
      const auto previewBounds = makePreviewBounds(bounds, previewKind);
      switch (previewKind) {
      case InlinePreviewKind::oscillator:
        drawOscillatorPreview(g, previewBounds, *nodePtr);
        break;
      case InlinePreviewKind::adsr:
        drawAdsrPreview(g, previewBounds, *nodePtr);
        break;
      case InlinePreviewKind::meter:
        drawMeterPreview(g, previewBounds, descriptor, *nodePtr,
                         [this](PortId portId) {
                           return ownerCanvas.getPortLevel(portId);
                         });
        break;
      case InlinePreviewKind::none:
      default:
        break;
      }
    }
  }

  if (isSelected) {
    g.setColour(TeulPalette::NodeBorderSelected);
    g.drawRoundedRectangle(bounds, cornerRadiusPx, borderSelectedThicknessPx);
  } else {
    g.setColour(TeulPalette::NodeBorder);
    g.drawRoundedRectangle(bounds, cornerRadiusPx, borderThicknessPx);
  }

  if (hasError) {
    g.setColour(juce::Colour(0x60f87171));
    g.drawRoundedRectangle(bounds.expanded(errorGlowExpandPx),
                           cornerRadiusPx + errorGlowExpandPx,
                           errorGlowThicknessPx);
  }
}

juce::Rectangle<int> TNodeComponent::getCollapseButtonBounds() const {
  const int buttonSize = juce::jmax(8, scaledInt(20));
  const int rightInset = juce::jmax(4, scaledInt(8));
  const int headerHeightPx = scaledInt(headerHeight);
  return juce::Rectangle<int>(getWidth() - rightInset - buttonSize,
                              (headerHeightPx - buttonSize) / 2, buttonSize,
                              buttonSize);
}

void TNodeComponent::mouseMove(const juce::MouseEvent &e) {
  const bool hover = getCollapseButtonBounds().contains(e.getPosition());

  if (isHoveringCollapse != hover) {
    isHoveringCollapse = hover;
    repaint(getCollapseButtonBounds());
  }
}

void TNodeComponent::mouseDown(const juce::MouseEvent &e) {
  ownerCanvas.grabKeyboardFocus();

  if (getCollapseButtonBounds().contains(e.getPosition()) &&
      e.mods.isLeftButtonDown()) {
    if (TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId)) {
      nodePtr->collapsed = !nodePtr->collapsed;
      ownerCanvas.getDocument().touch(false);
      recalculateHeight();
      resized();
      repaint();
      ownerCanvas.repaint();
    }
    return;
  }

  if (e.mods.isRightButtonDown()) {
    const auto viewPos = e.getEventRelativeTo(&ownerCanvas).position;
    const auto screenPos =
        ownerCanvas.localPointToGlobal(viewPos.roundToInt()).toFloat();
    ownerCanvas.requestNodeContextMenu(nodeId, viewPos, screenPos);
    return;
  }

  if (!e.mods.isLeftButtonDown())
    return;

  toFront(false);
  ownerCanvas.requestNodeMouseDown(nodeId, e);
}

void TNodeComponent::mouseDrag(const juce::MouseEvent &e) {
  if (!e.mods.isLeftButtonDown())
    return;

  ownerCanvas.requestNodeMouseDrag(nodeId, e);
}

void TNodeComponent::mouseUp(const juce::MouseEvent &e) {
  ownerCanvas.requestNodeMouseUp(nodeId, e);
}
} // namespace Teul
