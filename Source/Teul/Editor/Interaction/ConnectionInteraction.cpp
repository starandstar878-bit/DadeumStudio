#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Search/SearchController.h"
#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/Editor/Port/TPortComponent.h"
#include "Teul/History/TCommands.h"

namespace Teul {

void TGraphCanvas::clearDragTargetHighlight() {
  for (auto &node : nodeComponents) {
    for (const auto &port : node->getInputPorts()) {
      port->setDragTargetHighlight(false, true);
    }
  }
}

void TGraphCanvas::updateDragTargetFromMouse(juce::Point<float> mousePosView) {
  clearDragTargetHighlight();

  wireDragState.targetNodeId = kInvalidNodeId;
  wireDragState.targetPortId = kInvalidPortId;
  wireDragState.targetTypeMatch = false;
  wireDragState.targetCycleFree = false;

  if (!wireDragState.active)
    return;

  const auto mousePosInt = mousePosView.roundToInt();

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
          !document.wouldCreateCycle(wireDragState.sourceNodeId,
                                     candidatePort.ownerNodeId);

      inputPort->setDragTargetHighlight(true, isCurrentDragTargetConnectable());
      return;
    }
  }
}

bool TGraphCanvas::isCurrentDragTargetConnectable() const {
  if (!wireDragState.active)
    return false;

  if (wireDragState.sourceNodeId == kInvalidNodeId ||
      wireDragState.sourcePortId == kInvalidPortId ||
      wireDragState.targetNodeId == kInvalidNodeId ||
      wireDragState.targetPortId == kInvalidPortId)
    return false;

  if (!wireDragState.targetTypeMatch || !wireDragState.targetCycleFree)
    return false;

  if (wireDragState.sourceNodeId == wireDragState.targetNodeId &&
      wireDragState.sourcePortId == wireDragState.targetPortId)
    return false;

  const TPort *sourcePort =
      findPortModel(wireDragState.sourceNodeId, wireDragState.sourcePortId);
  const TPort *targetPort =
      findPortModel(wireDragState.targetNodeId, wireDragState.targetPortId);

  if (!sourcePort || !targetPort)
    return false;

  if (sourcePort->direction != TPortDirection::Output ||
      targetPort->direction != TPortDirection::Input)
    return false;

  const bool duplicateExists =
      std::any_of(document.connections.begin(), document.connections.end(),
                  [&](const TConnection &conn) {
                    return conn.from.nodeId == wireDragState.sourceNodeId &&
                           conn.from.portId == wireDragState.sourcePortId &&
                           conn.to.nodeId == wireDragState.targetNodeId &&
                           conn.to.portId == wireDragState.targetPortId;
                  });

  return !duplicateExists;
}

void TGraphCanvas::tryCreateConnectionFromDrag() {
  if (!isCurrentDragTargetConnectable())
    return;

  TConnection conn;
  conn.connectionId = document.allocConnectionId();
  conn.from.nodeId = wireDragState.sourceNodeId;
  conn.from.portId = wireDragState.sourcePortId;
  conn.to.nodeId = wireDragState.targetNodeId;
  conn.to.portId = wireDragState.targetPortId;

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

  const auto fromPos = portCentreInCanvas(conn->from.nodeId, conn->from.portId);
  const auto toPos = portCentreInCanvas(conn->to.nodeId, conn->to.portId);

  const float fromDistSq = fromPos.getDistanceSquaredFrom(breakPointView);
  const float toDistSq = toPos.getDistanceSquaredFrom(breakPointView);

  const juce::Point<float> anchorPos = (fromDistSq <= toDistSq) ? toPos : fromPos;

  const TPort *sourcePort = findPortModel(conn->from.nodeId, conn->from.portId);
  const TPortDataType sourceType =
      sourcePort ? sourcePort->dataType : TPortDataType::Audio;

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
