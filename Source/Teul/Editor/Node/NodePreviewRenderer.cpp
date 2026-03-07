#include "Teul/Editor/Node/NodePreviewRenderer.h"

#include "Teul/UI/TeulPalette.h"

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

static InlinePreviewKind inlinePreviewKindForImpl(const TNodeDescriptor *descriptor) {
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

static int previewHeightForKindImpl(InlinePreviewKind kind) {
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

static int minWidthForKindImpl(InlinePreviewKind kind) {
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
makePreviewBoundsImpl(const juce::Rectangle<float> &bounds, InlinePreviewKind kind) {
  const auto height = (float)previewHeightForKindImpl(kind);
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

static void drawAdsrPreviewImpl(juce::Graphics &g, const juce::Rectangle<float> &bounds,
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

static void drawMeterPreviewImpl(juce::Graphics &g, const juce::Rectangle<float> &bounds,
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

InlinePreviewKind inlinePreviewKindFor(const TNodeDescriptor *descriptor) {
  return inlinePreviewKindForImpl(descriptor);
}

int previewHeightForKind(InlinePreviewKind kind) {
  return previewHeightForKindImpl(kind);
}

int minWidthForKind(InlinePreviewKind kind) {
  return minWidthForKindImpl(kind);
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
                      const TNodeDescriptor *descriptor, const TNode &node) {
  drawMeterPreviewImpl(g, bounds, descriptor, node);
}

} // namespace Teul
