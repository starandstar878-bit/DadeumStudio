#include "Teul/Verification/TVerificationStimulus.h"
#include <algorithm>
#include <cmath>
namespace Teul {
namespace {
NodeId findNodeIdByLabel(const TGraphDocument &document,
                         const juce::String &nodeLabel) {
  for (const auto &node : document.nodes) {
    if (node.label.equalsIgnoreCase(nodeLabel))
      return node.nodeId;
  }
  return kInvalidNodeId;
}
float valueForLaneAtSample(const TVerificationAutomationLane &lane,
                           int sampleIndex,
                           int totalSamples,
                           double sampleRate) {
  if (lane.mode == TVerificationAutomationMode::Linear) {
    if (totalSamples <= 1)
      return lane.endValue;
    const float progress = juce::jlimit(
        0.0f, 1.0f, static_cast<float>(sampleIndex) / static_cast<float>(totalSamples - 1));
    return lane.startValue + ((lane.endValue - lane.startValue) * progress);
  }
  const int stepIntervalSamples = juce::jmax(
      1, juce::roundToInt(juce::jmax(0.001, lane.stepIntervalSeconds) * sampleRate));
  const int segmentIndex = sampleIndex / stepIntervalSamples;
  return (segmentIndex % 2 == 0) ? lane.startValue : lane.endValue;
}
void writeError(juce::String *errorMessageOut, const juce::String &message) {
  if (errorMessageOut != nullptr)
    *errorMessageOut = message;
}
} // namespace
TVerificationRenderProfile makePrimaryVerificationRenderProfile() {
  return {"primary", 48000.0, 128, 2, 2.0};
}
TVerificationRenderProfile makeSecondaryVerificationRenderProfile() {
  return {"secondary", 48000.0, 480, 2, 2.0};
}
TVerificationRenderProfile makeExtendedVerificationRenderProfile() {
  return {"extended", 96000.0, 128, 2, 2.0};
}
TVerificationStimulusSpec makeStaticRenderStimulus() {
  return {"S1", "Static Render", TVerificationStimulusKind::StaticRender, {}, {}};
}
TVerificationStimulusSpec makeStepAutomationStimulus(const juce::String &nodeLabel,
                                                     const juce::String &paramKey,
                                                     float startValue,
                                                     float endValue,
                                                     double stepIntervalSeconds) {
  TVerificationStimulusSpec stimulus;
  stimulus.stimulusId = "S2";
  stimulus.displayName = "Step Automation";
  stimulus.kind = TVerificationStimulusKind::StepAutomation;
  stimulus.automationLanes.push_back(
      {nodeLabel, paramKey, TVerificationAutomationMode::Step, startValue,
       endValue, stepIntervalSeconds});
  return stimulus;
}
TVerificationStimulusSpec makeSweepAutomationStimulus(const juce::String &nodeLabel,
                                                      const juce::String &paramKey,
                                                      float startValue,
                                                      float endValue) {
  TVerificationStimulusSpec stimulus;
  stimulus.stimulusId = "S3";
  stimulus.displayName = "Sweep Automation";
  stimulus.kind = TVerificationStimulusKind::SweepAutomation;
  stimulus.automationLanes.push_back(
      {nodeLabel, paramKey, TVerificationAutomationMode::Linear, startValue,
       endValue, 0.25});
  return stimulus;
}
TVerificationStimulusSpec makeMidiPhraseStimulus() {
  TVerificationStimulusSpec stimulus;
  stimulus.stimulusId = "S4";
  stimulus.displayName = "MIDI Phrase";
  stimulus.kind = TVerificationStimulusKind::MidiPhrase;
  const double sampleRate = 48000.0;
  auto sampleOffsetForSeconds = [sampleRate](double seconds) {
    return juce::roundToInt(seconds * sampleRate);
  };
  stimulus.midiEvents.push_back(
      {sampleOffsetForSeconds(0.00), juce::MidiMessage::noteOn(1, 60, (juce::uint8)96)});
  stimulus.midiEvents.push_back(
      {sampleOffsetForSeconds(0.35), juce::MidiMessage::noteOff(1, 60)});
  stimulus.midiEvents.push_back(
      {sampleOffsetForSeconds(0.55), juce::MidiMessage::noteOn(1, 64, (juce::uint8)110)});
  stimulus.midiEvents.push_back(
      {sampleOffsetForSeconds(0.95), juce::MidiMessage::noteOff(1, 64)});
  stimulus.midiEvents.push_back(
      {sampleOffsetForSeconds(1.15), juce::MidiMessage::noteOn(1, 67, (juce::uint8)100)});
  stimulus.midiEvents.push_back(
      {sampleOffsetForSeconds(1.65), juce::MidiMessage::noteOff(1, 67)});
  return stimulus;
}
bool renderGraphWithStimulus(const TNodeRegistry &registry,
                             const TGraphDocument &document,
                             const TVerificationRenderProfile &profile,
                             const TVerificationStimulusSpec &stimulus,
                             TVerificationRenderResult &resultOut,
                             juce::String *errorMessageOut) {
  if (profile.sampleRate <= 0.0 || profile.blockSize <= 0 ||
      profile.outputChannels <= 0 || profile.durationSeconds <= 0.0) {
    writeError(errorMessageOut, "Invalid verification render profile.");
    return false;
  }
  std::vector<std::pair<NodeId, TVerificationAutomationLane>> resolvedLanes;
  resolvedLanes.reserve(stimulus.automationLanes.size());
  for (const auto &lane : stimulus.automationLanes) {
    const NodeId nodeId = findNodeIdByLabel(document, lane.nodeLabel);
    if (nodeId == kInvalidNodeId) {
      writeError(errorMessageOut,
                 "Verification stimulus references a missing node label: " +
                     lane.nodeLabel);
      return false;
    }
    resolvedLanes.push_back({nodeId, lane});
  }
  std::vector<TVerificationMidiEvent> midiEvents = stimulus.midiEvents;
  std::sort(midiEvents.begin(), midiEvents.end(), [](const auto &a, const auto &b) {
    return a.sampleOffset < b.sampleOffset;
  });
  TGraphRuntime runtime(&registry);
  if (!runtime.buildGraph(document)) {
    writeError(errorMessageOut, "Failed to build verification graph.");
    return false;
  }
  runtime.setCurrentChannelLayout(0, profile.outputChannels);
  runtime.prepareToPlay(profile.sampleRate, profile.blockSize);
  const int totalSamples = juce::jmax(
      1, juce::roundToInt(profile.durationSeconds * profile.sampleRate));
  resultOut.graphName = document.meta.name;
  resultOut.stimulusId = stimulus.stimulusId;
  resultOut.profileId = profile.profileId;
  resultOut.totalSamples = totalSamples;
  resultOut.audioBuffer.setSize(profile.outputChannels, totalSamples, false, false, true);
  resultOut.audioBuffer.clear();
  resultOut.renderedBlockCount = 0;
  std::size_t midiEventIndex = 0;
  for (int blockStart = 0; blockStart < totalSamples; blockStart += profile.blockSize) {
    const int blockSamples = juce::jmin(profile.blockSize, totalSamples - blockStart);
    for (const auto &resolvedLane : resolvedLanes) {
      const float value = valueForLaneAtSample(resolvedLane.second, blockStart,
                                               totalSamples, profile.sampleRate);
      runtime.queueParameterChange(resolvedLane.first, resolvedLane.second.paramKey,
                                   value);
    }
    juce::AudioBuffer<float> blockBuffer(profile.outputChannels, blockSamples);
    juce::MidiBuffer midiBuffer;
    while (midiEventIndex < midiEvents.size() &&
           midiEvents[midiEventIndex].sampleOffset < (blockStart + blockSamples)) {
      const auto &event = midiEvents[midiEventIndex];
      if (event.sampleOffset >= blockStart) {
        midiBuffer.addEvent(event.message, event.sampleOffset - blockStart);
      }
      ++midiEventIndex;
    }
    runtime.processBlock(blockBuffer, midiBuffer);
    for (int channel = 0; channel < profile.outputChannels; ++channel) {
      resultOut.audioBuffer.copyFrom(channel, blockStart, blockBuffer, channel, 0,
                                     blockSamples);
    }
    ++resultOut.renderedBlockCount;
  }
  resultOut.runtimeStats = runtime.getRuntimeStats();
  return true;
}
} // namespace Teul