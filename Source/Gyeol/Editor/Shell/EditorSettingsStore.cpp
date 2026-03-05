#include "Gyeol/Editor/Shell/EditorSettingsStore.h"

namespace Gyeol::Shell
{
    int EditorSettingsStore::breakpointStorageKey(
        EditorLayoutCoordinator::BreakpointClass bp) noexcept
    {
        switch (bp)
        {
            case EditorLayoutCoordinator::BreakpointClass::wide: return 0;
            case EditorLayoutCoordinator::BreakpointClass::desktop: return 1;
            case EditorLayoutCoordinator::BreakpointClass::tablet: return 2;
            case EditorLayoutCoordinator::BreakpointClass::narrow:
            default: return 3;
        }
    }

    juce::String EditorSettingsStore::breakpointToken(
        EditorLayoutCoordinator::BreakpointClass bp)
    {
        switch (bp)
        {
            case EditorLayoutCoordinator::BreakpointClass::wide: return "wide";
            case EditorLayoutCoordinator::BreakpointClass::desktop: return "desktop";
            case EditorLayoutCoordinator::BreakpointClass::tablet: return "tablet";
            case EditorLayoutCoordinator::BreakpointClass::narrow:
            default: return "narrow";
        }
    }

    void EditorSettingsStore::ensureInitialized()
    {
        if (settingsFile != nullptr)
            return;

        juce::PropertiesFile::Options options;
        options.applicationName = "DadeumStudio";
        options.filenameSuffix = "settings";
        options.folderName = "DadeumStudio";
        options.osxLibrarySubFolder = "Application Support";
        options.commonToAllUsers = false;
        settingsFile = std::make_unique<juce::PropertiesFile>(options);
    }

    bool EditorSettingsStore::isInitialized() const noexcept
    {
        return settingsFile != nullptr;
    }

    void EditorSettingsStore::load(
        StateSnapshot& state,
        juce::Rectangle<int> currentBounds,
        const std::function<EditorLayoutCoordinator::LayoutProfile(
            EditorLayoutCoordinator::BreakpointClass,
            juce::Rectangle<int>)>& defaultLayoutProfileFor)
    {
        ensureInitialized();
        if (settingsFile == nullptr || state.settingsLoaded)
            return;

        const auto themeRaw = settingsFile->getIntValue("ui.themeVariant", state.themeVariantOrdinal);
        if (themeRaw >= 0 && themeRaw <= 3)
            state.themeVariantOrdinal = themeRaw;

        const auto densityRaw = settingsFile->getIntValue("ui.densityPreset", state.densityPresetOrdinal);
        if (densityRaw >= 0 && densityRaw <= 2)
            state.densityPresetOrdinal = densityRaw;

        state.reducedMotionEnabled = settingsFile->getBoolValue("ui.reducedMotion", false);
        state.lowEndRenderingMode = settingsFile->getBoolValue("ui.lowEndRendering", false);

        for (const auto bp : {
                 EditorLayoutCoordinator::BreakpointClass::wide,
                 EditorLayoutCoordinator::BreakpointClass::desktop,
                 EditorLayoutCoordinator::BreakpointClass::tablet,
                 EditorLayoutCoordinator::BreakpointClass::narrow })
        {
            auto profile = defaultLayoutProfileFor(bp, currentBounds);
            const auto prefix = "layout." + breakpointToken(bp) + ".";
            profile.leftWidth = settingsFile->getIntValue(prefix + "leftWidth", profile.leftWidth);
            profile.rightWidth = settingsFile->getIntValue(prefix + "rightWidth", profile.rightWidth);
            profile.historyHeight =
                settingsFile->getIntValue(prefix + "historyHeight", profile.historyHeight);
            profile.leftDockVisible =
                settingsFile->getBoolValue(prefix + "leftDockVisible", profile.leftDockVisible);
            profile.rightDockVisible =
                settingsFile->getBoolValue(prefix + "rightDockVisible", profile.rightDockVisible);
            profile.leftOverlayOpen =
                settingsFile->getBoolValue(prefix + "leftOverlayOpen", false);
            profile.rightOverlayOpen =
                settingsFile->getBoolValue(prefix + "rightOverlayOpen", false);

            state.layoutProfiles[breakpointStorageKey(bp)] = profile;
        }

        state.settingsLoaded = true;
    }

    void EditorSettingsStore::save(
        const StateSnapshot& state,
        const std::vector<ShortcutBindingEntry>& shortcutBindings)
    {
        ensureInitialized();
        if (settingsFile == nullptr)
            return;

        settingsFile->setValue("ui.themeVariant", state.themeVariantOrdinal);
        settingsFile->setValue("ui.densityPreset", state.densityPresetOrdinal);
        settingsFile->setValue("ui.reducedMotion", state.reducedMotionEnabled);
        settingsFile->setValue("ui.lowEndRendering", state.lowEndRenderingMode);

        for (const auto& [key, profile] : state.layoutProfiles)
        {
            EditorLayoutCoordinator::BreakpointClass bp =
                EditorLayoutCoordinator::BreakpointClass::desktop;
            switch (key)
            {
                case 0:
                    bp = EditorLayoutCoordinator::BreakpointClass::wide;
                    break;
                case 1:
                    bp = EditorLayoutCoordinator::BreakpointClass::desktop;
                    break;
                case 2:
                    bp = EditorLayoutCoordinator::BreakpointClass::tablet;
                    break;
                case 3:
                default:
                    bp = EditorLayoutCoordinator::BreakpointClass::narrow;
                    break;
            }

            const auto prefix = "layout." + breakpointToken(bp) + ".";
            settingsFile->setValue(prefix + "leftWidth", profile.leftWidth);
            settingsFile->setValue(prefix + "rightWidth", profile.rightWidth);
            settingsFile->setValue(prefix + "historyHeight", profile.historyHeight);
            settingsFile->setValue(prefix + "leftDockVisible", profile.leftDockVisible);
            settingsFile->setValue(prefix + "rightDockVisible", profile.rightDockVisible);
            settingsFile->setValue(prefix + "leftOverlayOpen", profile.leftOverlayOpen);
            settingsFile->setValue(prefix + "rightOverlayOpen", profile.rightOverlayOpen);
        }

        for (const auto& binding : shortcutBindings)
        {
            const auto key = "shortcut." + binding.id;
            settingsFile->setValue(key,
                                   binding.userKey.isValid()
                                       ? binding.userKey.getTextDescription()
                                       : juce::String());
        }

        settingsFile->saveIfNeeded();
    }

    void EditorSettingsStore::loadShortcutOverrides(
        std::vector<ShortcutBindingEntry>& shortcutBindings)
    {
        ensureInitialized();
        if (settingsFile == nullptr)
            return;

        for (auto& binding : shortcutBindings)
        {
            const auto raw = settingsFile->getValue("shortcut." + binding.id, {});
            if (raw.isEmpty())
            {
                binding.userKey = {};
                continue;
            }

            binding.userKey = juce::KeyPress::createFromDescription(raw);
        }
    }
}