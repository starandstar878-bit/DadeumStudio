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

        std::vector<WidgetId> directWidgetsForParent(const DocumentModel& document, WidgetId parentId)
        {
            const auto ownerById = directWidgetOwnerById(document);
            std::vector<WidgetId> ids;
            ids.reserve(document.widgets.size());

            for (const auto& widget : document.widgets)
            {
                const auto owner = ownerById.count(widget.id) > 0 ? ownerById.at(widget.id) : kRootId;
                if (owner == parentId)
                    ids.push_back(widget.id);
            }

            return ids;
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

        std::vector<WidgetId> directGroupsForParentBackOrder(const DocumentModel& document, WidgetId parentId)
        {
            struct Entry
            {
                WidgetId groupId = kRootId;
                int anchor = std::numeric_limits<int>::max();
            };

            const auto orderByWidgetId = widgetOrderById(document);
            std::vector<Entry> entries;
            for (const auto& group : document.groups)
            {
                if (group.parentGroupId.value_or(kRootId) != parentId)
                    continue;

                entries.push_back({ group.id, groupAnchorOrder(document, group.id, orderByWidgetId) });
            }

            std::sort(entries.begin(),
                      entries.end(),
                      [](const Entry& lhs, const Entry& rhs)
                      {
                          if (lhs.anchor != rhs.anchor)
                              return lhs.anchor < rhs.anchor;
                          return lhs.groupId < rhs.groupId;
                      });

            std::vector<WidgetId> ordered;
            ordered.reserve(entries.size());
            for (const auto& entry : entries)
                ordered.push_back(entry.groupId);
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

            std::vector<WidgetId> remaining;
            remaining.reserve(orderedSiblings.size() - movedIdsInSiblingOrder.size());
            for (const auto siblingId : orderedSiblings)
            {
                if (moveSet.count(siblingId) == 0)
                    remaining.push_back(siblingId);
            }

            if (command == LayerMoveCommand::sendToBack)
            {
                outInsertIndex = 0;
                return true;
            }

            if (command == LayerMoveCommand::bringToFront)
            {
                outInsertIndex = -1;
                return true;
            }

            if (command == LayerMoveCommand::bringForward)
            {
                const auto nextUnselected = std::find_if(orderedSiblings.begin() + (lastSelectedIndex + 1),
                                                         orderedSiblings.end(),
                                                         [&moveSet](WidgetId id)
                                                         {
                                                             return moveSet.count(id) == 0;
                                                         });
                if (nextUnselected == orderedSiblings.end())
                    return false;

                const auto nextId = *nextUnselected;
                const auto nextRemainingIt = std::find(remaining.begin(), remaining.end(), nextId);
                if (nextRemainingIt == remaining.end())
                    return false;

                outInsertIndex = static_cast<int>(std::distance(remaining.begin(), nextRemainingIt) + 1);
                return true;
            }

            const auto prevUnselectedIt = std::find_if(orderedSiblings.rbegin() + (static_cast<std::ptrdiff_t>(orderedSiblings.size() - firstSelectedIndex)),
                                                       orderedSiblings.rend(),
                                                       [&moveSet](WidgetId id)
                                                       {
                                                           return moveSet.count(id) == 0;
                                                       });
            if (prevUnselectedIt == orderedSiblings.rend())
            {
                outInsertIndex = 0;
                return true;
            }

            const auto prevId = *prevUnselectedIt;
            const auto prevRemainingIt = std::find(remaining.begin(), remaining.end(), prevId);
            if (prevRemainingIt == remaining.end())
                return false;

            outInsertIndex = static_cast<int>(std::distance(remaining.begin(), prevRemainingIt));
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

        std::vector<LayerNodeRef> makeNodeRefs(const std::vector<WidgetId>& ids, LayerNodeKind kind)
        {
            std::vector<LayerNodeRef> refs;
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
            const auto siblings = directGroupsForParentBackOrder(snapshot, parentId);
            const auto moved = orderedIntersection(siblings, { *selectedGroup });

            int insertIndex = -1;
            if (!applyBlockMoveCommand(siblings, moved, command, insertIndex))
                return juce::Result::fail("Group is already at requested layer edge");

            ReparentAction action;
            action.refs = makeNodeRefs(moved, LayerNodeKind::group);
            action.parentId = parentId;
            action.insertIndex = insertIndex;
            if (!document.reparent(std::move(action)))
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

        const auto siblingWidgets = directWidgetsForParent(snapshot, parentId.value_or(kRootId));
        const auto moved = orderedIntersection(siblingWidgets, selection);

        int insertIndex = -1;
        if (!applyBlockMoveCommand(siblingWidgets, moved, command, insertIndex))
            return juce::Result::fail("Selection is already at requested layer edge");

        ReorderAction action;
        action.refs = makeNodeRefs(moved, LayerNodeKind::widget);
        action.parentId = parentId.value_or(kRootId);
        action.insertIndex = insertIndex;
        if (!document.reorder(std::move(action)))
            return juce::Result::fail("Failed to apply widget layer move");

        return juce::Result::ok();
    }

    juce::Result LayerOrderEngine::reorder(DocumentHandle& document,
                                           const std::vector<WidgetId>& ids,
                                           WidgetId parentId,
                                           int insertIndex)
    {
        if (ids.empty())
            return juce::Result::fail("LayerOrderEngine::reorder requires ids");

        ReorderAction action;
        action.refs = makeNodeRefs(ids, LayerNodeKind::widget);
        action.parentId = parentId;
        action.insertIndex = insertIndex;
        if (!document.reorder(std::move(action)))
            return juce::Result::fail("LayerOrderEngine::reorder failed");

        return juce::Result::ok();
    }

    juce::Result LayerOrderEngine::applyTreeDrop(DocumentHandle& document, const LayerTreeDropRequest& request)
    {
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

        if (draggedKind == LayerNodeKind::widget)
        {
            std::vector<WidgetId> ids;
            ids.reserve(request.dragged.size());
            for (const auto& dragged : request.dragged)
                ids.push_back(dragged.id);

            const auto ownerById = directWidgetOwnerById(document.snapshot());
            bool sameParent = true;
            for (const auto widgetId : ids)
            {
                const auto owner = ownerById.count(widgetId) > 0 ? ownerById.at(widgetId) : kRootId;
                if (owner != request.parentId)
                {
                    sameParent = false;
                    break;
                }
            }

            const auto ok = [&]() -> bool
            {
                if (sameParent)
                {
                    ReorderAction action;
                    action.refs = request.dragged;
                    action.parentId = request.parentId;
                    action.insertIndex = request.insertIndex;
                    return document.reorder(std::move(action));
                }

                ReparentAction action;
                action.refs = request.dragged;
                action.parentId = request.parentId;
                action.insertIndex = request.insertIndex;
                return document.reparent(std::move(action));
            }();
            if (!ok)
                return juce::Result::fail("Layer tree drop failed for widget nodes");

            return juce::Result::ok();
        }

        const auto& snapshot = document.snapshot();
        bool sameParent = true;
        for (const auto& dragged : request.dragged)
        {
            const auto* group = findGroupById(snapshot, dragged.id);
            if (group == nullptr)
                return juce::Result::fail("Layer tree drop references missing group node");

            if (group->parentGroupId.value_or(kRootId) != request.parentId)
            {
                sameParent = false;
                break;
            }
        }

        const auto ok = [&]() -> bool
        {
            if (sameParent)
            {
                ReorderAction action;
                action.refs = request.dragged;
                action.parentId = request.parentId;
                action.insertIndex = request.insertIndex;
                return document.reorder(std::move(action));
            }

            ReparentAction action;
            action.refs = request.dragged;
            action.parentId = request.parentId;
            action.insertIndex = request.insertIndex;
            return document.reparent(std::move(action));
        }();

        if (!ok)
            return juce::Result::fail("Layer tree drop failed for group nodes");

        return juce::Result::ok();
    }
}
