#pragma once

#include "Teul/Model/TGraphDocument.h"
#include "Teul/Serialization/TSerializer.h"

#include <JuceHeader.h>
#include <vector>

namespace Teul {

struct TPatchPresetSummary {
  juce::String presetName;
  juce::String sourceFrameUuid;
  int nodeCount = 0;
  int connectionCount = 0;
  int frameCount = 0;
  juce::Rectangle<float> bounds;
};

struct TPatchPresetLoadReport {
  int sourceSchemaVersion = 0;
  int targetSchemaVersion = 0;
  bool migrated = false;
  bool usedLegacyAliases = false;
  bool degraded = false;
  juce::StringArray warnings;
  juce::StringArray appliedSteps;
  TSchemaMigrationReport graphMigration;
};

class TPatchPresetIO {
public:
  static juce::String fileExtension();
  static juce::File defaultPresetDirectory();

  static juce::Result saveFrameToFile(const TGraphDocument &document,
                                      int frameId,
                                      const juce::File &file,
                                      TPatchPresetSummary *summaryOut = nullptr);
  static juce::Result loadFromFile(TGraphDocument &presetDocumentOut,
                                   TPatchPresetSummary &summaryOut,
                                   const juce::File &file,
                                   TPatchPresetLoadReport *loadReportOut = nullptr);
  static juce::Result insertFromFile(TGraphDocument &targetDocument,
                                     const juce::File &file,
                                     juce::Point<float> originWorld,
                                     std::vector<NodeId> *insertedNodeIdsOut = nullptr,
                                     int *insertedFrameIdOut = nullptr,
                                     TPatchPresetSummary *summaryOut = nullptr);
};

} // namespace Teul
