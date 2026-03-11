#include "Teul/Editor/Search/SearchController.h"

#include "Teul/History/TCommands.h"

#include <algorithm>
#include <set>

namespace Teul {

bool TGraphCanvas::insertNodeOnConnection(ConnectionId connectionId,
                                          NodeId insertedNodeId) {
  const TConnection *existing = document.findConnection(connectionId);
  TNode *inserted = document.findNode(insertedNodeId);
  if (existing == nullptr || inserted == nullptr)
    return false;

  const TConnection original = *existing;
  const TPort *sourcePort = findPortModel(original.from.nodeId, original.from.portId);
  if (sourcePort == nullptr)
    return false;

  PortId inPortId = kInvalidPortId;
  PortId outPortId = kInvalidPortId;

  for (const auto &port : inserted->ports) {
    if (port.direction == TPortDirection::Input &&
        port.dataType == sourcePort->dataType) {
      inPortId = port.portId;
      break;
    }
  }

  for (const auto &port : inserted->ports) {
    if (port.direction == TPortDirection::Output &&
        port.dataType == sourcePort->dataType) {
      outPortId = port.portId;
      break;
    }
  }

  if (inPortId == kInvalidPortId || outPortId == kInvalidPortId)
    return false;

  document.executeCommand(std::make_unique<DeleteConnectionCommand>(connectionId));

  TConnection left;
  left.connectionId = document.allocConnectionId();
  left.from = original.from;
  left.to = {insertedNodeId, inPortId};
  document.executeCommand(std::make_unique<AddConnectionCommand>(left));

  TConnection right;
  right.connectionId = document.allocConnectionId();
  right.from = {insertedNodeId, outPortId};
  right.to = original.to;
  document.executeCommand(std::make_unique<AddConnectionCommand>(right));

  selectedConnectionId = right.connectionId;
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
    TPort port;
    port.portId = document.allocPortId();
    port.direction = portSpec.direction;
    port.dataType = portSpec.dataType;
    port.name = portSpec.name;
    port.ownerNodeId = node.nodeId;
    port.maxIncomingConnections = portSpec.maxIncomingConnections;
    port.maxOutgoingConnections = portSpec.maxOutgoingConnections;
    node.ports.push_back(port);
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
