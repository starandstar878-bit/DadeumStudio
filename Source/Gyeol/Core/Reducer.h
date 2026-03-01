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

        inline std::optional<size_t> findLayerIndex(const DocumentModel& document, WidgetId id) noexcept
        {
            const auto it = std::find_if(document.layers.begin(),
                                         document.layers.end(),
                                         [id](const LayerModel& layer)
                                         {
                                             return layer.id == id;
                                         });

            if (it == document.layers.end())
                return std::nullopt;

            return static_cast<size_t>(std::distance(document.layers.begin(), it));
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

        inline LayerModel* findLayer(DocumentModel& document, WidgetId id) noexcept
        {
            const auto index = findLayerIndex(document, id);
            return index.has_value() ? &document.layers[*index] : nullptr;
        }

        inline const LayerModel* findLayer(const DocumentModel& document, WidgetId id) noexcept
        {
            const auto index = findLayerIndex(document, id);
            return index.has_value() ? &document.layers[*index] : nullptr;
        }

        inline WidgetId nextEntityIdFromDocument(const DocumentModel& document) noexcept
        {
            WidgetId maxId = kRootId;
            for (const auto& widget : document.widgets)
                maxId = std::max(maxId, widget.id);
            for (const auto& group : document.groups)
                maxId = std::max(maxId, group.id);
            for (const auto& layer : document.layers)
                maxId = std::max(maxId, layer.id);

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

        inline void rebuildGroupMemberGroupIds(DocumentModel& document)
        {
            for (auto& group : document.groups)
                group.memberGroupIds.clear();

            for (const auto& childGroup : document.groups)
            {
                if (!childGroup.parentGroupId.has_value())
                    continue;

                if (auto* parentGroup = findGroup(document, *childGroup.parentGroupId); parentGroup != nullptr)
                {
                    if (std::find(parentGroup->memberGroupIds.begin(),
                                  parentGroup->memberGroupIds.end(),
                                  childGroup.id) == parentGroup->memberGroupIds.end())
                    {
                        parentGroup->memberGroupIds.push_back(childGroup.id);
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

        inline bool entityIdExists(const DocumentModel& document, WidgetId id) noexcept
        {
            if (id <= kRootId)
                return false;

            if (findWidgetIndex(document, id).has_value())
                return true;
            if (findGroupIndex(document, id).has_value())
                return true;

            return std::find_if(document.layers.begin(),
                                document.layers.end(),
                                [id](const LayerModel& layer)
                                {
                                    return layer.id == id;
                                }) != document.layers.end();
        }

        inline WidgetId allocateLayerId(const DocumentModel& document) noexcept
        {
            WidgetId candidate = nextEntityIdFromDocument(document);
            if (candidate <= kRootId)
                candidate = 1;

            while (entityIdExists(document, candidate))
            {
                if (candidate >= std::numeric_limits<WidgetId>::max())
                    return std::numeric_limits<WidgetId>::max();

                ++candidate;
            }

            return candidate;
        }

        inline LayerModel* resolveTargetLayer(DocumentModel& document, std::optional<WidgetId> preferredLayerId) noexcept
        {
            if (preferredLayerId.has_value())
            {
                if (auto* layer = findLayer(document, *preferredLayerId); layer != nullptr)
                    return layer;
            }

            if (document.layers.empty())
                return nullptr;

            auto it = std::max_element(document.layers.begin(),
                                       document.layers.end(),
                                       [](const LayerModel& lhs, const LayerModel& rhs)
                                       {
                                           if (lhs.order != rhs.order)
                                               return lhs.order < rhs.order;
                                           return lhs.id < rhs.id;
                                       });
            return it == document.layers.end() ? nullptr : &(*it);
        }

        inline std::unordered_map<WidgetId, WidgetId> directOwnerGroupByWidgetId(const DocumentModel& document)
        {
            std::unordered_map<WidgetId, WidgetId> ownerByWidgetId;
            ownerByWidgetId.reserve(document.widgets.size());

            for (const auto& group : document.groups)
            {
                for (const auto widgetId : group.memberWidgetIds)
                    ownerByWidgetId[widgetId] = group.id;
            }

            return ownerByWidgetId;
        }

        inline bool isGroupCoveredByLayer(const DocumentModel& document,
                                          WidgetId groupId,
                                          const std::unordered_set<WidgetId>& directLayerGroupIds) noexcept
        {
            WidgetId cursor = groupId;
            std::unordered_set<WidgetId> visited;
            while (cursor > kRootId && visited.insert(cursor).second)
            {
                if (directLayerGroupIds.count(cursor) > 0)
                    return true;

                const auto* group = findGroup(document, cursor);
                if (group == nullptr || !group->parentGroupId.has_value())
                    break;

                cursor = *group->parentGroupId;
            }

            return false;
        }

        inline bool isWidgetCoveredByLayer(const DocumentModel& document,
                                           WidgetId widgetId,
                                           const std::unordered_set<WidgetId>& directLayerWidgetIds,
                                           const std::unordered_set<WidgetId>& directLayerGroupIds,
                                           const std::unordered_map<WidgetId, WidgetId>& ownerByWidgetId) noexcept
        {
            if (directLayerWidgetIds.count(widgetId) > 0)
                return true;

            auto ownerIt = ownerByWidgetId.find(widgetId);
            if (ownerIt == ownerByWidgetId.end())
                return false;

            WidgetId cursor = ownerIt->second;
            std::unordered_set<WidgetId> visited;
            while (cursor > kRootId && visited.insert(cursor).second)
            {
                if (directLayerGroupIds.count(cursor) > 0)
                    return true;

                const auto* group = findGroup(document, cursor);
                if (group == nullptr || !group->parentGroupId.has_value())
                    break;

                cursor = *group->parentGroupId;
            }

            return false;
        }

        inline void ensureLayerCoverage(DocumentModel& document)
        {
            if (document.layers.empty())
            {
                LayerModel layer;
                layer.id = allocateLayerId(document);
                if (layer.id == std::numeric_limits<WidgetId>::max())
                    layer.id = 1;
                layer.name = "Layer 1";
                layer.order = 0;
                document.layers.push_back(std::move(layer));
            }

            std::vector<LayerModel*> orderedLayers;
            orderedLayers.reserve(document.layers.size());
            for (auto& layer : document.layers)
                orderedLayers.push_back(&layer);

            std::sort(orderedLayers.begin(),
                      orderedLayers.end(),
                      [](const LayerModel* lhs, const LayerModel* rhs)
                      {
                          if (lhs == nullptr || rhs == nullptr)
                              return lhs != nullptr;
                          if (lhs->order != rhs->order)
                              return lhs->order < rhs->order;
                          return lhs->id < rhs->id;
                      });

            for (size_t i = 0; i < orderedLayers.size(); ++i)
                orderedLayers[i]->order = static_cast<int>(i);

            std::unordered_set<WidgetId> validWidgetIds;
            validWidgetIds.reserve(document.widgets.size());
            for (const auto& widget : document.widgets)
                validWidgetIds.insert(widget.id);

            std::unordered_set<WidgetId> validGroupIds;
            validGroupIds.reserve(document.groups.size());
            for (const auto& group : document.groups)
                validGroupIds.insert(group.id);

            std::unordered_set<WidgetId> seenWidgets;
            std::unordered_set<WidgetId> seenGroups;
            seenWidgets.reserve(document.widgets.size());
            seenGroups.reserve(document.groups.size());

            for (auto* layer : orderedLayers)
            {
                if (layer == nullptr)
                    continue;

                std::vector<WidgetId> filteredWidgets;
                filteredWidgets.reserve(layer->memberWidgetIds.size());
                for (const auto widgetId : layer->memberWidgetIds)
                {
                    if (validWidgetIds.count(widgetId) == 0)
                        continue;
                    if (!seenWidgets.insert(widgetId).second)
                        continue;
                    filteredWidgets.push_back(widgetId);
                }
                layer->memberWidgetIds = std::move(filteredWidgets);

                std::vector<WidgetId> filteredGroups;
                filteredGroups.reserve(layer->memberGroupIds.size());
                for (const auto groupId : layer->memberGroupIds)
                {
                    if (validGroupIds.count(groupId) == 0)
                        continue;
                    if (!seenGroups.insert(groupId).second)
                        continue;
                    filteredGroups.push_back(groupId);
                }
                layer->memberGroupIds = std::move(filteredGroups);
            }

            auto& fallbackLayer = *orderedLayers.front();
            const auto ownerByWidgetId = directOwnerGroupByWidgetId(document);

            std::unordered_set<WidgetId> directLayerWidgetIds;
            std::unordered_set<WidgetId> directLayerGroupIds;
            directLayerWidgetIds.reserve(document.widgets.size());
            directLayerGroupIds.reserve(document.groups.size());
            for (const auto* layer : orderedLayers)
            {
                if (layer == nullptr)
                    continue;

                directLayerWidgetIds.insert(layer->memberWidgetIds.begin(), layer->memberWidgetIds.end());
                directLayerGroupIds.insert(layer->memberGroupIds.begin(), layer->memberGroupIds.end());
            }

            for (const auto& group : document.groups)
            {
                if (isGroupCoveredByLayer(document, group.id, directLayerGroupIds))
                    continue;

                appendUniqueWidgetId(fallbackLayer.memberGroupIds, group.id);
                directLayerGroupIds.insert(group.id);
            }

            for (const auto& widget : document.widgets)
            {
                if (isWidgetCoveredByLayer(document,
                                           widget.id,
                                           directLayerWidgetIds,
                                           directLayerGroupIds,
                                           ownerByWidgetId))
                {
                    continue;
                }

                appendUniqueWidgetId(fallbackLayer.memberWidgetIds, widget.id);
                directLayerWidgetIds.insert(widget.id);
            }
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

        inline juce::Result validateParentRefExists(const DocumentModel& document, const ParentRef& parent)
        {
            if (parent.kind == ParentKind::root)
                return parent.id == kRootId
                           ? juce::Result::ok()
                           : juce::Result::fail("Parent root must use rootId");

            if (parent.id <= kRootId)
                return juce::Result::fail("Parent id must be > rootId for non-root parent");

            if (parent.kind == ParentKind::group)
            {
                return findGroup(document, parent.id) != nullptr
                           ? juce::Result::ok()
                           : juce::Result::fail("Parent group was not found");
            }

            return findLayer(document, parent.id) != nullptr
                       ? juce::Result::ok()
                       : juce::Result::fail("Parent layer was not found");
        }

        inline WidgetId parentGroupIdFromRef(const ParentRef& parent) noexcept
        {
            return parent.kind == ParentKind::group ? parent.id : kRootId;
        }

        inline std::optional<WidgetId> targetLayerIdFromRef(const ParentRef& parent) noexcept
        {
            return parent.kind == ParentKind::layer
                       ? std::optional<WidgetId> { parent.id }
                       : std::optional<WidgetId> {};
        }

        inline void removeWidgetFromAllLayers(DocumentModel& document, WidgetId widgetId)
        {
            for (auto& layer : document.layers)
            {
                layer.memberWidgetIds.erase(std::remove(layer.memberWidgetIds.begin(),
                                                        layer.memberWidgetIds.end(),
                                                        widgetId),
                                            layer.memberWidgetIds.end());
            }
        }

        inline void removeGroupFromAllLayers(DocumentModel& document, WidgetId groupId)
        {
            for (auto& layer : document.layers)
            {
                layer.memberGroupIds.erase(std::remove(layer.memberGroupIds.begin(),
                                                       layer.memberGroupIds.end(),
                                                       groupId),
                                           layer.memberGroupIds.end());
            }
        }

        inline std::optional<WidgetId> directLayerForWidget(const DocumentModel& document, WidgetId widgetId) noexcept
        {
            for (const auto& layer : document.layers)
            {
                if (std::find(layer.memberWidgetIds.begin(), layer.memberWidgetIds.end(), widgetId) != layer.memberWidgetIds.end())
                    return layer.id;
            }

            return std::nullopt;
        }

        inline std::optional<WidgetId> directLayerForGroup(const DocumentModel& document, WidgetId groupId) noexcept
        {
            for (const auto& layer : document.layers)
            {
                if (std::find(layer.memberGroupIds.begin(), layer.memberGroupIds.end(), groupId) != layer.memberGroupIds.end())
                    return layer.id;
            }

            return std::nullopt;
        }

        inline void assignWidgetsToLayer(DocumentModel& document,
                                         const std::vector<WidgetId>& widgetIds,
                                         WidgetId layerId)
        {
            auto* targetLayer = findLayer(document, layerId);
            if (targetLayer == nullptr)
                return;

            for (const auto widgetId : widgetIds)
            {
                removeWidgetFromAllLayers(document, widgetId);
                appendUniqueWidgetId(targetLayer->memberWidgetIds, widgetId);
            }
        }

        inline void assignGroupsToLayer(DocumentModel& document,
                                        const std::vector<WidgetId>& groupIds,
                                        WidgetId layerId)
        {
            auto* targetLayer = findLayer(document, layerId);
            if (targetLayer == nullptr)
                return;

            for (const auto groupId : groupIds)
            {
                removeGroupFromAllLayers(document, groupId);
                appendUniqueWidgetId(targetLayer->memberGroupIds, groupId);
            }
        }

        inline void normalizeLayerOrder(DocumentModel& document)
        {
            std::sort(document.layers.begin(),
                      document.layers.end(),
                      [](const LayerModel& lhs, const LayerModel& rhs)
                      {
                          if (lhs.order != rhs.order)
                              return lhs.order < rhs.order;
                          return lhs.id < rhs.id;
                      });

            for (size_t i = 0; i < document.layers.size(); ++i)
                document.layers[i].order = static_cast<int>(i);
        }

        inline juce::Result reorderLayers(DocumentModel& document,
                                          const std::vector<WidgetId>& ids,
                                          int insertIndex)
        {
            normalizeLayerOrder(document);
            std::unordered_set<WidgetId> moveSet(ids.begin(), ids.end());

            std::vector<LayerModel> moved;
            std::vector<LayerModel> remaining;
            moved.reserve(ids.size());
            remaining.reserve(document.layers.size());

            for (const auto& layer : document.layers)
            {
                if (moveSet.count(layer.id) > 0)
                    moved.push_back(layer);
                else
                    remaining.push_back(layer);
            }

            if (moved.size() != ids.size())
                return juce::Result::fail("ReorderAction layer refs not found");

            const auto clampedInsertIndex = insertIndex < 0
                                            ? static_cast<int>(remaining.size())
                                            : std::clamp(insertIndex, 0, static_cast<int>(remaining.size()));
            remaining.insert(remaining.begin() + clampedInsertIndex, moved.begin(), moved.end());

            document.layers = std::move(remaining);
            for (size_t i = 0; i < document.layers.size(); ++i)
                document.layers[i].order = static_cast<int>(i);

            return juce::Result::ok();
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

        inline bool isRootSiblingOwnedByLayer(const DocumentModel& document,
                                              const SiblingRef& sibling,
                                              WidgetId layerId) noexcept
        {
            if (layerId <= kRootId)
                return false;

            if (sibling.kind == SiblingKind::widget)
            {
                if (directOwnerGroupId(document, sibling.id).value_or(kRootId) != kRootId)
                    return false;

                return directLayerForWidget(document, sibling.id).value_or(kRootId) == layerId;
            }

            const auto* group = findGroup(document, sibling.id);
            if (group == nullptr)
                return false;
            if (group->parentGroupId.value_or(kRootId) != kRootId)
                return false;

            return directLayerForGroup(document, sibling.id).value_or(kRootId) == layerId;
        }

        inline void insertLayerScopedRefsIntoRootSiblings(const DocumentModel& document,
                                                          std::vector<SiblingRef>& rootSiblings,
                                                          const std::vector<SiblingRef>& movedRefs,
                                                          WidgetId layerId,
                                                          int insertIndex)
        {
            std::vector<SiblingRef> layerScoped;
            layerScoped.reserve(rootSiblings.size() + movedRefs.size());
            for (const auto& sibling : rootSiblings)
            {
                if (isRootSiblingOwnedByLayer(document, sibling, layerId))
                    layerScoped.push_back(sibling);
            }

            eraseSiblingRefs(layerScoped, movedRefs);
            insertSiblingRefs(layerScoped, movedRefs, insertIndex);

            std::vector<SiblingRef> rebuilt;
            rebuilt.reserve(rootSiblings.size() + movedRefs.size());
            bool insertedLayerBlock = false;
            for (const auto& sibling : rootSiblings)
            {
                if (!isRootSiblingOwnedByLayer(document, sibling, layerId))
                {
                    rebuilt.push_back(sibling);
                    continue;
                }

                if (!insertedLayerBlock)
                {
                    rebuilt.insert(rebuilt.end(), layerScoped.begin(), layerScoped.end());
                    insertedLayerBlock = true;
                }
            }

            if (!insertedLayerBlock)
                rebuilt.insert(rebuilt.end(), layerScoped.begin(), layerScoped.end());

            rootSiblings = std::move(rebuilt);
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

        const auto result = std::visit([&document, createdIdsOut](const auto& typedAction) -> juce::Result
                                       {
                              using T = std::decay_t<decltype(typedAction)>;

                              if constexpr (std::is_same_v<T, CreateAction>)
                              {
                                  return std::visit([&](const auto& payload) -> juce::Result
                                  {
                                      using P = std::decay_t<decltype(payload)>;
                                      if constexpr (std::is_same_v<P, CreateWidgetPayload>)
                                      {
                                          const auto parentCheck = detail::validateParentRefExists(document, payload.parent);
                                          if (parentCheck.failed())
                                              return parentCheck;

                                          WidgetId newId = payload.forcedId.value_or(detail::nextEntityIdFromDocument(document));
                                          if (newId <= kRootId)
                                              return juce::Result::fail("Unable to allocate a valid widget id");

                                          if (detail::findWidgetIndex(document, newId).has_value()
                                              || detail::findGroupIndex(document, newId).has_value()
                                              || detail::findLayerIndex(document, newId).has_value())
                                          {
                                              return juce::Result::fail("CreateAction widget id already exists");
                                          }

                                          WidgetModel widget;
                                          widget.id = newId;
                                          widget.type = payload.type;
                                          widget.bounds = payload.bounds;
                                          widget.properties = payload.properties;

                                          const auto insertAt = payload.insertIndex < 0
                                                                ? static_cast<int>(document.widgets.size())
                                                                : std::clamp(payload.insertIndex,
                                                                             0,
                                                                             static_cast<int>(document.widgets.size()));
                                          document.widgets.insert(document.widgets.begin() + insertAt, std::move(widget));

                                          if (payload.parent.kind == ParentKind::group)
                                          {
                                              if (auto* parentGroup = detail::findGroup(document, payload.parent.id); parentGroup != nullptr)
                                                  detail::appendUniqueWidgetId(parentGroup->memberWidgetIds, newId);
                                              detail::removeWidgetFromAllLayers(document, newId);
                                          }
                                          else if (payload.parent.kind == ParentKind::layer)
                                          {
                                              detail::assignWidgetsToLayer(document, { newId }, payload.parent.id);
                                          }
                                          else
                                          {
                                              if (auto* targetLayer = detail::resolveTargetLayer(document, std::nullopt); targetLayer != nullptr)
                                                  detail::appendUniqueWidgetId(targetLayer->memberWidgetIds, newId);
                                          }

                                          if (createdIdsOut != nullptr)
                                              createdIdsOut->push_back(newId);

                                          return juce::Result::ok();
                                      }
                                      else if constexpr (std::is_same_v<P, CreateGroupPayload>)
                                      {
                                          const auto parentCheck = detail::validateParentRefExists(document, payload.parent);
                                          if (parentCheck.failed())
                                              return parentCheck;

                                          std::vector<WidgetId> widgetIds;
                                          std::vector<WidgetId> groupIds;
                                          widgetIds.reserve(payload.members.size());
                                          groupIds.reserve(payload.members.size());
                                          for (const auto& member : payload.members)
                                          {
                                              if (member.kind == NodeKind::widget)
                                                  widgetIds.push_back(member.id);
                                              else if (member.kind == NodeKind::group)
                                                  groupIds.push_back(member.id);
                                          }

                                          if (!widgetIds.empty())
                                          {
                                              const auto widgetExistence = detail::validateAllIdsExist(document, widgetIds);
                                              if (widgetExistence.failed())
                                                  return widgetExistence;
                                          }

                                          if (!groupIds.empty())
                                          {
                                              const auto groupExistence = detail::validateAllGroupIdsExist(document, groupIds);
                                              if (groupExistence.failed())
                                                  return groupExistence;
                                          }

                                          std::unordered_set<WidgetId> selectedGroupSet(groupIds.begin(),
                                                                                        groupIds.end());
                                          std::vector<WidgetId> normalizedGroupIds;
                                          normalizedGroupIds.reserve(groupIds.size());
                                          for (const auto groupId : groupIds)
                                          {
                                              if (!detail::hasSelectedAncestor(document, groupId, selectedGroupSet))
                                                  normalizedGroupIds.push_back(groupId);
                                          }

                                          std::unordered_set<WidgetId> explicitWidgetSet(widgetIds.begin(),
                                                                                         widgetIds.end());
                                          std::unordered_set<WidgetId> selectedGroupWidgetIds;

                                          for (const auto groupId : normalizedGroupIds)
                                          {
                                              const auto coveredWidgets = detail::collectGroupWidgetMembersRecursive(document, groupId);
                                              selectedGroupWidgetIds.insert(coveredWidgets.begin(), coveredWidgets.end());
                                              for (const auto coveredWidgetId : coveredWidgets)
                                                  explicitWidgetSet.erase(coveredWidgetId);
                                          }

                                          const auto normalizedUnitCount = explicitWidgetSet.size() + normalizedGroupIds.size();
                                          const auto allowSingleGroupWrapper = explicitWidgetSet.empty()
                                                                            && normalizedGroupIds.size() == 1;
                                          if (normalizedUnitCount < 2 && !allowSingleGroupWrapper)
                                              return juce::Result::fail("Grouped selection must contain at least two units");

                                          WidgetId groupId = payload.forcedId.value_or(detail::nextEntityIdFromDocument(document));
                                          if (groupId <= kRootId)
                                              return juce::Result::fail("Unable to allocate a valid group id");
                                          if (detail::findWidgetIndex(document, groupId).has_value()
                                              || detail::findGroupIndex(document, groupId).has_value()
                                              || detail::findLayerIndex(document, groupId).has_value())
                                          {
                                              return juce::Result::fail("Group id already exists in document");
                                          }

                                          GroupModel group;
                                          group.id = groupId;
                                          group.name = payload.name.isNotEmpty() ? payload.name : juce::String("Group");
                                          if (payload.parent.kind == ParentKind::group)
                                              group.parentGroupId = payload.parent.id;

                                          group.memberWidgetIds.reserve(explicitWidgetSet.size());
                                          for (const auto& widget : document.widgets)
                                          {
                                              if (explicitWidgetSet.count(widget.id) > 0)
                                              {
                                                  detail::removeWidgetFromAllGroups(document, widget.id);
                                                  detail::removeWidgetFromAllLayers(document, widget.id);
                                                  group.memberWidgetIds.push_back(widget.id);
                                              }
                                          }

                                          for (const auto selectedGroupId : normalizedGroupIds)
                                          {
                                              if (auto* selectedGroup = detail::findGroup(document, selectedGroupId); selectedGroup != nullptr)
                                              {
                                                  selectedGroup->parentGroupId = groupId;
                                                  detail::removeGroupFromAllLayers(document, selectedGroupId);
                                              }
                                          }
                                          for (const auto widgetId : selectedGroupWidgetIds)
                                              detail::removeWidgetFromAllLayers(document, widgetId);

                                          document.groups.push_back(std::move(group));

                                          if (payload.parent.kind == ParentKind::layer)
                                          {
                                              detail::assignGroupsToLayer(document, { groupId }, payload.parent.id);
                                          }
                                          else if (payload.parent.kind == ParentKind::root)
                                          {
                                              if (auto* targetLayer = detail::resolveTargetLayer(document, std::nullopt); targetLayer != nullptr)
                                                  detail::appendUniqueWidgetId(targetLayer->memberGroupIds, groupId);
                                          }

                                          detail::pruneDegenerateGroups(document);
                                          if (createdIdsOut != nullptr)
                                              createdIdsOut->push_back(groupId);
                                          return juce::Result::ok();
                                      }
                                      else
                                      {
                                          WidgetId layerId = payload.forcedId.value_or(detail::allocateLayerId(document));
                                          if (layerId <= kRootId)
                                              return juce::Result::fail("Unable to allocate a valid layer id");
                                          if (detail::findWidgetIndex(document, layerId).has_value()
                                              || detail::findGroupIndex(document, layerId).has_value()
                                              || detail::findLayerIndex(document, layerId).has_value())
                                          {
                                              return juce::Result::fail("Layer id already exists in document");
                                          }

                                          detail::normalizeLayerOrder(document);

                                          LayerModel layer;
                                          layer.id = layerId;
                                          layer.name = payload.name.isNotEmpty()
                                                           ? payload.name
                                                           : juce::String("Layer ")
                                                               + juce::String(static_cast<int>(document.layers.size() + 1));
                                          layer.visible = payload.visible;
                                          layer.locked = payload.locked;

                                          const auto insertAt = payload.insertIndex < 0
                                                                ? static_cast<int>(document.layers.size())
                                                                : std::clamp(payload.insertIndex,
                                                                             0,
                                                                             static_cast<int>(document.layers.size()));
                                          document.layers.insert(document.layers.begin() + insertAt, std::move(layer));
                                          detail::normalizeLayerOrder(document);

                                          if (createdIdsOut != nullptr)
                                              createdIdsOut->push_back(layerId);
                                          return juce::Result::ok();
                                      }
                                  }, typedAction.payload);
                              }

                              else if constexpr (std::is_same_v<T, DeleteAction>)
                              {
                                  if (typedAction.kind == NodeKind::widget)
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

                                      for (const auto id : typedAction.ids)
                                          detail::removeWidgetFromAllLayers(document, id);

                                      detail::pruneDegenerateGroups(document);
                                      return juce::Result::ok();
                                  }

                                  if (typedAction.kind == NodeKind::group)
                                  {
                                      const auto existence = detail::validateAllGroupIdsExist(document, typedAction.ids);
                                      if (existence.failed())
                                          return existence;

                                      std::unordered_set<WidgetId> selectedGroupSet(typedAction.ids.begin(),
                                                                                    typedAction.ids.end());
                                      std::vector<WidgetId> normalizedGroupIds;
                                      normalizedGroupIds.reserve(typedAction.ids.size());
                                      for (const auto groupId : typedAction.ids)
                                      {
                                          if (!detail::hasSelectedAncestor(document, groupId, selectedGroupSet))
                                              normalizedGroupIds.push_back(groupId);
                                      }

                                      for (const auto groupId : normalizedGroupIds)
                                      {
                                          detail::removeGroupFromAllLayers(document, groupId);
                                          detail::removeGroupAndRelink(document, groupId);
                                      }

                                      detail::pruneDegenerateGroups(document);
                                      return juce::Result::ok();
                                  }

                                  detail::normalizeLayerOrder(document);
                                  std::unordered_set<WidgetId> idsToDelete(typedAction.ids.begin(), typedAction.ids.end());
                                  for (const auto layerId : typedAction.ids)
                                  {
                                      if (!detail::findLayerIndex(document, layerId).has_value())
                                          return juce::Result::fail("DeleteAction layer id not found");
                                  }

                                  DeleteLayerPolicy layerPolicy;
                                  if (const auto* policy = std::get_if<DeleteLayerPolicy>(&typedAction.policy); policy != nullptr)
                                      layerPolicy = *policy;

                                  if (layerPolicy.forbidDeletingLastLayer && document.layers.size() <= idsToDelete.size())
                                      return juce::Result::fail("Cannot delete the last layer");

                                  std::vector<WidgetId> deleteOrder;
                                  deleteOrder.reserve(typedAction.ids.size());
                                  for (const auto& layer : document.layers)
                                  {
                                      if (idsToDelete.count(layer.id) > 0)
                                          deleteOrder.push_back(layer.id);
                                  }

                                  for (const auto deleteId : deleteOrder)
                                  {
                                      if (document.layers.size() <= 1 && layerPolicy.forbidDeletingLastLayer)
                                          return juce::Result::fail("Cannot delete the last layer");

                                      detail::normalizeLayerOrder(document);
                                      const auto deleteIndexOpt = detail::findLayerIndex(document, deleteId);
                                      if (!deleteIndexOpt.has_value())
                                          continue;

                                      const auto deleteIndex = static_cast<int>(*deleteIndexOpt);
                                      std::optional<WidgetId> targetLayerId;
                                      if (layerPolicy.targetLayerId.has_value()
                                          && *layerPolicy.targetLayerId != deleteId
                                          && idsToDelete.count(*layerPolicy.targetLayerId) == 0
                                          && detail::findLayerIndex(document, *layerPolicy.targetLayerId).has_value())
                                      {
                                          targetLayerId = layerPolicy.targetLayerId;
                                      }
                                      else
                                      {
                                          for (int i = deleteIndex - 1; i >= 0; --i)
                                          {
                                              const auto candidateId = document.layers[static_cast<size_t>(i)].id;
                                              if (idsToDelete.count(candidateId) == 0)
                                              {
                                                  targetLayerId = candidateId;
                                                  break;
                                              }
                                          }

                                          if (!targetLayerId.has_value())
                                          {
                                              for (size_t i = static_cast<size_t>(deleteIndex + 1); i < document.layers.size(); ++i)
                                              {
                                                  const auto candidateId = document.layers[i].id;
                                                  if (idsToDelete.count(candidateId) == 0)
                                                  {
                                                      targetLayerId = candidateId;
                                                      break;
                                                  }
                                              }
                                          }
                                      }

                                      if (!targetLayerId.has_value())
                                          return juce::Result::fail("DeleteAction layer requires migration target");

                                      const auto deletingLayer = document.layers[static_cast<size_t>(deleteIndex)];
                                      if (auto* targetLayer = detail::findLayer(document, *targetLayerId); targetLayer != nullptr)
                                      {
                                          for (const auto widgetId : deletingLayer.memberWidgetIds)
                                              detail::appendUniqueWidgetId(targetLayer->memberWidgetIds, widgetId);
                                          for (const auto groupId : deletingLayer.memberGroupIds)
                                              detail::appendUniqueWidgetId(targetLayer->memberGroupIds, groupId);
                                      }

                                      document.layers.erase(document.layers.begin() + deleteIndex);
                                      idsToDelete.erase(deleteId);
                                  }

                                  detail::normalizeLayerOrder(document);
                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, SetPropsAction>)
                              {
                                  if (typedAction.kind == NodeKind::widget)
                                  {
                                      const auto existence = detail::validateAllIdsExist(document, typedAction.ids);
                                      if (existence.failed())
                                          return existence;

                                      const auto patch = std::get<WidgetPropsPatch>(typedAction.patch);
                                      for (const auto id : typedAction.ids)
                                      {
                                          const auto index = detail::findWidgetIndex(document, id);
                                          if (!index.has_value())
                                              return juce::Result::fail("SetPropsAction widget id not found");

                                          auto& widget = document.widgets[*index];
                                          if (patch.visible.has_value())
                                              widget.visible = *patch.visible;
                                          if (patch.locked.has_value())
                                              widget.locked = *patch.locked;
                                          if (patch.opacity.has_value())
                                              widget.opacity = *patch.opacity;

                                          for (int i = 0; i < patch.patch.size(); ++i)
                                          {
                                              const auto key = patch.patch.getName(i);
                                              widget.properties.set(key, patch.patch.getValueAt(i));
                                          }
                                      }

                                      return juce::Result::ok();
                                  }

                                  if (typedAction.kind == NodeKind::group)
                                  {
                                      const auto existence = detail::validateAllGroupIdsExist(document, typedAction.ids);
                                      if (existence.failed())
                                          return existence;

                                      const auto patch = std::get<GroupPropsPatch>(typedAction.patch);
                                      for (const auto id : typedAction.ids)
                                      {
                                          auto* group = detail::findGroup(document, id);
                                          if (group == nullptr)
                                              return juce::Result::fail("SetPropsAction group id not found");

                                          if (patch.name.has_value())
                                              group->name = *patch.name;
                                          if (patch.visible.has_value())
                                              group->visible = *patch.visible;
                                          if (patch.locked.has_value())
                                              group->locked = *patch.locked;
                                          if (patch.opacity.has_value())
                                              group->opacity = *patch.opacity;
                                      }

                                      return juce::Result::ok();
                                  }

                                  for (const auto id : typedAction.ids)
                                  {
                                      if (!detail::findLayerIndex(document, id).has_value())
                                          return juce::Result::fail("SetPropsAction layer id not found");
                                  }

                                  const auto patch = std::get<LayerPropsPatch>(typedAction.patch);
                                  for (const auto id : typedAction.ids)
                                  {
                                      auto* layer = detail::findLayer(document, id);
                                      if (layer == nullptr)
                                          return juce::Result::fail("SetPropsAction layer id not found");

                                      if (patch.name.has_value())
                                          layer->name = *patch.name;
                                      if (patch.visible.has_value())
                                          layer->visible = *patch.visible;
                                      if (patch.locked.has_value())
                                          layer->locked = *patch.locked;
                                  }

                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, SetBoundsAction>)
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
                                          return juce::Result::fail("SetBoundsAction target id not found");

                                      document.widgets[*index].bounds = item.bounds;
                                  }

                                  return juce::Result::ok();
                              }

                              else if constexpr (std::is_same_v<T, ReparentAction>)
                              {
                                  if (typedAction.refs.empty())
                                      return juce::Result::fail("ReparentAction requires non-empty refs");

                                  const auto parentCheck = detail::validateParentRefExists(document, typedAction.parent);
                                  if (parentCheck.failed())
                                      return parentCheck;

                                  const auto nodeKind = typedAction.refs.front().kind;
                                  std::vector<WidgetId> ids;
                                  ids.reserve(typedAction.refs.size());
                                  for (const auto& ref : typedAction.refs)
                                      ids.push_back(ref.id);

                                  const auto targetParentGroupId = detail::parentGroupIdFromRef(typedAction.parent);
                                  const auto targetLayerId = detail::targetLayerIdFromRef(typedAction.parent);
                                  const auto groupParentCheck = detail::validateParentGroupExists(document, targetParentGroupId);
                                  if (groupParentCheck.failed())
                                      return groupParentCheck;

                                  if (nodeKind == NodeKind::widget)
                                  {
                                      const auto existence = detail::validateAllIdsExist(document, ids);
                                      if (existence.failed())
                                          return existence;

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

                                      if (targetLayerId.has_value() && targetParentGroupId == kRootId)
                                      {
                                          auto* rootSiblings = detail::findSiblings(siblingsByParent, kRootId);
                                          if (rootSiblings == nullptr)
                                              rootSiblings = &siblingsByParent[kRootId];

                                          detail::insertLayerScopedRefsIntoRootSiblings(document,
                                                                                        *rootSiblings,
                                                                                        movedRefs,
                                                                                        *targetLayerId,
                                                                                        typedAction.insertIndex);
                                      }
                                      else
                                      {
                                          auto* targetSiblings = detail::findSiblings(siblingsByParent, targetParentGroupId);
                                          if (targetSiblings == nullptr)
                                              targetSiblings = &siblingsByParent[targetParentGroupId];

                                          detail::insertSiblingRefs(*targetSiblings, movedRefs, typedAction.insertIndex);
                                      }

                                      const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                      if (rebuild.failed())
                                          return rebuild;

                                      if (targetLayerId.has_value())
                                          detail::assignWidgetsToLayer(document, ids, *targetLayerId);
                                      else if (typedAction.parent.kind == ParentKind::group)
                                      {
                                          for (const auto id : ids)
                                              detail::removeWidgetFromAllLayers(document, id);
                                      }

                                      return juce::Result::ok();
                                  }

                                  const auto existence = detail::validateAllGroupIdsExist(document, ids);
                                  if (existence.failed())
                                      return existence;

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

                                  auto cursor = targetParentGroupId;
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

                                  if (targetLayerId.has_value() && targetParentGroupId == kRootId)
                                  {
                                      auto* rootSiblings = detail::findSiblings(siblingsByParent, kRootId);
                                      if (rootSiblings == nullptr)
                                          rootSiblings = &siblingsByParent[kRootId];

                                      detail::insertLayerScopedRefsIntoRootSiblings(document,
                                                                                    *rootSiblings,
                                                                                    movedRefs,
                                                                                    *targetLayerId,
                                                                                    typedAction.insertIndex);
                                  }
                                  else
                                  {
                                      auto* targetSiblings = detail::findSiblings(siblingsByParent, targetParentGroupId);
                                      if (targetSiblings == nullptr)
                                          targetSiblings = &siblingsByParent[targetParentGroupId];
                                      detail::insertSiblingRefs(*targetSiblings, movedRefs, typedAction.insertIndex);
                                  }

                                  for (const auto groupId : normalizedGroupIds)
                                      parentByGroupId[groupId] = targetParentGroupId;

                                  const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                  if (rebuild.failed())
                                      return rebuild;

                                  if (targetLayerId.has_value())
                                      detail::assignGroupsToLayer(document, normalizedGroupIds, *targetLayerId);
                                  else if (typedAction.parent.kind == ParentKind::group)
                                  {
                                      for (const auto groupId : normalizedGroupIds)
                                          detail::removeGroupFromAllLayers(document, groupId);
                                  }

                                  return juce::Result::ok();
                              }

                              else // ReorderAction
                              {
                                  if (typedAction.refs.empty())
                                      return juce::Result::fail("ReorderAction requires non-empty refs");

                                  const auto parentCheck = detail::validateParentRefExists(document, typedAction.parent);
                                  if (parentCheck.failed())
                                      return parentCheck;

                                  const auto nodeKind = typedAction.refs.front().kind;
                                  std::vector<WidgetId> ids;
                                  ids.reserve(typedAction.refs.size());
                                  for (const auto& ref : typedAction.refs)
                                      ids.push_back(ref.id);

                                  if (nodeKind == NodeKind::layer)
                                      return detail::reorderLayers(document, ids, typedAction.insertIndex);

                                  const auto targetParentGroupId = detail::parentGroupIdFromRef(typedAction.parent);
                                  const auto targetLayerId = detail::targetLayerIdFromRef(typedAction.parent);
                                  const auto groupParentCheck = detail::validateParentGroupExists(document, targetParentGroupId);
                                  if (groupParentCheck.failed())
                                      return groupParentCheck;

                                  if (nodeKind == NodeKind::widget)
                                  {
                                      const auto existence = detail::validateAllIdsExist(document, ids);
                                      if (existence.failed())
                                          return existence;

                                      for (const auto widgetId : ids)
                                      {
                                          const auto ownerParent = detail::directOwnerGroupId(document, widgetId).value_or(kRootId);
                                          if (ownerParent != targetParentGroupId)
                                              return juce::Result::fail("ReorderAction widget refs must be direct children of parentId");

                                          if (targetLayerId.has_value())
                                          {
                                              if (ownerParent != kRootId)
                                                  return juce::Result::fail("Layer parent requires root-level widgets");
                                              if (detail::directLayerForWidget(document, widgetId).value_or(kRootId) != *targetLayerId)
                                                  return juce::Result::fail("ReorderAction widget refs must belong to target layer");
                                          }
                                      }

                                      auto siblingsByParent = detail::buildSiblingMap(document);
                                      const auto parentByGroupId = detail::groupParentMap(document);

                                      std::unordered_set<WidgetId> moveSet(ids.begin(), ids.end());
                                      std::vector<detail::SiblingRef> movedRefs;
                                      movedRefs.reserve(ids.size());

                                      if (const auto* sourceSiblings = detail::findSiblings(siblingsByParent, targetParentGroupId);
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

                                      auto* sourceSiblings = detail::findSiblings(siblingsByParent, targetParentGroupId);
                                      if (sourceSiblings == nullptr)
                                          return juce::Result::fail("ReorderAction source parent not found in sibling map");

                                      detail::eraseSiblingRefs(*sourceSiblings, movedRefs);
                                      if (targetLayerId.has_value() && targetParentGroupId == kRootId)
                                      {
                                          detail::insertLayerScopedRefsIntoRootSiblings(document,
                                                                                        *sourceSiblings,
                                                                                        movedRefs,
                                                                                        *targetLayerId,
                                                                                        typedAction.insertIndex);
                                      }
                                      else
                                      {
                                          detail::insertSiblingRefs(*sourceSiblings, movedRefs, typedAction.insertIndex);
                                      }

                                      const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                      if (rebuild.failed())
                                          return rebuild;

                                      if (targetLayerId.has_value())
                                          detail::assignWidgetsToLayer(document, ids, *targetLayerId);
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
                                      if (ownerParent != targetParentGroupId)
                                          return juce::Result::fail("ReorderAction group refs must be direct children of parentId");

                                      if (targetLayerId.has_value())
                                      {
                                          if (ownerParent != kRootId)
                                              return juce::Result::fail("Layer parent requires root-level groups");
                                          if (detail::directLayerForGroup(document, groupId).value_or(kRootId) != *targetLayerId)
                                              return juce::Result::fail("ReorderAction group refs must belong to target layer");
                                      }
                                  }

                                  auto siblingsByParent = detail::buildSiblingMap(document);
                                  const auto parentByGroupId = detail::groupParentMap(document);

                                  std::unordered_set<WidgetId> moveSet(ids.begin(), ids.end());
                                  std::vector<detail::SiblingRef> movedRefs;
                                  movedRefs.reserve(ids.size());

                                  if (const auto* sourceSiblings = detail::findSiblings(siblingsByParent, targetParentGroupId);
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

                                  auto* sourceSiblings = detail::findSiblings(siblingsByParent, targetParentGroupId);
                                  if (sourceSiblings == nullptr)
                                      return juce::Result::fail("ReorderAction source parent not found in sibling map");

                                  detail::eraseSiblingRefs(*sourceSiblings, movedRefs);
                                  if (targetLayerId.has_value() && targetParentGroupId == kRootId)
                                  {
                                      detail::insertLayerScopedRefsIntoRootSiblings(document,
                                                                                    *sourceSiblings,
                                                                                    movedRefs,
                                                                                    *targetLayerId,
                                                                                    typedAction.insertIndex);
                                  }
                                  else
                                  {
                                      detail::insertSiblingRefs(*sourceSiblings, movedRefs, typedAction.insertIndex);
                                  }

                                  const auto rebuild = detail::rebuildFromSiblingMap(document, siblingsByParent, parentByGroupId);
                                  if (rebuild.failed())
                                      return rebuild;

                                  if (targetLayerId.has_value())
                                      detail::assignGroupsToLayer(document, ids, *targetLayerId);
                                  return juce::Result::ok();
                              }
                                       },
                                       action);

        if (result.failed())
            return result;

        detail::ensureLayerCoverage(document);
        detail::rebuildGroupMemberGroupIds(document);
        return juce::Result::ok();
    }
}
