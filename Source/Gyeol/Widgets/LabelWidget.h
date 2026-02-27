#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"

namespace Gyeol::Widgets
{
    class LabelWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::label;
            descriptor.typeKey = "label";
            descriptor.displayName = "Label";
            descriptor.exportTargetType = "juce::Label";
            descriptor.defaultBounds = { 0.0f, 0.0f, 120.0f, 28.0f };
            descriptor.minSize = { 60.0f, 20.0f };
            descriptor.defaultProperties.set("text", juce::String("Label"));
            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                g.fillRoundedRectangle(body, 3.0f);
                g.setColour(juce::Colour::fromRGB(236, 238, 242));
                g.setFont(juce::FontOptions(12.0f));
                const auto text = widget.properties.getWithDefault("text", juce::var("Label")).toString();
                g.drawFittedText(text.isEmpty() ? "Label" : text,
                                 body.reduced(6.0f).toNearestInt(),
                                 juce::Justification::centredLeft,
                                 1);
            };
            descriptor.cursorProvider = [](const WidgetModel&, juce::Point<float>)
            {
                return juce::MouseCursor(juce::MouseCursor::IBeamCursor);
            };
            descriptor.dropOptions = [](const WidgetModel&, const AssetRef& asset)
            {
                if (asset.mime.startsWithIgnoreCase("text/"))
                {
                    return std::vector<DropOption> {
                        { "Text", juce::Identifier("text"), "Use dropped text asset id as label text source" }
                    };
                }

                if (asset.mime.startsWithIgnoreCase("image/"))
                {
                    return std::vector<DropOption> {
                        { "Background Image", juce::Identifier("label.backgroundImage"), "Apply image to label background" }
                    };
                }

                return std::vector<DropOption> {};
            };
            descriptor.applyDrop = [](PropertyBag& patchOut, const WidgetModel&, const AssetRef& asset, const DropOption& option)
            {
                patchOut.set(option.propKey, asset.assetId);
                return juce::Result::ok();
            };
            return descriptor;
        }
    };

    GYEOL_WIDGET_AUTOREGISTER(LabelWidget);
}
