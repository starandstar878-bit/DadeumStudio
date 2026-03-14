#include "Teul2/Runtime/AudioGraph/TGraphProcessor.h"
#include "Teul2/Runtime/TTeulRuntime.h"

namespace Teul {

#if JUCE_MODULE_AVAILABLE_juce_audio_processors

TGraphProcessor::TGraphProcessor(TTeulRuntime &runtimeEngine)
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      runtime(runtimeEngine) {}

TGraphProcessor::~TGraphProcessor() = default;

void TGraphProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
  runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
}

void TGraphProcessor::releaseResources() {
  runtime.releaseResources();
}

void TGraphProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  const auto totalNumInputChannels = getTotalNumInputChannels();
  const auto totalNumOutputChannels = getTotalNumOutputChannels();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  runtime.setCurrentChannelLayout(totalNumInputChannels, totalNumOutputChannels);
  runtime.processBlock(buffer, midiMessages);
}

void TGraphProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String TGraphProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void TGraphProcessor::changeProgramName(int index, const juce::String &newName) { juce::ignoreUnused(index, newName); }
void TGraphProcessor::getStateInformation(juce::MemoryBlock &destData) { juce::ignoreUnused(destData); }
void TGraphProcessor::setStateInformation(const void *data, int sizeInBytes) { juce::ignoreUnused(data, sizeInBytes); }

ITeulParamProvider &TGraphProcessor::paramProvider() noexcept { return runtime; }
const ITeulParamProvider &TGraphProcessor::paramProvider() const noexcept { return runtime; }

#else

TGraphProcessor::TGraphProcessor(TTeulRuntime &runtimeEngine) : runtime(runtimeEngine) {}
TGraphProcessor::~TGraphProcessor() = default;

void TGraphProcessor::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
  runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
}

void TGraphProcessor::releaseResources() {
  runtime.releaseResources();
}

void TGraphProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  runtime.setCurrentChannelLayout(0, buffer.getNumChannels());
  runtime.processBlock(buffer, midiMessages);
}

ITeulParamProvider &TGraphProcessor::paramProvider() noexcept { return runtime; }
const ITeulParamProvider &TGraphProcessor::paramProvider() const noexcept { return runtime; }

#endif

} // namespace Teul