#include "Teul/Editor/Search/SearchController.h"

#include <algorithm>

namespace Teul {

void TGraphCanvas::showCommandPaletteOverlay() {
  if (searchOverlay == nullptr)
    return;

  searchOverlay->present(
      "Command Palette", "Search commands...",
      [this](const juce::String &query) {
        struct Candidate {
          SearchEntry entry;
          int score = std::numeric_limits<int>::min();
        };

        std::vector<Candidate> candidates;
        auto addCommand = [&](const juce::String &title,
                              const juce::String &subtitle,
                              std::function<void()> action) {
          int score = scoreTextMatch(title, query);
          score = juce::jmax(score, scoreTextMatch(subtitle, query));
          score = juce::jmax(score, scoreTextMatch(title + " " + subtitle, query));
          if (score == std::numeric_limits<int>::min())
            return;

          Candidate candidate;
          candidate.entry.title = title;
          candidate.entry.subtitle = subtitle;
          candidate.entry.onSelect = std::move(action);
          candidate.score = score;
          candidates.push_back(std::move(candidate));
        };

        addCommand("Quick Add", "Insert a node at the current view center",
                   [this] { showQuickAddPrompt(getViewCenter()); });
        addCommand("Jump To Node", "Search nodes and focus the camera",
                   [this] { showNodeSearchOverlay(); });
        addCommand("Add Bookmark", "Store the current viewport as a bookmark",
                   [this] {
                     TBookmark bookmark;
                     bookmark.bookmarkId = document.allocBookmarkId();
                     bookmark.name = "Bookmark " + juce::String(bookmark.bookmarkId);
                     bookmark.focusX = viewOriginWorld.x;
                     bookmark.focusY = viewOriginWorld.y;
                     bookmark.zoom = zoomLevel;
                     document.bookmarks.push_back(bookmark);
                     document.touch(false);
                     pushStatusHint("Added bookmark.");
                   });
        addCommand("Duplicate Selection", "Ctrl+D",
                   [this] { duplicateSelection(); });
        addCommand("Delete Selection", "Delete or Backspace",
                   [this] { deleteSelectionWithPrompt(); });
        addCommand("Disconnect Selection", "Disconnect all selected wires",
                   [this] { disconnectSelectionWithPrompt(); });
        addCommand("Toggle Bypass", "Toggle bypass on selected nodes",
                   [this] { toggleBypassSelection(); });
        addCommand("Align Left", "Align selected nodes to the left edge",
                   [this] { alignSelectionLeft(); });
        addCommand("Align Top", "Align selected nodes to the top edge",
                   [this] { alignSelectionTop(); });
        addCommand("Distribute Horizontally",
                   "Evenly distribute selected nodes horizontally",
                   [this] { distributeSelectionHorizontally(); });
        addCommand("Distribute Vertically",
                   "Evenly distribute selected nodes vertically",
                   [this] { distributeSelectionVertically(); });

        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate &a, const Candidate &b) {
                           if (a.score != b.score)
                             return a.score > b.score;
                           return a.entry.title < b.entry.title;
                         });

        std::vector<SearchEntry> entries;
        entries.reserve(candidates.size());
        for (auto &candidate : candidates)
          entries.push_back(std::move(candidate.entry));
        return entries;
      },
      false, getViewCenter());
}

} // namespace Teul
