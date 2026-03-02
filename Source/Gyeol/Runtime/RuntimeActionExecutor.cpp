#include "Gyeol/Runtime/RuntimeActionExecutor.h"

namespace Gyeol::Runtime
{
    namespace
    {
        bool isUiThreadOnlyAction(const RuntimeActionKind kind) noexcept
        {
            return kind == RuntimeActionKind::setNodeProps
                || kind == RuntimeActionKind::setNodeBounds;
        }

        RuntimeActionResult makeFailure(const juce::String& message)
        {
            RuntimeActionResult result;
            result.success = false;
            result.message = message;
            return result;
        }

        RuntimeActionResult makeThreadPolicyBlocked(const juce::String& message)
        {
            RuntimeActionResult result;
            result.success = false;
            result.blockedByThreadPolicy = true;
            result.message = message;
            return result;
        }

        RuntimeActionResult makeDeferred(const juce::String& message)
        {
            RuntimeActionResult result;
            result.skipped = true;
            result.deferred = true;
            result.message = message;
            return result;
        }

        RuntimeActionResult makeSuccess(bool changed)
        {
            RuntimeActionResult result;
            result.success = true;
            result.documentChanged = changed;
            return result;
        }
    }

    RuntimeActionResult RuntimeActionExecutor::execute(const RuntimeActionModel& action,
                                                       const juce::var& eventPayload,
                                                       Context& context) const
    {
        if (isUiThreadOnlyAction(action.kind)
            && context.dispatchThread != RuntimeDispatchThread::uiMessage)
        {
            if (static_cast<bool>(context.deferUiAction))
            {
                context.deferUiAction(action);
                return makeDeferred("ui-only action deferred to message thread");
            }

            return makeThreadPolicyBlocked("ui-only action blocked outside message thread");
        }

        switch (action.kind)
        {
            case RuntimeActionKind::setRuntimeParam:
            {
                if (context.paramBridge == nullptr)
                    return makeFailure("param bridge is not configured");

                const auto result = context.paramBridge->set(action.paramKey, action.value, eventPayload);
                if (!result.wasOk())
                    return makeFailure("setRuntimeParam failed: " + result.message);

                return makeSuccess(false);
            }

            case RuntimeActionKind::adjustRuntimeParam:
            {
                if (context.paramBridge == nullptr)
                    return makeFailure("param bridge is not configured");

                const auto result = context.paramBridge->adjust(action.paramKey, action.delta);
                if (!result.wasOk())
                    return makeFailure("adjustRuntimeParam failed: " + result.message);

                return makeSuccess(false);
            }

            case RuntimeActionKind::toggleRuntimeParam:
            {
                if (context.paramBridge == nullptr)
                    return makeFailure("param bridge is not configured");

                const auto result = context.paramBridge->toggle(action.paramKey);
                if (!result.wasOk())
                    return makeFailure("toggleRuntimeParam failed: " + result.message);

                return makeSuccess(false);
            }

            case RuntimeActionKind::setNodeProps:
            {
                if (action.target.id <= kRootId)
                    return makeFailure("setNodeProps target id is invalid");
                if (!static_cast<bool>(context.applySetProps))
                    return makeFailure("setNodeProps callback is not configured");

                SetPropsAction setProps;
                setProps.kind = action.target.kind;
                setProps.ids = { action.target.id };

                if (action.target.kind == NodeKind::widget)
                {
                    WidgetPropsPatch patch;
                    patch.visible = action.visible;
                    patch.locked = action.locked;
                    patch.opacity = action.opacity;
                    for (int i = 0; i < action.patch.size(); ++i)
                        patch.patch.set(action.patch.getName(i), action.patch.getValueAt(i));
                    setProps.patch = std::move(patch);
                }
                else if (action.target.kind == NodeKind::group)
                {
                    GroupPropsPatch patch;
                    patch.visible = action.visible;
                    patch.locked = action.locked;
                    patch.opacity = action.opacity;
                    setProps.patch = std::move(patch);
                }
                else
                {
                    LayerPropsPatch patch;
                    patch.visible = action.visible;
                    patch.locked = action.locked;
                    setProps.patch = std::move(patch);
                }

                if (!context.applySetProps(setProps))
                    return makeFailure("setNodeProps apply failed");

                return makeSuccess(true);
            }

            case RuntimeActionKind::setNodeBounds:
            {
                if (action.targetWidgetId <= kRootId)
                    return makeFailure("setNodeBounds targetWidgetId is invalid");
                if (!static_cast<bool>(context.applySetBounds))
                    return makeFailure("setNodeBounds callback is not configured");

                SetBoundsAction setBounds;
                setBounds.items.push_back({ action.targetWidgetId, action.bounds });
                if (!context.applySetBounds(setBounds))
                    return makeFailure("setNodeBounds apply failed");

                return makeSuccess(true);
            }
        }

        RuntimeActionResult result;
        result.skipped = true;
        return result;
    }
}
