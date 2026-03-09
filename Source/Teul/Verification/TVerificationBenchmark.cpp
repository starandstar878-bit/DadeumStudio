#include "Teul/Verification/TVerificationBenchmark.h"
#include <cmath>
namespace Teul {
namespace {
juce::String sanitizePathFragment(const juce::String &text) {
  juce::String cleaned;
  for (const auto ch : text) {
    if (juce::CharacterFunctions::isLetterOrDigit(ch))
      cleaned << ch;
    else
      cleaned << '_';
  }
  return cleaned.trimCharactersAtEnd("_").trimCharactersAtStart("_");
}
void writeTextArtifact(const juce::File &file, const juce::String &text) {
  juce::ignoreUnused(file.replaceWithText(text, false, false, "\r\n"));
}
void writeJsonArtifact(const juce::File &file, const juce::var &json) {
  juce::ignoreUnused(file.replaceWithText(juce::JSON::toString(json, true), false,
                                          false, "\r\n"));
}
juce::String relativeArtifactPath(const juce::File &root, const juce::File &file) {
  const auto path = file.getFullPathName();
  if (path.isEmpty())
    return {};
  return file.getRelativePathFrom(root).replaceCharacter('\\', '/');
}
juce::var makeArtifactFileEntry(const juce::String &role,
                                const juce::File &root,
                                const juce::File &file) {
  auto *entry = new juce::DynamicObject();
  entry->setProperty("role", role);
  entry->setProperty("relativePath", relativeArtifactPath(root, file));
  entry->setProperty("exists", file.exists());
  return juce::var(entry);
}
bool isFiniteRuntimeStats(const TGraphRuntime::RuntimeStats &stats) {
  return std::isfinite(stats.sampleRate) && std::isfinite(stats.cpuLoadPercent) &&
         std::isfinite(stats.lastBuildMilliseconds) &&
         std::isfinite(stats.maxBuildMilliseconds) &&
         std::isfinite(stats.lastProcessMilliseconds) &&
         std::isfinite(stats.maxProcessMilliseconds);
}
TGraphRuntime::RuntimeStats maxRuntimeStats(const TGraphRuntime::RuntimeStats &lhs,
                                            const TGraphRuntime::RuntimeStats &rhs) {
  TGraphRuntime::RuntimeStats result = lhs;
  result.sampleRate = rhs.sampleRate;
  result.preparedBlockSize = juce::jmax(lhs.preparedBlockSize, rhs.preparedBlockSize);
  result.lastInputChannels = juce::jmax(lhs.lastInputChannels, rhs.lastInputChannels);
  result.lastOutputChannels = juce::jmax(lhs.lastOutputChannels, rhs.lastOutputChannels);
  result.activeNodeCount = juce::jmax(lhs.activeNodeCount, rhs.activeNodeCount);
  result.allocatedPortChannels =
      juce::jmax(lhs.allocatedPortChannels, rhs.allocatedPortChannels);
  result.largestBlockSeen = juce::jmax(lhs.largestBlockSeen, rhs.largestBlockSeen);
  result.largestOutputChannelCountSeen = juce::jmax(
      lhs.largestOutputChannelCountSeen, rhs.largestOutputChannelCountSeen);
  result.smoothingActiveCount =
      juce::jmax(lhs.smoothingActiveCount, rhs.smoothingActiveCount);
  result.processBlockCount = juce::jmax(lhs.processBlockCount, rhs.processBlockCount);
  result.rebuildRequestCount =
      juce::jmax(lhs.rebuildRequestCount, rhs.rebuildRequestCount);
  result.rebuildCommitCount =
      juce::jmax(lhs.rebuildCommitCount, rhs.rebuildCommitCount);
  result.paramChangeCount = juce::jmax(lhs.paramChangeCount, rhs.paramChangeCount);
  result.droppedParamChangeCount =
      juce::jmax(lhs.droppedParamChangeCount, rhs.droppedParamChangeCount);
  result.droppedParamNotificationCount = juce::jmax(
      lhs.droppedParamNotificationCount, rhs.droppedParamNotificationCount);
  result.activeGeneration = juce::jmax(lhs.activeGeneration, rhs.activeGeneration);
  result.pendingGeneration = juce::jmax(lhs.pendingGeneration, rhs.pendingGeneration);
  result.rebuildPending = lhs.rebuildPending || rhs.rebuildPending;
  result.clipDetected = lhs.clipDetected || rhs.clipDetected;
  result.denormalDetected = lhs.denormalDetected || rhs.denormalDetected;
  result.xrunDetected = lhs.xrunDetected || rhs.xrunDetected;
  result.mutedFallbackActive = lhs.mutedFallbackActive || rhs.mutedFallbackActive;
  result.cpuLoadPercent = juce::jmax(lhs.cpuLoadPercent, rhs.cpuLoadPercent);
  result.lastBuildMilliseconds =
      juce::jmax(lhs.lastBuildMilliseconds, rhs.lastBuildMilliseconds);
  result.maxBuildMilliseconds =
      juce::jmax(lhs.maxBuildMilliseconds, rhs.maxBuildMilliseconds);
  result.lastProcessMilliseconds =
      juce::jmax(lhs.lastProcessMilliseconds, rhs.lastProcessMilliseconds);
  result.maxProcessMilliseconds =
      juce::jmax(lhs.maxProcessMilliseconds, rhs.maxProcessMilliseconds);
  return result;
}
struct BenchmarkCaseSpec {
  juce::String fixtureId;
  TVerificationRenderProfile profile;
  TVerificationStimulusSpec stimulus;
  TVerificationBenchmarkThresholds thresholds;
};
std::vector<BenchmarkCaseSpec> makeRepresentativeBenchmarkCases() {
  std::vector<BenchmarkCaseSpec> cases;
  cases.push_back({"G1",
                   makePrimaryVerificationRenderProfile(),
                   makeStaticRenderStimulus(),
                   {1.00f, 0.45, 5.0}});
  cases.push_back({"G2",
                   makePrimaryVerificationRenderProfile(),
                   makeSweepAutomationStimulus("LowPass", "cutoff", 320.0f,
                                               7200.0f),
                   {0.90f, 0.45, 5.0}});
  cases.push_back({"G3",
                   makePrimaryVerificationRenderProfile(),
                   makeSweepAutomationStimulus("Stereo Pan", "pan", -1.0f,
                                               1.0f),
                   {1.10f, 0.60, 5.0}});
  cases.push_back({"G4",
                   makePrimaryVerificationRenderProfile(),
                   makeMidiPhraseStimulus(),
                   {1.20f, 0.50, 5.0}});
  cases.push_back({"G5",
                   makePrimaryVerificationRenderProfile(),
                   makeStepAutomationStimulus("Delay", "mix", 0.15f, 0.75f,
                                              0.25),
                   {1.00f, 0.50, 5.0}});
  return cases;
}
juce::File makeSuiteArtifactDirectory(const juce::String &suiteId) {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("Benchmark")
      .getChildFile(sanitizePathFragment(suiteId));
}
juce::File makeCaseArtifactDirectory(const juce::File &suiteDirectory,
                                     const TVerificationGraphFixture &fixture,
                                     const TVerificationRenderProfile &profile,
                                     const TVerificationStimulusSpec &stimulus) {
  return suiteDirectory.getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                                     sanitizePathFragment(stimulus.stimulusId) + "_" +
                                     sanitizePathFragment(profile.profileId));
}
juce::File makeSuiteHistoryFile(const juce::String &suiteId) {
  const auto sanitized = sanitizePathFragment(suiteId);
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("Benchmark")
      .getChildFile(sanitized + "-history.json");
}
juce::var buildBenchmarkHistoryEntry(const TVerificationBenchmarkSuiteReport &report) {
  double peakCpuLoadPercent = 0.0;
  double peakProcessMilliseconds = 0.0;
  double peakBuildMilliseconds = 0.0;
  juce::Array<juce::var> cases;
  for (const auto &caseReport : report.caseReports) {
    peakCpuLoadPercent = juce::jmax(
        peakCpuLoadPercent,
        static_cast<double>(caseReport.worstRuntimeStats.cpuLoadPercent));
    peakProcessMilliseconds = juce::jmax(
        peakProcessMilliseconds,
        caseReport.worstRuntimeStats.maxProcessMilliseconds);
    peakBuildMilliseconds = juce::jmax(
        peakBuildMilliseconds,
        caseReport.worstRuntimeStats.maxBuildMilliseconds);

    auto *caseObject = new juce::DynamicObject();
    caseObject->setProperty("graphId", caseReport.graphId);
    caseObject->setProperty("stimulusId", caseReport.stimulusId);
    caseObject->setProperty("profileId", caseReport.profileId);
    caseObject->setProperty("passed", caseReport.passed);
    caseObject->setProperty("worstCpuLoadPercent",
                            caseReport.worstRuntimeStats.cpuLoadPercent);
    caseObject->setProperty("worstMaxProcessMilliseconds",
                            caseReport.worstRuntimeStats.maxProcessMilliseconds);
    caseObject->setProperty("worstMaxBuildMilliseconds",
                            caseReport.worstRuntimeStats.maxBuildMilliseconds);
    if (caseReport.failureReason.isNotEmpty())
      caseObject->setProperty("failureReason", caseReport.failureReason);
    cases.add(juce::var(caseObject));
  }

  auto *entry = new juce::DynamicObject();
  entry->setProperty("timestampUtc",
                     juce::Time::getCurrentTime().toISO8601(true));
  entry->setProperty("suiteId", report.suiteId);
  entry->setProperty("passed", report.passed);
  entry->setProperty("failedCaseCount", report.failedCaseCount);
  entry->setProperty("iterationCount", report.iterationCount);
  entry->setProperty("peakCpuLoadPercent", peakCpuLoadPercent);
  entry->setProperty("peakMaxProcessMilliseconds", peakProcessMilliseconds);
  entry->setProperty("peakMaxBuildMilliseconds", peakBuildMilliseconds);
  entry->setProperty("cases", juce::var(cases));
  return juce::var(entry);
}
void appendBenchmarkHistory(const TVerificationBenchmarkSuiteReport &report) {
  const auto historyFile = makeSuiteHistoryFile(report.suiteId);
  juce::ignoreUnused(historyFile.getParentDirectory().createDirectory());

  juce::Array<juce::var> entries;
  if (historyFile.existsAsFile()) {
    const auto parsed = juce::JSON::parse(historyFile);
    if (parsed.isObject()) {
      if (const auto *object = parsed.getDynamicObject()) {
        const auto existingEntries = object->getProperty("entries");
        if (existingEntries.isArray())
          entries = *existingEntries.getArray();
      }
    }
  }

  entries.add(buildBenchmarkHistoryEntry(report));
  while (entries.size() > 24)
    entries.remove(0);

  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-benchmark-history");
  root->setProperty("suiteId", report.suiteId);
  root->setProperty("entries", juce::var(entries));
  writeJsonArtifact(historyFile, juce::var(root));
}
const TVerificationGraphFixture *findFixtureById(
    const std::vector<TVerificationGraphFixture> &fixtures,
    const juce::String &fixtureId) {
  for (const auto &fixture : fixtures) {
    if (fixture.fixtureId == fixtureId)
      return &fixture;
  }
  return nullptr;
}
juce::String buildFailureReason(const TVerificationBenchmarkCaseReport &report) {
  juce::String failure;
  if (report.worstRuntimeStats.cpuLoadPercent > report.thresholds.maxCpuLoadPercent) {
    failure << "cpuLoadPercent exceeded baseline ("
            << juce::String(report.worstRuntimeStats.cpuLoadPercent, 6) << " > "
            << juce::String(report.thresholds.maxCpuLoadPercent, 6) << ")";
  }
  if (report.worstRuntimeStats.maxProcessMilliseconds >
      report.thresholds.maxProcessMilliseconds) {
    if (failure.isNotEmpty())
      failure << "; ";
    failure << "maxProcessMilliseconds exceeded baseline ("
            << juce::String(report.worstRuntimeStats.maxProcessMilliseconds, 6)
            << " > " << juce::String(report.thresholds.maxProcessMilliseconds, 6)
            << ")";
  }
  if (report.worstRuntimeStats.maxBuildMilliseconds >
      report.thresholds.maxBuildMilliseconds) {
    if (failure.isNotEmpty())
      failure << "; ";
    failure << "maxBuildMilliseconds exceeded baseline ("
            << juce::String(report.worstRuntimeStats.maxBuildMilliseconds, 6)
            << " > " << juce::String(report.thresholds.maxBuildMilliseconds, 6)
            << ")";
  }
  return failure;
}
juce::String buildBenchmarkCaseSummaryText(
    const TVerificationBenchmarkCaseReport &report) {
  juce::String summary;
  summary << "graphId=" << report.graphId << "\r\n";
  summary << "stimulusId=" << report.stimulusId << "\r\n";
  summary << "profileId=" << report.profileId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "iterationCount=" << report.iterationCount << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalRenderedSamples=" << report.totalRenderedSamples << "\r\n";
  summary << "totalRenderedBlocks=" << report.totalRenderedBlocks << "\r\n";
  summary << "thresholdCpuLoadPercent="
          << juce::String(report.thresholds.maxCpuLoadPercent, 6) << "\r\n";
  summary << "thresholdMaxProcessMilliseconds="
          << juce::String(report.thresholds.maxProcessMilliseconds, 6)
          << "\r\n";
  summary << "thresholdMaxBuildMilliseconds="
          << juce::String(report.thresholds.maxBuildMilliseconds, 6)
          << "\r\n";
  summary << "worstCpuLoadPercent="
          << juce::String(report.worstRuntimeStats.cpuLoadPercent, 6) << "\r\n";
  summary << "worstMaxProcessMilliseconds="
          << juce::String(report.worstRuntimeStats.maxProcessMilliseconds, 6)
          << "\r\n";
  summary << "worstMaxBuildMilliseconds="
          << juce::String(report.worstRuntimeStats.maxBuildMilliseconds, 6)
          << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  return summary;
}
juce::var makeBenchmarkCaseArtifactBundle(const juce::File &artifactDirectory,
                                          const TVerificationBenchmarkCaseReport &report) {
  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("benchmarkCaseSummary",
                                  artifactDirectory,
                                  artifactDirectory.getChildFile("case-summary.txt")));
  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", "benchmark-case");
  root->setProperty("graphId", report.graphId);
  root->setProperty("stimulusId", report.stimulusId);
  root->setProperty("profileId", report.profileId);
  root->setProperty("passed", report.passed);
  root->setProperty("iterationCount", report.iterationCount);
  root->setProperty("totalRenderedSamples", report.totalRenderedSamples);
  root->setProperty("totalRenderedBlocks", report.totalRenderedBlocks);
  root->setProperty("artifactDirectory", artifactDirectory.getFullPathName());
  root->setProperty("thresholdCpuLoadPercent", report.thresholds.maxCpuLoadPercent);
  root->setProperty("thresholdMaxProcessMilliseconds",
                    report.thresholds.maxProcessMilliseconds);
  root->setProperty("thresholdMaxBuildMilliseconds",
                    report.thresholds.maxBuildMilliseconds);
  root->setProperty("worstCpuLoadPercent", report.worstRuntimeStats.cpuLoadPercent);
  root->setProperty("worstMaxProcessMilliseconds",
                    report.worstRuntimeStats.maxProcessMilliseconds);
  root->setProperty("worstMaxBuildMilliseconds",
                    report.worstRuntimeStats.maxBuildMilliseconds);
  if (report.failureReason.isNotEmpty())
    root->setProperty("failureReason", report.failureReason);
  root->setProperty("files", juce::var(files));
  return juce::var(root);
}
void finalizeBenchmarkCaseArtifacts(const juce::File &artifactDirectory,
                                    const TVerificationBenchmarkCaseReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());
  writeTextArtifact(artifactDirectory.getChildFile("case-summary.txt"),
                    buildBenchmarkCaseSummaryText(report));
  writeJsonArtifact(artifactDirectory.getChildFile("artifact-bundle.json"),
                    makeBenchmarkCaseArtifactBundle(artifactDirectory, report));
}
juce::String buildBenchmarkSuiteSummaryText(
    const TVerificationBenchmarkSuiteReport &report) {
  juce::String summary;
  summary << "suiteId=" << report.suiteId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "iterationCount=" << report.iterationCount << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalCaseCount=" << report.totalCaseCount << "\r\n";
  summary << "passedCaseCount=" << report.passedCaseCount << "\r\n";
  summary << "failedCaseCount=" << report.failedCaseCount << "\r\n\r\n";
  for (const auto &caseReport : report.caseReports) {
    summary << "case=" << caseReport.graphId << "/" << caseReport.stimulusId
            << "/" << caseReport.profileId << "\r\n";
    summary << "passed=" << (caseReport.passed ? "true" : "false") << "\r\n";
    summary << "artifactDirectory=" << caseReport.artifactDirectory << "\r\n";
    summary << "worstCpuLoadPercent="
            << juce::String(caseReport.worstRuntimeStats.cpuLoadPercent, 6)
            << "\r\n";
    summary << "worstMaxProcessMilliseconds="
            << juce::String(caseReport.worstRuntimeStats.maxProcessMilliseconds, 6)
            << "\r\n";
    summary << "worstMaxBuildMilliseconds="
            << juce::String(caseReport.worstRuntimeStats.maxBuildMilliseconds, 6)
            << "\r\n";
    if (caseReport.failureReason.isNotEmpty())
      summary << "failureReason=" << caseReport.failureReason << "\r\n";
    summary << "\r\n";
  }
  return summary;
}
juce::var makeBenchmarkSuiteArtifactBundle(const juce::File &artifactDirectory,
                                           const TVerificationBenchmarkSuiteReport &report) {
  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("benchmarkSummary",
                                  artifactDirectory,
                                  artifactDirectory.getChildFile("benchmark-summary.txt")));
  files.add(makeArtifactFileEntry("benchmarkHistory",
                                  artifactDirectory,
                                  makeSuiteHistoryFile(report.suiteId)));
  juce::Array<juce::var> cases;
  for (const auto &caseReport : report.caseReports) {
    auto *entry = new juce::DynamicObject();
    entry->setProperty("graphId", caseReport.graphId);
    entry->setProperty("stimulusId", caseReport.stimulusId);
    entry->setProperty("profileId", caseReport.profileId);
    entry->setProperty("passed", caseReport.passed);
    entry->setProperty("iterationCount", caseReport.iterationCount);
    entry->setProperty("artifactDirectory", caseReport.artifactDirectory);
    if (caseReport.artifactDirectory.isNotEmpty()) {
      const auto caseDir = juce::File(caseReport.artifactDirectory);
      entry->setProperty("relativeArtifactDirectory",
                         relativeArtifactPath(artifactDirectory, caseDir));
      entry->setProperty("bundleRelativePath",
                         relativeArtifactPath(
                             artifactDirectory,
                             caseDir.getChildFile("artifact-bundle.json")));
    }
    if (caseReport.failureReason.isNotEmpty())
      entry->setProperty("failureReason", caseReport.failureReason);
    cases.add(juce::var(entry));
  }
  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", "benchmark-suite");
  root->setProperty("suiteId", report.suiteId);
  root->setProperty("passed", report.passed);
  root->setProperty("iterationCount", report.iterationCount);
  root->setProperty("artifactDirectory", artifactDirectory.getFullPathName());
  root->setProperty("totalCaseCount", report.totalCaseCount);
  root->setProperty("passedCaseCount", report.passedCaseCount);
  root->setProperty("failedCaseCount", report.failedCaseCount);
  root->setProperty("files", juce::var(files));
  root->setProperty("cases", juce::var(cases));
  return juce::var(root);
}
void finalizeBenchmarkSuiteArtifacts(const juce::File &artifactDirectory,
                                     const TVerificationBenchmarkSuiteReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());
  writeTextArtifact(artifactDirectory.getChildFile("benchmark-summary.txt"),
                    buildBenchmarkSuiteSummaryText(report));
  appendBenchmarkHistory(report);
  writeJsonArtifact(artifactDirectory.getChildFile("artifact-bundle.json"),
                    makeBenchmarkSuiteArtifactBundle(artifactDirectory, report));
}
} // namespace
bool runRepresentativeBenchmarkGate(const TNodeRegistry &registry,
                                    TVerificationBenchmarkSuiteReport &reportOut,
                                    int iterationCount) {
  reportOut = {};
  reportOut.suiteId = "representative-benchmark-primary";
  reportOut.iterationCount = juce::jmax(1, iterationCount);
  const auto suiteArtifactDirectory = makeSuiteArtifactDirectory(reportOut.suiteId);
  reportOut.artifactDirectory = suiteArtifactDirectory.getFullPathName();
  juce::ignoreUnused(suiteArtifactDirectory.deleteRecursively());
  juce::ignoreUnused(suiteArtifactDirectory.createDirectory());
  struct ArtifactScope {
    juce::File directory;
    TVerificationBenchmarkSuiteReport &report;
    ~ArtifactScope() { finalizeBenchmarkSuiteArtifacts(directory, report); }
  } artifactScope{suiteArtifactDirectory, reportOut};
  const auto caseSpecs = makeRepresentativeBenchmarkCases();
  const auto fixtures = makeRepresentativeVerificationGraphSet(registry);
  for (const auto &caseSpec : caseSpecs) {
    TVerificationBenchmarkCaseReport caseReport;
    caseReport.graphId = caseSpec.fixtureId;
    caseReport.stimulusId = caseSpec.stimulus.stimulusId;
    caseReport.profileId = caseSpec.profile.profileId;
    caseReport.iterationCount = reportOut.iterationCount;
    caseReport.thresholds = caseSpec.thresholds;
    const auto *fixture = findFixtureById(fixtures, caseSpec.fixtureId);
    if (fixture == nullptr) {
      caseReport.failureReason = "Representative benchmark fixture was not found.";
    } else {
      const auto caseArtifactDirectory =
          makeCaseArtifactDirectory(suiteArtifactDirectory, *fixture, caseSpec.profile,
                                    caseSpec.stimulus);
      caseReport.artifactDirectory = caseArtifactDirectory.getFullPathName();
      juce::ignoreUnused(caseArtifactDirectory.deleteRecursively());
      juce::ignoreUnused(caseArtifactDirectory.createDirectory());
      for (int iteration = 0; iteration < reportOut.iterationCount; ++iteration) {
        TVerificationRenderResult renderResult;
        juce::String renderError;
        if (!renderGraphWithStimulus(registry, fixture->document, caseSpec.profile,
                                     caseSpec.stimulus, renderResult, &renderError)) {
          caseReport.failureReason =
              "Benchmark render failed at iteration " + juce::String(iteration + 1) +
              ": " + renderError;
          break;
        }
        if (renderResult.audioBuffer.getNumChannels() <= 0 ||
            renderResult.audioBuffer.getNumSamples() <= 0) {
          caseReport.failureReason =
              "Benchmark render produced an empty audio buffer at iteration " +
              juce::String(iteration + 1) + ".";
          break;
        }
        if (!isFiniteRuntimeStats(renderResult.runtimeStats)) {
          caseReport.failureReason =
              "Benchmark render produced non-finite runtime stats at iteration " +
              juce::String(iteration + 1) + ".";
          break;
        }
        caseReport.totalRenderedSamples += renderResult.totalSamples;
        caseReport.totalRenderedBlocks += renderResult.renderedBlockCount;
        caseReport.worstRuntimeStats =
            (iteration == 0)
                ? renderResult.runtimeStats
                : maxRuntimeStats(caseReport.worstRuntimeStats,
                                  renderResult.runtimeStats);
      }
      if (caseReport.failureReason.isEmpty())
        caseReport.failureReason = buildFailureReason(caseReport);
      caseReport.passed = caseReport.failureReason.isEmpty();
      finalizeBenchmarkCaseArtifacts(caseArtifactDirectory, caseReport);
    }
    ++reportOut.totalCaseCount;
    if (caseReport.passed)
      ++reportOut.passedCaseCount;
    else
      ++reportOut.failedCaseCount;
    reportOut.caseReports.push_back(caseReport);
  }
  reportOut.passed =
      reportOut.totalCaseCount > 0 && reportOut.failedCaseCount == 0;
  return reportOut.passed;
}
} // namespace Teul
