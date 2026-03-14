#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <atomic>

namespace Teul {

struct TRuntimeDeviceState;

struct TRuntimeStats {
  double sampleRate = 48000.0;
  int preparedBlockSize = 256;
  int lastInputChannels = 0;
  int lastOutputChannels = 0;
  int activeNodeCount = 0;
  int allocatedPortChannels = 0;
  int largestBlockSeen = 0;
  int largestOutputChannelCountSeen = 0;
  int smoothingActiveCount = 0;
  std::uint64_t processBlockCount = 0;
  std::uint64_t rebuildRequestCount = 0;
  std::uint64_t rebuildCommitCount = 0;
  std::uint64_t paramChangeCount = 0;
  std::uint64_t droppedParamChangeCount = 0;
  std::uint64_t droppedParamNotificationCount = 0;
  std::uint64_t activeGeneration = 0;
  std::uint64_t pendingGeneration = 0;
  bool rebuildPending = false;
  bool clipDetected = false;
  bool denormalDetected = false;
  bool xrunDetected = false;
  bool mutedFallbackActive = false;
  float cpuLoadPercent = 0.0f;
  double lastBuildMilliseconds = 0.0;
  double maxBuildMilliseconds = 0.0;
  double lastProcessMilliseconds = 0.0;
  double maxProcessMilliseconds = 0.0;
};

class TRuntimeDiagnostics {
public:
  static void mergeCoordinatorState(
      TRuntimeStats &stats, const TRuntimeDeviceState &deviceState) noexcept;
  static bool hasFault(const TRuntimeStats &stats) noexcept;
  static juce::String summarizeStatus(const TRuntimeStats &stats);

  // Timing & Atomic Utilities (TGraphRuntime 에서 이관)
  static std::uint64_t ticksToMicros(juce::int64 tickDelta) noexcept;
  static double microsToMilliseconds(std::uint64_t micros) noexcept;
  static void updateAtomicMax(std::atomic<std::uint64_t> &target,
                              std::uint64_t candidate) noexcept;
  static void updateAtomicMax(std::atomic<int> &target, int candidate) noexcept;
};

} // namespace Teul