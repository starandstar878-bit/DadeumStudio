#pragma once

#include "TDocumentMigration.h"
#include "TTeulDocument.h"

#include <map>
#include <vector>

namespace Teul {

class TDocumentStore {
public:
  static bool saveToFile(const TTeulDocument &document, const juce::File &file);
  static bool loadFromFile(TTeulDocument &document,
                           const juce::File &file,
                           TSchemaMigrationReport *migrationReportOut = nullptr);
  static juce::Result importEditableGraphPackage(const juce::File &path,
                                                 TTeulDocument &documentOut);
};

class TFileIo {
public:
  static bool saveToFile(const TTeulDocument &document, const juce::File &file);
  static bool loadFromFile(TTeulDocument &document,
                           const juce::File &file,
                           TSchemaMigrationReport *migrationReportOut = nullptr);
};

struct TPatchPresetSummary {
  juce::String presetName;
  juce::String sourceFrameUuid;
  int nodeCount = 0;
  int connectionCount = 0;
  int frameCount = 0;
  juce::Rectangle<float> bounds;
};

class TPatchPresetIO {
public:
  static juce::String fileExtension();
  static juce::File defaultPresetDirectory();

  static juce::Result saveFrameToFile(const TTeulDocument &document,
                                      int frameId,
                                      const juce::File &file,
                                      TPatchPresetSummary *summaryOut = nullptr);
  static juce::Result loadFromFile(TTeulDocument &presetDocumentOut,
                                   TPatchPresetSummary &summaryOut,
                                   const juce::File &file,
                                   TPatchPresetLoadReport *loadReportOut = nullptr);
  static juce::Result insertFromFile(TTeulDocument &targetDocument,
                                     const juce::File &file,
                                     juce::Point<float> originWorld,
                                     std::vector<NodeId> *insertedNodeIdsOut = nullptr,
                                     int *insertedFrameIdOut = nullptr,
                                     TPatchPresetSummary *summaryOut = nullptr);
};

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

  static juce::Result saveDocumentToFile(const TTeulDocument &document,
                                         const juce::File &file,
                                         TStatePresetSummary *summaryOut = nullptr);
  static juce::Result loadFromFile(std::vector<TStatePresetNodeState> &nodeStatesOut,
                                   TStatePresetSummary &summaryOut,
                                   const juce::File &file,
                                   TStatePresetLoadReport *loadReportOut = nullptr);
  static juce::Result previewAgainstDocument(
      const TTeulDocument &document,
      const juce::File &file,
      TStatePresetDiffPreview *previewOut = nullptr);
  static juce::Result applyToDocument(TTeulDocument &document,
                                      const juce::File &file,
                                      TStatePresetApplyReport *reportOut = nullptr);
};

} // namespace Teul
