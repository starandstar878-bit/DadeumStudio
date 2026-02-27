#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace Gyeol
{
    using WidgetId = std::int64_t;
    using PropertyBag = juce::NamedValueSet;
    constexpr WidgetId kRootId = 0;

    enum class WidgetType
    {
        button,
        slider,
        knob,
        label,
        meter
    };

    enum class LayerNodeKind
    {
        widget,
        group
    };

    struct LayerNodeRef
    {
        LayerNodeKind kind = LayerNodeKind::widget;
        WidgetId id = kRootId;
    };

    struct WidgetModel
    {
        WidgetId id = 0;
        WidgetType type = WidgetType::button;
        juce::Rectangle<float> bounds;
        PropertyBag properties;
    };

    struct GroupModel
    {
        WidgetId id = 0;
        juce::String name;
        std::vector<WidgetId> memberWidgetIds;
        std::optional<WidgetId> parentGroupId;
    };

    struct SchemaVersion
    {
        int major = 0;
        int minor = 2;
        int patch = 0;
    };

    inline SchemaVersion currentSchemaVersion() noexcept
    {
        return {};
    }

    inline int compareSchemaVersion(const SchemaVersion& lhs, const SchemaVersion& rhs) noexcept
    {
        if (lhs.major != rhs.major)
            return lhs.major < rhs.major ? -1 : 1;
        if (lhs.minor != rhs.minor)
            return lhs.minor < rhs.minor ? -1 : 1;
        if (lhs.patch != rhs.patch)
            return lhs.patch < rhs.patch ? -1 : 1;
        return 0;
    }

    struct DocumentModel
    {
        SchemaVersion schemaVersion = currentSchemaVersion();
        std::vector<WidgetModel> widgets;
        std::vector<GroupModel> groups;
    };

    struct EditorStateModel
    {
        std::vector<WidgetId> selection;
    };

    inline juce::String widgetIdToJsonString(WidgetId id)
    {
        return juce::String(id);
    }

    inline std::optional<WidgetId> widgetIdFromJsonString(const juce::String& value)
    {
        const auto trimmed = value.trim();
        if (!trimmed.containsOnly("0123456789"))
            return std::nullopt;

        const auto parsed = trimmed.getLargeIntValue();
        if (parsed < 0)
            return std::nullopt;

        return static_cast<WidgetId>(parsed);
    }

    inline bool isNumericVar(const juce::var& value) noexcept
    {
        return value.isInt() || value.isInt64() || value.isDouble();
    }

    inline bool isRectFVar(const juce::var& value) noexcept
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return false;

        const auto& props = object->getProperties();
        if (props.size() != 4)
            return false;

        static const juce::Identifier keys[] { "x", "y", "w", "h" };
        for (const auto& key : keys)
        {
            if (!props.contains(key))
                return false;
            if (!isNumericVar(props[key]))
                return false;
        }

        return true;
    }

    inline bool isAllowedPropertyValue(const juce::var& value) noexcept
    {
        if (value.isVoid())
            return false;
        if (value.isBool() || value.isInt() || value.isInt64() || value.isDouble() || value.isString())
            return true;
        if (isRectFVar(value))
            return true;

        return false;
    }

    inline juce::Result validatePropertyBag(const PropertyBag& bag)
    {
        for (int i = 0; i < bag.size(); ++i)
        {
            const auto name = bag.getName(i).toString();
            const auto& value = bag.getValueAt(i);

            // Bounds are first-class widget geometry and must not live in PropertyBag.
            if (name == "bounds")
                return juce::Result::fail("PropertyBag key 'bounds' is reserved");

            if (!isAllowedPropertyValue(value))
                return juce::Result::fail("Unsupported PropertyBag value type at key: " + name);

            if (value.isString()
                && name.containsIgnoreCase("path")
                && juce::File::isAbsolutePath(value.toString()))
            {
                return juce::Result::fail("Absolute path is not allowed in PropertyBag key: " + name);
            }
        }

        return juce::Result::ok();
    }
}
