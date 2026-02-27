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
            descriptor.exportTargetType = "juce::TextButton";
            descriptor.defaultBounds = { 0.0f, 0.0f, 96.0f, 30.0f };
            descriptor.minSize = { 48.0f, 24.0f };
            descriptor.defaultProperties.set("text", juce::String("Button"));
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
