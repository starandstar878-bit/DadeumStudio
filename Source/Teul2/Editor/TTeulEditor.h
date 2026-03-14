#pragma once
 
#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Runtime/TTeulRuntime.h"

#include <JuceHeader.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

using ParamBindingSummaryResolver =
    std::function<juce::String(const juce::String &paramId)>;
using ParamBindingRevisionProvider = std::function<std::uint64_t()>;

struct TEditorSessionStatus {
  std::uint64_t lastPersistedDocumentRevision = 0;
  bool hasAutosaveSnapshot = false;
  juce::Time lastAutosaveTime;
};
struct TControlDeviceProfilePresence {
  juce::String profileId;
  juce::String deviceId;
  juce::String displayName;
  bool autoDetected = true;
};

class TTeulEditor : public juce::Component {
public:
  explicit TTeulEditor(
      juce::AudioDeviceManager *audioDeviceManager = nullptr,
      ParamBindingSummaryResolver bindingSummaryResolver = {},
      ParamBindingRevisionProvider bindingRevisionProvider = {});
  ~TTeulEditor() override;

  TTeulDocument &document() noexcept;
  const TTeulDocument &document() const noexcept;

  void refreshFromDocument();
  void setSessionStatus(const TEditorSessionStatus &status);
  bool applyLearnedControlBinding(
      const TDeviceBindingSignature &binding, const juce::String &profileId,
      const juce::String &deviceId, const juce::String &profileDisplayName,
      TControlSourceKind kind, TControlSourceMode mode,
      const juce::String &sourceDisplayName = {}, bool autoDetected = true,
      bool confirmed = true);
  bool applyLearnedMidiMessage(const juce::MidiMessage &message,
                               const juce::String &midiDeviceName = {},
                               const juce::String &hardwareId = {},
                               const juce::String &profileId = {},
                               const juce::String &profileDisplayName = {},
                               bool autoDetected = true, bool confirmed = true);
  void enqueueLearnedControlBinding(
      const TDeviceBindingSignature &binding, const juce::String &profileId,
      const juce::String &deviceId, const juce::String &profileDisplayName,
      TControlSourceKind kind, TControlSourceMode mode,
      const juce::String &sourceDisplayName = {}, bool autoDetected = true,
      bool confirmed = true);
  void enqueueControlDeviceProfilesSync(
      const std::vector<TControlDeviceProfilePresence> &profiles,
      bool autoMarkMissing = true);
  void enqueueControlDeviceProfilePresent(const juce::String &profileId,
                                          const juce::String &deviceId = {},
                                          const juce::String &displayName = {},
                                          bool autoDetected = true);
  void enqueueControlDeviceProfileMissing(const juce::String &profileId);
  bool reportControlDeviceProfilePresent(const juce::String &profileId,
                                         const juce::String &deviceId = {},
                                         const juce::String &displayName = {},
                                         bool autoDetected = true);
  bool reportControlDeviceProfileMissing(const juce::String &profileId);
  bool syncControlDeviceProfiles(
      const std::vector<TControlDeviceProfilePresence> &profiles,
      bool autoMarkMissing = true);
  TExportResult runExportDryRun(const TExportOptions &options = {}) const;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TTeulEditor)
};

std::unique_ptr<TTeulEditor>
createTeulEditor(juce::AudioDeviceManager *audioDeviceManager = nullptr,
                 ParamBindingSummaryResolver bindingSummaryResolver = {},
                 ParamBindingRevisionProvider bindingRevisionProvider = {});

} // namespace Teul