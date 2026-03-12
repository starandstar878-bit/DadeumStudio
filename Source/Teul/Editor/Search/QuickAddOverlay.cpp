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

bool splitStereoLabel(juce::StringRef name, bool &isLeft, juce::String &suffix) {
  const juce::String trimmed = juce::String(name).trim();
  if (trimmed.equalsIgnoreCase("L")) {
    isLeft = true;
    suffix.clear();
    return true;
  }
  if (trimmed.equalsIgnoreCase("R")) {
    isLeft = false;
    suffix.clear();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("L ")) {
    isLeft = true;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("R ")) {
    isLeft = false;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  return false;
}

bool endpointStereoInfo(const TGraphDocument &document, const TEndpoint &endpoint,
                        bool &isLeft, juce::String &suffix,
                        TPortDataType &dataTypeOut,
                        TPortDirection &directionOut) {
  if (endpoint.isNodePort()) {
    const auto *node = document.findNode(endpoint.nodeId);
    const auto *port = node != nullptr ? node->findPort(endpoint.portId) : nullptr;
    if (port == nullptr)
      return false;
    dataTypeOut = port->dataType;
    directionOut = port->direction;
    if (port->dataType != TPortDataType::Audio)
      return false;
    return splitStereoLabel(port->name, isLeft, suffix);
  }

  const auto *railEndpoint = document.controlState.findEndpoint(endpoint.railEndpointId);
  const auto *railPort = document.findSystemRailPort(endpoint.railEndpointId,
                                                     endpoint.railPortId);
  if (railEndpoint == nullptr || railPort == nullptr) {
    return false;
  }

  dataTypeOut = railPort->dataType;
  directionOut = railEndpoint->railId == "output-rail" ? TPortDirection::Input
                                                        : TPortDirection::Output;
  if (!railEndpoint->stereo || railPort->dataType != TPortDataType::Audio)
    return false;

  if (railEndpoint->ports.size() < 2)
    return false;

  if (railEndpoint->ports[0].portId == endpoint.railPortId) {
    isLeft = true;
    suffix.clear();
    return true;
  }
  if (railEndpoint->ports[1].portId == endpoint.railPortId) {
    isLeft = false;
    suffix.clear();
    return true;
  }
  return false;
}

bool findStereoSiblingEndpoint(const TGraphDocument &document,
                               const TEndpoint &endpoint,
                               TEndpoint &siblingOut) {
  bool isLeft = false;
  juce::String suffix;
  TPortDataType dataType = TPortDataType::Audio;
  TPortDirection direction = TPortDirection::Input;
  if (!endpointStereoInfo(document, endpoint, isLeft, suffix, dataType, direction))
    return false;

  if (endpoint.isNodePort()) {
    const auto *node = document.findNode(endpoint.nodeId);
    if (node == nullptr)
      return false;

    for (const auto &candidate : node->ports) {
      if (candidate.portId == endpoint.portId || candidate.dataType != dataType ||
          candidate.direction != direction) {
        continue;
      }

      bool candidateIsLeft = false;
      juce::String candidateSuffix;
      if (!splitStereoLabel(candidate.name, candidateIsLeft, candidateSuffix) ||
          candidateIsLeft == isLeft || candidateSuffix != suffix) {
        continue;
      }

      siblingOut = TEndpoint::makeNodePort(endpoint.nodeId, candidate.portId);
      return true;
    }
    return false;
  }

  const auto *railEndpoint = document.controlState.findEndpoint(endpoint.railEndpointId);
  if (railEndpoint == nullptr || railEndpoint->ports.size() < 2)
    return false;

  siblingOut = TEndpoint::makeRailPort(
      endpoint.railEndpointId,
      isLeft ? railEndpoint->ports[1].portId : railEndpoint->ports[0].portId);
  return true;
}

bool findConnectionByEndpoints(const TGraphDocument &document,
                               const TEndpoint &from, const TEndpoint &to,
                               TConnection &connectionOut) {
  for (const auto &connection : document.connections) {
    if (endpointsEqual(connection.from, from) && endpointsEqual(connection.to, to)) {
      connectionOut = connection;
      return true;
    }
  }
  return false;
}

std::vector<TEndpoint> orderedStereoEndpoints(const TGraphDocument &document,
                                              const TEndpoint &first,
                                              const TEndpoint &second) {
  bool isLeft = false;
  juce::String suffix;
  TPortDataType dataType = TPortDataType::Audio;
  TPortDirection direction = TPortDirection::Input;
  if (endpointStereoInfo(document, first, isLeft, suffix, dataType, direction) &&
      !isLeft) {
    return {second, first};
  }

  return {first, second};
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

bool findFirstMatchingStereoBundle(const TNode &node, TPortDirection direction,
                                   TPortDataType type,
                                   std::vector<PortId> &portIdsOut) {
  if (type != TPortDataType::Audio)
    return false;

  for (const auto &port : node.ports) {
    if (port.direction != direction || port.dataType != type)
      continue;

    bool isLeft = false;
    juce::String suffix;
    if (!splitStereoLabel(port.name, isLeft, suffix))
      continue;

    for (const auto &candidate : node.ports) {
      if (candidate.portId == port.portId || candidate.direction != direction ||
          candidate.dataType != type) {
        continue;
      }

      bool candidateIsLeft = false;
      juce::String candidateSuffix;
      if (!splitStereoLabel(candidate.name, candidateIsLeft, candidateSuffix) ||
          candidateIsLeft == isLeft || candidateSuffix != suffix) {
        continue;
      }

      portIdsOut.clear();
      if (isLeft) {
        portIdsOut.push_back(port.portId);
        portIdsOut.push_back(candidate.portId);
      } else {
        portIdsOut.push_back(candidate.portId);
        portIdsOut.push_back(port.portId);
      }
      return true;
    }
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

  std::vector<TEndpoint> sourceEndpoints{original.from};
  std::vector<TEndpoint> targetEndpoints{original.to};
  std::vector<ConnectionId> connectionsToReplace{original.connectionId};

  TEndpoint fromSibling;
  TEndpoint toSibling;
  TConnection companionConnection;
  if (sourceType == TPortDataType::Audio && targetType == TPortDataType::Audio &&
      findStereoSiblingEndpoint(document, original.from, fromSibling) &&
      findStereoSiblingEndpoint(document, original.to, toSibling) &&
      findConnectionByEndpoints(document, fromSibling, toSibling,
                               companionConnection)) {
    sourceEndpoints = orderedStereoEndpoints(document, original.from, fromSibling);
    targetEndpoints = orderedStereoEndpoints(document, original.to, toSibling);
    connectionsToReplace.push_back(companionConnection.connectionId);
  }

  std::vector<PortId> inputPortIds;
  std::vector<PortId> outputPortIds;

  if (sourceEndpoints.size() > 1) {
    if (!findFirstMatchingStereoBundle(*inserted, TPortDirection::Input,
                                       sourceType, inputPortIds) ||
        !findFirstMatchingStereoBundle(*inserted, TPortDirection::Output,
                                       targetType, outputPortIds)) {
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
