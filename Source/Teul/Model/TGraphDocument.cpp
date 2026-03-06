#include "TGraphDocument.h"
#include "../History/TCommand.h"

#include <queue>
#include <unordered_set>

namespace Teul {

TGraphDocument::TGraphDocument()
    : historyStack(std::make_unique<THistoryStack>()) {}

TGraphDocument::~TGraphDocument() = default;

TGraphDocument::TGraphDocument(const TGraphDocument &other) {
  schemaVersion = other.schemaVersion;
  meta = other.meta;
  nodes = other.nodes;
  connections = other.connections;
  frames = other.frames;
  bookmarks = other.bookmarks;
  nextNodeId = other.nextNodeId;
  nextPortId = other.nextPortId;
  nextConnectionId = other.nextConnectionId;
  nextFrameId = other.nextFrameId;
  nextBookmarkId = other.nextBookmarkId;
  historyStack = std::make_unique<THistoryStack>();
}

TGraphDocument &TGraphDocument::operator=(const TGraphDocument &other) {
  if (this != &other) {
    schemaVersion = other.schemaVersion;
    meta = other.meta;
    nodes = other.nodes;
    connections = other.connections;
    frames = other.frames;
    bookmarks = other.bookmarks;
    nextNodeId = other.nextNodeId;
    nextPortId = other.nextPortId;
    nextConnectionId = other.nextConnectionId;
    nextFrameId = other.nextFrameId;
    nextBookmarkId = other.nextBookmarkId;
    historyStack = std::make_unique<THistoryStack>();
  }
  return *this;
}

TGraphDocument::TGraphDocument(TGraphDocument &&other) noexcept = default;
TGraphDocument &
TGraphDocument::operator=(TGraphDocument &&other) noexcept = default;

void TGraphDocument::executeCommand(std::unique_ptr<TCommand> command) {
  if (historyStack)
    historyStack->pushNext(std::move(command), *this);
}

bool TGraphDocument::undo() {
  if (historyStack)
    return historyStack->undo(*this);
  return false;
}

bool TGraphDocument::redo() {
  if (historyStack)
    return historyStack->redo(*this);
  return false;
}

void TGraphDocument::clearHistory() {
  if (historyStack)
    historyStack->clear();
}

bool TGraphDocument::wouldCreateCycle(NodeId fromNodeId,
                                      NodeId toNodeId) const noexcept {
  if (fromNodeId == toNodeId)
    return true;

  std::unordered_set<NodeId> visited;
  std::queue<NodeId> queue;
  queue.push(toNodeId);

  while (!queue.empty()) {
    const NodeId current = queue.front();
    queue.pop();

    if (current == fromNodeId)
      return true;

    if (!visited.insert(current).second)
      continue;

    for (const auto &connection : connections) {
      if (connection.from.nodeId == current &&
          visited.find(connection.to.nodeId) == visited.end()) {
        queue.push(connection.to.nodeId);
      }
    }
  }

  return false;
}

} // namespace Teul