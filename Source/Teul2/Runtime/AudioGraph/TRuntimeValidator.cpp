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

} // namespace

std::vector<TRuntimeValidationGraphFixture>
TRuntimeValidator::makeRepresentativeGraphSet(const TNodeRegistry &registry) {
  std::vector<TRuntimeValidationGraphFixture> fixtures;
  for (const auto &legacyFixture : makeRepresentativeVerificationGraphSet(registry))
    fixtures.push_back(convertLegacyFixture(legacyFixture));
  return fixtures;
}

TRuntimeValidationRenderProfile TRuntimeValidator::makePrimaryRenderProfile() {
  return {"primary", 48000.0, 128, 2, 2.0};
}

TRuntimeValidationRenderProfile
TRuntimeValidator::makeSecondaryRenderProfile() {
  return {"secondary", 48000.0, 480, 2, 2.0};
}

TRuntimeValidationRenderProfile
TRuntimeValidator::makeExtendedRenderProfile() {
  return {"extended", 96000.0, 128, 2, 2.0};
}

TRuntimeValidationStimulusSpec TRuntimeValidator::makeStaticRenderStimulus() {
  return {"S1", "Static Render", TRuntimeValidationStimulusKind::StaticRender,
          {}, {}};
}
TRuntimeValidationStimulusSpec TRuntimeValidator::makeStepAutomationStimulus(
    const juce::String &nodeLabel, const juce::String &paramKey,
    float startValue, float endValue, double stepIntervalSeconds) {
  TRuntimeValidationStimulusSpec stimulus;
  stimulus.stimulusId = "S2";
  stimulus.displayName = "Step Automation";
  stimulus.kind = TRuntimeValidationStimulusKind::StepAutomation;
  stimulus.automationLanes.push_back(
      {nodeLabel, paramKey, TRuntimeValidationAutomationMode::Step,
       startValue, endValue, stepIntervalSeconds});
  return stimulus;
}

TRuntimeValidationStimulusSpec TRuntimeValidator::makeSweepAutomationStimulus(
    const juce::String &nodeLabel, const juce::String &paramKey,
    float startValue, float endValue) {
  TRuntimeValidationStimulusSpec stimulus;
  stimulus.stimulusId = "S3";
  stimulus.displayName = "Sweep Automation";
  stimulus.kind = TRuntimeValidationStimulusKind::SweepAutomation;
  stimulus.automationLanes.push_back(
      {nodeLabel, paramKey, TRuntimeValidationAutomationMode::Linear,
       startValue, endValue, 0.25});
  return stimulus;
}

TRuntimeValidationStimulusSpec TRuntimeValidator::makeMidiPhraseStimulus() {
  TRuntimeValidationStimulusSpec stimulus;
  stimulus.stimulusId = "S4";
  stimulus.displayName = "MIDI Phrase";
  stimulus.kind = TRuntimeValidationStimulusKind::MidiPhrase;
  const double sampleRate = 48000.0;
  auto sampleOffsetForSeconds = [sampleRate](double seconds) {
    return juce::roundToInt(seconds * sampleRate);
  };
  stimulus.midiEvents.push_back({sampleOffsetForSeconds(0.00),
                                 juce::MidiMessage::noteOn(
                                     1, 60, (juce::uint8)96)});
  stimulus.midiEvents.push_back({sampleOffsetForSeconds(0.35),
                                 juce::MidiMessage::noteOff(1, 60)});
  stimulus.midiEvents.push_back({sampleOffsetForSeconds(0.55),
                                 juce::MidiMessage::noteOn(
                                     1, 64, (juce::uint8)110)});
  stimulus.midiEvents.push_back({sampleOffsetForSeconds(0.95),
                                 juce::MidiMessage::noteOff(1, 64)});
  stimulus.midiEvents.push_back({sampleOffsetForSeconds(1.15),
                                 juce::MidiMessage::noteOn(
                                     1, 67, (juce::uint8)100)});
  stimulus.midiEvents.push_back({sampleOffsetForSeconds(1.65),
                                 juce::MidiMessage::noteOff(1, 67)});
  return stimulus;
}

bool TRuntimeValidator::renderDocumentWithStimulus(
    const TNodeRegistry &registry, const TTeulDocument &document,
    const TRuntimeValidationRenderProfile &profile,
    const TRuntimeValidationStimulusSpec &stimulus,
    TRuntimeValidationRenderResult &resultOut,
    juce::String *errorMessageOut) {
  if (profile.sampleRate <= 0.0 || profile.blockSize <= 0 ||
      profile.outputChannels <= 0 || profile.durationSeconds <= 0.0) {
    writeError(errorMessageOut, "Invalid verification render profile.");
    return false;
  }

  std::vector<std::pair<NodeId, TRuntimeValidationAutomationLane>> resolvedLanes;
  resolvedLanes.reserve(stimulus.automationLanes.size());
  for (const auto &lane : stimulus.automationLanes) {
    const NodeId nodeId = findNodeIdByLabel(document, lane.nodeLabel);
    if (nodeId == kInvalidNodeId) {
      writeError(errorMessageOut,
                 "Verification stimulus references a missing node label: " +
                     lane.nodeLabel);
      return false;
    }
    resolvedLanes.push_back({nodeId, lane});
  }

  std::vector<TRuntimeValidationMidiEvent> midiEvents = stimulus.midiEvents;
  std::sort(midiEvents.begin(), midiEvents.end(),
            [](const auto &lhs, const auto &rhs) {
              return lhs.sampleOffset < rhs.sampleOffset;
            });

  TTeulRuntime runtime(&registry);
  if (!runtime.buildGraph(document)) {
    writeError(errorMessageOut, "Failed to build verification graph.");
    return false;
  }

  runtime.setCurrentChannelLayout(0, profile.outputChannels);
  runtime.prepareToPlay(profile.sampleRate, profile.blockSize);

  const int totalSamples = juce::jmax(
      1, juce::roundToInt(profile.durationSeconds * profile.sampleRate));
  resultOut.graphName = document.meta.name;
  resultOut.stimulusId = stimulus.stimulusId;
  resultOut.profileId = profile.profileId;
  resultOut.totalSamples = totalSamples;
  resultOut.audioBuffer.setSize(profile.outputChannels, totalSamples, false,
                                false, true);
  resultOut.audioBuffer.clear();
  resultOut.renderedBlockCount = 0;

  std::size_t midiEventIndex = 0;
  for (int blockStart = 0; blockStart < totalSamples;
       blockStart += profile.blockSize) {
    const int blockSamples =
        juce::jmin(profile.blockSize, totalSamples - blockStart);
    for (const auto &resolvedLane : resolvedLanes) {
      const float value = valueForLaneAtSample(
          resolvedLane.second, blockStart, totalSamples, profile.sampleRate);
      runtime.queueParameterChange(resolvedLane.first,
                                   resolvedLane.second.paramKey, value);
    }

    juce::AudioBuffer<float> blockBuffer(profile.outputChannels, blockSamples);
    juce::MidiBuffer midiBuffer;
    while (midiEventIndex < midiEvents.size() &&
           midiEvents[midiEventIndex].sampleOffset <
               (blockStart + blockSamples)) {
      const auto &event = midiEvents[midiEventIndex];
      if (event.sampleOffset >= blockStart)
        midiBuffer.addEvent(event.message, event.sampleOffset - blockStart);
      ++midiEventIndex;
    }

    runtime.processBlock(blockBuffer, midiBuffer);
    for (int channel = 0; channel < profile.outputChannels; ++channel) {
      resultOut.audioBuffer.copyFrom(channel, blockStart, blockBuffer, channel,
                                     0, blockSamples);
    }
    ++resultOut.renderedBlockCount;
  }

  resultOut.runtimeStats = runtime.getRuntimeStats();
  return true;
}
bool TRuntimeValidator::runEditableExportRoundTripParity(
    const TNodeRegistry &registry,
    const TRuntimeValidationGraphFixture &fixture,
    const TRuntimeValidationRenderProfile &profile,
    const TRuntimeValidationStimulusSpec &stimulus,
    TRuntimeValidationParityReport &reportOut, float maxAbsoluteTolerance,
    double rmsTolerance) {
  reportOut = {};
  reportOut.graphId = fixture.fixtureId;
  reportOut.stimulusId = stimulus.stimulusId;
  reportOut.profileId = profile.profileId;
  reportOut.modeId = "editable-roundtrip";

  const auto artifactDirectory =
      makeCaseArtifactDirectory(fixture, profile, stimulus);
  reportOut.artifactDirectory = artifactDirectory.getFullPathName();
  juce::ignoreUnused(artifactDirectory.deleteRecursively());
  juce::ignoreUnused(artifactDirectory.createDirectory());

  TRuntimeValidationRenderResult sourceRender;
  juce::String sourceRenderError;
  if (!renderDocumentWithStimulus(registry, fixture.document, profile, stimulus,
                                  sourceRender, &sourceRenderError)) {
    reportOut.failureReason =
        "Source runtime render failed: " + sourceRenderError;
    finalizeCaseArtifacts(artifactDirectory, reportOut);
    return false;
  }

  TGraphDocument exportDocument;
  if (!TGraphCompiler::compileLegacyDocument(exportDocument, fixture.document)) {
    reportOut.failureReason =
        "EditableGraph export failed: Teul2 document compile to legacy graph failed.";
    finalizeCaseArtifacts(artifactDirectory, reportOut);
    return false;
  }

  TExportOptions options;
  options.mode = TExportMode::EditableGraph;
  options.dryRunOnly = false;
  options.outputDirectory = artifactDirectory.getChildFile("export-package");
  options.projectRootDirectory = juce::File::getCurrentWorkingDirectory();

  TExportReport exportReport;
  const auto exportResult =
      TExporter::exportToDirectory(exportDocument, registry, options,
                                   exportReport);
  reportOut.exportReport = exportReport;
  reportOut.exportWarningCount = exportReport.warningCount();
  reportOut.exportErrorCount = exportReport.errorCount();
  if (exportResult.failed()) {
    reportOut.failureReason =
        "EditableGraph export failed: " + exportResult.getErrorMessage();
    finalizeCaseArtifacts(artifactDirectory, reportOut);
    return false;
  }

  TTeulDocument importedDocument;
  const auto importResult =
      TDocumentStore::importEditableGraphPackage(options.outputDirectory,
                                                 importedDocument);
  if (importResult.failed()) {
    reportOut.failureReason =
        "EditableGraph import failed: " + importResult.getErrorMessage();
    finalizeCaseArtifacts(artifactDirectory, reportOut);
    return false;
  }
  reportOut.importedDocumentLoaded = true;

  TRuntimeValidationRenderResult importedRender;
  juce::String importedRenderError;
  if (!renderDocumentWithStimulus(registry, importedDocument, profile,
                                  stimulus, importedRender,
                                  &importedRenderError)) {
    reportOut.failureReason =
        "Imported runtime render failed: " + importedRenderError;
    finalizeCaseArtifacts(artifactDirectory, reportOut);
    return false;
  }

  const int channels = juce::jmin(sourceRender.audioBuffer.getNumChannels(),
                                  importedRender.audioBuffer.getNumChannels());
  const int samples = juce::jmin(sourceRender.audioBuffer.getNumSamples(),
                                 importedRender.audioBuffer.getNumSamples());
  reportOut.totalSamples = samples;
  reportOut.comparedChannels = channels;
  if (channels <= 0 || samples <= 0) {
    reportOut.failureReason =
        "Parity render produced an empty audio buffer.";
    finalizeCaseArtifacts(artifactDirectory, reportOut);
    return false;
  }

  double squaredErrorSum = 0.0;
  int comparedSampleCount = 0;
  for (int channel = 0; channel < channels; ++channel) {
    const float *lhs = sourceRender.audioBuffer.getReadPointer(channel);
    const float *rhs = importedRender.audioBuffer.getReadPointer(channel);
    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex) {
      const float a = normalizeSample(lhs[sampleIndex]);
      const float b = normalizeSample(rhs[sampleIndex]);
      if (!std::isfinite(a) || !std::isfinite(b)) {
        reportOut.firstMismatchChannel = channel;
        reportOut.firstMismatchSample = sampleIndex;
        reportOut.failureReason =
            "Parity compare encountered NaN or Inf.";
        finalizeCaseArtifacts(artifactDirectory, reportOut);
        return false;
      }

      const double error = static_cast<double>(a) - static_cast<double>(b);
      const float absError = static_cast<float>(std::abs(error));
      reportOut.maxAbsoluteError = juce::jmax(reportOut.maxAbsoluteError,
                                              absError);
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
  finalizeCaseArtifacts(artifactDirectory, reportOut);
  return reportOut.passed;
}

bool TRuntimeValidator::runInitialG1StaticParitySmoke(
    const TNodeRegistry &registry,
    TRuntimeValidationParityReport &reportOut) {
  const auto fixtures = makeRepresentativeGraphSet(registry);
  for (const auto &fixture : fixtures) {
    if (fixture.fixtureId == "G1") {
      return runEditableExportRoundTripParity(
          registry, fixture, makePrimaryRenderProfile(),
          makeStaticRenderStimulus(), reportOut);
    }
  }
  reportOut = {};
  reportOut.graphId = "G1";
  reportOut.stimulusId = "S1";
  reportOut.profileId = "primary";
  reportOut.modeId = "editable-roundtrip";
  reportOut.failureReason = "G1 fixture was not found.";
  return false;
}

bool TRuntimeValidator::runRepresentativePrimaryParityMatrix(
    const TNodeRegistry &registry,
    TRuntimeValidationParitySuiteReport &reportOut) {
  reportOut = {};
  reportOut.suiteId = "representative-primary";
  const auto artifactDirectory = juce::File::getCurrentWorkingDirectory()
                                     .getChildFile("Builds")
                                     .getChildFile("TeulVerification")
                                     .getChildFile("EditableRoundTrip")
                                     .getChildFile(
                                         "RepresentativeMatrix_primary");
  reportOut.artifactDirectory = artifactDirectory.getFullPathName();
  juce::ignoreUnused(artifactDirectory.deleteRecursively());
  juce::ignoreUnused(artifactDirectory.createDirectory());

  struct MatrixCase {
    juce::String fixtureId;
    TRuntimeValidationRenderProfile profile;
    TRuntimeValidationStimulusSpec stimulus;
  };

  std::vector<MatrixCase> matrixCases;
  matrixCases.push_back(
      {"G1", makePrimaryRenderProfile(), makeStaticRenderStimulus()});
  matrixCases.push_back(
      {"G2", makePrimaryRenderProfile(),
       makeSweepAutomationStimulus("LowPass", "cutoff", 320.0f, 7200.0f)});
  matrixCases.push_back(
      {"G3", makePrimaryRenderProfile(),
       makeSweepAutomationStimulus("Stereo Pan", "pan", -1.0f, 1.0f)});
  matrixCases.push_back(
      {"G4", makePrimaryRenderProfile(), makeMidiPhraseStimulus()});
  matrixCases.push_back(
      {"G5", makePrimaryRenderProfile(),
       makeStepAutomationStimulus("Delay", "mix", 0.15f, 0.75f, 0.25)});

  const auto fixtures = makeRepresentativeGraphSet(registry);
  for (const auto &matrixCase : matrixCases) {
    TRuntimeValidationParityReport caseReport;
    const auto fixtureIt = std::find_if(
        fixtures.begin(), fixtures.end(), [&](const auto &fixture) {
          return fixture.fixtureId == matrixCase.fixtureId;
        });
    if (fixtureIt == fixtures.end()) {
      caseReport.graphId = matrixCase.fixtureId;
      caseReport.stimulusId = matrixCase.stimulus.stimulusId;
      caseReport.profileId = matrixCase.profile.profileId;
      caseReport.modeId = "editable-roundtrip";
      caseReport.failureReason =
          "Representative parity matrix fixture was not found.";
      caseReport.passed = false;
    } else {
      caseReport.passed = runEditableExportRoundTripParity(
          registry, *fixtureIt, matrixCase.profile, matrixCase.stimulus,
          caseReport);
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
  finalizeSuiteArtifacts(artifactDirectory, reportOut);
  return reportOut.passed;
}

} // namespace Teul