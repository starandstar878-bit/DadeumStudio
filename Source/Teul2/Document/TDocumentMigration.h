#pragma once

#include <JuceHeader.h>

namespace Teul {

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
};

} // namespace Teul
