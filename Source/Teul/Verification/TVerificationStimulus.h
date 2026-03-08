#pragma once
#include "Teul/Runtime/TGraphRuntime.h"
#include <vector>
namespace Teul {
enum class TVerificationStimulusKind {
  StaticRender,
  StepAutomation,
  SweepAutomation,
  MidiPhrase,
};
enum class TVerificationAutomationMode {
  Step,
  Linear,
};
struct TVerificationRenderProfile {
  juce::String profileId;
  double sampleRate = 48000.0;
  int blockSize = 128;
  int outputChannels = 2;
  double durationSeconds = 2.0;
};
struct TVerificationAutomationLane {
  juce::String nodeLabel;
  juce::String paramKey;
  TVerificationAutomationMode mode = TVerificationAutomationMode::Step;
  float startValue = 0.0f;
  float endValue = 0.0f;
  double stepIntervalSeconds = 0.25;
};
struct TVerificationMidiEvent {
  int sampleOffset = 0;
  juce::MidiMessage message;
};
struct TVerificationStimulusSpec {
  juce::String stimulusId;
  juce::String displayName;
  TVerificationStimulusKind kind = TVerificationStimulusKind::StaticRender;
  std::vector<TVerificationAutomationLane> automationLanes;
  std::vector<TVerificationMidiEvent> midiEvents;
};
struct TVerificationRenderResult {
  juce::String graphName;
  juce::String stimulusId;
  juce::String profileId;
  juce::AudioBuffer<float> audioBuffer;
  int totalSamples = 0;
  int renderedBlockCount = 0;
  TGraphRuntime::RuntimeStats runtimeStats;
};
TVerificationRenderProfile makePrimaryVerificationRenderProfile();
TVerificationRenderProfile makeSecondaryVerificationRenderProfile();
TVerificationRenderProfile makeExtendedVerificationRenderProfile();
TVerificationStimulusSpec makeStaticRenderStimulus();
TVerificationStimulusSpec makeStepAutomationStimulus(const juce::String &nodeLabel,
                                                     const juce::String &paramKey,
                                                     float startValue,
                                                     float endValue,
                                                     double stepIntervalSeconds = 0.25);
TVerificationStimulusSpec makeSweepAutomationStimulus(const juce::String &nodeLabel,
                                                      const juce::String &paramKey,
                                                      float startValue,
                                                      float endValue);
TVerificationStimulusSpec makeMidiPhraseStimulus();
bool renderGraphWithStimulus(const TNodeRegistry &registry,
                             const TGraphDocument &document,
                             const TVerificationRenderProfile &profile,
                             const TVerificationStimulusSpec &stimulus,
                             TVerificationRenderResult &resultOut,
                             juce::String *errorMessageOut = nullptr);
} // namespace Teul