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
  struct Listener {
    virtual ~Listener() = default;
    virtual void deviceStateChanged(const TRuntimeDeviceState &newState) = 0;
  };

  void addListener(Listener *l) { listeners.add(l); }
  void removeListener(Listener *l) { listeners.remove(l); }

  void consume(const TRuntimeEvent &event);
  void consume(const std::vector<TRuntimeEvent> &events);
  TRuntimeDeviceState snapshot() const;
  void reset();

private:
  void notifyListeners();

  mutable juce::CriticalSection lock;
  TRuntimeDeviceState state;
  juce::ListenerList<Listener> listeners;
};

} // namespace Teul
