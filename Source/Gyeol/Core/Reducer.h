#pragma once

#include "Gyeol/Public/Action.h"
#include "Gyeol/Public/Types.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <unordered_map>
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

        inline std::optional<size_t> findGroupIndex(const DocumentModel& document, WidgetId id) noexcept
        {
            const auto it = std::find_if(document.groups.begin(),
                                         document.groups.end(),
                                         [id](const GroupModel& group)
                                         {
                                             return group.id == id;
                                         });

            if (it == document.groups.end())
                return std::nullopt;

            return static_cast<size_t>(std::distance(document.groups.begin(), it));
        }

        inline GroupModel* findGroup(DocumentModel& document, WidgetId id) noexcept
        {
            const auto index = findGroupIndex(document, id);
            return index.has_value() ? &document.groups[*index] : nullptr;
        }

        inline const GroupModel* findGroup(const DocumentModel& document, WidgetId id) noexcept
        {
            const auto index = findGroupIndex(document, id);
            return index.has_value() ? &document.groups[*index] : nullptr;
        }

        inline WidgetId nextEntityIdFromDocument(const DocumentModel& document) noexcept
        {
            WidgetId maxId = kRootId;
            for (const auto& widget : document.widgets)
                maxId = std::max(maxId, widget.id);
            for (const auto& group : document.groups)
                maxId = std::max(maxId, group.id);

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

        inline juce::Result validateAllGroupIdsExist(const DocumentModel& document, const std::vector<WidgetId>& ids)
        {
            for (const auto id : ids)
            {
                if (!findGroupIndex(document, id).has_value())
                    return juce::Result::fail("Action group id not found in document");
            }

            return juce::Result::ok();
        }

        inline std::vector<WidgetId> childGroupIds(const DocumentModel& document, WidgetId parentGroupId)
        {
            std::vector<WidgetId> children;
            for (const auto& group : document.groups)
            {
                if (group.parentGroupId.has_value() && *group.parentGroupId == parentGroupId)
                    children.push_back(group.id);
            }
            return children;
        }

        inline std::optional<WidgetId> directOwnerGroupId(const DocumentModel& document, WidgetId widgetId) noexcept
        {
            for (const auto& group : document.groups)
            {
                if (std::find(group.memberWidgetIds.begin(), group.memberWidgetIds.end(), widgetId) != group.memberWidgetIds.end())
                    return group.id;
            }

            return std::nullopt;
        }

        inline void removeWidgetFromAllGroups(DocumentModel& document, WidgetId widgetId)
        {
            for (auto& group : document.groups)
            {
                group.memberWidgetIds.erase(std::remove(group.memberWidgetIds.begin(),
                                                        group.memberWidgetIds.end(),
                                                        widgetId),
                                            group.memberWidgetIds.end());
            }
        }

        inline void appendUniqueWidgetId(std::vector<WidgetId>& target, WidgetId id)
        {
            if (std::find(target.begin(), target.end(), id) == target.end())
                target.push_back(id);
        }

        inline void collectGroupWidgetMembersRecursive(const DocumentModel& document,
                                                       WidgetId groupId,
                                                       std::unordered_set<WidgetId>& outWidgets,
                                                       std::unordered_set<WidgetId>& visitedGroups)
        {
            if (!visitedGroups.insert(groupId).second)
                return;

            const auto* group = findGroup(document, groupId);
            if (group == nullptr)
                return;

            for (const auto widgetId : group->memberWidgetIds)
                outWidgets.insert(widgetId);

            for (const auto childId : childGroupIds(document, groupId))
                collectGroupWidgetMembersRecursive(document, childId, outWidgets, visitedGroups);
        }

        inline std::unordered_set<WidgetId> collectGroupWidgetMembersRecursive(const DocumentModel& document, WidgetId groupId)
        {
            std::unordered_set<WidgetId> widgets;
            std::unordered_set<WidgetId> visitedGroups;
            collectGroupWidgetMembersRecursive(document, groupId, widgets, visitedGroups);
            return widgets;
        }

        inline bool hasSelectedAncestor(const DocumentModel& document,
                                        WidgetId groupId,
                                        const std::unordered_set<WidgetId>& selectedGroupIds)
        {
            const auto* group = findGroup(document, groupId);
            if (group == nullptr)
                return false;

            auto parent = group->parentGroupId;
            while (parent.has_value())
            {
                if (selectedGroupIds.count(*parent) > 0)
                    return true;

                const auto* parentGroup = findGroup(document, *parent);
                if (parentGroup == nullptr)
                    break;
                parent = parentGroup->parentGroupId;
            }

            return false;
        }

        inline void removeGroupAndRelink(DocumentModel& document, WidgetId groupId)
        {
            const auto index = findGroupIndex(document, groupId);
            if (!index.has_value())
                return;

            const auto removedGroup = document.groups[*index];
            const auto parentId = removedGroup.parentGroupId;

            // Lift child groups one level up.
            for (auto& candidate : document.groups)
            {
                if (candidate.parentGroupId.has_value() && *candidate.parentGroupId == removedGroup.id)
                    candidate.parentGroupId = parentId;
            }

            // Lift direct widget members one level up if parent exists; otherwise keep ungrouped.
            if (parentId.has_value())
            {
                if (auto* parentGroup = findGroup(document, *parentId); parentGroup != nullptr)
                {
                    for (const auto widgetId : removedGroup.memberWidgetIds)
                        appendUniqueWidgetId(parentGroup->memberWidgetIds, widgetId);
                }
            }

            document.groups.erase(document.groups.begin() + static_cast<std::ptrdiff_t>(*index));
        }

        inline std::unordered_map<WidgetId, int> computeChildCounts(const DocumentModel& document)
        {
            std::unordered_map<WidgetId, int> childCounts;
            childCounts.reserve(document.groups.size());

            for (const auto& group : document.groups)
                childCounts[group.id] = 0;

            for (const auto& group : document.groups)
            {
                if (group.parentGroupId.has_value())
                {
                    auto it = childCounts.find(*group.parentGroupId);
                    if (it != childCounts.end())
                        ++it->second;
                }
            }

            return childCounts;
        }

        inline void pruneDegenerateGroups(DocumentModel& document)
        {
            bool changed = true;
            while (changed)
            {
                changed = false;
                const auto childCounts = computeChildCounts(document);

                for (const auto& group : document.groups)
                {
                    const auto childCount = childCounts.count(group.id) > 0 ? childCounts.at(group.id) : 0;

                    // Preserve old behavior for leaf groups: less than two direct members is not a stable group.
                    if (childCount == 0 && group.memberWidgetIds.size() < 2)
                    {
                        removeGroupAndRelink(document, group.id);
                        changed = true;
                        break;
                    }
                }
            }
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

        enum class SiblingKind
        {
            widget,
            group
        };

        struct SiblingRef
        {
            SiblingKind kind = SiblingKind::widget;
            WidgetId id = kRootId;
        };

        inline bool operator==(const SiblingRef& lhs, const SiblingRef& rhs) noexcept
        {
            return lhs.kind == rhs.kind && lhs.id == rhs.id;
        }

        using SiblingMap = std::unordered_map<WidgetId, std::vector<SiblingRef>>;

        inline std::unordered_map<WidgetId, int> widgetOrderIndexById(const DocumentModel& document)
        {
            std::unordered_map<WidgetId, int> order;
            order.reserve(document.widgets.size());
            for (size_t i = 0; i < document.widgets.size(); ++i)
                order[document.widgets[i].id] = static_cast<int>(i);
            return order;
        }

        inline std::unordered_map<WidgetId, WidgetId> directWidgetOwnerMap(const DocumentModel& document)
        {
            std::unordered_map<WidgetId, WidgetId> owners;
            owners.reserve(document.widgets.size());

            for (const auto& group : document.groups)
            {
                for (const auto widgetId : group.memberWidgetIds)
                    owners[widgetId] = group.id;
            }

            return owners;
        }

        inline std::unordered_map<WidgetId, WidgetId> groupParentMap(const DocumentModel& document)
        {
            std::unordered_map<WidgetId, WidgetId> parents;
            parents.reserve(document.groups.size());

            for (const auto& group : document.groups)
                parents[group.id] = group.parentGroupId.value_or(kRootId);

            return parents;
        }

        inline juce::Result validateGroupParentMap(const std::unordered_map<WidgetId, WidgetId>& parentByGroupId)
        {
            for (const auto& [groupId, parentId] : parentByGroupId)
            {
                if (groupId <= kRootId)
                    return juce::Result::fail("Group id must be > rootId");

                if (parentId == kRootId)
                    continue;

                if (parentId == groupId)
                    return juce::Result::fail("group.parentGroupId must not equal group.id");

                if (parentByGroupId.find(parentId) == parentByGroupId.end())
                    return juce::Result::fail("group.parentGroupId not found in group set");
            }

            for (const auto& [groupId, _] : parentByGroupId)
            {
                std::unordered_set<WidgetId> visited;
                auto cursor = groupId;
                while (cursor != kRootId)
                {
                    if (!visited.insert(cursor).second)
                        return juce::Result::fail("group hierarchy must not contain cycles");

                    const auto it = parentByGroupId.find(cursor);
                    if (it == parentByGroupId.end())
                        return juce::Result::fail("group hierarchy references missing parent");

                    cursor = it->second;
                }
            }

            return juce::Result::ok();
        }

        inline juce::Result validateParentGroupExists(const DocumentModel& document, WidgetId parentId)
        {
            if (parentId == kRootId)
                return juce::Result::ok();

            return findGroup(document, parentId) != nullptr
                       ? juce::Result::ok()
                       : juce::Result::fail("Action parent group id not found");
        }

        inline int computeGroupAnchorIndex(const DocumentModel& document,
                                           WidgetId groupId,
                                           const std::unordered_map<WidgetId, int>& widgetOrderById,
                                           std::unordered_map<WidgetId, int>& memo,
                                           std::unordered_set<WidgetId>& visiting)
        {
            if (const auto memoIt = memo.find(groupId); memoIt != memo.end())
                return memoIt->second;

            if (!visiting.insert(groupId).second)
                return std::numeric_limits<int>::max();

            int minIndex = std::numeric_limits<int>::max();
            const auto* group = findGroup(document, groupId);
            if (group != nullptr)
            {
                for (const auto widgetId : group->memberWidgetIds)
                {
                    if (const auto widgetIt = widgetOrderById.find(widgetId); widgetIt != widgetOrderById.end())
                        minIndex = std::min(minIndex, widgetIt->second);
                }

                for (const auto childId : childGroupIds(document, groupId))
                {
                    const auto childAnchor = computeGroupAnchorIndex(document,
                                                                     childId,
                                                                     widgetOrderById,
                                                                     memo,
                                                                     visiting);
                    minIndex = std::min(minIndex, childAnchor);
                }
            }

            visiting.erase(groupId);
            memo[groupId] = minIndex;
            return minIndex;
        }

        inline SiblingMap buildSiblingMap(const DocumentModel& document)
        {
            const auto widgetOrderById = widgetOrderIndexById(document);
            const auto ownersByWidget = directWidgetOwnerMap(document);

            std::unordered_map<WidgetId, int> groupAnchorMemo;
            groupAnchorMemo.reserve(document.groups.size());

            SiblingMap siblingsByParent;
            siblingsByParent.reserve(document.groups.size() + 1);

            auto buildForParent = [&](WidgetId parentId)
            {
                struct AnchoredSibling
                {
                    int anchor = std::numeric_limits<int>::max();
                    SiblingRef ref;
                };

                std::vector<AnchoredSibling> anchored;

                for (const auto& group : document.groups)
                {
                    const auto groupParent = group.parentGroupId.value_or(kRootId);
                    if (groupParent != parentId)
                        continue;

                    std::unordered_set<WidgetId> visiting;
                    const auto anchor = computeGroupAnchorIndex(document,
                                                                group.id,
                                                                widgetOrderById,
                                                                groupAnchorMemo,
                                                                visiting);
                    anchored.push_back({ anchor, { SiblingKind::group, group.id } });
                }

                for (const auto& widget : document.widgets)
                {
                    const auto widgetOwner = ownersByWidget.count(widget.id) > 0 ? ownersByWidget.at(widget.id) : kRootId;
                    if (widgetOwner != parentId)
                        continue;

                    const auto anchor = widgetOrderById.count(widget.id) > 0
                                            ? widgetOrderById.at(widget.id)
                                            : std::numeric_limits<int>::max();
                    anchored.push_back({ anchor, { SiblingKind::widget, widget.id } });
                }

                std::sort(anchored.begin(),
                          anchored.end(),
                          [](const AnchoredSibling& lhs, const AnchoredSibling& rhs)
                          {
                              if (lhs.anchor != rhs.anchor)
                                  return lhs.anchor < rhs.anchor;
                              if (lhs.ref.kind != rhs.ref.kind)
                                  return lhs.ref.kind < rhs.ref.kind;
                              return lhs.ref.id < rhs.ref.id;
                          });

                auto& siblings = siblingsByParent[parentId];
                siblings.clear();
                siblings.reserve(anchored.size());
                for (const auto& entry : anchored)
                    siblings.push_back(entry.ref);
            };

            buildForParent(kRootId);
            for (const auto& group : document.groups)
                buildForParent(group.id);

            return siblingsByParent;
        }

        inline std::vector<SiblingRef>* findSiblings(SiblingMap& siblingsByParent, WidgetId parentId)
        {
            const auto it = siblingsByParent.find(parentId);
            return it == siblingsByParent.end() ? nullptr : &it->second;
        }

        inline const std::vector<SiblingRef>* findSiblings(const SiblingMap& siblingsByParent, WidgetId parentId)
        {
            const auto it = siblingsByParent.find(parentId);
            return it == siblingsByParent.end() ? nullptr : &it->second;
        }

        inline std::optional<WidgetId> findParentForSibling(const SiblingMap& siblingsByParent, SiblingRef ref)
        {
            for (const auto& [parentId, siblings] : siblingsByParent)
            {
                if (std::find(siblings.begin(), siblings.end(), ref) != siblings.end())
                    return parentId;
            }

            return std::nullopt;
        }

        inline void eraseSiblingRefs(std::vector<SiblingRef>& siblings, const std::vector<SiblingRef>& refsToErase)
        {
            siblings.erase(std::remove_if(siblings.begin(),
                                          siblings.end(),
                                          [&refsToErase](const SiblingRef& candidate)
                                          {
                                              return std::find(refsToErase.begin(), refsToErase.end(), candidate) != refsToErase.end();
                                          }),
                           siblings.end());
        }

        inline void insertSiblingRefs(std::vector<SiblingRef>& siblings, const std::vector<SiblingRef>& refs, int insertIndex)
        {
            const auto clampedInsertIndex = insertIndex < 0
                                                ? static_cast<int>(siblings.size())
                                                : std::clamp(insertIndex, 0, static_cast<int>(siblings.size()));
            siblings.insert(siblings.begin() + clampedInsertIndex, refs.begin(), refs.end());
        }

        inline juce::Result rebuildFromSiblingMap(DocumentModel& document,
                                                  const SiblingMap& siblingsByParent,
                                                  const std::unordered_map<WidgetId, WidgetId>& parentByGroupId)
        {
            const auto parentMapValidation = validateGroupParentMap(parentByGroupId);
            if (parentMapValidation.failed())
                return parentMapValidation;

            std::unordered_map<WidgetId, WidgetModel> widgetById;
            widgetById.reserve(document.widgets.size());
            for (const auto& widget : document.widgets)
                widgetById.emplace(widget.id, widget);

            std::vector<WidgetId> emittedWidgetIds;
            emittedWidgetIds.reserve(document.widgets.size());
            std::unordered_set<WidgetId> emittedWidgetSet;
            emittedWidgetSet.reserve(document.widgets.size());
            std::unordered_set<WidgetId> activeGroupStack;
            activeGroupStack.reserve(document.groups.size());

            std::function<juce::Result(WidgetId)> emitParent = [&](WidgetId parentId) -> juce::Result
            {
                const auto* siblings = findSiblings(siblingsByParent, parentId);
                if (siblings == nullptr)
                    return juce::Result::ok();

                for (const auto& sibling : *siblings)
                {
                    if (sibling.kind == SiblingKind::widget)
                    {
                        if (widgetById.find(sibling.id) == widgetById.end())
                            return juce::Result::fail("Sibling map references missing widget");
                        if (!emittedWidgetSet.insert(sibling.id).second)
                            return juce::Result::fail("Widget appears multiple times in sibling map");

                        emittedWidgetIds.push_back(sibling.id);
                        continue;
                    }

                    if (parentByGroupId.find(sibling.id) == parentByGroupId.end())
                        return juce::Result::fail("Sibling map references missing group");
                    if (!activeGroupStack.insert(sibling.id).second)
                        return juce::Result::fail("Sibling map contains group cycle");

                    const auto childResult = emitParent(sibling.id);
                    activeGroupStack.erase(sibling.id);
                    if (childResult.failed())
                        return childResult;
                }

                return juce::Result::ok();
            };

            const auto emissionResult = emitParent(kRootId);
            if (emissionResult.failed())
                return emissionResult;

            if (emittedWidgetIds.size() != document.widgets.size())
                return juce::Result::fail("Sibling map did not emit all widgets");

            std::vector<WidgetModel> reorderedWidgets;
            reorderedWidgets.reserve(document.widgets.size());
            for (const auto widgetId : emittedWidgetIds)
                reorderedWidgets.push_back(widgetById.at(widgetId));
            document.widgets = std::move(reorderedWidgets);

            for (auto& group : document.groups)
            {
                const auto parentIt = parentByGroupId.find(group.id);
                if (parentIt == parentByGroupId.end())
                    return juce::Result::fail("Group parent map entry missing");

                group.parentGroupId = parentIt->second == kRootId
                                          ? std::optional<WidgetId> {}
                                          : std::optional<WidgetId> { parentIt->second };

                group.memberWidgetIds.clear();
                if (const auto* siblings = findSiblings(siblingsByParent, group.id); siblings != nullptr)
                {
                    for (const auto& sibling : *siblings)
                    {
                        if (sibling.kind == SiblingKind::widget)
                            group.memberWidgetIds.push_back(sibling.id);
                    }
                }
            }

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

                                  WidgetId newId = typedAction.forcedId.value_or(detail::nextEntityIdFromDocument(document));
                                  if (newId <= kRootId)
                                      return juce::Result::fail("Unable to allocate a valid widget id");

                                  if (detail::findWidgetIndex(document, newId).has_value()
                                      || detail::findGroupIndex(document, newId).has_value())
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

                                  for (auto& group : document.groups)
                                  {
                                      group.memberWidgetIds.erase(std::remove_if(group.memberWidgetIds.begin(),
                                                                                 group.memberWidgetIds.end(),
                                                                                 [&ids](WidgetId memberId)
                                                                                 {
                                                                                     return ids.count(memberId) > 0;
                                                                                 }),
                                                                 group.memberWidgetIds.end());
                                  }

                                  detail::pruneDegenerateGroups(document);
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

                              else if constexpr (std::is_same_v<T, SetWidgetsBoundsAction>)
                              {
                                  std::vector<WidgetId> ids;
                                  ids.reserve(typedAction.items.size());
                                  for (const auto& item : typedAction.items)
                                      ids.push_back(item.id);

                                  const auto existence = detail::validateAllIdsExist(document, ids);
                                  if (existence.failed())
                                      return existence;

                                  for (const auto& item : typedAction.items)
                                  {
                                      const auto index = detail::findWidgetIndex(document, item.id);
                                      if (!index.has_value())
                                          return juce::Result::fail("SetWidgetsBounds target id not found");

                                      document.widgets[*index].bounds = item.bounds;
                                  }

                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, GroupWidgetsAction>)
                              {
                                  if (!typedAction.widgetIds.empty())
                                  {
                                      const auto widgetExistence = detail::validateAllIdsExist(document, typedAction.widgetIds);
                                      if (widgetExistence.failed())
                                          return widgetExistence;
                                  }

                                  if (!typedAction.groupIds.empty())
                                  {
                                      const auto groupExistence = detail::validateAllGroupIdsExist(document, typedAction.groupIds);
                                      if (groupExistence.failed())
                                          return groupExistence;
                                  }

                                  std::unordered_set<WidgetId> selectedGroupSet(typedAction.groupIds.begin(),
                                                                                typedAction.groupIds.end());

                                  std::vector<WidgetId> normalizedGroupIds;
                                  normalizedGroupIds.reserve(typedAction.groupIds.size());
                                  for (const auto groupId : typedAction.groupIds)
                                  {
                                      if (!detail::hasSelectedAncestor(document, groupId, selectedGroupSet))
                                          normalizedGroupIds.push_back(groupId);
                                  }

                                  std::unordered_set<WidgetId> explicitWidgetSet(typedAction.widgetIds.begin(),
                                                                                 typedAction.widgetIds.end());

                                  for (const auto groupId : normalizedGroupIds)
                                  {
                                      const auto coveredWidgets = detail::collectGroupWidgetMembersRecursive(document, groupId);
                                      for (const auto coveredWidgetId : coveredWidgets)
                                          explicitWidgetSet.erase(coveredWidgetId);
                                  }

                                  const auto normalizedUnitCount = explicitWidgetSet.size() + normalizedGroupIds.size();
                                  const auto allowSingleGroupWrapper = explicitWidgetSet.empty()
                                                                    && normalizedGroupIds.size() == 1;
                                  if (normalizedUnitCount < 2 && !allowSingleGroupWrapper)
                                      return juce::Result::fail("Grouped selection must contain at least two units");

                                  bool commonParentInitialized = false;
                                  bool hasCommonParent = true;
                                  std::optional<WidgetId> commonParent;
                                  const auto feedParent = [&](std::optional<WidgetId> parentId)
                                  {
                                      if (!commonParentInitialized)
                                      {
                                          commonParentInitialized = true;
                                          commonParent = parentId;
                                          return;
                                      }

                                      if (commonParent != parentId)
                                          hasCommonParent = false;
                                  };

                                  for (const auto widgetId : explicitWidgetSet)
                                      feedParent(detail::directOwnerGroupId(document, widgetId));

                                  for (const auto groupId : normalizedGroupIds)
                                  {
                                      if (const auto* group = detail::findGroup(document, groupId); group != nullptr)
                                          feedParent(group->parentGroupId);
                                  }

                                  WidgetId groupId = typedAction.forcedGroupId.value_or(detail::nextEntityIdFromDocument(document));
                                  if (groupId <= kRootId)
                                      return juce::Result::fail("Unable to allocate a valid group id");
                                  if (detail::findWidgetIndex(document, groupId).has_value()
                                      || detail::findGroupIndex(document, groupId).has_value())
                                  {
                                      return juce::Result::fail("Group id already exists in document");
                                  }

                                  GroupModel group;
                                  group.id = groupId;
                                  group.name = typedAction.groupName.isNotEmpty() ? typedAction.groupName : juce::String("Group");
                                  if (hasCommonParent)
                                      group.parentGroupId = commonParent;

                                  group.memberWidgetIds.reserve(explicitWidgetSet.size());
                                  for (const auto& widget : document.widgets)
                                  {
                                      if (explicitWidgetSet.count(widget.id) > 0)
                                      {
                                          detail::removeWidgetFromAllGroups(document, widget.id);
                                          group.memberWidgetIds.push_back(widget.id);
                                      }
                                  }

                                  for (const auto selectedGroupId : normalizedGroupIds)
                                  {
                                      if (auto* selectedGroup = detail::findGroup(document, selectedGroupId); selectedGroup != nullptr)
                                          selectedGroup->parentGroupId = groupId;
                                  }

                                  document.groups.push_back(std::move(group));
                                  detail::pruneDegenerateGroups(document);
                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, UngroupWidgetsAction>)
                              {
                                  const auto existence = detail::validateAllGroupIdsExist(document, typedAction.groupIds);
                                  if (existence.failed())
                                      return existence;

                                  for (const auto groupId : typedAction.groupIds)
                                      detail::removeGroupAndRelink(document, groupId);

                                  detail::pruneDegenerateGroups(document);
                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, ReparentAction>)
                              {
                                  if (typedAction.refs.empty())
                                      return juce::Result::fail("ReparentAction requires non-empty refs");

                                  const auto nodeKind = typedAction.refs.front().kind;
                                  std::vector<WidgetId> ids;
                                  ids.reserve(typedAction.refs.size());
                                  for (const auto& ref : typedAction.refs)
                                      ids.push_back(ref.id);

                                  if (nodeKind == LayerNodeKind::widget)
                                  {
                                      const auto existence = detail::validateAllIdsExist(document, ids);
                                      if (existence.failed())
                                          return existence;

                                      const auto parentCheck = detail::validateParentGroupExists(document, typedAction.parentId);
                                      if (parentCheck.failed())
                                          return parentCheck;

                                      auto siblingsByParent = detail::buildSiblingMap(document);
                                      const auto parentByGroupId = detail::groupParentMap(document);

                                      std::unordered_set<WidgetId> moveSet(ids.begin(), ids.end());
                                      std::vector<detail::SiblingRef> movedRefs;
                                      movedRefs.reserve(ids.size());
                                      for (const auto& widget : document.widgets)
                                      {
                                          if (moveSet.count(widget.id) > 0)
                                              movedRefs.push_back({ detail::SiblingKind::widget, widget.id });
                                      }

                                      if (movedRefs.size() != ids.size())
                                          return juce::Result::fail("ReparentAction widget refs not found in z-order traversal");

                                      for (auto& [_, siblings] : siblingsByParent)
                                          detail::eraseSiblingRefs(siblings, movedRefs);

                                      auto* targetSiblings = detail::findSiblings(siblingsByParent, typedAction.parentId);
                                      if (targetSiblings == nullptr)
                                          targetSiblings = &siblingsByParent[typedAction.parentId];

                                      detail::insertSiblingRefs(*targetSiblings, movedRefs, typedAction.insertIndex);

                                      const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                      if (rebuild.failed())
                                          return rebuild;

                                      detail::pruneDegenerateGroups(document);
                                      return juce::Result::ok();
                                  }

                                  const auto existence = detail::validateAllGroupIdsExist(document, ids);
                                  if (existence.failed())
                                      return existence;

                                  const auto parentCheck = detail::validateParentGroupExists(document, typedAction.parentId);
                                  if (parentCheck.failed())
                                      return parentCheck;

                                  auto parentByGroupId = detail::groupParentMap(document);
                                  std::unordered_set<WidgetId> requestedSet(ids.begin(), ids.end());
                                  std::vector<WidgetId> normalizedGroupIds;
                                  normalizedGroupIds.reserve(ids.size());
                                  for (const auto groupId : ids)
                                  {
                                      auto parent = parentByGroupId[groupId];
                                      bool hasRequestedAncestor = false;
                                      while (parent != kRootId)
                                      {
                                          if (requestedSet.count(parent) > 0)
                                          {
                                              hasRequestedAncestor = true;
                                              break;
                                          }

                                          const auto it = parentByGroupId.find(parent);
                                          if (it == parentByGroupId.end())
                                              break;
                                          parent = it->second;
                                      }

                                      if (!hasRequestedAncestor)
                                          normalizedGroupIds.push_back(groupId);
                                  }

                                  if (normalizedGroupIds.empty())
                                      return juce::Result::fail("ReparentAction normalized group refs must not be empty");

                                  std::unordered_set<WidgetId> moveSet(normalizedGroupIds.begin(), normalizedGroupIds.end());

                                  auto cursor = typedAction.parentId;
                                  while (cursor != kRootId)
                                  {
                                      if (moveSet.count(cursor) > 0)
                                          return juce::Result::fail("ReparentAction would create hierarchy cycle");

                                      const auto it = parentByGroupId.find(cursor);
                                      if (it == parentByGroupId.end())
                                          return juce::Result::fail("ReparentAction target parent chain is broken");
                                      cursor = it->second;
                                  }

                                  auto siblingsByParent = detail::buildSiblingMap(document);
                                  std::vector<detail::SiblingRef> movedRefs;
                                  movedRefs.reserve(normalizedGroupIds.size());

                                  std::function<void(WidgetId)> collectMovedRefsInTreeOrder = [&](WidgetId parentId)
                                  {
                                      const auto* siblings = detail::findSiblings(siblingsByParent, parentId);
                                      if (siblings == nullptr)
                                          return;

                                      for (const auto& sibling : *siblings)
                                      {
                                          if (sibling.kind != detail::SiblingKind::group)
                                              continue;

                                          if (moveSet.count(sibling.id) > 0)
                                          {
                                              movedRefs.push_back(sibling);
                                          }
                                          else
                                          {
                                              collectMovedRefsInTreeOrder(sibling.id);
                                          }
                                      }
                                  };
                                  collectMovedRefsInTreeOrder(kRootId);

                                  if (movedRefs.size() != normalizedGroupIds.size())
                                      return juce::Result::fail("ReparentAction group refs not found in sibling map");

                                  for (auto& [_, siblings] : siblingsByParent)
                                      detail::eraseSiblingRefs(siblings, movedRefs);

                                  auto* targetSiblings = detail::findSiblings(siblingsByParent, typedAction.parentId);
                                  if (targetSiblings == nullptr)
                                      targetSiblings = &siblingsByParent[typedAction.parentId];
                                  detail::insertSiblingRefs(*targetSiblings, movedRefs, typedAction.insertIndex);

                                  for (const auto groupId : normalizedGroupIds)
                                      parentByGroupId[groupId] = typedAction.parentId;

                                  const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                  if (rebuild.failed())
                                      return rebuild;

                                  detail::pruneDegenerateGroups(document);
                                  return juce::Result::ok();
                              }

                              else // ReorderAction
                              {
                                  if (typedAction.refs.empty())
                                      return juce::Result::fail("ReorderAction requires non-empty refs");

                                  const auto nodeKind = typedAction.refs.front().kind;
                                  std::vector<WidgetId> ids;
                                  ids.reserve(typedAction.refs.size());
                                  for (const auto& ref : typedAction.refs)
                                      ids.push_back(ref.id);

                                  const auto parentCheck = detail::validateParentGroupExists(document, typedAction.parentId);
                                  if (parentCheck.failed())
                                      return parentCheck;

                                  if (nodeKind == LayerNodeKind::widget)
                                  {
                                      const auto existence = detail::validateAllIdsExist(document, ids);
                                      if (existence.failed())
                                          return existence;

                                      for (const auto widgetId : ids)
                                      {
                                          const auto ownerParent = detail::directOwnerGroupId(document, widgetId).value_or(kRootId);
                                          if (ownerParent != typedAction.parentId)
                                              return juce::Result::fail("ReorderAction widget refs must be direct children of parentId");
                                      }

                                      auto siblingsByParent = detail::buildSiblingMap(document);
                                      const auto parentByGroupId = detail::groupParentMap(document);

                                      std::unordered_set<WidgetId> moveSet(ids.begin(), ids.end());
                                      std::vector<detail::SiblingRef> movedRefs;
                                      movedRefs.reserve(ids.size());

                                      if (const auto* sourceSiblings = detail::findSiblings(siblingsByParent, typedAction.parentId);
                                          sourceSiblings != nullptr)
                                      {
                                          for (const auto& sibling : *sourceSiblings)
                                          {
                                              if (sibling.kind == detail::SiblingKind::widget
                                                  && moveSet.count(sibling.id) > 0)
                                              {
                                                  movedRefs.push_back(sibling);
                                              }
                                          }
                                      }

                                      if (movedRefs.size() != ids.size())
                                          return juce::Result::fail("ReorderAction widget refs not found under parentId");

                                      auto* sourceSiblings = detail::findSiblings(siblingsByParent, typedAction.parentId);
                                      if (sourceSiblings == nullptr)
                                          return juce::Result::fail("ReorderAction source parent not found in sibling map");

                                      detail::eraseSiblingRefs(*sourceSiblings, movedRefs);
                                      detail::insertSiblingRefs(*sourceSiblings, movedRefs, typedAction.insertIndex);

                                      const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                      if (rebuild.failed())
                                          return rebuild;

                                      detail::pruneDegenerateGroups(document);
                                      return juce::Result::ok();
                                  }

                                  const auto existence = detail::validateAllGroupIdsExist(document, ids);
                                  if (existence.failed())
                                      return existence;

                                  for (const auto groupId : ids)
                                  {
                                      const auto* group = detail::findGroup(document, groupId);
                                      if (group == nullptr)
                                          return juce::Result::fail("ReorderAction group ref not found");

                                      const auto ownerParent = group->parentGroupId.value_or(kRootId);
                                      if (ownerParent != typedAction.parentId)
                                          return juce::Result::fail("ReorderAction group refs must be direct children of parentId");
                                  }

                                  auto siblingsByParent = detail::buildSiblingMap(document);
                                  const auto parentByGroupId = detail::groupParentMap(document);

                                  std::unordered_set<WidgetId> moveSet(ids.begin(), ids.end());
                                  std::vector<detail::SiblingRef> movedRefs;
                                  movedRefs.reserve(ids.size());

                                  if (const auto* sourceSiblings = detail::findSiblings(siblingsByParent, typedAction.parentId);
                                      sourceSiblings != nullptr)
                                  {
                                      for (const auto& sibling : *sourceSiblings)
                                      {
                                          if (sibling.kind == detail::SiblingKind::group
                                              && moveSet.count(sibling.id) > 0)
                                          {
                                              movedRefs.push_back(sibling);
                                          }
                                      }
                                  }

                                  if (movedRefs.size() != ids.size())
                                      return juce::Result::fail("ReorderAction group refs not found under parentId");

                                  auto* sourceSiblings = detail::findSiblings(siblingsByParent, typedAction.parentId);
                                  if (sourceSiblings == nullptr)
                                      return juce::Result::fail("ReorderAction source parent not found in sibling map");

                                  detail::eraseSiblingRefs(*sourceSiblings, movedRefs);
                                  detail::insertSiblingRefs(*sourceSiblings, movedRefs, typedAction.insertIndex);

                                  const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                  if (rebuild.failed())
                                      return rebuild;

                                  detail::pruneDegenerateGroups(document);
                                  return juce::Result::ok();
                              }
                          },
                          action);
    }
}
