#include "Teul/Public/EditorHandle.h"
#include "Teul/Editor/EditorHandleImpl.h"

namespace Teul {

EditorHandle::EditorHandle(juce::AudioDeviceManager *audioDeviceManager,
                           ParamBindingSummaryResolver bindingSummaryResolver,
                           ParamBindingRevisionProvider bindingRevisionProvider) {
  impl = std::make_unique<Impl>(*this, audioDeviceManager,
                                std::move(bindingSummaryResolver),
                                std::move(bindingRevisionProvider));
  resized();
}

EditorHandle::~EditorHandle() = default;

TGraphDocument &EditorHandle::document() noexcept { return impl->document(); }

const TGraphDocument &EditorHandle::document() const noexcept {
  return impl->document();
}

void EditorHandle::refreshFromDocument() { impl->refreshFromDocument(); }

void EditorHandle::setSessionStatus(const TEditorSessionStatus &status) {
  if (impl != nullptr)
    impl->setSessionStatus(status);
}

bool EditorHandle::applyLearnedControlBinding(
    const TDeviceBindingSignature &binding, const juce::String &profileId,
    const juce::String &deviceId, const juce::String &profileDisplayName,
    TControlSourceKind kind, TControlSourceMode mode,
    const juce::String &sourceDisplayName, bool autoDetected, bool confirmed) {
  if (impl == nullptr)
    return false;

  return impl->applyLearnedControlBinding(binding, profileId, deviceId,
                                          profileDisplayName, kind, mode,
                                          sourceDisplayName, autoDetected,
                                          confirmed);
}

bool EditorHandle::applyLearnedMidiMessage(
    const juce::MidiMessage &message, const juce::String &midiDeviceName,
    const juce::String &hardwareId, const juce::String &profileId,
    const juce::String &profileDisplayName, bool autoDetected, bool confirmed) {
  if (impl == nullptr)
    return false;

  return impl->applyLearnedMidiMessage(message, midiDeviceName, hardwareId,
                                       profileId, profileDisplayName,
                                       autoDetected, confirmed);
}

void EditorHandle::enqueueLearnedControlBinding(
    const TDeviceBindingSignature &binding, const juce::String &profileId,
    const juce::String &deviceId, const juce::String &profileDisplayName,
    TControlSourceKind kind, TControlSourceMode mode,
    const juce::String &sourceDisplayName, bool autoDetected, bool confirmed) {
  if (impl == nullptr)
    return;

  impl->queueLearnedControlBinding(binding, profileId, deviceId,
                                   profileDisplayName, kind, mode,
                                   sourceDisplayName, autoDetected,
                                   confirmed);
}

void EditorHandle::enqueueControlDeviceProfilesSync(
    const std::vector<TControlDeviceProfilePresence> &profiles,
    bool autoMarkMissing) {
  if (impl == nullptr)
    return;

  impl->queueControlDeviceProfileSync(profiles, autoMarkMissing);
}

bool EditorHandle::reportControlDeviceProfilePresent(
    const juce::String &profileId, const juce::String &deviceId,
    const juce::String &displayName, bool autoDetected) {
  if (impl == nullptr)
    return false;

  return impl->reportControlDeviceProfilePresent(profileId, deviceId,
                                                 displayName, autoDetected);
}

bool EditorHandle::reportControlDeviceProfileMissing(
    const juce::String &profileId) {
  if (impl == nullptr)
    return false;

  return impl->reportControlDeviceProfileMissing(profileId);
}
bool EditorHandle::syncControlDeviceProfiles(
    const std::vector<TControlDeviceProfilePresence> &profiles,
    bool autoMarkMissing) {
  if (impl == nullptr)
    return false;
  return impl->syncControlDeviceProfiles(profiles, autoMarkMissing);
}
TExportResult EditorHandle::runExportDryRun(
    const TExportOptions &options) const {
  if (impl == nullptr || impl->registry() == nullptr)
    return {};

  return TExporter::run(impl->document(), *impl->registry(), options);
}

void EditorHandle::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff121212));
}

void EditorHandle::resized() {
  if (impl == nullptr)
    return;

  impl->layout(getLocalBounds());
}

std::unique_ptr<EditorHandle> createEditor(
    juce::AudioDeviceManager *audioDeviceManager,
    ParamBindingSummaryResolver bindingSummaryResolver,
    ParamBindingRevisionProvider bindingRevisionProvider) {
  return std::make_unique<EditorHandle>(audioDeviceManager,
                                        std::move(bindingSummaryResolver),
                                        std::move(bindingRevisionProvider));
}

} // namespace Teul
