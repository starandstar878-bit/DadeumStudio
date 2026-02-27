#pragma once

#include "Gyeol/Editor/Canvas/CanvasRenderer.h"
#include "Gyeol/Public/Types.h"
#include <JuceHeader.h>

namespace Gyeol::Ui::Canvas
{
    class WidgetComponent : public juce::Component
    {
    public:
        explicit WidgetComponent(CanvasRenderer& rendererIn);

        void setModel(const WidgetModel& widgetIn, bool selectedIn);
        WidgetId widgetId() const noexcept;

        void paint(juce::Graphics& g) override;

    private:
        CanvasRenderer& renderer;
        WidgetModel model;
        bool selected = false;
    };
}
