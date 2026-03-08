#include "Teul/Verification/TVerificationGoldenAudio.h"
#include <cmath>
namespace Teul {
namespace {
struct GoldenCaseSpec {
  juce::String fixtureId;
  TVerificationRenderProfile profile;
  TVerificationStimulusSpec stimulus;
};
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
std::vector<GoldenCaseSpec> makeRepresentativeGoldenCases() {
  std::vector<GoldenCaseSpec> cases;
  cases.push_back({"G1", makePrimaryVerificationRenderProfile(), makeStaticRenderStimulus()});
  cases.push_back({"G2", makePrimaryVerificationRenderProfile(), makeSweepAutomationStimulus("LowPass", "cutoff", 320.0f, 7200.0f)});
  cases.push_back({"G3", makePrimaryVerificationRenderProfile(), makeSweepAutomationStimulus("Stereo Pan", "pan", -1.0f, 1.0f)});
  cases.push_back({"G4", makePrimaryVerificationRenderProfile(), makeMidiPhraseStimulus()});
  cases.push_back({"G5", makePrimaryVerificationRenderProfile(), makeStepAutomationStimulus("Delay", "mix", 0.15f, 0.75f, 0.25)});
  return cases;
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
juce::File caseDirectoryFor(const juce::File &root,
                            const TVerificationGraphFixture &fixture,
                            const TVerificationRenderProfile &profile,
                            const TVerificationStimulusSpec &stimulus) {
  return root.getChildFile(sanitizePathFragment(fixture.fixtureId) + "_" +
                           sanitizePathFragment(stimulus.stimulusId) + "_" +
                           sanitizePathFragment(profile.profileId));
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
bool writeWaveFile(const juce::File &file,
                   const juce::AudioBuffer<float> &buffer,
                   double sampleRate,
                   juce::String *errorMessageOut) {
  if (sampleRate <= 0.0 || buffer.getNumChannels() <= 0 ||
      buffer.getNumSamples() <= 0) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Invalid golden audio buffer.";
    return false;
  }
  juce::ignoreUnused(file.getParentDirectory().createDirectory());
  auto output = file.createOutputStream();
  if (output == nullptr) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Failed to create golden audio output stream.";
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
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Failed to create golden audio WAV writer.";
    return false;
  }
  if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples())) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Failed to write golden audio WAV data.";
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
  std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
  if (reader == nullptr) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Failed to open golden audio WAV file.";
    return false;
  }
  sampleRateOut = reader->sampleRate;
  const int channels = static_cast<int>(reader->numChannels);
  const int samples = static_cast<int>(reader->lengthInSamples);
  if (channels <= 0 || samples <= 0) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Golden audio WAV file is empty.";
    return false;
  }
  bufferOut.setSize(channels, samples, false, false, true);
  if (!reader->read(&bufferOut, 0, samples, 0, true, true)) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Failed to read golden audio WAV data.";
    return false;
  }
  return true;
}
juce::var makeGoldenMetadata(const TVerificationGraphFixture &fixture,
                             const TVerificationRenderProfile &profile,
                             const TVerificationStimulusSpec &stimulus,
                             const TVerificationRenderResult &renderResult,
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
  root->setProperty("baselineWave", relativeArtifactPath(caseDirectory, caseDirectory.getChildFile("golden-output.wav")));
  root->setProperty("recordedAtUtcMilliseconds", juce::Time::getCurrentTime().toMilliseconds());
  return juce::var(root);
}
juce::String buildRecordSummaryText(const TVerificationGoldenAudioReport &report,
                                    const TVerificationRenderProfile &profile) {
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
juce::String buildVerifySummaryText(const TVerificationGoldenAudioReport &report) {
  juce::String summary;
  summary << "graphId=" << report.graphId << "\r\n";
  summary << "stimulusId=" << report.stimulusId << "\r\n";
  summary << "profileId=" << report.profileId << "\r\n";
  summary << "modeId=" << report.modeId << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "baselineExists=" << (report.baselineExists ? "true" : "false") << "\r\n";
  summary << "baselineDirectory=" << report.baselineDirectory << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "totalSamples=" << report.totalSamples << "\r\n";
  summary << "comparedChannels=" << report.comparedChannels << "\r\n";
  summary << "maxAbsoluteError=" << juce::String(report.maxAbsoluteError, 9) << "\r\n";
  summary << "rmsError=" << juce::String(report.rmsError, 12) << "\r\n";
  summary << "firstMismatchChannel=" << report.firstMismatchChannel << "\r\n";
  summary << "firstMismatchSample=" << report.firstMismatchSample << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  return summary;
}
juce::var makeCaseArtifactBundle(const juce::File &rootDirectory,
                                 const TVerificationGoldenAudioReport &report,
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
    addFile("goldenMetadata", rootDirectory.getChildFile("golden-metadata.json"));
    addFile("goldenOutput", rootDirectory.getChildFile("golden-output.wav"));
  }
  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", verifyMode ? "golden-verify-case" : "golden-record-case");
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
juce::String buildSuiteSummaryText(const TVerificationGoldenAudioSuiteReport &report) {
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
    summary << "case=" << caseReport.graphId << "/" << caseReport.stimulusId << "/"
            << caseReport.profileId << "\r\n";
    summary << "passed=" << (caseReport.passed ? "true" : "false") << "\r\n";
    summary << "baselineDirectory=" << caseReport.baselineDirectory << "\r\n";
    if (caseReport.artifactDirectory.isNotEmpty())
      summary << "artifactDirectory=" << caseReport.artifactDirectory << "\r\n";
    if (caseReport.failureReason.isNotEmpty())
      summary << "failureReason=" << caseReport.failureReason << "\r\n";
    summary << "\r\n";
  }
  return summary;
}
juce::var makeSuiteArtifactBundle(const juce::File &rootDirectory,
                                  const TVerificationGoldenAudioSuiteReport &report,
                                  bool verifyMode) {
  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry(verifyMode ? "goldenVerifySummary" : "goldenRecordSummary",
                                  rootDirectory,
                                  rootDirectory.getChildFile("golden-suite-summary.txt")));
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
  root->setProperty("scope", verifyMode ? "golden-verify-suite" : "golden-record-suite");
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
void finalizeRecordCaseArtifacts(const juce::File &caseDirectory,
                                 const TVerificationGoldenAudioReport &report,
                                 const TVerificationRenderProfile &profile) {
  writeTextArtifact(caseDirectory.getChildFile("golden-summary.txt"),
                    buildRecordSummaryText(report, profile));
  writeJsonArtifact(caseDirectory.getChildFile("artifact-bundle.json"),
                    makeCaseArtifactBundle(caseDirectory, report, false));
}
void finalizeVerifyCaseArtifacts(const juce::File &artifactDirectory,
                                 const TVerificationGoldenAudioReport &report) {
  writeTextArtifact(artifactDirectory.getChildFile("golden-summary.txt"),
                    buildVerifySummaryText(report));
  if (report.failureReason.isNotEmpty()) {
    writeTextArtifact(artifactDirectory.getChildFile("golden-failure.txt"),
                      report.failureReason);
  }
  writeJsonArtifact(artifactDirectory.getChildFile("artifact-bundle.json"),
                    makeCaseArtifactBundle(artifactDirectory, report, true));
}
void finalizeSuiteArtifacts(const juce::File &directory,
                           const TVerificationGoldenAudioSuiteReport &report,
                           bool verifyMode) {
  juce::ignoreUnused(directory.createDirectory());
  writeTextArtifact(directory.getChildFile("golden-suite-summary.txt"),
                    buildSuiteSummaryText(report));
  writeJsonArtifact(directory.getChildFile("artifact-bundle.json"),
                    makeSuiteArtifactBundle(directory, report, verifyMode));
}
bool compareWithGolden(const TVerificationRenderResult &renderResult,
                       const juce::AudioBuffer<float> &goldenBuffer,
                       TVerificationGoldenAudioReport &reportOut,
                       float maxAbsoluteTolerance,
                       double rmsTolerance) {
  const int channels = juce::jmin(renderResult.audioBuffer.getNumChannels(),
                                  goldenBuffer.getNumChannels());
  const int samples = juce::jmin(renderResult.audioBuffer.getNumSamples(),
                                 goldenBuffer.getNumSamples());
  reportOut.totalSamples = samples;
  reportOut.comparedChannels = channels;
  if (channels <= 0 || samples <= 0) {
    reportOut.failureReason = "Golden audio compare produced an empty audio buffer.";
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
        reportOut.failureReason = "Golden audio compare encountered NaN or Inf.";
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
                           ? std::sqrt(squaredErrorSum /
                                       static_cast<double>(comparedSampleCount))
                           : 0.0;
  reportOut.passed = reportOut.maxAbsoluteError <= maxAbsoluteTolerance &&
                     reportOut.rmsError <= rmsTolerance;
  if (!reportOut.passed && reportOut.failureReason.isEmpty()) {
    reportOut.failureReason = "Golden audio output drift exceeded tolerance.";
  }
  return reportOut.passed;
}
} // namespace
bool runRepresentativeGoldenAudioRecord(const TNodeRegistry &registry,
                                        TVerificationGoldenAudioSuiteReport &reportOut) {
  reportOut = {};
  reportOut.suiteId = "representative-primary";
  reportOut.modeId = "record";
  const auto suiteDirectory = baselineSuiteDirectory();
  reportOut.baselineDirectory = suiteDirectory.getFullPathName();
  juce::ignoreUnused(suiteDirectory.createDirectory());
  const auto fixtures = makeRepresentativeVerificationGraphSet(registry);
  const auto cases = makeRepresentativeGoldenCases();
  for (const auto &caseSpec : cases) {
    TVerificationGoldenAudioReport caseReport;
    caseReport.graphId = caseSpec.fixtureId;
    caseReport.stimulusId = caseSpec.stimulus.stimulusId;
    caseReport.profileId = caseSpec.profile.profileId;
    caseReport.modeId = "record";
    const auto *fixture = findFixtureById(fixtures, caseSpec.fixtureId);
    if (fixture == nullptr) {
      caseReport.failureReason = "Representative golden fixture was not found.";
    } else {
      const auto caseDirectory = caseDirectoryFor(suiteDirectory, *fixture,
                                                  caseSpec.profile,
                                                  caseSpec.stimulus);
      juce::ignoreUnused(caseDirectory.deleteRecursively());
      juce::ignoreUnused(caseDirectory.createDirectory());
      caseReport.baselineDirectory = caseDirectory.getFullPathName();
      TVerificationRenderResult renderResult;
      juce::String renderError;
      if (!renderGraphWithStimulus(registry, fixture->document, caseSpec.profile,
                                   caseSpec.stimulus, renderResult, &renderError)) {
        caseReport.failureReason = "Golden audio record render failed: " + renderError;
      } else {
        caseReport.totalSamples = renderResult.totalSamples;
        const auto waveFile = caseDirectory.getChildFile("golden-output.wav");
        juce::String writeError;
        if (!writeWaveFile(waveFile, renderResult.audioBuffer, caseSpec.profile.sampleRate,
                           &writeError)) {
          caseReport.failureReason = writeError;
        } else {
          writeJsonArtifact(caseDirectory.getChildFile("golden-metadata.json"),
                            makeGoldenMetadata(*fixture, caseSpec.profile,
                                               caseSpec.stimulus, renderResult,
                                               caseDirectory));
          caseReport.baselineExists = true;
          caseReport.passed = true;
        }
        finalizeRecordCaseArtifacts(caseDirectory, caseReport, caseSpec.profile);
      }
    }
    ++reportOut.totalCaseCount;
    if (caseReport.passed)
      ++reportOut.passedCaseCount;
    else
      ++reportOut.failedCaseCount;
    reportOut.caseReports.push_back(caseReport);
  }
  reportOut.passed = reportOut.totalCaseCount > 0 && reportOut.failedCaseCount == 0;
  finalizeSuiteArtifacts(suiteDirectory, reportOut, false);
  return reportOut.passed;
}
bool runRepresentativeGoldenAudioVerify(const TNodeRegistry &registry,
                                        TVerificationGoldenAudioSuiteReport &reportOut,
                                        float maxAbsoluteTolerance,
                                        double rmsTolerance) {
  reportOut = {};
  reportOut.suiteId = "representative-primary";
  reportOut.modeId = "verify";
  const auto baselineDirectory = baselineSuiteDirectory();
  const auto artifactDirectory = verifySuiteArtifactDirectory();
  reportOut.baselineDirectory = baselineDirectory.getFullPathName();
  reportOut.artifactDirectory = artifactDirectory.getFullPathName();
  juce::ignoreUnused(artifactDirectory.deleteRecursively());
  juce::ignoreUnused(artifactDirectory.createDirectory());
  const auto fixtures = makeRepresentativeVerificationGraphSet(registry);
  const auto cases = makeRepresentativeGoldenCases();
  for (const auto &caseSpec : cases) {
    TVerificationGoldenAudioReport caseReport;
    caseReport.graphId = caseSpec.fixtureId;
    caseReport.stimulusId = caseSpec.stimulus.stimulusId;
    caseReport.profileId = caseSpec.profile.profileId;
    caseReport.modeId = "verify";
    const auto *fixture = findFixtureById(fixtures, caseSpec.fixtureId);
    if (fixture == nullptr) {
      caseReport.failureReason = "Representative golden fixture was not found.";
    } else {
      const auto caseBaselineDirectory = caseDirectoryFor(baselineDirectory, *fixture,
                                                          caseSpec.profile,
                                                          caseSpec.stimulus);
      const auto caseArtifactDirectory = caseDirectoryFor(artifactDirectory, *fixture,
                                                          caseSpec.profile,
                                                          caseSpec.stimulus);
      juce::ignoreUnused(caseArtifactDirectory.createDirectory());
      caseReport.baselineDirectory = caseBaselineDirectory.getFullPathName();
      caseReport.artifactDirectory = caseArtifactDirectory.getFullPathName();
      const auto baselineWaveFile = caseBaselineDirectory.getChildFile("golden-output.wav");
      caseReport.baselineExists = baselineWaveFile.existsAsFile();
      if (!caseReport.baselineExists) {
        caseReport.failureReason = "Golden baseline WAV file is missing.";
      } else {
        TVerificationRenderResult renderResult;
        juce::String renderError;
        if (!renderGraphWithStimulus(registry, fixture->document, caseSpec.profile,
                                     caseSpec.stimulus, renderResult, &renderError)) {
          caseReport.failureReason = "Golden audio verify render failed: " + renderError;
        } else {
          juce::AudioBuffer<float> goldenBuffer;
          double goldenSampleRate = 0.0;
          juce::String readError;
          if (!readWaveFile(baselineWaveFile, goldenBuffer, goldenSampleRate,
                            &readError)) {
            caseReport.failureReason = readError;
          } else if (std::abs(goldenSampleRate - caseSpec.profile.sampleRate) > 0.01) {
            caseReport.failureReason = "Golden baseline sample rate does not match the verification profile.";
          } else {
            compareWithGolden(renderResult, goldenBuffer, caseReport,
                              maxAbsoluteTolerance, rmsTolerance);
          }
        }
      }
      finalizeVerifyCaseArtifacts(caseArtifactDirectory, caseReport);
    }
    ++reportOut.totalCaseCount;
    if (caseReport.passed)
      ++reportOut.passedCaseCount;
    else
      ++reportOut.failedCaseCount;
    reportOut.caseReports.push_back(caseReport);
  }
  reportOut.passed = reportOut.totalCaseCount > 0 && reportOut.failedCaseCount == 0;
  finalizeSuiteArtifacts(artifactDirectory, reportOut, true);
  return reportOut.passed;
}
} // namespace Teul
