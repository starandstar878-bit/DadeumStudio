#pragma once

#include <JuceHeader.h>
#include <memory>

namespace Gyeol::Ui::Panels
{
    enum class PropertyEditorKind
    {
        text,
        number,
        boolean,
        unsupported
    };

    class PropertyEditorFactory
    {
    public:
        static PropertyEditorKind inferKind(const juce::var& value) noexcept;
        static std::unique_ptr<juce::Component> createEditor(const juce::Identifier& key, const juce::var& value);
    };
}
