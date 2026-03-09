// BOM
#pragma once

#include "AppServices.h"
#include "Gyeol/Gyeol.h"
#include "Teul/Teul.h"
#include <JuceHeader.h>

// =============================================================================
//  AppPage — 현재 활성 페이지 열거형
// =============================================================================
enum class AppPage {
  Gyeol, // UI 편집기
  Teul,  // DSP 그래프 에디터
         // Ieum, // TODO: Ieum 브리지 페이지 (미래)
};

// =============================================================================
//  AppPageTabBar — 하단 페이지 전환 탭바
//
//  TODO [UI]: 아이콘 + 레이블 형식으로 개선
//  TODO [UI]: 활성 탭 하이라이트 애니메이션
// =============================================================================
class AppPageTabBar : public juce::Component {
public:
  std::function<void(AppPage)> onPageSelected;

  explicit AppPageTabBar(AppPage initialPage) : currentPage(initialPage) {
    addAndMakeVisible(gyeolButton);
    addAndMakeVisible(teulButton);

    gyeolButton.setButtonText(
        juce::CharPointer_UTF8("\xf0\x9f\x8e\xa8  Gyeol"));
    teulButton.setButtonText(juce::CharPointer_UTF8("\xf0\x9f\x94\x8a  Teul"));

    gyeolButton.setRadioGroupId(1);
    teulButton.setRadioGroupId(1);
    gyeolButton.setClickingTogglesState(true);
    teulButton.setClickingTogglesState(true);

    gyeolButton.setToggleState(currentPage == AppPage::Gyeol,
                               juce::dontSendNotification);
    teulButton.setToggleState(currentPage == AppPage::Teul,
                              juce::dontSendNotification);

    gyeolButton.onClick = [this] { switchTo(AppPage::Gyeol); };
    teulButton.onClick = [this] { switchTo(AppPage::Teul); };
  }

  void resized() override {
    auto area = getLocalBounds().reduced(4, 4);
    const int tabW = 140;
    gyeolButton.setBounds(area.removeFromLeft(tabW));
    area.removeFromLeft(4);
    teulButton.setBounds(area.removeFromLeft(tabW));
  }

private:
  void switchTo(AppPage page) {
    currentPage = page;
    if (onPageSelected)
      onPageSelected(page);
  }

  AppPage currentPage;
  juce::TextButton gyeolButton, teulButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppPageTabBar)
};

// =============================================================================
//  MainComponent
//
//  레이아웃:
//    ┌─────────────────────────────────────────────┐
//    │ [GlobalToolbar]              42px           │
//    ├─────────────────────────────────────────────┤
//    │                                             │
//    │   현재 페이지 (Gyeol 또는 Teul)              │
//    │   setVisible() 전환 — 숨겨도 오디오는 유지   │
//    │                                             │
//    ├─────────────────────────────────────────────┤
//    │ [AppPageTabBar]              40px           │
//    └─────────────────────────────────────────────┘
//
//  AppServices 는 참조로만 보유 — 수명은 DadeumStudioApplication 이 관리
// =============================================================================
class MainComponent : public juce::Component, private juce::Timer {
public:
  explicit MainComponent(AppServices &services);
  ~MainComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void timerCallback() override;

  // -------------------------------------------------------------------------
  //  페이지 전환
  // -------------------------------------------------------------------------
  void switchToPage(AppPage page);

  // -------------------------------------------------------------------------
  //  세션 저장/복원
  // -------------------------------------------------------------------------
  void restoreSession();
  void persistSession() const;
  void updateTeulSessionStatus() const;
  static juce::File sessionFilePath();
  static juce::File teulSessionFilePath();
  static juce::File sessionStateFilePath();

  // -------------------------------------------------------------------------
  //  앱 공통 서비스 (참조, 수명 소유하지 않음)
  // -------------------------------------------------------------------------
  AppServices &appServices;

  // -------------------------------------------------------------------------
  //  현재 활성 페이지
  // -------------------------------------------------------------------------
  AppPage currentPage{AppPage::Teul};

  // -------------------------------------------------------------------------
  //  서브 에디터 페이지
  //  addChildComponent() 로 등록 → setVisible() 로 전환
  //  숨겨도 인스턴스는 살아있어 상태를 유지한다.
  // -------------------------------------------------------------------------
  std::unique_ptr<Gyeol::EditorHandle> gyeolPage;
  std::unique_ptr<Teul::EditorHandle> teulPage; // TODO [Phase 1]: 구현 채우기

  // -------------------------------------------------------------------------
  //  UI 크롬
  // -------------------------------------------------------------------------
  // TODO [UI]: GlobalToolbar 컴포넌트로 분리
  //            - 재생/정지 버튼 (→ teulPage 의 TGraphRuntime 제어)
  //            - 오디오 장치 선택 콤보박스 (→ appServices.audioDeviceManager)
  //            - 프로젝트 저장 버튼 (Gyeol + Teul 통합 세이브)
  std::unique_ptr<AppPageTabBar> pageTabBar;
  mutable std::uint64_t lastPersistedGyeolHistorySerial = 0;
  mutable std::uint64_t lastPersistedTeulDocumentRevision = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
