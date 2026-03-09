#include "Teul/Preset/TPresetCatalog.h"

#include "Teul/Serialization/TFileIo.h"
#include "Teul/Serialization/TPatchPresetIO.h"
#include "Teul/Serialization/TStatePresetIO.h"

#include <algorithm>
#include <map>

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

juce::File resolveLibraryStateFile() {
  auto directory = juce::File::getCurrentWorkingDirectory()
                       .getChildFile("Builds")
                       .getChildFile("TeulPresetLibrary");
  if (!directory.exists())
    directory.createDirectory();
  return directory.getChildFile("preset-browser-state.json");
}

juce::StringArray jsonArrayToStringArray(const juce::var &value) {
  juce::StringArray result;
  if (auto *array = value.getArray()) {
    for (const auto &item : *array) {
      const auto text = item.toString().trim();
      if (text.isNotEmpty() && !result.contains(text))
        result.add(text);
    }
  }
  return result;
}

juce::var stringArrayToJson(const juce::StringArray &values) {
  juce::Array<juce::var> items;
  for (const auto &value : values) {
    const auto trimmed = value.trim();
    if (trimmed.isNotEmpty())
      items.add(trimmed);
  }
  return juce::var(items);
}

std::map<juce::String, TPresetLibraryEntryState> loadLibraryStateMap(
    const juce::File &file) {
  std::map<juce::String, TPresetLibraryEntryState> result;
  if (!file.existsAsFile())
    return result;

  juce::var json;
  if (juce::JSON::parse(file.loadFileAsString(), json).failed())
    return result;

  const auto *root = json.getDynamicObject();
  if (root == nullptr)
    return result;

  const auto entriesValue = root->getProperty("entries");
  auto *entriesArray = entriesValue.getArray();
  if (entriesArray == nullptr)
    return result;

  for (const auto &item : *entriesArray) {
    const auto *entryObject = item.getDynamicObject();
    if (entryObject == nullptr)
      continue;

    const auto entryId = entryObject->getProperty("entryId").toString().trim();
    if (entryId.isEmpty())
      continue;

    TPresetLibraryEntryState state;
    state.favorite = static_cast<bool>(entryObject->getProperty("favorite"));
    state.lastUsedMs = static_cast<int64_t>(entryObject->getProperty("lastUsedMs"));
    state.tags = jsonArrayToStringArray(entryObject->getProperty("tags"));
    result[entryId] = state;
  }

  return result;
}

void saveLibraryStateMap(const juce::File &file,
                         const std::map<juce::String, TPresetLibraryEntryState> &stateMap) {
  auto root = std::make_unique<juce::DynamicObject>();
  root->setProperty("schemaVersion", 1);
  root->setProperty("format", "teul.preset-browser-state");

  juce::Array<juce::var> items;
  for (const auto &pair : stateMap) {
    const auto &entryId = pair.first;
    const auto &state = pair.second;
    auto entry = std::make_unique<juce::DynamicObject>();
    entry->setProperty("entryId", entryId);
    entry->setProperty("favorite", state.favorite);
    entry->setProperty("lastUsedMs", static_cast<int64_t>(state.lastUsedMs));
    entry->setProperty("tags", stringArrayToJson(state.tags));
    items.add(juce::var(entry.release()));
  }

  root->setProperty("entries", juce::var(items));
  file.replaceWithText(juce::JSON::toString(juce::var(root.release()), true));
}

void applyLibraryState(const std::map<juce::String, TPresetLibraryEntryState> &stateMap,
                       TPresetEntry &entry) {
  const auto found = stateMap.find(entry.entryId);
  if (found == stateMap.end())
    return;

  entry.favorite = found->second.favorite;
  entry.lastUsedTime = juce::Time(found->second.lastUsedMs);
  entry.recent = found->second.lastUsedMs > 0;
  entry.tags = found->second.tags;
}

juce::File resolveSessionDirectory() {
  auto dir =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getChildFile("DadeumStudio");
  if (!dir.exists())
    dir.createDirectory();

  if (!dir.exists()) {
    dir = juce::File::getCurrentWorkingDirectory()
              .getChildFile("Builds")
              .getChildFile("GyeolSession");
    if (!dir.exists())
      dir.createDirectory();
  }

  return dir;
}

bool readCleanShutdownMarker(const juce::File &file) {
  if (!file.existsAsFile())
    return true;

  juce::var json;
  if (juce::JSON::parse(file.loadFileAsString(), json).failed())
    return true;

  const auto *root = json.getDynamicObject();
  if (root == nullptr)
    return true;

  const auto value = root->getProperty("cleanShutdown");
  if (value.isBool())
    return static_cast<bool>(value);

  const auto textValue = value.toString().trim();
  if (textValue.isEmpty())
    return true;

  return textValue.equalsIgnoreCase("true") || textValue == "1";
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

void appendLibraryDetail(TPresetEntry &entry) {
  juce::StringArray lines;
  if (entry.favorite)
    lines.add("Library: favorite");
  if (entry.recent)
    lines.add("Last Used: " + formatTimestamp(entry.lastUsedTime));
  if (!entry.tags.isEmpty())
    lines.add("Tags: " + entry.tags.joinIntoString(", "));
  if (lines.isEmpty())
    return;

  entry.detailText << "\r\n" << lines.joinIntoString("\r\n");
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

juce::String buildRecoverySummary(const TGraphDocument &document,
                                  const TSchemaMigrationReport &report,
                                  bool available,
                                  bool cleanShutdown) {
  if (!available)
    return "Autosave snapshot could not be parsed";

  juce::StringArray parts;
  parts.add(juce::String(document.nodes.size()) + " nodes");
  parts.add(juce::String(document.connections.size()) + " wires");
  if (!document.frames.empty())
    parts.add(juce::String(document.frames.size()) + " frames");
  parts.add(cleanShutdown ? "clean marker" : "active autosave marker");
  if (report.degraded)
    parts.add("degraded");
  if (report.migrated)
    parts.add("migrated");
  return parts.joinIntoString("  |  ");
}

juce::String buildRecoveryDetail(const juce::File &autosaveFile,
                                 const juce::File &stateFile,
                                 const TGraphDocument &document,
                                 const TSchemaMigrationReport &report,
                                 const juce::Result &loadResult,
                                 bool cleanShutdown) {
  juce::StringArray lines;
  lines.add("Kind: Recovery Snapshot");
  lines.add("File: " + autosaveFile.getFullPathName());
  lines.add("Modified: " +
            formatTimestamp(autosaveFile.getLastModificationTime()));
  lines.add("Session State File: " + stateFile.getFullPathName());
  lines.add("Shutdown Marker: " +
            juce::String(cleanShutdown ? "clean" : "active or unclean"));

  if (loadResult.failed()) {
    lines.add("Status: Invalid");
    lines.add("Error: " + loadResult.getErrorMessage());
    return lines.joinIntoString("\r\n");
  }

  lines.add("Graph: " +
            (document.meta.name.isNotEmpty() ? document.meta.name : "Untitled"));
  lines.add("Domains: teul");
  lines.add("Nodes: " + juce::String(document.nodes.size()));
  lines.add("Connections: " + juce::String(document.connections.size()));
  lines.add("Frames: " + juce::String(document.frames.size()));
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

class TeulRecoverySnapshotProvider final : public TPresetProvider {
public:
  juce::String providerId() const override { return "teul.recovery"; }

  juce::StringArray domains() const override {
    juce::StringArray result;
    result.add("teul");
    return result;
  }

  void collectEntries(std::vector<TPresetEntry> &entriesOut) const override {
    const auto directory = resolveSessionDirectory();
    const auto autosaveFile = directory.getChildFile("autosave-teul.teul");
    const auto stateFile =
        directory.getChildFile("autosave-session-state.json");
    if (!autosaveFile.existsAsFile())
      return;

    TPresetEntry entry;
    entry.entryId = providerId() + ":" + autosaveFile.getFullPathName();
    entry.presetKind = "teul.recovery";
    entry.kindLabel = "Recovery Snapshot";
    entry.primaryActionLabel = "Restore";
    entry.secondaryActionLabel = "Discard";
    entry.domains = domains();
    entry.file = autosaveFile;
    entry.modifiedTime = autosaveFile.getLastModificationTime();

    TGraphDocument autosaveDocument;
    TSchemaMigrationReport report;
    const bool loaded = TFileIo::loadFromFile(autosaveDocument, autosaveFile,
                                              &report);
    const auto loadResult = loaded
                                ? juce::Result::ok()
                                : juce::Result::fail(
                                      "Autosave snapshot could not be parsed.");
    const bool cleanShutdown = readCleanShutdownMarker(stateFile);

    entry.available = loadResult.wasOk();
    entry.degraded = report.degraded;
    entry.displayName = autosaveDocument.meta.name.isNotEmpty()
                            ? autosaveDocument.meta.name + " Autosave"
                            : juce::String("Latest Teul Autosave");
    entry.summaryText =
        buildRecoverySummary(autosaveDocument, report, entry.available,
                             cleanShutdown);
    entry.detailText = buildRecoveryDetail(autosaveFile, stateFile,
                                           autosaveDocument, report, loadResult,
                                           cleanShutdown);
    entry.warningText = joinWarnings(report.warnings);
    if (!cleanShutdown) {
      if (entry.warningText.isNotEmpty())
        entry.warningText << " | ";
      entry.warningText << "Session shutdown marker is still active.";
    }
    if (!entry.available && entry.summaryText.isEmpty())
      entry.summaryText = loadResult.getErrorMessage();
    entriesOut.push_back(std::move(entry));
  }
};

} // namespace

TPresetCatalog::TPresetCatalog(
    std::vector<std::unique_ptr<TPresetProvider>> providersIn)
    : providers(std::move(providersIn)), stateFile(resolveLibraryStateFile()) {
  loadLibraryState();
}

void TPresetCatalog::reload() {
  loadLibraryState();
  entries.clear();
  for (const auto &provider : providers) {
    if (provider == nullptr)
      continue;
    provider->collectEntries(entries);
  }

  for (auto &entry : entries) {
    applyLibraryState(libraryState, entry);
    if (entry.detailText.isEmpty()) {
      entry.detailText = "Kind: " + entry.kindLabel + "\r\n"
                         "Domains: " + joinDomains(entry.domains) + "\r\n"
                         "File: " + entry.file.getFullPathName();
    }
    appendLibraryDetail(entry);
  }

  std::stable_sort(entries.begin(), entries.end(),
                   [](const TPresetEntry &a, const TPresetEntry &b) {
                     if (a.favorite != b.favorite)
                       return a.favorite && !b.favorite;
                     if (a.lastUsedTime != b.lastUsedTime)
                       return a.lastUsedTime > b.lastUsedTime;
                     if (a.modifiedTime != b.modifiedTime)
                       return a.modifiedTime > b.modifiedTime;
                     if (a.kindLabel != b.kindLabel)
                       return a.kindLabel < b.kindLabel;
                     return a.displayName.compareIgnoreCase(b.displayName) < 0;
                   });
}

bool TPresetCatalog::toggleFavorite(const juce::String &entryId) {
  if (entryId.isEmpty())
    return false;

  auto &state = libraryState[entryId];
  state.favorite = !state.favorite;
  saveLibraryState();
  return state.favorite;
}

void TPresetCatalog::markUsed(const juce::String &entryId) {
  if (entryId.isEmpty())
    return;

  auto &state = libraryState[entryId];
  state.lastUsedMs = juce::Time::getCurrentTime().toMilliseconds();
  saveLibraryState();
}

void TPresetCatalog::loadLibraryState() {
  libraryState = loadLibraryStateMap(stateFile);
}

void TPresetCatalog::saveLibraryState() const {
  saveLibraryStateMap(stateFile, libraryState);
}

std::unique_ptr<TPresetCatalog> makeDefaultPresetCatalog() {
  std::vector<std::unique_ptr<TPresetProvider>> providers;
  providers.push_back(std::make_unique<TeulPatchPresetProvider>());
  providers.push_back(std::make_unique<TeulStatePresetProvider>());
  providers.push_back(std::make_unique<TeulRecoverySnapshotProvider>());
  return std::make_unique<TPresetCatalog>(std::move(providers));
}

} // namespace Teul
