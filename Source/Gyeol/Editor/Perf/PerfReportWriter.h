#pragma once

#include "Gyeol/Editor/Perf/EditorPerfTracker.h"
#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Ui::Perf
{
    class PerfReportWriter
    {
    public:
        static juce::String toText(const std::vector<PerfSummary>& summaries,
                                   const juce::String& title = "Editor Performance Report");
    };
}
