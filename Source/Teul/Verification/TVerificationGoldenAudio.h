#pragma once
#include "Teul/Verification/TVerificationFixtures.h"
#include "Teul/Verification/TVerificationStimulus.h"
namespace Teul {
struct TVerificationGoldenAudioReport {
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
struct TVerificationGoldenAudioSuiteReport {
  juce::String suiteId;
  juce::String modeId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  juce::String baselineDirectory;
  juce::String artifactDirectory;
  std::vector<TVerificationGoldenAudioReport> caseReports;
};
bool runRepresentativeGoldenAudioRecord(const TNodeRegistry &registry,
                                        TVerificationGoldenAudioSuiteReport &reportOut);
bool runRepresentativeGoldenAudioVerify(const TNodeRegistry &registry,
                                        TVerificationGoldenAudioSuiteReport &reportOut,
                                        float maxAbsoluteTolerance = 1.0e-5f,
                                        double rmsTolerance = 1.0e-6);
} // namespace Teul
