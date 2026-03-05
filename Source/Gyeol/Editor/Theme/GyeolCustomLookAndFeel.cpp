#include "Gyeol/Editor/Theme/GyeolCustomLookAndFeel.h"

#include <array>

namespace Gyeol::Theme
{
    juce::String themeVariantToken(GyeolThemeVariant variant)
    {
        switch (variant)
        {
            case GyeolThemeVariant::deepDark: return "deepDark";
            case GyeolThemeVariant::light: return "light";
            case GyeolThemeVariant::highContrast:
            default: return "highContrast";
        }
    }

    GyeolThemeVariant themeVariantFromToken(
        const juce::String& token,
        GyeolThemeVariant fallback)
    {
        const auto normalized = token.trim().toLowerCase();
        if (normalized == "deepdark" || normalized == "deep_dark" || normalized == "dark")
            return GyeolThemeVariant::deepDark;
        if (normalized == "light")
            return GyeolThemeVariant::light;
        if (normalized == "highcontrast" || normalized == "high_contrast"
            || normalized == "contrast")
            return GyeolThemeVariant::highContrast;

        return fallback;
    }

    juce::StringArray paletteDebugDump()
    {
        using Entry = std::pair<const char*, GyeolPalette>;
        static constexpr std::array<Entry, 20> entries = {
            Entry{ "CanvasBackground", GyeolPalette::CanvasBackground },
            Entry{ "PanelBackground", GyeolPalette::PanelBackground },
            Entry{ "HeaderBackground", GyeolPalette::HeaderBackground },
            Entry{ "OverlayBackground", GyeolPalette::OverlayBackground },
            Entry{ "BorderDefault", GyeolPalette::BorderDefault },
            Entry{ "BorderActive", GyeolPalette::BorderActive },
            Entry{ "TextPrimary", GyeolPalette::TextPrimary },
            Entry{ "TextSecondary", GyeolPalette::TextSecondary },
            Entry{ "AccentPrimary", GyeolPalette::AccentPrimary },
            Entry{ "AccentHover", GyeolPalette::AccentHover },
            Entry{ "ValidSuccess", GyeolPalette::ValidSuccess },
            Entry{ "ValidWarning", GyeolPalette::ValidWarning },
            Entry{ "ValidError", GyeolPalette::ValidError },
            Entry{ "ControlBase", GyeolPalette::ControlBase },
            Entry{ "ControlHover", GyeolPalette::ControlHover },
            Entry{ "ControlDown", GyeolPalette::ControlDown },
            Entry{ "ControlDisabled", GyeolPalette::ControlDisabled },
            Entry{ "TextDisabled", GyeolPalette::TextDisabled },
            Entry{ "SelectionBackground", GyeolPalette::SelectionBackground },
            Entry{ "GridLine", GyeolPalette::GridLine },
        };

        juce::StringArray lines;
        lines.add("Theme=" + themeVariantToken(getGyeolThemeVariant()));
        for (const auto& [name, palette] : entries)
            lines.add(juce::String(name) + "=" + getGyeolColor(palette).toDisplayString(true));

        return lines;
    }
}