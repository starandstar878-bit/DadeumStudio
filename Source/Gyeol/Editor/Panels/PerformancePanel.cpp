
#include "Gyeol/Editor/Panels/PerformancePanel.h"

#include "Gyeol/Editor/Theme/GyeolCustomLookAndFeel.h"

#include <algorithm>
#include <numeric>

namespace Gyeol::Ui::Panels {
namespace {
using Gyeol::GyeolPalette;

juce::Colour palette(GyeolPalette id, float alpha = 1.0f) {
  return Gyeol::getGyeolColor(id).withAlpha(alpha);
}

juce::Font makePanelFont(const juce::Component &component, float height,
                         bool bold) {
  if (auto *lf = dynamic_cast<const Gyeol::GyeolCustomLookAndFeel *>(
          &component.getLookAndFeel());
      lf != nullptr)
    return lf->makeFont(height, bold);

  auto fallback = juce::Font(juce::FontOptions(height));
  return bold ? fallback.boldened() : fallback;
}

juce::String formatMs(double value) {
  return juce::String(value, 3) + " ms";
}

juce::String formatPoint(const juce::Point<float> &point) {
  return "(" + juce::String(point.x, 1) + ", " + juce::String(point.y, 1) + ")";
}

juce::String formatRect(const juce::Rectangle<float> &rect) {
  return "(" + juce::String(rect.getX(), 1) + ", " + juce::String(rect.getY(), 1) +
         ") " + juce::String(rect.getWidth(), 1) + "x" +
         juce::String(rect.getHeight(), 1);
}

juce::Colour hzColour(double hz) {
  if (hz >= 60.0)
    return palette(GyeolPalette::ValidSuccess);
  if (hz >= 30.0)
    return palette(GyeolPalette::ValidWarning);
  if (hz > 0.0)
    return palette(GyeolPalette::ValidError);
  return palette(GyeolPalette::TextSecondary);
}
} // namespace

void PerformancePanel::PerformanceMetricsStore::push(double paintMs,
                                                     double actionMs,
                                                     double layoutMs) {
  paintBuffer[static_cast<size_t>(writeIndex)] = juce::jmax(0.0, paintMs);
  actionBuffer[static_cast<size_t>(writeIndex)] = juce::jmax(0.0, actionMs);
  layoutBuffer[static_cast<size_t>(writeIndex)] = juce::jmax(0.0, layoutMs);

  writeIndex = (writeIndex + 1) % capacity;
  sampleCount = juce::jmin(capacity, sampleCount + 1);
}

int PerformancePanel::PerformanceMetricsStore::size() const noexcept {
  return sampleCount;
}

std::array<double, PerformancePanel::PerformanceMetricsStore::capacity>
PerformancePanel::PerformanceMetricsStore::paintSamples() const noexcept {
  return ordered(paintBuffer);
}

std::array<double, PerformancePanel::PerformanceMetricsStore::capacity>
PerformancePanel::PerformanceMetricsStore::actionSamples() const noexcept {
  return ordered(actionBuffer);
}

std::array<double, PerformancePanel::PerformanceMetricsStore::capacity>
PerformancePanel::PerformanceMetricsStore::layoutSamples() const noexcept {
  return ordered(layoutBuffer);
}

double PerformancePanel::PerformanceMetricsStore::averagePaint(
    int recentCount) const noexcept {
  return averageRecent(paintBuffer, recentCount);
}

double PerformancePanel::PerformanceMetricsStore::averageAction(
    int recentCount) const noexcept {
  return averageRecent(actionBuffer, recentCount);
}

double PerformancePanel::PerformanceMetricsStore::averageLayout(
    int recentCount) const noexcept {
  return averageRecent(layoutBuffer, recentCount);
}

std::array<double, PerformancePanel::PerformanceMetricsStore::capacity>
PerformancePanel::PerformanceMetricsStore::ordered(
    const std::array<double, capacity> &source) const noexcept {
  std::array<double, capacity> out{};
  if (sampleCount <= 0)
    return out;

  const auto beginIndex = (writeIndex - sampleCount + capacity) % capacity;
  for (int i = 0; i < sampleCount; ++i)
    out[static_cast<size_t>(i)] = source[static_cast<size_t>((beginIndex + i) % capacity)];

  return out;
}

double PerformancePanel::PerformanceMetricsStore::averageRecent(
    const std::array<double, capacity> &source,
    int recentCount) const noexcept {
  if (sampleCount <= 0)
    return 0.0;

  const auto count = juce::jlimit(1, sampleCount, recentCount);
  double sum = 0.0;
  for (int i = 0; i < count; ++i) {
    const auto idx = (writeIndex - 1 - i + capacity) % capacity;
    sum += source[static_cast<size_t>(idx)];
  }

  return sum / static_cast<double>(count);
}

PerformancePanel::DashboardButton::DashboardButton(const juce::String &textIn)
    : juce::Button(textIn), text(textIn) {
  setClickingTogglesState(true);
}

void PerformancePanel::DashboardButton::paintButton(juce::Graphics &g,
                                                    bool highlighted,
                                                    bool down) {
  if (auto *lf = dynamic_cast<const Gyeol::GyeolCustomLookAndFeel *>(
          &getLookAndFeel());
      lf != nullptr) {
    lf->drawPerformanceDashboardButton(g,
                                       getLocalBounds().toFloat(),
                                       text,
                                       getToggleState(),
                                       highlighted,
                                       down,
                                       isEnabled());
    return;
  }

  g.setColour(isEnabled() ? juce::Colours::darkgrey : juce::Colours::grey);
  g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f);
  g.setColour(juce::Colours::white);
  g.drawText(text, getLocalBounds(), juce::Justification::centred, true);
}

PerformancePanel::PerformancePanel() {
  diagnosticsToggle.setClickingTogglesState(true);
  diagnosticsToggle.setToggleState(false, juce::dontSendNotification);
  diagnosticsToggle.onClick = [this] {
    diagnosticsRunning = diagnosticsToggle.getToggleState();
    if (diagnosticsRunning)
      startTimer(120);
    else if (runModeSamplingEndsAtMs <= juce::Time::getMillisecondCounterHiRes())
      stopTimer();
    repaint();
  };
  addAndMakeVisible(diagnosticsToggle);

  frameTabButton.onClick = [this] {
    activeTab = DashboardTab::frameCycle;
    updateTabButtonStates();
    repaint();
  };
  heatmapTabButton.onClick = [this] {
    activeTab = DashboardTab::overdrawHeatmap;
    updateTabButtonStates();
    repaint();
  };
  memoryTabButton.onClick = [this] {
    activeTab = DashboardTab::memory;
    updateTabButtonStates();
    repaint();
  };

  addAndMakeVisible(frameTabButton);
  addAndMakeVisible(heatmapTabButton);
  addAndMakeVisible(memoryTabButton);

  heatmapToggle.setClickingTogglesState(true);
  heatmapToggle.onClick = [this] {
    snapshot.heatmapModeActive = heatmapToggle.getToggleState();
    if (onHeatmapToggled)
      onHeatmapToggled(snapshot.heatmapModeActive);
    repaint();
  };
  addAndMakeVisible(heatmapToggle);

  suggestionButton.onClick = [this] {
    if (onNotification) {
      onNotification("Render Cache Suggestion",
                     "Top hotspots are overlap-heavy. Consider enabling widget-level render cache.");
    }
  };
  addAndMakeVisible(suggestionButton);

  updateTabButtonStates();
}

void PerformancePanel::setSnapshot(const Snapshot &snapshotIn) {
  snapshot = snapshotIn;
  heatmapToggle.setToggleState(snapshot.heatmapModeActive,
                               juce::dontSendNotification);

  if (diagnosticsRunning || runModeSamplingEndsAtMs > juce::Time::getMillisecondCounterHiRes()) {
    metricsStore.push(snapshot.lastPaintMs,
                      snapshot.lastSelectionSyncMs,
                      snapshot.editorLayoutMs);
  }

  updateDerivedRates();
  repaint();
}

void PerformancePanel::setHeatmapToggledCallback(
    std::function<void(bool)> callback) {
  onHeatmapToggled = std::move(callback);
}

void PerformancePanel::setNotificationCallback(
    std::function<void(const juce::String &, const juce::String &)> callback) {
  onNotification = std::move(callback);
}

void PerformancePanel::beginRunModeSamplingWindow(int durationMs) {
  diagnosticsRunning = true;
  diagnosticsToggle.setToggleState(true, juce::dontSendNotification);
  runModeSamplingEndsAtMs = juce::Time::getMillisecondCounterHiRes() +
                            juce::jmax(250, durationMs);
  runModeSamplingResultNotified = false;
  startTimer(120);
  repaint();
}

void PerformancePanel::paint(juce::Graphics &g) {
  g.fillAll(palette(GyeolPalette::PanelBackground));
  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawRect(getLocalBounds(), 1);

  auto area = contentBounds;
  if (area.isEmpty())
    area = getLocalBounds().reduced(8);

  g.setColour(palette(GyeolPalette::TextPrimary));
  g.setFont(makePanelFont(*this, 12.0f, true));
  g.drawText("Performance", area.removeFromTop(18), juce::Justification::centredLeft,
             true);

  area.removeFromTop(4);

  auto cardsArea = area.removeFromTop(60);
  constexpr int gap = 6;
  const auto cardWidth = (cardsArea.getWidth() - gap * 2) / 3;

  const auto drawCard = [&](juce::Rectangle<int> bounds,
                            const juce::String &label,
                            double hz,
                            juce::Colour valueColour) {
    if (bounds.isEmpty())
      return;

    auto card = bounds.toFloat().reduced(0.5f);
    g.setColour(palette(GyeolPalette::HeaderBackground, 0.88f));
    g.fillRoundedRectangle(card, 6.0f);
    g.setColour(palette(GyeolPalette::BorderDefault, 0.94f));
    g.drawRoundedRectangle(card, 6.0f, 1.0f);

    auto text = bounds.reduced(8, 5);
    g.setColour(palette(GyeolPalette::TextSecondary));
    g.setFont(makePanelFont(*this, 9.5f, true));
    g.drawText(label, text.removeFromTop(14), juce::Justification::centredLeft,
               true);

    g.setColour(valueColour);
    g.setFont(makePanelFont(*this, 16.0f, true));
    g.drawText(juce::String(hz, 1) + " Hz", text,
               juce::Justification::centredLeft, true);
  };

  auto refreshCard = cardsArea.removeFromLeft(cardWidth);
  cardsArea.removeFromLeft(gap);
  auto paintCard = cardsArea.removeFromLeft(cardWidth);
  cardsArea.removeFromLeft(gap);
  auto selectionCard = cardsArea;

  drawCard(refreshCard, "Refresh", refreshHz, hzColour(refreshHz));
  drawCard(paintCard, "Paint", paintHz, hzColour(paintHz));
  drawCard(selectionCard, "Selection Sync", selectionSyncHz,
           hzColour(selectionSyncHz));

  area.removeFromTop(6);

  switch (activeTab) {
  case DashboardTab::frameCycle:
    drawSparkline(g, area.removeFromTop(90), metricsStore.paintSamples(), 8.0,
                  16.0, "Paint ms");
    drawSparkline(g, area.removeFromTop(90), metricsStore.actionSamples(), 4.0,
                  10.0, "Action ms");
    drawSparkline(g, area.removeFromTop(90), metricsStore.layoutSamples(), 6.0,
                  12.0, "Layout ms");
    break;
  case DashboardTab::overdrawHeatmap:
    drawOverdrawReport(g, area.removeFromTop(230));
    break;
  case DashboardTab::memory:
    drawMemoryPieChart(g, area.removeFromTop(220));
    break;
  }

  area.removeFromTop(5);

  juce::StringArray details;
  details.add("Refresh/Paint/Selection: " +
              juce::String(static_cast<int64_t>(snapshot.refreshCount)) + " / " +
              juce::String(static_cast<int64_t>(snapshot.paintCount)) + " / " +
              juce::String(static_cast<int64_t>(snapshot.selectionSyncCount)));
  details.add("Widgets/Groups/Layers/Assets: " +
              juce::String(snapshot.documentWidgetCount) + " / " +
              juce::String(snapshot.documentGroupCount) + " / " +
              juce::String(snapshot.documentLayerCount) + " / " +
              juce::String(snapshot.documentAssetCount));
  details.add("Zoom " + juce::String(snapshot.zoomLevel, 3) + "  Origin " +
              formatPoint(snapshot.viewOriginWorld));
  details.add("Visible " + formatRect(snapshot.visibleWorldBounds));
  details.add("Deferred req/coalesced/flush: " +
              juce::String(static_cast<int64_t>(snapshot.deferredRefreshRequestCount)) +
              " / " +
              juce::String(static_cast<int64_t>(snapshot.deferredRefreshCoalescedCount)) +
              " / " +
              juce::String(static_cast<int64_t>(snapshot.deferredRefreshFlushCount)));

  auto infoArea = area.removeFromTop(88);
  g.setColour(palette(GyeolPalette::HeaderBackground, 0.80f));
  g.fillRoundedRectangle(infoArea.toFloat(), 6.0f);
  g.setColour(palette(GyeolPalette::BorderDefault, 0.90f));
  g.drawRoundedRectangle(infoArea.toFloat(), 6.0f, 1.0f);

  auto textArea = infoArea.reduced(8, 6);
  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(makePanelFont(*this, 9.0f, false));
  for (int i = 0; i < details.size(); ++i)
    g.drawText(details[i], textArea.removeFromTop(14), juce::Justification::centredLeft, true);

  const auto samplingRemainingMs =
      juce::jmax(0.0, runModeSamplingEndsAtMs - juce::Time::getMillisecondCounterHiRes());
  if (samplingRemainingMs > 0.0) {
    const auto bar = juce::Rectangle<float>(
        static_cast<float>(getWidth() - 110),
        static_cast<float>(getHeight() - 18),
        96.0f,
        8.0f);
    g.setColour(palette(GyeolPalette::ControlBase, 0.9f));
    g.fillRoundedRectangle(bar, 3.0f);

    const auto ratio = static_cast<float>(samplingRemainingMs / 3000.0);
    auto fill = bar;
    fill.setWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, ratio));
    g.setColour(palette(GyeolPalette::AccentPrimary, 0.88f));
    g.fillRoundedRectangle(fill, 3.0f);
  }
}

void PerformancePanel::resized() {
  contentBounds = getLocalBounds().reduced(8);

  auto top = contentBounds;
  top.removeFromTop(20);

  diagnosticsToggle.setBounds(top.removeFromLeft(100));
  frameTabButton.setBounds(top.removeFromLeft(68));
  top.removeFromLeft(4);
  heatmapTabButton.setBounds(top.removeFromLeft(76));
  top.removeFromLeft(4);
  memoryTabButton.setBounds(top.removeFromLeft(68));

  auto heatmapControls = getLocalBounds();
  heatmapControls.removeFromTop(78);
  heatmapControls = heatmapControls.removeFromTop(24).reduced(10, 0);
  heatmapToggle.setBounds(heatmapControls.removeFromLeft(130));
  suggestionButton.setBounds(heatmapControls.removeFromLeft(170));
}

void PerformancePanel::lookAndFeelChanged() { repaint(); }

void PerformancePanel::timerCallback() {
  evaluateRunModeThresholdIfNeeded();

  const auto now = juce::Time::getMillisecondCounterHiRes();
  const auto shouldKeepRunning = diagnosticsRunning || now < runModeSamplingEndsAtMs;
  if (!shouldKeepRunning)
    stopTimer();

  repaint();
}

void PerformancePanel::updateDerivedRates() {
  const auto nowMs = juce::Time::getMillisecondCounterHiRes();
  if (!hasPreviousSnapshot) {
    hasPreviousSnapshot = true;
    previousSnapshot = snapshot;
    lastSampleTimestampMs = nowMs;
    refreshHz = 0.0;
    paintHz = 0.0;
    selectionSyncHz = 0.0;
    return;
  }

  const auto deltaMs = nowMs - lastSampleTimestampMs;
  if (deltaMs < 20.0)
    return;

  const auto deltaSec = deltaMs / 1000.0;
  if (deltaSec <= 0.0)
    return;

  refreshHz = static_cast<double>(snapshot.refreshCount - previousSnapshot.refreshCount) /
              deltaSec;
  paintHz = static_cast<double>(snapshot.paintCount - previousSnapshot.paintCount) /
            deltaSec;
  selectionSyncHz =
      static_cast<double>(snapshot.selectionSyncCount - previousSnapshot.selectionSyncCount) /
      deltaSec;

  previousSnapshot = snapshot;
  lastSampleTimestampMs = nowMs;
}

void PerformancePanel::updateTabButtonStates() {
  frameTabButton.setToggleState(activeTab == DashboardTab::frameCycle,
                                juce::dontSendNotification);
  heatmapTabButton.setToggleState(activeTab == DashboardTab::overdrawHeatmap,
                                  juce::dontSendNotification);
  memoryTabButton.setToggleState(activeTab == DashboardTab::memory,
                                 juce::dontSendNotification);
}

void PerformancePanel::evaluateRunModeThresholdIfNeeded() {
  if (runModeSamplingResultNotified)
    return;

  const auto now = juce::Time::getMillisecondCounterHiRes();
  if (now < runModeSamplingEndsAtMs)
    return;

  runModeSamplingResultNotified = true;

  const auto avgPaint = metricsStore.averagePaint(30);
  const auto avgAction = metricsStore.averageAction(30);
  const auto avgLayout = metricsStore.averageLayout(30);

  if (avgPaint <= 16.0 && avgAction <= 10.0 && avgLayout <= 12.0)
    return;

  if (onNotification) {
    onNotification("Performance Warning",
                   "Run-mode sampling exceeded threshold. Paint=" +
                       formatMs(avgPaint) + " Action=" + formatMs(avgAction) +
                       " Layout=" + formatMs(avgLayout));
  }
}

void PerformancePanel::drawSparkline(
    juce::Graphics &g,
    juce::Rectangle<int> area,
    const std::array<double, PerformanceMetricsStore::capacity> &values,
    double warnThreshold,
    double errorThreshold,
    const juce::String &label) const {
  if (area.isEmpty())
    return;

  g.setColour(palette(GyeolPalette::HeaderBackground, 0.82f));
  g.fillRoundedRectangle(area.toFloat(), 6.0f);
  g.setColour(palette(GyeolPalette::BorderDefault, 0.88f));
  g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);

  auto inner = area.reduced(8, 6);
  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(makePanelFont(*this, 9.3f, true));
  g.drawText(label, inner.removeFromTop(12), juce::Justification::centredLeft,
             true);

  const auto chart = inner.reduced(0, 2);
  if (chart.isEmpty())
    return;

  const auto activeSamples = metricsStore.size();
  if (activeSamples <= 1)
    return;

  const auto maxValue = std::max(errorThreshold * 1.2,
      *std::max_element(values.begin(), values.begin() + activeSamples));
  if (maxValue <= 0.0001)
    return;

  auto toPoint = [&](int index) {
    const auto x = chart.getX() +
                   (chart.getWidth() - 1) * (static_cast<float>(index) / static_cast<float>(activeSamples - 1));
    const auto normalized = static_cast<float>(juce::jlimit(0.0, 1.0, values[static_cast<size_t>(index)] / maxValue));
    const auto y = chart.getBottom() - normalized * chart.getHeight();
    return juce::Point<float>(x, y);
  };

  for (int i = 1; i < activeSamples; ++i) {
    const auto prev = toPoint(i - 1);
    const auto cur = toPoint(i);
    const auto sample = values[static_cast<size_t>(i)];
    juce::Colour colour = palette(GyeolPalette::ValidSuccess);
    if (sample >= errorThreshold)
      colour = palette(GyeolPalette::ValidError);
    else if (sample >= warnThreshold)
      colour = palette(GyeolPalette::ValidSuccess)
                   .interpolatedWith(palette(GyeolPalette::ValidWarning), 0.75f);

    g.setColour(colour.withAlpha(0.92f));
    g.drawLine(prev.x, prev.y, cur.x, cur.y, 1.7f);
  }
}

void PerformancePanel::drawMemoryPieChart(juce::Graphics &g,
                                          juce::Rectangle<int> area) const {
  if (area.isEmpty())
    return;

  g.setColour(palette(GyeolPalette::HeaderBackground, 0.82f));
  g.fillRoundedRectangle(area.toFloat(), 6.0f);
  g.setColour(palette(GyeolPalette::BorderDefault, 0.88f));
  g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);

  auto inner = area.reduced(8, 6);
  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(makePanelFont(*this, 9.5f, true));
  g.drawText("Memory buckets", inner.removeFromTop(12),
             juce::Justification::centredLeft, true);

  const auto pieArea = inner.removeFromLeft(120).toFloat().reduced(6.0f);

  juce::int64 total = 0;
  for (const auto &[_, value] : snapshot.memoryBuckets)
    total += juce::jmax<juce::int64>(0, value);

  if (total <= 0) {
    g.setColour(palette(GyeolPalette::TextDisabled));
    g.drawText("No memory stats", inner, juce::Justification::centredLeft, true);
    return;
  }

  std::array<juce::Colour, 6> colors{
      palette(GyeolPalette::AccentPrimary),
      palette(GyeolPalette::ValidWarning),
      palette(GyeolPalette::ValidSuccess),
      palette(GyeolPalette::ValidError),
      palette(GyeolPalette::BorderActive),
      palette(GyeolPalette::TextSecondary)};

  float start = -juce::MathConstants<float>::halfPi;
  int colorIndex = 0;
  for (const auto &[label, value] : snapshot.memoryBuckets) {
    const auto fraction = static_cast<float>(juce::jmax<juce::int64>(0, value)) /
                          static_cast<float>(total);
    const auto end = start + fraction * juce::MathConstants<float>::twoPi;

    juce::Path wedge;
    wedge.addPieSegment(pieArea, start, end, 0.45f);

    g.setColour(colors[static_cast<size_t>(colorIndex % colors.size())].withAlpha(0.84f));
    g.fillPath(wedge);

    start = end;
    ++colorIndex;
  }

  auto legend = inner;
  colorIndex = 0;
  for (const auto &[label, value] : snapshot.memoryBuckets) {
    auto row = legend.removeFromTop(13);
    if (row.isEmpty())
      break;

    g.setColour(colors[static_cast<size_t>(colorIndex % colors.size())].withAlpha(0.90f));
    g.fillRoundedRectangle(row.removeFromLeft(10).toFloat().reduced(1.0f), 2.0f);

    g.setColour(palette(GyeolPalette::TextSecondary));
    g.setFont(makePanelFont(*this, 9.0f, false));
    g.drawText(label + " " + juce::String(static_cast<int64_t>(value / 1024)) + " KB",
               row.reduced(4, 0), juce::Justification::centredLeft, true);
    ++colorIndex;
  }
}

void PerformancePanel::drawOverdrawReport(juce::Graphics &g,
                                          juce::Rectangle<int> area) const {
  if (area.isEmpty())
    return;

  g.setColour(palette(GyeolPalette::HeaderBackground, 0.82f));
  g.fillRoundedRectangle(area.toFloat(), 6.0f);
  g.setColour(palette(GyeolPalette::BorderDefault, 0.88f));
  g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);

  auto inner = area.reduced(8, 6);
  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(makePanelFont(*this, 9.5f, true));
  g.drawText("Overdraw hotspots", inner.removeFromTop(12),
             juce::Justification::centredLeft, true);

  auto toggleRow = inner.removeFromTop(20);
  g.setColour(snapshot.heatmapModeActive ? palette(GyeolPalette::ValidWarning)
                                         : palette(GyeolPalette::TextDisabled));
  g.setFont(makePanelFont(*this, 9.0f, true));
  g.drawText("Heatmap: " + juce::String(snapshot.heatmapModeActive ? "On" : "Off"),
             toggleRow, juce::Justification::centredLeft, true);

  const auto count = juce::jmin(static_cast<int>(snapshot.overdrawHotspotLabels.size()),
                                static_cast<int>(snapshot.overdrawHotspotScores.size()));

  for (int i = 0; i < count && i < 5; ++i) {
    auto row = inner.removeFromTop(24);
    g.setColour(palette(GyeolPalette::ControlBase, 0.92f));
    g.fillRoundedRectangle(row.toFloat(), 4.0f);

    g.setColour(palette(GyeolPalette::TextSecondary));
    g.setFont(makePanelFont(*this, 9.0f, true));
    g.drawText(juce::String(i + 1) + ". " + snapshot.overdrawHotspotLabels[static_cast<size_t>(i)],
               row.removeFromLeft(180).reduced(6, 0),
               juce::Justification::centredLeft,
               true);

    const auto score = snapshot.overdrawHotspotScores[static_cast<size_t>(i)];
    const auto ratio = juce::jlimit(0.0f, 1.0f, score / 4.0f);
    auto bar = row.reduced(6, 6);
    g.setColour(palette(GyeolPalette::ControlDown));
    g.fillRoundedRectangle(bar.toFloat(), 3.0f);

    auto fill = bar;
    fill.setWidth(static_cast<int>(std::round(bar.getWidth() * ratio)));
    g.setColour(palette(GyeolPalette::ValidError, 0.84f));
    g.fillRoundedRectangle(fill.toFloat(), 3.0f);
  }

  if (count == 0) {
    g.setColour(palette(GyeolPalette::TextDisabled));
    g.setFont(makePanelFont(*this, 9.0f, false));
    g.drawText("No hotspot data yet", inner, juce::Justification::centred,
               true);
  }
}
} // namespace Gyeol::Ui::Panels
