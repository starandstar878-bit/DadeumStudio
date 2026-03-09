#pragma once

#include "Teul/Model/TGraphDocument.h"

#include <JuceHeader.h>
#include <map>
#include <vector>

namespace Teul {

struct TStatePresetNodeState {
  NodeId nodeId = kInvalidNodeId;
  juce::String typeKey;
  juce::String label;
  bool bypassed = false;
  std::map<juce::String, juce::var> params;
};

struct TStatePresetSummary {
  juce::String presetName;
  juce::String targetGraphName;
  int nodeStateCount = 0;
  int paramValueCount = 0;
};

struct TStatePresetLoadReport {
  int sourceSchemaVersion = 0;
  int targetSchemaVersion = 0;
  bool migrated = false;
  bool usedLegacyAliases = false;
  bool degraded = false;
  juce::StringArray warnings;
  juce::StringArray appliedSteps;
};

struct TStatePresetApplyReport {
  TStatePresetSummary summary;
  TStatePresetLoadReport loadReport;
  int appliedNodeCount = 0;
  int skippedNodeCount = 0;
  int appliedParamValueCount = 0;
  bool degraded = false;
  juce::StringArray warnings;
  std::vector<NodeId> appliedNodeIds;
};

struct TStatePresetDiffPreview {
  TStatePresetSummary summary;
  TStatePresetLoadReport loadReport;
  int matchedNodeCount = 0;
  int changedNodeCount = 0;
  int changedBypassCount = 0;
  int changedParamValueCount = 0;
  int missingNodeCount = 0;
  bool degraded = false;
  juce::StringArray warnings;
  juce::StringArray changedNodeLabels;
};

class TStatePresetIO {
public:
  static juce::String fileExtension();
  static juce::File defaultPresetDirectory();

  static juce::Result saveDocumentToFile(const TGraphDocument &document,
                                         const juce::File &file,
                                         TStatePresetSummary *summaryOut = nullptr);
  static juce::Result loadFromFile(std::vector<TStatePresetNodeState> &nodeStatesOut,
                                   TStatePresetSummary &summaryOut,
                                   const juce::File &file,
                                   TStatePresetLoadReport *loadReportOut = nullptr);
  static juce::Result previewAgainstDocument(
      const TGraphDocument &document,
      const juce::File &file,
      TStatePresetDiffPreview *previewOut = nullptr);
  static juce::Result applyToDocument(TGraphDocument &document,
                                      const juce::File &file,
                                      TStatePresetApplyReport *reportOut = nullptr);
};

} // namespace Teul
