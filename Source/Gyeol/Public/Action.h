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
    struct CreateWidgetPayload
    {
        WidgetType type = WidgetType::button;
        ParentRef parent;
        int insertIndex = -1;
        juce::Rectangle<float> bounds;
        PropertyBag properties;
        std::optional<WidgetId> forcedId;
    };

    struct CreateGroupPayload
    {
        ParentRef parent;
        int insertIndex = -1;
        std::vector<NodeRef> members;
        juce::String name;
        std::optional<WidgetId> forcedId;
    };

    struct CreateLayerPayload
    {
        juce::String name;
        int insertIndex = -1;
        std::optional<WidgetId> forcedId;
        bool visible = true;
        bool locked = false;
    };

    struct CreateAction
    {
        NodeKind kind = NodeKind::widget;
        std::variant<CreateWidgetPayload, CreateGroupPayload, CreateLayerPayload> payload;
    };

    enum class DeleteGroupMode
    {
        liftChildren
    };

    struct DeleteGroupPolicy
    {
        DeleteGroupMode mode = DeleteGroupMode::liftChildren;
    };

    struct DeleteLayerPolicy
    {
        bool forbidDeletingLastLayer = true;
        std::optional<WidgetId> targetLayerId;
    };

    struct DeleteAction
    {
        NodeKind kind = NodeKind::widget;
        std::vector<WidgetId> ids;
        std::variant<std::monostate, DeleteGroupPolicy, DeleteLayerPolicy> policy;
    };

    struct WidgetPropsPatch
    {
        std::optional<bool> visible;
        std::optional<bool> locked;
        std::optional<float> opacity;
        PropertyBag patch;
    };

    struct GroupPropsPatch
    {
        std::optional<juce::String> name;
        std::optional<bool> visible;
        std::optional<bool> locked;
        std::optional<float> opacity;
    };

    struct LayerPropsPatch
    {
        std::optional<juce::String> name;
        std::optional<bool> visible;
        std::optional<bool> locked;
    };

    struct SetPropsAction
    {
        NodeKind kind = NodeKind::widget;
        std::vector<WidgetId> ids;
        std::variant<WidgetPropsPatch, GroupPropsPatch, LayerPropsPatch> patch;
    };

    struct SetBoundsAction
    {
        struct Item
        {
            WidgetId id = kRootId;
            juce::Rectangle<float> bounds;
        };

        std::vector<Item> items;
    };

    struct ReparentAction
    {
        std::vector<NodeRef> refs;
        ParentRef parent;
        int insertIndex = -1;
    };

    struct ReorderAction
    {
        std::vector<NodeRef> refs;
        ParentRef parent;
        int insertIndex = -1;
    };

    using Action = std::variant<
        CreateAction,
        DeleteAction,
        SetPropsAction,
        SetBoundsAction,
        ReparentAction,
        ReorderAction>;

    static_assert(std::variant_size_v<Action> == 6,
                  "Action variant must contain exactly six document actions");

    enum class ActionKind
    {
        create,
        del,
        setProps,
        setBounds,
        reparent,
        reorder
    };

    inline ActionKind getActionKind(const Action& action) noexcept
    {
        return std::visit([](const auto& a) -> ActionKind
        {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, CreateAction>)   return ActionKind::create;
            if constexpr (std::is_same_v<T, DeleteAction>)   return ActionKind::del;
            if constexpr (std::is_same_v<T, SetPropsAction>) return ActionKind::setProps;
            if constexpr (std::is_same_v<T, SetBoundsAction>) return ActionKind::setBounds;
            if constexpr (std::is_same_v<T, ReparentAction>) return ActionKind::reparent;
            return ActionKind::reorder;
        }, action);
    }

    inline juce::Result validateAction(const Action& action)
    {
        const auto validateIds = [](const std::vector<WidgetId>& ids) -> juce::Result
        {
            if (ids.empty())
                return juce::Result::fail("Action ids must not be empty");

            std::vector<WidgetId> sorted;
            sorted.reserve(ids.size());
            for (const auto id : ids)
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

        const auto validateParent = [](const ParentRef& parent) -> juce::Result
        {
            if (parent.kind == ParentKind::root)
            {
                if (parent.id != kRootId)
                    return juce::Result::fail("ParentRef root must use rootId");
                return juce::Result::ok();
            }

            if (parent.id <= kRootId)
                return juce::Result::fail("ParentRef id must be > rootId for non-root parent");

            return juce::Result::ok();
        };

        const auto validateNodeRefs = [](const std::vector<NodeRef>& refs) -> juce::Result
        {
            if (refs.empty())
                return juce::Result::fail("Action refs must not be empty");

            std::vector<NodeRef> sorted;
            sorted.reserve(refs.size());
            const auto firstKind = refs.front().kind;

            for (const auto& ref : refs)
            {
                if (ref.id <= kRootId)
                    return juce::Result::fail("Action ref ids must be > rootId");
                if (ref.kind != firstKind)
                    return juce::Result::fail("Action refs must share same kind");
                sorted.push_back(ref);
            }

            std::sort(sorted.begin(),
                      sorted.end(),
                      [](const NodeRef& lhs, const NodeRef& rhs)
                      {
                          if (lhs.kind != rhs.kind)
                              return lhs.kind < rhs.kind;
                          return lhs.id < rhs.id;
                      });
            if (std::adjacent_find(sorted.begin(),
                                   sorted.end(),
                                   [](const NodeRef& lhs, const NodeRef& rhs)
                                   {
                                       return lhs.kind == rhs.kind && lhs.id == rhs.id;
                                   }) != sorted.end())
            {
                return juce::Result::fail("Action refs must not contain duplicates");
            }

            return juce::Result::ok();
        };

        return std::visit([&](const auto& typedAction) -> juce::Result
        {
            using T = std::decay_t<decltype(typedAction)>;

            if constexpr (std::is_same_v<T, CreateAction>)
            {
                return std::visit([&](const auto& payload) -> juce::Result
                {
                    using P = std::decay_t<decltype(payload)>;
                    if constexpr (std::is_same_v<P, CreateWidgetPayload>)
                    {
                        if (typedAction.kind != NodeKind::widget)
                            return juce::Result::fail("CreateAction kind/payload mismatch(widget)");
                        if (payload.insertIndex < -1)
                            return juce::Result::fail("CreateWidgetPayload insertIndex must be >= -1");
                        if (payload.forcedId.has_value() && *payload.forcedId <= kRootId)
                            return juce::Result::fail("CreateWidgetPayload forcedId must be > rootId");
                        const auto parentOk = validateParent(payload.parent);
                        if (parentOk.failed())
                            return parentOk;
                        const auto x = payload.bounds.getX();
                        const auto y = payload.bounds.getY();
                        const auto w = payload.bounds.getWidth();
                        const auto h = payload.bounds.getHeight();
                        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h))
                            return juce::Result::fail("CreateWidgetPayload bounds must be finite");
                        if (w < 0.0f || h < 0.0f)
                            return juce::Result::fail("CreateWidgetPayload bounds width/height must be >= 0");
                        return validatePropertyBag(payload.properties);
                    }
                    else if constexpr (std::is_same_v<P, CreateGroupPayload>)
                    {
                        if (typedAction.kind != NodeKind::group)
                            return juce::Result::fail("CreateAction kind/payload mismatch(group)");
                        if (payload.insertIndex < -1)
                            return juce::Result::fail("CreateGroupPayload insertIndex must be >= -1");
                        if (payload.forcedId.has_value() && *payload.forcedId <= kRootId)
                            return juce::Result::fail("CreateGroupPayload forcedId must be > rootId");
                        const auto parentOk = validateParent(payload.parent);
                        if (parentOk.failed())
                            return parentOk;
                        if (payload.members.empty())
                            return juce::Result::fail("CreateGroupPayload members must not be empty");

                        std::vector<NodeRef> sorted;
                        sorted.reserve(payload.members.size());
                        for (const auto& member : payload.members)
                        {
                            if (member.id <= kRootId)
                                return juce::Result::fail("CreateGroupPayload member ids must be > rootId");
                            if (member.kind == NodeKind::layer)
                                return juce::Result::fail("CreateGroupPayload members must not include layer");
                            sorted.push_back(member);
                        }

                        std::sort(sorted.begin(),
                                  sorted.end(),
                                  [](const NodeRef& lhs, const NodeRef& rhs)
                                  {
                                      if (lhs.kind != rhs.kind)
                                          return lhs.kind < rhs.kind;
                                      return lhs.id < rhs.id;
                                  });
                        if (std::adjacent_find(sorted.begin(),
                                               sorted.end(),
                                               [](const NodeRef& lhs, const NodeRef& rhs)
                                               {
                                                   return lhs.kind == rhs.kind && lhs.id == rhs.id;
                                               }) != sorted.end())
                        {
                            return juce::Result::fail("CreateGroupPayload members must not contain duplicates");
                        }

                        return juce::Result::ok();
                    }
                    else
                    {
                        if (typedAction.kind != NodeKind::layer)
                            return juce::Result::fail("CreateAction kind/payload mismatch(layer)");
                        if (payload.insertIndex < -1)
                            return juce::Result::fail("CreateLayerPayload insertIndex must be >= -1");
                        if (payload.forcedId.has_value() && *payload.forcedId <= kRootId)
                            return juce::Result::fail("CreateLayerPayload forcedId must be > rootId");
                        return juce::Result::ok();
                    }
                }, typedAction.payload);
            }

            else if constexpr (std::is_same_v<T, DeleteAction>)
            {
                const auto idsOk = validateIds(typedAction.ids);
                if (idsOk.failed())
                    return idsOk;

                if (typedAction.kind == NodeKind::widget)
                    return juce::Result::ok();

                if (typedAction.kind == NodeKind::group)
                {
                    if (std::holds_alternative<DeleteLayerPolicy>(typedAction.policy))
                        return juce::Result::fail("DeleteAction group must not use layer policy");
                    return juce::Result::ok();
                }

                if (std::holds_alternative<DeleteGroupPolicy>(typedAction.policy))
                    return juce::Result::fail("DeleteAction layer must not use group policy");
                if (const auto* policy = std::get_if<DeleteLayerPolicy>(&typedAction.policy);
                    policy != nullptr && policy->targetLayerId.has_value() && *policy->targetLayerId <= kRootId)
                {
                    return juce::Result::fail("DeleteLayerPolicy targetLayerId must be > rootId");
                }
                return juce::Result::ok();
            }

            else if constexpr (std::is_same_v<T, SetPropsAction>)
            {
                const auto idsOk = validateIds(typedAction.ids);
                if (idsOk.failed())
                    return idsOk;

                if (typedAction.kind == NodeKind::widget)
                {
                    const auto* patch = std::get_if<WidgetPropsPatch>(&typedAction.patch);
                    if (patch == nullptr)
                        return juce::Result::fail("SetPropsAction widget requires WidgetPropsPatch");
                    if (!patch->visible.has_value()
                        && !patch->locked.has_value()
                        && !patch->opacity.has_value()
                        && patch->patch.size() == 0)
                    {
                        return juce::Result::fail("SetPropsAction widget patch is empty");
                    }
                    if (patch->opacity.has_value()
                        && (!std::isfinite(*patch->opacity) || *patch->opacity < 0.0f || *patch->opacity > 1.0f))
                    {
                        return juce::Result::fail("SetPropsAction widget opacity must be in [0,1]");
                    }
                    return validatePropertyBag(patch->patch);
                }

                if (typedAction.kind == NodeKind::group)
                {
                    const auto* patch = std::get_if<GroupPropsPatch>(&typedAction.patch);
                    if (patch == nullptr)
                        return juce::Result::fail("SetPropsAction group requires GroupPropsPatch");
                    if (!patch->name.has_value()
                        && !patch->visible.has_value()
                        && !patch->locked.has_value()
                        && !patch->opacity.has_value())
                    {
                        return juce::Result::fail("SetPropsAction group patch is empty");
                    }
                    if (patch->opacity.has_value()
                        && (!std::isfinite(*patch->opacity) || *patch->opacity < 0.0f || *patch->opacity > 1.0f))
                    {
                        return juce::Result::fail("SetPropsAction group opacity must be in [0,1]");
                    }
                    return juce::Result::ok();
                }

                const auto* patch = std::get_if<LayerPropsPatch>(&typedAction.patch);
                if (patch == nullptr)
                    return juce::Result::fail("SetPropsAction layer requires LayerPropsPatch");
                if (!patch->name.has_value()
                    && !patch->visible.has_value()
                    && !patch->locked.has_value())
                {
                    return juce::Result::fail("SetPropsAction layer patch is empty");
                }
                return juce::Result::ok();
            }

            else if constexpr (std::is_same_v<T, SetBoundsAction>)
            {
                if (typedAction.items.empty())
                    return juce::Result::fail("SetBoundsAction requires non-empty items");

                std::vector<WidgetId> ids;
                ids.reserve(typedAction.items.size());
                for (const auto& item : typedAction.items)
                {
                    if (item.id <= kRootId)
                        return juce::Result::fail("SetBoundsAction item.id must be > rootId");
                    const auto x = item.bounds.getX();
                    const auto y = item.bounds.getY();
                    const auto w = item.bounds.getWidth();
                    const auto h = item.bounds.getHeight();
                    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h))
                        return juce::Result::fail("SetBoundsAction bounds must be finite");
                    if (w < 0.0f || h < 0.0f)
                        return juce::Result::fail("SetBoundsAction bounds width/height must be >= 0");
                    ids.push_back(item.id);
                }

                std::sort(ids.begin(), ids.end());
                if (std::adjacent_find(ids.begin(), ids.end()) != ids.end())
                    return juce::Result::fail("SetBoundsAction ids must not contain duplicates");
                return juce::Result::ok();
            }

            else if constexpr (std::is_same_v<T, ReparentAction>)
            {
                const auto refsOk = validateNodeRefs(typedAction.refs);
                if (refsOk.failed())
                    return refsOk;
                const auto parentOk = validateParent(typedAction.parent);
                if (parentOk.failed())
                    return parentOk;
                if (typedAction.insertIndex < -1)
                    return juce::Result::fail("ReparentAction insertIndex must be >= -1");
                if (typedAction.refs.front().kind == NodeKind::layer)
                    return juce::Result::fail("ReparentAction does not support layer refs");
                if (typedAction.parent.kind == ParentKind::group)
                {
                    for (const auto& ref : typedAction.refs)
                    {
                        if (ref.kind == NodeKind::group && ref.id == typedAction.parent.id)
                            return juce::Result::fail("ReparentAction group refs must not include parent");
                    }
                }
                return juce::Result::ok();
            }

            else if constexpr (std::is_same_v<T, ReorderAction>)
            {
                const auto refsOk = validateNodeRefs(typedAction.refs);
                if (refsOk.failed())
                    return refsOk;
                const auto parentOk = validateParent(typedAction.parent);
                if (parentOk.failed())
                    return parentOk;
                if (typedAction.insertIndex < -1)
                    return juce::Result::fail("ReorderAction insertIndex must be >= -1");

                const auto kind = typedAction.refs.front().kind;
                if (kind == NodeKind::layer)
                {
                    if (typedAction.parent.kind != ParentKind::root)
                        return juce::Result::fail("ReorderAction layer refs require root parent");
                }
                else if (typedAction.parent.kind == ParentKind::group)
                {
                    for (const auto& ref : typedAction.refs)
                    {
                        if (ref.kind == NodeKind::group && ref.id == typedAction.parent.id)
                            return juce::Result::fail("ReorderAction group refs must not include parent");
                    }
                }

                return juce::Result::ok();
            }
            else
            {
                return juce::Result::fail("Unhandled Action variant in validateAction");
            }
        }, action);
    }
} // namespace Gyeol
