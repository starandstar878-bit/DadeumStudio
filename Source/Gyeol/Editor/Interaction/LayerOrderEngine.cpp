#include "Gyeol/Editor/Interaction/LayerOrderEngine.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace Gyeol::Ui::Interaction
{
    namespace
    {
        const GroupModel* findGroupById(const DocumentModel& document, WidgetId groupId) noexcept
        {
            const auto it = std::find_if(document.groups.begin(),
                                         document.groups.end(),
                                         [groupId](const GroupModel& group)
                                         {
                                             return group.id == groupId;
                                         });
            return it == document.groups.end() ? nullptr : &(*it);
        }

        const LayerModel* findLayerById(const DocumentModel& document, WidgetId layerId) noexcept
        {
            const auto it = std::find_if(document.layers.begin(),
                                         document.layers.end(),
                                         [layerId](const LayerModel& layer)
                                         {
                                             return layer.id == layerId;
                                         });
            return it == document.layers.end() ? nullptr : &(*it);
        }

        std::unordered_map<WidgetId, WidgetId> directWidgetOwnerById(const DocumentModel& document)
        {
            std::unordered_map<WidgetId, WidgetId> ownerById;
            ownerById.reserve(document.widgets.size());
            for (const auto& group : document.groups)
            {
                for (const auto widgetId : group.memberWidgetIds)
                    ownerById[widgetId] = group.id;
            }
            return ownerById;
        }

        std::optional<WidgetId> directLayerForWidget(const DocumentModel& document, WidgetId widgetId) noexcept
        {
            for (const auto& layer : document.layers)
            {
                if (std::find(layer.memberWidgetIds.begin(), layer.memberWidgetIds.end(), widgetId) != layer.memberWidgetIds.end())
                    return layer.id;
            }

            return std::nullopt;
        }

        std::optional<WidgetId> directLayerForGroup(const DocumentModel& document, WidgetId groupId) noexcept
        {
            for (const auto& layer : document.layers)
            {
                if (std::find(layer.memberGroupIds.begin(), layer.memberGroupIds.end(), groupId) != layer.memberGroupIds.end())
                    return layer.id;
            }

            return std::nullopt;
        }

        void collectGroupWidgetsRecursive(const DocumentModel& document,
                                          WidgetId groupId,
                                          std::unordered_set<WidgetId>& outWidgetIds,
                                          std::unordered_set<WidgetId>& visitedGroupIds)
        {
            if (!visitedGroupIds.insert(groupId).second)
                return;

            const auto* group = findGroupById(document, groupId);
            if (group == nullptr)
                return;

            for (const auto widgetId : group->memberWidgetIds)
                outWidgetIds.insert(widgetId);

            for (const auto& child : document.groups)
            {
                if (child.parentGroupId.value_or(kRootId) == groupId)
                    collectGroupWidgetsRecursive(document, child.id, outWidgetIds, visitedGroupIds);
            }
        }

        std::unordered_set<WidgetId> collectGroupWidgetsRecursive(const DocumentModel& document, WidgetId groupId)
        {
            std::unordered_set<WidgetId> widgets;
            std::unordered_set<WidgetId> visited;
            collectGroupWidgetsRecursive(document, groupId, widgets, visited);
            return widgets;
        }

        bool selectionEqualsGroup(const DocumentModel& document,
                                  const std::vector<WidgetId>& selection,
                                  WidgetId groupId)
        {
            const auto groupWidgets = collectGroupWidgetsRecursive(document, groupId);
            if (groupWidgets.size() != selection.size())
                return false;

            for (const auto selectionId : selection)
            {
                if (groupWidgets.count(selectionId) == 0)
                    return false;
            }

            return true;
        }

        std::optional<WidgetId> selectedWholeGroupId(const DocumentModel& document, const std::vector<WidgetId>& selection)
        {
            if (selection.size() < 2)
                return std::nullopt;

            for (const auto& group : document.groups)
            {
                if (selectionEqualsGroup(document, selection, group.id))
                    return group.id;
            }

            return std::nullopt;
        }

        std::unordered_map<WidgetId, int> widgetOrderById(const DocumentModel& document)
        {
            std::unordered_map<WidgetId, int> order;
            order.reserve(document.widgets.size());
            for (size_t i = 0; i < document.widgets.size(); ++i)
                order[document.widgets[i].id] = static_cast<int>(i);
            return order;
        }

        int groupAnchorOrder(const DocumentModel& document,
                             WidgetId groupId,
                             const std::unordered_map<WidgetId, int>& orderByWidgetId)
        {
            const auto widgetIds = collectGroupWidgetsRecursive(document, groupId);
            int anchor = std::numeric_limits<int>::max();
            for (const auto widgetId : widgetIds)
            {
                if (const auto it = orderByWidgetId.find(widgetId); it != orderByWidgetId.end())
                    anchor = std::min(anchor, it->second);
            }
            return anchor;
        }

        std::vector<WidgetId> directLayerSiblingsForParentBackOrder(const DocumentModel& document, WidgetId parentId)
        {
            struct Entry
            {
                WidgetId id = kRootId;
                NodeKind kind = NodeKind::widget;
                int anchor = std::numeric_limits<int>::max();
            };

            const auto orderByWidgetId = widgetOrderById(document);
            const auto ownerByWidgetId = directWidgetOwnerById(document);
            std::vector<Entry> entries;
            entries.reserve(document.widgets.size() + document.groups.size());

            for (const auto& group : document.groups)
            {
                if (group.parentGroupId.value_or(kRootId) != parentId)
                    continue;

                entries.push_back({ group.id, NodeKind::group, groupAnchorOrder(document, group.id, orderByWidgetId) });
            }

            for (const auto& widget : document.widgets)
            {
                const auto owner = ownerByWidgetId.count(widget.id) > 0 ? ownerByWidgetId.at(widget.id) : kRootId;
                if (owner != parentId)
                    continue;

                entries.push_back({ widget.id, NodeKind::widget, orderByWidgetId.at(widget.id) });
            }

            std::sort(entries.begin(),
                      entries.end(),
                      [](const Entry& lhs, const Entry& rhs)
                      {
                          if (lhs.anchor != rhs.anchor)
                              return lhs.anchor < rhs.anchor;
                          if (lhs.kind != rhs.kind)
                              return lhs.kind < rhs.kind;
                          return lhs.id < rhs.id;
                      });

            std::vector<WidgetId> ordered;
            ordered.reserve(entries.size());
            for (auto it = entries.rbegin(); it != entries.rend(); ++it)
                ordered.push_back(it->id);
            return ordered;
        }

        bool applyBlockMoveCommand(const std::vector<WidgetId>& orderedSiblings,
                                   const std::vector<WidgetId>& movedIdsInSiblingOrder,
                                   LayerMoveCommand command,
                                   int& outInsertIndex)
        {
            if (orderedSiblings.empty() || movedIdsInSiblingOrder.empty())
                return false;

            std::unordered_set<WidgetId> moveSet(movedIdsInSiblingOrder.begin(), movedIdsInSiblingOrder.end());

            auto firstSelectedIt = std::find_if(orderedSiblings.begin(),
                                                orderedSiblings.end(),
                                                [&moveSet](WidgetId id)
                                                {
                                                    return moveSet.count(id) > 0;
                                                });
            if (firstSelectedIt == orderedSiblings.end())
                return false;

            auto lastSelectedIt = std::find_if(orderedSiblings.rbegin(),
                                               orderedSiblings.rend(),
                                               [&moveSet](WidgetId id)
                                               {
                                                   return moveSet.count(id) > 0;
                                               });
            if (lastSelectedIt == orderedSiblings.rend())
                return false;

            const auto firstSelectedIndex = static_cast<int>(std::distance(orderedSiblings.begin(), firstSelectedIt));
            const auto lastSelectedIndex = static_cast<int>(orderedSiblings.size() - 1 - std::distance(orderedSiblings.rbegin(), lastSelectedIt));

            auto hasUnselectedBefore = [&]() -> bool
            {
                for (int i = firstSelectedIndex - 1; i >= 0; --i)
                {
                    if (moveSet.count(orderedSiblings[static_cast<size_t>(i)]) == 0)
                        return true;
                }
                return false;
            };

            auto hasUnselectedAfter = [&]() -> bool
            {
                for (int i = lastSelectedIndex + 1; i < static_cast<int>(orderedSiblings.size()); ++i)
                {
                    if (moveSet.count(orderedSiblings[static_cast<size_t>(i)]) == 0)
                        return true;
                }
                return false;
            };

            std::vector<WidgetId> remaining;
            remaining.reserve(orderedSiblings.size() - movedIdsInSiblingOrder.size());
            for (const auto siblingId : orderedSiblings)
            {
                if (moveSet.count(siblingId) == 0)
                    remaining.push_back(siblingId);
            }

            const auto frontInsertToBackInsert = [&remaining](int frontInsertIndex) -> int
            {
                const auto clampedFront = std::clamp(frontInsertIndex,
                                                     0,
                                                     static_cast<int>(remaining.size()));
                const auto backInsertIndex = static_cast<int>(remaining.size()) - clampedFront;
                if (backInsertIndex >= static_cast<int>(remaining.size()))
                    return -1; // back-order end == front
                return backInsertIndex;
            };

            if (command == LayerMoveCommand::sendToBack)
            {
                if (!hasUnselectedAfter())
                    return false;
                outInsertIndex = 0;
                return true;
            }

            if (command == LayerMoveCommand::bringToFront)
            {
                if (!hasUnselectedBefore())
                    return false;
                outInsertIndex = -1;
                return true;
            }

            if (command == LayerMoveCommand::bringForward)
            {
                WidgetId prevUnselectedId = kRootId;
                bool foundPrevUnselected = false;
                for (int i = firstSelectedIndex - 1; i >= 0; --i)
                {
                    const auto candidate = orderedSiblings[static_cast<size_t>(i)];
                    if (moveSet.count(candidate) == 0)
                    {
                        prevUnselectedId = candidate;
                        foundPrevUnselected = true;
                        break;
                    }
                }
                if (!foundPrevUnselected)
                    return false;

                const auto prevRemainingIt = std::find(remaining.begin(), remaining.end(), prevUnselectedId);
                if (prevRemainingIt == remaining.end())
                    return false;

                const auto frontInsertIndex = static_cast<int>(std::distance(remaining.begin(), prevRemainingIt));
                outInsertIndex = frontInsertToBackInsert(frontInsertIndex);
                return true;
            }

            WidgetId nextUnselectedId = kRootId;
            bool foundNextUnselected = false;
            for (int i = lastSelectedIndex + 1; i < static_cast<int>(orderedSiblings.size()); ++i)
            {
                const auto candidate = orderedSiblings[static_cast<size_t>(i)];
                if (moveSet.count(candidate) == 0)
                {
                    nextUnselectedId = candidate;
                    foundNextUnselected = true;
                    break;
                }
            }
            if (!foundNextUnselected)
                return false;

            const auto nextRemainingIt = std::find(remaining.begin(), remaining.end(), nextUnselectedId);
            if (nextRemainingIt == remaining.end())
                return false;

            const auto frontInsertIndex = static_cast<int>(std::distance(remaining.begin(), nextRemainingIt) + 1);
            outInsertIndex = frontInsertToBackInsert(frontInsertIndex);
            return true;
        }

        std::vector<WidgetId> orderedIntersection(const std::vector<WidgetId>& orderedValues,
                                                  const std::vector<WidgetId>& selectedValues)
        {
            std::unordered_set<WidgetId> selectedSet(selectedValues.begin(), selectedValues.end());
            std::vector<WidgetId> intersection;
            for (const auto value : orderedValues)
            {
                if (selectedSet.count(value) > 0)
                    intersection.push_back(value);
            }
            return intersection;
        }

        std::vector<NodeRef> makeNodeRefs(const std::vector<WidgetId>& ids, NodeKind kind)
        {
            std::vector<NodeRef> refs;
            refs.reserve(ids.size());
            for (const auto id : ids)
                refs.push_back({ kind, id });
            return refs;
        }
    }

    juce::Result LayerOrderEngine::moveSelection(DocumentHandle& document, LayerMoveCommand command)
    {
        const auto& snapshot = document.snapshot();
        const auto& selection = document.editorState().selection;
        if (selection.empty())
            return juce::Result::fail("Layer move requires non-empty selection");

        if (const auto selectedGroup = selectedWholeGroupId(snapshot, selection); selectedGroup.has_value())
        {
            const auto* group = findGroupById(snapshot, *selectedGroup);
            if (group == nullptr)
                return juce::Result::fail("Selected group was not found");

            const auto parentId = group->parentGroupId.value_or(kRootId);
            const auto siblings = directLayerSiblingsForParentBackOrder(snapshot, parentId);
            const auto moved = orderedIntersection(siblings, { *selectedGroup });

            int insertIndex = -1;
            if (!applyBlockMoveCommand(siblings, moved, command, insertIndex))
                return juce::Result::fail("Group is already at requested layer edge");

            ReorderAction action;
            action.refs = makeNodeRefs(moved, NodeKind::group);
            action.parent.kind = parentId == kRootId ? ParentKind::root : ParentKind::group;
            action.parent.id = parentId == kRootId ? kRootId : parentId;
            action.insertIndex = insertIndex;
            if (!document.reorder(std::move(action)))
                return juce::Result::fail("Failed to apply group layer move");

            return juce::Result::ok();
        }

        const auto ownerById = directWidgetOwnerById(snapshot);
        std::optional<WidgetId> parentId;
        for (const auto widgetId : selection)
        {
            const auto owner = ownerById.count(widgetId) > 0 ? ownerById.at(widgetId) : kRootId;
            if (!parentId.has_value())
            {
                parentId = owner;
                continue;
            }

            if (*parentId != owner)
                return juce::Result::fail("Layer move requires selection to share same direct parent");
        }

        const auto siblings = directLayerSiblingsForParentBackOrder(snapshot, parentId.value_or(kRootId));
        const auto moved = orderedIntersection(siblings, selection);

        int insertIndex = -1;
        if (!applyBlockMoveCommand(siblings, moved, command, insertIndex))
            return juce::Result::fail("Selection is already at requested layer edge");

        ReorderAction action;
        action.refs = makeNodeRefs(moved, NodeKind::widget);
        const auto parentGroupId = parentId.value_or(kRootId);
        action.parent.kind = parentGroupId == kRootId ? ParentKind::root : ParentKind::group;
        action.parent.id = parentGroupId == kRootId ? kRootId : parentGroupId;
        action.insertIndex = insertIndex;
        if (!document.reorder(std::move(action)))
            return juce::Result::fail("Failed to apply widget layer move");

        return juce::Result::ok();
    }

    juce::Result LayerOrderEngine::reorder(DocumentHandle& document,
                                           const std::vector<WidgetId>& ids,
                                           ParentRef parent,
                                           int insertIndex)
    {
        if (ids.empty())
            return juce::Result::fail("LayerOrderEngine::reorder requires ids");

        ReorderAction action;
        action.refs = makeNodeRefs(ids, NodeKind::widget);
        action.parent = parent;
        action.insertIndex = insertIndex;
        if (!document.reorder(std::move(action)))
            return juce::Result::fail("LayerOrderEngine::reorder failed");

        return juce::Result::ok();
    }

    juce::Result LayerOrderEngine::applyTreeDrop(DocumentHandle& document, const LayerTreeDropRequest& request)
    {
        DBG("[Gyeol][LayerTreeDnD][Engine] applyTreeDrop draggedCount="
            + juce::String(static_cast<int>(request.dragged.size()))
            + " parentKind=" + juce::String(static_cast<int>(request.parent.kind))
            + " parentId=" + juce::String(request.parent.id)
            + " insertIndex=" + juce::String(request.insertIndex));

        if (request.dragged.empty())
            return juce::Result::fail("Layer tree drop requires dragged items");

        const auto draggedKind = request.dragged.front().kind;
        for (const auto& dragged : request.dragged)
        {
            if (dragged.kind != draggedKind)
                return juce::Result::fail("Layer tree drop requires homogeneous node kinds");
            if (dragged.id <= kRootId)
                return juce::Result::fail("Layer tree drop requires node ids > rootId");
        }

        if (draggedKind == NodeKind::layer)
        {
            if (request.parent.kind != ParentKind::root)
                return juce::Result::fail("Layer tree drop for layer nodes requires root parent");

            ReorderAction action;
            action.refs = request.dragged;
            action.parent = request.parent;
            action.insertIndex = request.insertIndex;
            if (!document.reorder(std::move(action)))
                return juce::Result::fail("Layer tree drop failed for layer nodes");
            DBG("[Gyeol][LayerTreeDnD][Engine] layer drop -> reorder ok");
            return juce::Result::ok();
        }

        if (draggedKind == NodeKind::widget)
        {
            std::vector<WidgetId> ids;
            ids.reserve(request.dragged.size());
            for (const auto& dragged : request.dragged)
                ids.push_back(dragged.id);

            const auto ownerById = directWidgetOwnerById(document.snapshot());
            bool sameParent = true;
            for (const auto widgetId : ids)
            {
                const auto ownerGroupId = ownerById.count(widgetId) > 0 ? ownerById.at(widgetId) : kRootId;
                bool parentMatches = false;
                if (request.parent.kind == ParentKind::group)
                    parentMatches = ownerGroupId == request.parent.id;
                else if (request.parent.kind == ParentKind::root)
                    parentMatches = ownerGroupId == kRootId;
                else
                    parentMatches = ownerGroupId == kRootId
                        && directLayerForWidget(document.snapshot(), widgetId).value_or(kRootId) == request.parent.id;

                if (!parentMatches)
                {
                    sameParent = false;
                    break;
                }
            }

            const auto ok = [&]() -> bool
            {
                if (sameParent)
                {
                    DBG("[Gyeol][LayerTreeDnD][Engine] widget drop path=reorder");
                    ReorderAction action;
                    action.refs = request.dragged;
                    action.parent = request.parent;
                    action.insertIndex = request.insertIndex;
                    return document.reorder(std::move(action));
                }

                DBG("[Gyeol][LayerTreeDnD][Engine] widget drop path=reparent");
                ReparentAction action;
                action.refs = request.dragged;
                action.parent = request.parent;
                action.insertIndex = request.insertIndex;
                return document.reparent(std::move(action));
            }();
            if (!ok)
                return juce::Result::fail("Layer tree drop failed for widget nodes");

            DBG("[Gyeol][LayerTreeDnD][Engine] widget drop applied");
            return juce::Result::ok();
        }

        const auto& snapshot = document.snapshot();
        bool sameParent = true;
        for (const auto& dragged : request.dragged)
        {
            const auto* group = findGroupById(snapshot, dragged.id);
            if (group == nullptr)
                return juce::Result::fail("Layer tree drop references missing group node");

            bool parentMatches = false;
            const auto ownerGroupId = group->parentGroupId.value_or(kRootId);
            if (request.parent.kind == ParentKind::group)
                parentMatches = ownerGroupId == request.parent.id;
            else if (request.parent.kind == ParentKind::root)
                parentMatches = ownerGroupId == kRootId;
            else
                parentMatches = ownerGroupId == kRootId
                    && directLayerForGroup(snapshot, dragged.id).value_or(kRootId) == request.parent.id;

            if (!parentMatches)
            {
                sameParent = false;
                break;
            }
        }

        const auto ok = [&]() -> bool
        {
            if (sameParent)
            {
                DBG("[Gyeol][LayerTreeDnD][Engine] group drop path=reorder");
                ReorderAction action;
                action.refs = request.dragged;
                action.parent = request.parent;
                action.insertIndex = request.insertIndex;
                return document.reorder(std::move(action));
            }

            DBG("[Gyeol][LayerTreeDnD][Engine] group drop path=reparent");
            ReparentAction action;
            action.refs = request.dragged;
            action.parent = request.parent;
            action.insertIndex = request.insertIndex;
            return document.reparent(std::move(action));
        }();

        if (!ok)
            return juce::Result::fail("Layer tree drop failed for group nodes");

        DBG("[Gyeol][LayerTreeDnD][Engine] group drop applied");
        return juce::Result::ok();
    }
}
