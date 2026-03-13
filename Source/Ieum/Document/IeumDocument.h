#pragma once

#include "IeumBindingSpec.h"
#include <vector>
#include <memory>

namespace Ieum {

class IeumCommand;
class IeumDocumentHistory;

/** 
 * Ieum 바인딩 플랫폼의 핵심 문서 모델 (Single Source of Truth)
 * 모든 바인딩 명세와 편집 이력을 관리합니다.
 */
class IeumDocument {
public:
    IeumDocument();
    ~IeumDocument();

    // -- 데이터 컬렉션 --
    std::vector<IeumBindingSpec> bindings;

    // -- 조회 및 관리 --
    IeumBindingSpec* findBinding(const BindingId& id) noexcept;
    const IeumBindingSpec* findBinding(const BindingId& id) const noexcept;
    
    /** 새로운 바인딩을 위한 고유 ID를 생성합니다. */
    BindingId allocBindingId() noexcept;

    // -- 명령 및 이력 (Roadmap Phase 2) --
    void executeCommand(std::unique_ptr<IeumCommand> command);
    bool undo();
    bool redo();
    void clearHistory();

    // -- 상태 추적 --
    std::uint64_t getDocumentRevision() const noexcept { return documentRevision; }
    
    /** 문서가 변경되었음을 알리고 레비전을 증가시킵니다. */
    void touch() noexcept;

private:
    std::uint64_t documentRevision = 0;
    std::uint32_t nextIdCounter = 1;

    std::unique_ptr<IeumDocumentHistory> history;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IeumDocument)
};

} // namespace Ieum
