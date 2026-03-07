#include "Teul/Editor/Search/SearchController.h"

#include <algorithm>
#include <limits>

namespace Teul {

const TNodeRegistry *getSharedRegistry() {
  static auto reg = makeDefaultNodeRegistry();
  return reg.get();
}


static juce::String toLowerCase(const juce::String &text) {
  return text.toLowerCase();
}

juce::String nodeLabelForUi(const TNode &node,
                                   const TNodeRegistry *registry) {
  if (node.label.isNotEmpty())
    return node.label;

  if (registry != nullptr) {
    if (const auto *desc = registry->descriptorFor(node.typeKey)) {
      if (desc->displayName.isNotEmpty())
        return desc->displayName;
    }
  }

  return node.typeKey;
}

juce::String categoryLabelForTypeKey(const juce::String &typeKey) {
  const auto tail = typeKey.fromFirstOccurrenceOf("Teul.", false, false);
  const int dot = tail.indexOfChar('.');
  return dot > 0 ? tail.substring(0, dot) : "Node";
}

static int fuzzySubsequenceScore(const juce::String &textLower,
                                 const juce::String &queryLower) {
  if (queryLower.isEmpty())
    return 0;

  int scan = 0;
  int score = 0;
  int contiguous = 0;

  for (int qi = 0; qi < queryLower.length(); ++qi) {
    const auto qc = queryLower[qi];
    bool found = false;

    for (int ti = scan; ti < textLower.length(); ++ti) {
      if (textLower[ti] != qc)
        continue;

      const int gap = ti - scan;
      score += juce::jmax(2, 18 - gap * 2);
      contiguous = (ti == scan ? contiguous + 1 : 0);
      score += contiguous * 5;
      scan = ti + 1;
      found = true;
      break;
    }

    if (!found)
      return std::numeric_limits<int>::min();
  }

  score -= juce::jmax(0, textLower.length() - queryLower.length());
  return score;
}

int scoreTextMatch(const juce::String &textRaw,
                          const juce::String &queryRaw) {
  const juce::String text = toLowerCase(textRaw.trim());
  const juce::String query = toLowerCase(queryRaw.trim());
  if (query.isEmpty())
    return 1;
  if (text.isEmpty())
    return std::numeric_limits<int>::min();
  if (text == query)
    return 420;
  if (text.startsWith(query))
    return 340 - juce::jmin(80, text.length() - query.length());

  const int index = text.indexOf(query);
  if (index >= 0)
    return 260 - index * 3;

  const int fuzzy = fuzzySubsequenceScore(text, query);
  if (fuzzy != std::numeric_limits<int>::min())
    return 140 + fuzzy;

  return std::numeric_limits<int>::min();
}

static const char *kLibraryDragPrefix = "teul.node:";

juce::String extractLibraryDragTypeKey(const juce::var &description) {
  const juce::String text = description.toString();
  if (!text.startsWith(kLibraryDragPrefix))
    return {};

  return text.fromFirstOccurrenceOf(kLibraryDragPrefix, false, false);
}

std::vector<const TNodeDescriptor *> TGraphCanvas::getAllNodeDescriptors() const {
  std::vector<const TNodeDescriptor *> result;
  if (nodeRegistry == nullptr)
    return result;

  const auto &all = nodeRegistry->getAllDescriptors();
  result.reserve(all.size());
  for (const auto &desc : all)
    result.push_back(&desc);

  std::stable_sort(result.begin(), result.end(),
                   [](const TNodeDescriptor *a, const TNodeDescriptor *b) {
                     if (a->category != b->category)
                       return a->category < b->category;
                     return a->displayName < b->displayName;
                   });

  return result;
}

int TGraphCanvas::scoreDescriptorMatch(const TNodeDescriptor &desc,
                                       const juce::String &query) const {
  const juce::String q = toLowerCase(query.trim());

  int best = scoreTextMatch(desc.displayName, q);
  best = juce::jmax(best, scoreTextMatch(desc.typeKey, q));
  best = juce::jmax(best, scoreTextMatch(desc.category, q));

  if (best == std::numeric_limits<int>::min())
    return best;

  auto recentIt = std::find(recentNodeTypes.begin(), recentNodeTypes.end(),
                            desc.typeKey);
  if (recentIt != recentNodeTypes.end()) {
    const int recentIndex = (int)std::distance(recentNodeTypes.begin(), recentIt);
    best += juce::jmax(18, 96 - recentIndex * 8);
  }

  if (q.isEmpty())
    best += 10;

  return best;
}

bool TGraphCanvas::focusNodeByQuery(const juce::String &query) {
  const juce::String q = toLowerCase(query.trim());
  if (q.isEmpty())
    return false;

  NodeId bestNodeId = kInvalidNodeId;
  int bestScore = std::numeric_limits<int>::min();

  for (const auto &node : document.nodes) {
    const TNodeDescriptor *desc =
        nodeRegistry ? nodeRegistry->descriptorFor(node.typeKey) : nullptr;

    int score = scoreTextMatch(node.label, q);
    score = juce::jmax(score, scoreTextMatch(node.typeKey, q));
    if (desc != nullptr)
      score = juce::jmax(score, scoreTextMatch(desc->displayName, q));

    if (score > bestScore) {
      bestScore = score;
      bestNodeId = node.nodeId;
    }
  }

  if (bestNodeId == kInvalidNodeId || bestScore == std::numeric_limits<int>::min())
    return false;

  focusNode(bestNodeId);
  return true;
}

void TGraphCanvas::rememberRecentNode(const juce::String &typeKey) {
  if (typeKey.isEmpty())
    return;

  recentNodeTypes.erase(
      std::remove(recentNodeTypes.begin(), recentNodeTypes.end(), typeKey),
      recentNodeTypes.end());
  recentNodeTypes.insert(recentNodeTypes.begin(), typeKey);

  if (recentNodeTypes.size() > 16)
    recentNodeTypes.resize(16);
}

} // namespace Teul
