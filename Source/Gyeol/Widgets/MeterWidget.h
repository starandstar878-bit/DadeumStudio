#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>
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
            descriptor.category = "Display";
            descriptor.tags = { "meter", "level", "display" };
            descriptor.iconKey = "meter";
            descriptor.exportTargetType = "gyeol::Meter";
            descriptor.defaultBounds = { 0.0f, 0.0f, 36.0f, 120.0f };
            descriptor.minSize = { 20.0f, 48.0f };

            descriptor.defaultProperties.set("meter.orientation", "vertical");
            descriptor.defaultProperties.set("meter.rangeMin", 0.0);
            descriptor.defaultProperties.set("meter.rangeMax", 1.0);
            descriptor.defaultProperties.set("value", 0.62);

            {
                WidgetPropertySpec orientationSpec;
                orientationSpec.key = juce::Identifier("meter.orientation");
                orientationSpec.label = "Orientation";
                orientationSpec.kind = WidgetPropertyKind::enumChoice;
                orientationSpec.uiHint = WidgetPropertyUiHint::dropdown;
                orientationSpec.group = "Style";
                orientationSpec.order = 10;
                orientationSpec.hint = "Meter fill direction";
                orientationSpec.defaultValue = juce::var("vertical");
                orientationSpec.enumOptions = {
                    { "vertical", "Vertical" },
                    { "horizontal", "Horizontal" }
                };
                descriptor.propertySpecs.push_back(std::move(orientationSpec));

                WidgetPropertySpec rangeMinSpec;
                rangeMinSpec.key = juce::Identifier("meter.rangeMin");
                rangeMinSpec.label = "Range Min";
                rangeMinSpec.kind = WidgetPropertyKind::number;
                rangeMinSpec.uiHint = WidgetPropertyUiHint::spinBox;
                rangeMinSpec.group = "Range";
                rangeMinSpec.order = 10;
                rangeMinSpec.hint = "Minimum range value";
                rangeMinSpec.defaultValue = juce::var(0.0);
                rangeMinSpec.decimals = 4;
                descriptor.propertySpecs.push_back(std::move(rangeMinSpec));

                WidgetPropertySpec rangeMaxSpec;
                rangeMaxSpec.key = juce::Identifier("meter.rangeMax");
                rangeMaxSpec.label = "Range Max";
                rangeMaxSpec.kind = WidgetPropertyKind::number;
                rangeMaxSpec.uiHint = WidgetPropertyUiHint::spinBox;
                rangeMaxSpec.group = "Range";
                rangeMaxSpec.order = 20;
                rangeMaxSpec.hint = "Maximum range value";
                rangeMaxSpec.defaultValue = juce::var(1.0);
                rangeMaxSpec.decimals = 4;
                descriptor.propertySpecs.push_back(std::move(rangeMaxSpec));

                WidgetPropertySpec valueSpec;
                valueSpec.key = juce::Identifier("value");
                valueSpec.label = "Value";
                valueSpec.kind = WidgetPropertyKind::number;
                valueSpec.uiHint = WidgetPropertyUiHint::spinBox;
                valueSpec.group = "Value";
                valueSpec.order = 10;
                valueSpec.hint = "Current meter level";
                valueSpec.defaultValue = juce::var(0.62);
                valueSpec.step = 0.01;
                valueSpec.decimals = 4;
                descriptor.propertySpecs.push_back(std::move(valueSpec));

                WidgetPropertySpec fillImageSpec;
                fillImageSpec.key = juce::Identifier("meter.fillImage");
                fillImageSpec.label = "Fill Image";
                fillImageSpec.kind = WidgetPropertyKind::assetRef;
                fillImageSpec.uiHint = WidgetPropertyUiHint::assetPicker;
                fillImageSpec.group = "Appearance";
                fillImageSpec.order = 100;
                fillImageSpec.hint = "Optional image asset id for fill area";
                fillImageSpec.acceptedAssetKinds = { AssetKind::image };
                fillImageSpec.advanced = true;
                descriptor.propertySpecs.push_back(std::move(fillImageSpec));

                WidgetPropertySpec bgImageSpec;
                bgImageSpec.key = juce::Identifier("meter.backgroundImage");
                bgImageSpec.label = "Background Image";
                bgImageSpec.kind = WidgetPropertyKind::assetRef;
                bgImageSpec.uiHint = WidgetPropertyUiHint::assetPicker;
                bgImageSpec.group = "Appearance";
                bgImageSpec.order = 110;
                bgImageSpec.hint = "Optional image asset id for meter body";
                bgImageSpec.acceptedAssetKinds = { AssetKind::image };
                bgImageSpec.advanced = true;
                descriptor.propertySpecs.push_back(std::move(bgImageSpec));
            }

            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                const auto readNumeric = [&widget](const juce::Identifier& key, double fallback) -> double
                {
                    const auto raw = widget.properties.getWithDefault(key, juce::var(fallback));
                    if (!raw.isInt() && !raw.isInt64() && !raw.isDouble())
                        return fallback;

                    const auto parsed = static_cast<double>(raw);
                    return std::isfinite(parsed) ? parsed : fallback;
                };

                const auto orientation = widget.properties.getWithDefault("meter.orientation", juce::var("vertical")).toString().trim();
                auto rangeMin = static_cast<float>(readNumeric("meter.rangeMin", 0.0));
                auto rangeMax = static_cast<float>(readNumeric("meter.rangeMax", 1.0));
                if (!std::isfinite(rangeMin))
                    rangeMin = 0.0f;
                if (!std::isfinite(rangeMax))
                    rangeMax = rangeMin + 1.0f;
                if (rangeMax <= rangeMin)
                    rangeMax = rangeMin + 1.0f;

                auto value = static_cast<float>(readNumeric("value", (rangeMin + rangeMax) * 0.5));
                if (!std::isfinite(value))
                    value = rangeMin;
                value = juce::jlimit(rangeMin, rangeMax, value);
                const auto normalized = [rangeMin, rangeMax](float v) noexcept
                {
                    const auto width = rangeMax - rangeMin;
                    if (width <= 0.000001f)
                        return 0.0f;
                    return juce::jlimit(0.0f, 1.0f, (v - rangeMin) / width);
                }(value);

                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                g.fillRoundedRectangle(body, 4.0f);

                auto fillArea = body.reduced(4.0f);
                juce::Rectangle<float> level;
                if (orientation == "horizontal")
                    level = fillArea.removeFromLeft(fillArea.getWidth() * normalized);
                else
                    level = fillArea.removeFromBottom(fillArea.getHeight() * normalized);

                g.setColour(juce::Colour::fromRGB(95, 210, 150));
                g.fillRoundedRectangle(level, 2.0f);
            };

            descriptor.exportCodegen = [](const ExportCodegenContext& context, ExportCodegenOutput& out)
            {
                const auto readNumeric = [&context](const juce::Identifier& key, double fallback) -> double
                {
                    const auto raw = context.widget.properties.getWithDefault(key, juce::var(fallback));
                    if (!raw.isInt() && !raw.isInt64() && !raw.isDouble())
                        return fallback;

                    const auto parsed = static_cast<double>(raw);
                    return std::isfinite(parsed) ? parsed : fallback;
                };

                auto rangeMin = readNumeric("meter.rangeMin", 0.0);
                auto rangeMax = readNumeric("meter.rangeMax", 1.0);
                if (!std::isfinite(rangeMin))
                    rangeMin = 0.0;
                if (!std::isfinite(rangeMax))
                    rangeMax = rangeMin + 1.0;
                if (rangeMax <= rangeMin)
                    rangeMax = rangeMin + 1.0;

                auto meterValue = readNumeric("value", (rangeMin + rangeMax) * 0.5);
                if (!std::isfinite(meterValue))
                    meterValue = rangeMin;
                meterValue = juce::jlimit(rangeMin, rangeMax, meterValue);

                const auto orientation = context.widget.properties.getWithDefault("meter.orientation", juce::var("vertical")).toString().trim();
                const auto styleLiteral = orientation == "horizontal"
                                            ? juce::String("juce::Slider::LinearBar")
                                            : juce::String("juce::Slider::LinearVertical");

                out.memberType = "juce::Slider";
                out.codegenKind = "custom_meter_slider_dynamic";
                out.constructorLines.clear();
                out.resizedLines.clear();

                out.constructorLines.add("    " + context.memberName + ".setSliderStyle(" + styleLiteral + ");");
                out.constructorLines.add("    " + context.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
                out.constructorLines.add("    " + context.memberName + ".setRange(" + juce::String(rangeMin, 8)
                                         + ", " + juce::String(rangeMax, 8)
                                         + ", 0.0);");
                out.constructorLines.add("    " + context.memberName + ".setValue(" + juce::String(meterValue, 8)
                                         + ", juce::dontSendNotification);");
                out.constructorLines.add("    " + context.memberName + ".setEnabled(false);");
                out.constructorLines.add("    " + context.memberName + ".setInterceptsMouseClicks(false, false);");
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
