#pragma once

#include "Export/TExport.h"

#include <JuceHeader.h>
#include <cstdint>
#include <functional>
#include <memory>

namespace Teul {
struct TGraphDocument;
}

namespace Teul {

using ParamBindingSummaryResolver =
    std::function<juce::String(const juce::String &paramId)>;
using ParamBindingRevisionProvider = std::function<std::uint64_t()>;

class EditorHandle : public juce::Component {
public:
  explicit EditorHandle(
      juce::AudioDeviceManager *audioDeviceManager = nullptr,
      ParamBindingSummaryResolver bindingSummaryResolver = {},
      ParamBindingRevisionProvider bindingRevisionProvider = {});
  ~EditorHandle() override;

  TGraphDocument &document() noexcept;
  const TGraphDocument &document() const noexcept;

  void refreshFromDocument();
  TExportResult runExportDryRun(const TExportOptions &options = {}) const;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorHandle)
};

std::unique_ptr<EditorHandle> createEditor(
    juce::AudioDeviceManager *audioDeviceManager = nullptr,
    ParamBindingSummaryResolver bindingSummaryResolver = {},
    ParamBindingRevisionProvider bindingRevisionProvider = {});

} // namespace Teul
