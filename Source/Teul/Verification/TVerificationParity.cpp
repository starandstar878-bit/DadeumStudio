#include "Teul/Verification/TVerificationParity.h"
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
float normalizeSample(float sample) noexcept {
  if (!std::isfinite(sample))
    return sample;
  return (std::abs(sample) < 1.0e-20f) ? 0.0f : sample;
}
juce::File makeArtifactDirectory(const TVerificationGraphFixture &fixture,
                                 const TVerificationRenderProfile &profile,
                                 const TVerificationStimulusSpec &stimulus) {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulVerification")
      .getChildFile("EditableRoundTrip")
      .getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                    sanitizePathFragment(stimulus.stimulusId) + "_" +
                    sanitizePathFragment(profile.profileId));
}
void writeTextArtifact(const juce::File &file, const juce::String &text) {
  juce::ignoreUnused(file.replaceWithText(text, false, false, "\r\n"));
}
void writeJsonArtifact(const juce::File &file, const juce::var &json) {
  juce::ignoreUnused(
      file.replaceWithText(juce::JSON::toString(json, true), false, false, "\r\n"));
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
juce::String buildParitySummaryText(const TVerificationParityReport &report) {
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
  summary << "firstMismatchChannel=" << report.firstMismatchChannel << "\r\n";
  summary << "firstMismatchSample=" << report.firstMismatchSample << "\r\n";
  summary << "exportWarnings=" << report.exportWarningCount << "\r\n";
  summary << "exportErrors=" << report.exportErrorCount << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  return summary;
}
juce::String buildFailureSummaryText(const TVerificationParityReport &report) {
  juce::String text = report.failureReason.trim();
  const auto exportText = report.exportReport.toText().trim();
  if (exportText.isNotEmpty()) {
    if (text.isNotEmpty())
      text << "\r\n\r\n";
    text << exportText;
  }
  return text;
}
juce::var makeParityArtifactBundle(const juce::File &artifactDirectory,
                                   const TVerificationParityReport &report) {
  juce::Array<juce::var> files;
  const auto addArtifactFile = [&](const juce::String &role, const juce::File &file) {
    if (file.getFullPathName().isNotEmpty())
      files.add(makeArtifactFileEntry(role, artifactDirectory, file));
  };
  addArtifactFile("paritySummary", artifactDirectory.getChildFile("parity-summary.txt"));
  addArtifactFile("exportReport", artifactDirectory.getChildFile("export-report.txt"));
  addArtifactFile("failureSummary", artifactDirectory.getChildFile("parity-failure.txt"));
  addArtifactFile("editableExportPackage", artifactDirectory.getChildFile("export-package"));
  addArtifactFile("editableGraph", report.exportReport.graphFile);
  addArtifactFile("editableManifest", report.exportReport.manifestFile);
  addArtifactFile("editableRuntimeData", report.exportReport.runtimeDataFile);
  addArtifactFile("editableExportReport", report.exportReport.reportFile);
  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", "case");
  root->setProperty("graphId", report.graphId);
  root->setProperty("stimulusId", report.stimulusId);
  root->setProperty("profileId", report.profileId);
  root->setProperty("modeId", report.modeId);
  root->setProperty("passed", report.passed);
  root->setProperty("artifactDirectory", artifactDirectory.getFullPathName());
  root->setProperty("importedDocumentLoaded", report.importedDocumentLoaded);
  root->setProperty("totalSamples", report.totalSamples);
  root->setProperty("comparedChannels", report.comparedChannels);
  root->setProperty("firstMismatchChannel", report.firstMismatchChannel);
  root->setProperty("firstMismatchSample", report.firstMismatchSample);
  root->setProperty("maxAbsoluteError", report.maxAbsoluteError);
  root->setProperty("rmsError", report.rmsError);
  root->setProperty("exportWarningCount", report.exportWarningCount);
  root->setProperty("exportErrorCount", report.exportErrorCount);
  if (report.failureReason.isNotEmpty())
    root->setProperty("failureReason", report.failureReason);
  root->setProperty("files", juce::var(files));
  return juce::var(root);
}
void finalizeParityCaseArtifacts(const juce::File &artifactDirectory,
                                 const TVerificationParityReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());
  writeTextArtifact(artifactDirectory.getChildFile("parity-summary.txt"),
                    buildParitySummaryText(report));
  writeTextArtifact(artifactDirectory.getChildFile("export-report.txt"),
                    report.exportReport.toText());
  if (report.failureReason.isNotEmpty()) {
    writeTextArtifact(artifactDirectory.getChildFile("parity-failure.txt"),
                      buildFailureSummaryText(report));
  }
  writeJsonArtifact(artifactDirectory.getChildFile("artifact-bundle.json"),
                    makeParityArtifactBundle(artifactDirectory, report));
}
juce::String buildParitySuiteSummaryText(const TVerificationParitySuiteReport &report) {
  juce::String summary;
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "suiteId=" << report.suiteId << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalCaseCount=" << report.totalCaseCount << "\r\n";
  summary << "passedCaseCount=" << report.passedCaseCount << "\r\n";
  summary << "failedCaseCount=" << report.failedCaseCount << "\r\n\r\n";
  for (const auto &caseReport : report.caseReports) {
    summary << "case=" << caseReport.graphId << "/" << caseReport.stimulusId
            << "/" << caseReport.profileId << "\r\n";
    summary << "passed=" << (caseReport.passed ? "true" : "false") << "\r\n";
    summary << "artifactDirectory=" << caseReport.artifactDirectory << "\r\n";
    if (caseReport.failureReason.isNotEmpty())
      summary << "failureReason=" << caseReport.failureReason << "\r\n";
    summary << "maxAbsoluteError="
            << juce::String(caseReport.maxAbsoluteError, 9) << "\r\n";
    summary << "rmsError=" << juce::String(caseReport.rmsError, 12)
            << "\r\n\r\n";
  }
  return summary;
}
juce::var makeParitySuiteArtifactBundle(const juce::File &artifactDirectory,
                                        const TVerificationParitySuiteReport &report) {
  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("matrixSummary",
                                  artifactDirectory,
                                  artifactDirectory.getChildFile("matrix-summary.txt")));
  juce::Array<juce::var> cases;
  for (const auto &caseReport : report.caseReports) {
    auto *entry = new juce::DynamicObject();
    entry->setProperty("graphId", caseReport.graphId);
    entry->setProperty("stimulusId", caseReport.stimulusId);
    entry->setProperty("profileId", caseReport.profileId);
    entry->setProperty("modeId", caseReport.modeId);
    entry->setProperty("passed", caseReport.passed);
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
  root->setProperty("scope", "suite");
  root->setProperty("suiteId", report.suiteId);
  root->setProperty("passed", report.passed);
  root->setProperty("artifactDirectory", artifactDirectory.getFullPathName());
  root->setProperty("totalCaseCount", report.totalCaseCount);
  root->setProperty("passedCaseCount", report.passedCaseCount);
  root->setProperty("failedCaseCount", report.failedCaseCount);
  root->setProperty("files", juce::var(files));
  root->setProperty("cases", juce::var(cases));
  return juce::var(root);
}
void finalizeParitySuiteArtifacts(const juce::File &artifactDirectory,
                                  const TVerificationParitySuiteReport &report) {
  juce::ignoreUnused(artifactDirectory.createDirectory());
  writeTextArtifact(artifactDirectory.getChildFile("matrix-summary.txt"),
                    buildParitySuiteSummaryText(report));
  writeJsonArtifact(artifactDirectory.getChildFile("artifact-bundle.json"),
                    makeParitySuiteArtifactBundle(artifactDirectory, report));
}
} // namespace
bool runEditableExportRoundTripParity(
    const TNodeRegistry &registry,
    const TVerificationGraphFixture &fixture,
    const TVerificationRenderProfile &profile,
    const TVerificationStimulusSpec &stimulus,
    TVerificationParityReport &reportOut,
    float maxAbsoluteTolerance,
    double rmsTolerance) {
  reportOut = {};
  reportOut.graphId = fixture.fixtureId;
  reportOut.stimulusId = stimulus.stimulusId;
  reportOut.profileId = profile.profileId;
  reportOut.modeId = "editable-roundtrip";
  const auto artifactDirectory = makeArtifactDirectory(fixture, profile, stimulus);
  reportOut.artifactDirectory = artifactDirectory.getFullPathName();
  juce::ignoreUnused(artifactDirectory.deleteRecursively());
  juce::ignoreUnused(artifactDirectory.createDirectory());
  struct ArtifactScope {
    juce::File directory;
    TVerificationParityReport &report;
    ~ArtifactScope() { finalizeParityCaseArtifacts(directory, report); }
  } artifactScope{artifactDirectory, reportOut};
  TVerificationRenderResult sourceRender;
  juce::String sourceRenderError;
  if (!renderGraphWithStimulus(registry, fixture.document, profile, stimulus,
                               sourceRender, &sourceRenderError)) {
    reportOut.failureReason = "Source runtime render failed: " + sourceRenderError;
    return false;
  }
  TExportOptions options;
  options.mode = TExportMode::EditableGraph;
  options.dryRunOnly = false;
  options.outputDirectory = artifactDirectory.getChildFile("export-package");
  options.projectRootDirectory = juce::File::getCurrentWorkingDirectory();
  TExportReport exportReport;
  const auto exportResult =
      TExporter::exportToDirectory(fixture.document, registry, options, exportReport);
  reportOut.exportReport = exportReport;
  reportOut.exportWarningCount = exportReport.warningCount();
  reportOut.exportErrorCount = exportReport.errorCount();
  if (exportResult.failed()) {
    reportOut.failureReason =
        "EditableGraph export failed: " + exportResult.getErrorMessage();
    return false;
  }
  TGraphDocument importedDocument;
  const auto importResult =
      TExporter::importEditableGraphPackage(options.outputDirectory, importedDocument);
  if (importResult.failed()) {
    reportOut.failureReason =
        "EditableGraph import failed: " + importResult.getErrorMessage();
    return false;
  }
  reportOut.importedDocumentLoaded = true;
  TVerificationRenderResult importedRender;
  juce::String importedRenderError;
  if (!renderGraphWithStimulus(registry, importedDocument, profile, stimulus,
                               importedRender, &importedRenderError)) {
    reportOut.failureReason = "Imported runtime render failed: " + importedRenderError;
    return false;
  }
  const int channels = juce::jmin(sourceRender.audioBuffer.getNumChannels(),
                                  importedRender.audioBuffer.getNumChannels());
  const int samples = juce::jmin(sourceRender.audioBuffer.getNumSamples(),
                                 importedRender.audioBuffer.getNumSamples());
  reportOut.totalSamples = samples;
  reportOut.comparedChannels = channels;
  if (channels <= 0 || samples <= 0) {
    reportOut.failureReason = "Parity render produced an empty audio buffer.";
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
        reportOut.failureReason = "Parity compare encountered NaN or Inf.";
        return false;
      }
      const double error = static_cast<double>(a) - static_cast<double>(b);
      const float absError = static_cast<float>(std::abs(error));
      if (absError > reportOut.maxAbsoluteError)
        reportOut.maxAbsoluteError = absError;
      squaredErrorSum += error * error;
      ++comparedSampleCount;
      if (reportOut.firstMismatchSample < 0 && absError > maxAbsoluteTolerance) {
        reportOut.firstMismatchChannel = channel;
        reportOut.firstMismatchSample = sampleIndex;
      }
    }
  }
  reportOut.rmsError =
      comparedSampleCount > 0
          ? std::sqrt(squaredErrorSum / static_cast<double>(comparedSampleCount))
          : 0.0;
  reportOut.passed = reportOut.maxAbsoluteError <= maxAbsoluteTolerance &&
                     reportOut.rmsError <= rmsTolerance;
  return reportOut.passed;
}
bool runInitialG1StaticParitySmoke(const TNodeRegistry &registry,
                                   TVerificationParityReport &reportOut) {
  const auto fixtures = makeRepresentativeVerificationGraphSet(registry);
  for (const auto &fixture : fixtures) {
    if (fixture.fixtureId == "G1") {
      return runEditableExportRoundTripParity(
          registry, fixture, makePrimaryVerificationRenderProfile(),
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
bool runRepresentativePrimaryParityMatrix(
    const TNodeRegistry &registry,
    TVerificationParitySuiteReport &reportOut) {
  reportOut = {};
  reportOut.suiteId = "representative-primary";
  const auto suiteArtifactDirectory = juce::File::getCurrentWorkingDirectory()
                                          .getChildFile("Builds")
                                          .getChildFile("TeulVerification")
                                          .getChildFile("EditableRoundTrip")
                                          .getChildFile("RepresentativeMatrix_primary");
  reportOut.artifactDirectory = suiteArtifactDirectory.getFullPathName();
  juce::ignoreUnused(suiteArtifactDirectory.deleteRecursively());
  juce::ignoreUnused(suiteArtifactDirectory.createDirectory());
  struct ArtifactScope {
    juce::File directory;
    TVerificationParitySuiteReport &report;
    ~ArtifactScope() { finalizeParitySuiteArtifacts(directory, report); }
  } artifactScope{suiteArtifactDirectory, reportOut};
  struct MatrixCase {
    juce::String fixtureId;
    TVerificationRenderProfile profile;
    TVerificationStimulusSpec stimulus;
  };
  std::vector<MatrixCase> matrixCases;
  matrixCases.push_back(
      {"G1", makePrimaryVerificationRenderProfile(), makeStaticRenderStimulus()});
  matrixCases.push_back({"G2",
                         makePrimaryVerificationRenderProfile(),
                         makeSweepAutomationStimulus("LowPass", "cutoff", 320.0f,
                                                     7200.0f)});
  matrixCases.push_back({"G3",
                         makePrimaryVerificationRenderProfile(),
                         makeSweepAutomationStimulus("Stereo Pan", "pan", -1.0f,
                                                     1.0f)});
  matrixCases.push_back(
      {"G4", makePrimaryVerificationRenderProfile(), makeMidiPhraseStimulus()});
  matrixCases.push_back({"G5",
                         makePrimaryVerificationRenderProfile(),
                         makeStepAutomationStimulus("Delay", "mix", 0.15f, 0.75f,
                                                    0.25)});
  const auto fixtures = makeRepresentativeVerificationGraphSet(registry);
  for (const auto &matrixCase : matrixCases) {
    TVerificationParityReport caseReport;
    const TVerificationGraphFixture *fixtureMatch = nullptr;
    for (const auto &fixture : fixtures) {
      if (fixture.fixtureId == matrixCase.fixtureId) {
        fixtureMatch = &fixture;
        break;
      }
    }
    if (fixtureMatch == nullptr) {
      caseReport.graphId = matrixCase.fixtureId;
      caseReport.stimulusId = matrixCase.stimulus.stimulusId;
      caseReport.profileId = matrixCase.profile.profileId;
      caseReport.modeId = "editable-roundtrip";
      caseReport.failureReason =
          "Representative parity matrix fixture was not found.";
      caseReport.passed = false;
    } else {
      caseReport.passed = runEditableExportRoundTripParity(
          registry, *fixtureMatch, matrixCase.profile, matrixCase.stimulus,
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
  return reportOut.passed;
}
} // namespace Teul
