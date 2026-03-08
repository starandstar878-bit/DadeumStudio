#pragma once
#include "Teul/Export/TExport.h"
#include "Teul/Verification/TVerificationFixtures.h"
#include "Teul/Verification/TVerificationStimulus.h"
namespace Teul {
struct TVerificationParityReport {
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
bool runEditableExportRoundTripParity(
    const TNodeRegistry &registry,
    const TVerificationGraphFixture &fixture,
    const TVerificationRenderProfile &profile,
    const TVerificationStimulusSpec &stimulus,
    TVerificationParityReport &reportOut,
    float maxAbsoluteTolerance = 1.0e-5f,
    double rmsTolerance = 1.0e-6);
struct TVerificationParitySuiteReport {
  juce::String suiteId;
  bool passed = false;
  int totalCaseCount = 0;
  int passedCaseCount = 0;
  int failedCaseCount = 0;
  juce::String artifactDirectory;
  std::vector<TVerificationParityReport> caseReports;
};
bool runInitialG1StaticParitySmoke(const TNodeRegistry &registry,
                                   TVerificationParityReport &reportOut);
bool runRepresentativePrimaryParityMatrix(
    const TNodeRegistry &registry,
    TVerificationParitySuiteReport &reportOut);
} // namespace Teul
