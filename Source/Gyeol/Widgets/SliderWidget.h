#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>
#include <cmath>

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
            descriptor.exportTargetType = "juce::Slider::LinearHorizontal";
            descriptor.defaultBounds = { 0.0f, 0.0f, 170.0f, 34.0f };
            descriptor.minSize = { 80.0f, 24.0f };
            descriptor.defaultProperties.set("value", 0.5);
            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                g.fillRoundedRectangle(body, 4.0f);
                const auto track = juce::Rectangle<float>(body.getX() + 10.0f,
                                                          body.getCentreY() - 2.0f,
                                                          std::max(8.0f, body.getWidth() - 20.0f),
                                                          4.0f);
                const auto rawValue = static_cast<float>(static_cast<double>(widget.properties.getWithDefault("value", juce::var(0.5))));
                const auto value = juce::jlimit(0.0f, 1.0f, std::isfinite(rawValue) ? rawValue : 0.5f);
                const auto thumbX = track.getX() + (track.getWidth() * value);

                g.setColour(juce::Colour::fromRGB(130, 136, 149));
                g.fillRoundedRectangle(track, 2.0f);
                g.setColour(juce::Colour::fromRGB(95, 160, 255));
                g.fillRoundedRectangle({ track.getX(), track.getY(), std::max(2.0f, thumbX - track.getX()), track.getHeight() }, 2.0f);
                g.setColour(juce::Colour::fromRGB(214, 220, 230));
                g.fillEllipse(thumbX - 6.0f, track.getCentreY() - 6.0f, 12.0f, 12.0f);
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
