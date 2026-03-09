#include "Teul/Preset/TPresetCatalog.h"

#include "Teul/Serialization/TPatchPresetIO.h"
#include "Teul/Serialization/TStatePresetIO.h"

#include <algorithm>

namespace Teul {
namespace {

juce::String joinWarnings(const juce::StringArray &warnings) {
  juce::StringArray normalized;
  for (const auto &warning : warnings) {
    const auto trimmed = warning.trim();
    if (trimmed.isNotEmpty() && !normalized.contains(trimmed))
      normalized.add(trimmed);
  }

  return normalized.joinIntoString(" | ");
}

juce::String joinDomains(const juce::StringArray &domains) {
  juce::StringArray normalized;
  for (const auto &domain : domains) {
    const auto trimmed = domain.trim();
    if (trimmed.isNotEmpty() && !normalized.contains(trimmed))
      normalized.add(trimmed);
  }

  return normalized.joinIntoString(", ");
}

juce::String formatTimestamp(const juce::Time &time) {
  if (time.toMilliseconds() <= 0)
    return "Unknown";
  return time.formatted("%Y-%m-%d %H:%M");
}

juce::String buildPatchSummary(const TPatchPresetSummary &summary,
                               const TPatchPresetLoadReport &report,
                               bool available) {
  if (!available)
    return "Preset could not be parsed";

  juce::StringArray parts;
  parts.add(juce::String(summary.nodeCount) + " nodes");
  parts.add(juce::String(summary.connectionCount) + " wires");
  if (summary.frameCount > 0)
    parts.add(juce::String(summary.frameCount) + " frames");
  if (report.degraded)
    parts.add("degraded");
  if (report.migrated)
    parts.add("migrated");
  return parts.joinIntoString("  |  ");
}

juce::String buildPatchDetail(const juce::File &file,
                              const TPatchPresetSummary &summary,
                              const TPatchPresetLoadReport &report,
                              const juce::Result &loadResult) {
  juce::StringArray lines;
  lines.add("Kind: Patch Preset");
  lines.add("File: " + file.getFullPathName());
  lines.add("Modified: " + formatTimestamp(file.getLastModificationTime()));

  if (loadResult.failed()) {
    lines.add("Status: Invalid");
    lines.add("Error: " + loadResult.getErrorMessage());
    return lines.joinIntoString("\r\n");
  }

  lines.add("Preset: " + summary.presetName);
  lines.add("Domains: teul");
  lines.add("Nodes: " + juce::String(summary.nodeCount));
  lines.add("Connections: " + juce::String(summary.connectionCount));
  lines.add("Frames: " + juce::String(summary.frameCount));
  lines.add("Source Frame: " +
            (summary.sourceFrameUuid.isNotEmpty() ? summary.sourceFrameUuid
                                                  : "none"));
  lines.add("Schema: " + juce::String(report.sourceSchemaVersion) + " -> " +
            juce::String(report.targetSchemaVersion));
  lines.add("Migrated: " + juce::String(report.migrated ? "yes" : "no"));
  lines.add("Legacy Aliases: " +
            juce::String(report.usedLegacyAliases ? "yes" : "no"));
  lines.add("Degraded: " + juce::String(report.degraded ? "yes" : "no"));
  if (report.warnings.isEmpty())
    lines.add("Warnings: none");
  else
    lines.add("Warnings: " + joinWarnings(report.warnings));
  return lines.joinIntoString("\r\n");
}

juce::String buildStateSummary(const TStatePresetSummary &summary,
                               const TStatePresetLoadReport &report,
                               bool available) {
  if (!available)
    return "Preset could not be parsed";

  juce::StringArray parts;
  parts.add(juce::String(summary.nodeStateCount) + " nodes");
  parts.add(juce::String(summary.paramValueCount) + " params");
  if (report.degraded)
    parts.add("degraded");
  if (report.migrated)
    parts.add("migrated");
  return parts.joinIntoString("  |  ");
}

juce::String buildStateDetail(const juce::File &file,
                              const TStatePresetSummary &summary,
                              const TStatePresetLoadReport &report,
                              const juce::Result &loadResult) {
  juce::StringArray lines;
  lines.add("Kind: State Preset");
  lines.add("File: " + file.getFullPathName());
  lines.add("Modified: " + formatTimestamp(file.getLastModificationTime()));

  if (loadResult.failed()) {
    lines.add("Status: Invalid");
    lines.add("Error: " + loadResult.getErrorMessage());
    return lines.joinIntoString("\r\n");
  }

  lines.add("Preset: " + summary.presetName);
  lines.add("Domains: teul");
  lines.add("Target Graph: " +
            (summary.targetGraphName.isNotEmpty() ? summary.targetGraphName
                                                  : "unnamed"));
  lines.add("Node States: " + juce::String(summary.nodeStateCount));
  lines.add("Param Values: " + juce::String(summary.paramValueCount));
  lines.add("Schema: " + juce::String(report.sourceSchemaVersion) + " -> " +
            juce::String(report.targetSchemaVersion));
  lines.add("Migrated: " + juce::String(report.migrated ? "yes" : "no"));
  lines.add("Legacy Aliases: " +
            juce::String(report.usedLegacyAliases ? "yes" : "no"));
  lines.add("Degraded: " + juce::String(report.degraded ? "yes" : "no"));
  if (report.warnings.isEmpty())
    lines.add("Warnings: none");
  else
    lines.add("Warnings: " + joinWarnings(report.warnings));
  return lines.joinIntoString("\r\n");
}

class TeulPatchPresetProvider final : public TPresetProvider {
public:
  juce::String providerId() const override { return "teul.patch"; }

  juce::StringArray domains() const override {
    juce::StringArray result;
    result.add("teul");
    return result;
  }

  void collectEntries(std::vector<TPresetEntry> &entriesOut) const override {
    const auto directory = TPatchPresetIO::defaultPresetDirectory();
    if (!directory.isDirectory())
      return;

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false,
                             "*" + TPatchPresetIO::fileExtension());
    for (const auto &file : files) {
      TPresetEntry entry;
      entry.entryId = providerId() + ":" + file.getFullPathName();
      entry.presetKind = "teul.patch";
      entry.kindLabel = "Patch Preset";
      entry.primaryActionLabel = "Insert";
      entry.domains = domains();
      entry.file = file;
      entry.modifiedTime = file.getLastModificationTime();

      TGraphDocument presetDocument;
      TPatchPresetSummary summary;
      TPatchPresetLoadReport report;
      const auto loadResult =
          TPatchPresetIO::loadFromFile(presetDocument, summary, file, &report);
      entry.available = loadResult.wasOk();
      entry.degraded = report.degraded;
      entry.displayName = summary.presetName.isNotEmpty()
                              ? summary.presetName
                              : file.getFileNameWithoutExtension();
      entry.summaryText = buildPatchSummary(summary, report, entry.available);
      entry.detailText = buildPatchDetail(file, summary, report, loadResult);
      entry.warningText = joinWarnings(report.warnings);
      if (!entry.available && entry.summaryText.isEmpty())
        entry.summaryText = loadResult.getErrorMessage();
      entriesOut.push_back(std::move(entry));
    }
  }
};

class TeulStatePresetProvider final : public TPresetProvider {
public:
  juce::String providerId() const override { return "teul.state"; }

  juce::StringArray domains() const override {
    juce::StringArray result;
    result.add("teul");
    return result;
  }

  void collectEntries(std::vector<TPresetEntry> &entriesOut) const override {
    const auto directory = TStatePresetIO::defaultPresetDirectory();
    if (!directory.isDirectory())
      return;

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false,
                             "*" + TStatePresetIO::fileExtension());
    for (const auto &file : files) {
      TPresetEntry entry;
      entry.entryId = providerId() + ":" + file.getFullPathName();
      entry.presetKind = "teul.state";
      entry.kindLabel = "State Preset";
      entry.primaryActionLabel = "Apply";
      entry.domains = domains();
      entry.file = file;
      entry.modifiedTime = file.getLastModificationTime();

      std::vector<TStatePresetNodeState> nodeStates;
      TStatePresetSummary summary;
      TStatePresetLoadReport report;
      const auto loadResult =
          TStatePresetIO::loadFromFile(nodeStates, summary, file, &report);
      juce::ignoreUnused(nodeStates);
      entry.available = loadResult.wasOk();
      entry.degraded = report.degraded;
      entry.displayName = summary.presetName.isNotEmpty()
                              ? summary.presetName
                              : file.getFileNameWithoutExtension();
      entry.summaryText = buildStateSummary(summary, report, entry.available);
      entry.detailText = buildStateDetail(file, summary, report, loadResult);
      entry.warningText = joinWarnings(report.warnings);
      if (!entry.available && entry.summaryText.isEmpty())
        entry.summaryText = loadResult.getErrorMessage();
      entriesOut.push_back(std::move(entry));
    }
  }
};

} // namespace

TPresetCatalog::TPresetCatalog(
    std::vector<std::unique_ptr<TPresetProvider>> providersIn)
    : providers(std::move(providersIn)) {}

void TPresetCatalog::reload() {
  entries.clear();
  for (const auto &provider : providers) {
    if (provider == nullptr)
      continue;
    provider->collectEntries(entries);
  }

  std::stable_sort(entries.begin(), entries.end(),
                   [](const TPresetEntry &a, const TPresetEntry &b) {
                     if (a.modifiedTime != b.modifiedTime)
                       return a.modifiedTime > b.modifiedTime;
                     if (a.kindLabel != b.kindLabel)
                       return a.kindLabel < b.kindLabel;
                     return a.displayName.compareIgnoreCase(b.displayName) < 0;
                   });

  for (auto &entry : entries) {
    if (entry.detailText.isEmpty()) {
      entry.detailText = "Kind: " + entry.kindLabel + "\r\n"
                         "Domains: " + joinDomains(entry.domains) + "\r\n"
                         "File: " + entry.file.getFullPathName();
    }
  }
}

std::unique_ptr<TPresetCatalog> makeDefaultPresetCatalog() {
  std::vector<std::unique_ptr<TPresetProvider>> providers;
  providers.push_back(std::make_unique<TeulPatchPresetProvider>());
  providers.push_back(std::make_unique<TeulStatePresetProvider>());
  return std::make_unique<TPresetCatalog>(std::move(providers));
}

} // namespace Teul
