#pragma once

#include "Gyeol/Public/Types.h"
#include <functional>
#include <memory>
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
        juce::String exportTargetType;
        juce::Rectangle<float> defaultBounds;
        juce::Point<float> minSize { 18.0f, 18.0f };
        PropertyBag defaultProperties;
        WidgetPainter painter;
        ExportCodegen exportCodegen;

        HitTest hitTest;
        CursorProvider cursorProvider;
        InteractionHandlers interaction;
        DropOptionsProvider dropOptions;
        ApplyDrop applyDrop;
    };

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
