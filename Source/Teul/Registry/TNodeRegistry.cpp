#include "TNodeRegistry.h"
#include "Nodes/Core/FXNodes.h"
#include "Nodes/Core/FilterNodes.h"
#include "Nodes/Core/MathLogicNodes.h"
#include "Nodes/Core/MidiNodes.h"
#include "Nodes/Core/MixerNodes.h"
#include "Nodes/Core/ModulationNodes.h"
#include "Nodes/Core/SourceNodes.h"
#include "TNodeSDK.h"


namespace Teul {

void TNodeRegistry::registerNode(const TNodeDescriptor &desc) {
  // 이미 존재하는 타입 키면 무시하거나 덮어쓰기 로직 필요
  for (auto &existing : descriptors) {
    if (existing.typeKey == desc.typeKey) {
      existing = desc;
      return;
    }
  }
  descriptors.push_back(desc);
}

const TNodeDescriptor *
TNodeRegistry::descriptorFor(const juce::String &typeKey) const {
  for (const auto &desc : descriptors) {
    if (desc.typeKey == typeKey)
      return &desc;
  }
  return nullptr;
}

const std::vector<TNodeDescriptor> &TNodeRegistry::getAllDescriptors() const {
  return descriptors;
}

std::unique_ptr<TNodeRegistry> makeDefaultNodeRegistry() {
  auto registry = std::make_unique<TNodeRegistry>();

  for (const auto &factory : TNodeClassCatalog::allFactories()) {
    if (!factory)
      continue;

    auto nodeClass = factory();
    if (nodeClass) {
      registry->registerNode(nodeClass->makeDescriptor());
    }
  }

  return registry;
}

} // namespace Teul
