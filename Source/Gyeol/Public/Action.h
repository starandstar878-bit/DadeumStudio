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

    struct ReparentWidgetsAction
    {
        std::vector<WidgetId> ids;
        WidgetId parentId = kRootId;
        int insertIndex = -1;                 // -1 == append
    };

    struct ReorderWidgetsAction
    {
        std::vector<WidgetId> ids;            // stable move: keep relative order of ids
        WidgetId parentId = kRootId;
        int insertIndex = -1;                 // -1 == append
    };

    using Action = std::variant<
        CreateWidgetAction,
        DeleteWidgetsAction,
        SetWidgetPropsAction,
        ReparentWidgetsAction,
        ReorderWidgetsAction>;

    static_assert(std::variant_size_v<Action> == 5,
                  "Action variant must contain exactly five document actions");

    enum class ActionKind
    {
        createWidget,
        deleteWidgets,
        setWidgetProps,
        reparentWidgets,
        reorderWidgets
    };

    inline ActionKind getActionKind(const Action& action) noexcept
    {
        return std::visit([](const auto& a) -> ActionKind
        {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, CreateWidgetAction>)   return ActionKind::createWidget;
            if constexpr (std::is_same_v<T, DeleteWidgetsAction>)  return ActionKind::deleteWidgets;
            if constexpr (std::is_same_v<T, SetWidgetPropsAction>) return ActionKind::setWidgetProps;
            if constexpr (std::is_same_v<T, ReparentWidgetsAction>)return ActionKind::reparentWidgets;
            return ActionKind::reorderWidgets;
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

        const auto idsMustNotContain = [](const std::vector<WidgetId>& ids, WidgetId forbidden) -> bool
        {
            return std::find(ids.begin(), ids.end(), forbidden) != ids.end();
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
            else if constexpr (std::is_same_v<T, ReparentWidgetsAction>)
            {
                auto idsOk = validateIds(a.ids);
                if (idsOk.failed())
                    return idsOk;

                if (a.parentId < kRootId)
                    return juce::Result::fail("ReparentWidgetsAction requires parentId >= rootId");

                if (a.insertIndex < -1)
                    return juce::Result::fail("ReparentWidgetsAction requires insertIndex >= -1");

                if (a.parentId > kRootId && idsMustNotContain(a.ids, a.parentId))
                    return juce::Result::fail("ReparentWidgetsAction ids must not contain parentId");

                return juce::Result::ok();
            }
            else // ReorderWidgetsAction
            {
                auto idsOk = validateIds(a.ids);
                if (idsOk.failed())
                    return idsOk;

                if (a.parentId < kRootId)
                    return juce::Result::fail("ReorderWidgetsAction requires parentId >= rootId");

                if (a.insertIndex < -1)
                    return juce::Result::fail("ReorderWidgetsAction requires insertIndex >= -1");

                if (a.parentId > kRootId && idsMustNotContain(a.ids, a.parentId))
                    return juce::Result::fail("ReorderWidgetsAction ids must not contain parentId");

                return juce::Result::ok();
            }
        }, action);
    }
} // namespace Gyeol
