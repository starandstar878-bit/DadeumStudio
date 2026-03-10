#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Search/SearchController.h"
#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/History/TCommands.h"

namespace Teul {
namespace {

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

bool isOutputRailZone(const TGraphDocument &document, const juce::String &zoneId) {
  juce::String endpointId;
  juce::String portId;
  if (!splitRailPortZoneId(zoneId, endpointId, portId))
    return false;

  const auto *endpoint = document.controlState.findEndpoint(endpointId);
  return endpoint != nullptr && endpoint->railId == "output-rail" &&
         document.findSystemRailPort(endpointId, portId) != nullptr;
}

bool connectionExists(const TGraphDocument &document,
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

} // namespace

void TGraphCanvas::clearDragTargetHighlight() {
  for (auto &node : nodeComponents) {
    for (const auto &port : node->getInputPorts()) {
      port->setDragTargetHighlight(false, true);
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

        const auto &candidatePort = inputPort->getPortData();
        wireDragState.targetNodeId = candidatePort.ownerNodeId;
        wireDragState.targetPortId = candidatePort.portId;
        wireDragState.targetTypeMatch =
            (candidatePort.dataType == wireDragState.sourceType);
        wireDragState.targetCycleFree =
            sourceIsExternal ? true
                             : !document.wouldCreateCycle(
                                   wireDragState.sourceNodeId,
                                   candidatePort.ownerNodeId);

        inputPort->setDragTargetHighlight(true, isCurrentDragTargetConnectable());
        notifyExternalDragTarget();
        return;
      }
    }
  }

  if (!externalDropZoneProvider) {
    notifyExternalDragTarget();
    return;
  }

  for (const auto &zone : externalDropZoneProvider()) {
    if (!zone.boundsView.contains(mousePosView))
      continue;

    wireDragState.targetExternalZoneId = zone.zoneId;
    wireDragState.targetTypeMatch = (zone.dataType == wireDragState.sourceType);
    wireDragState.targetCycleFree = true;
    notifyExternalDragTarget();
    return;
  }

  notifyExternalDragTarget();
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

    const auto from = TEndpoint::makeRailPort(wireDragState.sourceExternalId,
                                              wireDragState.sourceExternalPortId);
    const auto to =
        TEndpoint::makeNodePort(wireDragState.targetNodeId,
                                wireDragState.targetPortId);
    return !connectionExists(document, from, to);
  }

  const TPort *sourcePort =
      findPortModel(wireDragState.sourceNodeId, wireDragState.sourcePortId);
  if (!sourcePort || sourcePort->direction != TPortDirection::Output)
    return false;

  if (wireDragState.targetExternalZoneId.isNotEmpty()) {
    if (!isOutputRailZone(document, wireDragState.targetExternalZoneId))
      return false;

    juce::String endpointId;
    juce::String portId;
    if (!splitRailPortZoneId(wireDragState.targetExternalZoneId, endpointId,
                             portId)) {
      return false;
    }

    const auto from =
        TEndpoint::makeNodePort(wireDragState.sourceNodeId,
                                wireDragState.sourcePortId);
    const auto to = TEndpoint::makeRailPort(endpointId, portId);
    return !connectionExists(document, from, to);
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

  const auto from = TEndpoint::makeNodePort(wireDragState.sourceNodeId,
                                            wireDragState.sourcePortId);
  const auto to = TEndpoint::makeNodePort(wireDragState.targetNodeId,
                                          wireDragState.targetPortId);
  return !connectionExists(document, from, to);
}

void TGraphCanvas::tryCreateConnectionFromDrag() {
  if (!isCurrentDragTargetConnectable())
    return;

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
    juce::String portId;
    if (splitRailPortZoneId(wireDragState.targetExternalZoneId, endpointId,
                            portId)) {
      TConnection conn;
      conn.connectionId = document.allocConnectionId();
      conn.from = TEndpoint::makeNodePort(wireDragState.sourceNodeId,
                                          wireDragState.sourcePortId);
      conn.to = TEndpoint::makeRailPort(endpointId, portId);
      document.executeCommand(std::make_unique<AddConnectionCommand>(conn));
      selectedConnectionId = conn.connectionId;
      return;
    }

    if (externalConnectionCommitHandler != nullptr) {
      const TPort *sourcePort =
          findPortModel(wireDragState.sourceNodeId, wireDragState.sourcePortId);
      externalConnectionCommitHandler(sourcePort, nullptr,
                                      wireDragState.targetExternalZoneId);
    }
    return;
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

  document.executeCommand(std::make_unique<AddConnectionCommand>(conn));
  selectedConnectionId = conn.connectionId;
}

void TGraphCanvas::beginConnectionDrag(const TPort &sourcePort,
                                       juce::Point<float> mousePosView) {
  auto sourcePosView = mousePosView;
  if (findPortComponent(sourcePort.ownerNodeId, sourcePort.portId) != nullptr)
    sourcePosView = portCentreInCanvas(sourcePort.ownerNodeId, sourcePort.portId);

  beginConnectionDragFromPoint(sourcePort, sourcePosView, mousePosView);
}

void TGraphCanvas::beginConnectionDragFromPoint(
    const TPort &sourcePort, juce::Point<float> sourcePosView,
    juce::Point<float> mousePosView) {
  if (sourcePort.direction != TPortDirection::Output)
    return;

  wireDragState = WireDragState{};
  wireDragState.active = true;
  wireDragState.sourceNodeId = sourcePort.ownerNodeId;
  wireDragState.sourcePortId = sourcePort.portId;
  wireDragState.sourceType = sourcePort.dataType;
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

  document.executeCommand(
      std::make_unique<DeleteConnectionCommand>(connectionId));

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
