#pragma once

#include "../Bridge/ITeulParamProvider.h"
#include "../Model/TGraphDocument.h"
#include "../Registry/TNodeRegistry.h"
#include "TNodeInstance.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <vector>

namespace Teul {

class TGraphRuntime : public juce::AudioIODeviceCallback,
                      public ITeulParamProvider,
                      private juce::AsyncUpdater,
                      private TParamValueReporter {
public:
  explicit TGraphRuntime(const TNodeRegistry *registry);
  ~TGraphRuntime() override;

  bool buildGraph(const TGraphDocument &doc);

  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);
  void releaseResources();
  void processBlock(juce::AudioBuffer<float> &deviceBuffer,
                    juce::MidiBuffer &midiMessages);

  void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
  void audioDeviceStopped() override;
  void audioDeviceIOCallbackWithContext(
      const float *const *inputChannelData, int numInputChannels,
      float *const *outputChannelData, int numOutputChannels, int numSamples,
      const juce::AudioIODeviceCallbackContext &context) override;

  void queueParameterChange(NodeId nodeId, const juce::String &paramKey,
                            float value);
  float getPortLevel(PortId portId) const noexcept;

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
    const TNode *nodeData = nullptr;
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

  struct RenderState : public juce::ReferenceCountedObject {
    using Ptr = juce::ReferenceCountedObjectPtr<RenderState>;
    std::vector<NodeEntry> sortedNodes;
    juce::AudioBuffer<float> globalPortBuffer;
    std::vector<PortTelemetry> portTelemetry;
    std::map<PortId, std::size_t> portTelemetryIndex;
    std::unique_ptr<std::atomic<float>[]> portLevels;
    int totalAllocatedChannels = 0;
  };

  struct AtomicState {
    std::atomic<RenderState *> state{nullptr};

    RenderState::Ptr get() const {
      return state.load(std::memory_order_acquire);
    }

    void set(RenderState *newState) {
      if (newState)
        newState->incReferenceCount();
      auto *old = state.exchange(newState, std::memory_order_release);
      if (old)
        old->decReferenceCount();
    }
  } activeState;

  struct ParamChange {
    NodeId nodeId = kInvalidNodeId;
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
  void enqueueParamNotification(NodeId nodeId,
                                const juce::String &paramKey,
                                float value);
  static float paramValueToFloat(const juce::var &value);
  static juce::var coerceValueLike(const juce::var &prototype,
                                   const juce::var &candidate);
  static float measureSignalLevel(const float *samples, int numSamples) noexcept;
  static float smoothMeterLevel(float previousLevel,
                                float measuredLevel) noexcept;

  const TNodeRegistry *nodeRegistry = nullptr;
  double currentSampleRate = 48000.0;
  int currentBlockSize = 256;

  mutable juce::CriticalSection paramSurfaceLock;
  TGraphDocument surfaceDocument;
  std::vector<TTeulExposedParam> exposedParams;
  std::map<juce::String, std::size_t> exposedParamIndexById;
  juce::ListenerList<Listener> listeners;
  std::atomic<bool> surfaceChangedPending{false};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphRuntime)
};

} // namespace Teul
