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
  auto dir = juce::File::getCurrentWorkingDirectory()
                 .getChildFile("Builds")
                 .getChildFile("TeulVerification")
                 .getChildFile("EditableRoundTrip")
                 .getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                               sanitizePathFragment(stimulus.stimulusId) + "_" +
                               sanitizePathFragment(profile.profileId));
  return dir;
}
void writeTextArtifact(const juce::File &file, const juce::String &text) {
  juce::ignoreUnused(file.replaceWithText(text, false, false, "\r\n"));
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
  TVerificationRenderResult sourceRender;
  juce::String sourceRenderError;
  if (!renderGraphWithStimulus(registry, fixture.document, profile, stimulus,
                               sourceRender, &sourceRenderError)) {
    reportOut.failureReason = "Source runtime render failed: " + sourceRenderError;
    writeTextArtifact(artifactDirectory.getChildFile("parity-failure.txt"),
                      reportOut.failureReason);
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
    reportOut.failureReason = "EditableGraph export failed: " + exportResult.getErrorMessage();
    writeTextArtifact(artifactDirectory.getChildFile("parity-failure.txt"),
                      reportOut.failureReason + "\r\n\r\n" + exportReport.toText());
    return false;
  }
  TGraphDocument importedDocument;
  const auto importResult =
      TExporter::importEditableGraphPackage(options.outputDirectory, importedDocument);
  if (importResult.failed()) {
    reportOut.failureReason = "EditableGraph import failed: " + importResult.getErrorMessage();
    writeTextArtifact(artifactDirectory.getChildFile("parity-failure.txt"),
                      reportOut.failureReason + "\r\n\r\n" + exportReport.toText());
    return false;
  }
  reportOut.importedDocumentLoaded = true;
  TVerificationRenderResult importedRender;
  juce::String importedRenderError;
  if (!renderGraphWithStimulus(registry, importedDocument, profile, stimulus,
                               importedRender, &importedRenderError)) {
    reportOut.failureReason = "Imported runtime render failed: " + importedRenderError;
    writeTextArtifact(artifactDirectory.getChildFile("parity-failure.txt"),
                      reportOut.failureReason + "\r\n\r\n" + exportReport.toText());
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
    writeTextArtifact(artifactDirectory.getChildFile("parity-failure.txt"),
                      reportOut.failureReason);
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
        writeTextArtifact(artifactDirectory.getChildFile("parity-failure.txt"),
                          reportOut.failureReason);
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
  reportOut.rmsError = comparedSampleCount > 0
                           ? std::sqrt(squaredErrorSum / static_cast<double>(comparedSampleCount))
                           : 0.0;
  reportOut.passed = reportOut.maxAbsoluteError <= maxAbsoluteTolerance &&
                     reportOut.rmsError <= rmsTolerance;
  juce::String summary;
  summary << "graphId=" << reportOut.graphId << "\r\n";
  summary << "stimulusId=" << reportOut.stimulusId << "\r\n";
  summary << "profileId=" << reportOut.profileId << "\r\n";
  summary << "modeId=" << reportOut.modeId << "\r\n";
  summary << "passed=" << (reportOut.passed ? "true" : "false") << "\r\n";
  summary << "totalSamples=" << reportOut.totalSamples << "\r\n";
  summary << "comparedChannels=" << reportOut.comparedChannels << "\r\n";
  summary << "maxAbsoluteError=" << juce::String(reportOut.maxAbsoluteError, 9) << "\r\n";
  summary << "rmsError=" << juce::String(reportOut.rmsError, 12) << "\r\n";
  summary << "firstMismatchChannel=" << reportOut.firstMismatchChannel << "\r\n";
  summary << "firstMismatchSample=" << reportOut.firstMismatchSample << "\r\n";
  summary << "exportWarnings=" << reportOut.exportWarningCount << "\r\n";
  summary << "exportErrors=" << reportOut.exportErrorCount << "\r\n";
  if (reportOut.failureReason.isNotEmpty())
    summary << "failureReason=" << reportOut.failureReason << "\r\n";
  writeTextArtifact(artifactDirectory.getChildFile("parity-summary.txt"), summary);
  writeTextArtifact(artifactDirectory.getChildFile("export-report.txt"), exportReport.toText());
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
} // namespace Teul