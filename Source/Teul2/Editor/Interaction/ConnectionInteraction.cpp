#include "Teul2/Editor/Canvas/TGraphCanvas.h"

#include "Teul2/Editor/Search/SearchController.h"
#include "Teul2/Editor/Renderers/TNodeRenderer.h"
#include "Teul2/Editor/Renderers/TPortRenderer.h"
#include "Teul2/Document/TDocumentHistory.h"

namespace Teul {
namespace {

constexpr auto kRailBundlePortPrefix = "bundle:";

bool splitRailPortZoneId(const juce::String &zoneId,
                         juce::String &endpointId,
                         juce::String &portId) {
  const int separatorIndex = zoneId.indexOf("::");
  if (separatorIndex <= 0)
    return false;

  endpointId = zoneId.substring(0, separatorIndex);
  portId = zoneId.substring(separatorIndex + 2);
  return endpointId.isNotEmpty() && portId.isNotEmpty();
}

bool isRailBundlePortToken(juce::StringRef token) {
  return juce::String(token).startsWith(kRailBundlePortPrefix);
}

juce::StringArray railPortIdsFromToken(juce::StringRef token) {
  const juce::String text(token);
  if (!isRailBundlePortToken(text))
    return {text};

  return juce::StringArray::fromTokens(
      text.substring((int)juce::String(kRailBundlePortPrefix).length()), "|", {});
}

bool externalZoneContains(const TGraphCanvas::ExternalDropZone &zone,
                          juce::Point<float> pointView) {
  if (!zone.elliptical)
    return zone.boundsView.contains(pointView);

  const auto bounds = zone.boundsView;
  const auto radiusX = bounds.getWidth() * 0.5f;
  const auto radiusY = bounds.getHeight() * 0.5f;
  if (radiusX <= 0.0f || radiusY <= 0.0f)
    return false;

  const auto dx = (pointView.x - bounds.getCentreX()) / radiusX;
  const auto dy = (pointView.y - bounds.getCentreY()) / radiusY;
  return dx * dx + dy * dy <= 1.0f;
}

bool buildNodeBundleEndpoints(const TTeulDocument &document, NodeId nodeId,
                              PortId anchorPortId, int channelCount,
                              std::vector<TEndpoint> &endpointsOut) {
  endpointsOut.clear();
  if (channelCount <= 0)
    return false;

  const auto *node = document.findNode(nodeId);
  if (node == nullptr)
    return false;

  int anchorIndex = -1;
  for (int i = 0; i < (int)node->ports.size(); ++i) {
    if (node->ports[i].portId == anchorPortId) {
      anchorIndex = i;
      break;
    }
  }
  if (anchorIndex < 0)
    return false;

  const auto &anchorPort = node->ports[anchorIndex];

  if (channelCount == 1) {
    endpointsOut.push_back(TEndpoint::makeNodePort(nodeId, anchorPortId));
    return true;
  }

  int startIndex = anchorIndex;
  while (startIndex > 0 &&
         node->ports[startIndex - 1].channelIndex == node->ports[startIndex].channelIndex - 1 &&
         node->ports[startIndex - 1].direction == anchorPort.direction &&
         node->ports[startIndex - 1].dataType == anchorPort.dataType) {
    --startIndex;
  }

  if (startIndex + channelCount > (int)node->ports.size())
    return false;

  for (int i = 0; i < channelCount; ++i) {
    const auto &p = node->ports[startIndex + i];
    if (p.channelIndex != i || p.direction != anchorPort.direction || p.dataType != anchorPort.dataType)
      return false;
    endpointsOut.push_back(TEndpoint::makeNodePort(nodeId, p.portId));
  }
  return true;
}

bool buildRailBundleEndpoints(const TTeulDocument &document,
                              const juce::String &endpointId,
                              const juce::String &portToken,
                              std::vector<TEndpoint> &endpointsOut) {
  endpointsOut.clear();
  const auto portIds = railPortIdsFromToken(portToken);
  if (portIds.isEmpty())
    return false;

  for (const auto &portId : portIds) {
    if (document.findSystemRailPort(endpointId, portId) == nullptr)
      return false;
    endpointsOut.push_back(TEndpoint::makeRailPort(endpointId, portId));
  }

  return true;
}

bool connectionExists(const TTeulDocument &document,
                      const TEndpoint &from,
                      const TEndpoint &to) {
  return std::any_of(document.connections.begin(), document.connections.end(),
                     [&](const TConnection &connection) {
                       return connection.from.ownerKind == from.ownerKind &&
                              connection.from.nodeId == from.nodeId &&
                              connection.from.portId == from.portId &&
                              connection.from.railEndpointId ==
                                  from.railEndpointId &&
                              connection.from.railPortId == from.railPortId &&
                              connection.to.ownerKind == to.ownerKind &&
                              connection.to.nodeId == to.nodeId &&
                              connection.to.portId == to.portId &&
                              connection.to.railEndpointId == to.railEndpointId &&
                              connection.to.railPortId == to.railPortId;
                     });
}

bool endpointsEqual(const TEndpoint &lhs, const TEndpoint &rhs) {
  return lhs.ownerKind == rhs.ownerKind && lhs.nodeId == rhs.nodeId &&
         lhs.portId == rhs.portId && lhs.railEndpointId == rhs.railEndpointId &&
         lhs.railPortId == rhs.railPortId;
}

int endpointIncomingCapacity(const TTeulDocument &document,
                           const TEndpoint &target) {
  if (target.isNodePort()) {
    const auto *node = document.findNode(target.nodeId);
    const auto *port = node != nullptr ? node->findPort(target.portId) : nullptr;
    if (port == nullptr)
      return 1;
    return port->maxIncomingConnections < 0 ? std::numeric_limits<int>::max()
                                            : port->maxIncomingConnections;
  }

  return 1;
}

int endpointIncomingConnectionCount(const TTeulDocument &document,
                                   const TEndpoint &target) {
  int count = 0;
  for (const auto &connection : document.connections) {
    if (endpointsEqual(connection.to, target))
      ++count;
  }
  return count;
}

int endpointOutgoingCapacity(const TTeulDocument &document,
                             const TEndpoint &source) {
  if (source.isNodePort()) {
    const auto *node = document.findNode(source.nodeId);
    const auto *port = node != nullptr ? node->findPort(source.portId) : nullptr;
    if (port == nullptr)
      return std::numeric_limits<int>::max();
    return port->maxOutgoingConnections < 0
               ? std::numeric_limits<int>::max()
               : port->maxOutgoingConnections;
  }

  return std::numeric_limits<int>::max();
}

int endpointOutgoingConnectionCount(const TTeulDocument &document,
                                    const TEndpoint &source) {
  int count = 0;
  for (const auto &connection : document.connections) {
    if (endpointsEqual(connection.from, source))
      ++count;
  }
  return count;
}

bool endpointHasIncomingConnection(const TTeulDocument &document,
                                   const TEndpoint &target) {
  return endpointIncomingConnectionCount(document, target) > 0;
}

enum class EndpointVectorRejectReason {
  none,
  duplicateConnection,
  sourceAtCapacity,
  targetAtCapacity,
  overlappingTargets,
};

EndpointVectorRejectReason
endpointVectorRejectReason(const TTeulDocument &document,
                           const std::vector<TEndpoint> &fromEndpoints,
                           const std::vector<TEndpoint> &toEndpoints) {
  if (fromEndpoints.empty() || fromEndpoints.size() != toEndpoints.size())
    return EndpointVectorRejectReason::targetAtCapacity;

  for (size_t index = 0; index < fromEndpoints.size(); ++index) {
    if (connectionExists(document, fromEndpoints[index], toEndpoints[index]))
      return EndpointVectorRejectReason::duplicateConnection;
    const int outgoingCount =
        endpointOutgoingConnectionCount(document, fromEndpoints[index]);
    const int outgoingCapacity =
        endpointOutgoingCapacity(document, fromEndpoints[index]);
    if (outgoingCount + 1 > outgoingCapacity)
      return EndpointVectorRejectReason::sourceAtCapacity;
    const int incomingCount =
        endpointIncomingConnectionCount(document, toEndpoints[index]);
    const int incomingCapacity =
        endpointIncomingCapacity(document, toEndpoints[index]);
    if (incomingCount + 1 > incomingCapacity)
      return EndpointVectorRejectReason::targetAtCapacity;
  }

  for (size_t i = 0; i < toEndpoints.size(); ++i) {
    for (size_t j = i + 1; j < toEndpoints.size(); ++j) {
      if (endpointsEqual(toEndpoints[i], toEndpoints[j]))
        return EndpointVectorRejectReason::overlappingTargets;
    }
  }

  return EndpointVectorRejectReason::none;
}

bool canConnectEndpointVectors(const TTeulDocument &document,
                               const std::vector<TEndpoint> &fromEndpoints,
                               const std::vector<TEndpoint> &toEndpoints) {
  return endpointVectorRejectReason(document, fromEndpoints, toEndpoints) ==
         EndpointVectorRejectReason::none;
}

juce::String connectionRejectHint(EndpointVectorRejectReason reason,
                                  bool targetIsExternal) {
  switch (reason) {
  case EndpointVectorRejectReason::duplicateConnection:
    return "That connection already exists.";
  case EndpointVectorRejectReason::sourceAtCapacity:
    return "That output is already at its fan-out limit.";
  case EndpointVectorRejectReason::targetAtCapacity:
    return targetIsExternal
               ? "That rail target is already occupied or at capacity."
               : "That input is already occupied or at capacity.";
  case EndpointVectorRejectReason::overlappingTargets:
    return "That bus body overlaps occupied target channels.";
  case EndpointVectorRejectReason::none:
    break;
  }
  return {};
}

bool isBundleDrag(int sourceBundleCount) {
  return sourceBundleCount > 1;
}

bool isNodePortHitCompatible(int sourceBundleCount, int targetPortCount,
                             const TPortComponent::HitResult &hit) {
  if (!hit.hit)
    return false;

  if (!isBundleDrag(sourceBundleCount)) {
    if (targetPortCount <= 1)
      return true;
    return !hit.bundle;
  }

  return hit.bundle && targetPortCount == sourceBundleCount;
}

TGraphCanvas::DragRejectReason
nodePortRejectReason(int sourceBundleCount, int targetPortCount,
                     const TPortComponent::HitResult &hit) {
  if (!hit.hit)
    return TGraphCanvas::DragRejectReason::none;

  if (!isBundleDrag(sourceBundleCount)) {
    if (targetPortCount > 1 && hit.bundle)
      return TGraphCanvas::DragRejectReason::useBusChannel;
    return TGraphCanvas::DragRejectReason::none;
  }

  if (targetPortCount <= 1)
    return TGraphCanvas::DragRejectReason::bundleRequiresMatchingBus;
  if (!hit.bundle)
    return TGraphCanvas::DragRejectReason::useBusBody;
  if (targetPortCount != sourceBundleCount)
    return TGraphCanvas::DragRejectReason::bundleSizeMismatch;
  return TGraphCanvas::DragRejectReason::none;
}

bool isExternalZoneCompatible(int sourceBundleCount, int zoneChannelCount,
                              bool zoneIsBundle) {
  if (!isBundleDrag(sourceBundleCount))
    return !(zoneChannelCount > 1 && zoneIsBundle);

  return zoneIsBundle && zoneChannelCount == sourceBundleCount;
}

TGraphCanvas::DragRejectReason
externalZoneRejectReason(int sourceBundleCount, int zoneChannelCount,
                         bool zoneIsBundle) {
  if (!isBundleDrag(sourceBundleCount)) {
    if (zoneChannelCount > 1 && zoneIsBundle)
      return TGraphCanvas::DragRejectReason::useBusChannel;
    return TGraphCanvas::DragRejectReason::none;
  }

  if (zoneChannelCount <= 1)
    return TGraphCanvas::DragRejectReason::bundleRequiresMatchingBus;
  if (!zoneIsBundle)
    return TGraphCanvas::DragRejectReason::useBusBody;
  if (zoneChannelCount != sourceBundleCount)
    return TGraphCanvas::DragRejectReason::bundleSizeMismatch;
  return TGraphCanvas::DragRejectReason::none;
}

bool isOutputRailZone(const TTeulDocument &document, const juce::String &zoneId) {
  juce::String endpointId;
  juce::String portToken;
  if (!splitRailPortZoneId(zoneId, endpointId, portToken))
    return false;

  const auto *endpoint = document.controlState.findEndpoint(endpointId);
  if (endpoint == nullptr || endpoint->railId != "output-rail")
    return false;

  std::vector<TEndpoint> endpoints;
  return buildRailBundleEndpoints(document, endpointId, portToken, endpoints);
}

} // namespace

void TGraphCanvas::clearDragTargetHighlight() {
  for (auto &node : nodeComponents) {
    for (const auto &port : node->getInputPorts()) {
      port->setDragTargetHighlight(false, true, kInvalidPortId, false);
    }
  }
}

void TGraphCanvas::updateDragTargetFromMouse(juce::Point<float> mousePosView) {
  const auto notifyExternalDragTarget = [this]() {
    if (externalDragTargetChangedHandler == nullptr)
      return;

    externalDragTargetChangedHandler(
        wireDragState.targetExternalZoneId,
        wireDragState.targetExternalZoneId.isNotEmpty() &&
            isCurrentDragTargetConnectable());
  };

  clearDragTargetHighlight();

  wireDragState.targetNodeId = kInvalidNodeId;
  wireDragState.targetPortId = kInvalidPortId;
  wireDragState.targetExternalZoneId.clear();
  wireDragState.targetTypeMatch = false;
  wireDragState.targetCycleFree = false;
  wireDragState.targetBundleCount = 1;
  wireDragState.rejectReason = TGraphCanvas::DragRejectReason::none;

  if (!wireDragState.active) {
    notifyExternalDragTarget();
    return;
  }

  const bool sourceIsExternal = wireDragState.sourceExternalId.isNotEmpty();
  const bool canTargetNodePorts =
      !sourceIsExternal ||
      wireDragState.sourceExternalKind == ExternalDragSourceKind::GraphConnection;
  const auto mousePosInt = mousePosView.roundToInt();

  if (canTargetNodePorts) {
    for (auto &node : nodeComponents) {
      for (const auto &inputPort : node->getInputPorts()) {
        const auto hitArea =
            getLocalArea(inputPort.get(), inputPort->getLocalBounds()).expanded(4);
        if (!hitArea.contains(mousePosInt))
          continue;

        const auto localPoint = inputPort->getLocalPoint(this, mousePosInt).toFloat();
        auto hit = inputPort->hitTestLocal(localPoint);

        const int targetPortCount = (int)inputPort->getPortGroup().size();
        if (!isNodePortHitCompatible(wireDragState.sourceBundleCount,
                                     targetPortCount, hit)) {
          const auto rejectReason =
              nodePortRejectReason(wireDragState.sourceBundleCount,
                                   targetPortCount, hit);
          if (rejectReason != TGraphCanvas::DragRejectReason::none)
            wireDragState.rejectReason = rejectReason;
          continue;
        }

        const TPort *candidatePort =
            findPortModel(node->getNodeId(), hit.portId);
        if (candidatePort == nullptr)
          continue;

        wireDragState.targetNodeId = candidatePort->ownerNodeId;
        wireDragState.targetPortId = candidatePort->portId;
        wireDragState.targetTypeMatch =
            (candidatePort->dataType == wireDragState.sourceType);
        wireDragState.targetCycleFree =
            sourceIsExternal ? true
                             : !document.wouldCreateCycle(
                                   wireDragState.sourceNodeId,
                                   candidatePort->ownerNodeId);
        wireDragState.targetBundleCount = hit.bundle ? hit.channelCount : 1;

        inputPort->setDragTargetHighlight(true, isCurrentDragTargetConnectable(),
                                          hit.portId, hit.bundle);
        notifyExternalDragTarget();
        return;
      }
    }
  }

  if (!externalDropZoneProvider) {
    notifyExternalDragTarget();
    return;
  }

  const auto zones = externalDropZoneProvider();
  const TGraphCanvas::ExternalDropZone *bestZone = nullptr;
  float bestArea = std::numeric_limits<float>::max();

  for (const auto &zone : zones) {
    if (!externalZoneContains(zone, mousePosView))
      continue;

    juce::String endpointId;
    juce::String portToken;
    if (splitRailPortZoneId(zone.zoneId, endpointId, portToken)) {
      const int zoneChannelCount = railPortIdsFromToken(portToken).size();
      const bool zoneIsBundle = isRailBundlePortToken(portToken);
      if (!isExternalZoneCompatible(wireDragState.sourceBundleCount,
                                    zoneChannelCount, zoneIsBundle)) {
        const auto rejectReason =
            externalZoneRejectReason(wireDragState.sourceBundleCount,
                                     zoneChannelCount, zoneIsBundle);
        if (rejectReason != TGraphCanvas::DragRejectReason::none)
          wireDragState.rejectReason = rejectReason;
        continue;
      }
    }

    const float area = zone.boundsView.getWidth() * zone.boundsView.getHeight();
    if (bestZone == nullptr ||
        (zone.elliptical && !bestZone->elliptical) ||
        (zone.elliptical == bestZone->elliptical && area < bestArea)) {
      bestZone = &zone;
      bestArea = area;
    }
  }

  if (bestZone != nullptr) {
    wireDragState.targetExternalZoneId = bestZone->zoneId;
    wireDragState.targetTypeMatch =
        (bestZone->dataType == wireDragState.sourceType);
    wireDragState.targetCycleFree = true;
    notifyExternalDragTarget();
    return;
  }

  notifyExternalDragTarget();
}

juce::String TGraphCanvas::currentDragStatusHint() const {
  if (!wireDragState.active)
    return {};

  const bool sourceIsExternal = wireDragState.sourceExternalId.isNotEmpty();
  if (sourceIsExternal &&
      wireDragState.sourceExternalKind == ExternalDragSourceKind::Assignment) {
    if (wireDragState.targetExternalZoneId.isNotEmpty())
      return isCurrentDragTargetConnectable()
                 ? "Release to create the control assignment."
                 : "This parameter cannot accept the selected control source.";
    return "Drop on a parameter row to create a control assignment.";
  }

  const int sourceCount = juce::jmax(1, wireDragState.sourceBundleCount);
  const int targetCount = juce::jmax(1, wireDragState.targetBundleCount);
  const bool hasTarget = wireDragState.targetExternalZoneId.isNotEmpty() ||
                         (wireDragState.targetNodeId != kInvalidNodeId &&
                          wireDragState.targetPortId != kInvalidPortId);

  if (!hasTarget) {
    switch (wireDragState.rejectReason) {
    case DragRejectReason::useBusBody:
      return "Drop on the bus body to connect the full bundle.";
    case DragRejectReason::useBusChannel:
      return "Drop on a bus channel circle to connect a single channel.";
    case DragRejectReason::bundleSizeMismatch:
      return "Bundle sizes must match on the bus body.";
    case DragRejectReason::bundleRequiresMatchingBus:
      return "Bundles only connect to matching bus bodies.";
    case DragRejectReason::none:
      break;
    }

    if (sourceCount > 1)
      return "Drag to a matching bus body to connect the full bundle.";
    return "Drag to a mono port or a bus channel circle.";
  }

  if (!wireDragState.targetTypeMatch)
    return "Signal types must match.";

  if (!wireDragState.targetCycleFree)
    return "This connection would create a feedback cycle.";

  if (sourceCount > 1 && targetCount != sourceCount)
    return "Bundle sizes must match.";

  if (!isCurrentDragTargetConnectable()) {
    if (wireDragState.sourceExternalId.isNotEmpty()) {
      std::vector<TEndpoint> fromEndpoints;
      std::vector<TEndpoint> toEndpoints;
      if (buildRailBundleEndpoints(document, wireDragState.sourceExternalId,
                                   wireDragState.sourceExternalPortId,
                                   fromEndpoints) &&
          (int)fromEndpoints.size() == targetCount &&
          buildNodeBundleEndpoints(document, wireDragState.targetNodeId,
                                   wireDragState.targetPortId, targetCount,
                                   toEndpoints)) {
        const auto reason =
            endpointVectorRejectReason(document, fromEndpoints, toEndpoints);
        const auto hint = connectionRejectHint(reason, false);
        if (hint.isNotEmpty())
          return hint;
      }
    } else if (wireDragState.targetExternalZoneId.isNotEmpty()) {
      juce::String endpointId;
      juce::String portToken;
      std::vector<TEndpoint> fromEndpoints;
      std::vector<TEndpoint> toEndpoints;
      if (splitRailPortZoneId(wireDragState.targetExternalZoneId, endpointId,
                              portToken) &&
          buildNodeBundleEndpoints(document, wireDragState.sourceNodeId,
                                   wireDragState.sourcePortId, sourceCount,
                                   fromEndpoints) &&
          buildRailBundleEndpoints(document, endpointId, portToken,
                                   toEndpoints)) {
        const auto reason =
            endpointVectorRejectReason(document, fromEndpoints, toEndpoints);
        const auto hint = connectionRejectHint(reason, true);
        if (hint.isNotEmpty())
          return hint;
      }
    } else {
      if (wireDragState.sourceNodeId == wireDragState.targetNodeId &&
          wireDragState.sourcePortId == wireDragState.targetPortId) {
        return "A port cannot connect to itself.";
      }

      std::vector<TEndpoint> fromEndpoints;
      std::vector<TEndpoint> toEndpoints;
      if (sourceCount > 1 || targetCount > 1) {
        if (buildNodeBundleEndpoints(document, wireDragState.sourceNodeId,
                                     wireDragState.sourcePortId, sourceCount,
                                     fromEndpoints) &&
            buildNodeBundleEndpoints(document, wireDragState.targetNodeId,
                                     wireDragState.targetPortId, targetCount,
                                     toEndpoints)) {
          const auto reason =
              endpointVectorRejectReason(document, fromEndpoints, toEndpoints);
          const auto hint = connectionRejectHint(reason, false);
          if (hint.isNotEmpty())
            return hint;
        }
      } else {
        fromEndpoints.push_back(TEndpoint::makeNodePort(
            wireDragState.sourceNodeId, wireDragState.sourcePortId));
        toEndpoints.push_back(TEndpoint::makeNodePort(
            wireDragState.targetNodeId, wireDragState.targetPortId));
        const auto reason =
            endpointVectorRejectReason(document, fromEndpoints, toEndpoints);
        const auto hint = connectionRejectHint(reason, false);
        if (hint.isNotEmpty())
          return hint;
      }
    }

    if (wireDragState.targetExternalZoneId.isNotEmpty())
      return "That rail target is already occupied or at capacity.";
    return "That target is already occupied or at capacity.";
  }

  if (sourceCount > 1)
    return "Release to connect the full bundle.";
  return "Release to connect the channel.";
}

bool TGraphCanvas::isCurrentDragTargetConnectable() const {
  if (!wireDragState.active)
    return false;

  const bool sourceIsNode = wireDragState.sourceNodeId != kInvalidNodeId &&
                            wireDragState.sourcePortId != kInvalidPortId;
  const bool sourceIsExternal = wireDragState.sourceExternalId.isNotEmpty();
  if (!sourceIsNode && !sourceIsExternal)
    return false;

  if (!wireDragState.targetTypeMatch || !wireDragState.targetCycleFree)
    return false;

  if (sourceIsExternal) {
    if (wireDragState.sourceExternalKind == ExternalDragSourceKind::Assignment) {
      return wireDragState.targetExternalZoneId.isNotEmpty() &&
             externalConnectionCommitHandler != nullptr;
    }

    if (wireDragState.targetNodeId == kInvalidNodeId ||
        wireDragState.targetPortId == kInvalidPortId) {
      return false;
    }

    const TPort *targetPort =
        findPortModel(wireDragState.targetNodeId, wireDragState.targetPortId);
    if (targetPort == nullptr || targetPort->direction != TPortDirection::Input)
      return false;

    std::vector<TEndpoint> fromEndpoints;
    std::vector<TEndpoint> toEndpoints;
    if (!buildRailBundleEndpoints(document, wireDragState.sourceExternalId,
                                  wireDragState.sourceExternalPortId,
                                  fromEndpoints)) {
      return false;
    }

    const int targetCount = juce::jmax(1, wireDragState.targetBundleCount);
    if ((int)fromEndpoints.size() != targetCount)
      return false;

    if (!buildNodeBundleEndpoints(document, wireDragState.targetNodeId,
                                  wireDragState.targetPortId, targetCount,
                                  toEndpoints)) {
      return false;
    }
    return canConnectEndpointVectors(document, fromEndpoints, toEndpoints);
  }

  const TPort *sourcePort =
      findPortModel(wireDragState.sourceNodeId, wireDragState.sourcePortId);
  if (!sourcePort || sourcePort->direction != TPortDirection::Output)
    return false;

  const int sourceCount = juce::jmax(1, wireDragState.sourceBundleCount);

  if (wireDragState.targetExternalZoneId.isNotEmpty()) {
    if (!isOutputRailZone(document, wireDragState.targetExternalZoneId))
      return false;

    juce::String endpointId;
    juce::String portToken;
    if (!splitRailPortZoneId(wireDragState.targetExternalZoneId, endpointId,
                             portToken)) {
      return false;
    }

    const int targetCount = railPortIdsFromToken(portToken).size();
    if (targetCount != sourceCount)
      return false;

    std::vector<TEndpoint> fromEndpoints;
    std::vector<TEndpoint> toEndpoints;
    if (!buildNodeBundleEndpoints(document, wireDragState.sourceNodeId,
                                  wireDragState.sourcePortId, sourceCount,
                                  fromEndpoints)) {
      return false;
    }
    if (!buildRailBundleEndpoints(document, endpointId, portToken, toEndpoints))
      return false;
    return canConnectEndpointVectors(document, fromEndpoints, toEndpoints);
  }

  if (wireDragState.targetNodeId == kInvalidNodeId ||
      wireDragState.targetPortId == kInvalidPortId)
    return false;

  if (wireDragState.sourceNodeId == wireDragState.targetNodeId &&
      wireDragState.sourcePortId == wireDragState.targetPortId)
    return false;

  const TPort *targetPort =
      findPortModel(wireDragState.targetNodeId, wireDragState.targetPortId);
  if (!targetPort || targetPort->direction != TPortDirection::Input)
    return false;

  const int targetCount = juce::jmax(1, wireDragState.targetBundleCount);
  if (sourceCount > 1 || targetCount > 1) {
    if (sourceCount != targetCount)
      return false;

    std::vector<TEndpoint> fromEndpoints;
    std::vector<TEndpoint> toEndpoints;
    if (!buildNodeBundleEndpoints(document, wireDragState.sourceNodeId,
                                  wireDragState.sourcePortId, sourceCount,
                                  fromEndpoints)) {
      return false;
    }
    if (!buildNodeBundleEndpoints(document, wireDragState.targetNodeId,
                                  wireDragState.targetPortId, targetCount,
                                  toEndpoints)) {
      return false;
    }
    return canConnectEndpointVectors(document, fromEndpoints, toEndpoints);
  }

  const auto from = TEndpoint::makeNodePort(wireDragState.sourceNodeId,
                                            wireDragState.sourcePortId);
  const auto to = TEndpoint::makeNodePort(wireDragState.targetNodeId,
                                          wireDragState.targetPortId);
  return !connectionExists(document, from, to) &&
         endpointOutgoingConnectionCount(document, from) + 1 <=
             endpointOutgoingCapacity(document, from) &&
         endpointIncomingConnectionCount(document, to) + 1 <=
             endpointIncomingCapacity(document, to);
}

void TGraphCanvas::tryCreateConnectionFromDrag() {
  if (!isCurrentDragTargetConnectable())
    return;

  const auto commitEndpointVectors = [this](const std::vector<TEndpoint> &froms,
                                            const std::vector<TEndpoint> &tos) {
    for (size_t index = 0; index < froms.size() && index < tos.size(); ++index) {
      TConnection conn;
      conn.connectionId = document.allocConnectionId();
      conn.from = froms[index];
      conn.to = tos[index];
      document.executeCommand(createAddConnectionCommand(conn));
      selectedConnectionId = conn.connectionId;
    }
  };

  if (wireDragState.targetExternalZoneId.isNotEmpty()) {
    if (wireDragState.sourceExternalKind == ExternalDragSourceKind::Assignment &&
        wireDragState.sourceExternalId.isNotEmpty()) {
      ExternalDragSource externalSource;
      externalSource.sourceId = wireDragState.sourceExternalId;
      externalSource.sourcePortId = wireDragState.sourceExternalPortId;
      externalSource.dataType = wireDragState.sourceType;
      externalSource.kind = wireDragState.sourceExternalKind;
      if (externalConnectionCommitHandler != nullptr)
        externalConnectionCommitHandler(nullptr, &externalSource,
                                        wireDragState.targetExternalZoneId);
      return;
    }

    juce::String endpointId;
    juce::String portToken;
    if (splitRailPortZoneId(wireDragState.targetExternalZoneId, endpointId,
                            portToken)) {
      std::vector<TEndpoint> fromEndpoints;
      std::vector<TEndpoint> toEndpoints;
      const int targetCount = railPortIdsFromToken(portToken).size();
      if (targetCount == juce::jmax(1, wireDragState.sourceBundleCount) &&
          buildNodeBundleEndpoints(document, wireDragState.sourceNodeId,
                                   wireDragState.sourcePortId,
                                   juce::jmax(1, wireDragState.sourceBundleCount),
                                   fromEndpoints) &&
          buildRailBundleEndpoints(document, endpointId, portToken, toEndpoints)) {
        commitEndpointVectors(fromEndpoints, toEndpoints);
        return;
      }
    }

    if (externalConnectionCommitHandler != nullptr) {
      const TPort *sourcePort =
          findPortModel(wireDragState.sourceNodeId, wireDragState.sourcePortId);
      externalConnectionCommitHandler(sourcePort, nullptr,
                                      wireDragState.targetExternalZoneId);
    }
    return;
  }

  if (wireDragState.sourceExternalId.isNotEmpty()) {
    std::vector<TEndpoint> fromEndpoints;
    std::vector<TEndpoint> toEndpoints;
    if (buildRailBundleEndpoints(document, wireDragState.sourceExternalId,
                                 wireDragState.sourceExternalPortId,
                                 fromEndpoints) &&
        (int)fromEndpoints.size() == juce::jmax(1, wireDragState.targetBundleCount) &&
        buildNodeBundleEndpoints(document, wireDragState.targetNodeId,
                                 wireDragState.targetPortId,
                                 juce::jmax(1, wireDragState.targetBundleCount),
                                 toEndpoints)) {
      commitEndpointVectors(fromEndpoints, toEndpoints);
      return;
    }
  }

  if (juce::jmax(1, wireDragState.sourceBundleCount) > 1 ||
      juce::jmax(1, wireDragState.targetBundleCount) > 1) {
    std::vector<TEndpoint> fromEndpoints;
    std::vector<TEndpoint> toEndpoints;
    if (buildNodeBundleEndpoints(document, wireDragState.sourceNodeId,
                                 wireDragState.sourcePortId,
                                 juce::jmax(1, wireDragState.sourceBundleCount),
                                 fromEndpoints) &&
        buildNodeBundleEndpoints(document, wireDragState.targetNodeId,
                                 wireDragState.targetPortId,
                                 juce::jmax(1, wireDragState.targetBundleCount),
                                 toEndpoints) &&
        fromEndpoints.size() == toEndpoints.size()) {
      commitEndpointVectors(fromEndpoints, toEndpoints);
      return;
    }
  }

  TConnection conn;
  conn.connectionId = document.allocConnectionId();
  if (wireDragState.sourceExternalId.isNotEmpty()) {
    conn.from = TEndpoint::makeRailPort(wireDragState.sourceExternalId,
                                        wireDragState.sourceExternalPortId);
  } else {
    conn.from = TEndpoint::makeNodePort(wireDragState.sourceNodeId,
                                        wireDragState.sourcePortId);
  }
  conn.to = TEndpoint::makeNodePort(wireDragState.targetNodeId,
                                    wireDragState.targetPortId);

  document.executeCommand(createAddConnectionCommand(conn));
  selectedConnectionId = conn.connectionId;
}

void TGraphCanvas::beginConnectionDrag(const TPort &sourcePort,
                                       juce::Point<float> mousePosView,
                                       int bundleChannelCount) {
  auto sourcePosView = mousePosView;
  if (findPortComponent(sourcePort.ownerNodeId, sourcePort.portId) != nullptr)
    sourcePosView = portCentreInCanvas(sourcePort.ownerNodeId, sourcePort.portId);

  beginConnectionDragFromPoint(sourcePort, sourcePosView, mousePosView,
                               bundleChannelCount);
}

void TGraphCanvas::beginConnectionDragFromPoint(
    const TPort &sourcePort, juce::Point<float> sourcePosView,
    juce::Point<float> mousePosView, int bundleChannelCount) {
  if (sourcePort.direction != TPortDirection::Output)
    return;

  wireDragState = WireDragState{};
  wireDragState.active = true;
  wireDragState.sourceNodeId = sourcePort.ownerNodeId;
  wireDragState.sourcePortId = sourcePort.portId;
  wireDragState.sourceType = sourcePort.dataType;
  wireDragState.sourceBundleCount = juce::jmax(1, bundleChannelCount);
  wireDragState.sourcePosView = sourcePosView;
  wireDragState.mousePosView = mousePosView;

  selectedConnectionId = kInvalidConnectionId;

  updateDragTargetFromMouse(mousePosView);
  repaint();
}

void TGraphCanvas::beginExternalConnectionDragFromPoint(
    const ExternalDragSource &source, juce::Point<float> sourcePosView,
    juce::Point<float> mousePosView) {
  if (source.sourceId.isEmpty() || source.sourcePortId.isEmpty())
    return;

  wireDragState = WireDragState{};
  wireDragState.active = true;
  wireDragState.sourceExternalId = source.sourceId;
  wireDragState.sourceExternalPortId = source.sourcePortId;
  wireDragState.sourceExternalKind = source.kind;
  wireDragState.sourceType = source.dataType;
  wireDragState.sourceBundleCount =
      source.kind == ExternalDragSourceKind::GraphConnection
          ? juce::jmax(1, railPortIdsFromToken(source.sourcePortId).size())
          : 1;
  wireDragState.sourcePosView = sourcePosView;
  wireDragState.mousePosView = mousePosView;

  selectedConnectionId = kInvalidConnectionId;

  updateDragTargetFromMouse(mousePosView);
  repaint();
}

void TGraphCanvas::updateConnectionDrag(juce::Point<float> mousePosView) {
  if (!wireDragState.active)
    return;

  wireDragState.mousePosView = mousePosView;
  updateDragTargetFromMouse(mousePosView);
  repaint();
}

void TGraphCanvas::endConnectionDrag(juce::Point<float> mousePosView) {
  if (!wireDragState.active)
    return;

  wireDragState.mousePosView = mousePosView;
  updateDragTargetFromMouse(mousePosView);
  tryCreateConnectionFromDrag();
  cancelConnectionDrag();
}

void TGraphCanvas::cancelConnectionDrag() {
  clearDragTargetHighlight();
  wireDragState = WireDragState{};
  if (externalDragTargetChangedHandler != nullptr)
    externalDragTargetChangedHandler({}, false);
  repaint();
}

void TGraphCanvas::startDisconnectAnimation(juce::Point<float> anchorPosView,
                                            juce::Point<float> loosePosView,
                                            juce::Point<float> impulseView,
                                            TPortDataType type) {
  disconnectAnimation.active = true;
  disconnectAnimation.anchorPosView = anchorPosView;
  disconnectAnimation.loosePosView = loosePosView;
  disconnectAnimation.velocityViewPerSecond = impulseView +
                                              juce::Point<float>(0.0f, 120.0f);
  disconnectAnimation.alpha = 1.0f;
  disconnectAnimation.type = type;
}

void TGraphCanvas::deleteConnectionWithAnimation(ConnectionId connectionId,
                                                 juce::Point<float> breakPointView,
                                                 juce::Point<float> impulseView) {
  const TConnection *conn = document.findConnection(connectionId);
  if (!conn)
    return;

  const auto fromPos = portCentreInCanvas(conn->from);
  const auto toPos = portCentreInCanvas(conn->to);

  const float fromDistSq = fromPos.getDistanceSquaredFrom(breakPointView);
  const float toDistSq = toPos.getDistanceSquaredFrom(breakPointView);

  const juce::Point<float> anchorPos = (fromDistSq <= toDistSq) ? toPos : fromPos;
  const TPortDataType sourceType = dataTypeForEndpoint(conn->from);

  startDisconnectAnimation(anchorPos, breakPointView, impulseView, sourceType);

  document.executeCommand(createDeleteConnectionCommand(connectionId));

  if (selectedConnectionId == connectionId)
    selectedConnectionId = kInvalidConnectionId;

  repaint();
}

bool TGraphCanvas::isInterestedInDragSource(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  return extractLibraryDragTypeKey(dragSourceDetails.description).isNotEmpty();
}

void TGraphCanvas::itemDragEnter(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  itemDragMove(dragSourceDetails);
}

void TGraphCanvas::itemDragMove(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  const juce::String typeKey =
      extractLibraryDragTypeKey(dragSourceDetails.description);
  const auto point = dragSourceDetails.localPosition.toFloat();

  if (typeKey.isEmpty() || !getLocalBounds().contains(point.roundToInt())) {
    itemDragExit(dragSourceDetails);
    return;
  }

  const auto nextTypeKeyUtf8 = typeKey.toStdString();
  const bool previewChanged = !libraryDropPreview.active ||
                              libraryDropPreview.pointView != point ||
                              libraryDropPreview.typeKeyUtf8 != nextTypeKeyUtf8;

  libraryDropPreview.active = true;
  libraryDropPreview.pointView = point;
  libraryDropPreview.typeKeyUtf8 = nextTypeKeyUtf8;

  if (previewChanged)
    repaint();
}

void TGraphCanvas::itemDragExit(
    const juce::DragAndDropTarget::SourceDetails &) {
  if (!libraryDropPreview.active)
    return;

  libraryDropPreview.active = false;
  libraryDropPreview.pointView = {};
  libraryDropPreview.typeKeyUtf8.clear();
  repaint();
}

void TGraphCanvas::itemDropped(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  const juce::String typeKey =
      extractLibraryDragTypeKey(dragSourceDetails.description);
  const auto point = dragSourceDetails.localPosition.toFloat();
  itemDragExit(dragSourceDetails);

  if (typeKey.isEmpty() || !getLocalBounds().contains(point.roundToInt()))
    return;

  addNodeByTypeAtView(typeKey, point, true);
}

} // namespace Teul
