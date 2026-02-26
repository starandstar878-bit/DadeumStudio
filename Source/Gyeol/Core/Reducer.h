#pragma once

#include "Gyeol/Public/Action.h"
#include "Gyeol/Public/Types.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Gyeol::Core::Reducer
{
    namespace detail
    {
        inline std::optional<size_t> findWidgetIndex(const DocumentModel& document, WidgetId id) noexcept
        {
            const auto it = std::find_if(document.widgets.begin(),
                                         document.widgets.end(),
                                         [id](const WidgetModel& widget)
                                         {
                                             return widget.id == id;
                                         });

            if (it == document.widgets.end())
                return std::nullopt;

            return static_cast<size_t>(std::distance(document.widgets.begin(), it));
        }

        inline WidgetId nextWidgetIdFromDocument(const DocumentModel& document) noexcept
        {
            WidgetId maxId = kRootId;
            for (const auto& widget : document.widgets)
                maxId = std::max(maxId, widget.id);

            return maxId < std::numeric_limits<WidgetId>::max() ? (maxId + 1) : std::numeric_limits<WidgetId>::max();
        }

        inline juce::Result validateAllIdsExist(const DocumentModel& document, const std::vector<WidgetId>& ids)
        {
            for (const auto id : ids)
            {
                if (!findWidgetIndex(document, id).has_value())
                    return juce::Result::fail("Action target id not found in document");
            }

            return juce::Result::ok();
        }

        inline juce::Result reorderRootLevelWidgets(DocumentModel& document,
                                                    const std::vector<WidgetId>& ids,
                                                    int insertIndex)
        {
            std::unordered_set<WidgetId> idsSet(ids.begin(), ids.end());

            std::vector<WidgetModel> moved;
            std::vector<WidgetModel> remaining;
            moved.reserve(ids.size());
            remaining.reserve(document.widgets.size());

            for (const auto& widget : document.widgets)
            {
                if (idsSet.count(widget.id) > 0)
                    moved.push_back(widget);
                else
                    remaining.push_back(widget);
            }

            if (moved.size() != ids.size())
                return juce::Result::fail("Reorder/Reparent target id not found in document");

            const auto clampedInsertIndex = insertIndex < 0
                                            ? static_cast<int>(remaining.size())
                                            : std::clamp(insertIndex, 0, static_cast<int>(remaining.size()));

            remaining.insert(remaining.begin() + clampedInsertIndex, moved.begin(), moved.end());
            document.widgets = std::move(remaining);
            return juce::Result::ok();
        }
    }

    inline juce::Result apply(DocumentModel& document,
                              const Action& action,
                              std::vector<WidgetId>* createdIdsOut = nullptr)
    {
        const auto validation = validateAction(action);
        if (validation.failed())
            return validation;

        return std::visit([&document, createdIdsOut](const auto& typedAction) -> juce::Result
                          {
                              using T = std::decay_t<decltype(typedAction)>;

                              if constexpr (std::is_same_v<T, CreateWidgetAction>)
                              {
                                  if (typedAction.parentId != kRootId)
                                      return juce::Result::fail("Hierarchy parenting is not supported yet");

                                  WidgetId newId = typedAction.forcedId.value_or(detail::nextWidgetIdFromDocument(document));
                                  if (newId <= kRootId)
                                      return juce::Result::fail("Unable to allocate a valid widget id");

                                  if (detail::findWidgetIndex(document, newId).has_value())
                                      return juce::Result::fail("CreateWidget id already exists");

                                  WidgetModel widget;
                                  widget.id = newId;
                                  widget.type = typedAction.type;
                                  widget.bounds = typedAction.bounds;
                                  widget.properties = typedAction.properties;

                                  const auto insertAt = typedAction.insertIndex < 0
                                                        ? static_cast<int>(document.widgets.size())
                                                        : std::clamp(typedAction.insertIndex,
                                                                     0,
                                                                     static_cast<int>(document.widgets.size()));
                                  document.widgets.insert(document.widgets.begin() + insertAt, std::move(widget));

                                  if (createdIdsOut != nullptr)
                                      createdIdsOut->push_back(newId);

                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, DeleteWidgetsAction>)
                              {
                                  const auto existence = detail::validateAllIdsExist(document, typedAction.ids);
                                  if (existence.failed())
                                      return existence;

                                  std::unordered_set<WidgetId> ids(typedAction.ids.begin(), typedAction.ids.end());
                                  document.widgets.erase(std::remove_if(document.widgets.begin(),
                                                                        document.widgets.end(),
                                                                        [&ids](const WidgetModel& widget)
                                                                        {
                                                                            return ids.count(widget.id) > 0;
                                                                        }),
                                                         document.widgets.end());
                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, SetWidgetPropsAction>)
                              {
                                  const auto existence = detail::validateAllIdsExist(document, typedAction.ids);
                                  if (existence.failed())
                                      return existence;

                                  for (const auto id : typedAction.ids)
                                  {
                                      const auto index = detail::findWidgetIndex(document, id);
                                      if (!index.has_value())
                                          return juce::Result::fail("SetWidgetProps target id not found");

                                      auto& widget = document.widgets[*index];
                                      if (typedAction.bounds.has_value())
                                          widget.bounds = *typedAction.bounds;

                                      for (int i = 0; i < typedAction.patch.size(); ++i)
                                      {
                                          const auto key = typedAction.patch.getName(i);
                                          widget.properties.set(key, typedAction.patch.getValueAt(i));
                                      }
                                  }

                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, ReparentWidgetsAction>)
                              {
                                  if (typedAction.parentId != kRootId)
                                      return juce::Result::fail("Hierarchy parenting is not supported yet");

                                  const auto existence = detail::validateAllIdsExist(document, typedAction.ids);
                                  if (existence.failed())
                                      return existence;

                                  return detail::reorderRootLevelWidgets(document,
                                                                         typedAction.ids,
                                                                         typedAction.insertIndex);
                              }

                              else
                              {
                                  if (typedAction.parentId != kRootId)
                                      return juce::Result::fail("Hierarchy ordering is not supported yet");

                                  const auto existence = detail::validateAllIdsExist(document, typedAction.ids);
                                  if (existence.failed())
                                      return existence;

                                  return detail::reorderRootLevelWidgets(document,
                                                                         typedAction.ids,
                                                                         typedAction.insertIndex);
                              }
                          },
                          action);
    }
}
