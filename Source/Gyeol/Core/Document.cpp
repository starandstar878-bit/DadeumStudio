#include "Gyeol/Core/Document.h"

#include <algorithm>
#include <atomic>
#include <limits>

namespace
{
    std::atomic<Gyeol::WidgetId> nextWidgetId { 1 };

    Gyeol::WidgetId normalizeNextWidgetId(Gyeol::WidgetId requested) noexcept
    {
        return requested <= 0 ? 1 : requested;
    }

    template <typename StateMutator>
    Gyeol::Core::Document cloneAndMutate(const Gyeol::DocumentModel& source, StateMutator&& mutate)
    {
        auto next = std::make_shared<Gyeol::DocumentModel>(source);
        mutate(*next);
        std::shared_ptr<const Gyeol::DocumentModel> immutable = next;
        return Gyeol::Core::Document(std::move(immutable));
    }
}

namespace Gyeol::Core
{
    Document::Document()
        : modelState(std::make_shared<const DocumentModel>())
    {
    }

    Document::Document(std::shared_ptr<const DocumentModel> model)
        : modelState(std::move(model))
    {
        if (modelState == nullptr)
            modelState = std::make_shared<const DocumentModel>();
    }

    const DocumentModel& Document::model() const noexcept
    {
        return *modelState;
    }

    const WidgetModel* Document::findWidget(const WidgetId& id) const noexcept
    {
        const auto it = std::find_if(modelState->widgets.begin(),
                                     modelState->widgets.end(),
                                     [&id](const WidgetModel& widget)
                                     {
                                         return widget.id == id;
                                     });

        if (it == modelState->widgets.end())
            return nullptr;

        return &(*it);
    }

    Document Document::withWidgetAdded(WidgetType type,
                                       juce::Rectangle<float> bounds,
                                       const PropertyBag& properties) const
    {
        const auto propertyValidation = validatePropertyBag(properties);
        if (propertyValidation.failed())
            return *this;

        return cloneAndMutate(model(),
                              [&](DocumentModel& state)
                              {
                                  const auto isDuplicateId = [&state](WidgetId id)
                                  {
                                      return std::find_if(state.widgets.begin(),
                                                          state.widgets.end(),
                                                          [id](const WidgetModel& widget)
                                                          {
                                                              return widget.id == id;
                                                          }) != state.widgets.end();
                                  };

                                  WidgetId newId = createWidgetId();
                                  while (newId <= kRootId || isDuplicateId(newId))
                                      newId = createWidgetId();

                                  WidgetModel widget;
                                  widget.id = newId;
                                  widget.type = type;
                                  widget.bounds = bounds;
                                  widget.properties = properties;
                                  state.widgets.push_back(std::move(widget));
                              });
    }

    Document Document::withWidgetRemoved(const WidgetId& id) const
    {
        return cloneAndMutate(model(),
                              [&](DocumentModel& state)
                              {
                                  state.widgets.erase(std::remove_if(state.widgets.begin(),
                                                                     state.widgets.end(),
                                                                     [&id](const WidgetModel& widget)
                                                                     {
                                                                         return widget.id == id;
                                                                     }),
                                                      state.widgets.end());
                              });
    }

    Document Document::withWidgetMoved(const WidgetId& id, juce::Point<float> delta) const
    {
        return cloneAndMutate(model(),
                              [&](DocumentModel& state)
                              {
                                  const auto it = std::find_if(state.widgets.begin(),
                                                               state.widgets.end(),
                                                               [&id](const WidgetModel& widget)
                                                               {
                                                                   return widget.id == id;
                                                               });

                                  if (it != state.widgets.end())
                                      it->bounds = it->bounds.translated(delta.x, delta.y);
                              });
    }

    WidgetId Document::createWidgetId()
    {
        return nextWidgetId.fetch_add(1, std::memory_order_relaxed);
    }

    void Document::syncNextWidgetIdAfterLoad(const DocumentModel& model) noexcept
    {
        WidgetId maxId = kRootId;
        for (const auto& widget : model.widgets)
            maxId = std::max(maxId, widget.id);

        WidgetId requestedNext = 1;
        if (maxId < std::numeric_limits<WidgetId>::max())
            requestedNext = maxId + 1;

        nextWidgetId.store(normalizeNextWidgetId(requestedNext), std::memory_order_relaxed);
    }
}
