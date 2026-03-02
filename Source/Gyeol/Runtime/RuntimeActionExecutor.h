#pragma once

#include "Gyeol/Public/Action.h"
#include "Gyeol/Public/Types.h"
#include "Gyeol/Runtime/RuntimeDiagnostics.h"
#include "Gyeol/Runtime/RuntimeParamBridge.h"
#include <functional>

namespace Gyeol::Runtime
{
    struct RuntimeActionResult
    {
        bool success = false;
        bool documentChanged = false;
        bool skipped = false;
        bool deferred = false;
        bool blockedByThreadPolicy = false;
        juce::String message;
    };

    class RuntimeActionExecutor
    {
    public:
        struct Context
        {
            RuntimeParamBridge* paramBridge = nullptr;
            std::function<bool(const SetPropsAction&)> applySetProps;
            std::function<bool(const SetBoundsAction&)> applySetBounds;
            RuntimeDispatchThread dispatchThread = RuntimeDispatchThread::uiMessage;
            std::function<void(RuntimeActionModel)> deferUiAction;
        };

        RuntimeActionResult execute(const RuntimeActionModel& action,
                                    const juce::var& eventPayload,
                                    Context& context) const;
    };
}
