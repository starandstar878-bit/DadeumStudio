#include "Teul2/Runtime/IOControl/TRuntimeDeviceManager.h"

namespace Teul {

void TRuntimeDeviceManager::consume(const TRuntimeEvent &event) {
  const juce::ScopedLock scopedLock(lock);

  switch (event.kind) {
  case TRuntimeEventKind::GraphBuildRequested:
    state.requestedBuildRevision = event.documentRuntimeRevision;
    state.buildPending = true;
    break;
  case TRuntimeEventKind::GraphBuildCommitted:
    state.requestedBuildRevision = event.documentRuntimeRevision;
    state.committedBuildRevision = event.documentRuntimeRevision;
    state.nodeCount = event.nodeCount;
    state.audioPortCount = event.audioPortCount;
    state.controlPortCount = event.controlPortCount;
    state.buildPending = false;
    break;
  case TRuntimeEventKind::GraphBuildFailed:
    state.requestedBuildRevision = event.documentRuntimeRevision;
    state.buildPending = false;
    break;
  case TRuntimeEventKind::PrepareToPlay:
    if (event.sampleRate > 0.0)
      state.sampleRate = event.sampleRate;
    if (event.blockSize > 0)
      state.preparedBlockSize = event.blockSize;
    break;
  case TRuntimeEventKind::ReleaseResources:
    state.audioDeviceRunning = false;
    break;
  case TRuntimeEventKind::ChannelLayoutChanged:
    state.inputChannels = juce::jmax(0, event.inputChannels);
    state.outputChannels = juce::jmax(0, event.outputChannels);
    break;
  case TRuntimeEventKind::MidiOutputSinkChanged:
    state.midiOutputSinkConfigured = event.flag;
    break;
  case TRuntimeEventKind::AudioDeviceStarted:
    state.audioDeviceRunning = true;
    state.activeDeviceName = event.text;
    if (event.sampleRate > 0.0)
      state.sampleRate = event.sampleRate;
    if (event.blockSize > 0)
      state.preparedBlockSize = event.blockSize;
    break;
  case TRuntimeEventKind::AudioDeviceStopped:
    state.audioDeviceRunning = false;
    state.activeDeviceName.clear();
    break;
  }
}

void TRuntimeDeviceManager::consume(const std::vector<TRuntimeEvent> &events) {
  for (const auto &event : events)
    consume(event);
}

TRuntimeDeviceState TRuntimeDeviceManager::snapshot() const {
  const juce::ScopedLock scopedLock(lock);
  return state;
}

void TRuntimeDeviceManager::reset() {
  const juce::ScopedLock scopedLock(lock);
  state = {};
}

} // namespace Teul