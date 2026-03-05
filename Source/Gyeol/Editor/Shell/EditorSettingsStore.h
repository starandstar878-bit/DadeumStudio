#pragma once

#include "Gyeol/Editor/Shell/EditorLayoutCoordinator.h"
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace Gyeol::Shell
{
    class EditorSettingsStore
    {
    public:
        struct ShortcutBindingEntry
        {
            juce::String id;
            juce::KeyPress defaultKey;
            juce::KeyPress userKey;
        };

        struct StateSnapshot
        {
            int themeVariantOrdinal = 0;
            int densityPresetOrdinal = 1;
            bool reducedMotionEnabled = false;
            bool lowEndRenderingMode = false;
            std::map<int, EditorLayoutCoordinator::LayoutProfile> layoutProfiles;
            bool settingsLoaded = false;
        };

        static int breakpointStorageKey(EditorLayoutCoordinator::BreakpointClass bp) noexcept;
        static juce::String breakpointToken(EditorLayoutCoordinator::BreakpointClass bp);

        void ensureInitialized();
        bool isInitialized() const noexcept;

        void load(
            StateSnapshot& state,
            juce::Rectangle<int> currentBounds,
            const std::function<EditorLayoutCoordinator::LayoutProfile(
                EditorLayoutCoordinator::BreakpointClass,
                juce::Rectangle<int>)>& defaultLayoutProfileFor);

        void save(const StateSnapshot& state,
                  const std::vector<ShortcutBindingEntry>& shortcutBindings);

        void loadShortcutOverrides(std::vector<ShortcutBindingEntry>& shortcutBindings);

    private:
        std::unique_ptr<juce::PropertiesFile> settingsFile;
    };
}