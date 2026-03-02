#pragma once

#include "Gyeol/Public/Types.h"
#include <JuceHeader.h>
#include <cstdint>

namespace Gyeol::Runtime
{
    enum class RuntimeDispatchThread
    {
        uiMessage,
        workerBackground,
        realtimeAudio
    };

    enum class RuntimeLogLevel
    {
        off,
        error,
        info,
        trace
    };

    struct RuntimeDispatchReport
    {
        WidgetId sourceWidgetId = kRootId;
        juce::String eventKey;
        int matchedBindings = 0;
        int queuedActions = 0;
        int executedActions = 0;
        int successCount = 0;
        int failureCount = 0;
        int skippedCount = 0;
        int deferredCount = 0;
        bool documentChanged = false;
        bool actionLimitHit = false;
        bool blockedByThreadPolicy = false;
        RuntimeDispatchThread dispatchThread = RuntimeDispatchThread::uiMessage;
        juce::StringArray messages;

        [[nodiscard]] bool hasErrors() const noexcept
        {
            return failureCount > 0;
        }
    };

    class RuntimeDiagnostics
    {
    public:
        struct Settings
        {
            RuntimeLogLevel logLevel = RuntimeLogLevel::info;
            int valueChangedLogStride = 12;
        };

        void setSettings(Settings nextSettings) noexcept;
        [[nodiscard]] const Settings& settings() const noexcept;
        void resetSession() noexcept;

        [[nodiscard]] bool shouldLogEvent(const RuntimeDispatchReport& report) noexcept;
        [[nodiscard]] juce::String formatEventSummary(const RuntimeDispatchReport& report,
                                                      const juce::var& payload) const;

    private:
        Settings runtimeSettings {};
        std::uint64_t valueChangedCounter = 0;
    };
}
