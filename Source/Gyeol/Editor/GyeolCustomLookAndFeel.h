#pragma once

#include <JuceHeader.h>

namespace Gyeol {
/**
 * @brief 다크 테마를 위한 코어 컬러 팔레트 정의
 * 모든 패널과 그리드, 위젯이 이 팔레트를 참조하여 통일감을 유지합니다.
 */
enum class GyeolPalette {
  // 1. Backgrounds (배경색 계층 - 숫자가 커질수록 사용자에게 가까워지거나
  // 밝아짐)
  CanvasBackground,  // 캔버스, 작업 공간 (가장 깊고 어두운 색)
  PanelBackground,   // 좌, 우 탭과 패널의 기본 배경색 (Canvas보다 살짝 밝음)
  HeaderBackground,  // 패널의 타이틀바(탑바) 또는 항목 헤더 영역
  OverlayBackground, // 글래스모피즘(투명도) 팝업, 네비게이터, 컨텍스트 메뉴 등
                     // 최상단 배경

  // 2. Borders & Dividers (경계선 및 디바이더)
  BorderDefault, // 탭 사이의 아주 미세하고 희미한 경계선
  BorderActive,  // 현재 선택된 객체나 활성 탭에 들어가는 강조된 테두리

  // 3. Typography (글씨 색상 명도)
  TextPrimary,   // 일반 본문, 하이라이트 텍스트 (명도 85% 이상)
  TextSecondary, // 부차적인 수치, 단위, 비활성 텍스트 (명도 50% 수준)

  // 4. Accent & Brand Colors (포인트 색상)
  AccentPrimary, // 메인 포인트 색 (푸른빛 - 슬라이더 바, 켜진 토글 버튼, 선택
                 // 박스)
  AccentHover,   // 마우스를 올렸을 때 살짝 더 밝아지는 강조색

  // 5. Semantic / Status (상태 표시)
  ValidSuccess, // 검증 통과 (녹색 계열)
  ValidWarning, // 경고, 누락된 에셋 (노란색/오렌지 계열)
  ValidError,   // 에러, 중요한 삭제 버튼 (붉은색 계열)

  // 6. Interactive Controls (버튼, 콤보박스, 입력폼 컨트롤)
  ControlBase,  // 컨트롤(버튼 등)의 평상시 배경
  ControlHover, // 컨트롤 마우스 호버 배경
  ControlDown,  // 컨트롤 클릭(Down) 시 배경

  // 7. States (특수 상태 - 비활성화, 선택됨)
  ControlDisabled,     // 조작 불가능한 컨트롤 배경
  TextDisabled,        // 조작 불가능한 텍스트 색상
  SelectionBackground, // 리스트나 트리에서 선택된 항목의 배경색 (Accent보다
                       // 낮은 명도)

  // 8. View Specifics (캔버스, 눈금자 등 특정 영역)
  GridLine // 캔버스 모눈종이, 스냅 라인 용 희미한 선 색상
};

/**
 * @brief 컬러 팔레트에서 실제 JUCE Colour 객체를 가져오는 파사드 헬퍼
 */
juce::Colour getGyeolColor(GyeolPalette colorId);

/**
 * @brief Gyeol Editor의 모든 컴포넌트 렌더링을 담당할 커스텀 LookAndFeel
 */
class GyeolCustomLookAndFeel : public juce::LookAndFeel_V4 {
public:
  GyeolCustomLookAndFeel();
  ~GyeolCustomLookAndFeel() override;

  // ==============================================================================
  // LookAndFeel_V4 오버라이드 함수들
  // ==============================================================================

  // 버튼(Toggle, Text Button 등) 커스텀
  void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

  // 슬라이더 / 스피너 커스텀
  void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float minSliderPos, float maxSliderPos,
                        const juce::Slider::SliderStyle style,
                        juce::Slider &slider) override;

  juce::Label *createSliderTextBox(juce::Slider &slider) override;

  // 스크롤바 커스텀
  void drawScrollbar(juce::Graphics &g, juce::ScrollBar &scrollbar, int x,
                     int y, int width, int height, bool isScrollbarVertical,
                     int thumbStartPosition, int thumbSize, bool isMouseOver,
                     bool isMouseDown) override;

  // 콤보박스 커스텀
  void drawComboBox(juce::Graphics &g, int width, int height, bool isButtonDown,
                    int buttonX, int buttonY, int buttonW, int buttonH,
                    juce::ComboBox &box) override;

  juce::Font getLabelFont(juce::Label &label) override;
  juce::Font getTextButtonFont(juce::TextButton &button,
                               int buttonHeight) override;

  // ==============================================================================
  // Phase 3: 마이크로 호버 트랜지션, 드롭 섀도우, 입체감
  // ==============================================================================

  // 텍스트 버튼 배경 (호버 Glow 및 부드러운 라운드 배경)
  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

  // 팝업 메뉴 배경 (글래스모피즘 + 드롭 섀도우)
  void drawPopupMenuBackground(juce::Graphics &g, int width,
                               int height) override;

  // 팝업 메뉴 보더 사이즈 (라운드 패딩)
  int getPopupMenuBorderSize() override;

  // 텍스트 에디터 배경 (다크 테마 + 라운드)
  void fillTextEditorBackground(juce::Graphics &g, int width, int height,
                                juce::TextEditor &editor) override;

  // 텍스트 에디터 아웃라인 (섬세한 보더)
  void drawTextEditorOutline(juce::Graphics &g, int width, int height,
                             juce::TextEditor &editor) override;

  // 그룹 컴포넌트 아웃라인 (섹션 헤더 스타일)
  void drawGroupComponentOutline(juce::Graphics &g, int width, int height,
                                 const juce::String &text,
                                 const juce::Justification &position,
                                 juce::GroupComponent &group) override;

  // ==============================================================================
  // 폰트 헬퍼 (BinaryData 기반 Typeface 로드 후 재사용)
  // ==============================================================================
  /** 현재 언어 설정에 따라 적절한 Typeface를 반환합니다. */
  juce::Font makeFont(float height, bool bold = false) const;

  // ==============================================================================
  // 유틸리티 헬퍼
  // ==============================================================================
  /** 주어진 영역 주위에 부드러운 드롭 섀도우를 그립니다. */
  static void drawSoftDropShadow(juce::Graphics &g,
                                 const juce::Rectangle<float> &area,
                                 float radius = 12.0f, float opacity = 0.35f);

private:
  // Nunito (라틴/영문 UI 기본 폰트)
  juce::Typeface::Ptr nunitoRegular;
  juce::Typeface::Ptr nunitoBold;
  // NanumSquareRound (한글 UI 기본 폰트)
  juce::Typeface::Ptr nanumRegular;
  juce::Typeface::Ptr nanumBold;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GyeolCustomLookAndFeel)
};
} // namespace Gyeol
