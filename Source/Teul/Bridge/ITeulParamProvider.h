#pragma once

#include "../Model/TTypes.h"
#include <JuceHeader.h>
#include <vector>

namespace Teul {

struct TTeulExposedParam {
  juce::String paramId;
  NodeId nodeId = kInvalidNodeId;
  juce::String nodeTypeKey;
  juce::String nodeDisplayName;
  juce::String paramKey;
  juce::String paramLabel;
  juce::var defaultValue;
  juce::var currentValue;
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

} // namespace Teul