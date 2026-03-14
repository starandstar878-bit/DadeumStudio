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
    /** 문서 변경 이벤트를 수신하기 위한 인터페이스 */
    class Listener {
    public:
        virtual ~Listener() = default;
        
        /** 바인딩 목록이나 개별 바인딩의 설정이 변경되었을 때 호출됩니다. */
        virtual void onIeumDocumentChanged(IeumDocument& doc) = 0;
        
        /** 바인딩의 상태(Status)가 실시간으로 변경되었을 때 호출됩니다. */
        virtual void onIeumBindingStatusChanged(IeumDocument& doc, const BindingId& id) {}
    };

    IeumDocument();
    ~IeumDocument();

    // -- 데이터 컬렉션 및 조회 --
    const std::vector<IeumBindingSpec>& getBindings() const noexcept { return bindings; }
    
    IeumBindingSpec* findBinding(const BindingId& id) noexcept;
    const IeumBindingSpec* findBinding(const BindingId& id) const noexcept;
    
    /** 새로운 바인딩을 위한 고유 ID를 생성합니다. */
    BindingId allocBindingId() noexcept;

    // -- 그룹 관리 (Roadmap 6.4) --
    const std::vector<IeumBindingGroup>& getGroups() const noexcept { return groups; }
    IeumBindingGroup* findGroup(const GroupId& id) noexcept;
    void addGroup(const IeumBindingGroup& group);
    void removeGroup(const GroupId& id);

    // -- 데이터 조작 (Index 자동 관리) --
    void addBinding(const IeumBindingSpec& spec);
    void removeBinding(const BindingId& id);
    void updateBinding(const IeumBindingSpec& spec);
    void clear() noexcept;

    // -- 고속 검색 (Reverse Index) --
    /** 특정 소스 엔드포인트에 연결된 모든 바인딩 ID를 찾습니다. */
    std::vector<BindingId> findBindingsBySource(const IeumEndpoint& source) const;
    /** 특정 타겟 엔드포인트에 연결된 모든 바인딩 ID를 찾습니다. */
    std::vector<BindingId> findBindingsByTarget(const IeumEndpoint& target) const;

    // -- 명령 및 이력 (Roadmap Phase 2) --
    void executeCommand(std::unique_ptr<IeumCommand> command);
    bool undo();
    bool redo();
    void clearHistory();

    // -- 리스너 관리 --
    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

    // -- 상태 추적 --
    std::uint64_t getDocumentRevision() const noexcept { return documentRevision; }
    
    /** 문서가 변경되었음을 알리고 레비전을 증가시킵니다. */
    void touch() noexcept;

    /** 바인딩 상태가 변경되었음을 리스너에게 알립니다. */
    void notifyBindingStatusChanged(const BindingId& id);

private:
    std::uint64_t documentRevision = 0;
    std::uint32_t nextIdCounter = 1;

    std::vector<IeumBindingSpec> bindings;
    std::vector<IeumBindingGroup> groups;

    std::unordered_map<juce::String, size_t> idToIndex;
    std::unordered_multimap<juce::String, size_t> sourceToIndex;
    std::unordered_multimap<juce::String, size_t> targetToIndex;

    void rebuildIndex() noexcept;
    static juce::String getEndpointKey(const IeumEndpoint& ep);

    std::unique_ptr<IeumDocumentHistory> history;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IeumDocument)
};

} // namespace Ieum
