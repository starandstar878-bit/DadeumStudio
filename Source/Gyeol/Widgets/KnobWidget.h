#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Gyeol::Widgets
{
    class KnobWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::knob;
            descriptor.typeKey = "knob";
            descriptor.displayName = "Knob";
            descriptor.category = "Control";
            descriptor.tags = { "knob", "rotary", "dial" };
            descriptor.iconKey = "knob";
            descriptor.exportTargetType = "juce::Slider::RotaryVerticalDrag";
            descriptor.defaultBounds = { 0.0f, 0.0f, 56.0f, 56.0f };
            descriptor.minSize = { 32.0f, 32.0f };
                        descriptor.runtimeEvents = {
                { "onValueChanged", "Value Changed", "Fires while value is changing", true },
                { "onValueCommit", "Value Commit", "Fires when value edit is committed", false }
            };

            descriptor.defaultProperties.set("knob.style", "rotaryVerticalDrag");
            descriptor.defaultProperties.set("knob.rangeMin", 0.0);
            descriptor.defaultProperties.set("knob.rangeMax", 1.0);
            descriptor.defaultProperties.set("knob.step", 0.0);
            descriptor.defaultProperties.set("value", 0.5);

            {
                WidgetPropertySpec styleSpec;
                styleSpec.key = juce::Identifier("knob.style");
                styleSpec.label = "Knob Style";
                styleSpec.kind = WidgetPropertyKind::enumChoice;
                styleSpec.uiHint = WidgetPropertyUiHint::dropdown;
                styleSpec.group = "Style";
                styleSpec.order = 10;
                styleSpec.hint = "JUCE rotary style";
                styleSpec.defaultValue = juce::var("rotaryVerticalDrag");
                styleSpec.enumOptions = {
                    { "rotary", "Rotary" },
                    { "rotaryHorizontalDrag", "Rotary Horizontal Drag" },
                    { "rotaryVerticalDrag", "Rotary Vertical Drag" },
                    { "rotaryHorizontalVerticalDrag", "Rotary Horizontal/Vertical Drag" }
                };
                descriptor.propertySpecs.push_back(std::move(styleSpec));

                WidgetPropertySpec rangeMinSpec;
                rangeMinSpec.key = juce::Identifier("knob.rangeMin");
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
                rangeMaxSpec.key = juce::Identifier("knob.rangeMax");
                rangeMaxSpec.label = "Range Max";
                rangeMaxSpec.kind = WidgetPropertyKind::number;
                rangeMaxSpec.uiHint = WidgetPropertyUiHint::spinBox;
                rangeMaxSpec.group = "Range";
                rangeMaxSpec.order = 20;
                rangeMaxSpec.hint = "Maximum range value";
                rangeMaxSpec.defaultValue = juce::var(1.0);
                rangeMaxSpec.decimals = 4;
                descriptor.propertySpecs.push_back(std::move(rangeMaxSpec));

                WidgetPropertySpec stepSpec;
                stepSpec.key = juce::Identifier("knob.step");
                stepSpec.label = "Step";
                stepSpec.kind = WidgetPropertyKind::number;
                stepSpec.uiHint = WidgetPropertyUiHint::spinBox;
                stepSpec.group = "Range";
                stepSpec.order = 30;
                stepSpec.hint = "0 means continuous";
                stepSpec.defaultValue = juce::var(0.0);
                stepSpec.minValue = 0.0;
                stepSpec.decimals = 6;
                descriptor.propertySpecs.push_back(std::move(stepSpec));

                WidgetPropertySpec valueSpec;
                valueSpec.key = juce::Identifier("value");
                valueSpec.label = "Value";
                valueSpec.kind = WidgetPropertyKind::number;
                valueSpec.uiHint = WidgetPropertyUiHint::spinBox;
                valueSpec.group = "Value";
                valueSpec.order = 10;
                valueSpec.hint = "Current knob value";
                valueSpec.defaultValue = juce::var(0.5);
                valueSpec.step = 0.01;
                valueSpec.decimals = 4;
                descriptor.propertySpecs.push_back(std::move(valueSpec));

                WidgetPropertySpec capImageSpec;
                capImageSpec.key = juce::Identifier("knob.capImage");
                capImageSpec.label = "Cap Image";
                capImageSpec.kind = WidgetPropertyKind::assetRef;
                capImageSpec.uiHint = WidgetPropertyUiHint::assetPicker;
                capImageSpec.group = "Appearance";
                capImageSpec.order = 100;
                capImageSpec.hint = "Optional image asset id for knob cap";
                capImageSpec.acceptedAssetKinds = { AssetKind::image };
                capImageSpec.advanced = true;
                descriptor.propertySpecs.push_back(std::move(capImageSpec));
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

                auto rangeMin = static_cast<float>(readNumeric("knob.rangeMin", 0.0));
                auto rangeMax = static_cast<float>(readNumeric("knob.rangeMax", 1.0));
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
                const auto diameter = std::max(12.0f, std::min(body.getWidth(), body.getHeight()) - 6.0f);
                const auto knob = juce::Rectangle<float>(diameter, diameter).withCentre(body.getCentre());
                g.fillEllipse(knob);

                g.setColour(juce::Colour::fromRGB(95, 160, 255));
                const auto minAngle = -juce::MathConstants<float>::pi * 0.75f;
                const auto maxAngle = juce::MathConstants<float>::pi * 0.75f;
                const auto angle = juce::jmap<float>(normalized, 0.0f, 1.0f, minAngle, maxAngle);

                const auto c = knob.getCentre();
                const auto r = knob.getWidth() * 0.34f;
                g.drawLine(c.x, c.y, c.x + std::cos(angle) * r, c.y + std::sin(angle) * r, 2.0f);
            };

            descriptor.exportCodegen = [](const ExportCodegenContext& context, ExportCodegenOutput& out) -> juce::Result
            {
                const auto readNumeric = [&context](const juce::Identifier& key, double fallback) -> double
                {
                    const auto raw = context.widget.properties.getWithDefault(key, juce::var(fallback));
                    if (!raw.isInt() && !raw.isInt64() && !raw.isDouble())
                        return fallback;

                    const auto parsed = static_cast<double>(raw);
                    return std::isfinite(parsed) ? parsed : fallback;
                };
                const auto styleToLiteral = [](const juce::String& styleKey) -> juce::String
                {
                    static const std::array<std::pair<const char*, const char*>, 4> mapping {
                        std::pair<const char*, const char*> { "rotary", "juce::Slider::Rotary" },
                        { "rotaryHorizontalDrag", "juce::Slider::RotaryHorizontalDrag" },
                        { "rotaryVerticalDrag", "juce::Slider::RotaryVerticalDrag" },
                        { "rotaryHorizontalVerticalDrag", "juce::Slider::RotaryHorizontalVerticalDrag" }
                    };
                    for (const auto& [key, literal] : mapping)
                    {
                        if (styleKey == key)
                            return literal;
                    }
                    return "juce::Slider::RotaryVerticalDrag";
                };

                auto rangeMin = readNumeric("knob.rangeMin", 0.0);
                auto rangeMax = readNumeric("knob.rangeMax", 1.0);
                if (!std::isfinite(rangeMin))
                    rangeMin = 0.0;
                if (!std::isfinite(rangeMax))
                    rangeMax = rangeMin + 1.0;
                if (rangeMax <= rangeMin)
                    rangeMax = rangeMin + 1.0;

                auto step = readNumeric("knob.step", 0.0);
                if (!std::isfinite(step) || step < 0.0)
                    step = 0.0;

                auto value = readNumeric("value", (rangeMin + rangeMax) * 0.5);
                if (!std::isfinite(value))
                    value = rangeMin;
                value = juce::jlimit(rangeMin, rangeMax, value);

                auto styleKey = context.widget.properties.getWithDefault("knob.style", juce::var("rotaryVerticalDrag")).toString().trim();
                if (styleKey.isEmpty())
                    styleKey = "rotaryVerticalDrag";

                out.memberType = "juce::Slider";
                out.codegenKind = "juce_knob_dynamic";
                out.constructorLines.clear();
                out.resizedLines.clear();

                out.constructorLines.add("    " + context.memberName + ".setSliderStyle(" + styleToLiteral(styleKey) + ");");
                out.constructorLines.add("    " + context.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
                out.constructorLines.add("    " + context.memberName + ".setRange(" + juce::String(rangeMin, 8)
                                         + ", " + juce::String(rangeMax, 8)
                                         + ", " + juce::String(step, 8) + ");");
                out.constructorLines.add("    " + context.memberName + ".setValue(" + juce::String(value, 8)
                                         + ", juce::dontSendNotification);");
                out.constructorLines.add("    addAndMakeVisible(" + context.memberName + ");");
                return juce::Result::ok();
            };

            descriptor.hitTest = [](const WidgetModel& widget, juce::Point<float> local)
            {
                const auto center = juce::Point<float>(widget.bounds.getWidth() * 0.5f, widget.bounds.getHeight() * 0.5f);
                const auto radius = std::min(widget.bounds.getWidth(), widget.bounds.getHeight()) * 0.5f;
                return local.getDistanceFrom(center) <= radius;
            };
            descriptor.cursorProvider = [](const WidgetModel&, juce::Point<float>)
            {
                return juce::MouseCursor(juce::MouseCursor::UpDownResizeCursor);
            };
            descriptor.dropOptions = [](const WidgetModel&, const AssetRef& asset)
            {
                if (!asset.mime.startsWithIgnoreCase("image/"))
                    return std::vector<DropOption> {};

                return std::vector<DropOption> {
                    { "Cap Image", juce::Identifier("knob.capImage"), "Apply image to knob cap" }
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

    GYEOL_WIDGET_AUTOREGISTER(KnobWidget);
}

