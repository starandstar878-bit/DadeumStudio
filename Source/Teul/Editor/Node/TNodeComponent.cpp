#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/Editor/Node/NodePreviewRenderer.h"
#include "Teul/Editor/Theme/TeulPalette.h"
#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include <algorithm>

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
    addAndMakeVisible(comp.get());

    if (portData.direction == TPortDirection::Input)
      inPorts.push_back(std::move(comp));
    else
      outPorts.push_back(std::move(comp));
  }

  recalculateHeight();

  setTopLeftPosition(
      ownerCanvas.worldToView({nodePtr->x, nodePtr->y}).roundToInt());
}

void TNodeComponent::recalculateHeight() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  const auto size = measureNodeSize(descriptor, (int)inPorts.size(),
                                    (int)outPorts.size(), collapsed);
  setSize(size.x, size.y);
}

void TNodeComponent::resized() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;

  int y = headerHeight + 8;
  for (auto &p : inPorts) {
    p->setBounds(-7, y, 14, 14);
    p->setVisible(!collapsed);
    y += portRowHeight;
  }

  y = headerHeight + 8;
  for (auto &p : outPorts) {
    p->setBounds(getWidth() - 7, y, 14, 14);
    p->setVisible(!collapsed);
    y += portRowHeight;
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

  const auto bounds = getLocalBounds().toFloat();

  if (bypassed)
    g.setOpacity(0.4f);

  g.setColour(TeulPalette::NodeBackground);
  g.fillRoundedRectangle(bounds, (float)cornerRadius);

  juce::Rectangle<float> headerBounds = bounds.withHeight((float)headerHeight);
  juce::Colour headerColor = TeulPalette::NodeHeader;
  if (nodePtr != nullptr && nodePtr->colorTag.isNotEmpty())
    headerColor = colorFromTag(nodePtr->colorTag).withAlpha(0.75f);
  g.setColour(headerColor);

  if (collapsed) {
    g.fillRoundedRectangle(headerBounds, (float)cornerRadius);
  } else {
    g.fillRoundedRectangle(headerBounds, (float)cornerRadius);
    g.fillRect(headerBounds.withTop(headerBounds.getBottom() - (float)cornerRadius));
  }

  juce::String title = descriptor ? descriptor->displayName : "Unknown";
  g.setFont(juce::FontOptions(14.0f, juce::Font::bold));

  if (hasError) {
    g.setColour(juce::Colour(0xfff87171));
    title = juce::String::fromUTF8("\xe2\x9a\xa0 ") + title;
  } else {
    g.setColour(juce::Colours::white);
  }

  g.drawText(title, headerBounds.reduced(8.0f, 0).toNearestInt(),
             juce::Justification::centredLeft);

  const auto colBtn = getCollapseButtonBounds().toFloat();
  g.setColour(isHoveringCollapse ? juce::Colours::white
                                 : juce::Colours::lightgrey);
  g.drawText(collapsed ? "+" : "-", colBtn.toNearestInt(),
             juce::Justification::centred);

  if (!collapsed) {
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(11.0f));

    int yIn = headerHeight + 8 - 1;
    for (auto &p : inPorts) {
      g.drawText(p->getPortData().name, 12, yIn, 72, 14,
                 juce::Justification::centredLeft);
      yIn += portRowHeight;
    }

    int yOut = headerHeight + 8 - 1;
    for (auto &p : outPorts) {
      g.drawText(p->getPortData().name, getWidth() - 12 - 72, yOut, 72, 14,
                 juce::Justification::centredRight);
      yOut += portRowHeight;
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
    g.drawRoundedRectangle(bounds, (float)cornerRadius, 1.5f);
  } else {
    g.setColour(TeulPalette::NodeBorder);
    g.drawRoundedRectangle(bounds, (float)cornerRadius, 1.0f);
  }

  if (hasError) {
    g.setColour(juce::Colour(0x60f87171));
    g.drawRoundedRectangle(bounds.expanded(2.0f), (float)cornerRadius + 2.0f, 2.0f);
  }
}

juce::Rectangle<int> TNodeComponent::getCollapseButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 28, (headerHeight - 20) / 2, 20,
                              20);
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
