#pragma once

#include "TDocumentTypes.h"

#include <memory>
#include <vector>

namespace Teul {

class TTeulDocument;

class TCommand {
public:
  virtual ~TCommand() = default;

  virtual void execute(TTeulDocument &document) = 0;
  virtual void undo(TTeulDocument &document) = 0;
};

class THistoryStack {
public:
  THistoryStack();
  ~THistoryStack();

  void pushNext(std::unique_ptr<TCommand> command, TTeulDocument &document);
  bool undo(TTeulDocument &document);
  bool redo(TTeulDocument &document);
  void clear();

private:
  std::vector<std::unique_ptr<TCommand>> commands;
  int currentIndex = -1;
};

std::unique_ptr<TCommand> createAddNodeCommand(const TNode &node);
std::unique_ptr<TCommand> createDeleteNodeCommand(NodeId nodeId);
std::unique_ptr<TCommand> createAddConnectionCommand(
    const TConnection &connection);
std::unique_ptr<TCommand> createDeleteConnectionCommand(
    ConnectionId connectionId);
std::unique_ptr<TCommand> createMoveNodeCommand(NodeId nodeId,
                                                float oldX,
                                                float oldY,
                                                float newX,
                                                float newY);
std::unique_ptr<TCommand> createSetParamCommand(NodeId nodeId,
                                                const juce::String &paramKey,
                                                const juce::var &oldValue,
                                                const juce::var &newValue);

} // namespace Teul
