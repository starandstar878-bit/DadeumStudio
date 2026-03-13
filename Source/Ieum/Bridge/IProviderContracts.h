#pragma once

#include "../Document/IBindingTypes.h"
#include <vector>

namespace Ieum {

/** 
 * 파라미터(속성) 메타데이터
 * 프로바이더가 어떤 값을 제공하거나 받을 수 있는지 설명합니다.
 */
struct IParamInfo {
    ParamId id;                 // 코드용 식별자
    juce::String name;          // UI 출력용 이름
    IValueType type = IValueType::Floating;
    IValue minValue = 0.0;
    IValue maxValue = 1.0;
    IValue defaultValue = 0.0;
    juce::String unit;          // 단위 (dB, Hz 등)
};

/** 
 * 엔티티(객체) 메타데이터
 * 위젯 하나, 혹은 DSP 노드 하나를 나타냅니다.
 */
struct IEntityInfo {
    EntityId id;                // 고유 ID (UUID 등)
    juce::String name;          // UI 출력용 이름
    juce::String category;      // 분류 (Oscillators, Sliders 등)
    std::vector<IParamInfo> params;
};

/** 
 * 소스 프로바이더 인터페이스 (ex: Gyeol)
 * 값을 내보내는 시스템이 구현해야 합니다.
 */
class ISourceProvider {
public:
    virtual ~ISourceProvider() = default;
    virtual ProviderId getProviderId() const = 0;

    /** 내보낼 수 있는 모든 소스 목록을 조사합니다. */
    virtual std::vector<IEntityInfo> getAvailableSources() = 0;

    /** 소스 값의 변화를 감지하기 위한 리스너 */
    class Listener {
    public:
        virtual ~Listener() = default;
        /** 소스에서 값이 변경되었을 때 호출됩니다. */
        virtual void onSourceValueChanged(const EntityId& entityId, 
                                          const ParamId& paramId, 
                                          IValue newValue) = 0;
    };

    virtual void addIeumListener(Listener* l) = 0;
    virtual void removeIeumListener(Listener* l) = 0;
};

/** 
 * 타겟 프로바이더 인터페이스 (ex: Teul2)
 * 값을 받아들이는 시스템이 구현해야 합니다.
 */
class ITargetProvider {
public:
    virtual ~ITargetProvider() = default;
    virtual ProviderId getProviderId() const = 0;

    /** 저장이 가능한 모든 타겟 목록을 조사합니다. */
    virtual std::vector<IEntityInfo> getAvailableTargets() = 0;

    /** 
     * 타겟에 실시간으로 값을 기록합니다.
     * @return 성공 여부 (타겟이 존재하지 않거나 타입 오류 시 false)
     */
    virtual bool writeTargetValue(const EntityId& entityId, 
                                  const ParamId& paramId, 
                                  IValue value) = 0;

    /** 
     * 런타임 최적화를 위해 타겟의 메모리 주소나 핸들을 미리 찾아둡니다. (Roadmap Phase 3)
     * 이 핸들은 나중에 Lock-free 전송 시 사용됩니다.
     */
    virtual void* resolveTargetHandle(const EntityId& entityId, const ParamId& paramId) = 0;
};

} // namespace Ieum

