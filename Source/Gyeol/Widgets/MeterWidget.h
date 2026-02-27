#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <cmath>

namespace Gyeol::Widgets
{
    class MeterWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::meter;
            descriptor.typeKey = "meter";
            descriptor.displayName = "Meter";
            descriptor.exportTargetType = "gyeol::Meter";
            descriptor.defaultBounds = { 0.0f, 0.0f, 36.0f, 120.0f };
            descriptor.minSize = { 20.0f, 48.0f };
            descriptor.defaultProperties.set("value", 0.62);
            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                g.fillRoundedRectangle(body, 4.0f);
                auto fillArea = body.reduced(4.0f);
                const auto rawValue = static_cast<float>(static_cast<double>(widget.properties.getWithDefault("value", juce::var(0.62))));
                const auto meterValue = juce::jlimit(0.0f, 1.0f, std::isfinite(rawValue) ? rawValue : 0.62f);
                const auto level = fillArea.removeFromBottom(fillArea.getHeight() * meterValue);
                g.setColour(juce::Colour::fromRGB(95, 210, 150));
                g.fillRoundedRectangle(level, 2.0f);
            };
            descriptor.exportCodegen = [](const ExportCodegenContext& context, ExportCodegenOutput& out)
            {
                const auto rawValue = static_cast<double>(context.widget.properties.getWithDefault("value", juce::var(0.62)));
                const auto meterValue = juce::jlimit(0.0, 1.0, std::isfinite(rawValue) ? rawValue : 0.62);

                out.memberType = "juce::Slider";
                out.codegenKind = "custom_meter_slider_vertical";
                out.constructorLines.clear();
                out.resizedLines.clear();

                out.constructorLines.add("    " + context.memberName + ".setSliderStyle(juce::Slider::LinearVertical);");
                out.constructorLines.add("    " + context.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
                out.constructorLines.add("    " + context.memberName + ".setRange(0.0, 1.0, 0.0);");
                out.constructorLines.add("    " + context.memberName + ".setValue(" + juce::String(meterValue, 6) + ");");
                out.constructorLines.add("    " + context.memberName + ".setEnabled(false);");
                out.constructorLines.add("    addAndMakeVisible(" + context.memberName + ");");
                return juce::Result::ok();
            };
            descriptor.cursorProvider = [](const WidgetModel&, juce::Point<float>)
            {
                return juce::MouseCursor(juce::MouseCursor::NormalCursor);
            };
            descriptor.dropOptions = [](const WidgetModel&, const AssetRef& asset)
            {
                if (!asset.mime.startsWithIgnoreCase("image/"))
                    return std::vector<DropOption> {};

                return std::vector<DropOption> {
                    { "Fill Image", juce::Identifier("meter.fillImage"), "Apply image to meter fill" },
                    { "Background Image", juce::Identifier("meter.backgroundImage"), "Apply image to meter body" }
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

    GYEOL_WIDGET_AUTOREGISTER(MeterWidget);
}
