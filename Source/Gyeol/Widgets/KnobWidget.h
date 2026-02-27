#pragma once

#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>
#include <cmath>

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
            descriptor.exportTargetType = "juce::Slider::RotaryVerticalDrag";
            descriptor.defaultBounds = { 0.0f, 0.0f, 56.0f, 56.0f };
            descriptor.minSize = { 32.0f, 32.0f };
            descriptor.defaultProperties.set("value", 0.5);
            descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body)
            {
                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                const auto diameter = std::max(12.0f, std::min(body.getWidth(), body.getHeight()) - 6.0f);
                const auto knob = juce::Rectangle<float>(diameter, diameter).withCentre(body.getCentre());
                g.fillEllipse(knob);
                g.setColour(juce::Colour::fromRGB(214, 220, 230));

                const auto rawValue = static_cast<float>(static_cast<double>(widget.properties.getWithDefault("value", juce::var(0.5))));
                const auto value = juce::jlimit(0.0f, 1.0f, std::isfinite(rawValue) ? rawValue : 0.5f);
                const auto minAngle = -juce::MathConstants<float>::pi * 0.75f;
                const auto maxAngle = juce::MathConstants<float>::pi * 0.75f;
                const auto angle = juce::jmap<float>(value, 0.0f, 1.0f, minAngle, maxAngle);

                const auto c = knob.getCentre();
                const auto r = knob.getWidth() * 0.34f;
                g.drawLine(c.x, c.y, c.x + std::cos(angle) * r, c.y + std::sin(angle) * r, 2.0f);
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
