#pragma once

#include "Gyeol/Public/Types.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

namespace Gyeol
{
    // -----------------------------------------------------------------------------
    //  Document Actions (Public contract)
    //
    //  Rules:
    //  - These actions mutate the Document and are undo/redo candidates.
    //  - All actions are payload-only and deterministic.
    //  - Validation here is "shape validation" (cheap checks). Reducer still does
    //    full semantic validation against current Document state.
    //
    //  NOTE:
    //  - bounds are first-class widget geometry (not a PropertyBag entry).
    //  - selection is EditorState (separate) and not included here.
    // -----------------------------------------------------------------------------

    struct CreateWidgetAction
    {
        WidgetType type = WidgetType::button;
        WidgetId parentId = kRootId;
        int insertIndex = -1;                 // -1 == append
        juce::Rectangle<float> bounds;
        PropertyBag properties;
        std::optional<WidgetId> forcedId;     // for paste/duplicate/remote; if absent store allocates
    };

    struct DeleteWidgetsAction
    {
        std::vector<WidgetId> ids;
    };

    struct SetWidgetPropsAction
    {
        std::vector<WidgetId> ids;            // batch apply same patch to all ids
        std::optional<juce::Rectangle<float>> bounds;
        PropertyBag patch;
    };

    struct SetWidgetsBoundsAction
    {
        struct Item
        {
            WidgetId id = kRootId;
            juce::Rectangle<float> bounds;
        };

        std::vector<Item> items;
    };

    struct GroupWidgetsAction
    {
        std::vector<WidgetId> widgetIds;
        std::vector<WidgetId> groupIds;      // optional: compose parent group from existing groups
        juce::String groupName;
        std::optional<WidgetId> forcedGroupId; // optional deterministic id for sync/import
    };

    struct UngroupWidgetsAction
    {
        std::vector<WidgetId> groupIds;
    };

    struct ReparentAction
    {
        std::vector<LayerNodeRef> refs;
        WidgetId parentId = kRootId;
        int insertIndex = -1;                 // -1 == append
    };

    struct ReorderAction
    {
        std::vector<LayerNodeRef> refs;       // stable move: keep relative order of refs
        WidgetId parentId = kRootId;
        int insertIndex = -1;                 // -1 == append
    };

    using Action = std::variant<
        CreateWidgetAction,
        DeleteWidgetsAction,
        SetWidgetPropsAction,
        SetWidgetsBoundsAction,
        GroupWidgetsAction,
        UngroupWidgetsAction,
        ReparentAction,
        ReorderAction>;

    static_assert(std::variant_size_v<Action> == 8,
                  "Action variant must contain exactly eight document actions");

    enum class ActionKind
    {
        createWidget,
        deleteWidgets,
        setWidgetProps,
        setWidgetsBounds,
        groupWidgets,
        ungroupWidgets,
        reparent,
        reorder
    };

    inline ActionKind getActionKind(const Action& action) noexcept
    {
        return std::visit([](const auto& a) -> ActionKind
        {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, CreateWidgetAction>)   return ActionKind::createWidget;
            if constexpr (std::is_same_v<T, DeleteWidgetsAction>)  return ActionKind::deleteWidgets;
            if constexpr (std::is_same_v<T, SetWidgetPropsAction>) return ActionKind::setWidgetProps;
            if constexpr (std::is_same_v<T, SetWidgetsBoundsAction>) return ActionKind::setWidgetsBounds;
            if constexpr (std::is_same_v<T, GroupWidgetsAction>)   return ActionKind::groupWidgets;
            if constexpr (std::is_same_v<T, UngroupWidgetsAction>) return ActionKind::ungroupWidgets;
            if constexpr (std::is_same_v<T, ReparentAction>) return ActionKind::reparent;
            return ActionKind::reorder;
        }, action);
    }

    // Small, cheap "shape validation".
    // Full validation (existence, cycles, bounds constraints, etc.) is in Reducer.
    inline juce::Result validateAction(const Action& action)
    {
        const auto validateIds = [](const std::vector<WidgetId>& ids) -> juce::Result
        {
            if (ids.empty())
                return juce::Result::fail("Action ids must not be empty");

            std::vector<WidgetId> sorted;
            sorted.reserve(ids.size());

            for (auto id : ids)
            {
                if (id <= kRootId)
                    return juce::Result::fail("Action ids must be > rootId");
                sorted.push_back(id);
            }

            std::sort(sorted.begin(), sorted.end());
            if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
                return juce::Result::fail("Action ids must not contain duplicates");

            return juce::Result::ok();
        };

        const auto validateIdsAllowEmpty = [](const std::vector<WidgetId>& ids) -> juce::Result
        {
            if (ids.empty())
                return juce::Result::ok();

            std::vector<WidgetId> sorted;
            sorted.reserve(ids.size());

            for (auto id : ids)
            {
                if (id <= kRootId)
                    return juce::Result::fail("Action ids must be > rootId");
                sorted.push_back(id);
            }

            std::sort(sorted.begin(), sorted.end());
            if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
                return juce::Result::fail("Action ids must not contain duplicates");

            return juce::Result::ok();
        };

        const auto validateNodeRefs = [](const std::vector<LayerNodeRef>& refs) -> juce::Result
        {
            if (refs.empty())
                return juce::Result::fail("Action refs must not be empty");

            std::vector<LayerNodeRef> sorted;
            sorted.reserve(refs.size());

            const auto firstKind = refs.front().kind;
            for (const auto& ref : refs)
            {
                if (ref.id <= kRootId)
                    return juce::Result::fail("Action ref ids must be > rootId");
                if (ref.kind != firstKind)
                    return juce::Result::fail("Action refs must share the same node kind");
                sorted.push_back(ref);
            }

            std::sort(sorted.begin(),
                      sorted.end(),
                      [](const LayerNodeRef& lhs, const LayerNodeRef& rhs)
                      {
                          if (lhs.kind != rhs.kind)
                              return lhs.kind < rhs.kind;
                          return lhs.id < rhs.id;
                      });

            if (std::adjacent_find(sorted.begin(),
                                   sorted.end(),
                                   [](const LayerNodeRef& lhs, const LayerNodeRef& rhs)
                                   {
                                       return lhs.kind == rhs.kind && lhs.id == rhs.id;
                                   }) != sorted.end())
            {
                return juce::Result::fail("Action refs must not contain duplicates");
            }

            return juce::Result::ok();
        };

        return std::visit([&](const auto& a) -> juce::Result
        {
            using T = std::decay_t<decltype(a)>;

            if constexpr (std::is_same_v<T, CreateWidgetAction>)
            {
                if (a.parentId < kRootId)
                    return juce::Result::fail("CreateWidgetAction requires parentId >= rootId");

                if (a.insertIndex < -1)
                    return juce::Result::fail("CreateWidgetAction requires insertIndex >= -1");

                if (a.forcedId.has_value() && *a.forcedId <= kRootId)
                    return juce::Result::fail("CreateWidgetAction forcedId must be > rootId when present");

                const auto x = a.bounds.getX();
                const auto y = a.bounds.getY();
                const auto w = a.bounds.getWidth();
                const auto h = a.bounds.getHeight();
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h))
                    return juce::Result::fail("CreateWidgetAction bounds must be finite");
                if (w < 0.0f || h < 0.0f)
                    return juce::Result::fail("CreateWidgetAction bounds width/height must be >= 0");

                auto propsOk = validatePropertyBag(a.properties);
                if (propsOk.failed())
                    return propsOk;

                return juce::Result::ok();
            }
            else if constexpr (std::is_same_v<T, DeleteWidgetsAction>)
            {
                return validateIds(a.ids);
            }
            else if constexpr (std::is_same_v<T, SetWidgetPropsAction>)
            {
                auto idsOk = validateIds(a.ids);
                if (idsOk.failed())
                    return idsOk;

                if (!a.bounds.has_value() && a.patch.size() == 0)
                    return juce::Result::fail("SetWidgetPropsAction requires bounds and/or non-empty patch");

                if (a.bounds.has_value())
                {
                    const auto x = a.bounds->getX();
                    const auto y = a.bounds->getY();
                    const auto w = a.bounds->getWidth();
                    const auto h = a.bounds->getHeight();
                    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h))
                        return juce::Result::fail("SetWidgetPropsAction bounds must be finite");
                    if (w < 0.0f || h < 0.0f)
                        return juce::Result::fail("SetWidgetPropsAction bounds width/height must be >= 0");
                }

                return validatePropertyBag(a.patch);
            }
            else if constexpr (std::is_same_v<T, SetWidgetsBoundsAction>)
            {
                if (a.items.empty())
                    return juce::Result::fail("SetWidgetsBoundsAction requires non-empty items");

                std::vector<WidgetId> ids;
                ids.reserve(a.items.size());

                for (const auto& item : a.items)
                {
                    if (item.id <= kRootId)
                        return juce::Result::fail("SetWidgetsBoundsAction item.id must be > rootId");

                    const auto x = item.bounds.getX();
                    const auto y = item.bounds.getY();
                    const auto w = item.bounds.getWidth();
                    const auto h = item.bounds.getHeight();
                    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h))
                        return juce::Result::fail("SetWidgetsBoundsAction bounds must be finite");
                    if (w < 0.0f || h < 0.0f)
                        return juce::Result::fail("SetWidgetsBoundsAction bounds width/height must be >= 0");

                    ids.push_back(item.id);
                }

                std::sort(ids.begin(), ids.end());
                if (std::adjacent_find(ids.begin(), ids.end()) != ids.end())
                    return juce::Result::fail("SetWidgetsBoundsAction ids must not contain duplicates");

                return juce::Result::ok();
            }
            else if constexpr (std::is_same_v<T, GroupWidgetsAction>)
            {
                auto widgetIdsOk = validateIdsAllowEmpty(a.widgetIds);
                if (widgetIdsOk.failed())
                    return widgetIdsOk;

                auto groupIdsOk = validateIdsAllowEmpty(a.groupIds);
                if (groupIdsOk.failed())
                    return groupIdsOk;

                if (a.widgetIds.empty() && a.groupIds.empty())
                    return juce::Result::fail("GroupWidgetsAction requires widgetIds and/or groupIds");

                const auto selectedUnitCount = a.widgetIds.size() + a.groupIds.size();
                const auto allowSingleGroupWrapper = a.widgetIds.empty() && a.groupIds.size() == 1;
                if (selectedUnitCount < 2 && !allowSingleGroupWrapper)
                    return juce::Result::fail("GroupWidgetsAction requires at least two selected units (or exactly one group)");

                for (const auto widgetId : a.widgetIds)
                {
                    if (std::find(a.groupIds.begin(), a.groupIds.end(), widgetId) != a.groupIds.end())
                        return juce::Result::fail("GroupWidgetsAction widgetIds and groupIds must not overlap");
                }

                if (a.forcedGroupId.has_value() && *a.forcedGroupId <= kRootId)
                    return juce::Result::fail("GroupWidgetsAction forcedGroupId must be > rootId when present");

                return juce::Result::ok();
            }
            else if constexpr (std::is_same_v<T, UngroupWidgetsAction>)
            {
                return validateIds(a.groupIds);
            }
            else if constexpr (std::is_same_v<T, ReparentAction>)
            {
                auto refsOk = validateNodeRefs(a.refs);
                if (refsOk.failed())
                    return refsOk;

                if (a.parentId < kRootId)
                    return juce::Result::fail("ReparentAction requires parentId >= rootId");

                if (a.insertIndex < -1)
                    return juce::Result::fail("ReparentAction requires insertIndex >= -1");

                if (a.parentId > kRootId)
                {
                    for (const auto& ref : a.refs)
                    {
                        if (ref.id == a.parentId)
                            return juce::Result::fail("ReparentAction refs must not contain parentId");
                    }
                }

                return juce::Result::ok();
            }
            else // ReorderAction
            {
                auto refsOk = validateNodeRefs(a.refs);
                if (refsOk.failed())
                    return refsOk;

                if (a.parentId < kRootId)
                    return juce::Result::fail("ReorderAction requires parentId >= rootId");

                if (a.insertIndex < -1)
                    return juce::Result::fail("ReorderAction requires insertIndex >= -1");

                if (a.parentId > kRootId)
                {
                    for (const auto& ref : a.refs)
                    {
                        if (ref.id == a.parentId)
                            return juce::Result::fail("ReorderAction refs must not contain parentId");
                    }
                }

                return juce::Result::ok();
            }
        }, action);
    }
} // namespace Gyeol
