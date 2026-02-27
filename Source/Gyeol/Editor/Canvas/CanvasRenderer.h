#pragma once

#include "Gyeol/Public/Types.h"
#include <JuceHeader.h>

namespace Gyeol::Ui::Canvas
{
    class CanvasRenderer
    {
    public:
        void paintCanvas(juce::Graphics& g, juce::Rectangle<int> bounds) const;
        void paintWidget(juce::Graphics& g,
                         const WidgetModel& widget,
                         juce::Rectangle<float> bounds,
                         bool selected) const;
    };
}
