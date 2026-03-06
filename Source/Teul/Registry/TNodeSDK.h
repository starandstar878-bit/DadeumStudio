#pragma once

#include "TNodeRegistry.h"
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

class TNodeInstance;

// =============================================================================
//  TNodeClass / Auto-Registration 시스템 (Gyeol WidgetSDK 참고)
// =============================================================================

// 각 센서/DSP 로직이 자신의 디스크립터를 제공하는 추상 인터페이스
class TNodeClass {
public:
  virtual ~TNodeClass() = default;
  virtual TNodeDescriptor makeDescriptor() const = 0;

  // 런타임 객체 생성 인터페이스 (커스텀 DSP 클래스가 오버라이드하여 제공)
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

// =============================================================================
//  C++ 매크로를 이용한 정적 자동 등록 (DLL/앱 시작 시)
// =============================================================================

template <typename NodeClassType> class AutoNodeClassRegistration {
public:
  explicit AutoNodeClassRegistration(const char *debugName = nullptr) {
    TNodeClassCatalog::registerFactory([]() -> std::unique_ptr<TNodeClass> {
      return std::make_unique<NodeClassType>();
    });

#if JUCE_DEBUG
    if (debugName != nullptr && debugName[0] != '\0')
      DBG("[Teul][Registry] Auto-registered node class: " << debugName);
    else
      DBG("[Teul][Registry] Auto-registered unnamed node class.");
#endif
  }
};

} // namespace Teul

// 매크로 병합 유틸
#define TEUL_NODE_INTERNAL_JOIN2(a, b) a##b
#define TEUL_NODE_INTERNAL_JOIN(a, b) TEUL_NODE_INTERNAL_JOIN2(a, b)

/**
 * \brief 노드 클래스 구현체 하단에 선언하여 자동으로 레지스트리에 등록합니다.
 * 사용법: TEUL_NODE_AUTOREGISTER(MyOscillatorNode);
 */
#define TEUL_NODE_AUTOREGISTER(NodeClassType)                                  \
  inline const ::Teul::AutoNodeClassRegistration<NodeClassType>                \
  TEUL_NODE_INTERNAL_JOIN(gTeulAutoRegister_, NodeClassType) {                 \
    #NodeClassType                                                             \
  }
