#pragma once

#include <JuceHeader.h>

namespace Gyeol {
enum class GyeolThemeVariant {
  deepDark,
  light,
  highContrast
};
/**
 * @brief ?ㅽ겕 ?뚮쭏瑜??꾪븳 肄붿뼱 而щ윭 ?붾젅???뺤쓽
 * 紐⑤뱺 ?⑤꼸怨?洹몃━?? ?꾩젽?????붾젅?몃? 李몄“?섏뿬 ?듭씪媛먯쓣 ?좎??⑸땲??
 */
enum class GyeolPalette {
  // 1. Backgrounds (諛곌꼍??怨꾩링 - ?レ옄媛 而ㅼ쭏?섎줉 ?ъ슜?먯뿉寃?媛源뚯썙吏嫄곕굹
  // 諛앹븘吏?
  CanvasBackground,  // 罹붾쾭?? ?묒뾽 怨듦컙 (媛??源딄퀬 ?대몢????
  PanelBackground,   // 醫? ????낵 ?⑤꼸??湲곕낯 諛곌꼍??(Canvas蹂대떎 ?댁쭩 諛앹쓬)
  HeaderBackground,  // ?⑤꼸????댄?諛??묐컮) ?먮뒗 ??ぉ ?ㅻ뜑 ?곸뿭
  OverlayBackground, // 湲?섏뒪紐⑦뵾利??щ챸?? ?앹뾽, ?ㅻ퉬寃뚯씠?? 而⑦뀓?ㅽ듃 硫붾돱 ??
                     // 理쒖긽??諛곌꼍

  // 2. Borders & Dividers (寃쎄퀎??諛??붾컮?대뜑)
  BorderDefault, // ???ъ씠???꾩＜ 誘몄꽭?섍퀬 ?щ???寃쎄퀎??
  BorderActive,  // ?꾩옱 ?좏깮??媛앹껜???쒖꽦 ??뿉 ?ㅼ뼱媛??媛뺤“???뚮몢由?

  // 3. Typography (湲???됱긽 紐낅룄)
  TextPrimary,   // ?쇰컲 蹂몃Ц, ?섏씠?쇱씠???띿뒪??(紐낅룄 85% ?댁긽)
  TextSecondary, // 遺李⑥쟻???섏튂, ?⑥쐞, 鍮꾪솢???띿뒪??(紐낅룄 50% ?섏?)

  // 4. Accent & Brand Colors (?ъ씤???됱긽)
  AccentPrimary, // 硫붿씤 ?ъ씤????(?몃Ⅸ鍮?- ?щ씪?대뜑 諛? 耳쒖쭊 ?좉? 踰꾪듉, ?좏깮
                 // 諛뺤뒪)
  AccentHover,   // 留덉슦?ㅻ? ?щ졇?????댁쭩 ??諛앹븘吏??媛뺤“??

  // 5. Semantic / Status (?곹깭 ?쒖떆)
  ValidSuccess, // 寃利??듦낵 (?뱀깋 怨꾩뿴)
  ValidWarning, // 寃쎄퀬, ?꾨씫???먯뀑 (?몃????ㅻ젋吏 怨꾩뿴)
  ValidError,   // ?먮윭, 以묒슂????젣 踰꾪듉 (遺됱???怨꾩뿴)

  // 6. Interactive Controls (踰꾪듉, 肄ㅻ낫諛뺤뒪, ?낅젰??而⑦듃濡?
  ControlBase,  // 而⑦듃濡?踰꾪듉 ?????됱긽??諛곌꼍
  ControlHover, // 而⑦듃濡?留덉슦???몃쾭 諛곌꼍
  ControlDown,  // 而⑦듃濡??대┃(Down) ??諛곌꼍

  // 7. States (?뱀닔 ?곹깭 - 鍮꾪솢?깊솕, ?좏깮??
  ControlDisabled,     // 議곗옉 遺덇??ν븳 而⑦듃濡?諛곌꼍
  TextDisabled,        // 議곗옉 遺덇??ν븳 ?띿뒪???됱긽
  SelectionBackground, // 由ъ뒪?몃굹 ?몃━?먯꽌 ?좏깮????ぉ??諛곌꼍??(Accent蹂대떎
                       // ??? 紐낅룄)

  // 8. View Specifics (罹붾쾭?? ?덇툑?????뱀젙 ?곸뿭)
  GridLine // 罹붾쾭??紐⑤늿醫낆씠, ?ㅻ깄 ?쇱씤 ???щ??????됱긽
};

/**
 * @brief 而щ윭 ?붾젅?몄뿉???ㅼ젣 JUCE Colour 媛앹껜瑜?媛?몄삤???뚯궗???ы띁
 */
juce::Colour getGyeolColor(GyeolPalette colorId);
void setGyeolThemeVariant(GyeolThemeVariant variant);
GyeolThemeVariant getGyeolThemeVariant() noexcept;
void setGyeolReducedMotionEnabled(bool enabled);
bool isGyeolReducedMotionEnabled() noexcept;
float getGyeolMotionScale() noexcept;

/**
 * @brief Gyeol Editor??紐⑤뱺 而댄룷?뚰듃 ?뚮뜑留곸쓣 ?대떦??而ㅼ뒪? LookAndFeel
 */
class GyeolCustomLookAndFeel : public juce::LookAndFeel_V4 {
public:
  GyeolCustomLookAndFeel();
  ~GyeolCustomLookAndFeel() override;
  void refreshFromTheme();

  // ==============================================================================
  // LookAndFeel_V4 ?ㅻ쾭?쇱씠???⑥닔??
  // ==============================================================================

  // 踰꾪듉(Toggle, Text Button ?? 而ㅼ뒪?
  void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

  // ?щ씪?대뜑 / ?ㅽ뵾??而ㅼ뒪?
  void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float minSliderPos, float maxSliderPos,
                        const juce::Slider::SliderStyle style,
                        juce::Slider &slider) override;

  juce::Label *createSliderTextBox(juce::Slider &slider) override;

  // ?ㅽ겕濡ㅻ컮 而ㅼ뒪?
  void drawScrollbar(juce::Graphics &g, juce::ScrollBar &scrollbar, int x,
                     int y, int width, int height, bool isScrollbarVertical,
                     int thumbStartPosition, int thumbSize, bool isMouseOver,
                     bool isMouseDown) override;

  // 肄ㅻ낫諛뺤뒪 而ㅼ뒪?
  void drawComboBox(juce::Graphics &g, int width, int height, bool isButtonDown,
                    int buttonX, int buttonY, int buttonW, int buttonH,
                    juce::ComboBox &box) override;

  juce::Font getLabelFont(juce::Label &label) override;
  juce::Font getTextButtonFont(juce::TextButton &button,
                               int buttonHeight) override;

  // ==============================================================================
  // Phase 3: 留덉씠?щ줈 ?몃쾭 ?몃옖吏?? ?쒕∼ ??꾩슦, ?낆껜媛?
  // ==============================================================================

  // ?띿뒪??踰꾪듉 諛곌꼍 (?몃쾭 Glow 諛?遺?쒕윭???쇱슫??諛곌꼍)
  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

  // ?앹뾽 硫붾돱 諛곌꼍 (湲?섏뒪紐⑦뵾利?+ ?쒕∼ ??꾩슦)
  void drawPopupMenuBackground(juce::Graphics &g, int width,
                               int height) override;

  // ?앹뾽 硫붾돱 蹂대뜑 ?ъ씠利?(?쇱슫???⑤뵫)
  int getPopupMenuBorderSize() override;

  // ?띿뒪???먮뵒??諛곌꼍 (?ㅽ겕 ?뚮쭏 + ?쇱슫??
  void fillTextEditorBackground(juce::Graphics &g, int width, int height,
                                juce::TextEditor &editor) override;

  // ?띿뒪???먮뵒???꾩썐?쇱씤 (?ъ꽭??蹂대뜑)
  void drawTextEditorOutline(juce::Graphics &g, int width, int height,
                             juce::TextEditor &editor) override;

  // 洹몃９ 而댄룷?뚰듃 ?꾩썐?쇱씤 (?뱀뀡 ?ㅻ뜑 ?ㅽ???
  void drawGroupComponentOutline(juce::Graphics &g, int width, int height,
                                 const juce::String &text,
                                 const juce::Justification &position,
                                 juce::GroupComponent &group) override;

  // ==============================================================================
  // ?고듃 ?ы띁 (BinaryData 湲곕컲 Typeface 濡쒕뱶 ???ъ궗??
  // ==============================================================================
  /** ?꾩옱 ?몄뼱 ?ㅼ젙???곕씪 ?곸젅??Typeface瑜?諛섑솚?⑸땲?? */
  juce::Font makeFont(float height, bool bold = false) const;

  // ==============================================================================
  // ?좏떥由ы떚 ?ы띁
  // ==============================================================================
  /** 二쇱뼱吏??곸뿭 二쇱쐞??遺?쒕윭???쒕∼ ??꾩슦瑜?洹몃┰?덈떎. */
  static void drawSoftDropShadow(juce::Graphics &g,
                                 const juce::Rectangle<float> &area,
                                 float radius = 12.0f, float opacity = 0.35f);

private:
  // Nunito (?쇳떞/?곷Ц UI 湲곕낯 ?고듃)
  juce::Typeface::Ptr nunitoRegular;
  juce::Typeface::Ptr nunitoBold;
  // NanumSquareRound (?쒓? UI 湲곕낯 ?고듃)
  juce::Typeface::Ptr nanumRegular;
  juce::Typeface::Ptr nanumBold;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GyeolCustomLookAndFeel)
};
} // namespace Gyeol

