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
  documentRevision = other.documentRevision;
  runtimeRevision = other.runtimeRevision;
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
    documentRevision = other.documentRevision;
    runtimeRevision = other.runtimeRevision;
    historyStack = std::make_unique<THistoryStack>();
  }
  return *this;
}

TGraphDocument::TGraphDocument(TGraphDocument &&other) noexcept = default;
TGraphDocument &
TGraphDocument::operator=(TGraphDocument &&other) noexcept = default;

void TGraphDocument::executeCommand(std::unique_ptr<TCommand> command) {
  if (!historyStack || command == nullptr)
    return;

  historyStack->pushNext(std::move(command), *this);
  touch(true);
}

bool TGraphDocument::undo() {
  if (historyStack && historyStack->undo(*this)) {
    touch(true);
    return true;
  }
  return false;
}

bool TGraphDocument::redo() {
  if (historyStack && historyStack->redo(*this)) {
    touch(true);
    return true;
  }
  return false;
}

void TGraphDocument::clearHistory() {
  if (historyStack)
    historyStack->clear();
}

void TGraphDocument::touch(bool runtimeRelevant) noexcept {
  ++documentRevision;
  if (runtimeRelevant)
    ++runtimeRevision;
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