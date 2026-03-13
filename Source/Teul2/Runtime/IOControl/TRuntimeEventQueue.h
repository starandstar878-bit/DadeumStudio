#pragma once

#include "Teul2/Runtime/IOControl/TRuntimeEvent.h"

#include <JuceHeader.h>
#include <vector>

namespace Teul {

class TRuntimeEventQueue {
public:
  void push(TRuntimeEvent event);
  std::vector<TRuntimeEvent> drain();
  void clear();
  bool isEmpty() const;
  int size() const;

private:
  static constexpr int kMaxQueuedEvents = 256;

  mutable juce::CriticalSection lock;
  std::vector<TRuntimeEvent> events;
};

} // namespace Teul