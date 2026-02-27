#include "Gyeol/Editor/Canvas/CanvasRenderer.h"

namespace Gyeol::Ui::Canvas
{
    void CanvasRenderer::paintCanvas(juce::Graphics& g, juce::Rectangle<int> bounds) const
    {
        g.fillAll(juce::Colour::fromRGB(18, 20, 25));

        g.setColour(juce::Colour::fromRGBA(255, 255, 255, 12));
        constexpr int grid = 12;
        for (int x = bounds.getX(); x < bounds.getRight(); x += grid)
            g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));
        for (int y = bounds.getY(); y < bounds.getBottom(); y += grid)
            g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
    }

    void CanvasRenderer::paintWidget(juce::Graphics& g,
                                     const WidgetModel& widget,
                                     juce::Rectangle<float> bounds,
                                     bool selected) const
    {
        (void) widget;

        g.setColour(juce::Colour::fromRGB(44, 49, 60));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);
        g.setColour(selected ? juce::Colour::fromRGB(78, 156, 255) : juce::Colour::fromRGB(95, 101, 114));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, selected ? 2.0f : 1.0f);
    }
}
