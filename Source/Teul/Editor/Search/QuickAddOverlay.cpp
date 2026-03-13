#include "Teul/Editor/Search/SearchController.h"

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

bool getBundleEndpointsFor(const TGraphDocument &document, const TEndpoint &endpoint,
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
      // 다중 채널이면 각 채널에 지정된 이름을 사용, 모노면 단일 이름 사용
      if (portSpec.channelCount > 1 && ch < (int)portSpec.channelNames.size()) {
        port.name = portSpec.channelNames[ch];
      } else {
        port.name = portSpec.name;
      }
      port.ownerNodeId = node.nodeId;
      port.channelIndex = ch; // Bus 내부에서의 채널 순번 기록
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
