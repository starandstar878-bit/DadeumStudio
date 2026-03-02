#pragma once

#include <JuceHeader.h>

namespace Gyeol::Ui::Panels
{
    class PerformancePanel : public juce::Component
    {
    public:
        struct Snapshot
        {
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

            uint64_t deferredRefreshRequestCount = 0;
            uint64_t deferredRefreshCoalescedCount = 0;
            uint64_t deferredRefreshFlushCount = 0;
        };

        PerformancePanel() = default;

        void setSnapshot(const Snapshot& snapshotIn);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void updateDerivedRates();

        Snapshot snapshot;
        Snapshot previousSnapshot;
        bool hasPreviousSnapshot = false;
        double lastSampleTimestampMs = 0.0;
        double refreshHz = 0.0;
        double paintHz = 0.0;
        double selectionSyncHz = 0.0;
        juce::Rectangle<int> contentBounds;
    };
}
