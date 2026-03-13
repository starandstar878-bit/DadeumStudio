#include "TDocumentMigration.h"

namespace Teul {

void TDocumentMigration::appendWarning(juce::StringArray &warnings,
                                       const juce::String &warning) {
  const auto normalized = warning.trim();
  if (normalized.isEmpty())
    return;
  if (!warnings.contains(normalized))
    warnings.add(normalized);
}

void TDocumentMigration::appendMigrationStep(TSchemaMigrationReport *report,
                                             const juce::String &stepName) {
  if (report == nullptr)
    return;

  const auto normalized = stepName.trim();
  if (normalized.isEmpty())
    return;
  if (!report->appliedSteps.contains(normalized))
    report->appliedSteps.add(normalized);
}

void TDocumentMigration::appendMigrationStep(TPatchPresetLoadReport *report,
                                             const juce::String &stepName) {
  if (report == nullptr)
    return;

  const auto normalized = stepName.trim();
  if (normalized.isEmpty())
    return;
  if (!report->appliedSteps.contains(normalized))
    report->appliedSteps.add(normalized);
}

void TDocumentMigration::appendMigrationStep(TStatePresetLoadReport *report,
                                             const juce::String &stepName) {
  if (report == nullptr)
    return;

  const auto normalized = stepName.trim();
  if (normalized.isEmpty())
    return;
  if (!report->appliedSteps.contains(normalized))
    report->appliedSteps.add(normalized);
}

juce::String TDocumentMigration::joinWarnings(
    const juce::StringArray &warnings) {
  juce::StringArray normalizedWarnings;
  for (const auto &warning : warnings) {
    const auto normalized = warning.trim();
    if (normalized.isNotEmpty() && !normalizedWarnings.contains(normalized))
      normalizedWarnings.add(normalized);
  }

  return normalizedWarnings.joinIntoString(" | ");
}

juce::Result TDocumentMigration::parseJsonFile(const juce::File &file,
                                               juce::var &jsonOut,
                                               const juce::String &label) {
  if (!file.existsAsFile()) {
    return juce::Result::fail(label + " file does not exist: " +
                              file.getFullPathName());
  }

  const auto parseResult = juce::JSON::parse(file.loadFileAsString(), jsonOut);
  if (parseResult.failed()) {
    return juce::Result::fail("Failed to parse " + label + ": " +
                              parseResult.getErrorMessage());
  }

  return juce::Result::ok();
}

} // namespace Teul
