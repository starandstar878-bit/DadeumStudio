#include "Gyeol/Editor/Panels/PerformancePanel.h"

#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        using Gyeol::GyeolPalette;

        juce::Colour palette(GyeolPalette id, float alpha = 1.0f)
        {
            return Gyeol::getGyeolColor(id).withAlpha(alpha);
        }

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

        juce::Colour hzColour(double hz)
        {
            if (hz >= 60.0)
                return palette(GyeolPalette::ValidSuccess);
            if (hz >= 30.0)
                return palette(GyeolPalette::ValidWarning);
            if (hz > 0.0)
                return palette(GyeolPalette::ValidError);
            return palette(GyeolPalette::TextSecondary);
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
        g.fillAll(palette(GyeolPalette::PanelBackground));
        g.setColour(palette(GyeolPalette::BorderDefault));
        g.drawRect(getLocalBounds(), 1);

        auto area = contentBounds;
        if (area.isEmpty())
            area = getLocalBounds().reduced(8);

        g.setColour(palette(GyeolPalette::TextPrimary));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("Performance", area.removeFromTop(18), juce::Justification::centredLeft, true);

        area.removeFromTop(5);

        auto cardsArea = area.removeFromTop(64);
        constexpr int gap = 6;
        const auto cardWidth = (cardsArea.getWidth() - gap * 2) / 3;

        const auto drawCard = [&](juce::Rectangle<int> bounds,
                                  const juce::String& label,
                                  double hz,
                                  juce::Colour valueColour)
        {
            if (bounds.isEmpty())
                return;

            auto card = bounds.toFloat().reduced(0.5f);
            g.setColour(palette(GyeolPalette::HeaderBackground, 0.88f));
            g.fillRoundedRectangle(card, 6.0f);
            g.setColour(palette(GyeolPalette::BorderDefault, 0.94f));
            g.drawRoundedRectangle(card, 6.0f, 1.0f);

            auto text = bounds.reduced(8, 6);
            g.setColour(palette(GyeolPalette::TextSecondary));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText(label, text.removeFromTop(14), juce::Justification::centredLeft, true);

            g.setColour(valueColour);
            g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
            g.drawText(juce::String(hz, 1) + " Hz", text, juce::Justification::centredLeft, true);
        };

        auto refreshCard = cardsArea.removeFromLeft(cardWidth);
        cardsArea.removeFromLeft(gap);
        auto paintCard = cardsArea.removeFromLeft(cardWidth);
        cardsArea.removeFromLeft(gap);
        auto selectionCard = cardsArea;

        drawCard(refreshCard, "Refresh", refreshHz, hzColour(refreshHz));
        drawCard(paintCard, "Paint", paintHz, hzColour(paintHz));
        drawCard(selectionCard, "Selection Sync", selectionSyncHz, hzColour(selectionSyncHz));

        area.removeFromTop(6);

        if (area.getHeight() >= 62)
        {
            auto chartArea = area.removeFromTop(58);
            g.setColour(palette(GyeolPalette::HeaderBackground, 0.80f));
            g.fillRoundedRectangle(chartArea.toFloat(), 5.0f);
            g.setColour(palette(GyeolPalette::BorderDefault, 0.90f));
            g.drawRoundedRectangle(chartArea.toFloat(), 5.0f, 1.0f);

            auto chartInner = chartArea.reduced(8, 6);
            g.setColour(palette(GyeolPalette::TextSecondary));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText("Last ms vs Max ms", chartInner.removeFromTop(12), juce::Justification::centredLeft, true);

            const auto msScale = std::max({ snapshot.maxRefreshMs,
                                            snapshot.maxPaintMs,
                                            snapshot.maxSelectionSyncMs,
                                            1.0 });

            const auto drawBar = [&](const juce::String& label,
                                     double lastMs,
                                     juce::Colour colour)
            {
                auto row = chartInner.removeFromTop(11);
                if (row.isEmpty())
                    return;

                auto nameArea = row.removeFromLeft(64);
                auto barArea = row.reduced(0, 2);

                g.setColour(palette(GyeolPalette::TextSecondary, 0.9f));
                g.setFont(juce::FontOptions(8.6f));
                g.drawText(label, nameArea, juce::Justification::centredLeft, true);

                g.setColour(palette(GyeolPalette::ControlBase, 0.92f));
                g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

                const auto ratio = juce::jlimit(0.0, 1.0, lastMs / msScale);
                auto fill = barArea;
                fill.setWidth(static_cast<int>(std::round(fill.getWidth() * ratio)));
                if (fill.getWidth() > 0)
                {
                    g.setColour(colour.withAlpha(0.88f));
                    g.fillRoundedRectangle(fill.toFloat(), 2.0f);
                }
            };

            drawBar("Refresh", snapshot.lastRefreshMs, palette(GyeolPalette::AccentPrimary));
            drawBar("Paint", snapshot.lastPaintMs, palette(GyeolPalette::ValidWarning));
            drawBar("Select", snapshot.lastSelectionSyncMs, palette(GyeolPalette::ValidSuccess));

            area.removeFromTop(6);
        }

        const auto drawSection = [&](juce::String title, const juce::StringArray& lines)
        {
            if (area.getHeight() < 16)
                return;

            auto sectionHeight = 18 + lines.size() * 14 + 6;
            if (sectionHeight > area.getHeight())
                sectionHeight = area.getHeight();

            auto section = area.removeFromTop(sectionHeight);
            g.setColour(palette(GyeolPalette::HeaderBackground, 0.78f));
            g.fillRoundedRectangle(section.toFloat(), 5.0f);
            g.setColour(palette(GyeolPalette::BorderDefault, 0.86f));
            g.drawRoundedRectangle(section.toFloat(), 5.0f, 1.0f);

            auto text = section.reduced(8, 5);
            g.setColour(palette(GyeolPalette::TextPrimary));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText(title, text.removeFromTop(12), juce::Justification::centredLeft, true);

            g.setColour(palette(GyeolPalette::TextSecondary, 0.94f));
            g.setFont(juce::FontOptions(9.2f));
            for (int i = 0; i < lines.size(); ++i)
            {
                auto row = text.removeFromTop(13);
                if (row.isEmpty())
                    break;
                g.drawText(lines[i], row, juce::Justification::centredLeft, true);
            }

            area.removeFromTop(4);
        };

        juce::StringArray timing;
        timing.add("Refresh last/max: " + formatMs(snapshot.lastRefreshMs) + " / " + formatMs(snapshot.maxRefreshMs));
        timing.add("Paint last/max: " + formatMs(snapshot.lastPaintMs) + " / " + formatMs(snapshot.maxPaintMs));
        timing.add("Selection last/max: " + formatMs(snapshot.lastSelectionSyncMs) + " / " + formatMs(snapshot.maxSelectionSyncMs));

        juce::StringArray counters;
        counters.add("Refresh/Paint/Selection: "
                     + juce::String(static_cast<int64_t>(snapshot.refreshCount)) + " / "
                     + juce::String(static_cast<int64_t>(snapshot.paintCount)) + " / "
                     + juce::String(static_cast<int64_t>(snapshot.selectionSyncCount)));
        counters.add("Full/Partial/SelectionPartial: "
                     + juce::String(static_cast<int64_t>(snapshot.refreshRequestedFullRepaintCount)) + " / "
                     + juce::String(static_cast<int64_t>(snapshot.refreshRequestedPartialRepaintCount)) + " / "
                     + juce::String(static_cast<int64_t>(snapshot.selectionSyncRequestedPartialRepaintCount)));
        counters.add("Drag preview updates: " + juce::String(static_cast<int64_t>(snapshot.dragPreviewUpdateCount)));

        juce::StringArray document;
        document.add("Widgets/Groups/Layers/Assets: "
                     + juce::String(snapshot.documentWidgetCount) + " / "
                     + juce::String(snapshot.documentGroupCount) + " / "
                     + juce::String(snapshot.documentLayerCount) + " / "
                     + juce::String(snapshot.documentAssetCount));
        document.add("Views/Selection: " + juce::String(snapshot.widgetViewCount) + " / " + juce::String(snapshot.selectionCount));
        document.add("Zoom " + juce::String(snapshot.zoomLevel, 3) + "  Origin " + formatPoint(snapshot.viewOriginWorld));
        document.add("Visible " + formatRect(snapshot.visibleWorldBounds));

        juce::StringArray debug;
        debug.add("Deferred req/coalesced/flush: "
                  + juce::String(static_cast<int64_t>(snapshot.deferredRefreshRequestCount)) + " / "
                  + juce::String(static_cast<int64_t>(snapshot.deferredRefreshCoalescedCount)) + " / "
                  + juce::String(static_cast<int64_t>(snapshot.deferredRefreshFlushCount)));
        debug.add("Last dirty px: " + juce::String(snapshot.lastDirtyAreaPx, 1));

        drawSection("Timing", timing);
        drawSection("Counters", counters);
        drawSection("Document", document);
        drawSection("Debug", debug);
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