#pragma once

#include <JuceHeader.h>

namespace Teul {

enum class TeulThemeVariant {
  studioDark,
  studioSlate,
  labLight,
};

struct TeulThemeColors {
  juce::Colour windowBackground;
  juce::Colour canvasBackground;
  juce::Colour gridLine;
  juce::Colour gridDot;

  juce::Colour panelBackground;
  juce::Colour panelBackgroundRaised;
  juce::Colour panelBackgroundAlt;
  juce::Colour panelBackgroundDeep;
  juce::Colour panelStroke;
  juce::Colour panelStrokeStrong;

  juce::Colour panelTextStrong;
  juce::Colour panelText;
  juce::Colour panelTextMuted;
  juce::Colour panelTextFaint;
  juce::Colour panelTextDisabled;

  juce::Colour inputBackground;
  juce::Colour inputBackgroundAlt;
  juce::Colour inputOutline;

  juce::Colour railBackground;
  juce::Colour railTrackBackground;
  juce::Colour portShellBackground;

  juce::Colour hudBackground;
  juce::Colour hudBackgroundAlt;
  juce::Colour hudStroke;
  juce::Colour backdropScrim;
  juce::Colour searchSelected;

  juce::Colour nodeBackground;
  juce::Colour nodeHeader;
  juce::Colour nodeBorder;
  juce::Colour nodeBorderSelected;

  juce::Colour accentBlue;
  juce::Colour accentSky;
  juce::Colour accentGreen;
  juce::Colour accentAmber;
  juce::Colour accentOrange;
  juce::Colour accentRed;
  juce::Colour accentPurple;
  juce::Colour accentSlate;
  juce::Colour accentGold;

  juce::Colour portAudio;
  juce::Colour portCV;
  juce::Colour portGate;
  juce::Colour portMIDI;
  juce::Colour portControl;

  juce::Colour wireNormal;
};

class TeulTheme {
public:
  static TeulThemeVariant variant() noexcept { return currentVariant(); }

  static void setVariant(TeulThemeVariant variantIn) noexcept {
    currentVariant() = variantIn;
  }

  static const TeulThemeColors &colors() noexcept {
    switch (currentVariant()) {
    case TeulThemeVariant::studioSlate:
      return studioSlate();
    case TeulThemeVariant::labLight:
      return labLight();
    case TeulThemeVariant::studioDark:
    default:
      return studioDark();
    }
  }

private:
  static TeulThemeVariant &currentVariant() noexcept {
    static TeulThemeVariant variant = TeulThemeVariant::studioSlate;
    return variant;
  }

  static const TeulThemeColors &studioDark() noexcept {
    static const TeulThemeColors colors{
        juce::Colour(0xff151c24), juce::Colour(0xff17202a),
        juce::Colour(0xff243140), juce::Colour(0xff314050),

        juce::Colour(0xff101922), juce::Colour(0xff14202b),
        juce::Colour(0xff0d151d), juce::Colour(0xff0b1220),
        juce::Colour(0xff324556), juce::Colour(0xff496378),

        juce::Colour(0xfff4f7fb), juce::Colour(0xffd7e1ea),
        juce::Colour(0xff9fb1c1), juce::Colour(0xff73869b),
        juce::Colour(0xff556577),

        juce::Colour(0xff111b25), juce::Colour(0xff0f1720),
        juce::Colour(0xff395064),

        juce::Colour(0xff0d1721), juce::Colour(0x16263747),
        juce::Colour(0xff0f172a),

        juce::Colour(0xd6131d28), juce::Colour(0xee111827),
        juce::Colour(0x284fa0f8), juce::Colour(0x66101822),
        juce::Colour(0xff285684),

        juce::Colour(0xff202734), juce::Colour(0xff2a3543),
        juce::Colour(0xff4a5968), juce::Colour(0xffd9e8f4),

        juce::Colour(0xff66a8ff), juce::Colour(0xff38bdf8),
        juce::Colour(0xff34d399), juce::Colour(0xfffbbf24),
        juce::Colour(0xfffb923c), juce::Colour(0xffef4444),
        juce::Colour(0xffa78bfa), juce::Colour(0xff94a3b8),
        juce::Colour(0xffd9c27c),

        juce::Colour(0xff2dd4bf), juce::Colour(0xfffbbf24),
        juce::Colour(0xfffb923c), juce::Colour(0xff60a5fa),
        juce::Colour(0xffc084fc),

        juce::Colour(0xd065e4d1)};
    return colors;
  }

  static const TeulThemeColors &studioSlate() noexcept {
    static const TeulThemeColors colors{
        juce::Colour(0xff1b2029), juce::Colour(0xff202732),
        juce::Colour(0xff303a47), juce::Colour(0xff404c5b),

        juce::Colour(0xff161d26), juce::Colour(0xff1a2330),
        juce::Colour(0xff121922), juce::Colour(0xff111926),
        juce::Colour(0xff415164), juce::Colour(0xff59738a),

        juce::Colour(0xfff3f6fa), juce::Colour(0xffd8e0e7),
        juce::Colour(0xffafbcc8), juce::Colour(0xff8291a0),
        juce::Colour(0xff627180),

        juce::Colour(0xff18212d), juce::Colour(0xff151c26),
        juce::Colour(0xff415468),

        juce::Colour(0xff131b25), juce::Colour(0x1a324457),
        juce::Colour(0xff162031),

        juce::Colour(0xd7192430), juce::Colour(0xee16212d),
        juce::Colour(0x285ea8f8), juce::Colour(0x66131b24),
        juce::Colour(0xff326391),

        juce::Colour(0xff26303c), juce::Colour(0xff33404f),
        juce::Colour(0xff59687a), juce::Colour(0xffe7eef5),

        juce::Colour(0xff7ab7ff), juce::Colour(0xff56c5ff),
        juce::Colour(0xff4ade80), juce::Colour(0xfff5c452),
        juce::Colour(0xfffb9f4a), juce::Colour(0xfff05a5a),
        juce::Colour(0xffb698ff), juce::Colour(0xffa5b3c2),
        juce::Colour(0xffdfc98a),

        juce::Colour(0xff40d9c6), juce::Colour(0xfff5c452),
        juce::Colour(0xfffb9f4a), juce::Colour(0xff76b5ff),
        juce::Colour(0xffcb97ff),

        juce::Colour(0xd07be8da)};
    return colors;
  }

  static const TeulThemeColors &labLight() noexcept {
    static const TeulThemeColors colors{
        juce::Colour(0xffe8eef5), juce::Colour(0xffedf2f7),
        juce::Colour(0xffd7dfe8), juce::Colour(0xffc1ccd8),

        juce::Colour(0xfff6f8fb), juce::Colour(0xffffffff),
        juce::Colour(0xffeef3f8), juce::Colour(0xffe8eef5),
        juce::Colour(0xffc9d4df), juce::Colour(0xffaab8c8),

        juce::Colour(0xff1e2936), juce::Colour(0xff314150),
        juce::Colour(0xff526276), juce::Colour(0xff738399),
        juce::Colour(0xff9aa8b8),

        juce::Colour(0xffffffff), juce::Colour(0xfff2f5f9),
        juce::Colour(0xffbecadb),

        juce::Colour(0xffeff4f8), juce::Colour(0x14c5d2de),
        juce::Colour(0xffe2eaf1),

        juce::Colour(0xddeff4f8), juce::Colour(0xfff6f8fb),
        juce::Colour(0x285b8fd1), juce::Colour(0x44e8eef5),
        juce::Colour(0xffdce7f4),

        juce::Colour(0xfff4f7fb), juce::Colour(0xffeaf0f6),
        juce::Colour(0xffbbc7d6), juce::Colour(0xff1e2936),

        juce::Colour(0xff2f6fd1), juce::Colour(0xff0ea5e9),
        juce::Colour(0xff16a34a), juce::Colour(0xffd97706),
        juce::Colour(0xffea580c), juce::Colour(0xffdc2626),
        juce::Colour(0xff7c3aed), juce::Colour(0xff64748b),
        juce::Colour(0xffa16207),

        juce::Colour(0xff0f9f89), juce::Colour(0xffd97706),
        juce::Colour(0xffea580c), juce::Colour(0xff2563eb),
        juce::Colour(0xff8b5cf6),

        juce::Colour(0xcc2f6fd1)};
    return colors;
  }
};

struct TeulPalette {
  static juce::Colour WindowBackground() noexcept { return TeulTheme::colors().windowBackground; }
  static juce::Colour CanvasBackground() noexcept { return TeulTheme::colors().canvasBackground; }
  static juce::Colour GridLine() noexcept { return TeulTheme::colors().gridLine; }
  static juce::Colour GridDot() noexcept { return TeulTheme::colors().gridDot; }
  static juce::Colour PanelBackground() noexcept { return TeulTheme::colors().panelBackground; }
  static juce::Colour PanelBackgroundRaised() noexcept { return TeulTheme::colors().panelBackgroundRaised; }
  static juce::Colour PanelBackgroundAlt() noexcept { return TeulTheme::colors().panelBackgroundAlt; }
  static juce::Colour PanelBackgroundDeep() noexcept { return TeulTheme::colors().panelBackgroundDeep; }
  static juce::Colour PanelStroke() noexcept { return TeulTheme::colors().panelStroke; }
  static juce::Colour PanelStrokeStrong() noexcept { return TeulTheme::colors().panelStrokeStrong; }
  static juce::Colour PanelTextStrong() noexcept { return TeulTheme::colors().panelTextStrong; }
  static juce::Colour PanelText() noexcept { return TeulTheme::colors().panelText; }
  static juce::Colour PanelTextMuted() noexcept { return TeulTheme::colors().panelTextMuted; }
  static juce::Colour PanelTextFaint() noexcept { return TeulTheme::colors().panelTextFaint; }
  static juce::Colour PanelTextDisabled() noexcept { return TeulTheme::colors().panelTextDisabled; }
  static juce::Colour InputBackground() noexcept { return TeulTheme::colors().inputBackground; }
  static juce::Colour InputBackgroundAlt() noexcept { return TeulTheme::colors().inputBackgroundAlt; }
  static juce::Colour InputOutline() noexcept { return TeulTheme::colors().inputOutline; }
  static juce::Colour RailBackground() noexcept { return TeulTheme::colors().railBackground; }
  static juce::Colour RailTrackBackground() noexcept { return TeulTheme::colors().railTrackBackground; }
  static juce::Colour PortShellBackground() noexcept { return TeulTheme::colors().portShellBackground; }
  static juce::Colour HudBackground() noexcept { return TeulTheme::colors().hudBackground; }
  static juce::Colour HudBackgroundAlt() noexcept { return TeulTheme::colors().hudBackgroundAlt; }
  static juce::Colour HudStroke() noexcept { return TeulTheme::colors().hudStroke; }
  static juce::Colour BackdropScrim() noexcept { return TeulTheme::colors().backdropScrim; }
  static juce::Colour SearchSelected() noexcept { return TeulTheme::colors().searchSelected; }
  static juce::Colour NodeBackground() noexcept { return TeulTheme::colors().nodeBackground; }
  static juce::Colour NodeHeader() noexcept { return TeulTheme::colors().nodeHeader; }
  static juce::Colour NodeBorder() noexcept { return TeulTheme::colors().nodeBorder; }
  static juce::Colour NodeBorderSelected() noexcept { return TeulTheme::colors().nodeBorderSelected; }
  static juce::Colour AccentBlue() noexcept { return TeulTheme::colors().accentBlue; }
  static juce::Colour AccentSky() noexcept { return TeulTheme::colors().accentSky; }
  static juce::Colour AccentGreen() noexcept { return TeulTheme::colors().accentGreen; }
  static juce::Colour AccentAmber() noexcept { return TeulTheme::colors().accentAmber; }
  static juce::Colour AccentOrange() noexcept { return TeulTheme::colors().accentOrange; }
  static juce::Colour AccentRed() noexcept { return TeulTheme::colors().accentRed; }
  static juce::Colour AccentPurple() noexcept { return TeulTheme::colors().accentPurple; }
  static juce::Colour AccentSlate() noexcept { return TeulTheme::colors().accentSlate; }
  static juce::Colour AccentGold() noexcept { return TeulTheme::colors().accentGold; }
  static juce::Colour PortAudio() noexcept { return TeulTheme::colors().portAudio; }
  static juce::Colour PortCV() noexcept { return TeulTheme::colors().portCV; }
  static juce::Colour PortGate() noexcept { return TeulTheme::colors().portGate; }
  static juce::Colour PortMIDI() noexcept { return TeulTheme::colors().portMIDI; }
  static juce::Colour PortControl() noexcept { return TeulTheme::colors().portControl; }
  static juce::Colour WireNormal() noexcept { return TeulTheme::colors().wireNormal; }
};

} // namespace Teul
