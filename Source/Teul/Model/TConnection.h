#pragma once

#include "TTypes.h"

namespace Teul {

enum class TEndpointOwnerKind {
  NodePort,
  RailPort,
};

struct TEndpoint {
  NodeId nodeId = kInvalidNodeId;
  PortId portId = kInvalidPortId;
  TEndpointOwnerKind ownerKind = TEndpointOwnerKind::NodePort;
  juce::String railEndpointId;
  juce::String railPortId;

  static TEndpoint makeNodePort(NodeId nodeIdIn, PortId portIdIn) {
    TEndpoint endpoint;
    endpoint.nodeId = nodeIdIn;
    endpoint.portId = portIdIn;
    endpoint.ownerKind = TEndpointOwnerKind::NodePort;
    return endpoint;
  }

  static TEndpoint makeRailPort(const juce::String &endpointId,
                                const juce::String &portIdIn) {
    TEndpoint endpoint;
    endpoint.ownerKind = TEndpointOwnerKind::RailPort;
    endpoint.railEndpointId = endpointId;
    endpoint.railPortId = portIdIn;
    return endpoint;
  }

  bool isNodePort() const noexcept {
    return ownerKind == TEndpointOwnerKind::NodePort;
  }

  bool isRailPort() const noexcept {
    return ownerKind == TEndpointOwnerKind::RailPort;
  }

  bool isValid() const noexcept {
    if (isNodePort())
      return nodeId != kInvalidNodeId && portId != kInvalidPortId;

    return railEndpointId.isNotEmpty() && railPortId.isNotEmpty();
  }

  bool referencesNode(NodeId id) const noexcept {
    return isNodePort() && nodeId == id;
  }
};

struct TConnection {
  ConnectionId connectionId = kInvalidConnectionId;
  TEndpoint from;
  TEndpoint to;

  bool isValid() const noexcept {
    return connectionId != kInvalidConnectionId && from.isValid() &&
           to.isValid();
  }
};

} // namespace Teul
