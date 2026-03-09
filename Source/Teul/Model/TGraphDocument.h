#pragma once

#include "TConnection.h"
#include "TNode.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace Teul {

class TCommand;
class THistoryStack;

struct TGraphMeta {
  juce::String name = "Untitled";
  float canvasOffsetX = 0.0f;
  float canvasOffsetY = 0.0f;
  float canvasZoom = 1.0f;
  double sampleRate = 48000.0;
  int blockSize = 256;
};

struct TFrameRegion {
  int frameId = 0;
  juce::String frameUuid;
  juce::String title = "Frame";
  float x = 0.0f;
  float y = 0.0f;
  float width = 360.0f;
  float height = 220.0f;
  juce::uint32 colorArgb = 0x334d8bf7;
  bool collapsed = false;
  bool locked = false;
  bool logicalGroup = true;
  bool membershipExplicit = false;
  std::vector<NodeId> memberNodeIds;

  bool containsNode(NodeId nodeId) const noexcept {
    return std::find(memberNodeIds.begin(), memberNodeIds.end(), nodeId) !=
           memberNodeIds.end();
  }

  void addMember(NodeId nodeId) {
    if (!containsNode(nodeId))
      memberNodeIds.push_back(nodeId);
  }

  void removeMember(NodeId nodeId) noexcept {
    memberNodeIds.erase(
        std::remove(memberNodeIds.begin(), memberNodeIds.end(), nodeId),
        memberNodeIds.end());
  }
};

struct TBookmark {
  int bookmarkId = 0;
  juce::String name = "Bookmark";
  float focusX = 0.0f;
  float focusY = 0.0f;
  float zoom = 1.0f;
  juce::String colorTag;
};

struct TGraphDocument {
  TGraphDocument();
  ~TGraphDocument();

  TGraphDocument(const TGraphDocument &other);
  TGraphDocument &operator=(const TGraphDocument &other);

  TGraphDocument(TGraphDocument &&other) noexcept;
  TGraphDocument &operator=(TGraphDocument &&other) noexcept;

  int schemaVersion = 3;
  TGraphMeta meta;

  std::vector<TNode> nodes;
  std::vector<TConnection> connections;
  std::vector<TFrameRegion> frames;
  std::vector<TBookmark> bookmarks;

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
      if ((c.from.nodeId == nodeId && c.from.portId == portId) ||
          (c.to.nodeId == nodeId && c.to.portId == portId))
        result.push_back(&c);
    }
    return result;
  }

  bool wouldCreateCycle(NodeId fromNodeId, NodeId toNodeId) const noexcept;

  NodeId getNextNodeId() const noexcept { return nextNodeId; }
  PortId getNextPortId() const noexcept { return nextPortId; }
  ConnectionId getNextConnectionId() const noexcept { return nextConnectionId; }
  int getNextFrameId() const noexcept { return nextFrameId; }
  int getNextBookmarkId() const noexcept { return nextBookmarkId; }
  std::uint64_t getDocumentRevision() const noexcept { return documentRevision; }
  std::uint64_t getRuntimeRevision() const noexcept { return runtimeRevision; }

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

  std::unique_ptr<THistoryStack> historyStack;
};

} // namespace Teul