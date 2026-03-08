#include "Teul/Editor/Panels/DiagnosticsDrawer.h"

#include <map>

namespace Teul {
namespace {

struct DiagnosticSnapshot {
  juce::String title;
  juce::String statusText;
  juce::String artifactDirectory;
  juce::String detailText;
  bool passed = false;
  bool available = false;
};

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

juce::Colour statusColour(const DiagnosticSnapshot &snapshot) {
  if (!snapshot.available)
    return juce::Colour(0xff94a3b8);
  return snapshot.passed ? juce::Colour(0xff22c55e) : juce::Colour(0xffef4444);
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
                                       const juce::String &primaryMetricLine) {
  DiagnosticSnapshot snapshot;
  snapshot.title = title;
  if (!summaryFile.existsAsFile()) {
    snapshot.statusText = "No artifacts yet";
    snapshot.detailText = "Artifact file not found: " + summaryFile.getFullPathName();
    return snapshot;
  }

  snapshot.available = true;
  const auto text = normalizeLineEndings(summaryFile.loadFileAsString());
  const auto values = parseSummary(text);
  snapshot.passed = valueOrFallback(values, "passed") == "true";
  snapshot.artifactDirectory = valueOrFallback(values, "artifactDirectory",
                                               summaryFile.getParentDirectory().getFullPathName());
  snapshot.statusText = snapshot.passed ? "PASS" : "FAIL";
  snapshot.detailText = title + "\r\n"
                        "Status: " + snapshot.statusText + "\r\n"
                        "Artifact: " + snapshot.artifactDirectory + "\r\n";
  if (primaryMetricLine.isNotEmpty())
    snapshot.detailText << primaryMetricLine << "\r\n";
  snapshot.detailText << "\r\n" << text.trim();
  return snapshot;
}

DiagnosticSnapshot loadCompileSnapshot() {
  DiagnosticSnapshot snapshot;
  snapshot.title = "Runtime Compile Smoke";

  const auto directory = findLatestCompileSmokeDirectory();
  const auto bundleFile = directory.getChildFile("artifact-bundle.json");
  if (!bundleFile.existsAsFile()) {
    snapshot.statusText = "No artifacts yet";
    snapshot.detailText = "Artifact file not found: Builds/TeulCompileSmoke_*/artifact-bundle.json";
    return snapshot;
  }

  snapshot.available = true;
  const auto parsed = juce::JSON::parse(bundleFile);
  if (!parsed.isObject()) {
    snapshot.statusText = "Invalid artifact";
    snapshot.detailText = "Failed to parse compile smoke artifact bundle.";
    return snapshot;
  }

  const auto *object = parsed.getDynamicObject();
  snapshot.passed = static_cast<bool>(object->getProperty("passed"));
  snapshot.artifactDirectory = object->getProperty("artifactDirectory").toString();
  snapshot.statusText = snapshot.passed ? "PASS" : "FAIL";
  snapshot.detailText = snapshot.title + "\r\n"
                        "Status: " + snapshot.statusText + "\r\n"
                        "Artifact: " + snapshot.artifactDirectory + "\r\n"
                        "Runtime Class: " + object->getProperty("runtimeClassName").toString() + "\r\n"
                        "Export Exit: " + object->getProperty("exportExitCode").toString() + "\r\n"
                        "Compile Exit: " + object->getProperty("compileExitCode").toString();
  const auto failureReason = object->getProperty("failureReason").toString().trim();
  if (failureReason.isNotEmpty())
    snapshot.detailText << "\r\nFailure: " << failureReason;
  return snapshot;
}

juce::String buildSummaryLine(const std::vector<DiagnosticSnapshot> &snapshots) {
  juce::String line;
  for (std::size_t index = 0; index < snapshots.size(); ++index) {
    const auto &snapshot = snapshots[index];
    if (index > 0)
      line << "  |  ";
    line << snapshot.title << ": " << (snapshot.available ? snapshot.statusText : "N/A");
  }
  return line;
}

juce::String buildReportText(const std::vector<DiagnosticSnapshot> &snapshots) {
  juce::String text;
  for (std::size_t index = 0; index < snapshots.size(); ++index) {
    if (index > 0)
      text << "\r\n\r\n";
    text << snapshots[index].detailText.trim();
  }
  return text;
}

class DiagnosticsDrawerImpl final : public DiagnosticsDrawer,
                                    private juce::Timer {
public:
  DiagnosticsDrawerImpl() {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(summaryLabel);
    addAndMakeVisible(refreshButton);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(reportEditor);

    titleLabel.setText("Diagnostics Drawer", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.94f));
    titleLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));

    summaryLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.68f));
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    summaryLabel.setMinimumHorizontalScale(0.8f);

    refreshButton.setButtonText("Refresh");
    closeButton.setButtonText("Hide");
    refreshButton.onClick = [this] { refreshArtifacts(true); };
    closeButton.onClick = [this] { setDrawerOpen(false); };

    reportEditor.setMultiLine(true);
    reportEditor.setReadOnly(true);
    reportEditor.setScrollbarsShown(true);
    reportEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(0xff0f172a));
    reportEditor.setColour(juce::TextEditor::textColourId,
                           juce::Colours::white.withAlpha(0.88f));
    reportEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colour(0xff334155));
    reportEditor.setFont(juce::FontOptions(12.0f, juce::Font::plain));

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

    std::vector<DiagnosticSnapshot> snapshots;
    snapshots.push_back(loadSummarySnapshot(
        "Golden Audio",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("GoldenAudio")
            .getChildFile("RepresentativePrimary_verify")
            .getChildFile("golden-suite-summary.txt"),
        {}));
    snapshots.push_back(loadSummarySnapshot(
        "Parity Smoke",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("EditableRoundTrip")
            .getChildFile("G1_S1_primary")
            .getChildFile("parity-summary.txt"),
        {}));
    snapshots.push_back(loadSummarySnapshot(
        "Parity Matrix",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("EditableRoundTrip")
            .getChildFile("RepresentativeMatrix_primary")
            .getChildFile("matrix-summary.txt"),
        {}));
    snapshots.push_back(loadSummarySnapshot(
        "Stress Soak",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("StressSoak")
            .getChildFile("representative_stress_primary")
            .getChildFile("stress-summary.txt"),
        {}));
    snapshots.push_back(loadSummarySnapshot(
        "Benchmark Gate",
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulVerification")
            .getChildFile("Benchmark")
            .getChildFile("representative_benchmark_primary")
            .getChildFile("benchmark-summary.txt"),
        {}));
    snapshots.push_back(loadCompileSnapshot());

    summaryLabel.setText(buildSummaryLine(snapshots), juce::dontSendNotification);
    reportEditor.setText(buildReportText(snapshots), false);
    overallAccent = juce::Colour(0xff22c55e);
    for (const auto &snapshot : snapshots) {
      if (!snapshot.available) {
        overallAccent = juce::Colour(0xff94a3b8);
        break;
      }
      if (!snapshot.passed) {
        overallAccent = juce::Colour(0xffef4444);
        break;
      }
      overallAccent = statusColour(snapshot);
    }
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
    titleLabel.setBounds(header.removeFromTop(18));
    auto summary = area.removeFromTop(24);
    summaryLabel.setBounds(summary);
    area.removeFromTop(6);
    reportEditor.setBounds(area);
  }

private:
  void timerCallback() override { refreshArtifacts(false); }

  juce::Label titleLabel;
  juce::Label summaryLabel;
  juce::TextButton refreshButton;
  juce::TextButton closeButton;
  juce::TextEditor reportEditor;
  juce::Colour overallAccent = juce::Colour(0xff22c55e);
  juce::Time lastRefreshTime;
  std::function<void()> onLayoutChanged;
};

} // namespace

std::unique_ptr<DiagnosticsDrawer> DiagnosticsDrawer::create() {
  return std::make_unique<DiagnosticsDrawerImpl>();
}

} // namespace Teul
