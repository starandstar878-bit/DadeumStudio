#include "Teul2/Editor/Canvas/TGraphCanvas.h"

#include "Teul2/Editor/Renderers/TNodeRenderer.h"
#include "Teul/History/TCommands.h"
#include "Teul/Registry/TNodeRegistry.h"

#include <algorithm>
#include <set>

namespace Teul {

bool TGraphCanvas::isNodeSelected(NodeId nodeId) const {
  return std::find(selectedNodeIds.begin(), selectedNodeIds.end(), nodeId) !=
         selectedNodeIds.end();
}

void TGraphCanvas::clearNodeSelection() {
  if (selectedNodeIds.empty())
    return;

  selectedNodeIds.clear();
  syncNodeSelectionToComponents();
}

void TGraphCanvas::clearFrameSelection() {
  if (selectedFrameId == 0)
    return;

  selectedFrameId = 0;
  auto handler = frameSelectionChangedHandler;
  if (handler != nullptr)
    handler(0);
  repaint();
}

void TGraphCanvas::clearAllSelection() {
  const bool hadConnectionSelection =
      selectedConnectionId != kInvalidConnectionId ||
      pressedConnectionId != kInvalidConnectionId || connectionBreakDragArmed;

  selectedConnectionId = kInvalidConnectionId;
  pressedConnectionId = kInvalidConnectionId;
  connectionBreakDragArmed = false;

  clearFrameSelection();
  clearNodeSelection();

  if (hadConnectionSelection)
    repaint();
}

void TGraphCanvas::setNodeSelection(NodeId nodeId, bool selected) {
  selectedConnectionId = kInvalidConnectionId;
  pressedConnectionId = kInvalidConnectionId;
  connectionBreakDragArmed = false;
  auto it = std::find(selectedNodeIds.begin(), selectedNodeIds.end(), nodeId);

  if (selected) {
    if (it == selectedNodeIds.end())
      selectedNodeIds.push_back(nodeId);
  } else {
    if (it != selectedNodeIds.end())
      selectedNodeIds.erase(it);
  }

  syncNodeSelectionToComponents();
}

void TGraphCanvas::selectOnlyNode(NodeId nodeId) {
  clearFrameSelection();
  selectedConnectionId = kInvalidConnectionId;
  pressedConnectionId = kInvalidConnectionId;
  connectionBreakDragArmed = false;
  selectedNodeIds.clear();
  selectedNodeIds.push_back(nodeId);
  syncNodeSelectionToComponents();
}

void TGraphCanvas::selectOnlyNodes(const std::vector<NodeId> &nodeIds) {
  clearFrameSelection();
  selectedConnectionId = kInvalidConnectionId;
  pressedConnectionId = kInvalidConnectionId;
  connectionBreakDragArmed = false;
  selectedNodeIds.clear();
  for (const auto nodeId : nodeIds) {
    if (document.findNode(nodeId) == nullptr)
      continue;
    if (std::find(selectedNodeIds.begin(), selectedNodeIds.end(), nodeId) !=
        selectedNodeIds.end())
      continue;
    selectedNodeIds.push_back(nodeId);
  }
  syncNodeSelectionToComponents();
}

void TGraphCanvas::selectOnlyFrame(int frameId) {
  if (frameId <= 0) {
    clearFrameSelection();
    return;
  }

  selectedConnectionId = kInvalidConnectionId;
  pressedConnectionId = kInvalidConnectionId;
  connectionBreakDragArmed = false;
  clearNodeSelection();

  selectedFrameId = frameId;
  auto handler = frameSelectionChangedHandler;
  if (handler != nullptr)
    handler(frameId);
  repaint();
}

void TGraphCanvas::syncNodeSelectionToComponents() {
  for (auto &comp : nodeComponents) {
    if (comp != nullptr)
      comp->isSelected = isNodeSelected(comp->getNodeId());
  }

  auto handler = nodeSelectionChangedHandler;
  const auto selectionSnapshot = selectedNodeIds;
  if (handler != nullptr)
    handler(selectionSnapshot);

  repaint();
}

std::vector<NodeId> TGraphCanvas::collectMarqueeSelection() const {
  std::vector<NodeId> next = marqueeState.baseSelection;

  for (const auto &comp : nodeComponents) {
    if (!comp->isVisible())
      continue;

    const auto boundsView = getNodeBoundsInView(*comp);
    if (!marqueeState.rectView.intersects(boundsView))
      continue;

    const NodeId id = comp->getNodeId();
    if (std::find(next.begin(), next.end(), id) == next.end())
      next.push_back(id);
  }

  return next;
}

void TGraphCanvas::applyMarqueeSelection() {
  selectedNodeIds = collectMarqueeSelection();
  syncNodeSelectionToComponents();
}

void TGraphCanvas::duplicateSelection() {
  if (selectedNodeIds.empty())
    return;

  const std::set<NodeId> selectedSet(selectedNodeIds.begin(), selectedNodeIds.end());

  std::unordered_map<NodeId, NodeId> newNodeByOld;
  std::unordered_map<PortId, PortId> newPortByOld;
  std::vector<NodeId> newSelection;

  for (const auto &node : document.nodes) {
    if (selectedSet.find(node.nodeId) == selectedSet.end())
      continue;

    TNode copy = node;
    const NodeId oldNodeId = copy.nodeId;
    copy.nodeId = document.allocNodeId();
    copy.x += 36.0f;
    copy.y += 24.0f;

    for (auto &port : copy.ports) {
      const PortId oldPortId = port.portId;
      port.portId = document.allocPortId();
      port.ownerNodeId = copy.nodeId;
      newPortByOld[oldPortId] = port.portId;
    }

    newNodeByOld[oldNodeId] = copy.nodeId;
    newSelection.push_back(copy.nodeId);
    document.executeCommand(std::make_unique<AddNodeCommand>(copy));
  }

  const std::vector<TConnection> existingConnections = document.connections;
  for (const auto &conn : existingConnections) {
    if (selectedSet.find(conn.from.nodeId) == selectedSet.end() ||
        selectedSet.find(conn.to.nodeId) == selectedSet.end()) {
      continue;
    }

    auto fromNodeIt = newNodeByOld.find(conn.from.nodeId);
    auto toNodeIt = newNodeByOld.find(conn.to.nodeId);
    auto fromPortIt = newPortByOld.find(conn.from.portId);
    auto toPortIt = newPortByOld.find(conn.to.portId);

    if (fromNodeIt == newNodeByOld.end() || toNodeIt == newNodeByOld.end() ||
        fromPortIt == newPortByOld.end() || toPortIt == newPortByOld.end()) {
      continue;
    }

    TConnection copyConn;
    copyConn.connectionId = document.allocConnectionId();
    copyConn.from = {fromNodeIt->second, fromPortIt->second};
    copyConn.to = {toNodeIt->second, toPortIt->second};
    document.executeCommand(std::make_unique<AddConnectionCommand>(copyConn));
  }

  selectedNodeIds = std::move(newSelection);
  rebuildNodeComponents();
  pushStatusHint("Duplicated selection. Undo: Ctrl+Z");
}

TGraphCanvas::DeleteSelectionImpact
TGraphCanvas::analyzeDeleteSelectionImpact(
    const std::vector<NodeId> &targets) const {
  DeleteSelectionImpact impact;
  if (targets.empty())
    return impact;

  const std::set<NodeId> targetSet(targets.begin(), targets.end());
  for (const auto &connection : document.connections) {
    if (targetSet.count(connection.from.nodeId) > 0 ||
        targetSet.count(connection.to.nodeId) > 0) {
      ++impact.touchingConnectionCount;
    }
  }

  if (bindingSummaryResolver == nullptr)
    return impact;

  std::set<juce::String> seenBindings;
  for (const NodeId nodeId : targets) {
    const TNode *node = document.findNode(nodeId);
    if (node == nullptr)
      continue;

    juce::String nodeName = node->label;
    if (nodeName.isEmpty()) {
      if (const auto *descriptor = findDescriptorByTypeKey(node->typeKey))
        nodeName = descriptor->displayName;
      else
        nodeName = node->typeKey;
    }

    for (const auto &param : listExposedParamsForNode(*node)) {
      const juce::String summary = bindingSummaryResolver(param.paramId).trim();
      if (summary.isEmpty())
        continue;

      const juce::String item =
          nodeName + " / " + param.paramLabel + ": " + summary;
      if (seenBindings.insert(item).second)
        impact.bindingSummaries.push_back(item);
    }
  }

  return impact;
}

void TGraphCanvas::deleteNodes(const std::vector<NodeId> &targets) {
  if (targets.empty())
    return;

  clearNodeSelection();

  for (const NodeId id : targets)
    document.executeCommand(std::make_unique<DeleteNodeCommand>(id));

  selectedConnectionId = kInvalidConnectionId;
  rebuildNodeComponents();
  pushStatusHint("Selection deleted. Undo: Ctrl+Z");
}

void TGraphCanvas::deleteSelectionWithPrompt() {

  if (selectedNodeIds.empty())

    return;



  const std::vector<NodeId> targets = selectedNodeIds;

  const auto impact = analyzeDeleteSelectionImpact(targets);



  if (!impact.requiresConfirmation()) {

    deleteNodes(targets);

    return;

  }



  juce::String message =

      targets.size() == 1 ? "Delete selected node?" : "Delete selected nodes?";



  if (impact.touchingConnectionCount > 0) {

    message << "\n\nThis will also remove "

            << juce::String(impact.touchingConnectionCount) << " connected wire";

    if (impact.touchingConnectionCount != 1)

      message << "s";

    message << ".";

  }



  if (!impact.bindingSummaries.empty()) {

    message << "\n\nThis may break "

            << juce::String((int)impact.bindingSummaries.size())

            << " Gyeol widget binding";

    if (impact.bindingSummaries.size() != 1)

      message << "s";

    message << " connected through Ieum.";



    const int previewCount = juce::jmin(3, (int)impact.bindingSummaries.size());

    for (int index = 0; index < previewCount; ++index)

      message << "\n- " << impact.bindingSummaries[(size_t)index];



    if ((int)impact.bindingSummaries.size() > previewCount) {

      message << "\n- ... and "

              << juce::String((int)impact.bindingSummaries.size() - previewCount)

              << " more";

    }

  }



  message << "\n\nCtrl+Z will undo.";



  const bool confirmed = juce::AlertWindow::showOkCancelBox(

      juce::MessageBoxIconType::WarningIcon, "Delete Nodes", message, "OK",

      "Cancel", nullptr, nullptr);

  if (!confirmed)

    return;



  deleteNodes(targets);

}

void TGraphCanvas::disconnectSelectionWithPrompt() {
  if (selectedNodeIds.empty())
    return;

  std::vector<ConnectionId> toDelete;
  for (const auto &conn : document.connections) {
    if (isNodeSelected(conn.from.nodeId) || isNodeSelected(conn.to.nodeId))
      toDelete.push_back(conn.connectionId);
  }

  if (toDelete.empty())
    return;

  const bool confirmed = juce::AlertWindow::showOkCancelBox(
      juce::MessageBoxIconType::WarningIcon, "Disconnect Selected",
      "Disconnect all wires touching selected nodes? Ctrl+Z will undo.", "OK", "Cancel", nullptr, nullptr);
  if (!confirmed)
    return;

  for (const ConnectionId id : toDelete)
    document.executeCommand(std::make_unique<DeleteConnectionCommand>(id));

  selectedConnectionId = kInvalidConnectionId;
  pushStatusHint("Disconnected selection. Undo: Ctrl+Z");
  repaint();
}


void TGraphCanvas::toggleBypassSelection() {
  if (selectedNodeIds.empty())
    return;

  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id))
      node->bypassed = !node->bypassed;
  }

  document.touch(true);
  repaint();
}

} // namespace Teul

#include "Teul2/Editor/Canvas/TGraphCanvas.h"

#include <algorithm>

namespace Teul {

void TGraphCanvas::alignSelectionLeft() {
  if (selectedNodeIds.size() < 2)
    return;

  float minX = std::numeric_limits<float>::max();
  for (const NodeId id : selectedNodeIds) {
    if (const TNode *node = document.findNode(id))
      minX = juce::jmin(minX, node->x);
  }

  bool didMove = false;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id)) {
      didMove = didMove || node->x != minX;
      node->x = minX;
    }
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}

void TGraphCanvas::alignSelectionTop() {
  if (selectedNodeIds.size() < 2)
    return;

  float minY = std::numeric_limits<float>::max();
  for (const NodeId id : selectedNodeIds) {
    if (const TNode *node = document.findNode(id))
      minY = juce::jmin(minY, node->y);
  }

  bool didMove = false;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id)) {
      didMove = didMove || node->y != minY;
      node->y = minY;
    }
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}

void TGraphCanvas::distributeSelectionHorizontally() {
  if (selectedNodeIds.size() < 3)
    return;

  std::vector<TNode *> nodes;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id))
      nodes.push_back(node);
  }

  if (nodes.size() < 3)
    return;

  std::sort(nodes.begin(), nodes.end(),
            [](const TNode *a, const TNode *b) { return a->x < b->x; });

  const float start = nodes.front()->x;
  const float end = nodes.back()->x;
  const float step = (end - start) / (float)(nodes.size() - 1);

  bool didMove = false;
  for (size_t i = 1; i + 1 < nodes.size(); ++i) {
    const float nextX = start + step * (float)i;
    didMove = didMove || nodes[i]->x != nextX;
    nodes[i]->x = nextX;
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}

void TGraphCanvas::distributeSelectionVertically() {
  if (selectedNodeIds.size() < 3)
    return;

  std::vector<TNode *> nodes;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id))
      nodes.push_back(node);
  }

  if (nodes.size() < 3)
    return;

  std::sort(nodes.begin(), nodes.end(),
            [](const TNode *a, const TNode *b) { return a->y < b->y; });

  const float start = nodes.front()->y;
  const float end = nodes.back()->y;
  const float step = (end - start) / (float)(nodes.size() - 1);

  bool didMove = false;
  for (size_t i = 1; i + 1 < nodes.size(); ++i) {
    const float nextY = start + step * (float)i;
    didMove = didMove || nodes[i]->y != nextY;
    nodes[i]->y = nextY;
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}

} // namespace Teul

#include "Teul2/Editor/Canvas/TGraphCanvas.h"

#include "Teul2/Editor/Renderers/TNodeRenderer.h"
#include "Teul/History/TCommands.h"

namespace Teul {

void TGraphCanvas::startFrameDrag(int frameId, juce::Point<float> mouseView) {
  frameDragState = FrameDragState{};

  for (const auto &frame : document.frames) {
    if (frame.frameId != frameId)
      continue;

    if (frame.locked)
      return;

    frameDragState.active = true;
    frameDragState.frameId = frameId;
    frameDragState.startMouseView = mouseView;
    frameDragState.startFramePosWorld = {frame.x, frame.y};

    for (const auto &node : document.nodes) {
      if (isNodeInsideFrame(node, frame))
        frameDragState.containedNodeStartWorld[node.nodeId] = {node.x, node.y};
    }

    return;
  }
}

void TGraphCanvas::updateFrameDrag(juce::Point<float> mouseView) {
  if (!frameDragState.active)
    return;

  auto frameIt = std::find_if(document.frames.begin(), document.frames.end(),
                              [&](const TFrameRegion &f) {
                                return f.frameId == frameDragState.frameId;
                              });
  if (frameIt == document.frames.end())
    return;

  const juce::Point<float> deltaWorld =
      (mouseView - frameDragState.startMouseView) / zoomLevel;

  frameIt->x = frameDragState.startFramePosWorld.x + deltaWorld.x;
  frameIt->y = frameDragState.startFramePosWorld.y + deltaWorld.y;

  for (const auto &pair : frameDragState.containedNodeStartWorld) {
    if (TNode *node = document.findNode(pair.first)) {
      node->x = pair.second.x + deltaWorld.x;
      node->y = pair.second.y + deltaWorld.y;
    }
  }

  updateChildPositions();
  repaint();
}

void TGraphCanvas::endFrameDrag() {
  bool didMove = false;

  if (frameDragState.active) {
    if (const auto *frame = document.findFrame(frameDragState.frameId)) {
      didMove = frame->x != frameDragState.startFramePosWorld.x ||
                frame->y != frameDragState.startFramePosWorld.y;
    }

    if (!didMove) {
      for (const auto &pair : frameDragState.containedNodeStartWorld) {
        if (const auto *node = document.findNode(pair.first)) {
          if (node->x != pair.second.x || node->y != pair.second.y) {
            didMove = true;
            break;
          }
        }
      }
    }
  }

  frameDragState = FrameDragState{};
  if (didMove)
    document.touch(false);
}

void TGraphCanvas::startNodeDrag(NodeId nodeId, juce::Point<float> mouseView) {
  nodeDragState = NodeDragState{};
  nodeDragState.active = true;
  nodeDragState.anchorNodeId = nodeId;
  nodeDragState.startMouseView = mouseView;

  for (const NodeId id : selectedNodeIds) {
    if (const TNode *node = document.findNode(id))
      nodeDragState.startWorldByNode[id] = {node->x, node->y};
  }
}

void TGraphCanvas::updateNodeDrag(juce::Point<float> mouseView) {
  if (!nodeDragState.active)
    return;

  juce::Point<float> deltaWorld =
      (mouseView - nodeDragState.startMouseView) / zoomLevel;

  snapGuideState = SnapGuideState{};

  if (selectedNodeIds.size() == 1) {
    const auto anchorIt =
        nodeDragState.startWorldByNode.find(nodeDragState.anchorNodeId);

    if (anchorIt != nodeDragState.startWorldByNode.end()) {
      const juce::Point<float> candidate = anchorIt->second + deltaWorld;
      const float threshold = 10.0f / zoomLevel;

      float bestDx = std::numeric_limits<float>::max();
      float bestDy = std::numeric_limits<float>::max();
      float snapX = candidate.x;
      float snapY = candidate.y;

      for (const auto &node : document.nodes) {
        if (node.nodeId == nodeDragState.anchorNodeId)
          continue;

        const float dx = std::abs(candidate.x - node.x);
        if (dx < bestDx && dx < threshold) {
          bestDx = dx;
          snapX = node.x;
        }

        const float dy = std::abs(candidate.y - node.y);
        if (dy < bestDy && dy < threshold) {
          bestDy = dy;
          snapY = node.y;
        }
      }

      if (bestDx != std::numeric_limits<float>::max()) {
        deltaWorld.x += (snapX - candidate.x);
        snapGuideState.xActive = true;
        snapGuideState.worldX = snapX;
      }

      if (bestDy != std::numeric_limits<float>::max()) {
        deltaWorld.y += (snapY - candidate.y);
        snapGuideState.yActive = true;
        snapGuideState.worldY = snapY;
      }
    }
  }

  for (const auto &pair : nodeDragState.startWorldByNode) {
    TNode *node = document.findNode(pair.first);
    if (node == nullptr)
      continue;

    if (isNodeMoveLocked(node->nodeId))
      continue;

    node->x = pair.second.x + deltaWorld.x;
    node->y = pair.second.y + deltaWorld.y;
  }

  updateChildPositions();
  repaint();
}

void TGraphCanvas::endNodeDrag() {
  bool didMove = false;
  if (nodeDragState.active) {
    for (const auto &pair : nodeDragState.startWorldByNode) {
      if (const auto *node = document.findNode(pair.first)) {
        if (node->x != pair.second.x || node->y != pair.second.y) {
          didMove = true;
          break;
        }
      }
    }
  }

  nodeDragState = NodeDragState{};
  snapGuideState = SnapGuideState{};
  if (didMove)
    document.touch(false);
  repaint();
}

void TGraphCanvas::requestNodeMouseDown(NodeId nodeId,
                                        const juce::MouseEvent &event) {
  if (!event.mods.isLeftButtonDown())
    return;

  clearFrameSelection();

  const bool toggle = event.mods.isCtrlDown() || event.mods.isCommandDown();
  const bool additive = event.mods.isShiftDown();

  if (toggle) {
    setNodeSelection(nodeId, !isNodeSelected(nodeId));
  } else if (additive) {
    setNodeSelection(nodeId, true);
  } else {
    if (!isNodeSelected(nodeId))
      selectOnlyNode(nodeId);
  }

  if (!isNodeSelected(nodeId))
    return;

  if (isNodeMoveLocked(nodeId)) {
    pushStatusHint("Node is inside a locked frame.");
    return;
  }

  const juce::Point<float> mouseView =
      getLocalPoint(nullptr, event.getScreenPosition()).toFloat();
  startNodeDrag(nodeId, mouseView);
}

void TGraphCanvas::requestNodeMouseDrag(NodeId nodeId,
                                        const juce::MouseEvent &event) {
  juce::ignoreUnused(nodeId);

  if (!nodeDragState.active)
    return;

  const juce::Point<float> mouseView =
      getLocalPoint(nullptr, event.getScreenPosition()).toFloat();
  updateNodeDrag(mouseView);
}

void TGraphCanvas::requestNodeMouseUp(NodeId nodeId,
                                      const juce::MouseEvent &event) {
  juce::ignoreUnused(nodeId, event);

  if (nodeDragState.active)
    endNodeDrag();
}

} // namespace Teul
