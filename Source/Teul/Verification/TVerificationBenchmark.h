#pragma once
#include "Teul/Verification/TVerificationFixtures.h"
#include "Teul/Verification/TVerificationStimulus.h"
namespace Teul {
struct TVerificationBenchmarkThresholds {
  float maxCpuLoadPercent = 0.0f;
  double maxProcessMilliseconds = 0.0;
  double maxBuildMilliseconds = 0.0;
};
struct TVerificationBenchmarkCaseReport {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  bool passed = false;
  int iterationCount = 0;
  int totalRenderedSamples = 0;
  int totalRenderedBlocks = 0;
  juce::String artifactDirectory;
  juce::String failureReason;
  TVerificationBenchmarkThresholds thresholds;
  TGraphRuntime::RuntimeStats worstRuntimeStats;
};
struct TVerificationBenchmarkSuiteReport {
  juce::String suiteId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  int iterationCount = 0;
  juce::String artifactDirectory;
  std::vector<TVerificationBenchmarkCaseReport> caseReports;
};
bool runRepresentativeBenchmarkGate(const TNodeRegistry &registry,
                                    TVerificationBenchmarkSuiteReport &reportOut,
                                    int iterationCount = 8);
} // namespace Teul
