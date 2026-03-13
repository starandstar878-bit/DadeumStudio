#include "Teul2/Editor/Renderers/TNodeRenderer.h"
#include "Teul2/Editor/Theme/TeulPalette.h"
#include "Teul2/Editor/Canvas/TGraphCanvas.h"
#include "Teul/Registry/TNodeRegistry.h"

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

static juce::Colour probeColourForPortType(TPortDataType type) {
  switch (type) {
  case TPortDataType::Audio:
    return TeulPalette::PortAudio();
  case TPortDataType::CV:
    return TeulPalette::PortCV();
  case TPortDataType::Gate:
    return TeulPalette::PortGate();
  case TPortDataType::MIDI:
    return TeulPalette::PortMIDI();
  case TPortDataType::Control:
    return TeulPalette::PortControl();
  default:
    return juce::Colour(0xff60a5fa);
  }
}

static juce::Colour heatColourForLevel(float heatLevel) {
  return juce::Colour(0xfffb923c)
      .interpolatedWith(juce::Colour(0xffef4444),
                        juce::jlimit(0.0f, 1.0f, heatLevel * 1.1f));
}

static juce::String laneCaptionText(const std::vector<std::unique_ptr<TPortComponent>> &ports,
                                    juce::StringRef singularLabel,
                                    juce::StringRef pluralLabel) {
  if (ports.size() <= 1)
    return {};

  return juce::String((int)ports.size()) + " " +
         (ports.size() == 1 ? juce::String(singularLabel)
                            : juce::String(pluralLabel));
}

static std::vector<std::unique_ptr<TPortComponent>> buildPortComponents(
    TNodeComponent &owner, const std::vector<TPort> &ports,
    TPortDirection direction, float viewScale) {
  std::vector<TPort> directionalPorts;
  for (const auto &port : ports) {
    if (port.direction == direction)
      directionalPorts.push_back(port);
  }

  std::vector<std::unique_ptr<TPortComponent>> result;

  for (size_t i = 0; i < directionalPorts.size(); ) {
    std::vector<TPort> group;
    group.push_back(directionalPorts[i]);
    size_t j = i + 1;

    // channelIndex 媛믪씠 ?댁쟾 ?ы듃蹂대떎 1留뚰겮 利앷??섎뒗 寃쎌슦 ?곗냽??Bus 臾띠쓬?쇰줈 媛꾩＜ (dataType???숈씪?댁빞 ??
    while (j < directionalPorts.size() &&
           directionalPorts[j].channelIndex == directionalPorts[j - 1].channelIndex + 1 &&
           directionalPorts[j].dataType == group[0].dataType) {
      group.push_back(directionalPorts[j]);
      ++j;
    }

    auto component = std::make_unique<TPortComponent>(owner, std::move(group));
    component->setScaleFactor(viewScale);
    result.push_back(std::move(component));

    i = j;
  }

  return result;
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

  inPorts = buildPortComponents(*this, nodePtr->ports, TPortDirection::Input,
                                viewScale);
  outPorts = buildPortComponents(*this, nodePtr->ports, TPortDirection::Output,
                                 viewScale);

  for (auto &port : inPorts)
    addAndMakeVisible(port.get());
  for (auto &port : outPorts)
    addAndMakeVisible(port.get());

  updatePortIssueStates();
  recalculateHeight();
}

void TNodeComponent::recalculateHeight() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  logicalSize = measureNodeSize(descriptor, (int)inPorts.size(),
                                (int)outPorts.size(), collapsed);

  if (!collapsed) {
    const int rowGap = portRowGapLogical();
    const auto stackHeightForPorts = [this, rowGap](const auto &ports) {
      if (ports.empty())
        return 0;

      int total = 0;
      for (size_t index = 0; index < ports.size(); ++index) {
        total += juce::roundToInt((float)ports[index]->getHeight() / viewScale);
        if (index + 1 < ports.size())
          total += rowGap;
      }
      return total;
    };

    const int stackHeight = juce::jmax(stackHeightForPorts(inPorts),
                                       stackHeightForPorts(outPorts));
    const auto previewKind = inlinePreviewKindFor(descriptor);
    const int laneCaptionHeight = laneCaptionLogicalHeight();
    const int desiredHeight = headerHeight + 6 + laneCaptionHeight + stackHeight + 10 +
                              previewHeightForKind(previewKind);
    logicalSize.y = juce::jmax(logicalSize.y, desiredHeight);

    const int desiredWidth = juce::jmax(
        logicalSize.x,
        juce::roundToInt((float)(inputLaneWidthPx() + outputLaneWidthPx()) /
                         viewScale) + 92);
    logicalSize.x = juce::jmax(logicalSize.x, desiredWidth);
  }

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

void TNodeComponent::updatePortIssueStates() {
  const auto &document = ownerCanvas.getDocument();
  const auto connectionIssueForPort =
      [&](const TPort &port, const TConnection &connection) -> TIssueState {
    const TEndpoint *otherEndpoint = nullptr;
    if (connection.from.isNodePort() && connection.from.nodeId == nodeId &&
        connection.from.portId == port.portId) {
      otherEndpoint = &connection.to;
    } else if (connection.to.isNodePort() && connection.to.nodeId == nodeId &&
               connection.to.portId == port.portId) {
      otherEndpoint = &connection.from;
    }

    if (otherEndpoint == nullptr)
      return TIssueState::none;

    if (otherEndpoint->isRailPort()) {
      const auto *endpoint =
          document.controlState.findEndpoint(otherEndpoint->railEndpointId);
      if (endpoint == nullptr)
        return TIssueState::invalidConfig;
      if (endpoint->missing)
        return TIssueState::missing;
      if (endpoint->degraded)
        return TIssueState::degraded;

      const auto *railPort = document.findSystemRailPort(
          otherEndpoint->railEndpointId, otherEndpoint->railPortId);
      if (railPort == nullptr)
        return TIssueState::invalidConfig;
      if (railPort->dataType != port.dataType)
        return TIssueState::invalidConfig;
      return TIssueState::none;
    }

    const auto *otherNode = document.findNode(otherEndpoint->nodeId);
    if (otherNode == nullptr)
      return TIssueState::invalidConfig;

    const auto *otherPort = otherNode->findPort(otherEndpoint->portId);
    if (otherPort == nullptr)
      return TIssueState::invalidConfig;
    if (otherPort->dataType != port.dataType ||
        otherPort->direction == port.direction) {
      return TIssueState::invalidConfig;
    }

    return TIssueState::none;
  };

  const auto collectIssues = [&](const TPortComponent &component) {
    std::vector<TPortComponent::PortIssueState> portIssues;
    TIssueState bundleIssue = TIssueState::none;
    bool allPortsAffected = !component.getPortGroup().empty();

    for (const auto &port : component.getPortGroup()) {
      TIssueState issueState = TIssueState::none;
      for (const auto &connection : document.connections)
        issueState = mergeIssueState(issueState, connectionIssueForPort(port, connection));

      if (hasIssueState(issueState))
        portIssues.push_back({port.portId, issueState});
      else
        allPortsAffected = false;

      bundleIssue = mergeIssueState(bundleIssue, issueState);
    }

    if (!allPortsAffected)
      bundleIssue = TIssueState::none;

    return std::pair<std::vector<TPortComponent::PortIssueState>, TIssueState>(
        std::move(portIssues), bundleIssue);
  };

  for (auto &port : inPorts) {
    auto [portIssues, bundleIssue] = collectIssues(*port);
    port->setIssueState(std::move(portIssues), bundleIssue);
  }

  for (auto &port : outPorts) {
    auto [portIssues, bundleIssue] = collectIssues(*port);
    port->setIssueState(std::move(portIssues), bundleIssue);
  }
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

int TNodeComponent::portRowGapLogical() const noexcept {
  return juce::jmax(inPorts.size(), outPorts.size()) >= 4 ? 8 : 6;
}

int TNodeComponent::laneCaptionLogicalHeight() const noexcept {
  if (inPorts.size() <= 1 && outPorts.size() <= 1)
    return 0;
  return juce::jmax(inPorts.size(), outPorts.size()) >= 4 ? 12 : 10;
}

void TNodeComponent::resized() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  const int headerHeightPx = scaledInt(headerHeight);
  const int rowGapPx = scaledInt(portRowGapLogical());
  const int laneCaptionHeightPx = scaledInt(laneCaptionLogicalHeight());
  const int portYInset = scaledInt(6);
  const int inputLaneWidth = inputLaneWidthPx();
  const int outputLaneWidth = outputLaneWidthPx();

  int y = headerHeightPx + portYInset + laneCaptionHeightPx;
  for (auto &p : inPorts) {
    const int portHeight = p->getHeight();
    p->setBounds(0, y, inputLaneWidth, portHeight);
    p->setVisible(!collapsed);
    y += portHeight + rowGapPx;
  }

  y = headerHeightPx + portYInset + laneCaptionHeightPx;
  for (auto &p : outPorts) {
    const int portHeight = p->getHeight();
    p->setBounds(getWidth() - outputLaneWidth, y, outputLaneWidth, portHeight);
    p->setVisible(!collapsed);
    y += portHeight + rowGapPx;
  }
}

TPortComponent *TNodeComponent::findPortComponent(PortId portId) noexcept {
  for (auto &p : inPorts) {
    if (p->containsPort(portId))
      return p.get();
  }

  for (auto &p : outPorts) {
    if (p->containsPort(portId))
      return p.get();
  }

  return nullptr;
}

const TPortComponent *
TNodeComponent::findPortComponent(PortId portId) const noexcept {
  for (const auto &p : inPorts) {
    if (p->containsPort(portId))
      return p.get();
  }

  for (const auto &p : outPorts) {
    if (p->containsPort(portId))
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

int TNodeComponent::inputLaneWidthPx() const noexcept {
  int width = 0;
  for (const auto &port : inPorts)
    width = juce::jmax(width, port->getWidth());
  return width;
}

int TNodeComponent::outputLaneWidthPx() const noexcept {
  int width = 0;
  for (const auto &port : outPorts)
    width = juce::jmax(width, port->getWidth());
  return width;
}

void TNodeComponent::paint(juce::Graphics &g) {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool bypassed = nodePtr ? nodePtr->bypassed : false;
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  const bool hasError = nodePtr ? nodePtr->hasError : false;
  const auto previewKind = inlinePreviewKindFor(descriptor);
  const float cornerRadiusPx = scaledFloat((float)cornerRadius);
  const float headerHeightPx = scaledFloat((float)headerHeight);
  const float titleFontPx = juce::jmax(7.0f, scaledFloat(13.0f));
  const float bodyFontPx = juce::jmax(6.0f, scaledFloat(10.2f));
  const int labelInsetPx = juce::jmax(4, scaledInt(10));
  const int labelWidthPx = juce::jmax(22, scaledInt(68));
  const int portTextHeightPx = juce::jmax(8, scaledInt(12));
  const float borderThicknessPx = juce::jmax(1.0f, 1.0f * viewScale);
  const float borderSelectedThicknessPx = juce::jmax(1.2f, 1.5f * viewScale);
  const float errorGlowExpandPx = juce::jmax(1.0f, 2.0f * viewScale);
  const float errorGlowThicknessPx = juce::jmax(1.2f, 2.0f * viewScale);

  const auto bounds = getLocalBounds().toFloat();
  const auto &runtimeViewOptions = ownerCanvas.getRuntimeViewOptions();
  const float estimatedCpuCost = descriptor != nullptr
                                     ? (float)descriptor->capabilities.estimatedCpuCost
                                     : 0.0f;
  const float heatLevel = runtimeViewOptions.heatmapEnabled && descriptor != nullptr
                              ? juce::jlimit(0.0f, 1.0f,
                                             (estimatedCpuCost - 0.75f) / 4.25f)
                              : 0.0f;
  const juce::Colour heatColour = heatColourForLevel(heatLevel);
  const juce::Colour nodeFill = TeulPalette::NodeBackground().interpolatedWith(
      heatColour.darker(0.78f), heatLevel * 0.58f);

  if (bypassed)
    g.setOpacity(0.4f);

  g.setColour(nodeFill);
  g.fillRoundedRectangle(bounds, cornerRadiusPx);

  if (!collapsed && heatLevel > 0.04f) {
    auto heatStrip = juce::Rectangle<float>(
        bounds.getX() + scaledFloat(5.0f), headerHeightPx + scaledFloat(5.0f),
        juce::jmax(3.0f, scaledFloat(5.0f)),
        juce::jmax(16.0f,
                   bounds.getHeight() - headerHeightPx - scaledFloat(10.0f)));
    g.setGradientFill(juce::ColourGradient(heatColour.withAlpha(0.92f),
                                           heatStrip.getCentreX(),
                                           heatStrip.getBottom(),
                                           heatColour.withAlpha(0.18f),
                                           heatStrip.getCentreX(),
                                           heatStrip.getY(), false));
    g.fillRoundedRectangle(heatStrip, heatStrip.getWidth() * 0.5f);
  }

  juce::Rectangle<float> headerBounds = bounds.withHeight(headerHeightPx);
  juce::Colour headerColor = TeulPalette::NodeHeader();
  if (nodePtr != nullptr && nodePtr->colorTag.isNotEmpty())
    headerColor = colorFromTag(nodePtr->colorTag).withAlpha(0.75f);
  else if (heatLevel > 0.0f)
    headerColor = headerColor.interpolatedWith(heatColour.darker(0.48f),
                                               heatLevel * 0.34f);
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

  g.drawText(title, headerBounds.reduced(scaledFloat(7.0f), 0).toNearestInt(),
             juce::Justification::centredLeft);

  const auto colBtn = getCollapseButtonBounds().toFloat();
  g.setColour(isHoveringCollapse ? juce::Colours::white
                                 : juce::Colours::lightgrey);
  g.drawText(collapsed ? "+" : "-", colBtn.toNearestInt(),
             juce::Justification::centred);

  if (!collapsed) {
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(bodyFontPx));

    const int inputLaneWidth = inputLaneWidthPx();
    const int outputLaneWidth = outputLaneWidthPx();
    const int labelPaddingPx = scaledInt(6);
    const bool densePortStack = juce::jmax(inPorts.size(), outPorts.size()) >= 4;
    const int laneCaptionHeightPx = scaledInt(laneCaptionLogicalHeight());
    const int inputLabelX = juce::jmax(labelInsetPx, inputLaneWidth + labelPaddingPx);
    const int outputLabelRight =
        juce::jmax(inputLabelX + labelWidthPx + scaledInt(8),
                   getWidth() - outputLaneWidth - labelPaddingPx);
    const int outputLabelX = juce::jmax(inputLabelX + scaledInt(8),
                                        outputLabelRight - labelWidthPx);

    if (laneCaptionHeightPx > 0) {
      g.setColour(juce::Colours::white.withAlpha(densePortStack ? 0.58f : 0.4f));
      g.setFont(juce::FontOptions(
          juce::jmax(6.2f, scaledFloat(densePortStack ? 8.6f : 8.2f)),
          juce::Font::bold));
      const int captionY = juce::roundToInt(headerHeightPx + scaledFloat(1.0f));
      const auto inputCaption = laneCaptionText(inPorts, "input", "inputs");
      if (inputCaption.isNotEmpty())
        g.drawText(inputCaption, inputLabelX, captionY, labelWidthPx, laneCaptionHeightPx,
                   juce::Justification::centredLeft);
      const auto outputCaption = laneCaptionText(outPorts, "output", "outputs");
      if (outputCaption.isNotEmpty())
        g.drawText(outputCaption, outputLabelX, captionY, labelWidthPx, laneCaptionHeightPx,
                   juce::Justification::centredRight);
      g.setColour(juce::Colours::white.withAlpha(densePortStack ? 0.78f : 0.68f));
      g.setFont(juce::FontOptions(bodyFontPx));
    }

    for (auto &p : inPorts) {
      const auto labelY = p->getBounds().getCentreY() - portTextHeightPx / 2;
      g.drawText(p->getDisplayName(), inputLabelX, labelY, labelWidthPx,
                 portTextHeightPx, juce::Justification::centredLeft);
    }

    for (auto &p : outPorts) {
      const auto labelY = p->getBounds().getCentreY() - portTextHeightPx / 2;
      g.drawText(p->getDisplayName(), outputLabelX, labelY, labelWidthPx,
                 portTextHeightPx, juce::Justification::centredRight);
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
      case InlinePreviewKind::meterTall:
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

    if (runtimeViewOptions.liveProbeEnabled && !outPorts.empty()) {
      const float railWidth = juce::jmax(3.0f, scaledFloat(isSelected ? 6.0f : 4.0f));
      const float railInset = scaledFloat(5.0f);
      const float railHeight = juce::jmax(9.0f, scaledFloat(10.0f));
      const float outputLaneLeft = (float)(getWidth() - outputLaneWidthPx());

      for (const auto &port : outPorts) {
        const auto &portData = port->getPortData();
        const float level = ownerCanvas.getPortLevel(portData.portId);
        const juce::Colour probeColour = probeColourForPortType(portData.dataType);
        const float railY = (float)port->getBounds().getCentreY() - railHeight * 0.5f;
        auto railRect = juce::Rectangle<float>(outputLaneLeft + railInset,
                                               railY, railWidth, railHeight);
        g.setColour(juce::Colour(0xaa0f172a));
        g.fillRoundedRectangle(railRect, railWidth * 0.5f);

        auto fillRect = railRect.reduced(0.8f, 0.8f);
        fillRect.removeFromTop(fillRect.getHeight() *
                               (1.0f - juce::jlimit(0.0f, 1.0f, level)));
        if (!fillRect.isEmpty()) {
          g.setColour(probeColour.withAlpha(isSelected ? 0.95f : 0.72f));
          g.fillRoundedRectangle(fillRect, railWidth * 0.45f);
        }

        g.setColour(probeColour.withAlpha(isSelected ? 0.82f : 0.48f));
        g.drawRoundedRectangle(railRect, railWidth * 0.5f, 0.9f);
      }
    }

    if (runtimeViewOptions.liveProbeEnabled && isSelected && !outPorts.empty()) {
      const float probeWidth = juce::jmax(30.0f, scaledFloat(34.0f));
      const float probeHeight = juce::jmax(9.0f, scaledFloat(10.0f));
      const float probeInset = scaledFloat(6.0f);
      const float barInset = scaledFloat(2.0f);
      const float outputProbeRight = (float)(getWidth() - outputLaneWidthPx()) - probeInset;

      for (const auto &port : outPorts) {
        const auto &portData = port->getPortData();
        const float level = ownerCanvas.getPortLevel(portData.portId);
        const juce::Colour probeColour = probeColourForPortType(portData.dataType);
        const float probeY = (float)port->getBounds().getCentreY() - probeHeight * 0.5f;
        auto probeRect = juce::Rectangle<float>(
            outputProbeRight - probeWidth, probeY, probeWidth, probeHeight);
        auto barRect = probeRect.reduced(barInset, barInset);
        barRect.setWidth(barRect.getWidth() * juce::jlimit(0.0f, 1.0f, level));

        g.setColour(juce::Colour(0xcc0f172a));
        g.fillRoundedRectangle(probeRect, probeHeight * 0.5f);
        g.setColour(probeColour.withAlpha(0.82f));
        g.fillRoundedRectangle(barRect, juce::jmax(2.0f, probeHeight * 0.35f));
        g.setColour(probeColour.brighter(0.2f));
        g.drawRoundedRectangle(probeRect, probeHeight * 0.5f, 0.9f);
        g.setColour(juce::Colours::white.withAlpha(0.88f));
        g.setFont(juce::FontOptions(juce::jmax(6.0f, scaledFloat(8.0f))));
        g.drawText(juce::String(level, 2), probeRect.toNearestInt(),
                   juce::Justification::centred, false);
      }
    }
  }

  if (heatLevel > 0.08f) {
    g.setColour(heatColour.withAlpha(isSelected ? 0.62f : 0.34f));
    g.drawRoundedRectangle(bounds.reduced(scaledFloat(1.0f)),
                           juce::jmax(1.0f, cornerRadiusPx - scaledFloat(1.0f)),
                           juce::jmax(1.0f, scaledFloat(1.1f)));
  }

  if (isSelected) {
    g.setColour(TeulPalette::NodeBorderSelected());
    g.drawRoundedRectangle(bounds, cornerRadiusPx, borderSelectedThicknessPx);
  } else {
    g.setColour(TeulPalette::NodeBorder().interpolatedWith(heatColour,
                                                         heatLevel * 0.45f));
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
  const int buttonSize = juce::jmax(8, scaledInt(18));
  const int rightInset = juce::jmax(4, scaledInt(7));
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

#include "Teul2/Editor/Renderers/TNodeRenderer.h"

#include "Teul2/Editor/Theme/TeulPalette.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Teul {
namespace {

struct MeterBar {
  juce::String label;
  float level = 0.0f;
  juce::Colour colour;
};

constexpr int kNodeHeaderHeight = 30;
constexpr int kNodePortRowHeight = 20;
constexpr int kNodeMinWidth = 132;

static InlinePreviewKind inlinePreviewKindForImpl(const TNodeDescriptor *descriptor) {
  if (descriptor == nullptr)
    return InlinePreviewKind::none;

  if (descriptor->typeKey == "Teul.Source.Oscillator")
    return InlinePreviewKind::oscillator;
  if (descriptor->typeKey == "Teul.Mod.ADSR")
    return InlinePreviewKind::adsr;
  if (descriptor->typeKey == "Teul.Mixer.StereoMixer4" ||
      descriptor->typeKey == "Teul.Mixer.MonoMixer4")
    return InlinePreviewKind::meterTall;
  if (descriptor->category == "Mixer")
    return InlinePreviewKind::meter;

  return InlinePreviewKind::none;
}

static int previewHeightForKindImpl(InlinePreviewKind kind) {
  switch (kind) {
  case InlinePreviewKind::oscillator:
  case InlinePreviewKind::adsr:
    return 48;
  case InlinePreviewKind::meter:
    return 38;
  case InlinePreviewKind::meterTall:
    return 58;
  case InlinePreviewKind::none:
  default:
    return 0;
  }
}

static int minWidthForKindImpl(InlinePreviewKind kind) {
  switch (kind) {
  case InlinePreviewKind::oscillator:
  case InlinePreviewKind::adsr:
    return 152;
  case InlinePreviewKind::meter:
    return 158;
  case InlinePreviewKind::meterTall:
    return 172;
  case InlinePreviewKind::none:
  default:
    return 140;
  }
}

static const juce::var *findNodeParam(const TNode &node,
                                      const juce::String &key) {
  const auto it = node.params.find(key);
  if (it == node.params.end())
    return nullptr;
  return &it->second;
}

static double paramAsDouble(const TNode &node, const juce::String &key,
                            double fallback) {
  if (const auto *value = findNodeParam(node, key)) {
    if (value->isBool())
      return (bool)*value ? 1.0 : 0.0;
    if (value->isInt())
      return (double)(int)*value;
    if (value->isInt64())
      return (double)(juce::int64)*value;
    if (value->isDouble())
      return (double)*value;
    if (value->isString())
      return value->toString().getDoubleValue();
    return (double)*value;
  }

  return fallback;
}

static int paramAsInt(const TNode &node, const juce::String &key, int fallback) {
  if (const auto *value = findNodeParam(node, key)) {
    if (value->isInt())
      return (int)*value;
    if (value->isInt64())
      return (int)(juce::int64)*value;
    if (value->isBool())
      return (bool)*value ? 1 : 0;
    return juce::roundToInt((float)paramAsDouble(node, key, (double)fallback));
  }

  return fallback;
}

static float clampUnit(float value) {
  return juce::jlimit(0.0f, 1.0f, value);
}

static juce::Colour meterColourForPortType(TPortDataType dataType) {
  switch (dataType) {
  case TPortDataType::Audio:
    return TeulPalette::PortAudio();
  case TPortDataType::CV:
    return TeulPalette::PortCV();
  case TPortDataType::Gate:
    return TeulPalette::PortGate();
  case TPortDataType::Control:
    return TeulPalette::PortControl();
  case TPortDataType::MIDI:
  default:
    return TeulPalette::PortMIDI();
  }
}

static juce::String meterLabelForPort(const juce::String &name) {
  const juce::String upper = name.trim().toUpperCase();
  if (upper.length() <= 3)
    return upper;
  if (upper.contains("L IN") || upper.contains("LEFT") || upper == "L")
    return "L";
  if (upper.contains("R IN") || upper.contains("RIGHT") || upper == "R")
    return "R";
  if (upper.contains("OUT"))
    return "OUT";
  return upper.substring(0, 3);
}

static juce::Rectangle<float>
makePreviewBoundsImpl(const juce::Rectangle<float> &bounds, InlinePreviewKind kind) {
  const auto height = (float)previewHeightForKindImpl(kind);
  if (height <= 0.0f)
    return {};

  auto preview = bounds.withTrimmedTop(bounds.getHeight() - (height + 8.0f));
  preview = preview.reduced(8.0f, 0.0f);
  preview.removeFromBottom(5.0f);
  return preview;
}

static void drawPreviewPanel(juce::Graphics &g,
                             const juce::Rectangle<float> &bounds,
                             juce::Colour accent) {
  if (bounds.isEmpty())
    return;

  juce::ColourGradient fill(accent.withAlpha(0.12f), bounds.getX(),
                            bounds.getY(), juce::Colour(0xff181818),
                            bounds.getRight(), bounds.getBottom(), false);
  g.setGradientFill(fill);
  g.fillRoundedRectangle(bounds, 6.0f);

  g.setColour(juce::Colours::black.withAlpha(0.16f));
  g.drawRoundedRectangle(bounds.translated(0.0f, 1.0f), 6.0f, 1.0f);

  g.setColour(accent.withAlpha(0.32f));
  g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

static void drawOscillatorPreviewImpl(juce::Graphics &g,
                                  const juce::Rectangle<float> &bounds,
                                  const TNode &node) {
  if (bounds.isEmpty())
    return;

  const int waveformIndex = juce::jlimit(0, 3, paramAsInt(node, "waveform", 0));
  const double frequency = juce::jlimit(20.0, 20000.0,
                                        paramAsDouble(node, "frequency", 440.0));
  const float gain = juce::jlimit(0.0f, 1.0f,
                                  (float)paramAsDouble(node, "gain", 0.707));
  const float logNorm = clampUnit(
      (float)((std::log10(frequency) - std::log10(20.0)) /
              (std::log10(20000.0) - std::log10(20.0))));
  const float cycles = 1.15f + logNorm * 2.75f;
  const float amplitude = 0.25f + gain * 0.55f;

  drawPreviewPanel(g, bounds, TeulPalette::PortAudio());

  auto area = bounds.reduced(7.0f, 6.0f);
  const auto baselineY = area.getCentreY();
  const auto usableHeight = area.getHeight() * 0.5f * amplitude;

  g.setColour(juce::Colours::white.withAlpha(0.08f));
  g.drawHorizontalLine((int)std::round(baselineY), area.getX(), area.getRight());

  juce::Path waveform;
  const int sampleCount = juce::jmax(48, (int)std::round(area.getWidth() * 1.4f));
  for (int i = 0; i < sampleCount; ++i) {
    const float alpha = sampleCount > 1 ? (float)i / (float)(sampleCount - 1) : 0.0f;
    const float phase = alpha * cycles;
    const float localPhase = phase - std::floor(phase);

    float value = 0.0f;
    switch (waveformIndex) {
    case 1:
      value = 1.0f - 4.0f * std::abs(localPhase - 0.5f);
      break;
    case 2:
      value = localPhase < 0.5f ? 1.0f : -1.0f;
      break;
    case 3:
      value = 1.0f - 2.0f * localPhase;
      break;
    case 0:
    default:
      value = std::sin(phase * juce::MathConstants<float>::twoPi);
      break;
    }

    const float x = area.getX() + alpha * area.getWidth();
    const float y = baselineY - value * usableHeight;
    if (i == 0)
      waveform.startNewSubPath(x, y);
    else
      waveform.lineTo(x, y);
  }

  g.setColour(TeulPalette::PortAudio().withAlpha(0.18f));
  g.strokePath(waveform,
               juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));
  g.setColour(TeulPalette::PortAudio().withAlpha(0.92f));
  g.strokePath(waveform,
               juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));

  static const char *waveLabels[] = {"SINE", "TRI", "SQR", "SAW"};
  const auto waveLabelBounds = bounds.withHeight(14.0f).reduced(8.0f, 0.0f);
  g.setColour(juce::Colours::white.withAlpha(0.62f));
  g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  g.drawText(waveLabels[waveformIndex], waveLabelBounds.toNearestInt(),
             juce::Justification::centredRight, false);
}

static void drawAdsrPreviewImpl(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                            const TNode &node) {
  if (bounds.isEmpty())
    return;

  const float attack = (float)juce::jmax(1.0, paramAsDouble(node, "attack", 10.0));
  const float decay = (float)juce::jmax(1.0, paramAsDouble(node, "decay", 100.0));
  const float sustain = juce::jlimit(0.0f, 1.0f,
                                     (float)paramAsDouble(node, "sustain", 0.5));
  const float release = (float)juce::jmax(1.0, paramAsDouble(node, "release", 300.0));

  drawPreviewPanel(g, bounds, TeulPalette::PortCV());

  auto area = bounds.reduced(7.0f, 7.0f);
  const float bottom = area.getBottom();
  const float top = area.getY() + 2.0f;
  const float sustainY = juce::jmap(sustain, bottom - 2.0f, top + 2.0f);

  const float total = attack + decay + release + 220.0f;
  const float width = area.getWidth();
  const float attackWidth = juce::jmax(16.0f, width * (attack / total));
  const float decayWidth = juce::jmax(18.0f, width * (decay / total));
  const float releaseWidth = juce::jmax(18.0f, width * (release / total));
  const float sustainWidth = juce::jmax(18.0f, width - attackWidth - decayWidth - releaseWidth);

  const float x0 = area.getX();
  const float x1 = x0 + attackWidth;
  const float x2 = x1 + decayWidth;
  const float x3 = x2 + sustainWidth;
  const float x4 = area.getRight();

  g.setColour(juce::Colours::white.withAlpha(0.08f));
  g.drawHorizontalLine((int)std::round(bottom - 1.0f), area.getX(), area.getRight());

  juce::Path env;
  env.startNewSubPath(x0, bottom - 1.0f);
  env.lineTo(x1, top + 1.0f);
  env.lineTo(x2, sustainY);
  env.lineTo(x3, sustainY);
  env.lineTo(x4, bottom - 1.0f);

  g.setColour(TeulPalette::PortCV().withAlpha(0.16f));
  g.strokePath(env,
               juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));
  g.setColour(TeulPalette::PortCV().withAlpha(0.95f));
  g.strokePath(env,
               juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));

  g.setColour(TeulPalette::PortCV().withAlpha(0.78f));
  for (const auto point : {juce::Point<float>(x1, top + 1.0f),
                           juce::Point<float>(x2, sustainY),
                           juce::Point<float>(x3, sustainY)}) {
    g.fillEllipse(point.x - 2.0f, point.y - 2.0f, 4.0f, 4.0f);
  }

  const auto adsrLabelBounds = bounds.withHeight(14.0f).reduced(8.0f, 0.0f);
  g.setColour(juce::Colours::white.withAlpha(0.6f));
  g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  g.drawText("ADSR", adsrLabelBounds.toNearestInt(),
             juce::Justification::centredRight, false);
}

static std::vector<MeterBar> buildMeterBars(const TNodeDescriptor *descriptor,
                                            const TNode &node,
                                            const PortLevelReader &portLevelReader) {
  std::vector<MeterBar> bars;
  if (descriptor == nullptr)
    return bars;

  if (portLevelReader) {
    for (const auto &port : node.ports) {
      if (port.direction != TPortDirection::Output ||
          port.dataType == TPortDataType::MIDI) {
        continue;
      }

      bars.push_back({meterLabelForPort(port.name),
                      clampUnit(portLevelReader(port.portId)),
                      meterColourForPortType(port.dataType)});
    }

    if (!bars.empty())
      return bars;
  }

  if (descriptor->typeKey == "Teul.Mixer.VCA") {
    const float gain = juce::jlimit(0.0f, 1.0f,
                                    (float)paramAsDouble(node, "gain", 1.0) / 2.0f);
    bars.push_back({"OUT", gain, TeulPalette::PortAudio()});
    return bars;
  }

  if (descriptor->typeKey == "Teul.Mixer.Mixer2") {
    const float gain1 = clampUnit((float)paramAsDouble(node, "gain1", 1.0));
    const float gain2 = clampUnit((float)paramAsDouble(node, "gain2", 1.0));
    bars.push_back({"A", gain1, TeulPalette::PortAudio()});
    bars.push_back({"B", gain2, TeulPalette::PortCV()});
    bars.push_back({"SUM", clampUnit((gain1 + gain2) * 0.5f),
                    juce::Colour(0xfff59e0b)});
    return bars;
  }

  if (descriptor->typeKey == "Teul.Mixer.StereoMixer4" ||
      descriptor->typeKey == "Teul.Mixer.MonoMixer4") {
    static const juce::Colour busColours[] = {
        juce::Colour(0xff22c55e),
        juce::Colour(0xff38bdf8),
        juce::Colour(0xfff59e0b),
        juce::Colour(0xfff97316),
    };
    for (int busIndex = 0; busIndex < 4; ++busIndex) {
      const auto gain = clampUnit((float)paramAsDouble(
          node, "gain" + juce::String(busIndex + 1), 1.0));
      bars.push_back({juce::String(busIndex + 1), gain,
                      busColours[(size_t)busIndex]});
    }
    return bars;
  }

  if (descriptor->typeKey == "Teul.Mixer.StereoPanner") {
    const float pan = juce::jlimit(-1.0f, 1.0f,
                                   (float)paramAsDouble(node, "pan", 0.0));
    const float left = clampUnit(1.0f - juce::jmax(0.0f, pan));
    const float right = clampUnit(1.0f - juce::jmax(0.0f, -pan));
    bars.push_back({"L", left, TeulPalette::PortAudio()});
    bars.push_back({"R", right, TeulPalette::PortMIDI()});
    return bars;
  }

  float average = 0.0f;
  int numericCount = 0;
  for (const auto &param : descriptor->paramSpecs) {
    if (!param.defaultValue.isBool() && !param.defaultValue.isInt() &&
        !param.defaultValue.isInt64() && !param.defaultValue.isDouble()) {
      continue;
    }

    const float value = clampUnit((float)paramAsDouble(node, param.key, 0.0));
    average += value;
    ++numericCount;
  }

  bars.push_back({"LVL", numericCount > 0 ? average / (float)numericCount : 0.0f,
                  TeulPalette::PortAudio()});
  return bars;
}

static void drawMeterPreviewImpl(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                             const TNodeDescriptor *descriptor,
                             const TNode &node,
                             const PortLevelReader &portLevelReader) {
  if (bounds.isEmpty())
    return;

  const auto bars = buildMeterBars(descriptor, node, portLevelReader);
  if (bars.empty())
    return;

  drawPreviewPanel(g, bounds, juce::Colour(0xff38bdf8));

  auto area = bounds.reduced(7.0f, 6.0f);
  const float rowGap = 3.0f;
  const float rowHeight = (area.getHeight() - rowGap * (float)(bars.size() - 1)) /
                          (float)bars.size();

  g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
  for (size_t i = 0; i < bars.size(); ++i) {
    auto row = juce::Rectangle<float>(area.getX(),
                                      area.getY() + (float)i * (rowHeight + rowGap),
                                      area.getWidth(), rowHeight);
    auto label = row.removeFromLeft(18.0f);
    auto track = row.reduced(0.0f, 2.0f);
    const auto level = clampUnit(bars[i].level);

    g.setColour(juce::Colours::white.withAlpha(0.58f));
    g.drawText(bars[i].label, label.toNearestInt(), juce::Justification::centredLeft,
               false);

    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.fillRoundedRectangle(track, 3.0f);

    const float fillWidth = juce::jmax(3.0f, track.getWidth() * level);
    auto fill = juce::Rectangle<float>(track.getX(), track.getY(), fillWidth,
                                       track.getHeight());
    juce::ColourGradient fillGradient(
        bars[i].colour.withMultipliedBrightness(0.8f), fill.getX(), fill.getY(),
        bars[i].colour.brighter(0.35f), fill.getRight(), fill.getBottom(), false);
    g.setGradientFill(fillGradient);
    g.fillRoundedRectangle(fill, 3.0f);

    g.setColour(bars[i].colour.withAlpha(0.36f));
    g.drawRoundedRectangle(track, 3.0f, 1.0f);
  }
}

} // namespace

InlinePreviewKind inlinePreviewKindFor(const TNodeDescriptor *descriptor) {
  return inlinePreviewKindForImpl(descriptor);
}

int previewHeightForKind(InlinePreviewKind kind) {
  return previewHeightForKindImpl(kind);
}

int minWidthForKind(InlinePreviewKind kind) {
  return minWidthForKindImpl(kind);
}

juce::Point<int> measureNodeSize(const TNodeDescriptor *descriptor,
                                 int inputPortCount,
                                 int outputPortCount,
                                 bool collapsed) {
  const auto previewKind = inlinePreviewKindForImpl(descriptor);
  const int width = juce::jmax(kNodeMinWidth, minWidthForKindImpl(previewKind));
  if (collapsed)
    return {width, kNodeHeaderHeight};

  const int maxPortCount = juce::jmax(inputPortCount, outputPortCount);
  const int height = kNodeHeaderHeight +
                     maxPortCount * kNodePortRowHeight + 16 +
                     previewHeightForKindImpl(previewKind);
  return {width, height};
}

juce::Rectangle<float> makePreviewBounds(const juce::Rectangle<float> &bounds,
                                         InlinePreviewKind kind) {
  return makePreviewBoundsImpl(bounds, kind);
}

void drawOscillatorPreview(juce::Graphics &g,
                           const juce::Rectangle<float> &bounds,
                           const TNode &node) {
  drawOscillatorPreviewImpl(g, bounds, node);
}

void drawAdsrPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                     const TNode &node) {
  drawAdsrPreviewImpl(g, bounds, node);
}

void drawMeterPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                      const TNodeDescriptor *descriptor, const TNode &node,
                      const PortLevelReader &portLevelReader) {
  drawMeterPreviewImpl(g, bounds, descriptor, node, portLevelReader);
}

} // namespace Teul
