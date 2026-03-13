#include "Teul2/Runtime/AudioGraph/TRuntimeValidator.h"

#include "Teul2/Document/TDocumentSerializer.h"
#include "Teul2/Document/TDocumentStore.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/Serialization/TSerializer.h"
#include "Teul/Verification/TVerificationFixtures.h"

#include <algorithm>
#include <cmath>

namespace Teul {
namespace {

struct RepresentativeCaseSpec {
  juce::String fixtureId;
  TRuntimeValidationRenderProfile profile;
  TRuntimeValidationStimulusSpec stimulus;
};

struct RepresentativeBenchmarkCaseSpec {
  juce::String fixtureId;
  TRuntimeValidationRenderProfile profile;
  TRuntimeValidationStimulusSpec stimulus;
  TRuntimeValidationBenchmarkThresholds thresholds;
};

bool convertLegacyDocument(TTeulDocument &documentOut,
                           const TGraphDocument &legacyDocument) {
  return TDocumentSerializer::fromJson(documentOut,
                                       TSerializer::toJson(legacyDocument),
                                       nullptr);
}

TRuntimeValidationGraphFixture convertLegacyFixture(
    const TVerificationGraphFixture &legacyFixture) {
  TRuntimeValidationGraphFixture fixture;
  fixture.fixtureId = legacyFixture.fixtureId;
  fixture.displayName = legacyFixture.displayName;
  juce::ignoreUnused(convertLegacyDocument(fixture.document,
                                           legacyFixture.document));
  return fixture;
}

const TRuntimeValidationGraphFixture *findFixtureById(
    const std::vector<TRuntimeValidationGraphFixture> &fixtures,
    const juce::String &fixtureId) {
  for (const auto &fixture : fixtures) {
    if (fixture.fixtureId == fixtureId)
      return &fixture;
  }

  return nullptr;
}

NodeId findNodeIdByLabel(const TTeulDocument &document,
                         const juce::String &nodeLabel) {
  for (const auto &node : document.nodes) {
    if (node.label.equalsIgnoreCase(nodeLabel))
      return node.nodeId;
  }
  return kInvalidNodeId;
}

float valueForLaneAtSample(const TRuntimeValidationAutomationLane &lane,
                           int sampleIndex, int totalSamples,
                           double sampleRate) {
  if (lane.mode == TRuntimeValidationAutomationMode::Linear) {
    if (totalSamples <= 1)
      return lane.endValue;
    const float progress = juce::jlimit(
        0.0f, 1.0f,
        static_cast<float>(sampleIndex) /
            static_cast<float>(totalSamples - 1));
    return lane.startValue + ((lane.endValue - lane.startValue) * progress);
  }

  const int stepIntervalSamples = juce::jmax(
      1, juce::roundToInt(juce::jmax(0.001, lane.stepIntervalSeconds) *
                          sampleRate));
  const int segmentIndex = sampleIndex / stepIntervalSamples;
  return (segmentIndex % 2 == 0) ? lane.startValue : lane.endValue;
}

void writeError(juce::String *errorMessageOut, const juce::String &message) {
  if (errorMessageOut != nullptr)
    *errorMessageOut = message;
}

float normalizeSample(float sample) noexcept {
  if (!std::isfinite(sample))
    return sample;
  return std::abs(sample) < 1.0e-20f ? 0.0f : sample;
}

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

juce::String relativeArtifactPath(const juce::File &root,
                                  const juce::File &file) {
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

bool writeTextArtifact(const juce::File &file, const juce::String &text) {
  return file.replaceWithText(text, false, false, "\r\n");
}

bool writeJsonArtifact(const juce::File &file, const juce::var &json) {
  return file.replaceWithText(juce::JSON::toString(json, true), false, false,
                              "\r\n");
}

std::vector<RepresentativeCaseSpec> makeRepresentativePrimaryCases() {
  std::vector<RepresentativeCaseSpec> cases;
  cases.push_back(
      {"G1", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeStaticRenderStimulus()});
  cases.push_back(
      {"G2", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeSweepAutomationStimulus("LowPass", "cutoff",
                                                      320.0f, 7200.0f)});
  cases.push_back(
      {"G3", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeSweepAutomationStimulus("Stereo Pan", "pan",
                                                      -1.0f, 1.0f)});
  cases.push_back(
      {"G4", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeMidiPhraseStimulus()});
  cases.push_back(
      {"G5", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeStepAutomationStimulus("Delay", "mix", 0.15f,
                                                     0.75f, 0.25)});
  return cases;
}

std::vector<RepresentativeBenchmarkCaseSpec>
makeRepresentativeBenchmarkCases() {
  std::vector<RepresentativeBenchmarkCaseSpec> cases;
  cases.push_back(
      {"G1", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeStaticRenderStimulus(), {1.00f, 0.45, 5.0}});
  cases.push_back(
      {"G2", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeSweepAutomationStimulus("LowPass", "cutoff",
                                                      320.0f, 7200.0f),
       {0.90f, 0.45, 5.0}});
  cases.push_back(
      {"G3", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeSweepAutomationStimulus("Stereo Pan", "pan",
                                                      -1.0f, 1.0f),
       {1.10f, 0.60, 5.0}});
  cases.push_back(
      {"G4", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeMidiPhraseStimulus(), {1.20f, 0.50, 5.0}});
  cases.push_back(
      {"G5", TRuntimeValidator::makePrimaryRenderProfile(),
       TRuntimeValidator::makeStepAutomationStimulus("Delay", "mix", 0.15f,
                                                     0.75f, 0.25),
       {1.00f, 0.50, 5.0}});
  return cases;
}

juce::File makeCaseArtifactDirectory(
    const TRuntimeValidationGraphFixture &fixture,
    const TRuntimeValidationRenderProfile &profile,
    const TRuntimeValidationStimulusSpec &stimulus) {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("EditableRoundTrip")
      .getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                    sanitizePathFragment(stimulus.stimulusId) + "_" +
                    sanitizePathFragment(profile.profileId));
}

void finalizeCaseArtifacts(const juce::File &artifactDirectory,
                           const TRuntimeValidationParityReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());

  juce::String summary;
  summary << "graphId=" << report.graphId << "\r\n";
  summary << "stimulusId=" << report.stimulusId << "\r\n";
  summary << "profileId=" << report.profileId << "\r\n";
  summary << "modeId=" << report.modeId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "importedDocumentLoaded="
          << (report.importedDocumentLoaded ? "true" : "false") << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalSamples=" << report.totalSamples << "\r\n";
  summary << "comparedChannels=" << report.comparedChannels << "\r\n";
  summary << "maxAbsoluteError=" << juce::String(report.maxAbsoluteError, 9)
          << "\r\n";
  summary << "rmsError=" << juce::String(report.rmsError, 12) << "\r\n";
  summary << "firstMismatchChannel=" << report.firstMismatchChannel
          << "\r\n";
  summary << "firstMismatchSample=" << report.firstMismatchSample
          << "\r\n";
  summary << "exportWarnings=" << report.exportWarningCount << "\r\n";
  summary << "exportErrors=" << report.exportErrorCount << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  juce::ignoreUnused(writeTextArtifact(
      artifactDirectory.getChildFile("parity-summary.txt"), summary));

  juce::ignoreUnused(writeTextArtifact(
      artifactDirectory.getChildFile("export-report.txt"),
      report.exportReport.toText()));

  if (report.failureReason.isNotEmpty()) {
    juce::ignoreUnused(writeTextArtifact(
        artifactDirectory.getChildFile("parity-failure.txt"),
        report.failureReason));
  }

  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry(
      "paritySummary", artifactDirectory,
      artifactDirectory.getChildFile("parity-summary.txt")));
  files.add(makeArtifactFileEntry(
      "exportReport", artifactDirectory,
      artifactDirectory.getChildFile("export-report.txt")));
  files.add(makeArtifactFileEntry(
      "bundle", artifactDirectory,
      artifactDirectory.getChildFile("artifact-bundle.json")));
  if (report.exportReport.graphFile.getFullPathName().isNotEmpty())
    files.add(makeArtifactFileEntry("editableGraph", artifactDirectory,
                                    report.exportReport.graphFile));
  if (report.exportReport.manifestFile.getFullPathName().isNotEmpty())
    files.add(makeArtifactFileEntry("editableManifest", artifactDirectory,
                                    report.exportReport.manifestFile));
  if (report.exportReport.runtimeDataFile.getFullPathName().isNotEmpty())
    files.add(makeArtifactFileEntry("editableRuntimeData", artifactDirectory,
                                    report.exportReport.runtimeDataFile));

  auto *bundle = new juce::DynamicObject();
  bundle->setProperty("kind", "teul-verification-artifact-bundle");
  bundle->setProperty("scope", "case");
  bundle->setProperty("graphId", report.graphId);
  bundle->setProperty("stimulusId", report.stimulusId);
  bundle->setProperty("profileId", report.profileId);
  bundle->setProperty("modeId", report.modeId);
  bundle->setProperty("passed", report.passed);
  bundle->setProperty("artifactDirectory", artifactDirectory.getFullPathName());
  bundle->setProperty("files", juce::var(files));
  if (report.failureReason.isNotEmpty())
    bundle->setProperty("failureReason", report.failureReason);
  juce::ignoreUnused(writeJsonArtifact(
      artifactDirectory.getChildFile("artifact-bundle.json"),
      juce::var(bundle)));
}

void finalizeSuiteArtifacts(const juce::File &artifactDirectory,
                            const TRuntimeValidationParitySuiteReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());

  juce::String summary;
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "suiteId=" << report.suiteId << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalCaseCount=" << report.totalCaseCount << "\r\n";
  summary << "passedCaseCount=" << report.passedCaseCount << "\r\n";
  summary << "failedCaseCount=" << report.failedCaseCount << "\r\n";
  for (const auto &caseReport : report.caseReports) {
    summary << "\r\ncase=" << caseReport.graphId << "/"
            << caseReport.stimulusId << "/" << caseReport.profileId
            << "\r\n";
    summary << "passed=" << (caseReport.passed ? "true" : "false")
            << "\r\n";
    if (caseReport.failureReason.isNotEmpty())
      summary << "failureReason=" << caseReport.failureReason << "\r\n";
  }
  juce::ignoreUnused(writeTextArtifact(
      artifactDirectory.getChildFile("matrix-summary.txt"), summary));

  juce::Array<juce::var> cases;
  for (const auto &caseReport : report.caseReports) {
    auto *entry = new juce::DynamicObject();
    entry->setProperty("graphId", caseReport.graphId);
    entry->setProperty("stimulusId", caseReport.stimulusId);
    entry->setProperty("profileId", caseReport.profileId);
    entry->setProperty("modeId", caseReport.modeId);
    entry->setProperty("passed", caseReport.passed);
    entry->setProperty("artifactDirectory", caseReport.artifactDirectory);
    if (caseReport.failureReason.isNotEmpty())
      entry->setProperty("failureReason", caseReport.failureReason);
    cases.add(juce::var(entry));
  }

  auto *bundle = new juce::DynamicObject();
  bundle->setProperty("kind", "teul-verification-artifact-bundle");
  bundle->setProperty("scope", "suite");
  bundle->setProperty("suiteId", report.suiteId);
  bundle->setProperty("passed", report.passed);
  bundle->setProperty("artifactDirectory", artifactDirectory.getFullPathName());
  bundle->setProperty("totalCaseCount", report.totalCaseCount);
  bundle->setProperty("passedCaseCount", report.passedCaseCount);
  bundle->setProperty("failedCaseCount", report.failedCaseCount);
  bundle->setProperty("cases", juce::var(cases));
  juce::ignoreUnused(writeJsonArtifact(
      artifactDirectory.getChildFile("artifact-bundle.json"),
      juce::var(bundle)));
}
juce::File baselineSuiteDirectory() {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Source")
      .getChildFile("Teul")
      .getChildFile("Verification")
      .getChildFile("GoldenAudio")
      .getChildFile("RepresentativePrimary");
}

juce::File verifySuiteArtifactDirectory() {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("GoldenAudio")
      .getChildFile("RepresentativePrimary_verify");
}

juce::File makeGoldenCaseDirectory(
    const juce::File &root,
    const TRuntimeValidationGraphFixture &fixture,
    const TRuntimeValidationRenderProfile &profile,
    const TRuntimeValidationStimulusSpec &stimulus) {
  return root.getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                           sanitizePathFragment(stimulus.stimulusId) + "_" +
                           sanitizePathFragment(profile.profileId));
}

bool writeWaveFile(const juce::File &file,
                   const juce::AudioBuffer<float> &buffer,
                   double sampleRate,
                   juce::String *errorMessageOut) {
  if (sampleRate <= 0.0 || buffer.getNumChannels() <= 0 ||
      buffer.getNumSamples() <= 0) {
    writeError(errorMessageOut, "Invalid golden audio buffer.");
    return false;
  }

  juce::ignoreUnused(file.getParentDirectory().createDirectory());
  auto output = file.createOutputStream();
  if (output == nullptr) {
    writeError(errorMessageOut,
               "Failed to create golden audio output stream.");
    return false;
  }

  juce::WavAudioFormat wavFormat;
  auto *rawStream = output.release();
  std::unique_ptr<juce::AudioFormatWriter> writer(
      wavFormat.createWriterFor(rawStream, sampleRate,
                                static_cast<unsigned int>(buffer.getNumChannels()),
                                32, {}, 0));
  if (writer == nullptr) {
    delete rawStream;
    writeError(errorMessageOut, "Failed to create golden audio WAV writer.");
    return false;
  }

  if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples())) {
    writeError(errorMessageOut, "Failed to write golden audio WAV data.");
    return false;
  }

  return true;
}

bool readWaveFile(const juce::File &file,
                  juce::AudioBuffer<float> &bufferOut,
                  double &sampleRateOut,
                  juce::String *errorMessageOut) {
  juce::AudioFormatManager formatManager;
  formatManager.registerBasicFormats();
  std::unique_ptr<juce::AudioFormatReader> reader(
      formatManager.createReaderFor(file));
  if (reader == nullptr) {
    writeError(errorMessageOut, "Failed to open golden audio WAV file.");
    return false;
  }

  sampleRateOut = reader->sampleRate;
  const int channels = static_cast<int>(reader->numChannels);
  const int samples = static_cast<int>(reader->lengthInSamples);
  if (channels <= 0 || samples <= 0) {
    writeError(errorMessageOut, "Golden audio WAV file is empty.");
    return false;
  }

  bufferOut.setSize(channels, samples, false, false, true);
  if (!reader->read(&bufferOut, 0, samples, 0, true, true)) {
    writeError(errorMessageOut, "Failed to read golden audio WAV data.");
    return false;
  }

  return true;
}

juce::var makeGoldenMetadata(
    const TRuntimeValidationGraphFixture &fixture,
    const TRuntimeValidationRenderProfile &profile,
    const TRuntimeValidationStimulusSpec &stimulus,
    const TRuntimeValidationRenderResult &renderResult,
    const juce::File &caseDirectory) {
  auto *root = new juce::DynamicObject();
  root->setProperty("graphId", fixture.fixtureId);
  root->setProperty("displayName", fixture.displayName);
  root->setProperty("stimulusId", stimulus.stimulusId);
  root->setProperty("profileId", profile.profileId);
  root->setProperty("sampleRate", profile.sampleRate);
  root->setProperty("blockSize", profile.blockSize);
  root->setProperty("outputChannels", profile.outputChannels);
  root->setProperty("durationSeconds", profile.durationSeconds);
  root->setProperty("totalSamples", renderResult.totalSamples);
  root->setProperty("renderedBlockCount", renderResult.renderedBlockCount);
  root->setProperty(
      "baselineWave",
      relativeArtifactPath(caseDirectory,
                           caseDirectory.getChildFile("golden-output.wav")));
  root->setProperty(
      "recordedAtUtcMilliseconds",
      juce::Time::getCurrentTime().toMilliseconds());
  return juce::var(root);
}

juce::String buildGoldenRecordSummaryText(
    const TRuntimeValidationGoldenAudioReport &report,
    const TRuntimeValidationRenderProfile &profile) {
  juce::String summary;
  summary << "graphId=" << report.graphId << "\r\n";
  summary << "stimulusId=" << report.stimulusId << "\r\n";
  summary << "profileId=" << report.profileId << "\r\n";
  summary << "modeId=" << report.modeId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "baselineDirectory=" << report.baselineDirectory << "\r\n";
  summary << "totalSamples=" << report.totalSamples << "\r\n";
  summary << "sampleRate=" << juce::String(profile.sampleRate, 1) << "\r\n";
  summary << "blockSize=" << profile.blockSize << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  return summary;
}

juce::String buildGoldenVerifySummaryText(
    const TRuntimeValidationGoldenAudioReport &report) {
  juce::String summary;
  summary << "graphId=" << report.graphId << "\r\n";
  summary << "stimulusId=" << report.stimulusId << "\r\n";
  summary << "profileId=" << report.profileId << "\r\n";
  summary << "modeId=" << report.modeId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "baselineExists=" << (report.baselineExists ? "true" : "false")
          << "\r\n";
  summary << "baselineDirectory=" << report.baselineDirectory << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalSamples=" << report.totalSamples << "\r\n";
  summary << "comparedChannels=" << report.comparedChannels << "\r\n";
  summary << "maxAbsoluteError=" << juce::String(report.maxAbsoluteError, 9)
          << "\r\n";
  summary << "rmsError=" << juce::String(report.rmsError, 12) << "\r\n";
  summary << "firstMismatchChannel=" << report.firstMismatchChannel << "\r\n";
  summary << "firstMismatchSample=" << report.firstMismatchSample << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  return summary;
}

juce::var makeGoldenCaseArtifactBundle(
    const juce::File &rootDirectory,
    const TRuntimeValidationGoldenAudioReport &report,
    bool verifyMode) {
  juce::Array<juce::var> files;
  const auto addFile = [&](const juce::String &role, const juce::File &file) {
    if (file.getFullPathName().isNotEmpty())
      files.add(makeArtifactFileEntry(role, rootDirectory, file));
  };

  if (verifyMode) {
    addFile("goldenSummary", rootDirectory.getChildFile("golden-summary.txt"));
    addFile("failureSummary", rootDirectory.getChildFile("golden-failure.txt"));
  } else {
    addFile("goldenSummary", rootDirectory.getChildFile("golden-summary.txt"));
    addFile("goldenMetadata",
            rootDirectory.getChildFile("golden-metadata.json"));
    addFile("goldenOutput", rootDirectory.getChildFile("golden-output.wav"));
  }

  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope",
                    verifyMode ? "golden-verify-case"
                               : "golden-record-case");
  root->setProperty("graphId", report.graphId);
  root->setProperty("stimulusId", report.stimulusId);
  root->setProperty("profileId", report.profileId);
  root->setProperty("modeId", report.modeId);
  root->setProperty("passed", report.passed);
  root->setProperty("baselineDirectory", report.baselineDirectory);
  if (report.artifactDirectory.isNotEmpty())
    root->setProperty("artifactDirectory", report.artifactDirectory);
  root->setProperty("totalSamples", report.totalSamples);
  root->setProperty("comparedChannels", report.comparedChannels);
  root->setProperty("maxAbsoluteError", report.maxAbsoluteError);
  root->setProperty("rmsError", report.rmsError);
  root->setProperty("firstMismatchChannel", report.firstMismatchChannel);
  root->setProperty("firstMismatchSample", report.firstMismatchSample);
  root->setProperty("baselineExists", report.baselineExists);
  if (report.failureReason.isNotEmpty())
    root->setProperty("failureReason", report.failureReason);
  root->setProperty("files", juce::var(files));
  return juce::var(root);
}

juce::String buildGoldenSuiteSummaryText(
    const TRuntimeValidationGoldenAudioSuiteReport &report) {
  juce::String summary;
  summary << "suiteId=" << report.suiteId << "\r\n";
  summary << "modeId=" << report.modeId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "baselineDirectory=" << report.baselineDirectory << "\r\n";
  if (report.artifactDirectory.isNotEmpty())
    summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalCaseCount=" << report.totalCaseCount << "\r\n";
  summary << "passedCaseCount=" << report.passedCaseCount << "\r\n";
  summary << "failedCaseCount=" << report.failedCaseCount << "\r\n\r\n";
  for (const auto &caseReport : report.caseReports) {
    summary << "case=" << caseReport.graphId << "/" << caseReport.stimulusId
            << "/" << caseReport.profileId << "\r\n";
    summary << "passed=" << (caseReport.passed ? "true" : "false")
            << "\r\n";
    summary << "baselineDirectory=" << caseReport.baselineDirectory
            << "\r\n";
    if (caseReport.artifactDirectory.isNotEmpty())
      summary << "artifactDirectory=" << caseReport.artifactDirectory
              << "\r\n";
    if (caseReport.failureReason.isNotEmpty())
      summary << "failureReason=" << caseReport.failureReason << "\r\n";
    summary << "\r\n";
  }
  return summary;
}

juce::var makeGoldenSuiteArtifactBundle(
    const juce::File &rootDirectory,
    const TRuntimeValidationGoldenAudioSuiteReport &report,
    bool verifyMode) {
  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry(
      verifyMode ? "goldenVerifySummary" : "goldenRecordSummary",
      rootDirectory, rootDirectory.getChildFile("golden-suite-summary.txt")));

  juce::Array<juce::var> cases;
  for (const auto &caseReport : report.caseReports) {
    auto *entry = new juce::DynamicObject();
    entry->setProperty("graphId", caseReport.graphId);
    entry->setProperty("stimulusId", caseReport.stimulusId);
    entry->setProperty("profileId", caseReport.profileId);
    entry->setProperty("modeId", caseReport.modeId);
    entry->setProperty("passed", caseReport.passed);
    entry->setProperty("baselineDirectory", caseReport.baselineDirectory);
    if (caseReport.artifactDirectory.isNotEmpty())
      entry->setProperty("artifactDirectory", caseReport.artifactDirectory);
    if (caseReport.failureReason.isNotEmpty())
      entry->setProperty("failureReason", caseReport.failureReason);
    cases.add(juce::var(entry));
  }

  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope",
                    verifyMode ? "golden-verify-suite"
                               : "golden-record-suite");
  root->setProperty("suiteId", report.suiteId);
  root->setProperty("modeId", report.modeId);
  root->setProperty("passed", report.passed);
  root->setProperty("baselineDirectory", report.baselineDirectory);
  if (report.artifactDirectory.isNotEmpty())
    root->setProperty("artifactDirectory", report.artifactDirectory);
  root->setProperty("totalCaseCount", report.totalCaseCount);
  root->setProperty("passedCaseCount", report.passedCaseCount);
  root->setProperty("failedCaseCount", report.failedCaseCount);
  root->setProperty("files", juce::var(files));
  root->setProperty("cases", juce::var(cases));
  return juce::var(root);
}

void finalizeGoldenRecordCaseArtifacts(
    const juce::File &caseDirectory,
    const TRuntimeValidationGoldenAudioReport &report,
    const TRuntimeValidationRenderProfile &profile) {
  juce::ignoreUnused(writeTextArtifact(
      caseDirectory.getChildFile("golden-summary.txt"),
      buildGoldenRecordSummaryText(report, profile)));
  juce::ignoreUnused(writeJsonArtifact(
      caseDirectory.getChildFile("artifact-bundle.json"),
      makeGoldenCaseArtifactBundle(caseDirectory, report, false)));
}

void finalizeGoldenVerifyCaseArtifacts(
    const juce::File &artifactDirectory,
    const TRuntimeValidationGoldenAudioReport &report) {
  juce::ignoreUnused(writeTextArtifact(
      artifactDirectory.getChildFile("golden-summary.txt"),
      buildGoldenVerifySummaryText(report)));
  if (report.failureReason.isNotEmpty()) {
    juce::ignoreUnused(writeTextArtifact(
        artifactDirectory.getChildFile("golden-failure.txt"),
        report.failureReason));
  }
  juce::ignoreUnused(writeJsonArtifact(
      artifactDirectory.getChildFile("artifact-bundle.json"),
      makeGoldenCaseArtifactBundle(artifactDirectory, report, true)));
}

void finalizeGoldenSuiteArtifacts(
    const juce::File &directory,
    const TRuntimeValidationGoldenAudioSuiteReport &report,
    bool verifyMode) {
  juce::ignoreUnused(directory.createDirectory());
  juce::ignoreUnused(writeTextArtifact(
      directory.getChildFile("golden-suite-summary.txt"),
      buildGoldenSuiteSummaryText(report)));
  juce::ignoreUnused(writeJsonArtifact(
      directory.getChildFile("artifact-bundle.json"),
      makeGoldenSuiteArtifactBundle(directory, report, verifyMode)));
}

bool compareWithGolden(const TRuntimeValidationRenderResult &renderResult,
                       const juce::AudioBuffer<float> &goldenBuffer,
                       TRuntimeValidationGoldenAudioReport &reportOut,
                       float maxAbsoluteTolerance,
                       double rmsTolerance) {
  const int channels = juce::jmin(renderResult.audioBuffer.getNumChannels(),
                                  goldenBuffer.getNumChannels());
  const int samples = juce::jmin(renderResult.audioBuffer.getNumSamples(),
                                 goldenBuffer.getNumSamples());
  reportOut.totalSamples = samples;
  reportOut.comparedChannels = channels;
  if (channels <= 0 || samples <= 0) {
    reportOut.failureReason =
        "Golden audio compare produced an empty audio buffer.";
    return false;
  }

  double squaredErrorSum = 0.0;
  int comparedSampleCount = 0;
  for (int channel = 0; channel < channels; ++channel) {
    const float *lhs = renderResult.audioBuffer.getReadPointer(channel);
    const float *rhs = goldenBuffer.getReadPointer(channel);
    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex) {
      const float a = normalizeSample(lhs[sampleIndex]);
      const float b = normalizeSample(rhs[sampleIndex]);
      if (!std::isfinite(a) || !std::isfinite(b)) {
        reportOut.firstMismatchChannel = channel;
        reportOut.firstMismatchSample = sampleIndex;
        reportOut.failureReason =
            "Golden audio compare encountered NaN or Inf.";
        return false;
      }

      const double error = static_cast<double>(a) - static_cast<double>(b);
      const float absError = static_cast<float>(std::abs(error));
      reportOut.maxAbsoluteError =
          juce::jmax(reportOut.maxAbsoluteError, absError);
      squaredErrorSum += error * error;
      ++comparedSampleCount;
      if (reportOut.firstMismatchSample < 0 &&
          absError > maxAbsoluteTolerance) {
        reportOut.firstMismatchChannel = channel;
        reportOut.firstMismatchSample = sampleIndex;
      }
    }
  }

  reportOut.rmsError =
      comparedSampleCount > 0
          ? std::sqrt(squaredErrorSum /
                      static_cast<double>(comparedSampleCount))
          : 0.0;
  reportOut.passed = reportOut.maxAbsoluteError <= maxAbsoluteTolerance &&
                     reportOut.rmsError <= rmsTolerance;
  if (!reportOut.passed && reportOut.failureReason.isEmpty()) {
    reportOut.failureReason =
        "Golden audio output drift exceeded tolerance.";
  }
  return reportOut.passed;
}
bool isFiniteRuntimeStats(const TTeulRuntime::RuntimeStats &stats) {
  return std::isfinite(stats.sampleRate) && std::isfinite(stats.cpuLoadPercent) &&
         std::isfinite(stats.lastBuildMilliseconds) &&
         std::isfinite(stats.maxBuildMilliseconds) &&
         std::isfinite(stats.lastProcessMilliseconds) &&
         std::isfinite(stats.maxProcessMilliseconds);
}

TTeulRuntime::RuntimeStats
maxRuntimeStats(const TTeulRuntime::RuntimeStats &lhs,
                const TTeulRuntime::RuntimeStats &rhs) {
  TTeulRuntime::RuntimeStats result = lhs;
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

juce::File makeStressSuiteArtifactDirectory(const juce::String &suiteId) {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("StressSoak")
      .getChildFile(sanitizePathFragment(suiteId));
}

juce::File makeStressCaseArtifactDirectory(
    const juce::File &suiteDirectory,
    const TRuntimeValidationGraphFixture &fixture,
    const TRuntimeValidationRenderProfile &profile,
    const TRuntimeValidationStimulusSpec &stimulus) {
  return suiteDirectory.getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                                     sanitizePathFragment(stimulus.stimulusId) + "_" +
                                     sanitizePathFragment(profile.profileId));
}

juce::String buildStressCaseSummaryText(
    const TRuntimeValidationStressCaseReport &report) {
  juce::String summary;
  summary << "graphId=" << report.graphId << "\r\n";
  summary << "stimulusId=" << report.stimulusId << "\r\n";
  summary << "profileId=" << report.profileId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "iterationCount=" << report.iterationCount << "\r\n";
  summary << "totalRenderedSamples=" << report.totalRenderedSamples << "\r\n";
  summary << "totalRenderedBlocks=" << report.totalRenderedBlocks << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "worstCpuLoadPercent="
          << juce::String(report.worstRuntimeStats.cpuLoadPercent, 6) << "\r\n";
  summary << "worstMaxProcessMilliseconds="
          << juce::String(report.worstRuntimeStats.maxProcessMilliseconds, 6)
          << "\r\n";
  summary << "xrunDetected="
          << (report.worstRuntimeStats.xrunDetected ? "true" : "false")
          << "\r\n";
  summary << "clipDetected="
          << (report.worstRuntimeStats.clipDetected ? "true" : "false")
          << "\r\n";
  summary << "denormalDetected="
          << (report.worstRuntimeStats.denormalDetected ? "true" : "false")
          << "\r\n";
  summary << "mutedFallbackActive="
          << (report.worstRuntimeStats.mutedFallbackActive ? "true" : "false")
          << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  return summary;
}

juce::var makeStressCaseArtifactBundle(
    const juce::File &artifactDirectory,
    const TRuntimeValidationStressCaseReport &report) {
  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("stressCaseSummary", artifactDirectory,
                                  artifactDirectory.getChildFile("case-summary.txt")));

  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", "stress-case");
  root->setProperty("graphId", report.graphId);
  root->setProperty("stimulusId", report.stimulusId);
  root->setProperty("profileId", report.profileId);
  root->setProperty("passed", report.passed);
  root->setProperty("iterationCount", report.iterationCount);
  root->setProperty("totalRenderedSamples", report.totalRenderedSamples);
  root->setProperty("totalRenderedBlocks", report.totalRenderedBlocks);
  root->setProperty("artifactDirectory", artifactDirectory.getFullPathName());
  root->setProperty("worstCpuLoadPercent", report.worstRuntimeStats.cpuLoadPercent);
  root->setProperty("worstMaxProcessMilliseconds",
                    report.worstRuntimeStats.maxProcessMilliseconds);
  root->setProperty("xrunDetected", report.worstRuntimeStats.xrunDetected);
  root->setProperty("clipDetected", report.worstRuntimeStats.clipDetected);
  root->setProperty("denormalDetected", report.worstRuntimeStats.denormalDetected);
  root->setProperty("mutedFallbackActive",
                    report.worstRuntimeStats.mutedFallbackActive);
  if (report.failureReason.isNotEmpty())
    root->setProperty("failureReason", report.failureReason);
  root->setProperty("files", juce::var(files));
  return juce::var(root);
}

void finalizeStressCaseArtifacts(
    const juce::File &artifactDirectory,
    const TRuntimeValidationStressCaseReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());
  juce::ignoreUnused(writeTextArtifact(
      artifactDirectory.getChildFile("case-summary.txt"),
      buildStressCaseSummaryText(report)));
  juce::ignoreUnused(writeJsonArtifact(
      artifactDirectory.getChildFile("artifact-bundle.json"),
      makeStressCaseArtifactBundle(artifactDirectory, report)));
}

juce::String buildStressSuiteSummaryText(
    const TRuntimeValidationStressSuiteReport &report) {
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
    summary << "passed=" << (caseReport.passed ? "true" : "false")
            << "\r\n";
    summary << "artifactDirectory=" << caseReport.artifactDirectory << "\r\n";
    summary << "iterationCount=" << caseReport.iterationCount << "\r\n";
    summary << "totalRenderedSamples=" << caseReport.totalRenderedSamples
            << "\r\n";
    summary << "totalRenderedBlocks=" << caseReport.totalRenderedBlocks
            << "\r\n";
    summary << "worstCpuLoadPercent="
            << juce::String(caseReport.worstRuntimeStats.cpuLoadPercent, 6)
            << "\r\n";
    summary << "worstMaxProcessMilliseconds="
            << juce::String(caseReport.worstRuntimeStats.maxProcessMilliseconds, 6)
            << "\r\n";
    if (caseReport.failureReason.isNotEmpty())
      summary << "failureReason=" << caseReport.failureReason << "\r\n";
    summary << "\r\n";
  }
  return summary;
}

juce::var makeStressSuiteArtifactBundle(
    const juce::File &artifactDirectory,
    const TRuntimeValidationStressSuiteReport &report) {
  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("stressSummary", artifactDirectory,
                                  artifactDirectory.getChildFile("stress-summary.txt")));

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
      entry->setProperty(
          "bundleRelativePath",
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
  root->setProperty("scope", "stress-suite");
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

void finalizeStressSuiteArtifacts(
    const juce::File &artifactDirectory,
    const TRuntimeValidationStressSuiteReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());
  juce::ignoreUnused(writeTextArtifact(
      artifactDirectory.getChildFile("stress-summary.txt"),
      buildStressSuiteSummaryText(report)));
  juce::ignoreUnused(writeJsonArtifact(
      artifactDirectory.getChildFile("artifact-bundle.json"),
      makeStressSuiteArtifactBundle(artifactDirectory, report)));
}

juce::File makeBenchmarkSuiteArtifactDirectory(const juce::String &suiteId) {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("Benchmark")
      .getChildFile(sanitizePathFragment(suiteId));
}

juce::File makeBenchmarkCaseArtifactDirectory(
    const juce::File &suiteDirectory,
    const TRuntimeValidationGraphFixture &fixture,
    const TRuntimeValidationRenderProfile &profile,
    const TRuntimeValidationStimulusSpec &stimulus) {
  return suiteDirectory.getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                                     sanitizePathFragment(stimulus.stimulusId) + "_" +
                                     sanitizePathFragment(profile.profileId));
}

juce::File makeBenchmarkSuiteHistoryFile(const juce::String &suiteId) {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("Benchmark")
      .getChildFile(sanitizePathFragment(suiteId) + "-history.json");
}