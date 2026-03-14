#include "Teul2/Runtime/AudioGraph/TRuntimeDiagnostics.h"

#include "Teul2/Runtime/IOControl/TRuntimeDeviceManager.h"

namespace Teul {

void TRuntimeDiagnostics::mergeCoordinatorState(
    TRuntimeStats &stats, const TRuntimeDeviceState &deviceState) noexcept {
  if (deviceState.sampleRate > 0.0)
    stats.sampleRate = deviceState.sampleRate;

  if (deviceState.preparedBlockSize > 0)
    stats.preparedBlockSize = deviceState.preparedBlockSize;

  if (deviceState.inputChannels >= 0)
    stats.lastInputChannels = deviceState.inputChannels;

  if (deviceState.outputChannels >= 0)
    stats.lastOutputChannels = deviceState.outputChannels;

  if (deviceState.buildPending) {
    stats.rebuildPending = true;
    if (deviceState.requestedBuildRevision > 0)
      stats.pendingGeneration = deviceState.requestedBuildRevision;
  }
}

bool TRuntimeDiagnostics::hasFault(const TRuntimeStats &stats) noexcept {
  return stats.clipDetected || stats.denormalDetected || stats.xrunDetected ||
         stats.mutedFallbackActive;
}

juce::String TRuntimeDiagnostics::summarizeStatus(const TRuntimeStats &stats) {
  juce::String text = juce::String::formatted(
      "%.1f kHz / %d blk / %d in / %d out / CPU %.1f%%",
      stats.sampleRate * 0.001, stats.preparedBlockSize,
      stats.lastInputChannels, stats.lastOutputChannels,
      stats.cpuLoadPercent);

  if (stats.rebuildPending) {
    text << " / Pending gen "
         << juce::String(static_cast<juce::int64>(stats.pendingGeneration));
  } else {
    text << " / Gen "
         << juce::String(static_cast<juce::int64>(stats.activeGeneration));
  }

  if (hasFault(stats)) {
    juce::StringArray faults;
    if (stats.xrunDetected)
      faults.add("xrun");
    if (stats.clipDetected)
      faults.add("clip");
    if (stats.denormalDetected)
      faults.add("denormal");
    if (stats.mutedFallbackActive)
      faults.add("muted");
    text << " / " << faults.joinIntoString(",");
  }

  return text;
}
std::uint64_t TRuntimeDiagnostics::ticksToMicros(juce::int64 tickDelta) noexcept {
  const auto ticksPerSecond = juce::Time::getHighResolutionTicksPerSecond();
  if (tickDelta <= 0 || ticksPerSecond <= 0)
    return 0;

  return static_cast<std::uint64_t>((tickDelta * 1000000) / ticksPerSecond);
}

double TRuntimeDiagnostics::microsToMilliseconds(std::uint64_t micros) noexcept {
  return static_cast<double>(micros) / 1000.0;
}

void TRuntimeDiagnostics::updateAtomicMax(std::atomic<std::uint64_t> &target,
                                          std::uint64_t candidate) noexcept {
  auto current = target.load(std::memory_order_relaxed);
  while (candidate > current && !target.compare_exchange_weak(
                                    current, candidate,
                                    std::memory_order_relaxed,
                                    std::memory_order_relaxed)) {
  }
}

void TRuntimeDiagnostics::updateAtomicMax(std::atomic<int> &target,
                                          int candidate) noexcept {
  auto current = target.load(std::memory_order_relaxed);
  while (candidate > current && !target.compare_exchange_weak(
                                    current, candidate,
                                    std::memory_order_relaxed,
                                    std::memory_order_relaxed)) {
  }
}

float TRuntimeDiagnostics::measureSignalLevel(const float *samples,
                                               int numSamples) noexcept {
  float maxPeak = 0.0f;
  if (samples == nullptr)
    return 0.0f;
  for (int i = 0; i < numSamples; ++i) {
    const float absVal = std::abs(samples[i]);
    if (absVal > maxPeak)
      maxPeak = absVal;
  }
  return maxPeak;
}

float TRuntimeDiagnostics::smoothMeterLevel(float previous,
                                             float current) noexcept {
  if (current > previous)
    return current; // Instant attack

  // Simple exponential decay (approx 0.98 at 48k/128blk)
  return previous * 0.98f + current * 0.02f;
}

} // namespace Teul