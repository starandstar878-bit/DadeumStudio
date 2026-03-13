#include "Teul2/Editor/Search/SearchController.h"

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

#include "Teul2/Editor/Search/SearchController.h"

#include "Teul/History/TCommands.h"

#include <algorithm>
#include <limits>
#include <set>

namespace Teul {
namespace {

bool endpointsEqual(const TEndpoint &lhs, const TEndpoint &rhs) {
  return lhs.ownerKind == rhs.ownerKind && lhs.nodeId == rhs.nodeId &&
         lhs.portId == rhs.portId && lhs.railEndpointId == rhs.railEndpointId &&
         lhs.railPortId == rhs.railPortId;
}

bool getBundleEndpointsFor(const TTeulDocument &document, const TEndpoint &endpoint,
                           std::vector<TEndpoint> &endpointsOut) {
  endpointsOut.clear();
  if (endpoint.isNodePort()) {
    const auto *node = document.findNode(endpoint.nodeId);
    if (node == nullptr) return false;

    int anchorIndex = -1;
    for (int i = 0; i < (int)node->ports.size(); ++i) {
      if (node->ports[i].portId == endpoint.portId) {
        anchorIndex = i;
        break;
      }
    }
    if (anchorIndex < 0) return false;

    const auto &anchorPort = node->ports[anchorIndex];
    int startIndex = anchorIndex;
    while (startIndex > 0 &&
           node->ports[startIndex - 1].channelIndex == node->ports[startIndex].channelIndex - 1 &&
           node->ports[startIndex - 1].direction == anchorPort.direction &&
           node->ports[startIndex - 1].dataType == anchorPort.dataType) {
      --startIndex;
    }

    int count = 1;
    while (startIndex + count < (int)node->ports.size() &&
           node->ports[startIndex + count].channelIndex == node->ports[startIndex + count - 1].channelIndex + 1 &&
           node->ports[startIndex + count].direction == anchorPort.direction &&
           node->ports[startIndex + count].dataType == anchorPort.dataType) {
      ++count;
    }

    for (int i = 0; i < count; ++i)
      endpointsOut.push_back(TEndpoint::makeNodePort(endpoint.nodeId, node->ports[startIndex + i].portId));

    return true;
  }

  const auto *railEndpoint = document.controlState.findEndpoint(endpoint.railEndpointId);
  const auto *railPort = document.findSystemRailPort(endpoint.railEndpointId, endpoint.railPortId);
  if (railEndpoint == nullptr || railPort == nullptr)
    return false;

  for (const auto &p : railEndpoint->ports)
    endpointsOut.push_back(TEndpoint::makeRailPort(endpoint.railEndpointId, p.portId));
  return true;
}

bool findFirstMatchingPort(const TNode &node, TPortDirection direction,
                           TPortDataType type, PortId &portIdOut) {
  for (const auto &port : node.ports) {
    if (port.direction == direction && port.dataType == type) {
      portIdOut = port.portId;
      return true;
    }
  }
  return false;
}

bool findFirstMatchingBundle(const TNode &node, TPortDirection direction,
                             TPortDataType type, size_t requiredCount,
                             std::vector<PortId> &portIdsOut) {
  portIdsOut.clear();
  for (size_t i = 0; i < node.ports.size(); ) {
    const auto &p = node.ports[i];
    if (p.direction != direction || p.dataType != type) {
      ++i;
      continue;
    }

    size_t j = i + 1;
    while (j < node.ports.size() &&
           node.ports[j].channelIndex == node.ports[j - 1].channelIndex + 1 &&
           node.ports[j].direction == direction &&
           node.ports[j].dataType == type) {
      ++j;
    }

    if (j - i >= requiredCount) {
      for (size_t k = i; k < j; ++k)
        portIdsOut.push_back(node.ports[k].portId);
      return true;
    }

    i = j;
  }
  return false;
}

} // namespace

bool TGraphCanvas::insertNodeOnConnection(ConnectionId connectionId,
                                          NodeId insertedNodeId) {
  const TConnection *existing = document.findConnection(connectionId);
  TNode *inserted = document.findNode(insertedNodeId);
  if (existing == nullptr || inserted == nullptr)
    return false;

  const TConnection original = *existing;
  const auto sourceType = dataTypeForEndpoint(original.from);
  const auto targetType = dataTypeForEndpoint(original.to);

  std::vector<TEndpoint> sourceEndpoints;
  std::vector<TEndpoint> targetEndpoints;
  std::vector<ConnectionId> connectionsToReplace;

  if (sourceType == TPortDataType::Audio && targetType == TPortDataType::Audio &&
      getBundleEndpointsFor(document, original.from, sourceEndpoints) &&
      getBundleEndpointsFor(document, original.to, targetEndpoints) &&
      sourceEndpoints.size() == targetEndpoints.size() &&
      sourceEndpoints.size() > 1) {

    bool fullBundleConnected = true;
    for (size_t i = 0; i < sourceEndpoints.size(); ++i) {
      bool found = false;
      for (const auto &c : document.connections) {
        if (endpointsEqual(c.from, sourceEndpoints[i]) && endpointsEqual(c.to, targetEndpoints[i])) {
          connectionsToReplace.push_back(c.connectionId);
          found = true;
          break;
        }
      }
      if (!found) {
        fullBundleConnected = false;
        break;
      }
    }

    if (!fullBundleConnected) {
      sourceEndpoints = {original.from};
      targetEndpoints = {original.to};
      connectionsToReplace = {original.connectionId};
    }
  } else {
    sourceEndpoints = {original.from};
    targetEndpoints = {original.to};
    connectionsToReplace = {original.connectionId};
  }

  std::vector<PortId> inputPortIds;
  std::vector<PortId> outputPortIds;

  if (sourceEndpoints.size() > 1) {
    if (!findFirstMatchingBundle(*inserted, TPortDirection::Input,
                                 sourceType, sourceEndpoints.size(), inputPortIds) ||
        !findFirstMatchingBundle(*inserted, TPortDirection::Output,
                                 targetType, targetEndpoints.size(), outputPortIds)) {
      return false;
    }
  } else {
    PortId inPortId = kInvalidPortId;
    PortId outPortId = kInvalidPortId;
    if (!findFirstMatchingPort(*inserted, TPortDirection::Input, sourceType,
                               inPortId) ||
        !findFirstMatchingPort(*inserted, TPortDirection::Output, targetType,
                               outPortId)) {
      return false;
    }
    inputPortIds.push_back(inPortId);
    outputPortIds.push_back(outPortId);
  }

  if (inputPortIds.size() != sourceEndpoints.size() ||
      outputPortIds.size() != targetEndpoints.size()) {
    return false;
  }

  for (const auto oldConnectionId : connectionsToReplace) {
    document.executeCommand(std::make_unique<DeleteConnectionCommand>(oldConnectionId));
  }

  ConnectionId lastConnectionId = kInvalidConnectionId;
  for (size_t index = 0; index < sourceEndpoints.size(); ++index) {
    TConnection left;
    left.connectionId = document.allocConnectionId();
    left.from = sourceEndpoints[index];
    left.to = TEndpoint::makeNodePort(insertedNodeId, inputPortIds[index]);
    document.executeCommand(std::make_unique<AddConnectionCommand>(left));
    lastConnectionId = left.connectionId;
  }

  for (size_t index = 0; index < targetEndpoints.size(); ++index) {
    TConnection right;
    right.connectionId = document.allocConnectionId();
    right.from = TEndpoint::makeNodePort(insertedNodeId, outputPortIds[index]);
    right.to = targetEndpoints[index];
    document.executeCommand(std::make_unique<AddConnectionCommand>(right));
    lastConnectionId = right.connectionId;
  }

  selectedConnectionId = lastConnectionId;
  return true;
}

bool TGraphCanvas::addNodeByTypeAtView(const juce::String &typeKey,
                                       juce::Point<float> viewPos,
                                       bool tryInsertOnWire) {
  if (nodeDescriptors.empty())
    return false;

  const TNodeDescriptor *desc = findDescriptorByTypeKey(typeKey);
  if (desc == nullptr)
    return false;

  TNode node;
  node.nodeId = document.allocNodeId();
  node.typeKey = desc->typeKey;

  const auto world = viewToWorld(viewPos);
  node.x = world.x;
  node.y = world.y;

  for (const auto &param : desc->paramSpecs)
    node.params[param.key] = param.defaultValue;

  for (const auto &portSpec : desc->portSpecs) {
    for (int ch = 0; ch < portSpec.channelCount; ++ch) {
      TPort port;
      port.portId = document.allocPortId();
      port.direction = portSpec.direction;
      port.dataType = portSpec.dataType;
      // ?ㅼ쨷 梨꾨꼸?대㈃ 媛?梨꾨꼸??吏?뺣맂 ?대쫫???ъ슜, 紐⑤끂硫??⑥씪 ?대쫫 ?ъ슜
      if (portSpec.channelCount > 1 && ch < (int)portSpec.channelNames.size()) {
        port.name = portSpec.channelNames[ch];
      } else {
        port.name = portSpec.name;
      }
      port.ownerNodeId = node.nodeId;
      port.channelIndex = ch; // Bus ?대??먯꽌??梨꾨꼸 ?쒕쾲 湲곕줉
      port.maxIncomingConnections = portSpec.maxIncomingConnections;
      port.maxOutgoingConnections = portSpec.maxOutgoingConnections;
      node.ports.push_back(port);
    }
  }

  document.executeCommand(std::make_unique<AddNodeCommand>(node));
  rememberRecentNode(desc->typeKey);

  if (tryInsertOnWire) {
    const ConnectionId hit = hitTestConnection(viewPos, 10.0f);
    if (hit != kInvalidConnectionId)
      insertNodeOnConnection(hit, node.nodeId);
  }

  rebuildNodeComponents();
  pushStatusHint("Added " + desc->displayName + ".");
  return true;
}

void TGraphCanvas::showQuickAddPrompt(juce::Point<float> pointView) {
  quickAddAnchorView = pointView;
  if (searchOverlay == nullptr)
    return;

  searchOverlay->present(
      "Quick Add", "Type to filter nodes...",
      [this, pointView](const juce::String &query) {
        struct Candidate {
          const TNodeDescriptor *desc = nullptr;
          int score = std::numeric_limits<int>::min();
        };

        std::vector<Candidate> candidates;
        for (const auto *desc : getAllNodeDescriptors()) {
          if (desc == nullptr)
            continue;

          const int score = scoreDescriptorMatch(*desc, query);
          if (score == std::numeric_limits<int>::min())
            continue;

          candidates.push_back({desc, score});
        }

        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate &a, const Candidate &b) {
                           if (a.score != b.score)
                             return a.score > b.score;
                           if (a.desc->category != b.desc->category)
                             return a.desc->category < b.desc->category;
                           return a.desc->displayName < b.desc->displayName;
                         });

        if (candidates.size() > 40)
          candidates.resize(40);

        std::vector<SearchEntry> entries;
        entries.reserve(candidates.size());
        for (const auto &candidate : candidates) {
          SearchEntry entry;
          entry.title = candidate.desc->displayName;
          entry.subtitle = candidate.desc->category + " / " + candidate.desc->typeKey;
          if (std::find(recentNodeTypes.begin(), recentNodeTypes.end(),
                        candidate.desc->typeKey) != recentNodeTypes.end()) {
            entry.subtitle = "Recent / " + entry.subtitle;
          }

          const juce::String typeKey = candidate.desc->typeKey;
          entry.onSelect = [this, typeKey, pointView] {
            addNodeByTypeAtView(typeKey, pointView, true);
          };
          entries.push_back(std::move(entry));
        }

        return entries;
      },
      true, pointView);
}

} // namespace Teul

#include "Teul2/Editor/Search/SearchController.h"

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
          const auto *desc = findDescriptorByTypeKey(node.typeKey);
          Candidate candidate;
          candidate.nodeId = node.nodeId;
          candidate.title = node.label.isNotEmpty()
                                ? node.label
                                : (desc != nullptr && desc->displayName.isNotEmpty()
                                       ? desc->displayName
                                       : node.typeKey);
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

#include "Teul2/Editor/Search/SearchController.h"

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
        addCommand(isRuntimeHeatmapEnabled() ? "Hide Heatmap" : "Show Heatmap",
                   "Toggle node CPU heat tint",
                   [this] { setRuntimeHeatmapEnabled(!isRuntimeHeatmapEnabled()); });
        addCommand(isLiveProbeEnabled() ? "Hide Live Probe" : "Show Live Probe",
                   "Toggle selected-node probe strips",
                   [this] { setLiveProbeEnabled(!isLiveProbeEnabled()); });
        addCommand(isDebugOverlayEnabled() ? "Hide Runtime Overlay"
                                           : "Show Runtime Overlay",
                   "Toggle runtime debug card on the canvas",
                   [this] { setDebugOverlayEnabled(!isDebugOverlayEnabled()); });
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
