#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"

namespace Gyeol::Widgets
{
    class ToggleWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::toggle;
            descriptor.typeKey = "toggle";
            descriptor.displayName = "Toggle";
            descriptor.category = "Input";
            descriptor.tags = { "toggle", "switch", "boolean" };
            descriptor.iconKey = "toggle";
            descriptor.exportTargetType = "juce::ToggleButton";
            descriptor.defaultBounds = { 0.0f, 0.0f, 120.0f, 28.0f };
            descriptor.minSize = { 72.0f, 24.0f };
                        descriptor.runtimeEvents = {
                { "onClick", "Click", "Fires when the toggle is clicked", false },
                { "onToggleChanged", "Toggle Changed", "Fires when checked state changes", false }
            };

            descriptor.defaultProperties.set("text", juce::String("Toggle"));
            descriptor.defaultProperties.set("state", false);

            {
                WidgetPropertySpec textSpec;
                textSpec.key = juce::Identifier("text");
                textSpec.label = "Text";
                textSpec.kind = WidgetPropertyKind::text;
                textSpec.uiHint = WidgetPropertyUiHint::lineEdit;
                textSpec.group = "Content";
                textSpec.order = 10;
                textSpec.hint = "Toggle label text";
                textSpec.defaultValue = juce::var("Toggle");
                descriptor.propertySpecs.push_back(std::move(textSpec));

                WidgetPropertySpec stateSpec;
                stateSpec.key = juce::Identifier("state");
                stateSpec.label = "State";
                stateSpec.kind = WidgetPropertyKind::boolean;
                stateSpec.uiHint = WidgetPropertyUiHint::toggle;
                stateSpec.group = "Value";
                stateSpec.order = 20;
                stateSpec.hint = "Checked state";
                stateSpec.defaultValue = juce::var(false);
                descriptor.propertySpecs.push_back(std::move(stateSpec));
            }

            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                const auto on = static_cast<bool>(widget.properties.getWithDefault("state", juce::var(false)));
                const auto text = widget.properties.getWithDefault("text", juce::var("Toggle")).toString();
                auto content = body;

                g.setColour(juce::Colour::fromRGB(33, 38, 48));
                g.fillRoundedRectangle(body, 5.0f);

                const auto indicator = content.removeFromLeft(std::max(20.0f, body.getHeight())).reduced(4.0f);
                g.setColour(on ? juce::Colour::fromRGB(86, 210, 132) : juce::Colour::fromRGB(90, 98, 114));
                g.fillEllipse(indicator);

                if (on)
                {
                    g.setColour(juce::Colour::fromRGB(14, 28, 22));
                    g.drawLine(indicator.getX() + 4.0f, indicator.getCentreY(),
                               indicator.getCentreX() - 1.0f, indicator.getBottom() - 5.0f, 2.0f);
                    g.drawLine(indicator.getCentreX() - 1.0f, indicator.getBottom() - 5.0f,
                               indicator.getRight() - 4.0f, indicator.getY() + 5.0f, 2.0f);
                }

                g.setColour(juce::Colour::fromRGB(220, 226, 236));
                g.setFont(juce::FontOptions(12.0f));
                g.drawFittedText(text.isNotEmpty() ? text : juce::String("Toggle"),
                                 content.reduced(6.0f).toNearestInt(),
                                 juce::Justification::centredLeft,
                                 1);
            };

            descriptor.exportCodegen = [](const ExportCodegenContext& context, ExportCodegenOutput& out)
            {
                const auto text = context.widget.properties.getWithDefault("text", juce::var("Toggle")).toString();
                const auto state = static_cast<bool>(context.widget.properties.getWithDefault("state", juce::var(false)));
                const auto toLiteral = [](const juce::String& value)
                {
                    return juce::JSON::toString(juce::var(value), false);
                };

                out.memberType = "juce::ToggleButton";
                out.codegenKind = "juce_toggle_button";
                out.constructorLines.clear();
                out.resizedLines.clear();

                out.constructorLines.add("    " + context.memberName + ".setButtonText(" + toLiteral(text.isNotEmpty() ? text : juce::String("Toggle")) + ");");
                out.constructorLines.add("    " + context.memberName + ".setToggleState(" + juce::String(state ? "true" : "false")
                                         + ", juce::dontSendNotification);");
                out.constructorLines.add("    addAndMakeVisible(" + context.memberName + ");");
                return juce::Result::ok();
            };

            descriptor.cursorProvider = [](const WidgetModel&, juce::Point<float>)
            {
                return juce::MouseCursor(juce::MouseCursor::PointingHandCursor);
            };
            return descriptor;
        }
    };

    GYEOL_WIDGET_AUTOREGISTER(ToggleWidget);
}

