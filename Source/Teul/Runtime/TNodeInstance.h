#pragma once

#include "../Model/TNode.h"
#include "../Model/TTypes.h"
#include <JuceHeader.h>
#include <map>

namespace Teul {

class TParamValueReporter {
public:
  virtual ~TParamValueReporter() = default;
  virtual void reportParamValueChange(NodeId nodeId,
                                      const juce::String &paramKey,
                                      float value) = 0;
};

struct TProcessContext {
  juce::AudioBuffer<float> *globalPortBuffer = nullptr;
  juce::AudioBuffer<float> *deviceAudioBuffer = nullptr;
  juce::MidiBuffer *midiMessages = nullptr;
  const std::map<PortId, int> *portToChannel = nullptr;
  const TNode *nodeData = nullptr;
  TParamValueReporter *paramValueReporter = nullptr;
};

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

} // namespace Teul