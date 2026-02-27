#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

namespace Gyeol::Ui::Perf
{
    struct PerfSummary
    {
        juce::String operation;
        int count = 0;
        double p50Ms = 0.0;
        double p95Ms = 0.0;
        double maxMs = 0.0;
    };

    class EditorPerfTracker
    {
    public:
        class Scope
        {
        public:
            Scope(EditorPerfTracker& ownerIn, juce::String operationIn);
            ~Scope();

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            EditorPerfTracker& owner;
            juce::String operation;
            double startedMs = 0.0;
        };

        Scope scoped(const juce::String& operation);
        void pushSample(const juce::String& operation, double elapsedMs);
        std::vector<PerfSummary> summarize() const;
        void clear();

    private:
        std::map<juce::String, std::vector<double>> samplesByOperation;
    };
}
