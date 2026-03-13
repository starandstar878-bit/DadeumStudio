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

} // namespace Teul