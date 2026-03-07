#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Node/TNodeComponent.h"
#include "Teul/History/TCommands.h"

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

void TGraphCanvas::setNodeSelection(NodeId nodeId, bool selected) {
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
  selectedNodeIds.clear();
  selectedNodeIds.push_back(nodeId);
  syncNodeSelectionToComponents();
}

void TGraphCanvas::syncNodeSelectionToComponents() {
  for (auto &comp : nodeComponents)
    comp->isSelected = isNodeSelected(comp->getNodeId());

  if (nodeSelectionChangedHandler != nullptr)
    nodeSelectionChangedHandler(selectedNodeIds);

  repaint();
}

void TGraphCanvas::updateMarqueeSelection() {
  std::vector<NodeId> next = marqueeState.baseSelection;

  for (const auto &comp : nodeComponents) {
    if (!comp->isVisible())
      continue;

    if (!marqueeState.rectView.intersects(comp->getBounds().toFloat()))
      continue;

    const NodeId id = comp->getNodeId();
    if (std::find(next.begin(), next.end(), id) == next.end())
      next.push_back(id);
  }

  selectedNodeIds = std::move(next);
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

void TGraphCanvas::deleteSelectionWithPrompt() {
  if (selectedNodeIds.empty())
    return;

  const bool confirmed = juce::AlertWindow::showOkCancelBox(
      juce::MessageBoxIconType::WarningIcon, "Delete Nodes",
      "Delete selected nodes? You can undo with Ctrl+Z.", "OK", "Cancel", nullptr, nullptr);
  if (!confirmed)
    return;

  const std::vector<NodeId> targets = selectedNodeIds;
  clearNodeSelection();

  for (const NodeId id : targets)
    document.executeCommand(std::make_unique<DeleteNodeCommand>(id));

  selectedConnectionId = kInvalidConnectionId;
  rebuildNodeComponents();
  pushStatusHint("Selection deleted. Undo: Ctrl+Z");
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
