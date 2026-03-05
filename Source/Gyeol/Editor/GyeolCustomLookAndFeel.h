#pragma once

#include <JuceHeader.h>

namespace Gyeol {
enum class GyeolThemeVariant { deepDark, light, highContrast, latte };

/**
 * @brief Central palette IDs for the Gyeol theme system.
 * 컴포넌트는 직접 색 값을 갖지 않고, 이 enum key를 통해 색상을 조회합니다.
 */
enum class GyeolPalette {
  // 1) Backgrounds: 배경 레이어 색상
  CanvasBackground,  // 전체 작업 캔버스 / main workspace background
  PanelBackground,   // 패널 기본 배경 / panel surface
  HeaderBackground,  // 헤더/타이틀 영역 / header surface
  OverlayBackground, // 팝업/모달 오버레이 / translucent overlay

  // 2) Borders & Dividers: 경계선/구분선
  BorderDefault, // 일반 상태 보더 / default border
  BorderActive,  // hover/focus 강조 보더 / active border

  // 3) Typography: 텍스트 명도 계열
  TextPrimary,   // 본문/주요 라벨 / primary text
  TextSecondary, // 보조 설명/메타 정보 / secondary text

  // 4) Accent & Brand: 포인트 컬러
  AccentPrimary, // 브랜드 포인트 / primary accent
  AccentHover,   // hover 시 강조 톤 / hover accent

  // 5) Semantic / Status: 상태 표현 색상
  ValidSuccess, // 성공/정상 상태 / success state
  ValidWarning, // 경고/주의 상태 / warning state
  ValidError,   // 에러/위험 상태 / error state

  // 6) Interactive Controls: 인터랙션 컨트롤 색상
  ControlBase,  // 기본 컨트롤 배경 / base control color
  ControlHover, // hover 컨트롤 배경 / hover control color
  ControlDown,  // pressed 컨트롤 배경 / down control color

  // 7) States: 비활성/선택 상태
  ControlDisabled,     // 비활성 컨트롤 배경 / disabled control
  TextDisabled,        // 비활성 텍스트 색상 / disabled text
  SelectionBackground, // 선택 영역 배경 / selection highlight

  // 8) View Specifics: 특정 뷰 전용 색상
  GridLine // 그리드/가이드 라인 색상 / grid guideline color
};

/**
 * @brief Resolve a palette id into a concrete JUCE colour.
 * 현재 활성 테마(deepDark/light/highContrast)를 기준으로 색상을 반환합니다.
 */
juce::Colour getGyeolColor(GyeolPalette colorId);
void setGyeolThemeVariant(GyeolThemeVariant variant);
GyeolThemeVariant getGyeolThemeVariant() noexcept;
void setGyeolReducedMotionEnabled(bool enabled);
bool isGyeolReducedMotionEnabled() noexcept;
float getGyeolMotionScale() noexcept;

/**
 * @brief Custom LookAndFeel for Gyeol Editor UI components.
 * 버튼, 슬라이더, 팝업 등 주요 위젯의 drawing rule을 통합 관리합니다.
 */
class GyeolCustomLookAndFeel : public juce::LookAndFeel_V4 {
public:
  GyeolCustomLookAndFeel();
  ~GyeolCustomLookAndFeel() override;
  void refreshFromTheme();

  // ==============================================================================
  // LookAndFeel_V4 overrides: JUCE 렌더링 훅
  // ==============================================================================

  // Toggle/TextButton custom draw: 토글/텍스트 버튼 렌더링
  void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

  // Slider custom draw: 슬라이더/값 바 렌더링
  void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float minSliderPos, float maxSliderPos,
                        const juce::Slider::SliderStyle style,
                        juce::Slider &slider) override;

  juce::Label *createSliderTextBox(juce::Slider &slider) override;

  // Scrollbar custom draw: 스크롤바 렌더링
  void drawScrollbar(juce::Graphics &g, juce::ScrollBar &scrollbar, int x,
                     int y, int width, int height, bool isScrollbarVertical,
                     int thumbStartPosition, int thumbSize, bool isMouseOver,
                     bool isMouseDown) override;

  // ComboBox custom draw: 콤보박스 렌더링
  void drawComboBox(juce::Graphics &g, int width, int height, bool isButtonDown,
                    int buttonX, int buttonY, int buttonW, int buttonH,
                    juce::ComboBox &box) override;

  juce::Font getLabelFont(juce::Label &label) override;
  juce::Font getTextButtonFont(juce::TextButton &button,
                               int buttonHeight) override;

  // ==============================================================================
  // Phase 3 polish: hover nuance, soft shadow, depth
  // ==============================================================================

  // Button background draw: hover glow + subtle rounded fill
  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

  // Popup menu background: glass feel + drop shadow
  void drawPopupMenuBackground(juce::Graphics &g, int width,
                               int height) override;

  // Popup menu border size: 라운드 코너 padding
  int getPopupMenuBorderSize() override;

  // TextEditor background: rounded surface fill
  void fillTextEditorBackground(juce::Graphics &g, int width, int height,
                                juce::TextEditor &editor) override;

  // TextEditor outline: focus state border treatment
  void drawTextEditorOutline(juce::Graphics &g, int width, int height,
                             juce::TextEditor &editor) override;

  // GroupComponent outline: header label + section frame
  void drawGroupComponentOutline(juce::Graphics &g, int width, int height,
                                 const juce::String &text,
                                 const juce::Justification &position,
                                 juce::GroupComponent &group) override;

  // ==============================================================================
  // Font helper: BinaryData typeface 기반 폰트 생성
  // ==============================================================================
  /** UI context에 맞는 Typeface를 선택해 Font를 반환합니다. */
  juce::Font makeFont(float height, bool bold = false) const;

  // ==============================================================================
  // Utility helper: 공용 soft shadow
  // ==============================================================================
  /** 지정 영역 주위에 부드러운 다층 그림자(soft drop shadow)를 그립니다. */
  static void drawSoftDropShadow(juce::Graphics &g,
                                 const juce::Rectangle<float> &area,
                                 float radius = 12.0f, float opacity = 0.35f);

private:
  // Nunito: Latin/UI default font
  juce::Typeface::Ptr nunitoRegular;
  juce::Typeface::Ptr nunitoBold;
  // NanumSquareRound: Korean/UI default font
  juce::Typeface::Ptr nanumRegular;
  juce::Typeface::Ptr nanumBold;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GyeolCustomLookAndFeel)
};
} // namespace Gyeol
