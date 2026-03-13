#include "TDocumentHistory.h"

namespace Teul {

// =============================================================================
//  노드 추가 (AddNodeCommand)
// =============================================================================
class AddNodeCommand : public TCommand {
public:
  explicit AddNodeCommand(const TNode &n) : nodeData(n) {}

  void execute(TTeulDocument &doc) override {
    addedNodeId = nodeData.nodeId;
    doc.nodes.push_back(nodeData);
  }

  void undo(TTeulDocument &doc) override {
    auto it = std::find_if(
        doc.nodes.begin(), doc.nodes.end(),
        [this](const TNode &n) { return n.nodeId == addedNodeId; });
    if (it != doc.nodes.end()) {
      doc.nodes.erase(it);
    }
  }

private:
  TNode nodeData;
  NodeId addedNodeId = kInvalidNodeId;
};

// =============================================================================
//  노드 삭제 (DeleteNodeCommand)
// =============================================================================
class DeleteNodeCommand : public TCommand {
public:
  explicit DeleteNodeCommand(NodeId id) : targetId(id) {}

  void execute(TTeulDocument &doc) override {
    connBackup.clear();
    frameMembershipBackup.clear();

    auto *n = doc.findNode(targetId);
    if (n)
      nodeBackup = *n;

    for (auto &frame : doc.frames) {
      if (!frame.containsNode(targetId))
        continue;

      frameMembershipBackup.push_back({frame.frameId, frame.memberNodeIds});
      frame.membershipExplicit = true;
      frame.removeMember(targetId);
    }

    for (auto it = doc.connections.begin(); it != doc.connections.end();) {
      if (it->from.referencesNode(targetId) || it->to.referencesNode(targetId)) {
        connBackup.push_back(*it);
        it = doc.connections.erase(it);
      } else {
        ++it;
      }
    }

    auto nit =
        std::find_if(doc.nodes.begin(), doc.nodes.end(),
                     [this](const TNode &n) { return n.nodeId == targetId; });
    if (nit != doc.nodes.end())
      doc.nodes.erase(nit);
  }

  void undo(TTeulDocument &doc) override {
    if (nodeBackup.nodeId != kInvalidNodeId)
      doc.nodes.push_back(nodeBackup);

    for (const auto &membership : frameMembershipBackup) {
      if (auto *frame = doc.findFrame(membership.first)) {
        frame->membershipExplicit = true;
        frame->memberNodeIds = membership.second;
      }
    }

    for (const auto &c : connBackup)
      doc.connections.push_back(c);
  }

private:
  NodeId targetId;
  TNode nodeBackup;
  std::vector<TConnection> connBackup;
  std::vector<std::pair<int, std::vector<NodeId>>> frameMembershipBackup;
};

// =============================================================================
//  AddConnectionCommand
// =============================================================================
class AddConnectionCommand : public TCommand {
public:
  explicit AddConnectionCommand(const TConnection &c) : connData(c) {}

  void execute(TTeulDocument &doc) override {
    addedConnId = connData.connectionId;
    doc.connections.push_back(connData);
  }

  void undo(TTeulDocument &doc) override {
    auto it = std::find_if(
        doc.connections.begin(), doc.connections.end(),
        [this](const TConnection &c) { return c.connectionId == addedConnId; });
    if (it != doc.connections.end()) {
      doc.connections.erase(it);
    }
  }

private:
  TConnection connData;
  ConnectionId addedConnId = kInvalidConnectionId;
};

// =============================================================================
//  연결 삭제 (DeleteConnectionCommand)
// =============================================================================
class DeleteConnectionCommand : public TCommand {
public:
  explicit DeleteConnectionCommand(ConnectionId id) : targetId(id) {}

  void execute(TTeulDocument &doc) override {
    auto *c = doc.findConnection(targetId);
    if (c) {
      connBackup = *c;
      auto it = std::find_if(doc.connections.begin(), doc.connections.end(),
                             [this](const TConnection &cc) {
                               return cc.connectionId == targetId;
                             });
      if (it != doc.connections.end())
        doc.connections.erase(it);
    }
  }

  void undo(TTeulDocument &doc) override {
    if (connBackup.connectionId != kInvalidConnectionId) {
      doc.connections.push_back(connBackup);
    }
  }

private:
  ConnectionId targetId;
  TConnection connBackup;
};

// =============================================================================
//  노드 이동 (MoveNodeCommand)
// =============================================================================
class MoveNodeCommand : public TCommand {
public:
  MoveNodeCommand(NodeId id, float oldX, float oldY, float newX, float newY)
      : targetId(id), prevX(oldX), prevY(oldY), nextX(newX), nextY(newY) {}

  void execute(TTeulDocument &doc) override {
    auto *n = doc.findNode(targetId);
    if (n) {
      n->x = nextX;
      n->y = nextY;
    }
  }

  void undo(TTeulDocument &doc) override {
    auto *n = doc.findNode(targetId);
    if (n) {
      n->x = prevX;
      n->y = prevY;
    }
  }

private:
  NodeId targetId;
  float prevX, prevY, nextX, nextY;
};

// =============================================================================
//  파라미터 변경 (SetParamCommand)
// =============================================================================
class SetParamCommand : public TCommand {
public:
  SetParamCommand(NodeId id, const juce::String &key, const juce::var &oldVal,
                  const juce::var &newVal)
      : targetId(id), paramKey(key), prevVal(oldVal), nextVal(newVal) {}

  void execute(TTeulDocument &doc) override {
    auto *n = doc.findNode(targetId);
    if (n) {
      n->params[paramKey] = nextVal;
    }
  }

  void undo(TTeulDocument &doc) override {
    auto *n = doc.findNode(targetId);
    if (n) {
      n->params[paramKey] = prevVal;
    }
  }

private:
  NodeId targetId;
  juce::String paramKey;
  juce::var prevVal;
  juce::var nextVal;
};

} // namespace Teul
