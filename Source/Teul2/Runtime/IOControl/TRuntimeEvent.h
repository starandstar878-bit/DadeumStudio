#pragma once

#include <JuceHeader.h>
#include <cstdint>

namespace Teul {

enum class TRuntimeEventKind {
  GraphBuildRequested,
  GraphBuildCommitted,
  GraphBuildFailed,
  PrepareToPlay,
  ReleaseResources,
  ChannelLayoutChanged,
  MidiOutputSinkChanged,
  AudioDeviceStarted,
  AudioDeviceStopped,
};

struct TRuntimeEvent {
  TRuntimeEventKind kind = TRuntimeEventKind::GraphBuildRequested;
  std::uint64_t documentRuntimeRevision = 0;
  double sampleRate = 0.0;
  int blockSize = 0;
  int inputChannels = 0;
  int outputChannels = 0;
  bool flag = false;
  juce::String text;
  juce::Time timestamp = juce::Time::getCurrentTime();

  static TRuntimeEvent makeGraphBuildRequested(std::uint64_t runtimeRevision);
  static TRuntimeEvent makeGraphBuildCommitted(std::uint64_t runtimeRevision);
  static TRuntimeEvent makeGraphBuildFailed(std::uint64_t runtimeRevision);
  static TRuntimeEvent makePrepareToPlay(double sampleRate, int blockSize);
  static TRuntimeEvent makeReleaseResources();
  static TRuntimeEvent makeChannelLayoutChanged(int inputChannels,
                                                int outputChannels);
  static TRuntimeEvent makeMidiOutputSinkChanged(bool configured);
  static TRuntimeEvent makeAudioDeviceStarted(const juce::String &deviceName,
                                              double sampleRate,
                                              int blockSize);
  static TRuntimeEvent makeAudioDeviceStopped();
};

inline TRuntimeEvent
TRuntimeEvent::makeGraphBuildRequested(std::uint64_t runtimeRevision) {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::GraphBuildRequested;
  event.documentRuntimeRevision = runtimeRevision;
  return event;
}

inline TRuntimeEvent
TRuntimeEvent::makeGraphBuildCommitted(std::uint64_t runtimeRevision) {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::GraphBuildCommitted;
  event.documentRuntimeRevision = runtimeRevision;
  return event;
}

inline TRuntimeEvent
TRuntimeEvent::makeGraphBuildFailed(std::uint64_t runtimeRevision) {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::GraphBuildFailed;
  event.documentRuntimeRevision = runtimeRevision;
  return event;
}

inline TRuntimeEvent TRuntimeEvent::makePrepareToPlay(double sampleRateIn,
                                                      int blockSizeIn) {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::PrepareToPlay;
  event.sampleRate = sampleRateIn;
  event.blockSize = blockSizeIn;
  return event;
}

inline TRuntimeEvent TRuntimeEvent::makeReleaseResources() {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::ReleaseResources;
  return event;
}

inline TRuntimeEvent
TRuntimeEvent::makeChannelLayoutChanged(int inputChannelsIn,
                                        int outputChannelsIn) {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::ChannelLayoutChanged;
  event.inputChannels = inputChannelsIn;
  event.outputChannels = outputChannelsIn;
  return event;
}

inline TRuntimeEvent
TRuntimeEvent::makeMidiOutputSinkChanged(bool configured) {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::MidiOutputSinkChanged;
  event.flag = configured;
  return event;
}

inline TRuntimeEvent
TRuntimeEvent::makeAudioDeviceStarted(const juce::String &deviceName,
                                      double sampleRateIn, int blockSizeIn) {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::AudioDeviceStarted;
  event.text = deviceName;
  event.sampleRate = sampleRateIn;
  event.blockSize = blockSizeIn;
  return event;
}

inline TRuntimeEvent TRuntimeEvent::makeAudioDeviceStopped() {
  TRuntimeEvent event;
  event.kind = TRuntimeEventKind::AudioDeviceStopped;
  return event;
}

} // namespace Teul