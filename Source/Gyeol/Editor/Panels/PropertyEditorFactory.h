#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace Gyeol::Ui::Panels
{
    struct EditorBuildSpec
    {
        Widgets::WidgetPropertySpec spec;
        juce::var value;
        bool mixed = false;
        bool readOnly = false;
        std::function<void(const juce::var&)> onPreview;
        std::function<void(const juce::var&)> onCommit;
        std::function<void()> onCancel;
    };

    class PropertyEditorFactory
    {
    public:
        static std::unique_ptr<juce::Component> createEditor(const EditorBuildSpec& spec);

        static juce::String formatValue(const Widgets::WidgetPropertySpec& spec, const juce::var& value);
        static bool parseValue(const Widgets::WidgetPropertySpec& spec, const juce::String& text, juce::var& valueOut);
        static bool normalizeValue(const Widgets::WidgetPropertySpec& spec, const juce::var& input, juce::var& valueOut);
    };
}
