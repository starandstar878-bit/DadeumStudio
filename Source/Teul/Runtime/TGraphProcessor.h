#pragma once
#include "TGraphRuntime.h"
#include <JuceHeader.h>

namespace Teul {

#if JUCE_MODULE_AVAILABLE_juce_audio_processors

class TGraphProcessor : public juce::AudioProcessor {
public:
  explicit TGraphProcessor(TGraphRuntime &runtimeEngine)
      : AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
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

private:
  TGraphRuntime &runtime;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphProcessor)
};

#else

class TGraphProcessor {
public:
  explicit TGraphProcessor(TGraphRuntime &runtimeEngine) : runtime(runtimeEngine) {}

  ~TGraphProcessor() = default;

  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
  }

  void releaseResources() { runtime.releaseResources(); }

  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &midiMessages) {
    runtime.processBlock(buffer, midiMessages);
  }

private:
  TGraphRuntime &runtime;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphProcessor)
};

#endif

} // namespace Teul