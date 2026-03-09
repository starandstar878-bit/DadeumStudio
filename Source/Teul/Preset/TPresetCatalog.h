#pragma once

#include <JuceHeader.h>
#include <map>
#include <memory>
#include <vector>

namespace Teul {

struct TPresetEntry {
  juce::String entryId;
  juce::String presetKind;
  juce::String kindLabel;
  juce::String primaryActionLabel;
  juce::String secondaryActionLabel;
  juce::String displayName;
  juce::String summaryText;
  juce::String detailText;
  juce::String warningText;
  juce::StringArray domains;
  juce::File file;
  juce::Time modifiedTime;
  juce::Time lastUsedTime;
  juce::StringArray tags;
  bool available = false;
  bool degraded = false;
  bool favorite = false;
  bool recent = false;
};

struct TPresetLibraryEntryState {
  bool favorite = false;
  int64_t lastUsedMs = 0;
  juce::StringArray tags;
};

class TPresetProvider {
public:
  virtual ~TPresetProvider() = default;

  virtual juce::String providerId() const = 0;
  virtual juce::StringArray domains() const = 0;
  virtual void collectEntries(std::vector<TPresetEntry> &entriesOut) const = 0;
};

class TPresetCatalog {
public:
  explicit TPresetCatalog(
      std::vector<std::unique_ptr<TPresetProvider>> providersIn);

  void reload();
  bool toggleFavorite(const juce::String &entryId);
  void markUsed(const juce::String &entryId);
  void setTags(const juce::String &entryId, const juce::StringArray &tags);
  const std::vector<TPresetEntry> &getEntries() const noexcept {
    return entries;
  }

private:
  void loadLibraryState();
  void saveLibraryState() const;

  std::vector<std::unique_ptr<TPresetProvider>> providers;
  std::vector<TPresetEntry> entries;
  juce::File stateFile;
  std::map<juce::String, TPresetLibraryEntryState> libraryState;
};

std::unique_ptr<TPresetCatalog> makeDefaultPresetCatalog();

} // namespace Teul
