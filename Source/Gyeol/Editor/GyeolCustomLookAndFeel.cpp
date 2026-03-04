#include "GyeolCustomLookAndFeel.h"

#include <BinaryData.h>

namespace {
Gyeol::GyeolThemeVariant gActiveThemeVariant = Gyeol::GyeolThemeVariant::deepDark;
bool gReducedMotionEnabled = false;
juce::Colour deepDarkColour(Gyeol::GyeolPalette colorId) {
  using Gyeol::GyeolPalette;
  switch (colorId) {
  case GyeolPalette::CanvasBackground:
    return juce::Colour::fromString("#FF181A1F");
  case GyeolPalette::PanelBackground:
    return juce::Colour::fromString("#FF21252B");
  case GyeolPalette::HeaderBackground:
    return juce::Colour::fromString("#FF1C1F26");
  case GyeolPalette::OverlayBackground:
    return juce::Colour::fromString("#3321252B");
  case GyeolPalette::BorderDefault:
    return juce::Colour::fromString("#FF2C313C");
  case GyeolPalette::BorderActive:
    return juce::Colour::fromString("#FF3E4451");
  case GyeolPalette::TextPrimary:
    return juce::Colour::fromString("#FFE0E6ED");
  case GyeolPalette::TextSecondary:
    return juce::Colour::fromString("#FF8A92A3");
  case GyeolPalette::AccentPrimary:
    return juce::Colour::fromString("#FF5A9CFF");
  case GyeolPalette::AccentHover:
    return juce::Colour::fromString("#FF7EB1FF");
  case GyeolPalette::ValidSuccess:
    return juce::Colour::fromString("#FF98C379");
  case GyeolPalette::ValidWarning:
    return juce::Colour::fromString("#FFE5C07B");
  case GyeolPalette::ValidError:
    return juce::Colour::fromString("#FFE06C75");
  case GyeolPalette::ControlBase:
    return juce::Colour::fromString("#FF282C34");
  case GyeolPalette::ControlHover:
    return juce::Colour::fromString("#FF30353F");
  case GyeolPalette::ControlDown:
    return juce::Colour::fromString("#FF1E2227");
  case GyeolPalette::ControlDisabled:
    return juce::Colour::fromString("#FF2C313A");
  case GyeolPalette::TextDisabled:
    return juce::Colour::fromString("#FF5C6370");
  case GyeolPalette::SelectionBackground:
    return juce::Colour::fromString("#FF2C313A");
  case GyeolPalette::GridLine:
    return juce::Colour::fromString("#FF2B2E36");
  default:
    return juce::Colours::transparentBlack;
  }
}
juce::Colour lightColour(Gyeol::GyeolPalette colorId) {
  using Gyeol::GyeolPalette;
  switch (colorId) {
  case GyeolPalette::CanvasBackground:
    return juce::Colour::fromString("#FFF4F7FB");
  case GyeolPalette::PanelBackground:
    return juce::Colour::fromString("#FFFFFFFF");
  case GyeolPalette::HeaderBackground:
    return juce::Colour::fromString("#FFE9EEF5");
  case GyeolPalette::OverlayBackground:
    return juce::Colour::fromString("#E8FFFFFF");
  case GyeolPalette::BorderDefault:
    return juce::Colour::fromString("#FFD2D8E2");
  case GyeolPalette::BorderActive:
    return juce::Colour::fromString("#FF9EA8B9");
  case GyeolPalette::TextPrimary:
    return juce::Colour::fromString("#FF1E2A37");
  case GyeolPalette::TextSecondary:
    return juce::Colour::fromString("#FF526173");
  case GyeolPalette::AccentPrimary:
    return juce::Colour::fromString("#FF1F6FEB");
  case GyeolPalette::AccentHover:
    return juce::Colour::fromString("#FF3C82F0");
  case GyeolPalette::ValidSuccess:
    return juce::Colour::fromString("#FF2A8E45");
  case GyeolPalette::ValidWarning:
    return juce::Colour::fromString("#FFB97700");
  case GyeolPalette::ValidError:
    return juce::Colour::fromString("#FFC83A42");
  case GyeolPalette::ControlBase:
    return juce::Colour::fromString("#FFF3F6FB");
  case GyeolPalette::ControlHover:
    return juce::Colour::fromString("#FFE9EEF7");
  case GyeolPalette::ControlDown:
    return juce::Colour::fromString("#FFDDE5F2");
  case GyeolPalette::ControlDisabled:
    return juce::Colour::fromString("#FFE4E8F0");
  case GyeolPalette::TextDisabled:
    return juce::Colour::fromString("#FF8B95A3");
  case GyeolPalette::SelectionBackground:
    return juce::Colour::fromString("#FFDDE7FA");
  case GyeolPalette::GridLine:
    return juce::Colour::fromString("#FFDCE2EC");
  default:
    return juce::Colours::transparentBlack;
  }
}
juce::Colour highContrastColour(Gyeol::GyeolPalette colorId) {
  using Gyeol::GyeolPalette;
  switch (colorId) {
  case GyeolPalette::CanvasBackground:
    return juce::Colour::fromString("#FF000000");
  case GyeolPalette::PanelBackground:
    return juce::Colour::fromString("#FF090909");
  case GyeolPalette::HeaderBackground:
    return juce::Colour::fromString("#FF121212");
  case GyeolPalette::OverlayBackground:
    return juce::Colour::fromString("#EA000000");
  case GyeolPalette::BorderDefault:
    return juce::Colour::fromString("#FFECECEC");
  case GyeolPalette::BorderActive:
    return juce::Colour::fromString("#FFFFFFFF");
  case GyeolPalette::TextPrimary:
    return juce::Colour::fromString("#FFFFFFFF");
  case GyeolPalette::TextSecondary:
    return juce::Colour::fromString("#FFE5E5E5");
  case GyeolPalette::AccentPrimary:
    return juce::Colour::fromString("#FF00C6FF");
  case GyeolPalette::AccentHover:
    return juce::Colour::fromString("#FF5BDFFF");
  case GyeolPalette::ValidSuccess:
    return juce::Colour::fromString("#FF1EFF8A");
  case GyeolPalette::ValidWarning:
    return juce::Colour::fromString("#FFFFD11A");
  case GyeolPalette::ValidError:
    return juce::Colour::fromString("#FFFF5A5A");
  case GyeolPalette::ControlBase:
    return juce::Colour::fromString("#FF131313");
  case GyeolPalette::ControlHover:
    return juce::Colour::fromString("#FF1F1F1F");
  case GyeolPalette::ControlDown:
    return juce::Colour::fromString("#FF272727");
  case GyeolPalette::ControlDisabled:
    return juce::Colour::fromString("#FF1A1A1A");
  case GyeolPalette::TextDisabled:
    return juce::Colour::fromString("#FF9A9A9A");
  case GyeolPalette::SelectionBackground:
    return juce::Colour::fromString("#FF1E2B36");
  case GyeolPalette::GridLine:
    return juce::Colour::fromString("#FF262626");
  default:
    return juce::Colours::transparentBlack;
  }
}
} // namespace
namespace Gyeol {
// ==============================================================================
// ???????? ??? ??? ???
// ==============================================================================
void setGyeolThemeVariant(GyeolThemeVariant variant) { gActiveThemeVariant = variant; }
GyeolThemeVariant getGyeolThemeVariant() noexcept { return gActiveThemeVariant; }
void setGyeolReducedMotionEnabled(bool enabled) { gReducedMotionEnabled = enabled; }
bool isGyeolReducedMotionEnabled() noexcept { return gReducedMotionEnabled; }
float getGyeolMotionScale() noexcept { return gReducedMotionEnabled ? 0.35f : 1.0f; }
juce::Colour getGyeolColor(GyeolPalette colorId) {
  switch (gActiveThemeVariant) {
  case GyeolThemeVariant::light:
    return lightColour(colorId);
  case GyeolThemeVariant::highContrast:
    return highContrastColour(colorId);
  case GyeolThemeVariant::deepDark:
  default:
    return deepDarkColour(colorId);
  }
}

// ==============================================================================
// GyeolCustomLookAndFeel ?앹꽦??諛?珥덇린??
// ==============================================================================
GyeolCustomLookAndFeel::GyeolCustomLookAndFeel() {
  // ============================================================================
  // 1. BinaryData?먯꽌 踰덈뱾 ?고듃瑜?濡쒕뱶?섏뿬 Typeface 罹먯떆?????
  //    Nunito  ???곷Ц/?쇳떞 UI 湲곕낯 ?고듃 (Clean Rounded Sans-Serif)
  //    NanumSquareRound ???쒓? UI 湲곕낯 ?고듃 (紐⑤뜕 ?κ렐 怨좊뵓)
  // ============================================================================
  nunitoRegular = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NunitoRegular_ttf, BinaryData::NunitoRegular_ttfSize);
  nunitoBold = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NunitoBold_ttf, BinaryData::NunitoBold_ttfSize);
  nanumRegular = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NanumSquareRoundR_ttf, BinaryData::NanumSquareRoundR_ttfSize);
  nanumBold = juce::Typeface::createSystemTypefaceFor(
      BinaryData::NanumSquareRoundB_ttf, BinaryData::NanumSquareRoundB_ttfSize);

  // 湲濡쒕쾶 湲곕낯 Typeface瑜?Nunito Regular濡?吏??
  // (JUCE媛 ?고듃 ?대쫫 ?놁씠 湲곕낯 ?곗꽭由ы봽瑜?洹몃┫ ????Typeface瑜??ъ슜)
  if (nunitoRegular != nullptr)
    setDefaultSansSerifTypeface(nunitoRegular);

  // ============================================================================
  // 2. JUCE 湲곕낯 而댄룷?뚰듃??而щ윭 泥닿퀎瑜?Gyeol ?붾젅???됱긽?쇰줈 媛뺤젣 ??뼱?곌린
  // ============================================================================

  // 李?諛?諛곌꼍
  setColour(juce::ResizableWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::CanvasBackground));

  // ?띿뒪???됱긽
  setColour(juce::Label::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));

  // ?щ씪?대뜑 ?됱긽
  setColour(juce::Slider::thumbColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::Slider::trackColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::Slider::backgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));

  // 踰꾪듉 ?됱긽
  setColour(juce::TextButton::buttonColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::TextButton::buttonOnColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::TextButton::textColourOffId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::TextButton::textColourOnId, juce::Colours::white);

  // ?앹뾽 硫붾돱 諛??붿궡?? 肄ㅻ낫諛뺤뒪 ?됱긽
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

  // ?ㅽ겕濡ㅻ컮 而ㅼ뒪? ?곕룞 (?κ렐 紐⑥꽌由ъ뿉 ?댁슱由щ룄濡??뉕퀬 ?대몼寃??좎?)
  setColour(juce::ScrollBar::thumbColourId,
            getGyeolColor(GyeolPalette::BorderActive));
  setColour(juce::ScrollBar::backgroundColourId,
            juce::Colours::transparentBlack);

  // ?몃━ 酉?而щ윭
  setColour(juce::TreeView::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::TreeView::selectedItemBackgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));

  // Phase 3: ?띿뒪???먮뵒??而щ윭
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

  // Phase 3: 由ъ뒪?몃컯??
  setColour(juce::ListBox::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::ListBox::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

  // Phase 3: ?댄똻 (?ㅽ겕 + ?쇱슫??
  setColour(juce::TooltipWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::HeaderBackground));
  setColour(juce::TooltipWindow::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::TooltipWindow::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));

  // Phase 3: ?뚮┝(AlertWindow) ?ㅽ겕
  setColour(juce::AlertWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::AlertWindow::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::AlertWindow::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));

  refreshFromTheme();
}

void GyeolCustomLookAndFeel::refreshFromTheme() {
  setColour(juce::ResizableWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::CanvasBackground));

  setColour(juce::Label::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));

  setColour(juce::Slider::thumbColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::Slider::trackColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::Slider::backgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));

  setColour(juce::TextButton::buttonColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::TextButton::buttonOnColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::TextButton::textColourOffId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::TextButton::textColourOnId,
            getGyeolColor(GyeolPalette::TextPrimary).contrasting(1.0f));

  setColour(juce::PopupMenu::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::PopupMenu::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::PopupMenu::highlightedBackgroundColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));
  setColour(juce::PopupMenu::highlightedTextColourId,
            getGyeolColor(GyeolPalette::TextPrimary).contrasting(1.0f));
  setColour(juce::ComboBox::backgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));
  setColour(juce::ComboBox::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::ComboBox::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));

  setColour(juce::ScrollBar::thumbColourId,
            getGyeolColor(GyeolPalette::BorderActive));
  setColour(juce::ScrollBar::backgroundColourId,
            juce::Colours::transparentBlack);

  setColour(juce::TreeView::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::TreeView::selectedItemBackgroundColourId,
            getGyeolColor(GyeolPalette::ControlBase));

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
  setColour(juce::TextEditor::highlightedTextColourId,
            getGyeolColor(GyeolPalette::TextPrimary).contrasting(1.0f));
  setColour(juce::CaretComponent::caretColourId,
            getGyeolColor(GyeolPalette::AccentPrimary));

  setColour(juce::ListBox::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::ListBox::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

  setColour(juce::TooltipWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::HeaderBackground));
  setColour(juce::TooltipWindow::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::TooltipWindow::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));

  setColour(juce::AlertWindow::backgroundColourId,
            getGyeolColor(GyeolPalette::PanelBackground));
  setColour(juce::AlertWindow::textColourId,
            getGyeolColor(GyeolPalette::TextPrimary));
  setColour(juce::AlertWindow::outlineColourId,
            getGyeolColor(GyeolPalette::BorderDefault));
}

GyeolCustomLookAndFeel::~GyeolCustomLookAndFeel() {}

// ==============================================================================
// 而ㅼ뒪? ?뚮뜑留??ㅻ쾭?쇱씠??援ы쁽
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
// makeFont: BinaryData Typeface 湲곕컲 Font ?앹꽦 ?ы띁
// ==============================================================================
juce::Font GyeolCustomLookAndFeel::makeFont(float height, bool bold) const {
  // NanumSquareRound???쒓? 臾몄옄瑜??대떦?섏?留?JUCE ?⑥씪 Typeface 援ъ“??
  // fallback 泥댁씤??吏곸젒 吏?먰븯吏 ?딆쑝誘濡?Nunito瑜?湲곕낯?쇰줈 ?ъ슜?⑸땲??
  // ?ν썑 ?ㅺ뎅???뺤옣 ??SystemFont fallback 泥섎━瑜??ш린??遺꾧린?????덉뒿?덈떎.
  juce::Typeface::Ptr tf = bold ? nunitoBold : nunitoRegular;
  if (tf == nullptr)
    return juce::Font(juce::FontOptions(height)); // 濡쒕뱶 ?ㅽ뙣 ???쒖뒪??湲곕낯
  return juce::Font(tf).withHeight(height);
}

// ==============================================================================
// Phase 3: 留덉씠?щ줈 ?몃쾭 ?몃옖吏?? ?쒕∼ ??꾩슦, ?낆껜媛?
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

  // 遺?쒕윭??諛곌꼍
  g.setColour(bg);
  g.fillRoundedRectangle(bounds, cornerRadius);

  // ?몃쾭 ??誘몄꽭??Glow ?뚮몢由?(AccentPrimary????щ챸???멸낸??
  if (shouldDrawButtonAsHighlighted && button.isEnabled() &&
      !shouldDrawButtonAsDown) {
    const auto glowColour =
        getGyeolColor(GyeolPalette::AccentPrimary).withAlpha(0.25f);
    g.setColour(glowColour);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.5f);
  } else if (!shouldDrawButtonAsDown) {
    // ?쇰컲 ?곹깭???꾩＜ 誘몄꽭??蹂대뜑
    g.setColour(getGyeolColor(GyeolPalette::BorderDefault));
    g.drawRoundedRectangle(bounds, cornerRadius, 0.8f);
  }
}

void GyeolCustomLookAndFeel::drawPopupMenuBackground(juce::Graphics &g,
                                                     int width, int height) {
  const auto bounds =
      juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
  const auto cornerRadius = 8.0f;

  // 1. ?쒕∼ ??꾩슦 (?낃퀬 ?볤쾶 ?쇱???洹몃┝??
  drawSoftDropShadow(g, bounds, 16.0f, 0.4f);

  // 2. 諛섑닾紐?諛곌꼍 (?쎄컙??湲?섏뒪紐⑦뵾利??④낵)
  const auto bgColour =
      getGyeolColor(GyeolPalette::PanelBackground).withAlpha(0.95f);
  g.setColour(bgColour);
  g.fillRoundedRectangle(bounds.reduced(1.0f), cornerRadius);

  // 3. 誘몄꽭??諛앹? 蹂대뜑濡??앹뾽 寃쎄퀎 媛뺤“
  g.setColour(getGyeolColor(GyeolPalette::BorderDefault).withAlpha(0.7f));
  g.drawRoundedRectangle(bounds.reduced(1.0f), cornerRadius, 1.0f);
}

int GyeolCustomLookAndFeel::getPopupMenuBorderSize() {
  return 4; // ?쇱슫??肄붾꼫瑜??꾪븳 ?대? ?щ갚
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
    // ?ъ빱???곹깭: AccentPrimary 蹂대뜑 + 誘몄꽭??Glow
    const auto accentCol = getGyeolColor(GyeolPalette::AccentPrimary);
    g.setColour(accentCol.withAlpha(0.2f));
    g.drawRoundedRectangle(bounds.expanded(1.0f), 5.0f, 2.0f);
    g.setColour(accentCol.withAlpha(0.7f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
  } else {
    // 鍮꾪룷而ㅼ뒪 ?곹깭: 誘몄꽭??蹂대뜑
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

  // 諛곌꼍
  g.setColour(getGyeolColor(GyeolPalette::HeaderBackground).withAlpha(0.5f));
  g.fillRoundedRectangle(bounds, 6.0f);

  // 誘몄꽭??蹂대뜑
  g.setColour(getGyeolColor(GyeolPalette::BorderDefault).withAlpha(0.5f));
  g.drawRoundedRectangle(bounds, 6.0f, 0.6f);

  // ?ㅻ뜑 ?띿뒪??
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
// drawSoftDropShadow: 踰붿슜 ?뚰봽???쒕∼ ??꾩슦 ?좏떥由ы떚
// ==============================================================================
void GyeolCustomLookAndFeel::drawSoftDropShadow(
    juce::Graphics &g, const juce::Rectangle<float> &area, float radius,
    float opacity) {
  // ?щ윭 寃뱀쓽 諛섑닾紐?寃????쇱슫???ш컖?뺤쑝濡??뚰봽????꾩슦 洹쇱궗
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






