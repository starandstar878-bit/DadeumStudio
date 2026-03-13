#include "TTeulDocument.h"

#include "TDocumentHistory.h"

#include <queue>
#include <unordered_set>

namespace Teul {

TTeulDocument::TTeulDocument()
    : historyStack(std::make_unique<THistoryStack>()) {
  controlState.ensurePreviewDataIfEmpty();
  controlState.reconcileDeviceProfilesAndSources();
}

TTeulDocument::~TTeulDocument() = default;

TTeulDocument::TTeulDocument(const TTeulDocument &other) {
  schemaVersion = other.schemaVersion;
  meta = other.meta;
  nodes = other.nodes;
  connections = other.connections;
  frames = other.frames;
  bookmarks = other.bookmarks;
  controlState = other.controlState;
  controlState.reconcileDeviceProfilesAndSources();
  nextNodeId = other.nextNodeId;
  nextPortId = other.nextPortId;
  nextConnectionId = other.nextConnectionId;
  nextFrameId = other.nextFrameId;
  nextBookmarkId = other.nextBookmarkId;
  documentRevision = other.documentRevision;
  runtimeRevision = other.runtimeRevision;
  transientNotice = other.transientNotice;
  transientNoticeRevision = other.transientNoticeRevision;
  historyStack = std::make_unique<THistoryStack>();
}

TTeulDocument &TTeulDocument::operator=(const TTeulDocument &other) {
  if (this != &other) {
    schemaVersion = other.schemaVersion;
    meta = other.meta;
    nodes = other.nodes;
    connections = other.connections;
    frames = other.frames;
    bookmarks = other.bookmarks;
    controlState = other.controlState;
    controlState.reconcileDeviceProfilesAndSources();
    nextNodeId = other.nextNodeId;
    nextPortId = other.nextPortId;
    nextConnectionId = other.nextConnectionId;
    nextFrameId = other.nextFrameId;
    nextBookmarkId = other.nextBookmarkId;
    documentRevision = other.documentRevision;
    runtimeRevision = other.runtimeRevision;
    transientNotice = other.transientNotice;
    transientNoticeRevision = other.transientNoticeRevision;
    historyStack = std::make_unique<THistoryStack>();
  }
  return *this;
}

TTeulDocument::TTeulDocument(TTeulDocument &&other) noexcept = default;
TTeulDocument &
TTeulDocument::operator=(TTeulDocument &&other) noexcept = default;

void TTeulDocument::executeCommand(std::unique_ptr<TCommand> command) {
  if (!historyStack || command == nullptr)
    return;

  historyStack->pushNext(std::move(command), *this);
  touch(true);
}

bool TTeulDocument::undo() {
  if (historyStack && historyStack->undo(*this)) {
    touch(true);
    return true;
  }
  return false;
}

bool TTeulDocument::redo() {
  if (historyStack && historyStack->redo(*this)) {
    touch(true);
    return true;
  }
  return false;
}

void TTeulDocument::clearHistory() {
  if (historyStack)
    historyStack->clear();
}

void TTeulDocument::touch(bool runtimeRelevant) noexcept {
  ++documentRevision;
  if (runtimeRelevant)
    ++runtimeRevision;
}

bool TTeulDocument::wouldCreateCycle(NodeId fromNodeId,
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
      if (!connection.from.isNodePort() || !connection.to.isNodePort())
        continue;

      if (connection.from.nodeId == current &&
          visited.find(connection.to.nodeId) == visited.end()) {
        queue.push(connection.to.nodeId);
      }
    }
  }

  return false;
}

} // namespace Teul
