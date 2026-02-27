#include "Gyeol/Editor/Perf/EditorPerfTracker.h"

#include <algorithm>

namespace Gyeol::Ui::Perf
{
    EditorPerfTracker::Scope::Scope(EditorPerfTracker& ownerIn, juce::String operationIn)
        : owner(ownerIn),
          operation(std::move(operationIn)),
          startedMs(juce::Time::getMillisecondCounterHiRes())
    {
    }

    EditorPerfTracker::Scope::~Scope()
    {
        const auto nowMs = juce::Time::getMillisecondCounterHiRes();
        owner.pushSample(operation, nowMs - startedMs);
    }

    EditorPerfTracker::Scope EditorPerfTracker::scoped(const juce::String& operation)
    {
        return Scope(*this, operation);
    }

    void EditorPerfTracker::pushSample(const juce::String& operation, double elapsedMs)
    {
        if (operation.isEmpty())
            return;
        if (!std::isfinite(elapsedMs) || elapsedMs < 0.0)
            return;

        samplesByOperation[operation].push_back(elapsedMs);
    }

    std::vector<PerfSummary> EditorPerfTracker::summarize() const
    {
        std::vector<PerfSummary> result;
        result.reserve(samplesByOperation.size());

        for (const auto& pair : samplesByOperation)
        {
            auto values = pair.second;
            if (values.empty())
                continue;

            std::sort(values.begin(), values.end());
            const auto p50Index = static_cast<size_t>(values.size() * 0.50);
            const auto p95Index = static_cast<size_t>(values.size() * 0.95);

            PerfSummary summary;
            summary.operation = pair.first;
            summary.count = static_cast<int>(values.size());
            summary.p50Ms = values[std::min(values.size() - 1, p50Index)];
            summary.p95Ms = values[std::min(values.size() - 1, p95Index)];
            summary.maxMs = values.back();
            result.push_back(summary);
        }

        return result;
    }

    void EditorPerfTracker::clear()
    {
        samplesByOperation.clear();
    }
}
