#pragma once
#include "Teul/Verification/TVerificationFixtures.h"
#include "Teul/Verification/TVerificationStimulus.h"
namespace Teul {
struct TVerificationStressCaseReport {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  bool passed = false;
  int iterationCount = 0;
  int totalRenderedSamples = 0;
  int totalRenderedBlocks = 0;
  juce::String artifactDirectory;
  juce::String failureReason;
  TGraphRuntime::RuntimeStats worstRuntimeStats;
};
struct TVerificationStressSuiteReport {
  juce::String suiteId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  int iterationCount = 0;
  juce::String artifactDirectory;
  std::vector<TVerificationStressCaseReport> caseReports;
};
bool runRepresentativeStressSoakSuite(const TNodeRegistry &registry,
                                      TVerificationStressSuiteReport &reportOut,
                                      int iterationCount = 32);
} // namespace Teul
