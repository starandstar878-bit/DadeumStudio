#include "Teul/Editor/Panels/DiagnosticsDrawer.h"
#include "Teul/Editor/Theme/TeulPalette.h"

#include <algorithm>
#include <array>
#include <map>
#include <vector>

namespace Teul {
namespace {

enum class DiagnosticCategory { Verification, Performance, Build };
enum class DiagnosticSeverity { Passing, Failure, Missing };

struct DiagnosticSnapshot {
  juce::String title;
  juce::String statusText;
  juce::String artifactDirectory;
  juce::String detailText;
  juce::String summaryText;
  juce::String contextText;
  std::map<juce::String, juce::String> summaryValues;
  DiagnosticCategory category = DiagnosticCategory::Verification;
  DiagnosticSeverity severity = DiagnosticSeverity::Missing;
  bool passed = false;
  bool available = false;
};

struct BenchmarkTimelineEntry {
  juce::String timestampUtc;
  bool passed = false;
  int failedCaseCount = 0;
  int iterationCount = 0;
  double peakCpuLoadPercent = 0.0;
  double peakMaxProcessMilliseconds = 0.0;
  double peakMaxBuildMilliseconds = 0.0;
};

constexpr int filterAll = 1;
constexpr int filterFailures = 2;
constexpr int filterPassing = 3;
constexpr int filterMissing = 4;
constexpr int compareNone = 1;
constexpr int compareBaseId = 100;

juce::String normalizeLineEndings(const juce::String &text) {
  return text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n");
}

std::map<juce::String, juce::String> parseSummary(const juce::String &text) {
  std::map<juce::String, juce::String> values;
  const auto lines = juce::StringArray::fromLines(text);
  for (const auto &line : lines) {
    const auto trimmed = line.trim();
    if (trimmed.isEmpty())
      continue;

    const auto separator = trimmed.indexOfChar('=');
    if (separator <= 0)
      continue;

    const auto key = trimmed.substring(0, separator).trim();
    if (values.find(key) != values.end())
      continue;

    values[key] = trimmed.substring(separator + 1).trim();
  }
  return values;
}

juce::String valueOrFallback(const std::map<juce::String, juce::String> &values,
                             const juce::String &key,
                             const juce::String &fallback = {}) {
  const auto it = values.find(key);
  return it != values.end() ? it->second : fallback;
}

juce::String joinParts(const juce::StringArray &parts) {
  juce::String text;
  for (int index = 0; index < parts.size(); ++index) {
    const auto part = parts[index].trim();
    if (part.isEmpty())
      continue;
    if (text.isNotEmpty())
      text << "  |  ";
    text << part;
  }
  return text;
}

juce::String categoryTitle(DiagnosticCategory category) {
  switch (category) {
  case DiagnosticCategory::Verification:
    return "Verification";
  case DiagnosticCategory::Performance:
    return "Performance";
  case DiagnosticCategory::Build:
    return "Build";
  }

  return "Diagnostics";
}

juce::Colour categoryColour(DiagnosticCategory category) {
  switch (category) {
  case DiagnosticCategory::Verification:
    return TeulPalette::AccentBlue();
  case DiagnosticCategory::Performance:
    return TeulPalette::AccentPurple();
  case DiagnosticCategory::Build:
    return TeulPalette::AccentAmber();
  }

  return TeulPalette::AccentSlate();
}

juce::Colour statusColour(const DiagnosticSnapshot &snapshot) {
  switch (snapshot.severity) {
  case DiagnosticSeverity::Passing:
    return TeulPalette::AccentGreen();
  case DiagnosticSeverity::Failure:
    return TeulPalette::AccentRed();
  case DiagnosticSeverity::Missing:
    return TeulPalette::AccentSlate();
  }

  return TeulPalette::AccentSlate();
}

juce::String formatMetric(const juce::String &label, const juce::String &value) {
  if (value.isEmpty())
    return {};
  return label + " " + value;
}

bool tryParseDouble(const juce::String &text, double &valueOut) {
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return false;

  if (!(trimmed.containsOnly("0123456789.-+") || trimmed.containsChar('e') ||
        trimmed.containsChar('E')))
    return false;

  valueOut = trimmed.getDoubleValue();
  return true;
}

juce::String formatDelta(double delta, int decimals = 6) {
  const auto prefix = delta > 0.0 ? "+" : "";
  return prefix + juce::String(delta, decimals);
}

juce::String buildSummaryText(const std::map<juce::String, juce::String> &values,
                              const DiagnosticSnapshot &snapshot) {
  juce::StringArray parts;
  const auto graphId = valueOrFallback(values, "graphId");
  const auto stimulusId = valueOrFallback(values, "stimulusId");
  const auto profileId = valueOrFallback(values, "profileId");
  if (graphId.isNotEmpty() || stimulusId.isNotEmpty() || profileId.isNotEmpty()) {
    juce::String caseId;
    if (graphId.isNotEmpty())
      caseId << graphId;
    if (stimulusId.isNotEmpty()) {
      if (caseId.isNotEmpty())
        caseId << "/";
      caseId << stimulusId;
    }
    if (profileId.isNotEmpty()) {
      if (caseId.isNotEmpty())
        caseId << "/";
      caseId << profileId;
    }
    parts.add(caseId);
  }

  const auto passedCaseCount = valueOrFallback(values, "passedCaseCount");
  const auto totalCaseCount = valueOrFallback(values, "totalCaseCount");
  if (passedCaseCount.isNotEmpty() && totalCaseCount.isNotEmpty())
    parts.add(passedCaseCount + "/" + totalCaseCount + " pass");

  const auto failedCaseCount = valueOrFallback(values, "failedCaseCount");
  if (failedCaseCount.isNotEmpty() && failedCaseCount != "0")
    parts.add(failedCaseCount + " fail");

  const auto iterationCount = valueOrFallback(values, "iterationCount");
  if (iterationCount.isNotEmpty())
    parts.add("iter " + iterationCount);

  const auto totalSamples = valueOrFallback(values, "totalSamples");
  if (totalSamples.isNotEmpty())
    parts.add(totalSamples + " samples");

  const auto maxAbs = formatMetric("max", valueOrFallback(values, "maxAbsoluteError"));
  const auto rms = formatMetric("rms", valueOrFallback(values, "rmsError"));
  if (maxAbs.isNotEmpty())
    parts.add(maxAbs);
  if (rms.isNotEmpty())
    parts.add(rms);

  const auto compileExitCode = valueOrFallback(values, "compileExitCode");
  if (compileExitCode.isNotEmpty())
    parts.add("compile " + compileExitCode);

  const auto failureReason = valueOrFallback(values, "failureReason");
  if (!snapshot.passed && failureReason.isNotEmpty())
    parts.add(failureReason);

  const auto summary = joinParts(parts);
  if (summary.isNotEmpty())
    return summary;

  if (!snapshot.available)
    return "No artifacts yet";
  return snapshot.passed ? "Latest run passed" : "Latest run failed";
}

juce::String buildContextText(const DiagnosticSnapshot &snapshot) {
  if (snapshot.artifactDirectory.isEmpty())
    return {};

  const auto directory = juce::File(snapshot.artifactDirectory);
  const auto name = directory.getFileName();
  return name.isNotEmpty() ? name : snapshot.artifactDirectory;
}

juce::File findLatestCompileSmokeDirectory() {
  const auto buildsDir = juce::File::getCurrentWorkingDirectory().getChildFile("Builds");
  if (!buildsDir.isDirectory())
    return {};

  juce::Array<juce::File> children;
  buildsDir.findChildFiles(children, juce::File::findDirectories, false,
                           "TeulCompileSmoke_*");
  juce::File latest;
  juce::Time latestTime;
  for (const auto &child : children) {
    const auto bundle = child.getChildFile("artifact-bundle.json");
    if (!bundle.existsAsFile())
      continue;
    const auto modified = child.getLastModificationTime();
    if (!latest.exists() || modified > latestTime) {
      latest = child;
      latestTime = modified;
    }
  }
  return latest;
}

std::vector<BenchmarkTimelineEntry> loadBenchmarkTimelineEntries() {
  std::vector<BenchmarkTimelineEntry> entries;
  const auto historyFile = juce::File::getCurrentWorkingDirectory()
                               .getChildFile("Builds")
                               .getChildFile("TeulVerification")
                               .getChildFile("Benchmark")
                               .getChildFile("representative_benchmark_primary-history.json");
  if (!historyFile.existsAsFile())
    return entries;

  const auto parsed = juce::JSON::parse(historyFile);
  if (!parsed.isObject())
    return entries;

  const auto *root = parsed.getDynamicObject();
  const auto values = root->getProperty("entries");
  if (!values.isArray())
    return entries;

  for (const auto &entryValue : *values.getArray()) {
    if (!entryValue.isObject())
      continue;

    const auto *entryObject = entryValue.getDynamicObject();
    BenchmarkTimelineEntry entry;
    entry.timestampUtc = entryObject->getProperty("timestampUtc").toString();
    entry.passed = static_cast<bool>(entryObject->getProperty("passed"));
    entry.failedCaseCount = static_cast<int>(entryObject->getProperty("failedCaseCount"));
    entry.iterationCount = static_cast<int>(entryObject->getProperty("iterationCount"));
    entry.peakCpuLoadPercent = static_cast<double>(entryObject->getProperty("peakCpuLoadPercent"));
    entry.peakMaxProcessMilliseconds = static_cast<double>(entryObject->getProperty("peakMaxProcessMilliseconds"));
    entry.peakMaxBuildMilliseconds = static_cast<double>(entryObject->getProperty("peakMaxBuildMilliseconds"));
    entries.push_back(std::move(entry));
  }

  return entries;
}

DiagnosticSnapshot loadSummarySnapshot(const juce::String &title,
                                       const juce::File &summaryFile,
                                       DiagnosticCategory category) {
  DiagnosticSnapshot snapshot;
  snapshot.title = title;
  snapshot.category = category;
  snapshot.statusText = "MISSING";

  if (!summaryFile.existsAsFile()) {
    snapshot.summaryText = "No artifacts yet";
    snapshot.detailText = title + "\r\n"
                          "Status: MISSING\r\n"
                          "Artifact file not found: " + summaryFile.getFullPathName();
    snapshot.contextText = summaryFile.getParentDirectory().getFileName();
    snapshot.summaryValues["passed"] = "false";
    return snapshot;
  }

  snapshot.available = true;
  const auto text = normalizeLineEndings(summaryFile.loadFileAsString());
  snapshot.summaryValues = parseSummary(text);
  snapshot.passed = valueOrFallback(snapshot.summaryValues, "passed") == "true";
  snapshot.severity = snapshot.passed ? DiagnosticSeverity::Passing
                                      : DiagnosticSeverity::Failure;
  snapshot.statusText = snapshot.passed ? "PASS" : "FAIL";
  snapshot.artifactDirectory = valueOrFallback(snapshot.summaryValues, "artifactDirectory",
                                               summaryFile.getParentDirectory().getFullPathName());
  snapshot.summaryText = buildSummaryText(snapshot.summaryValues, snapshot);
  snapshot.contextText = buildContextText(snapshot);
  snapshot.detailText = title + "\r\n"
                        "Status: " + snapshot.statusText + "\r\n"
                        "Artifact: " + snapshot.artifactDirectory + "\r\n\r\n"
                        + text.trim();
  return snapshot;
}

DiagnosticSnapshot loadCompileSnapshot() {
  DiagnosticSnapshot snapshot;
  snapshot.title = "Runtime Compile Smoke";
  snapshot.category = DiagnosticCategory::Build;
  snapshot.statusText = "MISSING";

  const auto directory = findLatestCompileSmokeDirectory();
  if (directory.getFullPathName().isEmpty()) {
    snapshot.summaryText = "No artifacts yet";
    snapshot.detailText = "Artifact file not found: Builds/TeulCompileSmoke_*/artifact-bundle.json";
    snapshot.summaryValues["passed"] = "false";
    return snapshot;
  }

  const auto bundleFile = directory.getChildFile("artifact-bundle.json");
  if (!bundleFile.existsAsFile()) {
    snapshot.summaryText = "No artifacts yet";
    snapshot.detailText = "Artifact file not found: Builds/TeulCompileSmoke_*/artifact-bundle.json";
    snapshot.contextText = directory.getFileName();
    snapshot.summaryValues["passed"] = "false";
    return snapshot;
  }

  snapshot.available = true;
  const auto parsed = juce::JSON::parse(bundleFile);
  if (!parsed.isObject()) {
    snapshot.statusText = "FAIL";
    snapshot.severity = DiagnosticSeverity::Failure;
    snapshot.summaryText = "Artifact bundle parse failed";
    snapshot.detailText = "Failed to parse compile smoke artifact bundle.";
    snapshot.contextText = directory.getFileName();
    snapshot.summaryValues["passed"] = "false";
    snapshot.summaryValues["failureReason"] = "Artifact bundle parse failed";
    return snapshot;
  }

  const auto *object = parsed.getDynamicObject();
  snapshot.passed = static_cast<bool>(object->getProperty("passed"));
  snapshot.severity = snapshot.passed ? DiagnosticSeverity::Passing
                                      : DiagnosticSeverity::Failure;
  snapshot.statusText = snapshot.passed ? "PASS" : "FAIL";
  snapshot.artifactDirectory = object->getProperty("artifactDirectory").toString();
  snapshot.summaryValues["passed"] = snapshot.passed ? "true" : "false";
  snapshot.summaryValues["runtimeClassName"] = object->getProperty("runtimeClassName").toString();
  snapshot.summaryValues["exportExitCode"] = object->getProperty("exportExitCode").toString();
  snapshot.summaryValues["compileExitCode"] = object->getProperty("compileExitCode").toString();
  snapshot.summaryValues["failureReason"] = object->getProperty("failureReason").toString().trim();

  juce::StringArray parts;
  parts.add("runtime " + snapshot.summaryValues["runtimeClassName"]);
  parts.add("export " + snapshot.summaryValues["exportExitCode"]);
  parts.add("compile " + snapshot.summaryValues["compileExitCode"]);
  const auto failureReason = snapshot.summaryValues["failureReason"];
  if (!snapshot.passed && failureReason.isNotEmpty())
    parts.add(failureReason);
  snapshot.summaryText = joinParts(parts);
  snapshot.contextText = buildContextText(snapshot);

  snapshot.detailText = snapshot.title + "\r\n"
                        "Status: " + snapshot.statusText + "\r\n"
                        "Artifact: " + snapshot.artifactDirectory + "\r\n"
                        "Runtime Class: " + object->getProperty("runtimeClassName").toString() + "\r\n"
                        "Export Exit: " + object->getProperty("exportExitCode").toString() + "\r\n"
                        "Compile Exit: " + object->getProperty("compileExitCode").toString();
  if (failureReason.isNotEmpty())
    snapshot.detailText << "\r\nFailure: " << failureReason;

  return snapshot;
}

bool matchesFilter(const DiagnosticSnapshot &snapshot, int filterId) {
  switch (filterId) {
  case filterFailures:
    return snapshot.severity == DiagnosticSeverity::Failure;
  case filterPassing:
    return snapshot.severity == DiagnosticSeverity::Passing;
  case filterMissing:
    return snapshot.severity == DiagnosticSeverity::Missing;
  case filterAll:
  default:
    return true;
  }
}

juce::String filterLabelText(int filterId) {
  switch (filterId) {
  case filterFailures:
    return "Failures";
  case filterPassing:
    return "Passing";
  case filterMissing:
    return "Missing";
  case filterAll:
  default:
    return "All";
  }
}

juce::String buildSummaryLine(const std::vector<DiagnosticSnapshot> &snapshots,
                              int filterId) {
  int passCount = 0;
  int failCount = 0;
  int missingCount = 0;
  for (const auto &snapshot : snapshots) {
    switch (snapshot.severity) {
    case DiagnosticSeverity::Passing:
      ++passCount;
      break;
    case DiagnosticSeverity::Failure:
      ++failCount;
      break;
    case DiagnosticSeverity::Missing:
      ++missingCount;
      break;
    }
  }

  juce::StringArray parts;
  parts.add("Filter " + filterLabelText(filterId));
  parts.add("Pass " + juce::String(passCount));
  parts.add("Fail " + juce::String(failCount));
  parts.add("Missing " + juce::String(missingCount));
  return joinParts(parts);
}

juce::String compareStatusText(const DiagnosticSnapshot &selected,
                               const DiagnosticSnapshot &other) {
  juce::String text;
  text << "Compare: " << selected.title << " vs " << other.title << "\r\n";
  text << "Status: " << selected.statusText << " vs " << other.statusText;
  if (selected.passed != other.passed)
    text << "\r\nResult changed";
  return text;
}

juce::String buildDiffText(const DiagnosticSnapshot &selected,
                           const DiagnosticSnapshot *other) {
  if (other == nullptr) {
    return "Select a comparison target to see report deltas.\r\n\r\n"
           "Recommended comparisons:\r\n"
           "- Golden Audio vs Compiled Parity\r\n"
           "- Parity Smoke vs Parity Matrix\r\n"
           "- Stress Soak vs Benchmark Gate";
  }

  juce::StringArray lines;
  lines.add(compareStatusText(selected, *other));
  lines.add("Artifact: " + selected.contextText + " vs " + other->contextText);

  const std::pair<const char *, const char *> metrics[] = {
      {"passedCaseCount", "passed cases"},
      {"failedCaseCount", "failed cases"},
      {"totalCaseCount", "total cases"},
      {"iterationCount", "iterations"},
      {"totalSamples", "samples"},
      {"maxAbsoluteError", "max abs error"},
      {"rmsError", "rms error"},
      {"firstMismatchChannel", "first mismatch channel"},
      {"firstMismatchSample", "first mismatch sample"},
      {"compileExitCode", "compile exit"},
      {"exportExitCode", "export exit"}};

  bool addedMetric = false;
  lines.add("\r\nMetric Diff:");
  for (const auto &[keyChars, labelChars] : metrics) {
    const juce::String key(keyChars);
    const auto selectedValue = valueOrFallback(selected.summaryValues, key);
    const auto otherValue = valueOrFallback(other->summaryValues, key);
    if (selectedValue.isEmpty() && otherValue.isEmpty())
      continue;

    double selectedNumber = 0.0;
    double otherNumber = 0.0;
    const bool selectedIsNumber = tryParseDouble(selectedValue, selectedNumber);
    const bool otherIsNumber = tryParseDouble(otherValue, otherNumber);

    juce::String line = "- " + juce::String(labelChars) + ": " +
                        (selectedValue.isNotEmpty() ? selectedValue : "n/a") +
                        " vs " + (otherValue.isNotEmpty() ? otherValue : "n/a");
    if (selectedIsNumber && otherIsNumber)
      line << "  (delta " << formatDelta(selectedNumber - otherNumber) << ")";
    lines.add(line);
    addedMetric = true;
  }

  if (!addedMetric)
    lines.add("- No shared numeric metrics in the selected pair.");

  const auto selectedFailure = valueOrFallback(selected.summaryValues, "failureReason");
  const auto otherFailure = valueOrFallback(other->summaryValues, "failureReason");
  if (selectedFailure.isNotEmpty() || otherFailure.isNotEmpty()) {
    lines.add("\r\nFailure Reason:");
    lines.add("- selected: " + (selectedFailure.isNotEmpty() ? selectedFailure : "none"));
    lines.add("- compare: " + (otherFailure.isNotEmpty() ? otherFailure : "none"));
  }

  return lines.joinIntoString("\r\n");
}


class BenchmarkTimeline final : public juce::Component {
public:
  void setEntries(std::vector<BenchmarkTimelineEntry> newEntries) {
    entries = std::move(newEntries);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    g.setColour(TeulPalette::PanelBackgroundDeep());
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(TeulPalette::PanelStrokeStrong());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

    auto content = bounds.reduced(10.0f, 8.0f);
    if (entries.empty()) {
      g.setColour(TeulPalette::PanelTextFaint().withAlpha(0.88f));
      g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
      g.drawText("Run Benchmark Gate a few times to build recent history.",
                 content.toNearestInt(), juce::Justification::centredLeft, false);
      return;
    }

    g.setColour(TeulPalette::PanelTextMuted().withAlpha(0.90f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("Recent Benchmark History", content.removeFromTop(16).toNearestInt(),
               juce::Justification::centredLeft, false);
    content.removeFromTop(4.0f);

    struct LaneSpec {
      juce::String label;
      juce::Colour colour;
      std::function<double(const BenchmarkTimelineEntry &)> value;
    };
    const std::array<LaneSpec, 3> lanes = {{{
        "CPU %", juce::Colour(0xff60a5fa),
        [](const BenchmarkTimelineEntry &entry) { return entry.peakCpuLoadPercent; }},
        {"Process ms", juce::Colour(0xfff59e0b),
         [](const BenchmarkTimelineEntry &entry) {
           return entry.peakMaxProcessMilliseconds;
         }},
        {"Build ms", juce::Colour(0xffa78bfa),
         [](const BenchmarkTimelineEntry &entry) {
           return entry.peakMaxBuildMilliseconds;
         }}}};

    const float laneGap = 6.0f;
    const float laneHeight = (content.getHeight() - laneGap * 2.0f) / 3.0f;
    for (int laneIndex = 0; laneIndex < static_cast<int>(lanes.size()); ++laneIndex) {
      auto lane = content.removeFromTop(laneHeight);
      if (laneIndex + 1 < static_cast<int>(lanes.size()))
        content.removeFromTop(laneGap);

      g.setColour(juce::Colours::white.withAlpha(0.46f));
      g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
      g.drawText(lanes[static_cast<std::size_t>(laneIndex)].label,
                 lane.removeFromLeft(72.0f).toNearestInt(),
                 juce::Justification::centredLeft, false);

      const auto chart = lane.reduced(4.0f, 2.0f);
      g.setColour(juce::Colours::white.withAlpha(0.08f));
      g.drawRect(chart.toNearestInt());

      double maxValue = 0.0;
      for (const auto &entry : entries)
        maxValue = juce::jmax(maxValue,
                              lanes[static_cast<std::size_t>(laneIndex)].value(entry));
      if (maxValue <= 0.0)
        maxValue = 1.0;
      maxValue *= 1.08;

      juce::Path path;
      for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto proportion = entries.size() > 1
                                    ? static_cast<float>(index) /
                                          static_cast<float>(entries.size() - 1)
                                    : 0.0f;
        const auto x = chart.getX() + chart.getWidth() * proportion;
        const auto value = lanes[static_cast<std::size_t>(laneIndex)].value(entries[index]);
        const auto normalized = static_cast<float>(value / maxValue);
        const auto y = chart.getBottom() -
                       chart.getHeight() * juce::jlimit(0.0f, 1.0f, normalized);
        if (index == 0)
          path.startNewSubPath(x, y);
        else
          path.lineTo(x, y);

        const auto markerColour = entries[index].passed ? juce::Colour(0xff22c55e)
                                                        : juce::Colour(0xffef4444);
        g.setColour(markerColour.withAlpha(0.92f));
        g.fillEllipse(x - 1.75f, y - 1.75f, 3.5f, 3.5f);
      }

      g.setColour(lanes[static_cast<std::size_t>(laneIndex)].colour.withAlpha(0.92f));
      g.strokePath(path, juce::PathStrokeType(1.8f));

      const auto latestValue =
          lanes[static_cast<std::size_t>(laneIndex)].value(entries.back());
      g.setColour(lanes[static_cast<std::size_t>(laneIndex)].colour.withAlpha(0.88f));
      g.drawText(juce::String(latestValue, 3),
                 juce::Rectangle<int>(juce::roundToInt(chart.getRight()) - 56,
                                      juce::roundToInt(chart.getY()), 56,
                                      juce::roundToInt(chart.getHeight())),
                 juce::Justification::centredRight, false);
    }
  }

private:
  std::vector<BenchmarkTimelineEntry> entries;
};

juce::String extractLocationTypeHint(const juce::String &text) {
  const auto typeIndex = text.indexOfIgnoreCase(0, "type=");
  if (typeIndex < 0)
    return {};

  const auto valueStart = typeIndex + 5;
  int valueEnd = text.indexOfAnyOf(",]\r\n", valueStart);
  if (valueEnd < 0)
    valueEnd = text.length();
  return text.substring(valueStart, valueEnd).trim().unquoted();
}

juce::String buildFocusQuery(const DiagnosticSnapshot &snapshot) {
  auto query = extractLocationTypeHint(snapshot.detailText);
  if (query.isNotEmpty())
    return query;

  query = valueOrFallback(snapshot.summaryValues, "nodeTypeKey");
  return query.trim();
}



class CompareScreen final : public juce::Component {
public:
  void setComparison(const DiagnosticSnapshot *selected,
                     const DiagnosticSnapshot *other) {
    selectedTitle = selected != nullptr ? selected->title : juce::String();
    compareTitle = other != nullptr ? other->title : juce::String();
    selectedStatus = selected != nullptr ? selected->statusText : juce::String();
    compareStatus = other != nullptr ? other->statusText : juce::String();
    selectedPassed = selected != nullptr && selected->passed;
    comparePassed = other != nullptr && other->passed;
    emptyMessage.clear();
    rows.clear();

    if (selected == nullptr) {
      emptyMessage = "Select a diagnostic result to inspect comparison metrics.";
      repaint();
      return;
    }
    if (other == nullptr) {
      emptyMessage = "Pick a comparison target to populate runtime vs export metrics.";
      repaint();
      return;
    }

    auto addRow = [&](const juce::String &label,
                      const juce::String &selectedValue,
                      const juce::String &compareValue) {
      if (selectedValue.isEmpty() && compareValue.isEmpty())
        return;

      CompareRow row;
      row.label = label;
      row.selectedValue = selectedValue.isNotEmpty() ? selectedValue : "n/a";
      row.compareValue = compareValue.isNotEmpty() ? compareValue : "n/a";

      double selectedNumber = 0.0;
      double compareNumber = 0.0;
      const bool hasSelectedNumber = tryParseDouble(selectedValue, selectedNumber);
      const bool hasCompareNumber = tryParseDouble(compareValue, compareNumber);
      if (hasSelectedNumber && hasCompareNumber)
        row.deltaValue = formatDelta(selectedNumber - compareNumber);
      else if (selectedValue != compareValue)
        row.deltaValue = "changed";
      else
        row.deltaValue = "same";
      rows.push_back(std::move(row));
    };

    addRow("Graph", valueOrFallback(selected->summaryValues, "graphId"),
           valueOrFallback(other->summaryValues, "graphId"));
    addRow("Stimulus", valueOrFallback(selected->summaryValues, "stimulusId"),
           valueOrFallback(other->summaryValues, "stimulusId"));
    addRow("Profile", valueOrFallback(selected->summaryValues, "profileId"),
           valueOrFallback(other->summaryValues, "profileId"));
    addRow("Max Error", valueOrFallback(selected->summaryValues, "maxAbsoluteError"),
           valueOrFallback(other->summaryValues, "maxAbsoluteError"));
    addRow("RMS", valueOrFallback(selected->summaryValues, "rmsError"),
           valueOrFallback(other->summaryValues, "rmsError"));
    addRow("Failed Cases", valueOrFallback(selected->summaryValues, "failedCaseCount"),
           valueOrFallback(other->summaryValues, "failedCaseCount"));
    addRow("Iterations", valueOrFallback(selected->summaryValues, "iterationCount"),
           valueOrFallback(other->summaryValues, "iterationCount"));
    addRow("Compile Exit", valueOrFallback(selected->summaryValues, "compileExitCode"),
           valueOrFallback(other->summaryValues, "compileExitCode"));
    addRow("Export Exit", valueOrFallback(selected->summaryValues, "exportExitCode"),
           valueOrFallback(other->summaryValues, "exportExitCode"));

    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    g.setColour(TeulPalette::PanelBackgroundDeep());
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(TeulPalette::PanelStrokeStrong());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

    auto content = getLocalBounds().reduced(10, 8);
    if (emptyMessage.isNotEmpty()) {
      g.setColour(TeulPalette::PanelTextMuted().withAlpha(0.86f));
      g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
      g.drawText(emptyMessage, content, juce::Justification::centredLeft, true);
      return;
    }

    auto header = content.removeFromTop(24);
    auto selectedArea = header.removeFromLeft(header.getWidth() / 2).reduced(0, 2);
    auto compareArea = header.reduced(0, 2);
    drawHeaderChip(g, selectedArea, selectedTitle, selectedStatus, selectedPassed);
    drawHeaderChip(g, compareArea, compareTitle, compareStatus, comparePassed);

    content.removeFromTop(6);
    g.setColour(TeulPalette::PanelTextDisabled().withAlpha(0.82f));
    g.drawLine(static_cast<float>(content.getX()), static_cast<float>(content.getY()),
               static_cast<float>(content.getRight()), static_cast<float>(content.getY()));
    content.removeFromTop(8);

    if (rows.empty()) {
      g.setColour(TeulPalette::PanelTextMuted().withAlpha(0.86f));
      g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
      g.drawText("The selected pair does not expose shared compare metrics.",
                 content, juce::Justification::centredLeft, true);
      return;
    }

    const int rowHeight = 18;
    const int labelWidth = 96;
    const int deltaWidth = 88;
    for (const auto &row : rows) {
      if (content.getHeight() < rowHeight)
        break;
      auto rowArea = content.removeFromTop(rowHeight);
      auto deltaArea = rowArea.removeFromRight(deltaWidth);
      auto compareValueArea = rowArea.removeFromRight((rowArea.getWidth() - labelWidth) / 2);
      auto selectedValueArea = rowArea;
      auto labelArea = selectedValueArea.removeFromLeft(labelWidth);

      g.setColour(TeulPalette::PanelTextFaint().withAlpha(0.88f));
      g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
      g.drawText(row.label, labelArea, juce::Justification::centredLeft, false);

      g.setColour(TeulPalette::PanelTextStrong().withAlpha(0.82f));
      g.setFont(juce::FontOptions(10.0f, juce::Font::plain));
      g.drawText(row.selectedValue, selectedValueArea, juce::Justification::centredLeft, false);
      g.drawText(row.compareValue, compareValueArea, juce::Justification::centredLeft, false);

      const auto deltaColour = row.deltaValue.startsWithChar('+')
                                   ? juce::Colour(0xfff59e0b)
                                   : (row.deltaValue == "same" ? juce::Colour(0xff22c55e)
                                                               : juce::Colours::white.withAlpha(0.58f));
      g.setColour(deltaColour);
      g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
      g.drawText(row.deltaValue, deltaArea, juce::Justification::centredRight, false);
    }
  }

private:
  struct CompareRow {
    juce::String label;
    juce::String selectedValue;
    juce::String compareValue;
    juce::String deltaValue;
  };

  static void drawHeaderChip(juce::Graphics &g,
                             juce::Rectangle<int> area,
                             const juce::String &title,
                             const juce::String &status,
                             bool passed) {
    const auto accent = passed ? juce::Colour(0xff22c55e) : juce::Colour(0xffef4444);
    g.setColour(accent.withAlpha(0.14f));
    g.fillRoundedRectangle(area.toFloat(), 8.0f);
    g.setColour(accent.withAlpha(0.88f));
    g.drawRoundedRectangle(area.toFloat(), 8.0f, 1.0f);

    auto textArea = area.reduced(8, 3);
    auto statusArea = textArea.removeFromRight(58);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText(title, textArea, juce::Justification::centredLeft, true);
    g.setColour(accent.withAlpha(0.94f));
    g.drawText(status, statusArea, juce::Justification::centredRight, false);
  }

  juce::String selectedTitle;
  juce::String compareTitle;
  juce::String selectedStatus;
  juce::String compareStatus;
  juce::String emptyMessage;
  bool selectedPassed = false;
  bool comparePassed = false;
  std::vector<CompareRow> rows;
};

class DiagnosticCard final : public juce::Component {
public:
  DiagnosticCard() {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(summaryLabel);
    addAndMakeVisible(contextLabel);
    addAndMakeVisible(openButton);
    addAndMakeVisible(copyButton);

    titleLabel.setFont(juce::FontOptions(12.2f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.96f));
    titleLabel.setInterceptsMouseClicks(false, false);

    statusLabel.setFont(juce::FontOptions(9.8f, juce::Font::bold));
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setInterceptsMouseClicks(false, false);

    summaryLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.82f));
    summaryLabel.setFont(juce::FontOptions(10.2f, juce::Font::plain));
    summaryLabel.setInterceptsMouseClicks(false, false);

    contextLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.56f));
    contextLabel.setFont(juce::FontOptions(9.4f, juce::Font::plain));
    contextLabel.setInterceptsMouseClicks(false, false);

    openButton.setButtonText("Artifact");
    copyButton.setButtonText("Copy");
    openButton.onClick = [this] {
      if (onOpenArtifact)
        onOpenArtifact();
    };
    copyButton.onClick = [this] {
      if (onCopyDetails)
        onCopyDetails();
    };
  }

  void setSnapshot(const DiagnosticSnapshot &newSnapshot, bool shouldSelect) {
    snapshot = newSnapshot;
    isSelected = shouldSelect;

    titleLabel.setText(snapshot.title, juce::dontSendNotification);
    statusLabel.setText(snapshot.statusText, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, statusColour(snapshot));
    summaryLabel.setText(snapshot.summaryText, juce::dontSendNotification);
    contextLabel.setText(snapshot.contextText, juce::dontSendNotification);
    openButton.setEnabled(snapshot.artifactDirectory.isNotEmpty());
    copyButton.setEnabled(snapshot.detailText.isNotEmpty());
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    const auto accent = statusColour(snapshot);
    const auto background = isSelected ? TeulPalette::PanelBackgroundRaised()
                                       : TeulPalette::PanelBackgroundAlt();

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 9.0f);
    g.setColour(accent.withAlpha(isSelected ? 0.92f : 0.56f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 9.0f, isSelected ? 1.4f : 1.0f);

    const auto chip = juce::Rectangle<float>(bounds.getX() + 9.0f,
                                             bounds.getY() + 9.0f, 52.0f, 16.0f);
    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(chip, 8.0f);
    g.setColour(accent.withAlpha(0.92f));
    g.drawRoundedRectangle(chip, 8.0f, 1.0f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(9, 7);
    auto buttons = area.removeFromRight(126);
    openButton.setBounds(buttons.removeFromTop(22));
    buttons.removeFromTop(4);
    copyButton.setBounds(buttons.removeFromTop(22));

    auto topRow = area.removeFromTop(20);
    topRow.removeFromLeft(64);
    titleLabel.setBounds(topRow);
    statusLabel.setBounds(11, 9, 48, 16);
    auto summaryRow = area.removeFromTop(18);
    summaryLabel.setBounds(summaryRow);
    contextLabel.setBounds(area.removeFromTop(14));
  }

  void mouseUp(const juce::MouseEvent &event) override {
    if (event.mouseWasDraggedSinceMouseDown())
      return;
    if (onSelected)
      onSelected();
  }

  std::function<void()> onSelected;
  std::function<void()> onOpenArtifact;
  std::function<void()> onCopyDetails;

private:
  DiagnosticSnapshot snapshot;
  bool isSelected = false;
  juce::Label titleLabel;
  juce::Label statusLabel;
  juce::Label summaryLabel;
  juce::Label contextLabel;
  juce::TextButton openButton;
  juce::TextButton copyButton;
};

class DiagnosticsDrawerImpl final : public DiagnosticsDrawer,
                                    private juce::Timer {
public:
  DiagnosticsDrawerImpl() {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(summaryLabel);
    addAndMakeVisible(filterLabel);
    addAndMakeVisible(filterBox);
    addAndMakeVisible(compareLabel);
    addAndMakeVisible(compareBox);
    addAndMakeVisible(runGoldenVerifyButton);
    addAndMakeVisible(runParityMatrixButton);
    addAndMakeVisible(runCompiledParityButton);
    addAndMakeVisible(runCompileSmokeButton);
    addAndMakeVisible(runBenchmarkButton);
    addAndMakeVisible(actionStatusLabel);
    addAndMakeVisible(copySummaryButton);
    addAndMakeVisible(copyCompareButton);
    addAndMakeVisible(openSelectedArtifactButton);
    addAndMakeVisible(openBundleButton);
    addAndMakeVisible(focusJumpButton);
    addAndMakeVisible(refreshButton);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(listViewport);
    addAndMakeVisible(detailLabel);
    addAndMakeVisible(diffLabel);
    addAndMakeVisible(compareScreenLabel);
    addAndMakeVisible(compareScreen);
    addAndMakeVisible(timelineLabel);
    addAndMakeVisible(benchmarkTimeline);
    addAndMakeVisible(detailEditor);
    addAndMakeVisible(diffEditor);

    titleLabel.setText("Diagnostics Drawer", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.94f));
    titleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

    summaryLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.68f));
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    summaryLabel.setMinimumHorizontalScale(0.8f);

    filterLabel.setText("Severity", juce::dontSendNotification);
    filterLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.62f));
    filterLabel.setJustificationType(juce::Justification::centredLeft);
    compareLabel.setText("Compare", juce::dontSendNotification);
    compareLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.62f));
    compareLabel.setJustificationType(juce::Justification::centredLeft);

    filterBox.addItem("All", filterAll);
    filterBox.addItem("Failures", filterFailures);
    filterBox.addItem("Passing", filterPassing);
    filterBox.addItem("Missing", filterMissing);
    filterBox.setSelectedId(filterAll, juce::dontSendNotification);
    filterBox.onChange = [this] { rebuildVisibleEntries(); };

    compareBox.addItem("No compare", compareNone);
    compareBox.setSelectedId(compareNone, juce::dontSendNotification);
    compareBox.onChange = [this] { refreshDetailPanels(); };

    runGoldenVerifyButton.setButtonText("Golden Verify");
    runParityMatrixButton.setButtonText("Parity Matrix");
    runCompiledParityButton.setButtonText("Compiled Parity");
    runCompileSmokeButton.setButtonText("Compile Smoke");
    runBenchmarkButton.setButtonText("Benchmark");
    runGoldenVerifyButton.onClick = [this] { startActionProcess("Golden Verify", "--teul-phase7-golden-audio-verify"); };
    runParityMatrixButton.onClick = [this] { startActionProcess("Parity Matrix", "--teul-phase7-parity-matrix"); };
    runCompiledParityButton.onClick = [this] { startActionProcess("Compiled Parity", "--teul-phase7-compiled-runtime-parity"); };
    runCompileSmokeButton.onClick = [this] { startActionProcess("Compile Smoke", "--teul-phase7-runtime-compile-smoke"); };
    runBenchmarkButton.onClick = [this] { startActionProcess("Benchmark Gate", "--teul-phase7-benchmark-gate"); };

    actionStatusLabel.setText("Action Bar idle", juce::dontSendNotification);
    actionStatusLabel.setColour(juce::Label::textColourId,
                                juce::Colours::white.withAlpha(0.58f));
    actionStatusLabel.setJustificationType(juce::Justification::centredLeft);

    copySummaryButton.setButtonText("Copy Summary");
    copyCompareButton.setButtonText("Copy Compare");
    openSelectedArtifactButton.setButtonText("Open Artifact");
    openBundleButton.setButtonText("Open Bundle");
    focusJumpButton.setButtonText("Focus Node");
    copySummaryButton.onClick = [this] { copySelectedSummary(); };
    copyCompareButton.onClick = [this] { copyCompareSummary(); };
    openSelectedArtifactButton.onClick = [this] { revealSelectedArtifact(); };
    openBundleButton.onClick = [this] { revealSelectedBundle(); };
    focusJumpButton.onClick = [this] { triggerFocusJump(); };

    refreshButton.setButtonText("Refresh");
    closeButton.setButtonText("Hide");
    refreshButton.onClick = [this] { refreshArtifacts(true); };
    closeButton.onClick = [this] { setDrawerOpen(false); };

    listViewport.setViewedComponent(&listContent, false);
    listViewport.setScrollBarsShown(true, false);
    listViewport.setScrollBarThickness(10);

    emptyStateLabel.setText("No diagnostics match the current filter.",
                            juce::dontSendNotification);
    emptyStateLabel.setJustificationType(juce::Justification::centred);
    emptyStateLabel.setColour(juce::Label::textColourId,
                              juce::Colours::white.withAlpha(0.48f));
    listContent.addAndMakeVisible(emptyStateLabel);

    detailLabel.setText("Selected Report", juce::dontSendNotification);
    diffLabel.setText("Diff View", juce::dontSendNotification);
    compareScreenLabel.setText("Compare Screen", juce::dontSendNotification);
    timelineLabel.setText("Benchmark Timeline", juce::dontSendNotification);
    detailLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.72f));
    diffLabel.setColour(juce::Label::textColourId,
                        juce::Colours::white.withAlpha(0.72f));
    compareScreenLabel.setColour(juce::Label::textColourId,
                                 juce::Colours::white.withAlpha(0.72f));
    timelineLabel.setColour(juce::Label::textColourId,
                            juce::Colours::white.withAlpha(0.72f));

    configureReadOnlyEditor(detailEditor);
    configureReadOnlyEditor(diffEditor);
    updateActionButtons();
    updateShareButtons();

    setVisible(false);
  }

  void setLayoutChangedCallback(std::function<void()> callback) override {
    onLayoutChanged = std::move(callback);
  }

  void setFocusRequestHandler(
      std::function<bool(const juce::String &, const juce::String &)> handler) override {
    focusRequestHandler = std::move(handler);
    updateShareButtons();
  }

  bool isDrawerOpen() const noexcept override { return isVisible(); }

  void setDrawerOpen(bool shouldOpen) override {
    if (isVisible() == shouldOpen)
      return;

    setVisible(shouldOpen);
    if (shouldOpen) {
      refreshArtifacts(true);
      startTimer(1000);
    } else {
      stopTimer();
    }

    if (onLayoutChanged)
      onLayoutChanged();
  }

  void refreshArtifacts(bool force = false) override {
    const auto now = juce::Time::getCurrentTime();
    if (!force && lastRefreshTime != juce::Time() &&
        (now - lastRefreshTime).inMilliseconds() < 900) {
      return;
    }

    snapshots.clear();
    snapshots.push_back(loadSummarySnapshot(
        "Golden Audio",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("GoldenAudio")
            .getChildFile("RepresentativePrimary_verify")
            .getChildFile("golden-suite-summary.txt"),
        DiagnosticCategory::Verification));
    snapshots.push_back(loadSummarySnapshot(
        "Compiled Parity",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("CompiledRuntimeParity")
            .getChildFile("RepresentativePrimary")
            .getChildFile("compiled-runtime-parity-summary.txt"),
        DiagnosticCategory::Verification));
    snapshots.push_back(loadSummarySnapshot(
        "Parity Smoke",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("EditableRoundTrip")
            .getChildFile("G1_S1_primary")
            .getChildFile("parity-summary.txt"),
        DiagnosticCategory::Verification));
    snapshots.push_back(loadSummarySnapshot(
        "Parity Matrix",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("EditableRoundTrip")
            .getChildFile("RepresentativeMatrix_primary")
            .getChildFile("matrix-summary.txt"),
        DiagnosticCategory::Verification));
    snapshots.push_back(loadSummarySnapshot(
        "Stress Soak",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("StressSoak")
            .getChildFile("representative_stress_primary")
            .getChildFile("stress-summary.txt"),
        DiagnosticCategory::Performance));
    snapshots.push_back(loadSummarySnapshot(
        "Benchmark Gate",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("Benchmark")
            .getChildFile("representative_benchmark_primary")
            .getChildFile("benchmark-summary.txt"),
        DiagnosticCategory::Performance));
    snapshots.push_back(loadCompileSnapshot());

    summaryLabel.setText(buildSummaryLine(snapshots, filterBox.getSelectedId()),
                         juce::dontSendNotification);

    overallAccent = juce::Colour(0xff22c55e);
    for (const auto &snapshot : snapshots) {
      if (snapshot.severity == DiagnosticSeverity::Failure) {
        overallAccent = juce::Colour(0xffef4444);
        break;
      }
      if (snapshot.severity == DiagnosticSeverity::Missing)
        overallAccent = juce::Colour(0xff94a3b8);
    }

    benchmarkTimeline.setEntries(loadBenchmarkTimelineEntries());
    rebuildVisibleEntries();
    lastRefreshTime = now;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setGradientFill(juce::ColourGradient(TeulPalette::PanelBackgroundDeep(),
                                           bounds.getCentreX(), bounds.getY(),
                                           TeulPalette::PanelBackgroundRaised(),
                                           bounds.getCentreX(), bounds.getBottom(),
                                           false));
    g.fillRoundedRectangle(bounds, 11.0f);
    g.setColour(overallAccent.withAlpha(0.32f));
    g.drawRoundedRectangle(bounds, 11.0f, 1.0f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(10);
    auto header = area.removeFromTop(24);
    auto buttons = header.removeFromRight(128);
    refreshButton.setBounds(buttons.removeFromLeft(60));
    buttons.removeFromLeft(8);
    closeButton.setBounds(buttons.removeFromLeft(60));
    titleLabel.setBounds(header);

    auto summaryArea = area.removeFromTop(20);
    summaryLabel.setBounds(summaryArea);

    auto controlArea = area.removeFromTop(24);
    filterLabel.setBounds(controlArea.removeFromLeft(52));
    controlArea.removeFromLeft(4);
    filterBox.setBounds(controlArea.removeFromLeft(124));
    controlArea.removeFromLeft(10);
    compareLabel.setBounds(controlArea.removeFromLeft(52));
    controlArea.removeFromLeft(4);
    compareBox.setBounds(controlArea.removeFromLeft(208));

    area.removeFromTop(4);
    auto actionRow = area.removeFromTop(24);
    runGoldenVerifyButton.setBounds(actionRow.removeFromLeft(100));
    actionRow.removeFromLeft(4);
    runParityMatrixButton.setBounds(actionRow.removeFromLeft(96));
    actionRow.removeFromLeft(4);
    runCompiledParityButton.setBounds(actionRow.removeFromLeft(106));
    actionRow.removeFromLeft(4);
    runCompileSmokeButton.setBounds(actionRow.removeFromLeft(102));
    actionRow.removeFromLeft(4);
    runBenchmarkButton.setBounds(actionRow.removeFromLeft(88));

    area.removeFromTop(3);
    auto actionStatusArea = area.removeFromTop(22);
    auto shareButtons = actionStatusArea.removeFromRight(500);
    copySummaryButton.setBounds(shareButtons.removeFromLeft(96));
    shareButtons.removeFromLeft(4);
    copyCompareButton.setBounds(shareButtons.removeFromLeft(96));
    shareButtons.removeFromLeft(4);
    openSelectedArtifactButton.setBounds(shareButtons.removeFromLeft(100));
    shareButtons.removeFromLeft(4);
    openBundleButton.setBounds(shareButtons.removeFromLeft(96));
    shareButtons.removeFromLeft(4);
    focusJumpButton.setBounds(shareButtons.removeFromLeft(100));
    actionStatusLabel.setBounds(actionStatusArea);

    area.removeFromTop(6);
    timelineLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(3);
    benchmarkTimeline.setBounds(area.removeFromTop(88));

    area.removeFromTop(6);
    auto listArea = area.removeFromTop(juce::roundToInt(area.getHeight() * 0.22f));
    listViewport.setBounds(listArea);

    area.removeFromTop(6);
    compareScreenLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(3);
    compareScreen.setBounds(area.removeFromTop(116));

    area.removeFromTop(6);
    auto bottomArea = area;
    auto left = bottomArea.removeFromLeft(bottomArea.getWidth() / 2);
    left.removeFromRight(4);
    bottomArea.removeFromLeft(4);

    detailLabel.setBounds(left.removeFromTop(16));
    left.removeFromTop(3);
    detailEditor.setBounds(left);

    diffLabel.setBounds(bottomArea.removeFromTop(16));
    bottomArea.removeFromTop(3);
    diffEditor.setBounds(bottomArea);

    layoutVisibleEntries();
  }

private:
  void timerCallback() override { pollRunningAction(); refreshArtifacts(false); }

  static void configureReadOnlyEditor(juce::TextEditor &editor) {
    editor.setMultiLine(true);
    editor.setReadOnly(true);
    editor.setScrollbarsShown(true);
    editor.setColour(juce::TextEditor::backgroundColourId,
                     juce::Colour(0xff0f172a));
    editor.setColour(juce::TextEditor::textColourId,
                     juce::Colours::white.withAlpha(0.88f));
    editor.setColour(juce::TextEditor::outlineColourId,
                     juce::Colour(0xff334155));
    editor.setFont(juce::FontOptions(11.0f, juce::Font::plain));
  }

  void updateActionButtons() {
    const bool running = runningActionProcess != nullptr;
    runGoldenVerifyButton.setEnabled(!running);
    runParityMatrixButton.setEnabled(!running);
    runCompiledParityButton.setEnabled(!running);
    runCompileSmokeButton.setEnabled(!running);
    runBenchmarkButton.setEnabled(!running);
  }

  const DiagnosticSnapshot *getSelectedSnapshot() const {
    if (selectedSnapshotIndex < 0 ||
        selectedSnapshotIndex >= static_cast<int>(snapshots.size()))
      return nullptr;
    return &snapshots[static_cast<std::size_t>(selectedSnapshotIndex)];
  }

  const DiagnosticSnapshot *getCompareSnapshot() const {
    const auto compareId = compareBox.getSelectedId();
    if (compareId < compareBaseId)
      return nullptr;

    const auto compareIndex = compareId - compareBaseId;
    if (compareIndex < 0 || compareIndex >= static_cast<int>(snapshots.size()))
      return nullptr;
    return &snapshots[static_cast<std::size_t>(compareIndex)];
  }

  juce::File getSelectedBundleFile() const {
    const auto *snapshot = getSelectedSnapshot();
    if (snapshot == nullptr || snapshot->artifactDirectory.isEmpty())
      return {};
    return juce::File(snapshot->artifactDirectory).getChildFile("artifact-bundle.json");
  }

  juce::String buildSelectedSummaryText() const {
    const auto *snapshot = getSelectedSnapshot();
    if (snapshot == nullptr)
      return "No diagnostic selected.";

    juce::String text;
    text << snapshot->title << "\r\n";
    text << "Status: " << snapshot->statusText << "\r\n";
    if (snapshot->summaryText.isNotEmpty())
      text << "Summary: " << snapshot->summaryText << "\r\n";
    if (snapshot->contextText.isNotEmpty())
      text << "Context: " << snapshot->contextText << "\r\n";
    if (snapshot->artifactDirectory.isNotEmpty())
      text << "Artifact: " << snapshot->artifactDirectory << "\r\n";
    return text.trimEnd();
  }

  juce::String buildCompareShareText() const {
    const auto *selected = getSelectedSnapshot();
    const auto *other = getCompareSnapshot();
    if (selected == nullptr)
      return "No diagnostic selected.";

    juce::String text = buildSelectedSummaryText();
    text << "\r\n";
    text << buildDiffText(*selected, other);
    return text.trimEnd();
  }

  void updateShareButtons() {
    const auto *selected = getSelectedSnapshot();
    copySummaryButton.setEnabled(selected != nullptr);
    openSelectedArtifactButton.setEnabled(selected != nullptr &&
                                          selected->artifactDirectory.isNotEmpty());

    const auto bundleFile = getSelectedBundleFile();
    openBundleButton.setEnabled(bundleFile.existsAsFile());
    copyCompareButton.setEnabled(selected != nullptr && getCompareSnapshot() != nullptr);
    focusJumpButton.setEnabled(selected != nullptr &&
                               focusRequestHandler != nullptr &&
                               buildFocusQuery(*selected).isNotEmpty());
  }

  void copySelectedSummary() {
    juce::SystemClipboard::copyTextToClipboard(buildSelectedSummaryText());
  }

  void copyCompareSummary() {
    juce::SystemClipboard::copyTextToClipboard(buildCompareShareText());
  }

  void revealSelectedArtifact() {
    const auto *selected = getSelectedSnapshot();
    if (selected == nullptr || selected->artifactDirectory.isEmpty())
      return;
    juce::File(selected->artifactDirectory).revealToUser();
  }

  void revealSelectedBundle() {
    const auto bundleFile = getSelectedBundleFile();
    if (!bundleFile.existsAsFile())
      return;
    bundleFile.revealToUser();
  }

  void triggerFocusJump() {
    const auto *selected = getSelectedSnapshot();
    if (selected == nullptr || focusRequestHandler == nullptr)
      return;

    const auto graphId = valueOrFallback(selected->summaryValues, "graphId");
    const auto query = buildFocusQuery(*selected);
    if (query.isEmpty())
      return;

    const bool focused = focusRequestHandler(graphId, query);
    actionStatusLabel.setText(
        focused ? ("Focus jump: " + query)
                : ("Focus jump failed: " + query),
        juce::dontSendNotification);
  }

  void startActionProcess(const juce::String &actionName,
                          const juce::String &commandArg) {
    if (runningActionProcess != nullptr)
      return;

    const auto executable =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    if (!executable.existsAsFile()) {
      actionStatusLabel.setText("Action launch failed: missing executable",
                                juce::dontSendNotification);
      return;
    }

    auto process = std::make_unique<juce::ChildProcess>();
    juce::StringArray arguments;
    arguments.add(executable.getFullPathName());
    arguments.add(commandArg);
    if (!process->start(arguments)) {
      actionStatusLabel.setText("Action launch failed: " + actionName,
                                juce::dontSendNotification);
      return;
    }

    runningActionName = actionName;
    runningActionCommand = commandArg;
    runningActionStart = juce::Time::getCurrentTime();
    runningActionProcess = std::move(process);
    actionStatusLabel.setText("Running " + actionName + "...",
                              juce::dontSendNotification);
    updateActionButtons();
  }

  void pollRunningAction() {
    if (runningActionProcess == nullptr)
      return;

    if (runningActionProcess->isRunning())
      return;

    const auto output = normalizeLineEndings(runningActionProcess->readAllProcessOutput()).trim();
    const auto exitCode = runningActionProcess->getExitCode();
    const auto elapsedMs =
        (juce::Time::getCurrentTime() - runningActionStart).inMilliseconds();
    const bool passed = exitCode == 0;

    actionStatusLabel.setText(
        runningActionName + (passed ? " PASS" : " FAIL") +
            "  |  " + juce::String(elapsedMs) + " ms",
        juce::dontSendNotification);

    if (output.isNotEmpty()) {
      diffEditor.setText(runningActionName + " output\r\n\r\n" + output,
                         false);
    }

    runningActionProcess.reset();
    runningActionName.clear();
    runningActionCommand.clear();
    updateActionButtons();
    updateShareButtons();
    refreshArtifacts(true);
  }

  std::vector<int> filteredSnapshotIndices() const {
    std::vector<int> indices;
    const auto filterId = filterBox.getSelectedId();
    for (int index = 0; index < static_cast<int>(snapshots.size()); ++index) {
      if (matchesFilter(snapshots[static_cast<std::size_t>(index)], filterId))
        indices.push_back(index);
    }
    return indices;
  }

  void rebuildVisibleEntries() {
    for (auto &label : sectionLabels)
      label.reset();
    sectionLabels.clear();
    for (auto &card : cards)
      card.reset();
    cards.clear();
    visibleSnapshotIndices.clear();

    visibleSnapshotIndices = filteredSnapshotIndices();
    emptyStateLabel.setVisible(visibleSnapshotIndices.empty());

    if (visibleSnapshotIndices.empty()) {
      selectedSnapshotIndex = -1;
      detailEditor.setText("No diagnostics match the current filter.", false);
      diffEditor.setText("No diagnostics match the current filter.", false);
      rebuildCompareOptions();
      layoutVisibleEntries();
      return;
    }

    const bool selectionStillVisible = std::find(visibleSnapshotIndices.begin(),
                                                 visibleSnapshotIndices.end(),
                                                 selectedSnapshotIndex) != visibleSnapshotIndices.end();
    if (!selectionStillVisible) {
      selectedSnapshotIndex = -1;
      for (const auto snapshotIndex : visibleSnapshotIndices) {
        if (snapshots[static_cast<std::size_t>(snapshotIndex)].severity == DiagnosticSeverity::Failure) {
          selectedSnapshotIndex = snapshotIndex;
          break;
        }
      }
      if (selectedSnapshotIndex < 0)
        selectedSnapshotIndex = visibleSnapshotIndices.front();
    }

    DiagnosticCategory previousCategory = DiagnosticCategory::Verification;
    bool hasPreviousCategory = false;
    for (const auto snapshotIndex : visibleSnapshotIndices) {
      const auto &snapshot = snapshots[static_cast<std::size_t>(snapshotIndex)];
      if (!hasPreviousCategory || snapshot.category != previousCategory) {
        auto label = std::make_unique<juce::Label>();
        label->setText(categoryTitle(snapshot.category), juce::dontSendNotification);
        label->setFont(juce::FontOptions(11.2f, juce::Font::bold));
        label->setColour(juce::Label::textColourId,
                         categoryColour(snapshot.category).withAlpha(0.92f));
        label->setJustificationType(juce::Justification::centredLeft);
        label->setInterceptsMouseClicks(false, false);
        listContent.addAndMakeVisible(*label);
        sectionLabels.push_back(std::move(label));
        previousCategory = snapshot.category;
        hasPreviousCategory = true;
      }

      auto card = std::make_unique<DiagnosticCard>();
      card->setSnapshot(snapshot, snapshotIndex == selectedSnapshotIndex);
      card->onSelected = [this, snapshotIndex] { selectSnapshot(snapshotIndex); };
      card->onOpenArtifact = [this, snapshotIndex] { revealArtifact(snapshotIndex); };
      card->onCopyDetails = [this, snapshotIndex] { copyDetails(snapshotIndex); };
      listContent.addAndMakeVisible(*card);
      cards.push_back(std::move(card));
    }

    rebuildCompareOptions();
    refreshDetailPanels();
    layoutVisibleEntries();
  }

  void layoutVisibleEntries() {
    const int width = juce::jmax(1, listViewport.getWidth() - 12);
    int y = 0;

    if (visibleSnapshotIndices.empty()) {
      emptyStateLabel.setBounds(0, 10, width, 24);
      listContent.setSize(width, 48);
      return;
    }

    std::size_t headerIndex = 0;
    DiagnosticCategory previousCategory = DiagnosticCategory::Verification;
    bool hasPreviousCategory = false;
    for (std::size_t cardIndex = 0; cardIndex < cards.size(); ++cardIndex) {
      const auto snapshotIndex = visibleSnapshotIndices[cardIndex];
      const auto category = snapshots[static_cast<std::size_t>(snapshotIndex)].category;
      if (!hasPreviousCategory || category != previousCategory) {
        auto *header = sectionLabels[headerIndex++].get();
        header->setBounds(0, y, width, 18);
        y += 22;
        previousCategory = category;
        hasPreviousCategory = true;
      }

      cards[cardIndex]->setBounds(0, y, width, 64);
      y += 70;
    }

    listContent.setSize(width, y);
  }

  void rebuildCompareOptions() {
    const auto previousSelection = compareBox.getSelectedId();
    compareBox.clear(juce::dontSendNotification);
    compareBox.addItem("No compare", compareNone);

    if (selectedSnapshotIndex < 0 ||
        selectedSnapshotIndex >= static_cast<int>(snapshots.size())) {
      compareBox.setSelectedId(compareNone, juce::dontSendNotification);
      return;
    }

    const auto selectedCategory =
        snapshots[static_cast<std::size_t>(selectedSnapshotIndex)].category;
    int preferredSelection = compareNone;
    for (int index = 0; index < static_cast<int>(snapshots.size()); ++index) {
      if (index == selectedSnapshotIndex)
        continue;

      const auto itemId = compareBaseId + index;
      juce::String label = snapshots[static_cast<std::size_t>(index)].title +
                           " [" + categoryTitle(snapshots[static_cast<std::size_t>(index)].category) + "]";
      compareBox.addItem(label, itemId);
      if (itemId == previousSelection)
        preferredSelection = itemId;
      else if (preferredSelection == compareNone &&
               snapshots[static_cast<std::size_t>(index)].category == selectedCategory)
        preferredSelection = itemId;
    }

    compareBox.setSelectedId(preferredSelection, juce::dontSendNotification);
  }

  void refreshDetailPanels() {
    if (selectedSnapshotIndex < 0 ||
        selectedSnapshotIndex >= static_cast<int>(snapshots.size())) {
      detailEditor.setText("No diagnostic selected.", false);
      diffEditor.setText("No diagnostic selected.", false);
      compareScreen.setComparison(nullptr, nullptr);
      updateShareButtons();
      return;
    }

    const auto &selected = snapshots[static_cast<std::size_t>(selectedSnapshotIndex)];
    detailEditor.setText(selected.detailText, false);

    DiagnosticSnapshot *compareSnapshot = nullptr;
    const auto compareId = compareBox.getSelectedId();
    if (compareId >= compareBaseId) {
      const auto compareIndex = compareId - compareBaseId;
      if (compareIndex >= 0 && compareIndex < static_cast<int>(snapshots.size()))
        compareSnapshot = &snapshots[static_cast<std::size_t>(compareIndex)];
    }

    compareScreen.setComparison(&selected, compareSnapshot);
    diffEditor.setText(buildDiffText(selected, compareSnapshot), false);
    updateShareButtons();
  }

  void selectSnapshot(int snapshotIndex) {
    if (snapshotIndex < 0 || snapshotIndex >= static_cast<int>(snapshots.size()))
      return;

    selectedSnapshotIndex = snapshotIndex;
    for (std::size_t index = 0; index < cards.size(); ++index) {
      const auto visibleSnapshotIndex = visibleSnapshotIndices[index];
      cards[index]->setSnapshot(snapshots[static_cast<std::size_t>(visibleSnapshotIndex)],
                                visibleSnapshotIndex == selectedSnapshotIndex);
    }
    rebuildCompareOptions();
    refreshDetailPanels();
    updateShareButtons();
  }

  void revealArtifact(int snapshotIndex) {
    if (snapshotIndex < 0 || snapshotIndex >= static_cast<int>(snapshots.size()))
      return;

    const auto artifactPath = snapshots[static_cast<std::size_t>(snapshotIndex)].artifactDirectory;
    if (artifactPath.isEmpty())
      return;

    juce::File(artifactPath).revealToUser();
  }

  void copyDetails(int snapshotIndex) {
    if (snapshotIndex < 0 || snapshotIndex >= static_cast<int>(snapshots.size()))
      return;

    juce::SystemClipboard::copyTextToClipboard(
        snapshots[static_cast<std::size_t>(snapshotIndex)].detailText);
  }

  juce::Label titleLabel;
  juce::Label summaryLabel;
  juce::Label filterLabel;
  juce::ComboBox filterBox;
  juce::Label compareLabel;
  juce::ComboBox compareBox;
  juce::TextButton runGoldenVerifyButton;
  juce::TextButton runParityMatrixButton;
  juce::TextButton runCompiledParityButton;
  juce::TextButton runCompileSmokeButton;
  juce::TextButton runBenchmarkButton;
  juce::Label actionStatusLabel;
  juce::TextButton copySummaryButton;
  juce::TextButton copyCompareButton;
  juce::TextButton openSelectedArtifactButton;
  juce::TextButton openBundleButton;
  juce::TextButton focusJumpButton;
  juce::TextButton refreshButton;
  juce::TextButton closeButton;
  juce::Viewport listViewport;
  juce::Component listContent;
  juce::Label emptyStateLabel;
  juce::Label detailLabel;
  juce::Label diffLabel;
  juce::Label compareScreenLabel;
  juce::Label timelineLabel;
  CompareScreen compareScreen;
  BenchmarkTimeline benchmarkTimeline;
  juce::TextEditor detailEditor;
  juce::TextEditor diffEditor;
  juce::Colour overallAccent = juce::Colour(0xff22c55e);
  juce::Time lastRefreshTime;
  juce::Time runningActionStart;
  std::unique_ptr<juce::ChildProcess> runningActionProcess;
  juce::String runningActionName;
  juce::String runningActionCommand;
  std::function<void()> onLayoutChanged;
  std::function<bool(const juce::String &, const juce::String &)> focusRequestHandler;
  std::vector<DiagnosticSnapshot> snapshots;
  std::vector<int> visibleSnapshotIndices;
  std::vector<std::unique_ptr<juce::Label>> sectionLabels;
  std::vector<std::unique_ptr<DiagnosticCard>> cards;
  int selectedSnapshotIndex = -1;
};

} // namespace

std::unique_ptr<DiagnosticsDrawer> DiagnosticsDrawer::create() {
  return std::make_unique<DiagnosticsDrawerImpl>();
}

} // namespace Teul
