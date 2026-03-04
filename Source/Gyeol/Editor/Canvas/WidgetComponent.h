#pragma once

#include "Gyeol/Editor/Canvas/CanvasRenderer.h"
#include "Gyeol/Public/Types.h"
#include <JuceHeader.h>

namespace Gyeol::Ui::Canvas
{
    class CanvasComponent;

    class WidgetComponent : public juce::Component
    {
    public:
        WidgetComponent(CanvasComponent& ownerIn, CanvasRenderer& rendererIn);

        void setModel(const WidgetModel& widgetIn,
                      juce::Rectangle<float> viewBoundsIn,
                      bool selectedIn,
                      float effectiveOpacityIn,
                      bool showResizeHandleIn);

        bool setSelectionVisual(bool selectedIn, bool showResizeHandleIn);
        void setViewBounds(const juce::Rectangle<float>& bounds);

        WidgetId widgetId() const noexcept;
        bool isResizeHandleHit(juce::Point<float> localPoint) const noexcept;

        void paint(juce::Graphics& g) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDoubleClick(const juce::MouseEvent& event) override;
        void mouseWheelMove(const juce::MouseEvent& event,
                            const juce::MouseWheelDetails& wheel) override;

    private:
        CanvasComponent& owner;
        CanvasRenderer& renderer;
        WidgetModel model;
        bool selected = false;
        bool showResizeHandle = false;
        bool resizeHandleHot = false;
        float effectiveOpacity = 1.0f;
    };
}
