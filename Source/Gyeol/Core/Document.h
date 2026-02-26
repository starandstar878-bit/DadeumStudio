#pragma once

#include "Gyeol/Public/Types.h"
#include <memory>

namespace Gyeol::Core
{
    class Document
    {
    public:
        Document();
        explicit Document(std::shared_ptr<const DocumentModel> model);

        const DocumentModel& model() const noexcept;
        const WidgetModel* findWidget(const WidgetId& id) const noexcept;

        Document withWidgetAdded(WidgetType type,
                                 juce::Rectangle<float> bounds,
                                 const PropertyBag& properties = {}) const;
        Document withWidgetRemoved(const WidgetId& id) const;
        Document withWidgetMoved(const WidgetId& id, juce::Point<float> delta) const;

        static void syncNextWidgetIdAfterLoad(const DocumentModel& model) noexcept;

    private:
        static WidgetId createWidgetId();
        std::shared_ptr<const DocumentModel> modelState;
    };
}
