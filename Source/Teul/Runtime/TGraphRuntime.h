#pragma once

#include "../Bridge/ITeulParamProvider.h"
#include "../Model/TGraphDocument.h"
#include "../Registry/TNodeRegistry.h"
#include "TNodeInstance.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace Teul {

class TGraphRuntime : public juce::AudioIODeviceCallback,
                      public ITeulParamProvider,
                      private juce::AsyncUpdater,
                      private TParamValueReporter {
public:
  struct RuntimeStats {
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

  explicit TGraphRuntime(const TNodeRegistry *registry);
  ~TGraphRuntime() override;

  bool buildGraph(const TGraphDocument &doc);

  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);
  void releaseResources();
  void processBlock(juce::AudioBuffer<float> &deviceBuffer,
                    juce::MidiBuffer &midiMessages);
  void setCurrentChannelLayout(int inputChannels, int outputChannels) noexcept;

  void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
  void audioDeviceStopped() override;
  void audioDeviceIOCallbackWithContext(
      const float *const *inputChannelData, int numInputChannels,
      float *const *outputChannelData, int numOutputChannels, int numSamples,
      const juce::AudioIODeviceCallbackContext &context) override;

  void queueParameterChange(NodeId nodeId, const juce::String &paramKey,
                            float value);
  float getPortLevel(PortId portId) const noexcept;
  RuntimeStats getRuntimeStats() const noexcept;

  std::vector<TTeulExposedParam> listExposedParams() const override;
  juce::var getParam(const juce::String &paramId) const override;
  bool setParam(const juce::String &paramId, const juce::var &value) override;
  void addListener(Listener *listener) override;
  void removeListener(Listener *listener) override;

private:
  void handleAsyncUpdate() override;
  void reportParamValueChange(NodeId nodeId,
                              const juce::String &paramKey,
                              float value) override;

  struct MixOp {
    int srcChannelIndex = -1;
    int dstChannelIndex = -1;
  };

  struct NodeEntry {
    NodeId nodeId = kInvalidNodeId;
    TNode nodeSnapshot;
    std::unique_ptr<TNodeInstance> instance;
    std::vector<MixOp> preProcessMixes;
    std::map<PortId, int> portChannels;
  };

  struct PortTelemetry {
    PortId portId = kInvalidPortId;
    NodeId nodeId = kInvalidNodeId;
    int channelIndex = -1;
    TPortDataType dataType = TPortDataType::Audio;
  };

  struct ParamDispatch {
    NodeId nodeId = kInvalidNodeId;
    TNodeInstance *instance = nullptr;
    juce::String paramKey;
    std::array<char, 32> paramKeyUtf8{};
    float currentValue = 0.0f;
    float targetValue = 0.0f;
    bool smoothingEnabled = false;
  };

  struct RenderState : public juce::ReferenceCountedObject {
    using Ptr = juce::ReferenceCountedObjectPtr<RenderState>;

    std::vector<NodeEntry> sortedNodes;
    juce::AudioBuffer<float> globalPortBuffer;
    std::vector<PortTelemetry> portTelemetry;
    std::map<PortId, std::size_t> portTelemetryIndex;
    std::unique_ptr<std::atomic<float>[]> portLevels;
    std::vector<ParamDispatch> paramDispatches;
    std::uint64_t generation = 0;
    int totalAllocatedChannels = 0;
  };

  struct AtomicState {
    ~AtomicState() { set(nullptr); }

    std::atomic<RenderState *> state{nullptr};

    RenderState::Ptr get() const { return state.load(std::memory_order_acquire); }

    bool hasState() const noexcept {
      return state.load(std::memory_order_acquire) != nullptr;
    }

    void set(RenderState *newState) {
      if (newState)
        newState->incReferenceCount();
      auto *old = state.exchange(newState, std::memory_order_release);
      if (old)
        old->decReferenceCount();
    }

    RenderState::Ptr take() {
      auto *old = state.exchange(nullptr, std::memory_order_acq_rel);
      RenderState::Ptr retained = old;
      if (old)
        old->decReferenceCount();
      return retained;
    }
  };

  struct ParamChange {
    NodeId nodeId = kInvalidNodeId;
    std::uint64_t generation = 0;
    int dispatchSlot = -1;
    float value = 0.0f;
    char paramKey[32] = {0};
  };

  static constexpr int kMaxParamQueueSize = 1024;
  juce::AbstractFifo paramQueueFifo{kMaxParamQueueSize};
  std::array<ParamChange, kMaxParamQueueSize> paramQueueData;

  struct PendingParamNotification {
    NodeId nodeId = kInvalidNodeId;
    float value = 0.0f;
    char paramKey[32] = {0};
  };

  static constexpr int kMaxParamNotificationQueueSize = 1024;
  juce::AbstractFifo paramNotificationFifo{kMaxParamNotificationQueueSize};
  std::array<PendingParamNotification, kMaxParamNotificationQueueSize>
      paramNotificationData;

  void rebuildParamSurfaceLocked(const TGraphDocument &doc);
  bool updateParamSurfaceValueLocked(NodeId nodeId,
                                     const juce::String &paramKey,
                                     const juce::var &value,
                                     TTeulExposedParam *updatedParam = nullptr);
  void rebuildQueuedParamDispatchLocked(const RenderState &state);
  void enqueueParamNotification(NodeId nodeId,
                                const juce::String &paramKey,
                                float value);
  void prepareStateForPlayback(RenderState &state,
                               double sampleRate,
                               int maximumExpectedSamplesPerBlock);
  bool commitPendingStateIfNeeded() noexcept;
  static float paramValueToFloat(const juce::var &value);
  static juce::var coerceValueLike(const juce::var &prototype,
                                   const juce::var &candidate);
  static float measureSignalLevel(const float *samples, int numSamples) noexcept;
  static float smoothMeterLevel(float previousLevel,
                                float measuredLevel) noexcept;
  static bool shouldSmoothParam(const TParamSpec *paramSpec,
                                const juce::var &initialValue) noexcept;
  static void writeParamKey(char *dest,
                            std::size_t destSize,
                            const juce::String &paramKey);
  static void writeParamKey(std::array<char, 32> &dest,
                            const juce::String &paramKey);
  static bool dispatchMatches(const ParamDispatch &dispatch,
                              NodeId nodeId,
                              const char *paramKey) noexcept;
  static std::uint64_t ticksToMicros(juce::int64 tickDelta) noexcept;
  static double microsToMilliseconds(std::uint64_t micros) noexcept;
  static void updateAtomicMax(std::atomic<std::uint64_t> &target,
                              std::uint64_t candidate) noexcept;
  static void updateAtomicMax(std::atomic<int> &target, int candidate) noexcept;

  const TNodeRegistry *nodeRegistry = nullptr;
  std::atomic<double> currentSampleRate{48000.0};
  std::atomic<int> currentBlockSize{256};
  std::atomic<int> lastInputChannels{0};
  std::atomic<int> lastOutputChannels{0};
  int outputFadeSamplesRemaining = 0;
  float outputFadeCurrentGain = 1.0f;

  mutable juce::CriticalSection paramSurfaceLock;
  TGraphDocument surfaceDocument;
  std::vector<TTeulExposedParam> exposedParams;
  std::map<juce::String, std::size_t> exposedParamIndexById;
  std::map<juce::String, int> queuedParamDispatchSlotById;
  std::uint64_t queuedParamDispatchGeneration = 0;
  juce::ListenerList<Listener> listeners;
  std::atomic<bool> surfaceChangedPending{false};

  AtomicState activeState;
  AtomicState pendingState;

  std::atomic<std::uint64_t> buildGenerationCounter{0};
  std::atomic<std::uint64_t> activeGeneration{0};
  std::atomic<std::uint64_t> pendingGeneration{0};
  std::atomic<bool> rebuildPending{false};

  std::atomic<std::uint64_t> processBlockCount{0};
  std::atomic<std::uint64_t> rebuildRequestCount{0};
  std::atomic<std::uint64_t> rebuildCommitCount{0};
  std::atomic<std::uint64_t> paramChangeCount{0};
  std::atomic<std::uint64_t> droppedParamChangeCount{0};
  std::atomic<std::uint64_t> droppedParamNotificationCount{0};
  std::atomic<std::uint64_t> lastBuildMicros{0};
  std::atomic<std::uint64_t> maxBuildMicros{0};
  std::atomic<std::uint64_t> lastProcessMicros{0};
  std::atomic<std::uint64_t> maxProcessMicros{0};
  std::atomic<int> activeNodeCount{0};
  std::atomic<int> allocatedPortChannels{0};
  std::atomic<int> largestBlockSeen{0};
  std::atomic<int> largestOutputChannelCountSeen{0};
  std::atomic<int> smoothingActiveCount{0};
  std::atomic<bool> clipDetected{false};
  std::atomic<bool> denormalDetected{false};
  std::atomic<bool> xrunDetected{false};
  std::atomic<bool> mutedFallbackActive{false};

  juce::MidiBuffer deviceCallbackMidiScratch;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphRuntime)
};

} // namespace Teul
