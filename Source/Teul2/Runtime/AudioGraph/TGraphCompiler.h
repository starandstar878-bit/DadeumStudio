#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

class TTeulDocument;

// =============================================================================
//  TNodeRegistry — 런타임 노드 명세 카탈로그
// =============================================================================

class TNodeRegistry {
public:
  TNodeRegistry() = default;

  void registerNode(const TNodeDescriptor &desc);
  const TNodeDescriptor *descriptorFor(const juce::String &typeKey) const;
  const std::vector<TNodeDescriptor> &getAllDescriptors() const;

  std::vector<TTeulExposedParam>
  listExposedParamsForNode(const TNode &node) const;

private:
  std::vector<TNodeDescriptor> descriptors;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNodeRegistry)
};

std::unique_ptr<TNodeRegistry> makeDefaultNodeRegistry();

// =============================================================================
//  TNodeClass / Auto-Registration 시스템 (Teul/Registry/TNodeSDK.h 에서 이관)
// =============================================================================

class TNodeClass {
public:
  virtual ~TNodeClass() = default;
  virtual TNodeDescriptor makeDescriptor() const = 0;

  // 런타임 객체 생성 인터페이스
  virtual std::unique_ptr<TNodeInstance> createInstance() const {
    return nullptr;
  }
};

using TNodeClassFactory = std::function<std::unique_ptr<TNodeClass>()>;

class TNodeClassCatalog {
public:
  static void registerFactory(TNodeClassFactory factory) {
    factories().push_back(std::move(factory));
  }

  static const std::vector<TNodeClassFactory> &allFactories() {
    return factories();
  }

private:
  static std::vector<TNodeClassFactory> &factories() {
    static std::vector<TNodeClassFactory> registered;
    return registered;
  }
};

template <typename NodeClassType> class AutoNodeClassRegistration {
public:
  explicit AutoNodeClassRegistration(const char *debugName = nullptr) {
    TNodeClassCatalog::registerFactory([]() -> std::unique_ptr<TNodeClass> {
      return std::make_unique<NodeClassType>();
    });
  }
};

// =============================================================================
//  TGraphCompiler — 도큐먼트 컴파일러
// =============================================================================

class TGraphCompiler {
public:
  static juce::var compileDocumentJson(const TTeulDocument &document);
};

} // namespace Teul

// 매크로 병합 유틸
#define TEUL2_NODE_INTERNAL_JOIN2(a, b) a##b
#define TEUL2_NODE_INTERNAL_JOIN(a, b) TEUL2_NODE_INTERNAL_JOIN2(a, b)

/**
 * \brief 노드 클래스 구현체 하단에 선언하여 자동으로 레지스트리에 등록합니다.
 * (Teul2 용)
 */
#define TEUL2_NODE_REGISTER(NodeClassType)                                     \
  inline const ::Teul::AutoNodeClassRegistration<NodeClassType>                \
  TEUL2_NODE_INTERNAL_JOIN(gTeul2AutoRegister_, NodeClassType) {               \
    #NodeClassType                                                             \
  }
