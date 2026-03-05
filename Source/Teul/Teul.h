// BOM
#pragma once

// =============================================================================
//  Teul (틀) — DSP 그래프 에디터 퍼블릭 진입점
//
//  외부 코드는 이 헤더만 include 한다.
//  내부 구현은 Source/Teul/... 하위에 위치한다.
//
//  현재 상태: STUB (Phase 1 구현 전 토대)
// =============================================================================

// TODO [Phase 1] TNode, TPort, TConnection, TGraphDocument 데이터 모델 정의
// TODO [Phase 1] JSON 직렬화 / 역직렬화 (toJson / fromJson)
// TODO [Phase 1] Undo / Redo 커맨드 스택
// TODO [Phase 2] TNodeDescriptor, TNodeRegistry, makeDefaultNodeRegistry()
// TODO [Phase 3] TGraphRuntime — AudioDeviceManager 를 외부에서 주입받아 사용
//                 └─ juce::AudioIODeviceCallback 구현체
//                 └─ 락-프리 파라미터 교환 (std::atomic / lock-free FIFO)
//                 └─ Double-buffer 그래프 스왑
// TODO [Phase 4] ITeulParamProvider — Ieum 연동 인터페이스

#include <JuceHeader.h>
#include <memory>

namespace Teul {
// -------------------------------------------------------------------------
//  EditorHandle — Teul 전체 UI 루트 컴포넌트
//
//  MainComponent 가 addChildComponent() 로 보유하며,
//  setVisible() 로 페이지 전환 시 숨기거나 보여준다.
//  숨겨져도 TGraphRuntime(오디오 스레드)은 계속 동작한다.
//
//  TODO [Phase 3] 생성자에서 AudioDeviceManager& 를 받아 TGraphRuntime 초기화
// -------------------------------------------------------------------------
class EditorHandle : public juce::Component {
public:
  EditorHandle();
  ~EditorHandle() override;

  // TODO [Phase 1] TGraphDocument 접근자
  // TGraphDocument& document() noexcept;

  // TODO [Phase 3] 오디오 재생 제어
  // void startAudioProcessing();
  // void stopAudioProcessing();
  // bool isAudioRunning() const noexcept;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  // TODO: Impl (pimpl) 으로 교체 예정
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorHandle)
};

// 팩토리 함수 — MainComponent 가 호출
// TODO [Phase 3] AudioDeviceManager& 파라미터 추가
std::unique_ptr<EditorHandle> createEditor();

} // namespace Teul
