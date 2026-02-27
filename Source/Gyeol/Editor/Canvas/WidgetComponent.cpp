#include "Gyeol/Editor/Canvas/WidgetComponent.h"

namespace Gyeol::Ui::Canvas
{
    WidgetComponent::WidgetComponent(CanvasRenderer& rendererIn)
        : renderer(rendererIn)
    {
    }

    void WidgetComponent::setModel(const WidgetModel& widgetIn, bool selectedIn)
    {
        model = widgetIn;
        selected = selectedIn;
        setBounds(model.bounds.getSmallestIntegerContainer());
        repaint();
    }

    WidgetId WidgetComponent::widgetId() const noexcept
    {
        return model.id;
    }

    void WidgetComponent::paint(juce::Graphics& g)
    {
        renderer.paintWidget(g, model, getLocalBounds().toFloat(), selected);
    }
}
