#pragma once

#include <JuceHeader.h>

namespace Teul {

class TTeulDocument;

struct TSchemaMigrationReport {
  int sourceSchemaVersion = 0;
  int targetSchemaVersion = 0;
  bool migrated = false;
  bool usedLegacyAliases = false;
  bool degraded = false;
  juce::StringArray warnings;
  juce::StringArray appliedSteps;
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

struct TStatePresetLoadReport {
  int sourceSchemaVersion = 0;
  int targetSchemaVersion = 0;
  bool migrated = false;
  bool usedLegacyAliases = false;
  bool degraded = false;
  juce::StringArray warnings;
  juce::StringArray appliedSteps;
};

class TDocumentMigration {
public:
  static void appendWarning(juce::StringArray &warnings,
                            const juce::String &warning);
  static void appendMigrationStep(TSchemaMigrationReport *report,
                                  const juce::String &stepName);
  static void appendMigrationStep(TPatchPresetLoadReport *report,
                                  const juce::String &stepName);
  static void appendMigrationStep(TStatePresetLoadReport *report,
                                  const juce::String &stepName);
  static juce::String joinWarnings(const juce::StringArray &warnings);
  static juce::Result parseJsonFile(const juce::File &file,
                                    juce::var &jsonOut,
                                    const juce::String &label);

  static bool usesLegacyDocumentAliases(const juce::var &json);
  static void normalizeLegacyRailBridgeNodes(TTeulDocument &document,
                                             TSchemaMigrationReport &migrationReport);
  static bool usesLegacyPatchAliases(const juce::var &json);
  static juce::var migratePatchPresetJson(const juce::var &json,
                                          int sourceSchemaVersion,
                                          int targetSchemaVersion,
                                          TPatchPresetLoadReport *report = nullptr);
  static bool usesLegacyStateAliases(const juce::var &json);
  static juce::var migrateStatePresetJson(const juce::var &json,
                                          int sourceSchemaVersion,
                                          int targetSchemaVersion,
                                          TStatePresetLoadReport *report = nullptr);
};

} // namespace Teul