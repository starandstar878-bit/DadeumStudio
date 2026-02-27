#pragma once

#include "Gyeol/Public/Types.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Gyeol::Core::SceneValidator
{
    inline bool isFiniteBounds(const juce::Rectangle<float>& bounds) noexcept
    {
        return std::isfinite(bounds.getX())
            && std::isfinite(bounds.getY())
            && std::isfinite(bounds.getWidth())
            && std::isfinite(bounds.getHeight());
    }

    inline juce::Result validateSchemaVersion(const SchemaVersion& version)
    {
        const auto current = currentSchemaVersion();
        if (version.major != current.major)
            return juce::Result::fail("schema.major mismatch");

        if (compareSchemaVersion(version, current) > 0)
            return juce::Result::fail("schema is newer than runtime");

        return juce::Result::ok();
    }

    inline juce::Result validateDocument(const DocumentModel& document)
    {
        const auto schemaCheck = validateSchemaVersion(document.schemaVersion);
        if (schemaCheck.failed())
            return schemaCheck;

        std::vector<WidgetId> sortedIds;
        sortedIds.reserve(document.widgets.size());

        for (const auto& widget : document.widgets)
        {
            if (widget.id <= kRootId)
                return juce::Result::fail("widget.id must be > rootId");

            if (!isFiniteBounds(widget.bounds))
                return juce::Result::fail("widget.bounds must be finite");
            if (widget.bounds.getWidth() < 0.0f || widget.bounds.getHeight() < 0.0f)
                return juce::Result::fail("widget.bounds width/height must be >= 0");

            const auto propertyResult = validatePropertyBag(widget.properties);
            if (propertyResult.failed())
                return juce::Result::fail("invalid widget.properties for widget id " + juce::String(widget.id)
                                          + ": " + propertyResult.getErrorMessage());

            sortedIds.push_back(widget.id);
        }

        std::sort(sortedIds.begin(), sortedIds.end());
        if (std::adjacent_find(sortedIds.begin(), sortedIds.end()) != sortedIds.end())
            return juce::Result::fail("widget ids must be unique");

        std::unordered_set<WidgetId> widgetIdSet(sortedIds.begin(), sortedIds.end());
        std::unordered_set<WidgetId> groupIds;
        std::unordered_set<WidgetId> groupedMembers;
        std::unordered_map<WidgetId, const GroupModel*> groupById;
        std::unordered_map<WidgetId, int> childCounts;
        groupById.reserve(document.groups.size());
        childCounts.reserve(document.groups.size());

        for (const auto& group : document.groups)
        {
            if (group.id <= kRootId)
                return juce::Result::fail("group.id must be > rootId");
            if (widgetIdSet.count(group.id) > 0)
                return juce::Result::fail("group.id must not collide with widget ids");
            if (!groupIds.insert(group.id).second)
                return juce::Result::fail("group ids must be unique");
            groupById.emplace(group.id, &group);
            childCounts[group.id] = 0;

            std::unordered_set<WidgetId> membersInGroup;
            for (const auto memberId : group.memberWidgetIds)
            {
                if (memberId <= kRootId)
                    return juce::Result::fail("group member id must be > rootId");
                if (widgetIdSet.count(memberId) == 0)
                    return juce::Result::fail("group member id not found in document widgets");
                if (!membersInGroup.insert(memberId).second)
                    return juce::Result::fail("group member ids must be unique inside each group");
                if (!groupedMembers.insert(memberId).second)
                    return juce::Result::fail("widget must belong to at most one group");
            }
        }

        for (const auto& group : document.groups)
        {
            if (!group.parentGroupId.has_value())
                continue;

            if (*group.parentGroupId <= kRootId)
                return juce::Result::fail("group.parentGroupId must be > rootId");
            if (*group.parentGroupId == group.id)
                return juce::Result::fail("group.parentGroupId must not equal group.id");
            if (groupById.find(*group.parentGroupId) == groupById.end())
                return juce::Result::fail("group.parentGroupId not found in document groups");

            ++childCounts[*group.parentGroupId];
        }

        for (const auto& group : document.groups)
        {
            std::unordered_set<WidgetId> chain;
            chain.insert(group.id);

            auto parent = group.parentGroupId;
            while (parent.has_value())
            {
                if (!chain.insert(*parent).second)
                    return juce::Result::fail("group hierarchy must not contain cycles");

                const auto it = groupById.find(*parent);
                if (it == groupById.end())
                    return juce::Result::fail("group hierarchy references missing parent");

                parent = it->second->parentGroupId;
            }
        }

        for (const auto& group : document.groups)
        {
            const auto childCountIt = childCounts.find(group.id);
            const auto childCount = childCountIt != childCounts.end() ? childCountIt->second : 0;

            // Keep leaf-group invariants stable: a leaf group should have at least two widgets.
            if (childCount == 0 && group.memberWidgetIds.size() < 2)
                return juce::Result::fail("leaf group must contain at least two widget ids");
        }

        return juce::Result::ok();
    }

    inline juce::Result validateEditorState(const DocumentModel& document,
                                            const EditorStateModel& editorState)
    {
        std::unordered_set<WidgetId> widgetIds;
        widgetIds.reserve(document.widgets.size());
        for (const auto& widget : document.widgets)
            widgetIds.insert(widget.id);

        std::vector<WidgetId> sortedSelection;
        sortedSelection.reserve(editorState.selection.size());

        for (const auto id : editorState.selection)
        {
            if (id <= kRootId)
                return juce::Result::fail("selection ids must be > rootId");
            if (widgetIds.find(id) == widgetIds.end())
                return juce::Result::fail("selection id not found in document");

            sortedSelection.push_back(id);
        }

        std::sort(sortedSelection.begin(), sortedSelection.end());
        if (std::adjacent_find(sortedSelection.begin(), sortedSelection.end()) != sortedSelection.end())
            return juce::Result::fail("selection ids must be unique");

        return juce::Result::ok();
    }

    inline juce::Result validateScene(const DocumentModel& document,
                                      const EditorStateModel* editorState = nullptr)
    {
        const auto documentResult = validateDocument(document);
        if (documentResult.failed())
            return documentResult;

        if (editorState != nullptr)
            return validateEditorState(document, *editorState);

        return juce::Result::ok();
    }
}
