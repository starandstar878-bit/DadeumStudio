#include "Gyeol/Editor/Canvas/SnapGuideOverlay.h"
#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

namespace Gyeol::Ui::Canvas
{
    namespace
    {
        juce::Colour palette(Gyeol::GyeolPalette id, float alpha = 1.0f)
        {
            return Gyeol::getGyeolColor(id).withAlpha(alpha);
        }
    }

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

        g.setColour(palette(Gyeol::GyeolPalette::AccentPrimary, 0.82f));
        for (const auto& line : guides)
            g.drawLine(line, 1.0f);
    }
}
