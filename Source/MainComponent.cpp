// BOM
#include "MainComponent.h"

// =============================================================================
//  MainComponent
// =============================================================================
MainComponent::MainComponent(AppServices &services) : appServices(services) {
  // ------------------------------------------------------------------
  //  ??륁뵠筌왖 ??밴쉐 (addChildComponent ??筌ｌ꼷?????ｊ볼????됱벉)
  // ------------------------------------------------------------------
  gyeolPage = Gyeol::createEditor();
  teulPage = Teul::createEditor(&appServices.audioDeviceManager);

  addChildComponent(*gyeolPage);
  addChildComponent(*teulPage);

  // ------------------------------------------------------------------
  //  ??而???밴쉐 獄??꾩뮆媛??怨뚭퍙
  // ------------------------------------------------------------------
  pageTabBar = std::make_unique<AppPageTabBar>(currentPage);
  addAndMakeVisible(*pageTabBar);

  pageTabBar->onPageSelected = [this](AppPage page) { switchToPage(page); };

  // ------------------------------------------------------------------
  //  ?λ뜃由???륁뵠筌왖 ??뽮쉐??
  // ------------------------------------------------------------------
  switchToPage(currentPage);

  // ------------------------------------------------------------------
  //  ?紐꾨?癰귣벊??(Gyeol ?얜챷苑?
  //  TODO [Phase 1]: Teul 域밸챶????얜챷苑????ｍ뜞 癰귣벊??
  // ------------------------------------------------------------------
  restoreSession();

  setSize(600, 400);
}

MainComponent::~MainComponent() { persistSession(); }

// =============================================================================
//  ??륁뵠筌왖 ?袁れ넎
// =============================================================================
void MainComponent::switchToPage(AppPage page) {
  currentPage = page;

  //  ??ｋ┛疫?/ 癰귣똻?졿묾怨뺤춸 ??뺣뼄 ???紐꾨뮞??곷뮞????? ???댘??? ??놁벉.
  //  ??Teul ??TGraphRuntime ?? ??ｊ볼?紐껊즲 ??삳탵????살쟿??뽯퓠???④쑴????덉삂??뺣뼄.
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

  // TODO [UI]: GlobalToolbar 揶쎛 ??룸┛筌??袁⑥삋 雅뚯눘苑???곸젫
  // if (globalToolbar != nullptr)
  //     globalToolbar->setBounds (bounds.removeFromTop (42));

  // ??롫뼊 ??而?
  if (pageTabBar != nullptr)
    pageTabBar->setBounds(bounds.removeFromBottom(40));

  // ??롢돢筌왖 = ??륁뵠筌왖 ?怨몃열 (筌뤴뫀諭???륁뵠筌왖 ??덉뵬 ??由경에???쇱젟)
  if (gyeolPage != nullptr)
    gyeolPage->setBounds(bounds);
  if (teulPage != nullptr)
    teulPage->setBounds(bounds);
}

// =============================================================================
//  ?紐꾨?????/ 癰귣벊??
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

  // TODO [Phase 1]: Teul ?紐꾨????ｍ뜞 癰귣벊??
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

  // TODO [Phase 1]: Teul ?紐꾨????ｍ뜞 ????
  //   auto teulFile = file.getSiblingFile("autosave-teul.json");
  //   teulPage->document().saveToFile(teulFile);
}