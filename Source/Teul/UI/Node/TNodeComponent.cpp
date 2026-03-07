#include "TNodeComponent.h"
#include "../../UI/TeulPalette.h"
#include "../Canvas/TGraphCanvas.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Teul {
namespace {

enum class InlinePreviewKind {
  none,
  oscillator,
  adsr,
  meter,
};

struct MeterBar {
  juce::String label;
  float level = 0.0f;
  juce::Colour colour;
};

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

static InlinePreviewKind inlinePreviewKindFor(const TNodeDescriptor *descriptor) {
  if (descriptor == nullptr)
    return InlinePreviewKind::none;

  if (descriptor->typeKey == "Teul.Source.Oscillator")
    return InlinePreviewKind::oscillator;
  if (descriptor->typeKey == "Teul.Mod.ADSR")
    return InlinePreviewKind::adsr;
  if (descriptor->category == "Mixer")
    return InlinePreviewKind::meter;

  return InlinePreviewKind::none;
}

static int previewHeightForKind(InlinePreviewKind kind) {
  switch (kind) {
  case InlinePreviewKind::oscillator:
  case InlinePreviewKind::adsr:
    return 54;
  case InlinePreviewKind::meter:
    return 44;
  case InlinePreviewKind::none:
  default:
    return 0;
  }
}

static int minWidthForKind(InlinePreviewKind kind) {
  switch (kind) {
  case InlinePreviewKind::oscillator:
  case InlinePreviewKind::adsr:
    return 160;
  case InlinePreviewKind::meter:
    return 168;
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

static juce::Rectangle<float>
makePreviewBounds(const juce::Rectangle<float> &bounds, InlinePreviewKind kind) {
  const auto height = (float)previewHeightForKind(kind);
  if (height <= 0.0f)
    return {};

  auto preview = bounds.withTrimmedTop(bounds.getHeight() - (height + 10.0f));
  preview = preview.reduced(10.0f, 0.0f);
  preview.removeFromBottom(6.0f);
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
  g.fillRoundedRectangle(bounds, 7.0f);

  g.setColour(juce::Colours::black.withAlpha(0.16f));
  g.drawRoundedRectangle(bounds.translated(0.0f, 1.0f), 7.0f, 1.0f);

  g.setColour(accent.withAlpha(0.32f));
  g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
}

static void drawOscillatorPreview(juce::Graphics &g,
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

  drawPreviewPanel(g, bounds, TeulPalette::PortAudio);

  auto area = bounds.reduced(8.0f, 7.0f);
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

  g.setColour(TeulPalette::PortAudio.withAlpha(0.18f));
  g.strokePath(waveform,
               juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));
  g.setColour(TeulPalette::PortAudio.withAlpha(0.92f));
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

static void drawAdsrPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                            const TNode &node) {
  if (bounds.isEmpty())
    return;

  const float attack = (float)juce::jmax(1.0, paramAsDouble(node, "attack", 10.0));
  const float decay = (float)juce::jmax(1.0, paramAsDouble(node, "decay", 100.0));
  const float sustain = juce::jlimit(0.0f, 1.0f,
                                     (float)paramAsDouble(node, "sustain", 0.5));
  const float release = (float)juce::jmax(1.0, paramAsDouble(node, "release", 300.0));

  drawPreviewPanel(g, bounds, TeulPalette::PortCV);

  auto area = bounds.reduced(8.0f, 8.0f);
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

  g.setColour(TeulPalette::PortCV.withAlpha(0.16f));
  g.strokePath(env,
               juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));
  g.setColour(TeulPalette::PortCV.withAlpha(0.95f));
  g.strokePath(env,
               juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded));

  g.setColour(TeulPalette::PortCV.withAlpha(0.78f));
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
                                            const TNode &node) {
  std::vector<MeterBar> bars;
  if (descriptor == nullptr)
    return bars;

  if (descriptor->typeKey == "Teul.Mixer.VCA") {
    const float gain = juce::jlimit(0.0f, 1.0f,
                                    (float)paramAsDouble(node, "gain", 1.0) / 2.0f);
    bars.push_back({"OUT", gain, TeulPalette::PortAudio});
    return bars;
  }

  if (descriptor->typeKey == "Teul.Mixer.Mixer2") {
    const float gain1 = clampUnit((float)paramAsDouble(node, "gain1", 1.0));
    const float gain2 = clampUnit((float)paramAsDouble(node, "gain2", 1.0));
    bars.push_back({"A", gain1, TeulPalette::PortAudio});
    bars.push_back({"B", gain2, TeulPalette::PortCV});
    bars.push_back({"SUM", clampUnit((gain1 + gain2) * 0.5f),
                    juce::Colour(0xfff59e0b)});
    return bars;
  }

  if (descriptor->typeKey == "Teul.Mixer.StereoPanner") {
    const float pan = juce::jlimit(-1.0f, 1.0f,
                                   (float)paramAsDouble(node, "pan", 0.0));
    const float left = clampUnit(1.0f - juce::jmax(0.0f, pan));
    const float right = clampUnit(1.0f - juce::jmax(0.0f, -pan));
    bars.push_back({"L", left, TeulPalette::PortAudio});
    bars.push_back({"R", right, TeulPalette::PortMIDI});
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
                  TeulPalette::PortAudio});
  return bars;
}

static void drawMeterPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                             const TNodeDescriptor *descriptor,
                             const TNode &node) {
  if (bounds.isEmpty())
    return;

  const auto bars = buildMeterBars(descriptor, node);
  if (bars.empty())
    return;

  drawPreviewPanel(g, bounds, juce::Colour(0xff38bdf8));

  auto area = bounds.reduced(8.0f, 7.0f);
  const float rowGap = 4.0f;
  const float rowHeight = (area.getHeight() - rowGap * (float)(bars.size() - 1)) /
                          (float)bars.size();

  g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
  for (size_t i = 0; i < bars.size(); ++i) {
    auto row = juce::Rectangle<float>(area.getX(),
                                      area.getY() + (float)i * (rowHeight + rowGap),
                                      area.getWidth(), rowHeight);
    auto label = row.removeFromLeft(22.0f);
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
  const auto previewKind = inlinePreviewKindFor(descriptor);
  const int width = juce::jmax(minNodeWidth, minWidthForKind(previewKind));

  if (collapsed) {
    setSize(width, headerHeight);
    return;
  }

  const size_t maxPorts = std::max(inPorts.size(), outPorts.size());
  const int previewHeight = previewHeightForKind(previewKind);
  const int totalHeight = headerHeight + (int)maxPorts * portRowHeight + 20 +
                          previewHeight;
  setSize(width, totalHeight);
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
  g.fillRoundedRectangle(bounds, cornerRadius);

  juce::Rectangle<float> headerBounds = bounds.withHeight((float)headerHeight);
  juce::Colour headerColor = TeulPalette::NodeHeader;
  if (nodePtr != nullptr && nodePtr->colorTag.isNotEmpty())
    headerColor = colorFromTag(nodePtr->colorTag).withAlpha(0.75f);
  g.setColour(headerColor);

  if (collapsed) {
    g.fillRoundedRectangle(headerBounds, cornerRadius);
  } else {
    g.fillRoundedRectangle(headerBounds, cornerRadius);
    g.fillRect(headerBounds.withTop(headerBounds.getBottom() - cornerRadius));
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
        drawMeterPreview(g, previewBounds, descriptor, *nodePtr);
        break;
      case InlinePreviewKind::none:
      default:
        break;
      }
    }
  }

  if (isSelected) {
    g.setColour(TeulPalette::NodeBorderSelected);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.5f);
  } else {
    g.setColour(TeulPalette::NodeBorder);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
  }

  if (hasError) {
    g.setColour(juce::Colour(0x60f87171));
    g.drawRoundedRectangle(bounds.expanded(2.0f), cornerRadius + 2.0f, 2.0f);
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
