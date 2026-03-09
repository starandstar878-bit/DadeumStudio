#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Node/TNodeComponent.h"
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

void TGraphCanvas::selectOnlyFrame(int frameId) {
  if (frameId <= 0) {
    clearFrameSelection();
    return;
  }

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
