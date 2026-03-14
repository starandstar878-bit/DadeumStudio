#pragma once

#include "IeumBindingTypes.h"
#include <memory>
#include <vector>

namespace Ieum {

class IeumDocument;
class IeumBindingSpec;

/** 
 * 이력 관리를 위한 개별 명령 인터페이스 
 */
class IeumCommand {
public:
    virtual ~IeumCommand() = default;

    virtual void execute(IeumDocument& document) = 0;
    virtual void undo(IeumDocument& document) = 0;
};

/** 
 * Undo/Redo를 담당하는 이력 스택
 */
class IeumDocumentHistory {
public:
    IeumDocumentHistory();
    ~IeumDocumentHistory();

    void pushNext(std::unique_ptr<IeumCommand> command, IeumDocument& document);
    bool undo(IeumDocument& document);
    bool redo(IeumDocument& document);
    void clear();

private:
    std::vector<std::unique_ptr<IeumCommand>> commands;
    int currentIndex = -1;
};

/** 
 * 여러 명령을 하나로 묶어주는 복합 명령 (Roadmap Phase 4)
 */
class IeumCompositeCommand final : public IeumCommand {
public:
    void addCommand(std::unique_ptr<IeumCommand> cmd) { commands.push_back(std::move(cmd)); }
    
    void execute(IeumDocument& doc) override {
        for (auto& cmd : commands) cmd->execute(doc);
    }
    
    void undo(IeumDocument& doc) override {
        for (auto it = commands.rbegin(); it != commands.rend(); ++it)
            (*it)->undo(doc);
    }

private:
    std::vector<std::unique_ptr<IeumCommand>> commands;
};

// -- 공통 명령 생성 도우미 함수들 --
std::unique_ptr<IeumCommand> createAddBindingCommand(const IeumBindingSpec& spec);
std::unique_ptr<IeumCommand> createRemoveBindingCommand(const BindingId& id);
std::unique_ptr<IeumCommand> createUpdateBindingCommand(const IeumBindingSpec& oldSpec, const IeumBindingSpec& newSpec);
std::unique_ptr<IeumCommand> createCompositeCommand();

} // namespace Ieum
