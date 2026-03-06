#pragma once

#include "../Model/TGraphDocument.h"

namespace Teul {

// =============================================================================
//  TCommand — 되돌리기(Undo/Redo) 가능한 단일 인터랙션 단위
// =============================================================================
class TCommand {
public:
  virtual ~TCommand() = default;

  /** 명령어를 수행하고 문서 상태를 변경합니다. (최초 실행 및 Redo 시 호출) */
  virtual void execute(TGraphDocument &doc) = 0;

  /** 이전 상태로 문서 구조를 되돌립니다. (Undo 시 호출) */
  virtual void undo(TGraphDocument &doc) = 0;
};

// =============================================================================
//  THistoryStack — Command 배열 및 undo/redo 인덱스 관리자
// =============================================================================
class THistoryStack {
public:
  void pushNext(std::unique_ptr<TCommand> command, TGraphDocument &doc) {
    // 새로운 액션을 수행하면 기존 redo 스택 날림
    if (currentIndex < (int)commands.size() - 1) {
      commands.erase(commands.begin() + currentIndex + 1, commands.end());
    }

    command->execute(doc);
    commands.push_back(std::move(command));
    currentIndex = (int)commands.size() - 1;

    // 이력 갯수 제한 트리밍 (최대 100개)
    if (commands.size() > 100) {
      commands.erase(commands.begin());
      currentIndex--;
    }
  }

  bool undo(TGraphDocument &doc) {
    if (currentIndex >= 0 && currentIndex < (int)commands.size()) {
      commands[(size_t)currentIndex]->undo(doc);
      currentIndex--;
      return true;
    }
    return false;
  }

  bool redo(TGraphDocument &doc) {
    if (currentIndex + 1 < (int)commands.size()) {
      currentIndex++;
      commands[(size_t)currentIndex]->execute(doc);
      return true;
    }
    return false;
  }

  void clear() {
    commands.clear();
    currentIndex = -1;
  }

private:
  std::vector<std::unique_ptr<TCommand>> commands;
  int currentIndex = -1;
};

} // namespace Teul
