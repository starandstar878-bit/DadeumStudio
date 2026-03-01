#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include <memory>

namespace Gyeol
{
    class EditorHandle : public juce::Component
    {
    public:
        EditorHandle();
        ~EditorHandle() override;

        DocumentHandle& document() noexcept;
        const DocumentHandle& document() const noexcept;
        void refreshFromDocument();

        void paint(juce::Graphics& g) override;
        void resized() override;
        bool keyPressed(const juce::KeyPress& key) override;

    private:
        class Impl;
        std::unique_ptr<Impl> impl;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorHandle)
    };

    std::unique_ptr<EditorHandle> createEditor();
}
