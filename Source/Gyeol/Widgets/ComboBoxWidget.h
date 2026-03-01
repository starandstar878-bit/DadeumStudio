#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>

namespace Gyeol::Widgets
{
    class ComboBoxWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::comboBox;
            descriptor.typeKey = "comboBox";
            descriptor.displayName = "ComboBox";
            descriptor.category = "Input";
            descriptor.tags = { "combo", "dropdown", "selection" };
            descriptor.iconKey = "comboBox";
            descriptor.exportTargetType = "juce::ComboBox";
            descriptor.defaultBounds = { 0.0f, 0.0f, 150.0f, 28.0f };
            descriptor.minSize = { 90.0f, 24.0f };
                        descriptor.runtimeEvents = {
                { "onSelectionChanged", "Selection Changed", "Fires when selected item changes", false }
            };

            descriptor.defaultProperties.set("combo.items", juce::String("Item 1\nItem 2\nItem 3"));
            descriptor.defaultProperties.set("combo.selectedIndex", 1);
            descriptor.defaultProperties.set("combo.editable", false);

            {
                WidgetPropertySpec itemsSpec;
                itemsSpec.key = juce::Identifier("combo.items");
                itemsSpec.label = "Items";
                itemsSpec.kind = WidgetPropertyKind::text;
                itemsSpec.uiHint = WidgetPropertyUiHint::multiLine;
                itemsSpec.group = "Content";
                itemsSpec.order = 10;
                itemsSpec.hint = "One item per line";
                itemsSpec.defaultValue = juce::var("Item 1\nItem 2\nItem 3");
                descriptor.propertySpecs.push_back(std::move(itemsSpec));

                WidgetPropertySpec selectedSpec;
                selectedSpec.key = juce::Identifier("combo.selectedIndex");
                selectedSpec.label = "Selected Index";
                selectedSpec.kind = WidgetPropertyKind::integer;
                selectedSpec.uiHint = WidgetPropertyUiHint::spinBox;
                selectedSpec.group = "Value";
                selectedSpec.order = 20;
                selectedSpec.hint = "1-based item index (0 = none)";
                selectedSpec.defaultValue = juce::var(1);
                selectedSpec.minValue = 0.0;
                selectedSpec.step = 1.0;
                descriptor.propertySpecs.push_back(std::move(selectedSpec));

                WidgetPropertySpec editableSpec;
                editableSpec.key = juce::Identifier("combo.editable");
                editableSpec.label = "Editable";
                editableSpec.kind = WidgetPropertyKind::boolean;
                editableSpec.uiHint = WidgetPropertyUiHint::toggle;
                editableSpec.group = "Behavior";
                editableSpec.order = 30;
                editableSpec.hint = "Allow free text input";
                editableSpec.defaultValue = juce::var(false);
                descriptor.propertySpecs.push_back(std::move(editableSpec));
            }

            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                juce::StringArray items;
                items.addLines(widget.properties.getWithDefault("combo.items", juce::var("Item 1\nItem 2\nItem 3")).toString());
                for (int i = items.size() - 1; i >= 0; --i)
                {
                    if (items[i].trim().isEmpty())
                        items.remove(i);
                    else
                        items.set(i, items[i].trim());
                }

                auto selectedIndex = static_cast<int>(widget.properties.getWithDefault("combo.selectedIndex", juce::var(1)));
                selectedIndex = juce::jlimit(0, std::max(0, items.size()), selectedIndex);
                const auto text = selectedIndex > 0 && selectedIndex <= items.size()
                                      ? items[selectedIndex - 1]
                                      : juce::String("Select...");

                g.setColour(juce::Colour::fromRGB(36, 42, 54));
                g.fillRoundedRectangle(body, 4.0f);
                g.setColour(juce::Colour::fromRGB(72, 82, 98));
                g.drawRoundedRectangle(body, 4.0f, 1.0f);

                auto textArea = body.reduced(8.0f, 0.0f);
                const auto arrowArea = textArea.removeFromRight(16.0f);
                textArea.removeFromRight(4.0f);

                g.setColour(juce::Colour::fromRGB(224, 230, 238));
                g.setFont(juce::FontOptions(12.0f));
                g.drawFittedText(text, textArea.toNearestInt(), juce::Justification::centredLeft, 1);

                juce::Path arrow;
                const auto cx = arrowArea.getCentreX();
                const auto cy = arrowArea.getCentreY();
                arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
                arrow.lineTo(cx + 4.0f, cy - 2.0f);
                arrow.lineTo(cx, cy + 3.0f);
                arrow.closeSubPath();
                g.setColour(juce::Colour::fromRGB(170, 178, 192));
                g.fillPath(arrow);
            };

            descriptor.exportCodegen = [](const ExportCodegenContext& context, ExportCodegenOutput& out)
            {
                juce::StringArray items;
                items.addLines(context.widget.properties.getWithDefault("combo.items", juce::var("Item 1\nItem 2\nItem 3")).toString());
                for (int i = items.size() - 1; i >= 0; --i)
                {
                    if (items[i].trim().isEmpty())
                        items.remove(i);
                    else
                        items.set(i, items[i].trim());
                }
                if (items.isEmpty())
                    items.add("Item 1");

                auto selectedIndex = static_cast<int>(context.widget.properties.getWithDefault("combo.selectedIndex", juce::var(1)));
                selectedIndex = juce::jlimit(0, std::max(0, items.size()), selectedIndex);
                const auto editable = static_cast<bool>(context.widget.properties.getWithDefault("combo.editable", juce::var(false)));
                const auto toLiteral = [](const juce::String& value)
                {
                    return juce::JSON::toString(juce::var(value), false);
                };

                out.memberType = "juce::ComboBox";
                out.codegenKind = "juce_combo_box";
                out.constructorLines.clear();
                out.resizedLines.clear();

                for (int i = 0; i < items.size(); ++i)
                {
                    out.constructorLines.add("    " + context.memberName + ".addItem(" + toLiteral(items[i])
                                             + ", " + juce::String(i + 1) + ");");
                }
                out.constructorLines.add("    " + context.memberName + ".setEditableText(" + juce::String(editable ? "true" : "false") + ");");
                out.constructorLines.add("    " + context.memberName + ".setTextWhenNothingSelected(\"Select...\");");
                if (selectedIndex > 0)
                {
                    out.constructorLines.add("    " + context.memberName + ".setSelectedId(" + juce::String(selectedIndex)
                                             + ", juce::dontSendNotification);");
                }
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

    GYEOL_WIDGET_AUTOREGISTER(ComboBoxWidget);
}

