#include "Gyeol/Editor/Canvas/MarqueeSelectionOverlay.h"

namespace Gyeol::Ui::Canvas
{
    void MarqueeSelectionOverlay::setMarquee(juce::Rectangle<float> marqueeBounds)
    {
        marquee = marqueeBounds;
        active = true;
        repaint();
    }

    void MarqueeSelectionOverlay::clearMarquee()
    {
        if (!active)
            return;

        active = false;
        marquee = {};
        repaint();
    }

    bool MarqueeSelectionOverlay::isActive() const noexcept
    {
        return active;
    }

    void MarqueeSelectionOverlay::paint(juce::Graphics& g)
    {
        if (!active)
            return;

        g.setColour(juce::Colour::fromRGBA(78, 156, 255, 30));
        g.fillRect(marquee.toNearestInt());
        g.setColour(juce::Colour::fromRGBA(78, 156, 255, 180));
        g.drawRect(marquee.toNearestInt(), 1);
    }
}
