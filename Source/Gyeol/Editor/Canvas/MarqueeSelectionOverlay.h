#pragma once

#include <JuceHeader.h>

namespace Gyeol::Ui::Canvas
{
    class MarqueeSelectionOverlay : public juce::Component
    {
    public:
        void setMarquee(juce::Rectangle<float> marqueeBounds);
        void clearMarquee();
        bool isActive() const noexcept;

        void paint(juce::Graphics& g) override;

    private:
        bool active = false;
        juce::Rectangle<float> marquee;
    };
}
