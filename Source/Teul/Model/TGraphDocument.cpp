#include "TGraphDocument.h"
#include "../History/TCommand.h"

namespace Teul {

// TGraphDocument 생성자/소멸자 등에서 std::unique_ptr<THistoryStack> 처리를
// 위해 필요 만약 구조체 초기화를 명시적으로 하기 위해선 생성자 정의 필요

TGraphDocument::TGraphDocument()
    : historyStack(std::make_unique<THistoryStack>()) {}

TGraphDocument::~TGraphDocument() = default;

TGraphDocument::TGraphDocument(const TGraphDocument &other) {
  schemaVersion = other.schemaVersion;
  meta = other.meta;
  nodes = other.nodes;
  connections = other.connections;
  nextNodeId = other.nextNodeId;
  nextPortId = other.nextPortId;
  nextConnectionId = other.nextConnectionId;
  historyStack = std::make_unique<THistoryStack>();
}

TGraphDocument &TGraphDocument::operator=(const TGraphDocument &other) {
  if (this != &other) {
    schemaVersion = other.schemaVersion;
    meta = other.meta;
    nodes = other.nodes;
    connections = other.connections;
    nextNodeId = other.nextNodeId;
    nextPortId = other.nextPortId;
    nextConnectionId = other.nextConnectionId;
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
  // TODO: Phase 3 (그래프 평가기) 에서 상세 구현 필요
  // 지금은 단순히 직접 연결된 루프만 방지
  if (fromNodeId == toNodeId)
    return true;
  return false;
}

} // namespace Teul
