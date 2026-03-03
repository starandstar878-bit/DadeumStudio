#include "GyeolCustomLookAndFeel.h"

#include <BinaryData.h>

namespace Gyeol {
// ==============================================================================
// 팔레트 색상 매핑 코어 로직
// ==============================================================================
juce::Colour getGyeolColor(GyeolPalette colorId) {
  switch (colorId) {
  // 배경 / 도킹 패널 계층 (Deep Dark)
  case GyeolPalette::CanvasBackground:
    return juce::Colour::fromString("#FF181A1F"); // 가장 어둡고 광활한 캔버스
  case GyeolPalette::PanelBackground:
    return juce::Colour::fromString(
        "#FF21252B"); // 툴바, 탭, 속성창 등의 중간 톤
  case GyeolPalette::HeaderBackground:
    return juce::Colour::fromString("#FF1C1F26"); // 탭 제목이 올라가는 바
  case GyeolPalette::OverlayBackground:
    return juce::Colour::fromString(
        "#3321252B"); // 팝업 등 (알파 20% 글래스모피즘용)

  // 보더 / 테두리 계층 (Borderless를 지향하나 필수적인 희미한 구분을 위해 사용)
  case GyeolPalette::BorderDefault:
    return juce::Colour::fromString("#FF2C313C"); // 패널 사이 분할선
  case GyeolPalette::BorderActive:
    return juce::Colour::fromString(
        "#FF3E4451"); // 선택된 항목의 보더를 미세하게 살림

  // 텍스트 타이포그래피 (모던 산세리프에 어울리는 명도)
  case GyeolPalette::TextPrimary:
    return juce::Colour::fromString(
        "#FFE0E6ED"); // 거의 흰색에 가까운 핵심 정보
  case GyeolPalette::TextSecondary:
    return juce::Colour::fromString(
        "#FF8A92A3"); // 회색빛의 단위 문자, 보조 설명

  // 액센트 및 브랜드 (Soft & Clean의 핵심 포인트 블루)
  case GyeolPalette::AccentPrimary:
    return juce::Colour::fromString(
        "#FF5A9CFF"); // 주 스위치 표기, 슬라이더 채우기
  case GyeolPalette::AccentHover:
    return juce::Colour::fromString(
        "#FF7EB1FF"); // 마우스를 올리면 글로우(Glow)하는 밝은 푸른색

  // 검증 상태 (Semantic Status)
  case GyeolPalette::ValidSuccess:
    return juce::Colour::fromString("#FF98C379"); // 부드러운 그린
  case GyeolPalette::ValidWarning:
    return juce::Colour::fromString("#FFE5C07B"); // 부드러운 옐로우/오렌지
  case GyeolPalette::ValidError:
    return juce::Colour::fromString(
        "#FFE06C75"); // 부드러운 레드 (삭제 버튼 겸용)

  // 렌더링 컨트롤 객체 (버튼, 콤보박스 네모박스)
  case GyeolPalette::ControlBase:
    return juce::Colour::fromString(
        "#FF282C34"); // 패널(21252b)보다 살짝 튀어나온 색
  case GyeolPalette::ControlHover:
    return juce::Colour::fromString("#FF30353F"); // 호버시 살짝 밝아짐
  case GyeolPalette::ControlDown:
    return juce::Colour::fromString("#FF1E2227"); // 클릭시 들어가는 음영

  // 특수 상태 (비활성화 및 선택)
  case GyeolPalette::ControlDisabled:
    return juce::Colour::fromString("#FF2C313A"); // 조작 불가 박스
  case GyeolPalette::TextDisabled:
    return juce::Colour::fromString("#FF5C6370"); // 흐릿한 비활성 글씨
  case GyeolPalette::SelectionBackground:
    return juce::Colour::fromString(
        "#FF2C313A"); // 트리 항목 등 선택시 어두운 강조색

  // 캔버스 모눈종이, 보조 선 등
  case GyeolPalette::GridLine:
    return juce::Colour::fromString(
        "#FF2B2E36"); // 눈에 띄지 않는 아주 희미한 선맥

  default:
    return juce::Colours::transparentBlack;
  }
}

// ==============================================================================
// GyeolCustomLookAndFeel 생성자 및 초기화
// ==============================================================================
GyeolCustomLookAndFeel::GyeolCustomLookAndFeel() {
  // ============================================================================
  // 1. BinaryData에서 번들 폰트를 로드하여 Typeface 캐시에 저장
  //    Nunito  → 영문/라틴 UI 기본 폰트 (Clean Rounded Sans-Serif)
  //    NanumSquareRound → 한글 UI 기본 폰트 (모던 둥근 고딕)
  // ============================================================================
  nunitoRegular = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NunitoRegular_ttf, BinaryData::NunitoRegular_ttfSize);
  nunitoBold = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NunitoBold_ttf, BinaryData::NunitoBold_ttfSize);
  nanumRegular = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NanumSquareRoundR_ttf, BinaryData::NanumSquareRoundR_ttfSize);
  nanumBold = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NanumSquareRoundB_ttf, BinaryData::NanumSquareRoundB_ttfSize);

  // 글로벌 기본 Typeface를 Nunito Regular로 지정
  // (JUCE가 폰트 이름 없이 기본 산세리프를 그릴 때 이 Typeface를 사용)
  if (nunitoRegular != nullptr)
    setDefaultSansSerifTypeface(nunitoRegular);

  // ============================================================================
  // 2. JUCE 기본 컴포넌트의 컬러 체계를 Gyeol 팔레트 색상으로 강제 덮어쓰기
  // ============================================================================

  // 창 및 배경
  setColour(juce::ResizableWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::CanvasBackground));

  // 텍스트 색상
  setColour(juce::Label::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));

  // 슬라이더 색상
  setColour(juce::Slider::thumbColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::Slider::trackColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::Slider::backgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));

  // 버튼 색상
  setColour(juce::TextButton::buttonColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::TextButton::buttonOnColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::TextButton::textColourOffId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::TextButton::textColourOnId, juce::Colours::white);

  // 팝업 메뉴 및 화살표, 콤보박스 색상
  setColour(juce::PopupMenu::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::PopupMenu::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::PopupMenu::highlightedBackgroundColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
  setColour(juce::ComboBox::backgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::ComboBox::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::ComboBox::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));

  // 스크롤바 커스텀 연동 (둥근 모서리에 어울리도록 얇고 어둡게 유지)
  setColour(juce::ScrollBar::thumbColourId,
            getGyeolColor(GyeolPalette::BorderActive));
  setColour(juce::ScrollBar::backgroundColourId,
            juce::Colours::transparentBlack);

  // 트리 뷰 컬러
  setColour(juce::TreeView::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::TreeView::selectedItemBackgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));

  // Phase 3: 텍스트 에디터 컬러
  setColour(juce::TextEditor::backgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::TextEditor::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::TextEditor::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));
  setColour(juce::TextEditor::focusedOutlineColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::TextEditor::highlightColourId,
            getGyeolColor(GyeolPalette::AccentPrimary).withAlpha(0.35f));
  setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
  setColour(juce::CaretComponent::caretColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));

  // Phase 3: 리스트박스
  setColour(juce::ListBox::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::ListBox::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

  // Phase 3: 툴팁 (다크 + 라운드)
  setColour(juce::TooltipWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::HeaderBackground));
  setColour(juce::TooltipWindow::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::TooltipWindow::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));

  // Phase 3: 알림(AlertWindow) 다크
  setColour(juce::AlertWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::AlertWindow::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::AlertWindow::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));
}

GyeolCustomLookAndFeel::~GyeolCustomLookAndFeel() {}

// ==============================================================================
// 커스텀 렌더링 오버라이드 구현
// ==============================================================================

void GyeolCustomLookAndFeel::drawToggleButton(
    juce::Graphics &g, juce::ToggleButton &button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
  float fontSize = 12.0f;
  float tickWidth = 32.0f;
  float tickHeight = 18.0f;

  auto bounds = button.getLocalBounds().toFloat();
  juce::Rectangle<float> pillBounds(
      0.0f, (bounds.getHeight() - tickHeight) * 0.5f, tickWidth, tickHeight);

  bool isToggled = button.getToggleState();
  auto cornerSize = tickHeight * 0.5f;

  // Background
  juce::Colour bgColour = getGyeolColor(isToggled ? GyeolPalette::AccentPrimary
                                                  : GyeolPalette::ControlBase);
  if (!button.isEnabled())
    bgColour = bgColour.withAlpha(0.5f);

  if (shouldDrawButtonAsHighlighted && !isToggled)
    bgColour = getGyeolColor(GyeolPalette::ControlHover);

  g.setColour(bgColour);
  g.fillRoundedRectangle(pillBounds, cornerSize);

  if (!isToggled) {
    g.setColour(getGyeolColor(GyeolPalette::BorderDefault));
    g.drawRoundedRectangle(pillBounds, cornerSize, 1.0f);
  }

  // Thumb
  float thumbInnerMargin = 2.0f;
  float thumbSize = tickHeight - (thumbInnerMargin * 2.0f);
  float thumbX = isToggled
                     ? (pillBounds.getRight() - thumbSize - thumbInnerMargin)
                     : (pillBounds.getX() + thumbInnerMargin);

  juce::Rectangle<float> thumbBounds(
      thumbX, pillBounds.getY() + thumbInnerMargin, thumbSize, thumbSize);

  // Drop Shadow on Thumb
  g.setColour(juce::Colours::black.withAlpha(0.2f));
  g.fillEllipse(thumbBounds.translated(0.0f, 1.0f));

  g.setColour(getGyeolColor(GyeolPalette::TextPrimary));
  if (!button.isEnabled())
    g.setColour(getGyeolColor(GyeolPalette::TextDisabled));

  g.fillEllipse(thumbBounds);

  // Draw text if any
  if (button.getButtonText().isNotEmpty()) {
    g.setColour(getGyeolColor(button.isEnabled() ? GyeolPalette::TextPrimary
                                                 : GyeolPalette::TextDisabled));
    g.setFont(makeFont(fontSize));
    juce::Rectangle<int> textBounds(
        static_cast<int>(pillBounds.getRight() + 8.0f), 0,
        button.getWidth() - static_cast<int>(pillBounds.getRight() + 8.0f),
        button.getHeight());
    g.drawFittedText(button.getButtonText(), textBounds,
                     juce::Justification::centredLeft, 1);
  }
}

void GyeolCustomLookAndFeel::drawLinearSlider(
    juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
    float minSliderPos, float maxSliderPos,
    const juce::Slider::SliderStyle style, juce::Slider &slider) {
  if (style == juce::Slider::LinearBar ||
      style == juce::Slider::LinearBarVertical) {
    juce::Rectangle<float> bounds((float)x, (float)y, (float)width,
                                  (float)height);

    auto bgCol = getGyeolColor(GyeolPalette::ControlBase);
    auto fillCol = getGyeolColor(GyeolPalette::AccentPrimary);

    if (!slider.isEnabled()) {
      bgCol = bgCol.withAlpha(0.5f);
      fillCol = fillCol.withAlpha(0.5f);
    }

    // Base Box
    g.setColour(bgCol);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Fill bar
    if (sliderPos > minSliderPos) {
      juce::Rectangle<float> fillBounds(bounds.getX(), bounds.getY(),
                                        sliderPos - minSliderPos,
                                        bounds.getHeight());
      g.setColour(
          fillCol.withAlpha(0.25f)); // very subtle fill inside the textbox
      g.fillRoundedRectangle(fillBounds, 4.0f);
    }
  } else {
    // Default linear sliders can just fall back to standard style or we could
    // thin them out
    juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                           minSliderPos, maxSliderPos, style,
                                           slider);
  }
}

juce::Label *GyeolCustomLookAndFeel::createSliderTextBox(juce::Slider &slider) {
  juce::Label *l = juce::LookAndFeel_V4::createSliderTextBox(slider);

  // Transparent because the custom slider draw handles the background box!
  l->setColour(juce::Label::backgroundColourId,
               juce::Colours::transparentBlack);
  l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
  l->setColour(juce::Label::textColourId,
               getGyeolColor(GyeolPalette::TextPrimary));

  l->setFont(makeFont(13.0f));

  return l;
}

void GyeolCustomLookAndFeel::drawScrollbar(
    juce::Graphics &g, juce::ScrollBar &scrollbar, int x, int y, int width,
    int height, bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
    bool isMouseOver, bool isMouseDown) {
  juce::ignoreUnused(scrollbar);

  juce::Rectangle<int> thumbBounds;
  if (isScrollbarVertical) {
    thumbBounds = juce::Rectangle<int>(x, thumbStartPosition, width, thumbSize);
  } else {
    thumbBounds =
        juce::Rectangle<int>(thumbStartPosition, y, thumbSize, height);
  }

  auto thumbCol = getGyeolColor(GyeolPalette::BorderActive);
  if (isMouseOver || isMouseDown)
    thumbCol = thumbCol.brighter(0.2f);
  else
    thumbCol = thumbCol.withAlpha(0.6f);

  g.setColour(thumbCol);

  if (isScrollbarVertical) {
    g.fillRoundedRectangle(thumbBounds.reduced(3, 1).toFloat(), 2.5f);
  } else {
    g.fillRoundedRectangle(thumbBounds.reduced(1, 3).toFloat(), 2.5f);
  }
}

void GyeolCustomLookAndFeel::drawComboBox(juce::Graphics &g, int width,
                                          int height, bool isButtonDown,
                                          int buttonX, int buttonY, int buttonW,
                                          int buttonH, juce::ComboBox &box) {
  juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);
  juce::Rectangle<int> boxBounds(0, 0, width, height);
  auto cornerSize = 4.0f;

  g.setColour(getGyeolColor(isButtonDown ? GyeolPalette::ControlDown
                                         : (box.isMouseOver()
                                                ? GyeolPalette::ControlHover
                                                : GyeolPalette::ControlBase)));
  g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);

  g.setColour(getGyeolColor(GyeolPalette::BorderDefault));
  g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f), cornerSize, 1.0f);

  juce::Rectangle<int> arrowZone(width - 24, 0, 24, height);
  juce::Path path;
  path.startNewSubPath((float)arrowZone.getX() + 7.0f,
                       (float)arrowZone.getCentreY() - 2.0f);
  path.lineTo((float)arrowZone.getCentreX(),
              (float)arrowZone.getCentreY() + 3.0f);
  path.lineTo((float)arrowZone.getRight() - 7.0f,
              (float)arrowZone.getCentreY() - 2.0f);

  g.setColour(getGyeolColor(GyeolPalette::TextSecondary));
  g.strokePath(path, juce::PathStrokeType(1.5f));
}

juce::Font GyeolCustomLookAndFeel::getLabelFont(juce::Label &label) {
  bool isBold = (label.getFont().getStyleFlags() & juce::Font::bold) != 0;
  return makeFont(label.getFont().getHeight(), isBold);
}

juce::Font GyeolCustomLookAndFeel::getTextButtonFont(juce::TextButton &button,
                                                     int buttonHeight) {
  juce::ignoreUnused(button);
  return makeFont(juce::jmin(13.0f, buttonHeight * 0.6f), /*bold=*/true);
}

// ==============================================================================
// makeFont: BinaryData Typeface 기반 Font 생성 헬퍼
// ==============================================================================
juce::Font GyeolCustomLookAndFeel::makeFont(float height, bool bold) const {
  // NanumSquareRound는 한글 문자를 담당하지만 JUCE 단일 Typeface 구조상
  // fallback 체인을 직접 지원하지 않으므로 Nunito를 기본으로 사용합니다.
  // 향후 다국어 확장 시 SystemFont fallback 처리를 여기서 분기할 수 있습니다.
  juce::Typeface::Ptr tf = bold ? nunitoBold : nunitoRegular;
  if (tf == nullptr)
    return juce::Font(juce::FontOptions(height)); // 로드 실패 시 시스템 기본
  return juce::Font(tf).withHeight(height);
}

// ==============================================================================
// Phase 3: 마이크로 호버 트랜지션, 드롭 섀도우, 입체감
// ==============================================================================

void GyeolCustomLookAndFeel::drawButtonBackground(
    juce::Graphics &g, juce::Button &button,
    const juce::Colour &backgroundColour, bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown) {
  const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
  const auto cornerRadius = 6.0f;

  juce::Colour bg = backgroundColour;

  if (!button.isEnabled()) {
    bg = getGyeolColor(GyeolPalette::ControlDisabled);
  } else if (shouldDrawButtonAsDown) {
    bg = getGyeolColor(GyeolPalette::ControlDown);
  } else if (shouldDrawButtonAsHighlighted) {
    bg = getGyeolColor(GyeolPalette::ControlHover);
  }

  // 부드러운 배경
  g.setColour(bg);
  g.fillRoundedRectangle(bounds, cornerRadius);

  // 호버 시 미세한 Glow 테두리 (AccentPrimary의 저투명도 외곽선)
  if (shouldDrawButtonAsHighlighted && button.isEnabled() &&
      !shouldDrawButtonAsDown) {
    const auto glowColour =
        getGyeolColor(GyeolPalette::AccentPrimary).withAlpha(0.25f);
    g.setColour(glowColour);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.5f);
  } else if (!shouldDrawButtonAsDown) {
    // 일반 상태의 아주 미세한 보더
    g.setColour(getGyeolColor(GyeolPalette::BorderDefault));
    g.drawRoundedRectangle(bounds, cornerRadius, 0.8f);
  }
}

void GyeolCustomLookAndFeel::drawPopupMenuBackground(juce::Graphics &g,
                                                     int width, int height) {
  const auto bounds =
      juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
  const auto cornerRadius = 8.0f;

  // 1. 드롭 섀도우 (옅고 넓게 퍼지는 그림자)
  drawSoftDropShadow(g, bounds, 16.0f, 0.4f);

  // 2. 반투명 배경 (약간의 글래스모피즘 효과)
  const auto bgColour =
      getGyeolColor(GyeolPalette::PanelBackground).withAlpha(0.95f);
  g.setColour(bgColour);
  g.fillRoundedRectangle(bounds.reduced(1.0f), cornerRadius);

  // 3. 미세한 밝은 보더로 팝업 경계 강조
  g.setColour(getGyeolColor(GyeolPalette::BorderDefault).withAlpha(0.7f));
  g.drawRoundedRectangle(bounds.reduced(1.0f), cornerRadius, 1.0f);
}

int GyeolCustomLookAndFeel::getPopupMenuBorderSize() {
  return 4; // 라운드 코너를 위한 내부 여백
}

void GyeolCustomLookAndFeel::fillTextEditorBackground(
    juce::Graphics &g, int width, int height, juce::TextEditor &editor) {
  juce::ignoreUnused(editor);
  const auto bounds =
      juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
  g.setColour(getGyeolColor(GyeolPalette::ControlBase));
  g.fillRoundedRectangle(bounds, 4.0f);
}

void GyeolCustomLookAndFeel::drawTextEditorOutline(juce::Graphics &g, int width,
                                                   int height,
                                                   juce::TextEditor &editor) {
  const auto bounds =
      juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);

  if (editor.hasKeyboardFocus(true)) {
    // 포커스 상태: AccentPrimary 보더 + 미세한 Glow
    const auto accentCol = getGyeolColor(GyeolPalette::AccentPrimary);
    g.setColour(accentCol.withAlpha(0.2f));
    g.drawRoundedRectangle(bounds.expanded(1.0f), 5.0f, 2.0f);
    g.setColour(accentCol.withAlpha(0.7f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
  } else {
    // 비포커스 상태: 미세한 보더
    g.setColour(getGyeolColor(GyeolPalette::BorderDefault));
    g.drawRoundedRectangle(bounds, 4.0f, 0.8f);
  }
}

void GyeolCustomLookAndFeel::drawGroupComponentOutline(
    juce::Graphics &g, int width, int height, const juce::String &text,
    const juce::Justification &position, juce::GroupComponent &group) {
  juce::ignoreUnused(position, group);
  const auto textHeight = 16.0f;
  const auto bounds = juce::Rectangle<float>(
      0.0f, textHeight * 0.5f, (float)width, (float)height - textHeight * 0.5f);

  // 배경
  g.setColour(getGyeolColor(GyeolPalette::HeaderBackground).withAlpha(0.5f));
  g.fillRoundedRectangle(bounds, 6.0f);

  // 미세한 보더
  g.setColour(getGyeolColor(GyeolPalette::BorderDefault).withAlpha(0.5f));
  g.drawRoundedRectangle(bounds, 6.0f, 0.6f);

  // 헤더 텍스트
  if (text.isNotEmpty()) {
    g.setColour(getGyeolColor(GyeolPalette::TextSecondary));
    g.setFont(makeFont(12.0f, true));
    g.drawText(
        text,
        juce::Rectangle<int>(8, 0, width - 16, juce::roundToInt(textHeight)),
        juce::Justification::centredLeft, true);
  }
}

// ==============================================================================
// drawSoftDropShadow: 범용 소프트 드롭 섀도우 유틸리티
// ==============================================================================
void GyeolCustomLookAndFeel::drawSoftDropShadow(
    juce::Graphics &g, const juce::Rectangle<float> &area, float radius,
    float opacity) {
  // 여러 겹의 반투명 검은색 라운드 사각형으로 소프트 섀도우 근사
  const int numLayers = 5;
  for (int i = numLayers; i >= 1; --i) {
    const float expand = radius * (float)i / (float)numLayers;
    const float alpha =
        opacity * (1.0f - (float)i / ((float)numLayers + 1.0f)) * 0.3f;
    g.setColour(juce::Colours::black.withAlpha(alpha));
    g.fillRoundedRectangle(area.expanded(expand), 8.0f + expand * 0.5f);
  }
}

} // namespace Gyeol
