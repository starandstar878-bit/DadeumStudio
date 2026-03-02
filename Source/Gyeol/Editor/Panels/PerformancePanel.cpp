#include "Gyeol/Editor/Panels/PerformancePanel.h"

namespace Gyeol::Ui::Panels
{
    namespace
    {
        juce::String formatMs(double value)
        {
            return juce::String(value, 3) + " ms";
        }

        juce::String formatPoint(const juce::Point<float>& point)
        {
            return "(" + juce::String(point.x, 1) + ", " + juce::String(point.y, 1) + ")";
        }

        juce::String formatRect(const juce::Rectangle<float>& rect)
        {
            return "(" + juce::String(rect.getX(), 1) + ", " + juce::String(rect.getY(), 1)
                + ") " + juce::String(rect.getWidth(), 1) + "x" + juce::String(rect.getHeight(), 1);
        }
    }

    void PerformancePanel::setSnapshot(const Snapshot& snapshotIn)
    {
        snapshot = snapshotIn;
        updateDerivedRates();
        repaint();
    }

    void PerformancePanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(40, 46, 56));
        g.drawRect(getLocalBounds(), 1);

        auto area = contentBounds;
        if (area.isEmpty())
            area = getLocalBounds().reduced(8);

        g.setColour(juce::Colour::fromRGB(188, 195, 208));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("Performance", area.removeFromTop(18), juce::Justification::centredLeft, true);

        area.removeFromTop(4);

        juce::StringArray lines;
        lines.add("Doc widgets/groups/layers/assets: "
                  + juce::String(snapshot.documentWidgetCount) + " / "
                  + juce::String(snapshot.documentGroupCount) + " / "
                  + juce::String(snapshot.documentLayerCount) + " / "
                  + juce::String(snapshot.documentAssetCount));
        lines.add("Canvas views/selection: "
                  + juce::String(snapshot.widgetViewCount) + " / "
                  + juce::String(snapshot.selectionCount));
        lines.add("Zoom: " + juce::String(snapshot.zoomLevel, 3)
                  + "  Origin: " + formatPoint(snapshot.viewOriginWorld));
        lines.add("Visible world: " + formatRect(snapshot.visibleWorldBounds));
        lines.add("Refresh count/rate: " + juce::String(static_cast<int64_t>(snapshot.refreshCount))
                  + " / " + juce::String(refreshHz, 1) + " Hz");
        lines.add("Paint count/rate: " + juce::String(static_cast<int64_t>(snapshot.paintCount))
                  + " / " + juce::String(paintHz, 1) + " Hz");
        lines.add("Selection sync count/rate: "
                  + juce::String(static_cast<int64_t>(snapshot.selectionSyncCount))
                  + " / " + juce::String(selectionSyncHz, 1) + " Hz");
        lines.add("Refresh last/max: " + formatMs(snapshot.lastRefreshMs)
                  + " / " + formatMs(snapshot.maxRefreshMs));
        lines.add("Paint last/max: " + formatMs(snapshot.lastPaintMs)
                  + " / " + formatMs(snapshot.maxPaintMs));
        lines.add("Selection sync last/max: " + formatMs(snapshot.lastSelectionSyncMs)
                  + " / " + formatMs(snapshot.maxSelectionSyncMs));
        lines.add("Repaint full/partial/selectionPartial: "
                  + juce::String(static_cast<int64_t>(snapshot.refreshRequestedFullRepaintCount)) + " / "
                  + juce::String(static_cast<int64_t>(snapshot.refreshRequestedPartialRepaintCount)) + " / "
                  + juce::String(static_cast<int64_t>(snapshot.selectionSyncRequestedPartialRepaintCount)));
        lines.add("Drag preview updates: "
                  + juce::String(static_cast<int64_t>(snapshot.dragPreviewUpdateCount))
                  + "  Last dirty px: " + juce::String(snapshot.lastDirtyAreaPx, 1));
        lines.add("Deferred refresh req/coalesced/flush: "
                  + juce::String(static_cast<int64_t>(snapshot.deferredRefreshRequestCount)) + " / "
                  + juce::String(static_cast<int64_t>(snapshot.deferredRefreshCoalescedCount)) + " / "
                  + juce::String(static_cast<int64_t>(snapshot.deferredRefreshFlushCount)));

        g.setColour(juce::Colour::fromRGB(174, 182, 196));
        g.setFont(juce::FontOptions(11.0f));
        for (int i = 0; i < lines.size(); ++i)
        {
            const auto row = area.removeFromTop(17);
            if (row.isEmpty())
                break;
            g.drawText(lines[i], row, juce::Justification::centredLeft, true);
        }
    }

    void PerformancePanel::resized()
    {
        contentBounds = getLocalBounds().reduced(8);
    }

    void PerformancePanel::updateDerivedRates()
    {
        const auto nowMs = juce::Time::getMillisecondCounterHiRes();
        if (!hasPreviousSnapshot)
        {
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

        refreshHz = (static_cast<double>(snapshot.refreshCount - previousSnapshot.refreshCount)) / deltaSec;
        paintHz = (static_cast<double>(snapshot.paintCount - previousSnapshot.paintCount)) / deltaSec;
        selectionSyncHz = (static_cast<double>(snapshot.selectionSyncCount - previousSnapshot.selectionSyncCount)) / deltaSec;

        previousSnapshot = snapshot;
        lastSampleTimestampMs = nowMs;
    }
}
