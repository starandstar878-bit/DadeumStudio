#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Gyeol::Shell
{
    class EditorCommandSystem
    {
    public:
        struct Binding
        {
            juce::String id;
            juce::String label;
            juce::String keywords;
            juce::KeyPress defaultKey;
            juce::KeyPress userKey;
            std::function<void()> action;
        };

        struct ShortcutOverride
        {
            juce::String id;
            juce::KeyPress key;
        };

        struct Conflict
        {
            juce::String shortcut;
            std::vector<juce::String> bindingIds;
        };

        struct ViewItem
        {
            juce::String id;
            juce::String label;
            juce::String keywords;
            juce::String shortcut;
            bool hasConflict = false;
            int matchScore = 0;
        };

        void clear();
        void addBinding(Binding binding);
        const std::vector<Binding>& bindings() const noexcept;

        juce::KeyPress effectiveShortcut(const Binding& binding) const;
        static juce::String shortcutText(const juce::KeyPress& key);

        bool trigger(const juce::KeyPress& key) const;

        std::vector<ShortcutOverride> exportOverrides() const;
        void applyOverrides(const std::vector<ShortcutOverride>& overrides);

        std::vector<Conflict> findConflicts() const;
        std::vector<ViewItem> buildCommandViews(const juce::String& query = {}) const;

    private:
        static int computeMatchScore(const Binding& binding,
                                     const juce::String& queryLower);

        std::vector<Binding> shortcutBindings;
    };
}