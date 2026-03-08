#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace Teul {

class DiagnosticsDrawer : public juce::Component {
public:
  ~DiagnosticsDrawer() override = default;

  virtual void setLayoutChangedCallback(std::function<void()> callback) = 0;
  virtual bool isDrawerOpen() const noexcept = 0;
  virtual void setDrawerOpen(bool shouldOpen) = 0;
  virtual void refreshArtifacts(bool force = false) = 0;

  static std::unique_ptr<DiagnosticsDrawer> create();
};

} // namespace Teul
