#pragma once

#include <JuceHeader.h>
#include <memory>

namespace Teul {
struct TGraphDocument;
}

namespace Teul {

// =============================================================================
//  EditorHandle — Teul 전체 UI 루트 컴포넌트
//
//  MainComponent 가 addChildComponent() 로 보유하며,
//  setVisible() 로 페이지 전환 시 숨기거나 보여준다.
//  숨겨져도 TGraphRuntime(Phase 3)은 오디오 스레드에서 계속 동작한다.
//
//  TODO [Phase 1 UI]: TGraphCanvas, NodeLibraryPanel, PropertyPanel 구성
//  TODO [Phase 3]:    생성자에서 AudioDeviceManager& 를 받아 TGraphRuntime
//  초기화
// =============================================================================
class EditorHandle : public juce::Component {
public:
  EditorHandle();
  ~EditorHandle() override;

  // TODO [Phase 1]: TGraphDocument 접근자
  // TGraphDocument& document() noexcept;
  // const TGraphDocument& document() const noexcept;

  // TODO [Phase 3]: 오디오 재생 제어
  // void startAudioProcessing();
  // void stopAudioProcessing();
  // bool isAudioRunning() const noexcept;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorHandle)
};

// 팩토리 함수 — MainComponent 가 호출
// TODO [Phase 3]: AudioDeviceManager& 파라미터 추가
std::unique_ptr<EditorHandle> createEditor();

} // namespace Teul
