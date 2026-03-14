#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include "Teul2/Runtime/TTeulRuntime.h"

#include <JuceHeader.h>
#include <map>

namespace Teul {

// =============================================================================
//  TParamValueReporter — 파라미터 업데이트 브로드캐스트용 인터페이스
// =============================================================================

class TParamValueReporter {
public:
  virtual ~TParamValueReporter() = default;
  virtual void reportParamValueChange(NodeId nodeId,
                                      const juce::String &paramKey,
                                      float value) = 0;
};

// =============================================================================
//  TProcessContext — DSP 프로세싱을 위한 런타임 환경
// =============================================================================

struct TProcessContext {
  juce::AudioBuffer<float> *globalPortBuffer = nullptr;
  const juce::AudioBuffer<float> *inputAudioBuffer = nullptr;
  juce::AudioBuffer<float> *deviceAudioBuffer = nullptr;
  juce::MidiBuffer *midiMessages = nullptr;
  const juce::MidiBuffer *deviceMidiMessages = nullptr;
  juce::MidiBuffer *midiOutputMessages = nullptr;
  const std::map<PortId, int> *portToChannel = nullptr;
  const TNode *nodeData = nullptr;
  TParamValueReporter *paramValueReporter = nullptr;
};

// =============================================================================
//  TNodeInstance — DSP 노드 실행체의 베이스 클래스
// =============================================================================

class TNodeInstance {
public:
  virtual ~TNodeInstance() = default;

  virtual void prepareToPlay(double sampleRate,
                             int maximumExpectedSamplesPerBlock) {}
  virtual void releaseResources() {}
  virtual void processSamples(const TProcessContext &context) {}
  virtual void setParameterValue(const juce::String &paramKey, float newValue) {}
  virtual void reset() {}
};

#if JUCE_MODULE_AVAILABLE_juce_audio_processors

class TGraphProcessor : public juce::AudioProcessor {
public:
  explicit TGraphProcessor(TTeulRuntime &runtimeEngine)
      : AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(),
                                      true)
                           .withOutput("Output",
                                       juce::AudioChannelSet::stereo(), true)),
        runtime(runtimeEngine) {}

  ~TGraphProcessor() override = default;

  void prepareToPlay(double sampleRate,
                     int maximumExpectedSamplesPerBlock) override {
    runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
  }

  void releaseResources() override { runtime.releaseResources(); }

  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &midiMessages) override {
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
      buffer.clear(i, 0, buffer.getNumSamples());

    runtime.setCurrentChannelLayout(totalNumInputChannels,
                                    totalNumOutputChannels);
    runtime.processBlock(buffer, midiMessages);
  }

  bool hasEditor() const override { return false; }
  juce::AudioProcessorEditor *createEditor() override { return nullptr; }
  const juce::String getName() const override { return "Teul Runtime Processor"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return true; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
  const juce::String getProgramName(int index) override {
    juce::ignoreUnused(index);
    return {};
  }
  void changeProgramName(int index, const juce::String &newName) override {
    juce::ignoreUnused(index, newName);
  }
  void getStateInformation(juce::MemoryBlock &destData) override {
    juce::ignoreUnused(destData);
  }
  void setStateInformation(const void *data, int sizeInBytes) override {
    juce::ignoreUnused(data, sizeInBytes);
  }

  ITeulParamProvider &paramProvider() noexcept { return runtime; }
  const ITeulParamProvider &paramProvider() const noexcept { return runtime; }

private:
  TTeulRuntime &runtime;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphProcessor)
};

#else

class TGraphProcessor {
public:
  explicit TGraphProcessor(TTeulRuntime &runtimeEngine) : runtime(runtimeEngine) {}

  ~TGraphProcessor() = default;

  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
  }

  void releaseResources() { runtime.releaseResources(); }

  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &midiMessages) {
    runtime.setCurrentChannelLayout(0, buffer.getNumChannels());
    runtime.processBlock(buffer, midiMessages);
  }

  ITeulParamProvider &paramProvider() noexcept { return runtime; }
  const ITeulParamProvider &paramProvider() const noexcept { return runtime; }

private:
  TTeulRuntime &runtime;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphProcessor)
};

#endif

} // namespace Teul