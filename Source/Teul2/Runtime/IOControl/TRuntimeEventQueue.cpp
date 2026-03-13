#include "Teul2/Runtime/IOControl/TRuntimeEventQueue.h"

namespace Teul {

void TRuntimeEventQueue::push(TRuntimeEvent event) {
  const juce::ScopedLock scopedLock(lock);
  if ((int)events.size() >= kMaxQueuedEvents)
    events.erase(events.begin());
  events.push_back(std::move(event));
}

std::vector<TRuntimeEvent> TRuntimeEventQueue::drain() {
  const juce::ScopedLock scopedLock(lock);
  std::vector<TRuntimeEvent> drained;
  drained.swap(events);
  return drained;
}

void TRuntimeEventQueue::clear() {
  const juce::ScopedLock scopedLock(lock);
  events.clear();
}

bool TRuntimeEventQueue::isEmpty() const {
  const juce::ScopedLock scopedLock(lock);
  return events.empty();
}

int TRuntimeEventQueue::size() const {
  const juce::ScopedLock scopedLock(lock);
  return static_cast<int>(events.size());
}

} // namespace Teul