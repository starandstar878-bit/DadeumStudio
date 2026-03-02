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
        meter,
        toggle,
        comboBox,
        textInput
    };

    enum class NodeKind
    {
        widget,
        group,
        layer
    };

    struct NodeRef
    {
        NodeKind kind = NodeKind::widget;
        WidgetId id = kRootId;
    };

    enum class ParentKind
    {
        root,
        layer,
        group
    };

    struct ParentRef
    {
        ParentKind kind = ParentKind::root;
        WidgetId id = kRootId;
    };

    enum class AssetKind
    {
        image,
        font,
        colorPreset,
        file
    };

    enum class RuntimeActionKind
    {
        setRuntimeParam,
        adjustRuntimeParam,
        toggleRuntimeParam,
        setNodeProps,
        setNodeBounds
    };

    struct RuntimeActionModel
    {
        RuntimeActionKind kind = RuntimeActionKind::setRuntimeParam;

        // Runtime parameter actions.
        juce::String paramKey;
        juce::var value;
        double delta = 0.0;

        // Node/document patch actions.
        NodeRef target;
        std::optional<bool> visible;
        std::optional<bool> locked;
        std::optional<float> opacity;
        PropertyBag patch;

        // Bounds action target.
        WidgetId targetWidgetId = kRootId;
        juce::Rectangle<float> bounds;
    };

    struct RuntimeBindingModel
    {
        WidgetId id = kRootId;
        juce::String name;
        bool enabled = true;
        WidgetId sourceWidgetId = kRootId;
        juce::String eventKey;
        std::vector<RuntimeActionModel> actions;
    };

    enum class RuntimeParamValueType
    {
        number,
        boolean,
        string
    };

    struct RuntimeParamModel
    {
        juce::String key;
        RuntimeParamValueType type = RuntimeParamValueType::number;
        juce::var defaultValue;
        juce::String description;
        bool exposed = true;
    };

    struct PropertyBindingModel
    {
        WidgetId id = kRootId;
        juce::String name;
        bool enabled = true;
        WidgetId targetWidgetId = kRootId;
        juce::String targetProperty;
        juce::String expression;
    };

    // Compatibility aliases for existing editor/interaction code paths.
    using LayerNodeKind = NodeKind;
    using LayerNodeRef = NodeRef;

    struct WidgetModel
    {
        WidgetId id = 0;
        WidgetType type = WidgetType::button;
        juce::Rectangle<float> bounds;
        bool visible = true;
        bool locked = false;
        float opacity = 1.0f; // [0, 1]
        PropertyBag properties;
    };

    struct GroupModel
    {
        WidgetId id = 0;
        juce::String name;
        bool visible = true;
        bool locked = false;
        float opacity = 1.0f; // [0, 1]
        std::vector<WidgetId> memberWidgetIds;
        std::vector<WidgetId> memberGroupIds;
        std::optional<WidgetId> parentGroupId;
    };

    struct LayerModel
    {
        WidgetId id = 0;
        juce::String name;
        int order = 0; // back-to-front order in root
        bool visible = true;
        bool locked = false;
        std::vector<WidgetId> memberWidgetIds;
        std::vector<WidgetId> memberGroupIds;
    };

    struct AssetModel
    {
        WidgetId id = 0;
        juce::String name;
        AssetKind kind = AssetKind::file;
        juce::String refKey;
        juce::String relativePath;
        juce::String mimeType;
        PropertyBag meta;
    };

    struct SchemaVersion
    {
        int major = 0;
        int minor = 6;
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
        std::vector<LayerModel> layers;
        std::vector<AssetModel> assets;
        std::vector<RuntimeParamModel> runtimeParams;
        std::vector<PropertyBindingModel> propertyBindings;
        std::vector<RuntimeBindingModel> runtimeBindings;
    };

    inline juce::String assetKindToKey(AssetKind kind)
    {
        switch (kind)
        {
            case AssetKind::image: return "image";
            case AssetKind::font: return "font";
            case AssetKind::colorPreset: return "colorPreset";
            case AssetKind::file: return "file";
        }

        return {};
    }

    inline std::optional<AssetKind> assetKindFromKey(const juce::String& key)
    {
        const auto normalized = key.trim();
        if (normalized == "image") return AssetKind::image;
        if (normalized == "font") return AssetKind::font;
        if (normalized == "colorPreset") return AssetKind::colorPreset;
        if (normalized == "file") return AssetKind::file;
        return std::nullopt;
    }

    inline juce::String runtimeActionKindToKey(RuntimeActionKind kind)
    {
        switch (kind)
        {
            case RuntimeActionKind::setRuntimeParam: return "setRuntimeParam";
            case RuntimeActionKind::adjustRuntimeParam: return "adjustRuntimeParam";
            case RuntimeActionKind::toggleRuntimeParam: return "toggleRuntimeParam";
            case RuntimeActionKind::setNodeProps: return "setNodeProps";
            case RuntimeActionKind::setNodeBounds: return "setNodeBounds";
        }

        return {};
    }

    inline std::optional<RuntimeActionKind> runtimeActionKindFromKey(const juce::String& key)
    {
        const auto normalized = key.trim();
        if (normalized == "setRuntimeParam") return RuntimeActionKind::setRuntimeParam;
        if (normalized == "adjustRuntimeParam") return RuntimeActionKind::adjustRuntimeParam;
        if (normalized == "toggleRuntimeParam") return RuntimeActionKind::toggleRuntimeParam;
        if (normalized == "setNodeProps") return RuntimeActionKind::setNodeProps;
        if (normalized == "setNodeBounds") return RuntimeActionKind::setNodeBounds;
        return std::nullopt;
    }

    inline juce::String runtimeParamValueTypeToKey(RuntimeParamValueType type)
    {
        switch (type)
        {
            case RuntimeParamValueType::number: return "number";
            case RuntimeParamValueType::boolean: return "boolean";
            case RuntimeParamValueType::string: return "string";
        }

        return {};
    }

    inline std::optional<RuntimeParamValueType> runtimeParamValueTypeFromKey(const juce::String& key)
    {
        const auto normalized = key.trim();
        if (normalized == "number") return RuntimeParamValueType::number;
        if (normalized == "boolean") return RuntimeParamValueType::boolean;
        if (normalized == "string") return RuntimeParamValueType::string;
        return std::nullopt;
    }

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

    inline bool isVec2FVar(const juce::var& value) noexcept
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return false;

        const auto& props = object->getProperties();
        if (props.size() != 2)
            return false;

        static const juce::Identifier keys[] { "x", "y" };
        for (const auto& key : keys)
        {
            if (!props.contains(key))
                return false;
            if (!isNumericVar(props[key]))
                return false;
        }

        return true;
    }

    inline bool isRgbaVar(const juce::var& value) noexcept
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return false;

        const auto& props = object->getProperties();
        if (props.size() < 3 || props.size() > 4)
            return false;

        static const juce::Identifier required[] { "r", "g", "b" };
        for (const auto& key : required)
        {
            if (!props.contains(key))
                return false;
            if (!isNumericVar(props[key]))
                return false;
        }

        if (props.contains("a") && !isNumericVar(props["a"]))
            return false;

        return true;
    }

    inline bool isHslaVar(const juce::var& value) noexcept
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return false;

        const auto& props = object->getProperties();
        if (props.size() < 3 || props.size() > 4)
            return false;

        static const juce::Identifier required[] { "h", "s", "l" };
        for (const auto& key : required)
        {
            if (!props.contains(key))
                return false;
            if (!isNumericVar(props[key]))
                return false;
        }

        if (props.contains("a") && !isNumericVar(props["a"]))
            return false;

        return true;
    }

    inline bool isAllowedPropertyValue(const juce::var& value) noexcept
    {
        if (value.isVoid())
            return false;
        if (value.isBool() || value.isInt() || value.isInt64() || value.isDouble() || value.isString())
            return true;
        if (isVec2FVar(value))
            return true;
        if (isRectFVar(value))
            return true;
        if (isRgbaVar(value) || isHslaVar(value))
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
