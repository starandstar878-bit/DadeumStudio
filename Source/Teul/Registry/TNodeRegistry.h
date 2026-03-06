#pragma once

#include "../Bridge/ITeulParamProvider.h"
#include "../Model/TNode.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Teul {

struct TParamSpec {
  juce::String key;
  juce::String label;
  juce::var defaultValue;
};

struct TPortSpec {
  TPortDirection direction;
  TPortDataType dataType;
  juce::String name;
};

struct TNodeCapabilities {
  bool canBypass = true;
  bool canMute = false;
  bool canSolo = false;
  bool isTimeDependent = false;

  int minInstances = 0;
  int maxInstances = -1;
  int maxPolyphony = 1;
  float processingLatencyMs = 0.0f;
  int estimatedCpuCost = 1;
};

class TNodeInstance;

struct TNodeDescriptor {
  juce::String typeKey;
  juce::String displayName;
  juce::String category;

  TNodeCapabilities capabilities;

  std::vector<TParamSpec> paramSpecs;
  std::vector<TPortSpec> portSpecs;

  std::function<std::unique_ptr<TNodeInstance>()> instanceFactory;
  std::function<std::vector<TTeulExposedParam>(const TNode &node)>
      exposedParamFactory;
};

class TNodeRegistry {
public:
  TNodeRegistry() = default;

  void registerNode(const TNodeDescriptor &desc);
  const TNodeDescriptor *descriptorFor(const juce::String &typeKey) const;
  const std::vector<TNodeDescriptor> &getAllDescriptors() const;
  std::vector<TTeulExposedParam> listExposedParamsForNode(
      const TNode &node) const;

private:
  std::vector<TNodeDescriptor> descriptors;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNodeRegistry)
};

std::unique_ptr<TNodeRegistry> makeDefaultNodeRegistry();

} // namespace Teul