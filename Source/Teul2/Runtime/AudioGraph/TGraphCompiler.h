#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <atomic>

namespace Teul {

class TTeulDocument;
class TNodeInstance;

// =============================================================================
//  컴파일된 그래프 실행에 필요한 데이터 구조 (TGraphRuntime.h 에서 이관)
// =============================================================================

struct TMixOp {
  int srcChannelIndex = -1;
  int dstChannelIndex = -1;
};

struct TNodeEntry {
  NodeId nodeId = kInvalidNodeId;
  TNode nodeSnapshot;
  std::unique_ptr<TNodeInstance> instance;
  std::vector<TMixOp> preProcessMixes;
  std::map<PortId, int> portChannels;
  std::map<PortId, juce::MidiBuffer> midiInputBuffers;
  std::map<PortId, juce::MidiBuffer> midiOutputBuffers;
};

struct TPortTelemetry {
  PortId portId = kInvalidPortId;
  NodeId nodeId = kInvalidNodeId;
  int channelIndex = -1;
  TPortDataType dataType = TPortDataType::Audio;
};

struct TRailInputSource {
  juce::String endpointId;
  juce::String portId;
  int channelIndex = -1;
  int deviceChannelIndex = -1;
  TPortDataType dataType = TPortDataType::Audio;
};

struct TRailOutputTarget {
  juce::String endpointId;
  juce::String portId;
  int sourceChannelIndex = -1;
  int deviceChannelIndex = -1;
  TPortDataType dataType = TPortDataType::Audio;
};

struct TRailMidiInputTarget {
  juce::String endpointId;
  juce::String portId;
  std::size_t targetNodeIndex = 0;
  PortId targetPortId = kInvalidPortId;
};

struct TRailMidiOutputTarget {
  juce::String endpointId;
  juce::String portId;
  std::size_t sourceNodeIndex = 0;
  PortId sourcePortId = kInvalidPortId;
};

struct TMidiRoute {
  std::size_t sourceNodeIndex = 0;
  PortId sourcePortId = kInvalidPortId;
  std::size_t targetNodeIndex = 0;
  PortId targetPortId = kInvalidPortId;
};

struct TControlRoute {
  juce::String sourceKey;
  juce::String paramId;
  TControlPortKind portKind = TControlPortKind::value;
  TParamValueType valueType = TParamValueType::Auto;
  TParamWidgetHint preferredWidget = TParamWidgetHint::Auto;
  juce::var defaultValue;
  juce::var minValue;
  juce::var maxValue;
  std::vector<TParamOptionSpec> enumOptions;
  bool inverted = false;
  float rangeMin = 0.0f;
  float rangeMax = 1.0f;
};

struct TParamDispatch {
  NodeId nodeId = kInvalidNodeId;
  TNodeInstance *instance = nullptr;
  juce::String paramKey;
  std::array<char, 32> paramKeyUtf8{};
  float currentValue = 0.0f;
  float targetValue = 0.0f;
  bool smoothingEnabled = false;
};

/**
 * \brief 컴파일된 실행용 그래프 상태 (구 RenderState)
 */
struct TCompiledGraph : public juce::ReferenceCountedObject {
  using Ptr = juce::ReferenceCountedObjectPtr<TCompiledGraph>;

  std::vector<TNodeEntry> sortedNodes;
  juce::AudioBuffer<float> globalPortBuffer;
  std::vector<TPortTelemetry> portTelemetry;
  std::map<PortId, std::size_t> portTelemetryIndex;
  std::unique_ptr<std::atomic<float>[]> portLevels;
  std::vector<TRailInputSource> railInputSources;
  std::vector<TRailOutputTarget> railOutputTargets;
  std::vector<TRailMidiInputTarget> railMidiInputTargets;
  std::vector<TRailMidiOutputTarget> railMidiOutputTargets;
  std::vector<TMidiRoute> midiRoutes;
  std::vector<TControlRoute> controlRoutes;
  std::vector<TParamDispatch> paramDispatches;
  std::uint64_t generation = 0;
  int totalAllocatedChannels = 0;
};

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

  /**
   * \brief TTeulDocument를 실행 가능한 TCompiledGraph로 컴파일합니다.
   * (TGraphRuntime::buildGraph 로직 이관)
   */
  static TCompiledGraph::Ptr compileDocument(const TTeulDocument &document,
                                             const TNodeRegistry &registry,
                                             double sampleRate,
                                             int blockSize);
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
