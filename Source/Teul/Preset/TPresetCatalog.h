#pragma once

#include <JuceHeader.h>
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
  bool available = false;
  bool degraded = false;
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
  const std::vector<TPresetEntry> &getEntries() const noexcept {
    return entries;
  }

private:
  std::vector<std::unique_ptr<TPresetProvider>> providers;
  std::vector<TPresetEntry> entries;
};

std::unique_ptr<TPresetCatalog> makeDefaultPresetCatalog();

} // namespace Teul
