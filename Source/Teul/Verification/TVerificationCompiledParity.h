#pragma once
#include "Teul/Export/TExport.h"
#include "Teul/Verification/TVerificationFixtures.h"
#include "Teul/Verification/TVerificationStimulus.h"

namespace Teul {

struct TVerificationCompiledParityCaseReport {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  bool passed = false;
  bool exportSucceeded = false;
  int totalSamples = 0;
  int comparedChannels = 0;
  int firstMismatchChannel = -1;
  int firstMismatchSample = -1;
  float maxAbsoluteError = 0.0f;
  double rmsError = 0.0;
  juce::String runtimeClassName;
  juce::String exportDirectory;
  juce::String artifactDirectory;
  juce::String failureReason;
  TExportReport exportReport;
};

struct TVerificationCompiledParitySuiteReport {
  juce::String suiteId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  int compileExitCode = -1;
  int runExitCode = -1;
  juce::String artifactDirectory;
  juce::String executablePath;
  juce::String failureReason;
  std::vector<TVerificationCompiledParityCaseReport> caseReports;
};

bool runRepresentativeCompiledRuntimeParity(
    const TNodeRegistry &registry,
    TVerificationCompiledParitySuiteReport &reportOut,
    float maxAbsoluteTolerance = 1.0e-5f,
    double rmsTolerance = 1.0e-6);

} // namespace Teul