#pragma once

#include "Teul2/Runtime/IOControl/TRuntimeEvent.h"

#include <JuceHeader.h>
#include <cstdint>
#include <vector>

namespace Teul {

struct TRuntimeDeviceState {
  double sampleRate = 0.0;
  int preparedBlockSize = 0;
  int inputChannels = -1;
  int outputChannels = -1;
  bool audioDeviceRunning = false;
  bool midiOutputSinkConfigured = false;
  bool buildPending = false;
  std::uint64_t requestedBuildRevision = 0;
  std::uint64_t committedBuildRevision = 0;
  int nodeCount = 0;
  int audioPortCount = 0;
  int controlPortCount = 0;
  juce::String activeDeviceName;
};

class TRuntimeDeviceManager {
public:
  void consume(const TRuntimeEvent &event);
  void consume(const std::vector<TRuntimeEvent> &events);
  TRuntimeDeviceState snapshot() const;
  void reset();

private:
  mutable juce::CriticalSection lock;
  TRuntimeDeviceState state;
};

} // namespace Teul