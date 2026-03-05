#include "Gyeol/Editor/Canvas/MarqueeSelectionOverlay.h"
#include "Gyeol/Editor/Theme/GyeolCustomLookAndFeel.h"

namespace Gyeol::Ui::Canvas
{
    namespace
    {
        juce::Colour palette(Gyeol::GyeolPalette id, float alpha = 1.0f)
        {
            return Gyeol::getGyeolColor(id).withAlpha(alpha);
        }
    }

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

        g.setColour(palette(Gyeol::GyeolPalette::SelectionBackground, 0.28f));
        g.fillRect(marquee.toNearestInt());
        g.setColour(palette(Gyeol::GyeolPalette::AccentPrimary, 0.78f));
        g.drawRect(marquee.toNearestInt(), 1);
    }
}
