#pragma once

#include "Gyeol/Editor/Interaction/SnapEngine.h"
#include "Gyeol/Public/Types.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>

namespace Gyeol::Ui::Canvas
{
    class CanvasRenderer
    {
    public:
        explicit CanvasRenderer(const Widgets::WidgetFactory& widgetFactoryIn) noexcept;

        void paintCanvas(juce::Graphics& g,
                         juce::Rectangle<int> viewportBounds,
                         juce::Rectangle<float> visibleWorldBounds,
                         juce::Rectangle<float> canvasWorldBounds,
                         float zoomLevel,
                         const Interaction::SnapSettings& snapSettings) const;

        void paintRulers(juce::Graphics& g,
                        juce::Rectangle<int> viewportBounds,
                        juce::Rectangle<float> visibleWorldBounds,
                        float zoomLevel) const;

        void paintWidget(juce::Graphics& g,
                         const WidgetModel& widget,
                         juce::Rectangle<float> localBounds,
                         bool selected,
                         float effectiveOpacity = 1.0f,
                         bool showResizeHandle = false,
                         bool resizeHandleHot = false) const;

        juce::Rectangle<float> resizeHandleBounds(const juce::Rectangle<float>& localBounds) const noexcept;
        bool hitResizeHandle(const juce::Rectangle<float>& localBounds,
                             juce::Point<float> point) const noexcept;

    private:
        const Widgets::WidgetFactory& widgetFactory;
    };
}
