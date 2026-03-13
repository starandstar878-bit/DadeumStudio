#pragma once

#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Runtime/TTeulRuntime.h"
#include "Teul/Export/TExport.h"

#include <JuceHeader.h>
#include <vector>

namespace Teul {

class TNodeRegistry;

enum class TRuntimeValidationStimulusKind {
  StaticRender,
  StepAutomation,
  SweepAutomation,
  MidiPhrase,
};

enum class TRuntimeValidationAutomationMode {
  Step,
  Linear,
};

struct TRuntimeValidationGraphFixture {
  juce::String fixtureId;
  juce::String displayName;
  TTeulDocument document;
};

struct TRuntimeValidationRenderProfile {
  juce::String profileId;
  double sampleRate = 48000.0;
  int blockSize = 128;
  int outputChannels = 2;
  double durationSeconds = 2.0;
};

struct TRuntimeValidationAutomationLane {
  juce::String nodeLabel;
  juce::String paramKey;
  TRuntimeValidationAutomationMode mode =
      TRuntimeValidationAutomationMode::Step;
  float startValue = 0.0f;
  float endValue = 0.0f;
  double stepIntervalSeconds = 0.25;
};

struct TRuntimeValidationMidiEvent {
  int sampleOffset = 0;
  juce::MidiMessage message;
};

struct TRuntimeValidationStimulusSpec {
  juce::String stimulusId;
  juce::String displayName;
  TRuntimeValidationStimulusKind kind =
      TRuntimeValidationStimulusKind::StaticRender;
  std::vector<TRuntimeValidationAutomationLane> automationLanes;
  std::vector<TRuntimeValidationMidiEvent> midiEvents;
};

struct TRuntimeValidationRenderResult {
  juce::String graphName;
  juce::String stimulusId;
  juce::String profileId;
  juce::AudioBuffer<float> audioBuffer;
  int totalSamples = 0;
  int renderedBlockCount = 0;
  TTeulRuntime::RuntimeStats runtimeStats;
};

struct TRuntimeValidationParityReport {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  juce::String modeId;
  bool passed = false;
  bool importedDocumentLoaded = false;
  int totalSamples = 0;
  int comparedChannels = 0;
  int firstMismatchChannel = -1;
  int firstMismatchSample = -1;
  float maxAbsoluteError = 0.0f;
  double rmsError = 0.0;
  int exportWarningCount = 0;
  int exportErrorCount = 0;
  juce::String artifactDirectory;
  juce::String failureReason;
  TExportReport exportReport;
};

struct TRuntimeValidationParitySuiteReport {
  juce::String suiteId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  juce::String artifactDirectory;
  std::vector<TRuntimeValidationParityReport> caseReports;
};

struct TRuntimeValidationGoldenAudioReport {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  juce::String modeId;
  bool passed = false;
  bool baselineExists = false;
  int totalSamples = 0;
  int comparedChannels = 0;
  int firstMismatchChannel = -1;
  int firstMismatchSample = -1;
  float maxAbsoluteError = 0.0f;
  double rmsError = 0.0;
  juce::String baselineDirectory;
  juce::String artifactDirectory;
  juce::String failureReason;
};

struct TRuntimeValidationGoldenAudioSuiteReport {
  juce::String suiteId;
  juce::String modeId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  juce::String baselineDirectory;
  juce::String artifactDirectory;
  std::vector<TRuntimeValidationGoldenAudioReport> caseReports;
};

struct TRuntimeValidationStressCaseReport {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  bool passed = false;
  int iterationCount = 0;
  int totalRenderedSamples = 0;
  int totalRenderedBlocks = 0;
  juce::String artifactDirectory;
  juce::String failureReason;
  TTeulRuntime::RuntimeStats worstRuntimeStats;
};

struct TRuntimeValidationStressSuiteReport {
  juce::String suiteId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  int iterationCount = 0;
  juce::String artifactDirectory;
  std::vector<TRuntimeValidationStressCaseReport> caseReports;
};

struct TRuntimeValidationBenchmarkThresholds {
  float maxCpuLoadPercent = 0.0f;
  double maxProcessMilliseconds = 0.0;
  double maxBuildMilliseconds = 0.0;
};

struct TRuntimeValidationBenchmarkCaseReport {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  bool passed = false;
  int iterationCount = 0;
  int totalRenderedSamples = 0;
  int totalRenderedBlocks = 0;
  juce::String artifactDirectory;
  juce::String failureReason;
  TRuntimeValidationBenchmarkThresholds thresholds;
  TTeulRuntime::RuntimeStats worstRuntimeStats;
};

struct TRuntimeValidationBenchmarkSuiteReport {
  juce::String suiteId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  int iterationCount = 0;
  juce::String artifactDirectory;
  std::vector<TRuntimeValidationBenchmarkCaseReport> caseReports;
};

class TRuntimeValidator {
public:
  static std::vector<TRuntimeValidationGraphFixture>
  makeRepresentativeGraphSet(const TNodeRegistry &registry);

  static TRuntimeValidationRenderProfile makePrimaryRenderProfile();
  static TRuntimeValidationRenderProfile makeSecondaryRenderProfile();
  static TRuntimeValidationRenderProfile makeExtendedRenderProfile();

  static TRuntimeValidationStimulusSpec makeStaticRenderStimulus();
  static TRuntimeValidationStimulusSpec makeStepAutomationStimulus(
      const juce::String &nodeLabel, const juce::String &paramKey,
      float startValue, float endValue,
      double stepIntervalSeconds = 0.25);
  static TRuntimeValidationStimulusSpec makeSweepAutomationStimulus(
      const juce::String &nodeLabel, const juce::String &paramKey,
      float startValue, float endValue);
  static TRuntimeValidationStimulusSpec makeMidiPhraseStimulus();

  static bool renderDocumentWithStimulus(
      const TNodeRegistry &registry, const TTeulDocument &document,
      const TRuntimeValidationRenderProfile &profile,
      const TRuntimeValidationStimulusSpec &stimulus,
      TRuntimeValidationRenderResult &resultOut,
      juce::String *errorMessageOut = nullptr);

  static bool runEditableExportRoundTripParity(
      const TNodeRegistry &registry,
      const TRuntimeValidationGraphFixture &fixture,
      const TRuntimeValidationRenderProfile &profile,
      const TRuntimeValidationStimulusSpec &stimulus,
      TRuntimeValidationParityReport &reportOut,
      float maxAbsoluteTolerance = 1.0e-5f,
      double rmsTolerance = 1.0e-6);

  static bool runInitialG1StaticParitySmoke(
      const TNodeRegistry &registry,
      TRuntimeValidationParityReport &reportOut);

  static bool runRepresentativePrimaryParityMatrix(
      const TNodeRegistry &registry,
      TRuntimeValidationParitySuiteReport &reportOut);

  static bool runRepresentativeGoldenAudioRecord(
      const TNodeRegistry &registry,
      TRuntimeValidationGoldenAudioSuiteReport &reportOut);

  static bool runRepresentativeGoldenAudioVerify(
      const TNodeRegistry &registry,
      TRuntimeValidationGoldenAudioSuiteReport &reportOut,
      float maxAbsoluteTolerance = 1.0e-5f,
      double rmsTolerance = 1.0e-6);

  static bool runRepresentativeStressSoakSuite(
      const TNodeRegistry &registry,
      TRuntimeValidationStressSuiteReport &reportOut,
      int iterationCount = 32);

  static bool runRepresentativeBenchmarkGate(
      const TNodeRegistry &registry,
      TRuntimeValidationBenchmarkSuiteReport &reportOut,
      int iterationCount = 8);
};

} // namespace Teul