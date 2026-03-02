#include "Gyeol/Runtime/RuntimeDiagnostics.h"

#include <algorithm>

namespace Gyeol::Runtime
{
    namespace
    {
        juce::String dispatchThreadToText(RuntimeDispatchThread thread)
        {
            switch (thread)
            {
                case RuntimeDispatchThread::uiMessage: return "ui";
                case RuntimeDispatchThread::workerBackground: return "worker";
                case RuntimeDispatchThread::realtimeAudio: return "audio";
            }

            return "unknown";
        }

        juce::String payloadToText(const juce::var& payload)
        {
            if (payload.isVoid() || payload.isUndefined())
                return "n/a";
            return payload.toString();
        }
    }

    void RuntimeDiagnostics::setSettings(Settings nextSettings) noexcept
    {
        nextSettings.valueChangedLogStride = std::max(1, nextSettings.valueChangedLogStride);
        runtimeSettings = nextSettings;
    }

    const RuntimeDiagnostics::Settings& RuntimeDiagnostics::settings() const noexcept
    {
        return runtimeSettings;
    }

    void RuntimeDiagnostics::resetSession() noexcept
    {
        valueChangedCounter = 0;
    }

    bool RuntimeDiagnostics::shouldLogEvent(const RuntimeDispatchReport& report) noexcept
    {
        if (report.hasErrors())
            return true;
        if (report.blockedByThreadPolicy)
            return true;

        if (runtimeSettings.logLevel == RuntimeLogLevel::off)
            return false;

        if (runtimeSettings.logLevel == RuntimeLogLevel::error)
            return false;

        if (report.eventKey == "onValueChanged")
        {
            valueChangedCounter += 1;
            return (valueChangedCounter % static_cast<std::uint64_t>(runtimeSettings.valueChangedLogStride)) == 0;
        }

        return true;
    }

    juce::String RuntimeDiagnostics::formatEventSummary(const RuntimeDispatchReport& report,
                                                        const juce::var& payload) const
    {
        auto text = "widget=" + juce::String(report.sourceWidgetId)
                  + " event=" + report.eventKey
                  + " bindings=" + juce::String(report.matchedBindings)
                  + " queued=" + juce::String(report.queuedActions)
                  + " exec=" + juce::String(report.executedActions)
                  + " ok=" + juce::String(report.successCount)
                  + " fail=" + juce::String(report.failureCount)
                  + " skip=" + juce::String(report.skippedCount)
                  + " defer=" + juce::String(report.deferredCount)
                  + " changed=" + juce::String(report.documentChanged ? 1 : 0)
                  + " limited=" + juce::String(report.actionLimitHit ? 1 : 0)
                  + " thread=" + dispatchThreadToText(report.dispatchThread)
                  + " payload=" + payloadToText(payload);

        if (!report.messages.isEmpty() && runtimeSettings.logLevel == RuntimeLogLevel::trace)
        {
            juce::StringArray preview;
            const auto count = std::min(3, report.messages.size());
            for (int i = 0; i < count; ++i)
                preview.add(report.messages[i]);
            text += " notes=" + preview.joinIntoString(" | ");
        }
        else if (!report.messages.isEmpty())
        {
            text += " note=" + report.messages[0];
        }

        return text;
    }
}
