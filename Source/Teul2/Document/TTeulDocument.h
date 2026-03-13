#pragma once

#include "TDocumentTypes.h"

namespace Teul {

class TCommand;
class THistoryStack;

class TTeulDocument {
public:
  TTeulDocument();
  ~TTeulDocument();

  TTeulDocument(const TTeulDocument &other);
  TTeulDocument &operator=(const TTeulDocument &other);

  TTeulDocument(TTeulDocument &&other) noexcept;
  TTeulDocument &operator=(TTeulDocument &&other) noexcept;

  int schemaVersion = 4;
  TGraphMeta meta;

  std::vector<TNode> nodes;
  std::vector<TConnection> connections;
  std::vector<TFrameRegion> frames;
  std::vector<TBookmark> bookmarks;
  TControlSourceState controlState;

  NodeId allocNodeId() noexcept { return nextNodeId++; }
  PortId allocPortId() noexcept { return nextPortId++; }
  ConnectionId allocConnectionId() noexcept { return nextConnectionId++; }
  int allocFrameId() noexcept { return nextFrameId++; }
  int allocBookmarkId() noexcept { return nextBookmarkId++; }

  TNode *findNode(NodeId id) noexcept {
    for (auto &n : nodes)
      if (n.nodeId == id)
        return &n;
    return nullptr;
  }

  const TNode *findNode(NodeId id) const noexcept {
    for (const auto &n : nodes)
      if (n.nodeId == id)
        return &n;
    return nullptr;
  }

  TConnection *findConnection(ConnectionId id) noexcept {
    for (auto &c : connections)
      if (c.connectionId == id)
        return &c;
    return nullptr;
  }

  const TConnection *findConnection(ConnectionId id) const noexcept {
    for (const auto &c : connections)
      if (c.connectionId == id)
        return &c;
    return nullptr;
  }

  TFrameRegion *findFrame(int frameId) noexcept {
    for (auto &frame : frames)
      if (frame.frameId == frameId)
        return &frame;
    return nullptr;
  }

  const TFrameRegion *findFrame(int frameId) const noexcept {
    for (const auto &frame : frames)
      if (frame.frameId == frameId)
        return &frame;
    return nullptr;
  }

  bool isNodeExplicitMemberOfFrame(NodeId nodeId, int frameId) const noexcept {
    if (const auto *frame = findFrame(frameId))
      return frame->containsNode(nodeId);
    return false;
  }

  void addNodeToFrame(NodeId nodeId, int frameId) {
    if (auto *frame = findFrame(frameId)) {
      frame->membershipExplicit = true;
      frame->addMember(nodeId);
    }
  }

  void addNodeToFrameExclusive(NodeId nodeId, int frameId) {
    for (auto &frame : frames) {
      if (frame.frameId == frameId)
        continue;
      if (!frame.logicalGroup)
        continue;
      if (!frame.containsNode(nodeId))
        continue;

      frame.membershipExplicit = true;
      frame.removeMember(nodeId);
    }

    addNodeToFrame(nodeId, frameId);
  }

  void removeNodeFromFrame(NodeId nodeId, int frameId) {
    if (auto *frame = findFrame(frameId)) {
      frame->membershipExplicit = true;
      frame->removeMember(nodeId);
    }
  }

  void removeNodeFromAllFrames(NodeId nodeId) {
    for (auto &frame : frames) {
      if (frame.containsNode(nodeId)) {
        frame.membershipExplicit = true;
        frame.removeMember(nodeId);
      }
    }
  }

  TBookmark *findBookmark(int bookmarkId) noexcept {
    for (auto &bookmark : bookmarks)
      if (bookmark.bookmarkId == bookmarkId)
        return &bookmark;
    return nullptr;
  }

  const TBookmark *findBookmark(int bookmarkId) const noexcept {
    for (const auto &bookmark : bookmarks)
      if (bookmark.bookmarkId == bookmarkId)
        return &bookmark;
    return nullptr;
  }

  std::vector<TConnection *> connectionsForPort(NodeId nodeId,
                                                PortId portId) noexcept {
    std::vector<TConnection *> result;
    for (auto &c : connections) {
      if ((c.from.isNodePort() && c.from.nodeId == nodeId &&
           c.from.portId == portId) ||
          (c.to.isNodePort() && c.to.nodeId == nodeId &&
           c.to.portId == portId)) {
        result.push_back(&c);
      }
    }
    return result;
  }

  TSystemRailPort *findSystemRailPort(const juce::String &endpointId,
                                      const juce::String &portId) noexcept {
    return controlState.findEndpointPort(endpointId, portId);
  }

  const TSystemRailPort *findSystemRailPort(
      const juce::String &endpointId,
      const juce::String &portId) const noexcept {
    return controlState.findEndpointPort(endpointId, portId);
  }

  bool wouldCreateCycle(NodeId fromNodeId, NodeId toNodeId) const noexcept;

  NodeId getNextNodeId() const noexcept { return nextNodeId; }
  PortId getNextPortId() const noexcept { return nextPortId; }
  ConnectionId getNextConnectionId() const noexcept { return nextConnectionId; }
  int getNextFrameId() const noexcept { return nextFrameId; }
  int getNextBookmarkId() const noexcept { return nextBookmarkId; }
  std::uint64_t getDocumentRevision() const noexcept { return documentRevision; }
  std::uint64_t getRuntimeRevision() const noexcept { return runtimeRevision; }
  const TDocumentNotice &getTransientNotice() const noexcept {
    return transientNotice;
  }
  std::uint64_t getTransientNoticeRevision() const noexcept {
    return transientNoticeRevision;
  }

  void setTransientNotice(TDocumentNoticeLevel level,
                          const juce::String &title,
                          const juce::String &detail = {}) noexcept {
    const auto normalizedTitle = title.trim();
    const auto normalizedDetail = detail.trim();
    if (normalizedTitle.isEmpty() && normalizedDetail.isEmpty()) {
      clearTransientNotice();
      return;
    }

    if (transientNotice.active && transientNotice.level == level &&
        transientNotice.title == normalizedTitle &&
        transientNotice.detail == normalizedDetail) {
      return;
    }

    transientNotice.active = true;
    transientNotice.level = level;
    transientNotice.title = normalizedTitle;
    transientNotice.detail = normalizedDetail;
    ++transientNoticeRevision;
  }

  void clearTransientNotice() noexcept {
    if (!transientNotice.active && transientNotice.title.isEmpty() &&
        transientNotice.detail.isEmpty()) {
      return;
    }

    transientNotice = {};
    ++transientNoticeRevision;
  }

  void setNextNodeId(NodeId id) noexcept { nextNodeId = id; }
  void setNextPortId(PortId id) noexcept { nextPortId = id; }
  void setNextConnectionId(ConnectionId id) noexcept { nextConnectionId = id; }
  void setNextFrameId(int id) noexcept { nextFrameId = id; }
  void setNextBookmarkId(int id) noexcept { nextBookmarkId = id; }

  void executeCommand(std::unique_ptr<TCommand> command);
  bool undo();
  bool redo();
  void clearHistory();
  void touch(bool runtimeRelevant = false) noexcept;

private:
  NodeId nextNodeId = 1;
  PortId nextPortId = 1;
  ConnectionId nextConnectionId = 1;
  int nextFrameId = 1;
  int nextBookmarkId = 1;
  std::uint64_t documentRevision = 0;
  std::uint64_t runtimeRevision = 0;
  TDocumentNotice transientNotice;
  std::uint64_t transientNoticeRevision = 0;

  std::unique_ptr<THistoryStack> historyStack;
};

} // namespace Teul
