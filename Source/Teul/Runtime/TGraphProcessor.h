#pragma once
#include "TGraphRuntime.h"
#include <JuceHeader.h>


namespace Teul {

// =============================================================================
// TGraphProcessor
// TGraphRuntime 엔진을 감싸서 플러그인 호스팅 / 외부 연동이 가능하도록 하는
// juce::AudioProcessor 래퍼 (Phase 3 Step 3)
// =============================================================================
class TGraphProcessor : public juce::AudioProcessor {
public:
  TGraphProcessor(TGraphRuntime &runtimeEngine)
      : AudioProcessor(
            BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        runtime(runtimeEngine) {}

  ~TGraphProcessor() override = default;

  // ===========================================================================
  void prepareToPlay(double sampleRate,
                     int maximumExpectedSamplesPerBlock) override {
    runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
  }

  void releaseResources() override { runtime.releaseResources(); }

  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &midiMessages) override {
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
      buffer.clear(i, 0, buffer.getNumSamples());

    // TGraphRuntime 에게 처리를 위임
    runtime.processBlock(buffer, midiMessages);
  }

  // ===========================================================================
  // AudioProcessor 부가 구현
  bool hasEditor() const override { return false; }
  juce::AudioProcessorEditor *createEditor() override { return nullptr; }
  const juce::String getName() const override {
    return "Teul Runtime Processor";
  }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return true; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int index) override {}
  const juce::String getProgramName(int index) override {
    return juce::String();
  }
  void changeProgramName(int index, const juce::String &newName) override {}
  void getStateInformation(juce::MemoryBlock &destData) override {}
  void setStateInformation(const void *data, int sizeInBytes) override {}

private:
  TGraphRuntime &runtime;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphProcessor)
};

} // namespace Teul
