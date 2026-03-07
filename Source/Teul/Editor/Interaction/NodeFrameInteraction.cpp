#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Node/TNodeComponent.h"
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

  const bool toggle = event.mods.isCtrlDown() || event.mods.isCommandDown();
  const bool additive = event.mods.isShiftDown();

  if (toggle) {
    setNodeSelection(nodeId, !isNodeSelected(nodeId));
  } else if (additive) {
    setNodeSelection(nodeId, true);
  } else {
    if (!isNodeSelected(nodeId) || selectedNodeIds.size() != 1)
      selectOnlyNode(nodeId);
  }

  if (!isNodeSelected(nodeId))
    return;

  if (isNodeMoveLocked(nodeId)) {
    pushStatusHint("Node is inside a locked frame.");
    return;
  }

  startNodeDrag(nodeId, event.getEventRelativeTo(this).position);
}

void TGraphCanvas::requestNodeMouseDrag(NodeId nodeId,
                                        const juce::MouseEvent &event) {
  juce::ignoreUnused(nodeId);

  if (!nodeDragState.active)
    return;

  updateNodeDrag(event.getEventRelativeTo(this).position);
}

void TGraphCanvas::requestNodeMouseUp(NodeId nodeId,
                                      const juce::MouseEvent &event) {
  juce::ignoreUnused(nodeId, event);

  if (nodeDragState.active)
    endNodeDrag();
}

} // namespace Teul
