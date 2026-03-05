#include "Gyeol/Editor/Shell/EditorCommandSystem.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Gyeol::Shell
{
    void EditorCommandSystem::clear()
    {
        shortcutBindings.clear();
    }

    void EditorCommandSystem::addBinding(Binding binding)
    {
        if (binding.id.trim().isEmpty())
            return;

        if (const auto it = std::find_if(shortcutBindings.begin(),
                                         shortcutBindings.end(),
                                         [&binding](const Binding& existing)
                                         {
                                             return existing.id == binding.id;
                                         });
            it != shortcutBindings.end())
        {
            const auto preservedUserKey = it->userKey;
            *it = std::move(binding);
            if (!it->userKey.isValid())
                it->userKey = preservedUserKey;
            return;
        }

        shortcutBindings.push_back(std::move(binding));
    }

    const std::vector<EditorCommandSystem::Binding>&
    EditorCommandSystem::bindings() const noexcept
    {
        return shortcutBindings;
    }

    juce::KeyPress EditorCommandSystem::effectiveShortcut(const Binding& binding) const
    {
        return binding.userKey.isValid() ? binding.userKey : binding.defaultKey;
    }

    juce::String EditorCommandSystem::shortcutText(const juce::KeyPress& key)
    {
        if (!key.isValid())
            return "(none)";

        auto text = key.getTextDescription();
        text = text.replace("command", "Cmd/Ctrl");
        text = text.replace("ctrl", "Ctrl");
        return text;
    }

    bool EditorCommandSystem::trigger(const juce::KeyPress& key) const
    {
        for (const auto& binding : shortcutBindings)
        {
            if (effectiveShortcut(binding) != key)
                continue;

            if (binding.action)
                binding.action();
            return true;
        }

        return false;
    }

    std::vector<EditorCommandSystem::ShortcutOverride>
    EditorCommandSystem::exportOverrides() const
    {
        std::vector<ShortcutOverride> overrides;
        overrides.reserve(shortcutBindings.size());

        for (const auto& binding : shortcutBindings)
        {
            if (!binding.userKey.isValid())
                continue;

            overrides.push_back({ binding.id, binding.userKey });
        }

        return overrides;
    }

    void EditorCommandSystem::applyOverrides(
        const std::vector<ShortcutOverride>& overrides)
    {
        std::unordered_map<juce::String, juce::KeyPress> byId;
        byId.reserve(overrides.size());
        for (const auto& entry : overrides)
            byId[entry.id] = entry.key;

        for (auto& binding : shortcutBindings)
        {
            if (const auto it = byId.find(binding.id); it != byId.end())
                binding.userKey = it->second;
        }
    }

    std::vector<EditorCommandSystem::Conflict> EditorCommandSystem::findConflicts() const
    {
        std::unordered_map<juce::String, std::vector<juce::String>> byShortcut;
        byShortcut.reserve(shortcutBindings.size());

        for (const auto& binding : shortcutBindings)
        {
            const auto key = effectiveShortcut(binding);
            if (!key.isValid())
                continue;

            byShortcut[shortcutText(key)].push_back(binding.id);
        }

        std::vector<Conflict> conflicts;
        conflicts.reserve(byShortcut.size());
        for (auto& [shortcut, ids] : byShortcut)
        {
            if (ids.size() < 2)
                continue;

            std::sort(ids.begin(), ids.end());
            conflicts.push_back({ shortcut, std::move(ids) });
        }

        std::sort(conflicts.begin(),
                  conflicts.end(),
                  [](const Conflict& lhs, const Conflict& rhs)
                  {
                      return lhs.shortcut < rhs.shortcut;
                  });

        return conflicts;
    }

    int EditorCommandSystem::computeMatchScore(const Binding& binding,
                                               const juce::String& queryLower)
    {
        if (queryLower.isEmpty())
            return 1;

        const auto idLower = binding.id.toLowerCase();
        const auto labelLower = binding.label.toLowerCase();
        const auto keywordsLower = binding.keywords.toLowerCase();
        const auto shortcutLower = shortcutText(
            binding.userKey.isValid() ? binding.userKey : binding.defaultKey)
                                     .toLowerCase();

        if (labelLower == queryLower)
            return 100;
        if (labelLower.startsWith(queryLower))
            return 80;
        if (labelLower.contains(queryLower))
            return 60;
        if (keywordsLower.contains(queryLower))
            return 40;
        if (idLower.contains(queryLower))
            return 30;
        if (shortcutLower.contains(queryLower))
            return 20;
        return 0;
    }

    std::vector<EditorCommandSystem::ViewItem>
    EditorCommandSystem::buildCommandViews(const juce::String& query) const
    {
        const auto queryLower = query.trim().toLowerCase();
        const auto conflicts = findConflicts();
        std::unordered_set<juce::String> conflictedIds;
        for (const auto& conflict : conflicts)
        {
            for (const auto& id : conflict.bindingIds)
                conflictedIds.insert(id);
        }

        std::vector<ViewItem> views;
        views.reserve(shortcutBindings.size());

        for (const auto& binding : shortcutBindings)
        {
            const auto score = computeMatchScore(binding, queryLower);
            if (!queryLower.isEmpty() && score <= 0)
                continue;

            ViewItem item;
            item.id = binding.id;
            item.label = binding.label;
            item.keywords = binding.keywords;
            item.shortcut = shortcutText(effectiveShortcut(binding));
            item.hasConflict = conflictedIds.count(binding.id) > 0;
            item.matchScore = score;
            views.push_back(std::move(item));
        }

        std::sort(views.begin(),
                  views.end(),
                  [&queryLower](const ViewItem& lhs, const ViewItem& rhs)
                  {
                      if (!queryLower.isEmpty() && lhs.matchScore != rhs.matchScore)
                          return lhs.matchScore > rhs.matchScore;
                      return lhs.label.compareIgnoreCase(rhs.label) < 0;
                  });

        return views;
    }
}