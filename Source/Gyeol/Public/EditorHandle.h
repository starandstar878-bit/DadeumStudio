#pragma once

#include "Gyeol/Public/DocumentHandle.h"

namespace Gyeol
{
    class EditorHandle : public juce::Component
    {
    public:
        EditorHandle() = default;
        ~EditorHandle() override = default;

        DocumentHandle& document() noexcept { return docHandle; }
        const DocumentHandle& document() const noexcept { return docHandle; }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(24, 24, 26));
            g.setColour(juce::Colour::fromRGB(220, 220, 220));
            g.setFont(juce::FontOptions(14.0f));
            g.drawFittedText("Gyeol Editor Placeholder",
                             getLocalBounds().reduced(12),
                             juce::Justification::topLeft,
                             1);
        }

    private:
        DocumentHandle docHandle;
    };

    inline std::unique_ptr<EditorHandle> createEditor()
    {
        return std::make_unique<EditorHandle>();
    }
}
