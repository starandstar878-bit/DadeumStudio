#pragma once

#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

namespace Gyeol::Theme
{
    juce::String themeVariantToken(GyeolThemeVariant variant);

    GyeolThemeVariant themeVariantFromToken(
        const juce::String& token,
        GyeolThemeVariant fallback = GyeolThemeVariant::deepDark);

    juce::StringArray paletteDebugDump();
}