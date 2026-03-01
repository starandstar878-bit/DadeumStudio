#include "Gyeol/Core/Document.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace
{
    std::atomic<Gyeol::WidgetId> nextWidgetId { 1 };

    Gyeol::WidgetId normalizeNextWidgetId(Gyeol::WidgetId requested) noexcept
    {
        return requested <= 0 ? 1 : requested;
    }

    bool isValidBounds(const juce::Rectangle<float>& bounds) noexcept
    {
        return std::isfinite(bounds.getX())
            && std::isfinite(bounds.getY())
            && std::isfinite(bounds.getWidth())
            && std::isfinite(bounds.getHeight())
            && bounds.getWidth() >= 0.0f
            && bounds.getHeight() >= 0.0f;
    }

    bool isFiniteDelta(const juce::Point<float>& delta) noexcept
    {
        return std::isfinite(delta.x) && std::isfinite(delta.y);
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
        if (!isValidBounds(bounds))
            return *this;

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
                                                          }) != state.widgets.end()
                                          || std::find_if(state.groups.begin(),
                                                          state.groups.end(),
                                                          [id](const GroupModel& group)
                                                          {
                                                              return group.id == id;
                                                          }) != state.groups.end();
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
        if (!isFiniteDelta(delta))
            return *this;

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
                                  {
                                      const auto moved = it->bounds.translated(delta.x, delta.y);
                                      if (isValidBounds(moved))
                                          it->bounds = moved;
                                  }
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
        for (const auto& group : model.groups)
            maxId = std::max(maxId, group.id);
        for (const auto& layer : model.layers)
            maxId = std::max(maxId, layer.id);

        WidgetId requestedNext = 1;
        if (maxId < std::numeric_limits<WidgetId>::max())
            requestedNext = maxId + 1;

        nextWidgetId.store(normalizeNextWidgetId(requestedNext), std::memory_order_relaxed);
    }
}
