#include "TDocumentHistory.h"

#include "TTeulDocument.h"

#include <algorithm>

namespace Teul {
namespace {

constexpr int kMaxHistoryCommands = 100;

class AddNodeCommand final : public TCommand {
public:
  explicit AddNodeCommand(const TNode &nodeIn) : node(nodeIn) {}

  void execute(TTeulDocument &document) override {
    addedNodeId = node.nodeId;
    document.nodes.push_back(node);
  }

  void undo(TTeulDocument &document) override {
    const auto nodeIt = std::find_if(
        document.nodes.begin(), document.nodes.end(),
        [&](const TNode &candidate) { return candidate.nodeId == addedNodeId; });
    if (nodeIt != document.nodes.end())
      document.nodes.erase(nodeIt);
  }

private:
  TNode node;
  NodeId addedNodeId = kInvalidNodeId;
};

class DeleteNodeCommand final : public TCommand {
public:
  explicit DeleteNodeCommand(NodeId targetIdIn) : targetId(targetIdIn) {}

  void execute(TTeulDocument &document) override {
    connectionBackup.clear();
    frameMembershipBackup.clear();

    if (const auto *node = document.findNode(targetId))
      nodeBackup = *node;

    for (auto &frame : document.frames) {
      if (!frame.containsNode(targetId))
        continue;

      frameMembershipBackup.push_back({frame.frameId, frame.memberNodeIds});
      frame.membershipExplicit = true;
      frame.removeMember(targetId);
    }

    for (auto connectionIt = document.connections.begin();
         connectionIt != document.connections.end();) {
      if (connectionIt->from.referencesNode(targetId) ||
          connectionIt->to.referencesNode(targetId)) {
        connectionBackup.push_back(*connectionIt);
        connectionIt = document.connections.erase(connectionIt);
        continue;
      }

      ++connectionIt;
    }

    const auto nodeIt = std::find_if(
        document.nodes.begin(), document.nodes.end(),
        [&](const TNode &candidate) { return candidate.nodeId == targetId; });
    if (nodeIt != document.nodes.end())
      document.nodes.erase(nodeIt);
  }

  void undo(TTeulDocument &document) override {
    if (nodeBackup.nodeId != kInvalidNodeId)
      document.nodes.push_back(nodeBackup);

    for (const auto &membership : frameMembershipBackup) {
      if (auto *frame = document.findFrame(membership.first)) {
        frame->membershipExplicit = true;
        frame->memberNodeIds = membership.second;
      }
    }

    for (const auto &connection : connectionBackup)
      document.connections.push_back(connection);
  }

private:
  NodeId targetId = kInvalidNodeId;
  TNode nodeBackup;
  std::vector<TConnection> connectionBackup;
  std::vector<std::pair<int, std::vector<NodeId>>> frameMembershipBackup;
};

class AddConnectionCommand final : public TCommand {
public:
  explicit AddConnectionCommand(const TConnection &connectionIn)
      : connection(connectionIn) {}

  void execute(TTeulDocument &document) override {
    addedConnectionId = connection.connectionId;
    document.connections.push_back(connection);
  }

  void undo(TTeulDocument &document) override {
    const auto connectionIt = std::find_if(
        document.connections.begin(), document.connections.end(),
        [&](const TConnection &candidate) {
          return candidate.connectionId == addedConnectionId;
        });
    if (connectionIt != document.connections.end())
      document.connections.erase(connectionIt);
  }

private:
  TConnection connection;
  ConnectionId addedConnectionId = kInvalidConnectionId;
};

class DeleteConnectionCommand final : public TCommand {
public:
  explicit DeleteConnectionCommand(ConnectionId targetIdIn)
      : targetId(targetIdIn) {}

  void execute(TTeulDocument &document) override {
    if (const auto *connection = document.findConnection(targetId))
      connectionBackup = *connection;

    const auto connectionIt = std::find_if(
        document.connections.begin(), document.connections.end(),
        [&](const TConnection &candidate) {
          return candidate.connectionId == targetId;
        });
    if (connectionIt != document.connections.end())
      document.connections.erase(connectionIt);
  }

  void undo(TTeulDocument &document) override {
    if (connectionBackup.connectionId != kInvalidConnectionId)
      document.connections.push_back(connectionBackup);
  }

private:
  ConnectionId targetId = kInvalidConnectionId;
  TConnection connectionBackup;
};

class MoveNodeCommand final : public TCommand {
public:
  MoveNodeCommand(NodeId targetIdIn,
                  float oldXIn,
                  float oldYIn,
                  float newXIn,
                  float newYIn)
      : targetId(targetIdIn),
        oldX(oldXIn),
        oldY(oldYIn),
        newX(newXIn),
        newY(newYIn) {}

  void execute(TTeulDocument &document) override {
    if (auto *node = document.findNode(targetId)) {
      node->x = newX;
      node->y = newY;
    }
  }

  void undo(TTeulDocument &document) override {
    if (auto *node = document.findNode(targetId)) {
      node->x = oldX;
      node->y = oldY;
    }
  }

private:
  NodeId targetId = kInvalidNodeId;
  float oldX = 0.0f;
  float oldY = 0.0f;
  float newX = 0.0f;
  float newY = 0.0f;
};

class SetParamCommand final : public TCommand {
public:
  SetParamCommand(NodeId targetIdIn,
                  const juce::String &paramKeyIn,
                  const juce::var &oldValueIn,
                  const juce::var &newValueIn)
      : targetId(targetIdIn),
        paramKey(paramKeyIn),
        oldValue(oldValueIn),
        newValue(newValueIn) {}

  void execute(TTeulDocument &document) override {
    if (auto *node = document.findNode(targetId))
      node->params[paramKey] = newValue;
  }

  void undo(TTeulDocument &document) override {
    if (auto *node = document.findNode(targetId))
      node->params[paramKey] = oldValue;
  }

private:
  NodeId targetId = kInvalidNodeId;
  juce::String paramKey;
  juce::var oldValue;
  juce::var newValue;
};

} // namespace

THistoryStack::THistoryStack() = default;

THistoryStack::~THistoryStack() = default;

void THistoryStack::pushNext(std::unique_ptr<TCommand> command,
                             TTeulDocument &document) {
  if (command == nullptr)
    return;

  if (currentIndex < static_cast<int>(commands.size()) - 1) {
    commands.erase(commands.begin() + currentIndex + 1, commands.end());
  }

  command->execute(document);
  commands.push_back(std::move(command));
  currentIndex = static_cast<int>(commands.size()) - 1;

  if (static_cast<int>(commands.size()) > kMaxHistoryCommands) {
    commands.erase(commands.begin());
    --currentIndex;
  }
}

bool THistoryStack::undo(TTeulDocument &document) {
  if (currentIndex < 0 || currentIndex >= static_cast<int>(commands.size()))
    return false;

  commands[static_cast<size_t>(currentIndex)]->undo(document);
  --currentIndex;
  return true;
}

bool THistoryStack::redo(TTeulDocument &document) {
  if (currentIndex + 1 >= static_cast<int>(commands.size()))
    return false;

  ++currentIndex;
  commands[static_cast<size_t>(currentIndex)]->execute(document);
  return true;
}

void THistoryStack::clear() {
  commands.clear();
  currentIndex = -1;
}

std::unique_ptr<TCommand> createAddNodeCommand(const TNode &node) {
  return std::make_unique<AddNodeCommand>(node);
}

std::unique_ptr<TCommand> createDeleteNodeCommand(NodeId nodeId) {
  return std::make_unique<DeleteNodeCommand>(nodeId);
}

std::unique_ptr<TCommand>
createAddConnectionCommand(const TConnection &connection) {
  return std::make_unique<AddConnectionCommand>(connection);
}

std::unique_ptr<TCommand> createDeleteConnectionCommand(
    ConnectionId connectionId) {
  return std::make_unique<DeleteConnectionCommand>(connectionId);
}

std::unique_ptr<TCommand> createMoveNodeCommand(NodeId nodeId,
                                                float oldX,
                                                float oldY,
                                                float newX,
                                                float newY) {
  return std::make_unique<MoveNodeCommand>(nodeId, oldX, oldY, newX, newY);
}

std::unique_ptr<TCommand> createSetParamCommand(NodeId nodeId,
                                                const juce::String &paramKey,
                                                const juce::var &oldValue,
                                                const juce::var &newValue) {
  return std::make_unique<SetParamCommand>(nodeId, paramKey, oldValue,
                                           newValue);
}

} // namespace Teul
