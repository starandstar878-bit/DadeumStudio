#include "Teul/Editor/Canvas/TGraphCanvas.h"

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
