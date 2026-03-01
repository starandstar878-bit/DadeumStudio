#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"

namespace Gyeol::Widgets
{
    class ButtonWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::button;
            descriptor.typeKey = "button";
            descriptor.displayName = "Button";
            descriptor.category = "Control";
            descriptor.tags = { "button", "trigger", "click" };
            descriptor.iconKey = "button";
            descriptor.exportTargetType = "juce::TextButton";
            descriptor.defaultBounds = { 0.0f, 0.0f, 96.0f, 30.0f };
            descriptor.minSize = { 48.0f, 24.0f };
            descriptor.defaultProperties.set("text", juce::String("Button"));
                        descriptor.runtimeEvents = {
                { "onClick", "Click", "Fires when the button is clicked", false },
                { "onPress", "Press", "Fires when mouse/touch is pressed", false },
                { "onRelease", "Release", "Fires when mouse/touch is released", false }
            };
            {
                WidgetPropertySpec textSpec;
                textSpec.key = juce::Identifier("text");
                textSpec.label = "Text";
                textSpec.kind = WidgetPropertyKind::text;
                textSpec.uiHint = WidgetPropertyUiHint::lineEdit;
                textSpec.group = "Content";
                textSpec.order = 10;
                textSpec.hint = "Button caption text";
                textSpec.defaultValue = juce::var("Button");
                descriptor.propertySpecs.push_back(std::move(textSpec));

                WidgetPropertySpec bgImageSpec;
                bgImageSpec.key = juce::Identifier("button.backgroundImage");
                bgImageSpec.label = "Background Image";
                bgImageSpec.kind = WidgetPropertyKind::assetRef;
                bgImageSpec.uiHint = WidgetPropertyUiHint::assetPicker;
                bgImageSpec.group = "Appearance";
                bgImageSpec.order = 100;
                bgImageSpec.hint = "Optional image asset id for button body";
                bgImageSpec.acceptedAssetKinds = { AssetKind::image };
                bgImageSpec.advanced = false;
                descriptor.propertySpecs.push_back(std::move(bgImageSpec));

                WidgetPropertySpec iconImageSpec;
                iconImageSpec.key = juce::Identifier("button.iconImage");
                iconImageSpec.label = "Icon Image";
                iconImageSpec.kind = WidgetPropertyKind::assetRef;
                iconImageSpec.uiHint = WidgetPropertyUiHint::assetPicker;
                iconImageSpec.group = "Appearance";
                iconImageSpec.order = 110;
                iconImageSpec.hint = "Optional image asset id for icon";
                iconImageSpec.acceptedAssetKinds = { AssetKind::image };
                iconImageSpec.advanced = true;
                descriptor.propertySpecs.push_back(std::move(iconImageSpec));
            }
            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                g.fillRoundedRectangle(body, 6.0f);
                g.setColour(juce::Colour::fromRGB(228, 232, 238));
                g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
                const auto text = widget.properties.getWithDefault("text", juce::var("Button")).toString();
                g.drawFittedText(text.isEmpty() ? "Button" : text,
                                 body.toNearestInt(),
                                 juce::Justification::centred,
                                 1);
            };
            descriptor.cursorProvider = [](const WidgetModel&, juce::Point<float>)
            {
                return juce::MouseCursor(juce::MouseCursor::PointingHandCursor);
            };
            descriptor.dropOptions = [](const WidgetModel&, const AssetRef& asset)
            {
                if (!asset.mime.startsWithIgnoreCase("image/"))
                    return std::vector<DropOption> {};

                return std::vector<DropOption> {
                    { "Button Background", juce::Identifier("button.backgroundImage"), "Apply image to button body" },
                    { "Button Icon", juce::Identifier("button.iconImage"), "Apply image as button icon" }
                };
            };
            descriptor.applyDrop = [](PropertyBag& patchOut, const WidgetModel&, const AssetRef& asset, const DropOption& option)
            {
                patchOut.set(option.propKey, asset.assetId);
                return juce::Result::ok();
            };
            return descriptor;
        }
    };

    GYEOL_WIDGET_AUTOREGISTER(ButtonWidget);
}

