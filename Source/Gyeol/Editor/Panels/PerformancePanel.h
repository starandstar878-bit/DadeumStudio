#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <map>
#include <vector>

namespace Gyeol::Ui::Panels {
class PerformancePanel : public juce::Component, private juce::Timer {
public:
  struct Snapshot {
    uint64_t refreshCount = 0;
    uint64_t paintCount = 0;
    uint64_t selectionSyncCount = 0;
    uint64_t dragPreviewUpdateCount = 0;
    uint64_t refreshRequestedPartialRepaintCount = 0;
    uint64_t refreshRequestedFullRepaintCount = 0;
    uint64_t selectionSyncRequestedPartialRepaintCount = 0;
    double lastRefreshMs = 0.0;
    double maxRefreshMs = 0.0;
    double lastPaintMs = 0.0;
    double maxPaintMs = 0.0;
    double lastSelectionSyncMs = 0.0;
    double maxSelectionSyncMs = 0.0;
    float lastDirtyAreaPx = 0.0f;
    int widgetViewCount = 0;
    int selectionCount = 0;

    int documentWidgetCount = 0;
    int documentGroupCount = 0;
    int documentLayerCount = 0;
    int documentAssetCount = 0;
    float zoomLevel = 1.0f;
    juce::Point<float> viewOriginWorld;
    juce::Rectangle<float> visibleWorldBounds;

    double editorToolbarPaintMs = 0.0;
    double editorLayoutMs = 0.0;
    double editorTelemetryMs = 0.0;
    uint64_t editorResizeThrottleCount = 0;
    bool lowEndRenderingMode = false;
    juce::String responsiveBreakpoint;
    juce::String densityPreset;
    juce::String themeVariant;
    juce::String panelLayoutSummary;
    juce::String panelPaintSummary;

    uint64_t deferredRefreshRequestCount = 0;
    uint64_t deferredRefreshCoalescedCount = 0;
    uint64_t deferredRefreshFlushCount = 0;

    std::map<juce::String, juce::int64> memoryBuckets;
    std::vector<juce::String> overdrawHotspotLabels;
    std::vector<float> overdrawHotspotScores;
    bool heatmapModeActive = false;
  };

  class PerformanceMetricsStore {
  public:
    static constexpr int capacity = 60;

    void push(double paintMs, double actionMs, double layoutMs);
    int size() const noexcept;
    std::array<double, capacity> paintSamples() const noexcept;
    std::array<double, capacity> actionSamples() const noexcept;
    std::array<double, capacity> layoutSamples() const noexcept;
    double averagePaint(int recentCount) const noexcept;
    double averageAction(int recentCount) const noexcept;
    double averageLayout(int recentCount) const noexcept;

  private:
    std::array<double, capacity> paintBuffer{};
    std::array<double, capacity> actionBuffer{};
    std::array<double, capacity> layoutBuffer{};
    int writeIndex = 0;
    int sampleCount = 0;

    std::array<double, capacity>
    ordered(const std::array<double, capacity> &source) const noexcept;
    double averageRecent(const std::array<double, capacity> &source,
                         int recentCount) const noexcept;
  };

  PerformancePanel();

  void setSnapshot(const Snapshot &snapshotIn);
  void setHeatmapToggledCallback(std::function<void(bool)> callback);
  void setNotificationCallback(
      std::function<void(const juce::String &, const juce::String &)> callback);
  void beginRunModeSamplingWindow(int durationMs);

  void paint(juce::Graphics &g) override;
  void resized() override;
  void lookAndFeelChanged() override;

private:
  enum class DashboardTab {
    frameCycle,
    overdrawHeatmap,
    memory
  };

  class DashboardButton : public juce::Button {
  public:
    explicit DashboardButton(const juce::String &text);

    void paintButton(juce::Graphics &g, bool highlighted,
                     bool down) override;

  private:
    juce::String text;
  };

  void timerCallback() override;
  void updateDerivedRates();
  void updateTabButtonStates();
  void evaluateRunModeThresholdIfNeeded();

  void drawSparkline(juce::Graphics &g,
                     juce::Rectangle<int> area,
                     const std::array<double, PerformanceMetricsStore::capacity>
                         &values,
                     double warnThreshold,
                     double errorThreshold,
                     const juce::String &label) const;
  void drawMemoryPieChart(juce::Graphics &g, juce::Rectangle<int> area) const;
  void drawOverdrawReport(juce::Graphics &g, juce::Rectangle<int> area) const;

  Snapshot snapshot;
  Snapshot previousSnapshot;
  bool hasPreviousSnapshot = false;
  double lastSampleTimestampMs = 0.0;
  double refreshHz = 0.0;
  double paintHz = 0.0;
  double selectionSyncHz = 0.0;

  PerformanceMetricsStore metricsStore;
  DashboardTab activeTab = DashboardTab::frameCycle;
  bool diagnosticsRunning = false;
  double runModeSamplingEndsAtMs = 0.0;
  bool runModeSamplingResultNotified = false;

  std::function<void(bool)> onHeatmapToggled;
  std::function<void(const juce::String &, const juce::String &)> onNotification;

  juce::ToggleButton diagnosticsToggle{"Diagnostics"};
  DashboardButton frameTabButton{"Frame"};
  DashboardButton heatmapTabButton{"Heatmap"};
  DashboardButton memoryTabButton{"Memory"};
  juce::ToggleButton heatmapToggle{"Heatmap Mode"};
  DashboardButton suggestionButton{"Suggest Render Cache"};

  juce::Rectangle<int> contentBounds;
};
} // namespace Gyeol::Ui::Panels
