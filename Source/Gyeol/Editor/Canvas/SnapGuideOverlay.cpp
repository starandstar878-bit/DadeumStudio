#include "Gyeol/Editor/Canvas/SnapGuideOverlay.h"

namespace Gyeol::Ui::Canvas
{
    void SnapGuideOverlay::setGuides(std::vector<juce::Line<float>> guidesIn)
    {
        guides = std::move(guidesIn);
        repaint();
    }

    void SnapGuideOverlay::clearGuides()
    {
        if (guides.empty())
            return;

        guides.clear();
        repaint();
    }

    void SnapGuideOverlay::paint(juce::Graphics& g)
    {
        if (guides.empty())
            return;

        g.setColour(juce::Colour::fromRGBA(255, 208, 95, 210));
        for (const auto& line : guides)
            g.drawLine(line, 1.0f);
    }
}
