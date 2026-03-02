#include "Gyeol/Runtime/RuntimeBindingEngine.h"

#include <algorithm>

namespace Gyeol::Runtime
{
    void RuntimeBindingEngine::setDispatchOptions(DispatchOptions nextOptions) noexcept
    {
        nextOptions.maxActionsPerEvent = std::max(1, nextOptions.maxActionsPerEvent);
        options = nextOptions;
    }

    const RuntimeBindingEngine::DispatchOptions& RuntimeBindingEngine::dispatchOptions() const noexcept
    {
        return options;
    }

    RuntimeDispatchReport RuntimeBindingEngine::dispatchEvent(const DocumentModel& document,
                                                              WidgetId sourceWidgetId,
                                                              const juce::String& eventKey,
                                                              const juce::var& payload,
                                                              RuntimeActionExecutor& actionExecutor,
                                                              RuntimeActionExecutor::Context& actionContext) const
    {
        RuntimeDispatchReport report;
        report.sourceWidgetId = sourceWidgetId;
        report.eventKey = eventKey.trim();
        report.dispatchThread = actionContext.dispatchThread;

        if (sourceWidgetId <= kRootId)
        {
            report.failureCount = 1;
            report.messages.add("invalid sourceWidgetId");
            return report;
        }

        if (report.eventKey.isEmpty())
        {
            report.failureCount = 1;
            report.messages.add("eventKey is empty");
            return report;
        }

        std::vector<const RuntimeActionModel*> queuedActions;
        for (const auto& binding : document.runtimeBindings)
        {
            if (!binding.enabled)
                continue;
            if (binding.sourceWidgetId != sourceWidgetId)
                continue;
            if (binding.eventKey.trim() != report.eventKey)
                continue;

            report.matchedBindings += 1;
            queuedActions.reserve(queuedActions.size() + binding.actions.size());
            for (const auto& action : binding.actions)
                queuedActions.push_back(&action);
        }

        report.queuedActions = static_cast<int>(queuedActions.size());
        if (queuedActions.empty())
            return report;

        const auto actionLimit = std::max(1, options.maxActionsPerEvent);
        const auto executeCount = std::min(report.queuedActions, actionLimit);
        if (executeCount < report.queuedActions)
        {
            report.actionLimitHit = true;
            report.messages.add("action limit reached: " + juce::String(executeCount) + "/" + juce::String(report.queuedActions));
            report.skippedCount += report.queuedActions - executeCount;
        }

        for (int i = 0; i < executeCount; ++i)
        {
            const auto* action = queuedActions[static_cast<size_t>(i)];
            if (action == nullptr)
            {
                report.failureCount += 1;
                report.messages.add("null runtime action at index " + juce::String(i));
                if (!options.continueOnActionFailure)
                    break;
                continue;
            }

            const auto result = actionExecutor.execute(*action, payload, actionContext);
            report.executedActions += 1;

            if (result.success)
            {
                report.successCount += 1;
                report.documentChanged = report.documentChanged || result.documentChanged;
            }
            else if (result.deferred)
            {
                report.deferredCount += 1;
                report.skippedCount += 1;
            }
            else if (result.skipped)
            {
                report.skippedCount += 1;
            }
            else
            {
                report.failureCount += 1;
                report.blockedByThreadPolicy = report.blockedByThreadPolicy || result.blockedByThreadPolicy;
                report.messages.add("action[" + juce::String(i) + "] " + result.message);
                if (!options.continueOnActionFailure)
                    break;
            }
        }

        return report;
    }
}
