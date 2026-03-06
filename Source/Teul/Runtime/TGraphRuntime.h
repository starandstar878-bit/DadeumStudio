#pragma once
#include "../Model/TGraphDocument.h"
#include "../Registry/TNodeRegistry.h"
#include "TNodeInstance.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace Teul {

// =============================================================================
//  TGraphRuntime — 위상 정렬 기반 오디오 그래프 실행 및 DSP 관리 엔진
//  (juce::AudioIODeviceCallback 을 상속받아 호스팅 및 플러그인 환경 호환 지원)
// =============================================================================
class TGraphRuntime : public juce::AudioIODeviceCallback {
public:
  TGraphRuntime(const TNodeRegistry *registry);
  ~TGraphRuntime();

  // ===========================================================================
  // 빌드 및 위상 정렬 (UI/메인 스레드 계열에서 동작)
  // ===========================================================================

  // 성공 시 true, 순환 참조(Cycle) 감지로 인한 실패 시 false 반환
  // 내부적으로 백그라운드에서 새 그래프를 만든 뒤 락프리 방식으로 스왑함
  bool buildGraph(const TGraphDocument &doc);

  // ===========================================================================
  // 수동 오디오/DSP 생명주기 제어용 (일반 버퍼 테스트나 커스텀 블록 처리 시)
  // ===========================================================================
  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);
  void releaseResources();
  void processBlock(juce::AudioBuffer<float> &deviceBuffer,
                    juce::MidiBuffer &midiMessages);

  // ===========================================================================
  // juce::AudioIODeviceCallback 필수 오버라이드
  // ===========================================================================
  void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
  void audioDeviceStopped() override;
  void audioDeviceIOCallback(const float **inputChannelData,
                             int numInputChannels, float **outputChannelData,
                             int numOutputChannels, int numSamples) override;

private:
  const TNodeRegistry *nodeRegistry;

  double currentSampleRate = 48000.0;
  int currentBlockSize = 256;

  // 한 포트 채널에서 다른 포트 채널로 데이터를 믹싱(합산)해야 하는 연결 정보
  struct MixOp {
    int srcChannelIndex;
    int dstChannelIndex;
  };

  // 런타임에 정렬되어 실행될 노드 엔트리 단위
  struct NodeEntry {
    NodeId nodeId;
    const TNode *nodeData = nullptr;
    std::unique_ptr<TNodeInstance> instance;
    std::vector<MixOp> preProcessMixes;
    std::map<PortId, int> portChannels;
  };

  // 스레드 안전한 더블 버퍼링/락프리 구조체
  struct RenderState : public juce::ReferenceCountedObject {
    using Ptr = juce::ReferenceCountedObjectPtr<RenderState>;
    std::vector<NodeEntry> sortedNodes;
    juce::AudioBuffer<float> globalPortBuffer;
    int totalAllocatedChannels = 0;
  };

  // 현재 실행 중인 렌더링 그래프 상태 (오디오 스레드 vs 메인 스레드 락프리
  // 접근용)
  struct AtomicState {
    std::atomic<RenderState *> state{nullptr};

    RenderState::Ptr get() {
      // 락프리 읽기: 참조 카운트를 증가시킬 수는 없지만(std::atomic이므로),
      // juce::ReferenceCountedObjectPtr를 안전하게 얻는 헬퍼가 필요함.
      // 간단한 구현으로는 교체(swap)가 일어나는 동안 가비지로 해제되는 걸 막기
      // 위해 hazard pointer 나 std::shared_ptr<const RenderState> 를 사용. 앱
      // 규모에서는 일단 반환 전 lock 을 쓰거나 스왑 시 지연 삭제를 도입함.
      return state.load(std::memory_order_acquire);
    }

    void set(RenderState *newState) {
      if (newState)
        newState->incReferenceCount();
      auto *old = state.exchange(newState, std::memory_order_release);
      if (old)
        old->decReferenceCount();
    }
  } activeState;

  // ===========================================================================
  // Phase 3 단계 2: 파라미터 업데이트용 Lock-free FIFO
  // UI 스레드에서 생성된 파라미터 변경 이벤트를 오디오 스레드에서 락 프리로
  // 꺼냄
  // ===========================================================================
  struct ParamChange {
    NodeId nodeId;
    // 오디오용으로 float 값만 우선 지원 (복잡한 객체/문자열은 allocation 이슈로
    // 배제)
    float value = 0.0f;
    // 최대 31글자의 파라미터 키 (ex. "frequency", "gain")
    char paramKey[32] = {0};
  };

  static constexpr int kMaxParamQueueSize = 1024;
  juce::AbstractFifo paramQueueFifo{kMaxParamQueueSize};
  std::array<ParamChange, kMaxParamQueueSize> paramQueueData;

public:
  // UI 스레드에서 호출 (ex. 슬라이더 조작)
  void queueParameterChange(NodeId nodeId, const juce::String &paramKey,
                            float value);

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphRuntime)
};

} // namespace Teul
