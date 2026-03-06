#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace Teul {
struct TGraphDocument;
}

namespace Teul {

using ParamBindingSummaryResolver =
    std::function<juce::String(const juce::String &paramId,
                               const juce::String &preferredBindingKey)>;

class EditorHandle : public juce::Component {
public:
  explicit EditorHandle(
      juce::AudioDeviceManager *audioDeviceManager = nullptr,
      ParamBindingSummaryResolver bindingSummaryResolver = {});
  ~EditorHandle() override;

  TGraphDocument &document() noexcept;
  const TGraphDocument &document() const noexcept;

  void refreshFromDocument();

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorHandle)
};

std::unique_ptr<EditorHandle> createEditor(
    juce::AudioDeviceManager *audioDeviceManager = nullptr,
    ParamBindingSummaryResolver bindingSummaryResolver = {});

} // namespace Teul
