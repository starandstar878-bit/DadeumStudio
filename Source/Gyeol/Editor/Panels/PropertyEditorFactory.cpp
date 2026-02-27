#include "Gyeol/Editor/Panels/PropertyEditorFactory.h"

namespace Gyeol::Ui::Panels
{
    PropertyEditorKind PropertyEditorFactory::inferKind(const juce::var& value) noexcept
    {
        if (value.isString())
            return PropertyEditorKind::text;
        if (value.isInt() || value.isInt64() || value.isDouble())
            return PropertyEditorKind::number;
        if (value.isBool())
            return PropertyEditorKind::boolean;
        return PropertyEditorKind::unsupported;
    }

    std::unique_ptr<juce::Component> PropertyEditorFactory::createEditor(const juce::Identifier& key,
                                                                         const juce::var& value)
    {
        (void) key;

        switch (inferKind(value))
        {
            case PropertyEditorKind::text:
            {
                auto editor = std::make_unique<juce::TextEditor>();
                editor->setText(value.toString(), false);
                return editor;
            }
            case PropertyEditorKind::number:
            {
                auto editor = std::make_unique<juce::Label>();
                editor->setText(value.toString(), juce::dontSendNotification);
                return editor;
            }
            case PropertyEditorKind::boolean:
            {
                auto editor = std::make_unique<juce::ToggleButton>();
                editor->setToggleState(static_cast<bool>(value), juce::dontSendNotification);
                return editor;
            }
            case PropertyEditorKind::unsupported:
            default:
            {
                auto editor = std::make_unique<juce::Label>();
                editor->setText("Unsupported", juce::dontSendNotification);
                return editor;
            }
        }
    }
}
