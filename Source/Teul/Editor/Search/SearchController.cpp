#include "Teul/Editor/Search/SearchController.h"

#include <algorithm>
#include <limits>
#include <set>

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

namespace {

void applyUiFallbackMetadata(TTeulExposedParam &param, const juce::var &value,
                             const juce::String &key) {
  param.valueType = value.isBool() ? TParamValueType::Bool
                                   : (value.isInt() || value.isInt64())
                                         ? TParamValueType::Int
                                         : value.isString() ? TParamValueType::String
                                                            : TParamValueType::Float;
  param.preferredWidget = param.valueType == TParamValueType::Bool
                              ? TParamWidgetHint::Toggle
                              : param.valueType == TParamValueType::String
                                    ? TParamWidgetHint::Text
                                    : TParamWidgetHint::Slider;
  param.isDiscrete = param.valueType == TParamValueType::Bool ||
                     param.valueType == TParamValueType::Int;
  param.preferredBindingKey = key;
  param.exportSymbol = key;
}

std::vector<TTeulExposedParam> buildUiFallbackExposedParams(
    const TNode &node, const juce::String &displayName,
    const juce::String &typeKey, const std::vector<TParamSpec> &paramSpecs) {
  std::vector<TTeulExposedParam> result;
  std::set<juce::String> seenKeys;

  const juce::String nodeName =
      node.label.isNotEmpty() ? node.label : displayName;

  result.reserve(paramSpecs.size() + node.params.size());
  for (const auto &spec : paramSpecs) {
    if (!spec.exposeToIeum)
      continue;

    TTeulExposedParam param;
    param.paramId = makeTeulParamId(node.nodeId, spec.key);
    param.nodeId = node.nodeId;
    param.nodeTypeKey = typeKey;
    param.nodeDisplayName = nodeName;
    param.paramKey = spec.key;
    param.paramLabel = spec.label.isNotEmpty() ? spec.label : spec.key;
    param.defaultValue = spec.defaultValue;

    if (const auto it = node.params.find(spec.key); it != node.params.end())
      param.currentValue = it->second;
    else
      param.currentValue = spec.defaultValue;

    param.valueType = spec.valueType;
    param.minValue = spec.minValue;
    param.maxValue = spec.maxValue;
    param.step = spec.step;
    param.unitLabel = spec.unitLabel;
    param.displayPrecision = spec.displayPrecision;
    param.group = spec.group;
    param.description = spec.description;
    param.enumOptions = spec.enumOptions;
    param.preferredWidget = spec.preferredWidget;
    param.showInNodeBody = spec.showInNodeBody;
    param.showInPropertyPanel = spec.showInPropertyPanel;
    param.isReadOnly = spec.isReadOnly;
    param.isAutomatable = spec.isAutomatable;
    param.isModulatable = spec.isModulatable;
    param.isDiscrete = spec.isDiscrete;
    param.exposeToIeum = spec.exposeToIeum;
    param.preferredBindingKey = spec.preferredBindingKey;
    param.exportSymbol = spec.exportSymbol;
    param.categoryPath = spec.categoryPath;

    result.push_back(std::move(param));
    seenKeys.insert(spec.key);
  }

  for (const auto &[key, value] : node.params) {
    if (seenKeys.find(key) != seenKeys.end())
      continue;

    TTeulExposedParam param;
    param.paramId = makeTeulParamId(node.nodeId, key);
    param.nodeId = node.nodeId;
    param.nodeTypeKey = typeKey;
    param.nodeDisplayName = nodeName;
    param.paramKey = key;
    param.paramLabel = key;
    param.defaultValue = value;
    param.currentValue = value;
    applyUiFallbackMetadata(param, value, key);
    result.push_back(std::move(param));
  }

  return result;
}

} // namespace

juce::String extractLibraryDragTypeKey(const juce::var &description) {
  const auto descriptionText = description.toString();
  const auto text = juce::String::fromUTF8(descriptionText.toRawUTF8());
  if (!text.startsWith(kLibraryDragPrefix))
    return {};

  const auto typeKey = text.fromFirstOccurrenceOf(kLibraryDragPrefix, false, false);
  return juce::String::fromUTF8(typeKey.toRawUTF8());
}

const TNodeDescriptor *
TGraphCanvas::findDescriptorByTypeKey(const juce::String &typeKey) const noexcept {
  for (const auto &desc : nodeDescriptors) {
    if (desc.typeKey == typeKey)
      return &desc;
  }
  return nullptr;
}

std::vector<TTeulExposedParam>
TGraphCanvas::listExposedParamsForNode(const TNode &node) const {
  if (const auto *desc = findDescriptorByTypeKey(node.typeKey)) {
    if (desc->exposedParamFactory)
      return desc->exposedParamFactory(node);

    return buildUiFallbackExposedParams(node, desc->displayName, desc->typeKey,
                                        desc->paramSpecs);
  }

  return buildUiFallbackExposedParams(node, node.typeKey, node.typeKey, {});
}

std::vector<const TNodeDescriptor *> TGraphCanvas::getAllNodeDescriptors() const {
  std::vector<const TNodeDescriptor *> result;
  result.reserve(nodeDescriptors.size());
  for (const auto &desc : nodeDescriptors)
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
    const TNodeDescriptor *desc = findDescriptorByTypeKey(node.typeKey);

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
