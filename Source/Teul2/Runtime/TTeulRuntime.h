#pragma once

#include "Teul/Bridge/ITeulParamProvider.h"

#include <JuceHeader.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

class TTeulDocument;
class TNodeRegistry;

class TTeulRuntime : public juce::AudioIODeviceCallback,
                     public ITeulParamProvider {
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

  explicit TTeulRuntime(const TNodeRegistry *registry);
  ~TTeulRuntime() override;

  bool buildGraph(const TTeulDocument &document);

  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);
  void releaseResources();
  void processBlock(juce::AudioBuffer<float> &deviceBuffer,
                    juce::MidiBuffer &midiMessages);
  void setCurrentChannelLayout(int inputChannels, int outputChannels) noexcept;
  void setMidiOutputSink(
      std::function<void(const juce::MidiBuffer &)> sinkCallback);

  void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
  void audioDeviceStopped() override;
  void audioDeviceIOCallbackWithContext(
      const float *const *inputChannelData, int numInputChannels,
      float *const *outputChannelData, int numOutputChannels, int numSamples,
      const juce::AudioIODeviceCallbackContext &context) override;

  void queueParameterChange(NodeId nodeId, const juce::String &paramKey,
                            float value);
  bool applyControlSourceValue(const juce::String &sourceId,
                               const juce::String &portId,
                               float normalizedValue);
  float getPortLevel(PortId portId) const noexcept;
  RuntimeStats getRuntimeStats() const noexcept;

  std::vector<TTeulExposedParam> listExposedParams() const override;
  juce::var getParam(const juce::String &paramId) const override;
  bool setParam(const juce::String &paramId, const juce::var &value) override;
  void addListener(Listener *listener) override;
  void removeListener(Listener *listener) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TTeulRuntime)
};

} // namespace Teul