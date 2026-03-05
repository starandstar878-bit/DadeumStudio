// BOM
#include "MainComponent.h"

// =============================================================================
//  MainComponent
// =============================================================================
MainComponent::MainComponent(AppServices &services) : appServices(services) {
  // ------------------------------------------------------------------
  //  페이지 생성 (addChildComponent → 처음엔 숨겨져 있음)
  // ------------------------------------------------------------------
  gyeolPage = Gyeol::createEditor();
  teulPage = Teul::createEditor();

  addChildComponent(*gyeolPage);
  addChildComponent(*teulPage);

  // ------------------------------------------------------------------
  //  탭바 생성 및 콜백 연결
  // ------------------------------------------------------------------
  pageTabBar = std::make_unique<AppPageTabBar>(currentPage);
  addAndMakeVisible(*pageTabBar);

  pageTabBar->onPageSelected = [this](AppPage page) { switchToPage(page); };

  // ------------------------------------------------------------------
  //  초기 페이지 활성化
  // ------------------------------------------------------------------
  switchToPage(currentPage);

  // ------------------------------------------------------------------
  //  세션 복원 (Gyeol 문서)
  //  TODO [Phase 1]: Teul 그래프 문서도 함께 복원
  // ------------------------------------------------------------------
  restoreSession();

  setSize(600, 400);
}

MainComponent::~MainComponent() { persistSession(); }

// =============================================================================
//  페이지 전환
// =============================================================================
void MainComponent::switchToPage(AppPage page) {
  currentPage = page;

  //  숨기기 / 보이기만 한다 — 인스턴스는 절대 파괴하지 않음.
  //  → Teul 의 TGraphRuntime 은 숨겨져도 오디오 스레드에서 계속 동작한다.
  gyeolPage->setVisible(page == AppPage::Gyeol);
  teulPage->setVisible(page == AppPage::Teul);

  resized();
}

// =============================================================================
//  paint / resized
// =============================================================================
void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized() {
  auto bounds = getLocalBounds();

  // TODO [UI]: GlobalToolbar 가 생기면 아래 주석 해제
  // if (globalToolbar != nullptr)
  //     globalToolbar->setBounds (bounds.removeFromTop (42));

  // 하단 탭바
  if (pageTabBar != nullptr)
    pageTabBar->setBounds(bounds.removeFromBottom(40));

  // 나머지 = 페이지 영역 (모든 페이지 동일 크기로 설정)
  if (gyeolPage != nullptr)
    gyeolPage->setBounds(bounds);
  if (teulPage != nullptr)
    teulPage->setBounds(bounds);
}

// =============================================================================
//  세션 저장 / 복원
// =============================================================================
juce::File MainComponent::sessionFilePath() {
  auto dir =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getChildFile("DadeumStudio");
  if (!dir.exists())
    dir.createDirectory();

  if (!dir.exists()) {
    dir = juce::File::getCurrentWorkingDirectory()
              .getChildFile("Builds")
              .getChildFile("GyeolSession");
    if (!dir.exists())
      dir.createDirectory();
  }

  return dir.getChildFile("autosave-session.json");
}

void MainComponent::restoreSession() {
  if (gyeolPage == nullptr)
    return;

  const auto file = sessionFilePath();
  if (!file.existsAsFile())
    return;

  const auto result = gyeolPage->document().loadFromFile(file);
  if (result.failed()) {
    DBG("[Gyeol] Session restore failed: " + result.getErrorMessage());
    return;
  }

  gyeolPage->refreshFromDocument();

  // TODO [Phase 1]: Teul 세션도 함께 복원
  //   auto teulFile = sessionFilePath().getSiblingFile("autosave-teul.json");
  //   teulPage->document().loadFromFile(teulFile);
}

void MainComponent::persistSession() const {
  if (gyeolPage == nullptr)
    return;

  const auto file = sessionFilePath();
  const auto parent = file.getParentDirectory();
  if (!parent.exists())
    parent.createDirectory();

  const auto result = gyeolPage->document().saveToFile(file);
  if (result.failed())
    DBG("[Gyeol] Session save failed: " + result.getErrorMessage());

  // TODO [Phase 1]: Teul 세션도 함께 저장
  //   auto teulFile = file.getSiblingFile("autosave-teul.json");
  //   teulPage->document().saveToFile(teulFile);
}
