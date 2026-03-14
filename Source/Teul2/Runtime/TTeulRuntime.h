#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include "Teul2/Runtime/AudioGraph/TRuntimeDiagnostics.h"

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

class TTeulDocument;
class TNodeRegistry;

#ifndef TEUL_PARAM_PROVIDER_CONTRACT_DEFINED
#define TEUL_PARAM_PROVIDER_CONTRACT_DEFINED 1

enum class TParamValueType {
  Auto,
  Float,
  Int,
  Bool,
  Enum,
  String,
};

enum class TParamWidgetHint {
  Auto,
  Slider,
  Combo,
  Toggle,
  Text,
};

struct TParamOptionSpec {
  juce::String id;
  juce::String label;
  juce::var value;
};

struct TTeulExposedParam {
  juce::String paramId;
  NodeId nodeId = kInvalidNodeId;
  juce::String nodeTypeKey;
  juce::String nodeDisplayName;
  juce::String paramKey;
  juce::String paramLabel;
  juce::var defaultValue;
  juce::var currentValue;
  TParamValueType valueType = TParamValueType::Auto;
  juce::var minValue;
  juce::var maxValue;
  juce::var step;
  juce::String unitLabel;
  int displayPrecision = 2;
  juce::String group;
  juce::String description;
  std::vector<TParamOptionSpec> enumOptions;
  TParamWidgetHint preferredWidget = TParamWidgetHint::Auto;
  bool showInNodeBody = false;
  bool showInPropertyPanel = true;
  bool isReadOnly = false;
  bool isAutomatable = true;
  bool isModulatable = true;
  bool isDiscrete = false;
  bool exposeToIeum = true;
  juce::String preferredBindingKey;
  juce::String exportSymbol;
  juce::String categoryPath;
};

inline juce::String makeTeulParamId(NodeId nodeId,
                                    const juce::String &paramKey) {
  return "teul.node." + juce::String(nodeId) + "." + paramKey;
}

inline bool parseTeulParamId(const juce::String &paramId,
                             NodeId &outNodeId,
                             juce::String &outParamKey) {
  const juce::String prefix = "teul.node.";
  if (!paramId.startsWith(prefix))
    return false;

  const juce::String tail = paramId.fromFirstOccurrenceOf(prefix, false, false);
  const int split = tail.indexOfChar('.');
  if (split <= 0)
    return false;

  outNodeId = tail.substring(0, split).getIntValue();
  outParamKey = tail.substring(split + 1);
  return outNodeId != kInvalidNodeId && outParamKey.isNotEmpty();
}

class ITeulParamProvider {
public:
  class Listener {
  public:
    virtual ~Listener() = default;
    virtual void teulParamSurfaceChanged() {}
    virtual void teulParamValueChanged(const TTeulExposedParam &param) = 0;
  };

  virtual ~ITeulParamProvider() = default;

  virtual std::vector<TTeulExposedParam> listExposedParams() const = 0;
  virtual juce::var getParam(const juce::String &paramId) const = 0;
  virtual bool setParam(const juce::String &paramId, const juce::var &value) = 0;
  virtual void addListener(Listener *listener) = 0;
  virtual void removeListener(Listener *listener) = 0;
};

#endif

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
