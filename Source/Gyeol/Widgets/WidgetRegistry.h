#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/ButtonWidget.h"
#include "Gyeol/Widgets/KnobWidget.h"
#include "Gyeol/Widgets/LabelWidget.h"
#include "Gyeol/Widgets/MeterWidget.h"
#include "Gyeol/Widgets/SliderWidget.h"
#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <vector>

namespace Gyeol::Widgets
{
    struct ExportWidgetMapping
    {
        WidgetType type = WidgetType::button;
        juce::String typeKey;
        juce::String exportTargetType;
    };

    class WidgetRegistry
    {
    public:
        bool registerWidget(WidgetDescriptor descriptor)
        {
            if (descriptor.typeKey.isEmpty() || descriptor.displayName.isEmpty())
                return false;
            if (descriptor.defaultBounds.getWidth() <= 0.0f || descriptor.defaultBounds.getHeight() <= 0.0f)
                return false;
            if (descriptor.minSize.x <= 0.0f || descriptor.minSize.y <= 0.0f)
                return false;
            if (!descriptor.painter)
                return false;

            if (find(descriptor.type) != nullptr || findByKey(descriptor.typeKey) != nullptr)
                return false;

            descriptors.push_back(std::move(descriptor));
            return true;
        }

        bool registerWidgetClass(const WidgetClass& widgetClass)
        {
            return registerWidget(widgetClass.makeDescriptor());
        }

        const WidgetDescriptor* find(WidgetType type) const noexcept
        {
            const auto it = std::find_if(descriptors.begin(),
                                         descriptors.end(),
                                         [type](const WidgetDescriptor& descriptor)
                                         {
                                             return descriptor.type == type;
                                         });
            return it == descriptors.end() ? nullptr : &(*it);
        }

        const WidgetDescriptor* findByKey(const juce::String& typeKey) const noexcept
        {
            const auto it = std::find_if(descriptors.begin(),
                                         descriptors.end(),
                                         [&typeKey](const WidgetDescriptor& descriptor)
                                         {
                                             return descriptor.typeKey == typeKey;
                                         });
            return it == descriptors.end() ? nullptr : &(*it);
        }

        const std::vector<WidgetDescriptor>& all() const noexcept
        {
            return descriptors;
        }

        std::vector<ExportWidgetMapping> exportMappings() const
        {
            std::vector<ExportWidgetMapping> mappings;
            mappings.reserve(descriptors.size());
            for (const auto& descriptor : descriptors)
            {
                ExportWidgetMapping mapping;
                mapping.type = descriptor.type;
                mapping.typeKey = descriptor.typeKey;
                mapping.exportTargetType = descriptor.exportTargetType.isNotEmpty()
                                               ? descriptor.exportTargetType
                                               : descriptor.typeKey;
                mappings.push_back(std::move(mapping));
            }

            return mappings;
        }

    private:
        std::vector<WidgetDescriptor> descriptors;
    };

    class WidgetFactory
    {
    public:
        explicit WidgetFactory(const WidgetRegistry& registryIn) noexcept
            : registry(registryIn)
        {
        }

        const WidgetDescriptor* descriptorFor(WidgetType type) const noexcept
        {
            const auto* descriptor = registry.find(type);
            if (descriptor == nullptr)
                warnOnce(type, "descriptorFor");
            return descriptor;
        }

        WidgetId createWidget(DocumentHandle& document, WidgetType type, juce::Point<float> origin) const
        {
            const auto* descriptor = registry.find(type);
            if (descriptor == nullptr)
            {
                warnOnce(type, "createWidget");
                return 0;
            }

            const auto bounds = descriptor->defaultBounds.withPosition(origin);
            auto props = descriptor->defaultProperties;
            return document.addWidget(type, bounds, props);
        }

        juce::Point<float> minSizeFor(WidgetType type) const noexcept
        {
            if (const auto* descriptor = registry.find(type))
                return { std::max(1.0f, descriptor->minSize.x), std::max(1.0f, descriptor->minSize.y) };

            warnOnce(type, "minSizeFor");
            return { 18.0f, 18.0f };
        }

        juce::String exportTargetTypeFor(WidgetType type) const
        {
            if (const auto* descriptor = registry.find(type))
                return descriptor->exportTargetType.isNotEmpty() ? descriptor->exportTargetType : descriptor->typeKey;

            warnOnce(type, "exportTargetTypeFor");
            return {};
        }

        std::vector<ExportWidgetMapping> exportMappings() const
        {
            return registry.exportMappings();
        }

    private:
        static int typeOrdinal(WidgetType type) noexcept
        {
            return static_cast<int>(type);
        }

        static void warnOnce(WidgetType type, const char* context)
        {
            static std::array<std::vector<int>, 4> warnedByContext;

            int contextIndex = 0;
            if (std::strcmp(context, "descriptorFor") == 0)
                contextIndex = 0;
            else if (std::strcmp(context, "createWidget") == 0)
                contextIndex = 1;
            else if (std::strcmp(context, "minSizeFor") == 0)
                contextIndex = 2;
            else
                contextIndex = 3;

            const auto ordinal = typeOrdinal(type);
            auto& warned = warnedByContext[static_cast<size_t>(contextIndex)];
            if (std::find(warned.begin(), warned.end(), ordinal) != warned.end())
                return;

            warned.push_back(ordinal);
            DBG("[Gyeol] Unsupported widget descriptor in " + juce::String(context)
                + " (type ordinal=" + juce::String(ordinal) + ")");
        }

        const WidgetRegistry& registry;
    };

    inline WidgetRegistry makeDefaultWidgetRegistry()
    {
        WidgetRegistry registry;

        forEachRegisteredDescriptor([&registry](WidgetDescriptor descriptor)
        {
            const auto registered = registry.registerWidget(std::move(descriptor));
            if (!registered)
                DBG("[Gyeol] Widget registration skipped (duplicate or invalid descriptor).");
        });

        return registry;
    }
}
