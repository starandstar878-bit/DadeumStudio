#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"

namespace Gyeol::Widgets
{
    class TextInputWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::textInput;
            descriptor.typeKey = "textInput";
            descriptor.displayName = "Text Input";
            descriptor.category = "Input";
            descriptor.tags = { "text", "input", "editor" };
            descriptor.iconKey = "textInput";
            descriptor.exportTargetType = "juce::TextEditor";
            descriptor.defaultBounds = { 0.0f, 0.0f, 180.0f, 32.0f };
            descriptor.minSize = { 90.0f, 24.0f };
                        descriptor.runtimeEvents = {
                { "onTextCommit", "Text Commit", "Fires when text input is committed", false }
            };

            descriptor.defaultProperties.set("text", juce::String());
            descriptor.defaultProperties.set("textInput.placeholder", juce::String("Type..."));
            descriptor.defaultProperties.set("textInput.multiline", false);
            descriptor.defaultProperties.set("textInput.readOnly", false);
            descriptor.defaultProperties.set("textInput.passwordChar", juce::String());

            {
                WidgetPropertySpec textSpec;
                textSpec.key = juce::Identifier("text");
                textSpec.label = "Text";
                textSpec.kind = WidgetPropertyKind::text;
                textSpec.uiHint = WidgetPropertyUiHint::multiLine;
                textSpec.group = "Content";
                textSpec.order = 10;
                textSpec.hint = "Initial text";
                textSpec.defaultValue = juce::var(juce::String());
                descriptor.propertySpecs.push_back(std::move(textSpec));

                WidgetPropertySpec placeholderSpec;
                placeholderSpec.key = juce::Identifier("textInput.placeholder");
                placeholderSpec.label = "Placeholder";
                placeholderSpec.kind = WidgetPropertyKind::text;
                placeholderSpec.uiHint = WidgetPropertyUiHint::lineEdit;
                placeholderSpec.group = "Content";
                placeholderSpec.order = 20;
                placeholderSpec.hint = "Shown when text is empty";
                placeholderSpec.defaultValue = juce::var("Type...");
                descriptor.propertySpecs.push_back(std::move(placeholderSpec));

                WidgetPropertySpec multilineSpec;
                multilineSpec.key = juce::Identifier("textInput.multiline");
                multilineSpec.label = "Multiline";
                multilineSpec.kind = WidgetPropertyKind::boolean;
                multilineSpec.uiHint = WidgetPropertyUiHint::toggle;
                multilineSpec.group = "Behavior";
                multilineSpec.order = 30;
                multilineSpec.hint = "Enable multi-line editing";
                multilineSpec.defaultValue = juce::var(false);
                descriptor.propertySpecs.push_back(std::move(multilineSpec));

                WidgetPropertySpec readOnlySpec;
                readOnlySpec.key = juce::Identifier("textInput.readOnly");
                readOnlySpec.label = "Read Only";
                readOnlySpec.kind = WidgetPropertyKind::boolean;
                readOnlySpec.uiHint = WidgetPropertyUiHint::toggle;
                readOnlySpec.group = "Behavior";
                readOnlySpec.order = 40;
                readOnlySpec.hint = "Disable user editing";
                readOnlySpec.defaultValue = juce::var(false);
                descriptor.propertySpecs.push_back(std::move(readOnlySpec));

                WidgetPropertySpec passwordSpec;
                passwordSpec.key = juce::Identifier("textInput.passwordChar");
                passwordSpec.label = "Password Char";
                passwordSpec.kind = WidgetPropertyKind::text;
                passwordSpec.uiHint = WidgetPropertyUiHint::lineEdit;
                passwordSpec.group = "Behavior";
                passwordSpec.order = 100;
                passwordSpec.hint = "Single character mask (empty = none)";
                passwordSpec.defaultValue = juce::var(juce::String());
                passwordSpec.advanced = true;
                descriptor.propertySpecs.push_back(std::move(passwordSpec));
            }

            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                const auto text = widget.properties.getWithDefault("text", juce::var(juce::String())).toString();
                const auto placeholder = widget.properties.getWithDefault("textInput.placeholder", juce::var("Type...")).toString();
                const auto readOnly = static_cast<bool>(widget.properties.getWithDefault("textInput.readOnly", juce::var(false)));

                g.setColour(juce::Colour::fromRGB(24, 30, 40));
                g.fillRoundedRectangle(body, 3.0f);
                g.setColour(readOnly ? juce::Colour::fromRGB(98, 104, 118) : juce::Colour::fromRGB(78, 90, 112));
                g.drawRoundedRectangle(body, 3.0f, 1.0f);

                const auto display = text.isNotEmpty() ? text : placeholder;
                g.setColour(text.isNotEmpty() ? juce::Colour::fromRGB(223, 230, 238) : juce::Colour::fromRGB(132, 140, 155));
                g.setFont(juce::FontOptions(12.0f));
                g.drawFittedText(display,
                                 body.reduced(8.0f, 5.0f).toNearestInt(),
                                 juce::Justification::centredLeft,
                                 2);
            };

            descriptor.exportCodegen = [](const ExportCodegenContext& context, ExportCodegenOutput& out)
            {
                const auto text = context.widget.properties.getWithDefault("text", juce::var(juce::String())).toString();
                const auto placeholder = context.widget.properties.getWithDefault("textInput.placeholder", juce::var("Type...")).toString();
                const auto multiline = static_cast<bool>(context.widget.properties.getWithDefault("textInput.multiline", juce::var(false)));
                const auto readOnly = static_cast<bool>(context.widget.properties.getWithDefault("textInput.readOnly", juce::var(false)));
                const auto passwordChar = context.widget.properties.getWithDefault("textInput.passwordChar", juce::var(juce::String())).toString();
                const auto toLiteral = [](const juce::String& value)
                {
                    return juce::JSON::toString(juce::var(value), false);
                };

                out.memberType = "juce::TextEditor";
                out.codegenKind = "juce_text_editor";
                out.constructorLines.clear();
                out.resizedLines.clear();

                out.constructorLines.add("    " + context.memberName + ".setMultiLine(" + juce::String(multiline ? "true" : "false") + ");");
                out.constructorLines.add("    " + context.memberName + ".setReturnKeyStartsNewLine(" + juce::String(multiline ? "true" : "false") + ");");
                out.constructorLines.add("    " + context.memberName + ".setReadOnly(" + juce::String(readOnly ? "true" : "false") + ");");
                out.constructorLines.add("    " + context.memberName + ".setTextToShowWhenEmpty(" + toLiteral(placeholder) + ", juce::Colours::grey);");
                out.constructorLines.add("    " + context.memberName + ".setText(" + toLiteral(text) + ", false);");

                if (passwordChar.isNotEmpty())
                {
                    const auto codepoint = static_cast<int>(passwordChar[0]);
                    out.constructorLines.add("    " + context.memberName + ".setPasswordCharacter(static_cast<juce_wchar>(" + juce::String(codepoint) + "));");
                }
                else
                {
                    out.constructorLines.add("    " + context.memberName + ".setPasswordCharacter(0);");
                }

                out.constructorLines.add("    addAndMakeVisible(" + context.memberName + ");");
                return juce::Result::ok();
            };

            descriptor.cursorProvider = [](const WidgetModel&, juce::Point<float>)
            {
                return juce::MouseCursor(juce::MouseCursor::IBeamCursor);
            };
            return descriptor;
        }
    };

    GYEOL_WIDGET_AUTOREGISTER(TextInputWidget);
}

