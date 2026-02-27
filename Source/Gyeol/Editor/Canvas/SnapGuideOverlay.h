#pragma once

#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Ui::Canvas
{
    class SnapGuideOverlay : public juce::Component
    {
    public:
        void setGuides(std::vector<juce::Line<float>> guidesIn);
        void clearGuides();

        void paint(juce::Graphics& g) override;

    private:
        std::vector<juce::Line<float>> guides;
    };
}
