#include "Teul/Editor/Search/SearchController.h"

#include <algorithm>

namespace Teul {

void TGraphCanvas::showNodeSearchOverlay() {
  if (searchOverlay == nullptr)
    return;

  searchOverlay->present(
      "Jump To Node", "Search node names...",
      [this](const juce::String &query) {
        struct Candidate {
          NodeId nodeId = kInvalidNodeId;
          juce::String title;
          juce::String subtitle;
          int score = std::numeric_limits<int>::min();
        };

        std::vector<Candidate> candidates;
        for (const auto &node : document.nodes) {
          const auto *desc =
              nodeRegistry ? nodeRegistry->descriptorFor(node.typeKey) : nullptr;
          Candidate candidate;
          candidate.nodeId = node.nodeId;
          candidate.title = nodeLabelForUi(node, nodeRegistry);
          const juce::String category =
              desc != nullptr ? desc->category : categoryLabelForTypeKey(node.typeKey);
          candidate.subtitle = category + " / " + node.typeKey;
          candidate.score = scoreTextMatch(candidate.title, query);
          candidate.score = juce::jmax(candidate.score,
                                       scoreTextMatch(candidate.subtitle, query));
          if (candidate.score == std::numeric_limits<int>::min())
            continue;
          candidates.push_back(std::move(candidate));
        }

        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate &a, const Candidate &b) {
                           if (a.score != b.score)
                             return a.score > b.score;
                           return a.title < b.title;
                         });

        if (candidates.size() > 48)
          candidates.resize(48);

        std::vector<SearchEntry> entries;
        entries.reserve(candidates.size());
        for (const auto &candidate : candidates) {
          SearchEntry entry;
          entry.title = candidate.title;
          entry.subtitle = candidate.subtitle;
          const NodeId nodeId = candidate.nodeId;
          const juce::String nodeTitle = candidate.title;
          entry.onSelect = [this, nodeId, nodeTitle] {
            focusNode(nodeId);
            pushStatusHint("Focused " + nodeTitle + ".");
          };
          entries.push_back(std::move(entry));
        }

        return entries;
      },
      false, getViewCenter());
}

} // namespace Teul
