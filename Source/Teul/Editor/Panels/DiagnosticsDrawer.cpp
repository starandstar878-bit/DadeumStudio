#include "Teul/Editor/Panels/DiagnosticsDrawer.h"

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
    return juce::Colour(0xff60a5fa);
  case DiagnosticCategory::Performance:
    return juce::Colour(0xffa78bfa);
  case DiagnosticCategory::Build:
    return juce::Colour(0xfff59e0b);
  }

  return juce::Colour(0xff94a3b8);
}

juce::Colour statusColour(const DiagnosticSnapshot &snapshot) {
  switch (snapshot.severity) {
  case DiagnosticSeverity::Passing:
    return juce::Colour(0xff22c55e);
  case DiagnosticSeverity::Failure:
    return juce::Colour(0xffef4444);
  case DiagnosticSeverity::Missing:
    return juce::Colour(0xff94a3b8);
  }

  return juce::Colour(0xff94a3b8);
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

class DiagnosticCard final : public juce::Component {
public:
  DiagnosticCard() {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(summaryLabel);
    addAndMakeVisible(contextLabel);
    addAndMakeVisible(openButton);
    addAndMakeVisible(copyButton);

    titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.96f));
    titleLabel.setInterceptsMouseClicks(false, false);

    statusLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setInterceptsMouseClicks(false, false);

    summaryLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.82f));
    summaryLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    summaryLabel.setInterceptsMouseClicks(false, false);

    contextLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.56f));
    contextLabel.setFont(juce::FontOptions(10.0f, juce::Font::plain));
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
    const auto background = isSelected ? juce::Colour(0xff172033)
                                       : juce::Colour(0xff101827);

    g.setColour(background);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(accent.withAlpha(isSelected ? 0.92f : 0.56f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, isSelected ? 1.5f : 1.0f);

    const auto chip = juce::Rectangle<float>(bounds.getX() + 10.0f,
                                             bounds.getY() + 10.0f, 56.0f, 18.0f);
    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(chip, 9.0f);
    g.setColour(accent.withAlpha(0.92f));
    g.drawRoundedRectangle(chip, 9.0f, 1.0f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(10, 8);
    auto buttons = area.removeFromRight(138);
    openButton.setBounds(buttons.removeFromTop(24));
    buttons.removeFromTop(6);
    copyButton.setBounds(buttons.removeFromTop(24));

    auto topRow = area.removeFromTop(22);
    topRow.removeFromLeft(70);
    titleLabel.setBounds(topRow);
    statusLabel.setBounds(12, 10, 52, 18);
    auto summaryRow = area.removeFromTop(20);
    summaryLabel.setBounds(summaryRow);
    contextLabel.setBounds(area.removeFromTop(16));
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
    addAndMakeVisible(refreshButton);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(listViewport);
    addAndMakeVisible(detailLabel);
    addAndMakeVisible(diffLabel);
    addAndMakeVisible(detailEditor);
    addAndMakeVisible(diffEditor);

    titleLabel.setText("Diagnostics Drawer", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.94f));
    titleLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));

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
    detailLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.72f));
    diffLabel.setColour(juce::Label::textColourId,
                        juce::Colours::white.withAlpha(0.72f));

    configureReadOnlyEditor(detailEditor);
    configureReadOnlyEditor(diffEditor);

    setVisible(false);
  }

  void setLayoutChangedCallback(std::function<void()> callback) override {
    onLayoutChanged = std::move(callback);
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

    rebuildVisibleEntries();
    lastRefreshTime = now;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff08111d),
                                           bounds.getCentreX(), bounds.getY(),
                                           juce::Colour(0xff0f172a),
                                           bounds.getCentreX(), bounds.getBottom(),
                                           false));
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(overallAccent.withAlpha(0.32f));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    auto header = area.removeFromTop(26);
    auto buttons = header.removeFromRight(140);
    refreshButton.setBounds(buttons.removeFromLeft(66));
    buttons.removeFromLeft(8);
    closeButton.setBounds(buttons.removeFromLeft(66));
    titleLabel.setBounds(header);

    auto summaryArea = area.removeFromTop(24);
    summaryLabel.setBounds(summaryArea);

    auto controlArea = area.removeFromTop(26);
    filterLabel.setBounds(controlArea.removeFromLeft(56));
    controlArea.removeFromLeft(6);
    filterBox.setBounds(controlArea.removeFromLeft(132));
    controlArea.removeFromLeft(12);
    compareLabel.setBounds(controlArea.removeFromLeft(56));
    controlArea.removeFromLeft(6);
    compareBox.setBounds(controlArea.removeFromLeft(220));

    area.removeFromTop(8);
    auto listArea = area.removeFromTop(juce::roundToInt(area.getHeight() * 0.42f));
    listViewport.setBounds(listArea);

    area.removeFromTop(8);
    auto bottomArea = area;
    auto left = bottomArea.removeFromLeft(bottomArea.getWidth() / 2);
    left.removeFromRight(4);
    bottomArea.removeFromLeft(4);

    detailLabel.setBounds(left.removeFromTop(18));
    left.removeFromTop(4);
    detailEditor.setBounds(left);

    diffLabel.setBounds(bottomArea.removeFromTop(18));
    bottomArea.removeFromTop(4);
    diffEditor.setBounds(bottomArea);

    layoutVisibleEntries();
  }

private:
  void timerCallback() override { refreshArtifacts(false); }

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
    editor.setFont(juce::FontOptions(12.0f, juce::Font::plain));
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
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
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
      emptyStateLabel.setBounds(0, 12, width, 28);
      listContent.setSize(width, 56);
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
        header->setBounds(0, y, width, 20);
        y += 24;
        previousCategory = category;
        hasPreviousCategory = true;
      }

      cards[cardIndex]->setBounds(0, y, width, 72);
      y += 80;
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

    diffEditor.setText(buildDiffText(selected, compareSnapshot), false);
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
  juce::TextButton refreshButton;
  juce::TextButton closeButton;
  juce::Viewport listViewport;
  juce::Component listContent;
  juce::Label emptyStateLabel;
  juce::Label detailLabel;
  juce::Label diffLabel;
  juce::TextEditor detailEditor;
  juce::TextEditor diffEditor;
  juce::Colour overallAccent = juce::Colour(0xff22c55e);
  juce::Time lastRefreshTime;
  std::function<void()> onLayoutChanged;
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
