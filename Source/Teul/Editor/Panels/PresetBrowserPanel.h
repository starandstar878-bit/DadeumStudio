#pragma once

#include "Teul/Preset/TPresetCatalog.h"

#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace Teul {

class PresetBrowserPanel : public juce::Component {
public:
  using PrimaryActionHandler = std::function<juce::Result(const TPresetEntry &)>;

  ~PresetBrowserPanel() override = default;

  virtual void setLayoutChangedCallback(std::function<void()> callback) = 0;
  virtual bool isBrowserOpen() const noexcept = 0;
  virtual void setBrowserOpen(bool shouldOpen) = 0;
  virtual void refreshEntries(bool force = false) = 0;
  virtual void setPrimaryActionHandler(PrimaryActionHandler handler) = 0;

  static std::unique_ptr<PresetBrowserPanel> create();
};

} // namespace Teul
