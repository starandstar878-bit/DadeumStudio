#include "TNodeRegistry.h"
#include "TNodeSDK.h"

// =============================================================================
//  ⚠️ 주의: 매크로(TEUL_NODE_AUTOREGISTER)가 런타임에 실행되려면,
//  반드시 최소 하나의 .cpp 파일에서 해당 헤더들을 #include 해주어야 합니다.
//  CoreNodes.h 를 통해 모든 기본 제공 노드들을 일괄 포함시킵니다.
// =============================================================================
#include "Nodes/CoreNodes.h"

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
