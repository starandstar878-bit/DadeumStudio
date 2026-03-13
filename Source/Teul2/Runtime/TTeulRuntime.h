#pragma once

#include "Teul2/Runtime/AudioGraph/TRuntimeDiagnostics.h"
#include "Teul/Bridge/ITeulParamProvider.h"

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

class TTeulDocument;
class TNodeRegistry;

class TTeulRuntime : public juce::AudioIODeviceCallback,
                     public ITeulParamProvider {
public:
  using RuntimeStats = TRuntimeStats;

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