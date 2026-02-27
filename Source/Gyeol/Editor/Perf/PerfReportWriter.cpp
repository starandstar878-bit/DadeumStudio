#include "Gyeol/Editor/Perf/PerfReportWriter.h"

namespace Gyeol::Ui::Perf
{
    juce::String PerfReportWriter::toText(const std::vector<PerfSummary>& summaries, const juce::String& title)
    {
        juce::StringArray lines;
        lines.add(title);
        lines.add("--------------------------------------------------");
        lines.add("operation\tcount\tp50(ms)\tp95(ms)\tmax(ms)");

        for (const auto& summary : summaries)
        {
            lines.add(summary.operation
                      + "\t" + juce::String(summary.count)
                      + "\t" + juce::String(summary.p50Ms, 3)
                      + "\t" + juce::String(summary.p95Ms, 3)
                      + "\t" + juce::String(summary.maxMs, 3));
        }

        return lines.joinIntoString("\n");
    }
}
