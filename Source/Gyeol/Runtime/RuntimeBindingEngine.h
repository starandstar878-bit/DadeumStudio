#pragma once

#include "Gyeol/Public/Types.h"
#include "Gyeol/Runtime/RuntimeActionExecutor.h"
#include "Gyeol/Runtime/RuntimeDiagnostics.h"

namespace Gyeol::Runtime
{
    class RuntimeBindingEngine
    {
    public:
        struct DispatchOptions
        {
            int maxActionsPerEvent = 128;
            bool continueOnActionFailure = true;
        };

        void setDispatchOptions(DispatchOptions nextOptions) noexcept;
        [[nodiscard]] const DispatchOptions& dispatchOptions() const noexcept;

        RuntimeDispatchReport dispatchEvent(const DocumentModel& document,
                                            WidgetId sourceWidgetId,
                                            const juce::String& eventKey,
                                            const juce::var& payload,
                                            RuntimeActionExecutor& actionExecutor,
                                            RuntimeActionExecutor::Context& actionContext) const;

    private:
        DispatchOptions options {};
    };
}
