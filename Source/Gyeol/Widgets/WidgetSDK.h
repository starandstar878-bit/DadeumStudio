#pragma once

#include "Gyeol/Public/Types.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace Gyeol::Widgets
{
    struct AssetRef
    {
        juce::String assetId;
        juce::String displayName;
        juce::String mime;
    };

    struct DropOption
    {
        juce::String label;
        juce::Identifier propKey;
        juce::String hint;
    };

    enum class WidgetPropertyKind
    {
        text,
        integer,
        number,
        boolean,
        enumChoice,
        color,
        vec2,
        rect,
        assetRef
    };

    enum class WidgetPropertyUiHint
    {
        autoHint,
        lineEdit,
        multiLine,
        spinBox,
        slider,
        toggle,
        dropdown,
        segmented,
        colorPicker,
        vec2Editor,
        rectEditor,
        assetPicker
    };

    enum class ColorStorage
    {
        hexString,      // "#RRGGBB" / "#RRGGBBAA"
        rgbaObject255,  // { r,g,b,a } in 0..255
        rgbaObject01,   // { r,g,b,a } in 0..1
        hslaObject,     // { h,s,l,a } with h:0..360, s/l/a:0..1
        argbInt,        // 0xAARRGGBB
        token           // design token id/string
    };

    struct WidgetEnumOption
    {
        juce::String value;
        juce::String label;
    };

    struct WidgetPropertySpec
    {
        juce::Identifier key;
        juce::String label;
        WidgetPropertyKind kind = WidgetPropertyKind::text;
        WidgetPropertyUiHint uiHint = WidgetPropertyUiHint::autoHint;
        juce::String group = "Widget";
        int order = 0;
        juce::String hint;
        juce::var defaultValue;
        std::optional<double> minValue;
        std::optional<double> maxValue;
        std::optional<double> step;
        int decimals = 3;
        std::vector<WidgetEnumOption> enumOptions;
        std::vector<AssetKind> acceptedAssetKinds;
        ColorStorage colorStorage = ColorStorage::hexString;
        bool colorAllowAlpha = true;
        bool colorAllowHdr = false;
        std::optional<juce::Identifier> dependsOnKey;
        std::optional<juce::var> dependsOnValue;
        bool advanced = false;
        bool readOnly = false;
    };

    struct RuntimeEventSpec
    {
        juce::String key;
        juce::String displayLabel;
        juce::String description;
        bool continuous = false;
    };

    using DropOptionsProvider = std::function<std::vector<DropOption>(const WidgetModel&, const AssetRef&)>;
    using ApplyDrop = std::function<juce::Result(PropertyBag& patchOut,
                                                 const WidgetModel&,
                                                 const AssetRef&,
                                                 const DropOption&)>;

    enum class ConsumeEvent
    {
        no,
        yes
    };

    using CursorProvider = std::function<juce::MouseCursor(const WidgetModel&, juce::Point<float> local)>;
    using HitTest = std::function<bool(const WidgetModel&, juce::Point<float> local)>;

    struct InteractionHandlers
    {
        std::function<ConsumeEvent(const WidgetModel&, const juce::MouseEvent&, PropertyBag& patchPreview)> onMouseDown;
        std::function<ConsumeEvent(const WidgetModel&, const juce::MouseEvent&, PropertyBag& patchPreview)> onMouseDrag;
        std::function<ConsumeEvent(const WidgetModel&, const juce::MouseEvent&, PropertyBag& patchCommit)> onMouseUp;
    };

    using WidgetPainter = std::function<void(juce::Graphics&, const WidgetModel&, const juce::Rectangle<float>&)>;

    struct ExportCodegenContext
    {
        const WidgetModel& widget;
        juce::String memberName;
        juce::String typeKey;
        juce::String exportTargetType;
    };

    struct ExportCodegenOutput
    {
        juce::String memberType;
        juce::String codegenKind;
        juce::StringArray constructorLines;
        juce::StringArray resizedLines;
    };

    using ExportCodegen = std::function<juce::Result(const ExportCodegenContext&, ExportCodegenOutput&)>;

    struct WidgetDescriptor
    {
        WidgetType type = WidgetType::button;
        juce::String typeKey;
        juce::String displayName;
        juce::String category;
        juce::StringArray tags;
        juce::String iconKey;
        juce::String exportTargetType;
        juce::Rectangle<float> defaultBounds;
        juce::Point<float> minSize { 18.0f, 18.0f };
        PropertyBag defaultProperties;
        std::vector<WidgetPropertySpec> propertySpecs;
        std::vector<RuntimeEventSpec> runtimeEvents;
        WidgetPainter painter;
        ExportCodegen exportCodegen;

        HitTest hitTest;
        CursorProvider cursorProvider;
        InteractionHandlers interaction;
        DropOptionsProvider dropOptions;
        ApplyDrop applyDrop;
    };

    inline const WidgetPropertySpec* findPropertySpec(const std::vector<WidgetPropertySpec>& specs,
                                                      const juce::Identifier& key) noexcept
    {
        const auto it = std::find_if(specs.begin(),
                                     specs.end(),
                                     [&key](const WidgetPropertySpec& spec)
                                     {
                                         return spec.key == key;
                                     });
        return it == specs.end() ? nullptr : &(*it);
    }

    inline const WidgetPropertySpec* findPropertySpec(const WidgetDescriptor& descriptor,
                                                      const juce::Identifier& key) noexcept
    {
        return findPropertySpec(descriptor.propertySpecs, key);
    }

    inline bool isAssetKindAccepted(const WidgetPropertySpec& spec, AssetKind kind) noexcept
    {
        if (spec.acceptedAssetKinds.empty())
            return true;

        return std::find(spec.acceptedAssetKinds.begin(), spec.acceptedAssetKinds.end(), kind)
            != spec.acceptedAssetKinds.end();
    }

    class WidgetClass
    {
    public:
        virtual ~WidgetClass() = default;
        virtual WidgetDescriptor makeDescriptor() const = 0;
    };

    using WidgetClassFactory = std::function<std::unique_ptr<WidgetClass>()>;

    class WidgetClassCatalog
    {
    public:
        static void registerFactory(WidgetClassFactory factory)
        {
            factories().push_back(std::move(factory));
        }

        static const std::vector<WidgetClassFactory>& allFactories()
        {
            return factories();
        }

    private:
        static std::vector<WidgetClassFactory>& factories()
        {
            static std::vector<WidgetClassFactory> registered;
            return registered;
        }
    };

    template <typename WidgetClassType>
    class AutoWidgetClassRegistration
    {
    public:
        explicit AutoWidgetClassRegistration(const char* debugName = nullptr)
        {
            WidgetClassCatalog::registerFactory([]() -> std::unique_ptr<WidgetClass>
                                                {
                                                    return std::make_unique<WidgetClassType>();
                                                });

            #if JUCE_DEBUG
            if (debugName != nullptr && debugName[0] != '\0')
                DBG("[Gyeol][WidgetSDK] Auto-registered widget class: " << debugName);
            else
                DBG("[Gyeol][WidgetSDK] Auto-registered unnamed widget class.");
            #endif
        }
    };

    template <typename DescriptorVisitor>
    void forEachRegisteredDescriptor(DescriptorVisitor&& visitor)
    {
        for (const auto& widgetFactory : WidgetClassCatalog::allFactories())
        {
            if (!widgetFactory)
                continue;

            auto widgetClass = widgetFactory();
            if (widgetClass == nullptr)
                continue;

            visitor(widgetClass->makeDescriptor());
        }
    }
}

#define GYEOL_WIDGET_INTERNAL_JOIN2(a, b) a##b
#define GYEOL_WIDGET_INTERNAL_JOIN(a, b) GYEOL_WIDGET_INTERNAL_JOIN2(a, b)

#define GYEOL_WIDGET_AUTOREGISTER(WidgetClassType) \
    inline const ::Gyeol::Widgets::AutoWidgetClassRegistration<WidgetClassType> \
        GYEOL_WIDGET_INTERNAL_JOIN(gGyeolAutoRegister_, WidgetClassType) { #WidgetClassType }
