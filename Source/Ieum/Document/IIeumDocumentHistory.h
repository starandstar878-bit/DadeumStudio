#pragma once

#include "IBindingTypes.h"
#include <memory>
#include <vector>

namespace Ieum {

class IIeumDocument;
class IBindingSpec;

/** 
 * 이력 관리를 위한 개별 명령 인터페이스 
 */
class IIeumCommand {
public:
    virtual ~IIeumCommand() = default;

    virtual void execute(IIeumDocument& document) = 0;
    virtual void undo(IIeumDocument& document) = 0;
};

/** 
 * Undo/Redo를 담당하는 이력 스택
 */
class IIeumDocumentHistory {
public:
    IIeumDocumentHistory();
    ~IIeumDocumentHistory();

    void pushNext(std::unique_ptr<IIeumCommand> command, IIeumDocument& document);
    bool undo(IIeumDocument& document);
    bool redo(IIeumDocument& document);
    void clear();

private:
    std::vector<std::unique_ptr<IIeumCommand>> commands;
    int currentIndex = -1;
};

// -- 공통 명령 생성 도우미 함수들 --
std::unique_ptr<IIeumCommand> createAddBindingCommand(const IBindingSpec& spec);
std::unique_ptr<IIeumCommand> createRemoveBindingCommand(const BindingId& id);
std::unique_ptr<IIeumCommand> createUpdateBindingCommand(const IBindingSpec& oldSpec, const IBindingSpec& newSpec);

} // namespace Ieum

