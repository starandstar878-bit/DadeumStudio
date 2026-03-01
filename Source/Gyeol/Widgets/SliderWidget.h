#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace Gyeol::Widgets
{
    class SliderWidget final : public WidgetClass
    {
    public:
        WidgetDescriptor makeDescriptor() const override
        {
            WidgetDescriptor descriptor;
            descriptor.type = WidgetType::slider;
            descriptor.typeKey = "slider";
            descriptor.displayName = "Slider";
            descriptor.category = "Control";
            descriptor.tags = { "slider", "linear", "range" };
            descriptor.iconKey = "slider";
            descriptor.exportTargetType = "juce::Slider::LinearHorizontal";
            descriptor.defaultBounds = { 0.0f, 0.0f, 170.0f, 34.0f };
            descriptor.minSize = { 80.0f, 24.0f };
                        descriptor.runtimeEvents = {
                { "onValueChanged", "Value Changed", "Fires while value is changing", true },
                { "onValueCommit", "Value Commit", "Fires when value edit is committed", false }
            };

            descriptor.defaultProperties.set("slider.style", "linearHorizontal");
            descriptor.defaultProperties.set("slider.rangeMin", 0.0);
            descriptor.defaultProperties.set("slider.rangeMax", 1.0);
            descriptor.defaultProperties.set("slider.step", 0.0);
            descriptor.defaultProperties.set("value", 0.5);
            descriptor.defaultProperties.set("minValue", 0.25);
            descriptor.defaultProperties.set("maxValue", 0.75);

            {
                WidgetPropertySpec styleSpec;
                styleSpec.key = juce::Identifier("slider.style");
                styleSpec.label = "Slider Style";
                styleSpec.kind = WidgetPropertyKind::enumChoice;
                styleSpec.uiHint = WidgetPropertyUiHint::dropdown;
                styleSpec.group = "Style";
                styleSpec.order = 10;
                styleSpec.hint = "JUCE linear/range slider style (rotary uses Knob widget)";
                styleSpec.defaultValue = juce::var("linearHorizontal");
                styleSpec.enumOptions = {
                    { "linearHorizontal", "Linear Horizontal" },
                    { "linearVertical", "Linear Vertical" },
                    { "linearBar", "Linear Bar" },
                    { "linearBarVertical", "Linear Bar Vertical" },
                    { "incDecButtons", "Inc/Dec Buttons" },
                    { "twoValueHorizontal", "Two Value Horizontal" },
                    { "twoValueVertical", "Two Value Vertical" },
                    { "threeValueHorizontal", "Three Value Horizontal" },
                    { "threeValueVertical", "Three Value Vertical" }
                };
                descriptor.propertySpecs.push_back(std::move(styleSpec));

                WidgetPropertySpec rangeMinSpec;
                rangeMinSpec.key = juce::Identifier("slider.rangeMin");
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
                rangeMaxSpec.key = juce::Identifier("slider.rangeMax");
                rangeMaxSpec.label = "Range Max";
                rangeMaxSpec.kind = WidgetPropertyKind::number;
                rangeMaxSpec.uiHint = WidgetPropertyUiHint::spinBox;
                rangeMaxSpec.group = "Range";
                rangeMaxSpec.order = 20;
                rangeMaxSpec.hint = "Maximum range value";
                rangeMaxSpec.defaultValue = juce::var(1.0);
                rangeMaxSpec.decimals = 4;
                descriptor.propertySpecs.push_back(std::move(rangeMaxSpec));

                WidgetPropertySpec rangeStepSpec;
                rangeStepSpec.key = juce::Identifier("slider.step");
                rangeStepSpec.label = "Step";
                rangeStepSpec.kind = WidgetPropertyKind::number;
                rangeStepSpec.uiHint = WidgetPropertyUiHint::spinBox;
                rangeStepSpec.group = "Range";
                rangeStepSpec.order = 30;
                rangeStepSpec.hint = "0 means continuous";
                rangeStepSpec.defaultValue = juce::var(0.0);
                rangeStepSpec.minValue = 0.0;
                rangeStepSpec.decimals = 6;
                descriptor.propertySpecs.push_back(std::move(rangeStepSpec));

                WidgetPropertySpec valueSpec;
                valueSpec.key = juce::Identifier("value");
                valueSpec.label = "Value";
                valueSpec.kind = WidgetPropertyKind::number;
                valueSpec.uiHint = WidgetPropertyUiHint::spinBox;
                valueSpec.group = "Value";
                valueSpec.order = 10;
                valueSpec.hint = "Single-value and three-value center";
                valueSpec.defaultValue = juce::var(0.5);
                valueSpec.step = 0.01;
                valueSpec.decimals = 3;
                descriptor.propertySpecs.push_back(std::move(valueSpec));

                WidgetPropertySpec minValueSpec;
                minValueSpec.key = juce::Identifier("minValue");
                minValueSpec.label = "Min Value";
                minValueSpec.kind = WidgetPropertyKind::number;
                minValueSpec.uiHint = WidgetPropertyUiHint::spinBox;
                minValueSpec.group = "Value";
                minValueSpec.order = 20;
                minValueSpec.hint = "Range start (two/three-value style)";
                minValueSpec.defaultValue = juce::var(0.25);
                minValueSpec.decimals = 3;
                descriptor.propertySpecs.push_back(std::move(minValueSpec));

                WidgetPropertySpec maxValueSpec;
                maxValueSpec.key = juce::Identifier("maxValue");
                maxValueSpec.label = "Max Value";
                maxValueSpec.kind = WidgetPropertyKind::number;
                maxValueSpec.uiHint = WidgetPropertyUiHint::spinBox;
                maxValueSpec.group = "Value";
                maxValueSpec.order = 30;
                maxValueSpec.hint = "Range end (two/three-value style)";
                maxValueSpec.defaultValue = juce::var(0.75);
                maxValueSpec.decimals = 3;
                descriptor.propertySpecs.push_back(std::move(maxValueSpec));

                WidgetPropertySpec thumbImageSpec;
                thumbImageSpec.key = juce::Identifier("slider.thumbImage");
                thumbImageSpec.label = "Thumb Image";
                thumbImageSpec.kind = WidgetPropertyKind::assetRef;
                thumbImageSpec.uiHint = WidgetPropertyUiHint::assetPicker;
                thumbImageSpec.group = "Appearance";
                thumbImageSpec.order = 100;
                thumbImageSpec.hint = "Optional image asset id for thumb";
                thumbImageSpec.acceptedAssetKinds = { AssetKind::image };
                thumbImageSpec.advanced = true;
                descriptor.propertySpecs.push_back(std::move(thumbImageSpec));

                WidgetPropertySpec trackImageSpec;
                trackImageSpec.key = juce::Identifier("slider.trackImage");
                trackImageSpec.label = "Track Image";
                trackImageSpec.kind = WidgetPropertyKind::assetRef;
                trackImageSpec.uiHint = WidgetPropertyUiHint::assetPicker;
                trackImageSpec.group = "Appearance";
                trackImageSpec.order = 110;
                trackImageSpec.hint = "Optional image asset id for track";
                trackImageSpec.acceptedAssetKinds = { AssetKind::image };
                trackImageSpec.advanced = true;
                descriptor.propertySpecs.push_back(std::move(trackImageSpec));
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

                const auto style = widget.properties.getWithDefault("slider.style", juce::var("linearHorizontal")).toString().trim();
                const auto isTwoValueStyle = [](const juce::String& styleKey) noexcept
                {
                    return styleKey == "twoValueHorizontal" || styleKey == "twoValueVertical";
                };
                const auto isThreeValueStyle = [](const juce::String& styleKey) noexcept
                {
                    return styleKey == "threeValueHorizontal" || styleKey == "threeValueVertical";
                };
                const auto isVerticalStyle = [](const juce::String& styleKey) noexcept
                {
                    return styleKey == "linearVertical"
                        || styleKey == "linearBarVertical"
                        || styleKey == "twoValueVertical"
                        || styleKey == "threeValueVertical";
                };
                const auto isRotaryStyle = [](const juce::String& styleKey) noexcept
                {
                    return styleKey == "rotary"
                        || styleKey == "rotaryHorizontalDrag"
                        || styleKey == "rotaryVerticalDrag"
                        || styleKey == "rotaryHorizontalVerticalDrag";
                };
                const auto normalizeSliderStyle = [isRotaryStyle](juce::String styleKey) -> juce::String
                {
                    if (styleKey.isEmpty() || isRotaryStyle(styleKey))
                        return "linearHorizontal";
                    return styleKey;
                };
                const auto normalizedStyle = normalizeSliderStyle(style);

                auto rangeMin = static_cast<float>(readNumeric("slider.rangeMin", 0.0));
                auto rangeMax = static_cast<float>(readNumeric("slider.rangeMax", 1.0));
                if (!std::isfinite(rangeMin))
                    rangeMin = 0.0f;
                if (!std::isfinite(rangeMax))
                    rangeMax = rangeMin + 1.0f;
                if (rangeMax <= rangeMin)
                    rangeMax = rangeMin + 1.0f;

                const auto clampToRange = [rangeMin, rangeMax](float v) noexcept
                {
                    return juce::jlimit(rangeMin, rangeMax, std::isfinite(v) ? v : rangeMin);
                };

                auto value = clampToRange(static_cast<float>(readNumeric("value", (rangeMin + rangeMax) * 0.5)));
                auto minValue = clampToRange(static_cast<float>(readNumeric("minValue", rangeMin)));
                auto maxValue = clampToRange(static_cast<float>(readNumeric("maxValue", rangeMax)));
                if (minValue > maxValue)
                    std::swap(minValue, maxValue);
                value = juce::jlimit(minValue, maxValue, value);

                const auto normalize = [rangeMin, rangeMax](float v) noexcept
                {
                    const auto width = rangeMax - rangeMin;
                    if (width <= 0.000001f)
                        return 0.0f;
                    return juce::jlimit(0.0f, 1.0f, (v - rangeMin) / width);
                };

                const auto valueNorm = normalize(value);
                const auto minNorm = normalize(minValue);
                const auto maxNorm = normalize(maxValue);

                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                g.fillRoundedRectangle(body, 4.0f);

                const auto drawThumb = [&g](float x, float y, juce::Colour color, float diameter = 12.0f)
                {
                    g.setColour(color);
                    g.fillEllipse(x - diameter * 0.5f, y - diameter * 0.5f, diameter, diameter);
                };

                if (isVerticalStyle(normalizedStyle))
                {
                    const auto track = juce::Rectangle<float>(body.getCentreX() - 2.0f,
                                                              body.getY() + 10.0f,
                                                              4.0f,
                                                              std::max(8.0f, body.getHeight() - 20.0f));

                    g.setColour(juce::Colour::fromRGB(130, 136, 149));
                    g.fillRoundedRectangle(track, 2.0f);

                    const auto yFromNorm = [&track](float norm) noexcept
                    {
                        return track.getBottom() - track.getHeight() * norm;
                    };

                    if (isTwoValueStyle(normalizedStyle) || isThreeValueStyle(normalizedStyle))
                    {
                        const auto yMin = yFromNorm(minNorm);
                        const auto yMax = yFromNorm(maxNorm);
                        const auto top = std::min(yMin, yMax);
                        const auto height = std::max(2.0f, std::abs(yMax - yMin));

                        g.setColour(juce::Colour::fromRGB(95, 160, 255));
                        g.fillRoundedRectangle({ track.getX(), top, track.getWidth(), height }, 2.0f);

                        drawThumb(track.getCentreX(), yMin, juce::Colour::fromRGB(214, 220, 230));
                        drawThumb(track.getCentreX(), yMax, juce::Colour::fromRGB(214, 220, 230));

                        if (isThreeValueStyle(normalizedStyle))
                            drawThumb(track.getCentreX(), yFromNorm(valueNorm), juce::Colour::fromRGB(84, 212, 255), 10.0f);
                    }
                    else
                    {
                        const auto yValue = yFromNorm(valueNorm);
                        g.setColour(juce::Colour::fromRGB(95, 160, 255));
                        g.fillRoundedRectangle({ track.getX(), yValue, track.getWidth(), std::max(2.0f, track.getBottom() - yValue) }, 2.0f);
                        drawThumb(track.getCentreX(), yValue, juce::Colour::fromRGB(214, 220, 230));
                    }

                    return;
                }

                const auto track = juce::Rectangle<float>(body.getX() + 10.0f,
                                                          body.getCentreY() - 2.0f,
                                                          std::max(8.0f, body.getWidth() - 20.0f),
                                                          4.0f);
                g.setColour(juce::Colour::fromRGB(130, 136, 149));
                g.fillRoundedRectangle(track, 2.0f);

                const auto xFromNorm = [&track](float norm) noexcept
                {
                    return track.getX() + track.getWidth() * norm;
                };

                if (isTwoValueStyle(normalizedStyle) || isThreeValueStyle(normalizedStyle))
                {
                    const auto xMin = xFromNorm(minNorm);
                    const auto xMax = xFromNorm(maxNorm);
                    const auto left = std::min(xMin, xMax);
                    const auto width = std::max(2.0f, std::abs(xMax - xMin));

                    g.setColour(juce::Colour::fromRGB(95, 160, 255));
                    g.fillRoundedRectangle({ left, track.getY(), width, track.getHeight() }, 2.0f);

                    drawThumb(xMin, track.getCentreY(), juce::Colour::fromRGB(214, 220, 230));
                    drawThumb(xMax, track.getCentreY(), juce::Colour::fromRGB(214, 220, 230));

                    if (isThreeValueStyle(normalizedStyle))
                        drawThumb(xFromNorm(valueNorm), track.getCentreY(), juce::Colour::fromRGB(84, 212, 255), 10.0f);
                }
                else
                {
                    const auto thumbX = xFromNorm(valueNorm);
                    g.setColour(juce::Colour::fromRGB(95, 160, 255));
                    g.fillRoundedRectangle({ track.getX(), track.getY(), std::max(2.0f, thumbX - track.getX()), track.getHeight() }, 2.0f);
                    drawThumb(thumbX, track.getCentreY(), juce::Colour::fromRGB(214, 220, 230));
                }
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
                const auto clampRange = [](double minValue, double maxValue, double value) noexcept
                {
                    if (!std::isfinite(value))
                        return minValue;
                    return juce::jlimit(minValue, maxValue, value);
                };
                const auto styleToLiteral = [](const juce::String& styleKey) -> juce::String
                {
                    static const std::array<std::pair<const char*, const char*>, 9> mapping {
                        std::pair<const char*, const char*> { "linearHorizontal", "juce::Slider::LinearHorizontal" },
                        { "linearVertical", "juce::Slider::LinearVertical" },
                        { "linearBar", "juce::Slider::LinearBar" },
                        { "linearBarVertical", "juce::Slider::LinearBarVertical" },
                        { "incDecButtons", "juce::Slider::IncDecButtons" },
                        { "twoValueHorizontal", "juce::Slider::TwoValueHorizontal" },
                        { "twoValueVertical", "juce::Slider::TwoValueVertical" },
                        { "threeValueHorizontal", "juce::Slider::ThreeValueHorizontal" },
                        { "threeValueVertical", "juce::Slider::ThreeValueVertical" }
                    };

                    for (const auto& [key, literal] : mapping)
                    {
                        if (styleKey == key)
                            return literal;
                    }

                    return "juce::Slider::LinearHorizontal";
                };
                const auto isTwoValueStyle = [](const juce::String& styleKey) noexcept
                {
                    return styleKey == "twoValueHorizontal" || styleKey == "twoValueVertical";
                };
                const auto isThreeValueStyle = [](const juce::String& styleKey) noexcept
                {
                    return styleKey == "threeValueHorizontal" || styleKey == "threeValueVertical";
                };
                const auto isRotaryStyle = [](const juce::String& styleKey) noexcept
                {
                    return styleKey == "rotary"
                        || styleKey == "rotaryHorizontalDrag"
                        || styleKey == "rotaryVerticalDrag"
                        || styleKey == "rotaryHorizontalVerticalDrag";
                };

                auto rangeMin = readNumeric("slider.rangeMin", 0.0);
                auto rangeMax = readNumeric("slider.rangeMax", 1.0);
                if (!std::isfinite(rangeMin))
                    rangeMin = 0.0;
                if (!std::isfinite(rangeMax))
                    rangeMax = rangeMin + 1.0;
                if (rangeMax <= rangeMin)
                    rangeMax = rangeMin + 1.0;

                auto step = readNumeric("slider.step", 0.0);
                if (!std::isfinite(step) || step < 0.0)
                    step = 0.0;

                auto value = clampRange(rangeMin, rangeMax, readNumeric("value", (rangeMin + rangeMax) * 0.5));
                auto minValue = clampRange(rangeMin, rangeMax, readNumeric("minValue", rangeMin));
                auto maxValue = clampRange(rangeMin, rangeMax, readNumeric("maxValue", rangeMax));
                if (minValue > maxValue)
                    std::swap(minValue, maxValue);
                value = juce::jlimit(minValue, maxValue, value);

                auto styleKey = context.widget.properties.getWithDefault("slider.style", juce::var("linearHorizontal")).toString().trim();
                if (styleKey.isEmpty() || isRotaryStyle(styleKey))
                    styleKey = "linearHorizontal";

                out.memberType = "juce::Slider";
                out.codegenKind = "juce_slider_dynamic";
                out.constructorLines.clear();
                out.resizedLines.clear();

                const auto styleLiteral = styleToLiteral(styleKey);
                out.constructorLines.add("    " + context.memberName + ".setSliderStyle(" + styleLiteral + ");");
                out.constructorLines.add("    " + context.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
                out.constructorLines.add("    " + context.memberName + ".setRange(" + juce::String(rangeMin, 8)
                                         + ", " + juce::String(rangeMax, 8)
                                         + ", " + juce::String(step, 8) + ");");

                if (isThreeValueStyle(styleKey))
                {
                    out.constructorLines.add("    " + context.memberName + ".setMinAndMaxValues(" + juce::String(minValue, 8)
                                             + ", " + juce::String(maxValue, 8)
                                             + ", juce::dontSendNotification);");
                    out.constructorLines.add("    " + context.memberName + ".setValue(" + juce::String(value, 8)
                                             + ", juce::dontSendNotification);");
                }
                else if (isTwoValueStyle(styleKey))
                {
                    out.constructorLines.add("    " + context.memberName + ".setMinAndMaxValues(" + juce::String(minValue, 8)
                                             + ", " + juce::String(maxValue, 8)
                                             + ", juce::dontSendNotification);");
                }
                else
                {
                    out.constructorLines.add("    " + context.memberName + ".setValue(" + juce::String(value, 8)
                                             + ", juce::dontSendNotification);");
                }

                out.constructorLines.add("    addAndMakeVisible(" + context.memberName + ");");
                return juce::Result::ok();
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
                    { "Thumb Image", juce::Identifier("slider.thumbImage"), "Apply image to thumb" },
                    { "Track Image", juce::Identifier("slider.trackImage"), "Apply image to track" }
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

    GYEOL_WIDGET_AUTOREGISTER(SliderWidget);
}

