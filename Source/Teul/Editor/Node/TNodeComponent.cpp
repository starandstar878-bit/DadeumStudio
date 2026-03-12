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

static juce::Colour probeColourForPortType(TPortDataType type) {
  switch (type) {
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
    return juce::Colour(0xff60a5fa);
  }
}

static juce::Colour heatColourForLevel(float heatLevel) {
  return juce::Colour(0xfffb923c)
      .interpolatedWith(juce::Colour(0xffef4444),
                        juce::jlimit(0.0f, 1.0f, heatLevel * 1.1f));
}

static bool splitStereoPortLabel(juce::StringRef name, bool &isLeft,
                                 juce::String &suffix) {
  const juce::String trimmed = juce::String(name).trim();
  if (trimmed.equalsIgnoreCase("L")) {
    isLeft = true;
    suffix.clear();
    return true;
  }
  if (trimmed.equalsIgnoreCase("R")) {
    isLeft = false;
    suffix.clear();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("L ")) {
    isLeft = true;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("R ")) {
    isLeft = false;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  return false;
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
  std::vector<bool> used(directionalPorts.size(), false);
  for (size_t index = 0; index < directionalPorts.size(); ++index) {
    if (used[index])
      continue;

    const auto &port = directionalPorts[index];
    std::vector<TPort> group;
    group.push_back(port);

    if (port.dataType == TPortDataType::Audio) {
      bool isLeft = false;
      juce::String suffix;
      if (splitStereoPortLabel(port.name, isLeft, suffix)) {
        for (size_t candidateIndex = index + 1; candidateIndex < directionalPorts.size(); ++candidateIndex) {
          if (used[candidateIndex])
            continue;

          const auto &candidate = directionalPorts[candidateIndex];
          if (candidate.dataType != port.dataType ||
              candidate.direction != port.direction)
            continue;

          bool candidateIsLeft = false;
          juce::String candidateSuffix;
          if (!splitStereoPortLabel(candidate.name, candidateIsLeft, candidateSuffix) ||
              candidateIsLeft == isLeft || candidateSuffix != suffix) {
            continue;
          }

          group.clear();
          if (isLeft) {
            group.push_back(port);
            group.push_back(candidate);
          } else {
            group.push_back(candidate);
            group.push_back(port);
          }
          used[candidateIndex] = true;
          break;
        }
      }
    }

    used[index] = true;
    auto component = std::make_unique<TPortComponent>(owner, std::move(group));
    component->setScaleFactor(viewScale);
    result.push_back(std::move(component));
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
    const auto stackHeightForPorts = [this](const auto &ports) {
      if (ports.empty())
        return 0;

      constexpr int rowGap = 6;
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
    const int desiredHeight = headerHeight + 8 + stackHeight + 12 +
                              previewHeightForKind(previewKind);
    logicalSize.y = juce::jmax(logicalSize.y, desiredHeight);

    const int desiredWidth = juce::jmax(
        logicalSize.x,
        juce::roundToInt((float)(inputLaneWidthPx() + outputLaneWidthPx()) /
                         viewScale) + 96);
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

void TNodeComponent::resized() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  const bool collapsed = nodePtr ? nodePtr->collapsed : false;
  const int headerHeightPx = scaledInt(headerHeight);
  const int rowGapPx = scaledInt(6);
  const int portYInset = scaledInt(8);
  const int inputLaneWidth = inputLaneWidthPx();
  const int outputLaneWidth = outputLaneWidthPx();

  int y = headerHeightPx + portYInset;
  for (auto &p : inPorts) {
    const int portHeight = p->getHeight();
    p->setBounds(0, y, inputLaneWidth, portHeight);
    p->setVisible(!collapsed);
    y += portHeight + rowGapPx;
  }

  y = headerHeightPx + portYInset;
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
  const float titleFontPx = juce::jmax(7.0f, scaledFloat(14.0f));
  const float bodyFontPx = juce::jmax(6.0f, scaledFloat(11.0f));
  const int labelInsetPx = juce::jmax(4, scaledInt(12));
  const int labelWidthPx = juce::jmax(24, scaledInt(72));
  const int portTextHeightPx = juce::jmax(8, scaledInt(14));
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
  const juce::Colour nodeFill = TeulPalette::NodeBackground.interpolatedWith(
      heatColour.darker(0.78f), heatLevel * 0.58f);

  if (bypassed)
    g.setOpacity(0.4f);

  g.setColour(nodeFill);
  g.fillRoundedRectangle(bounds, cornerRadiusPx);

  if (!collapsed && heatLevel > 0.04f) {
    auto heatStrip = juce::Rectangle<float>(
        bounds.getX() + scaledFloat(6.0f), headerHeightPx + scaledFloat(6.0f),
        juce::jmax(3.0f, scaledFloat(5.0f)),
        juce::jmax(16.0f,
                   bounds.getHeight() - headerHeightPx - scaledFloat(12.0f)));
    g.setGradientFill(juce::ColourGradient(heatColour.withAlpha(0.92f),
                                           heatStrip.getCentreX(),
                                           heatStrip.getBottom(),
                                           heatColour.withAlpha(0.18f),
                                           heatStrip.getCentreX(),
                                           heatStrip.getY(), false));
    g.fillRoundedRectangle(heatStrip, heatStrip.getWidth() * 0.5f);
  }

  juce::Rectangle<float> headerBounds = bounds.withHeight(headerHeightPx);
  juce::Colour headerColor = TeulPalette::NodeHeader;
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

    const int inputLaneWidth = inputLaneWidthPx();
    const int outputLaneWidth = outputLaneWidthPx();
    const int labelPaddingPx = scaledInt(8);
    const int inputLabelX = juce::jmax(labelInsetPx, inputLaneWidth + labelPaddingPx);
    const int outputLabelRight =
        juce::jmax(inputLabelX + labelWidthPx + scaledInt(8),
                   getWidth() - outputLaneWidth - labelPaddingPx);
    const int outputLabelX = juce::jmax(inputLabelX + scaledInt(8),
                                        outputLabelRight - labelWidthPx);

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
      const float railWidth = juce::jmax(4.0f, scaledFloat(isSelected ? 7.0f : 5.0f));
      const float railInset = scaledFloat(6.0f);
      const float railHeight = juce::jmax(10.0f, scaledFloat(12.0f));
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
      const float probeWidth = juce::jmax(34.0f, scaledFloat(40.0f));
      const float probeHeight = juce::jmax(10.0f, scaledFloat(12.0f));
      const float probeInset = scaledFloat(8.0f);
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
        g.setFont(juce::FontOptions(juce::jmax(6.0f, scaledFloat(9.0f))));
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
    g.setColour(TeulPalette::NodeBorderSelected);
    g.drawRoundedRectangle(bounds, cornerRadiusPx, borderSelectedThicknessPx);
  } else {
    g.setColour(TeulPalette::NodeBorder.interpolatedWith(heatColour,
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
