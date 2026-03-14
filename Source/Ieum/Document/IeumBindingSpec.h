#pragma once

#include "IeumBindingTypes.h"

namespace Ieum {

/** 
 * 소스 또는 타겟의 위치를 가리키는 주소 정보 
 */
struct IeumEndpoint {
    ProviderId providerId;
    EntityId entityId;
    ParamId paramId;

    bool isValid() const { return providerId.isValid() && !entityId.isEmpty() && !paramId.isEmpty(); }
    
    bool operator==(const IeumEndpoint& other) const {
        return providerId == other.providerId && entityId == other.entityId && paramId == other.paramId;
    }
    bool operator!=(const IeumEndpoint& other) const { return !(*this == other); }
};

/** 
 * 개별 바인딩의 명세 (Roadmap Phase 2)
 * 소스에서 타겟으로의 데이터 흐름을 정의하는 핵심 데이터 모델입니다.
 */
class IeumBindingSpec {
public:
    IeumBindingSpec() = default;
    IeumBindingSpec(const BindingId& id) : id(id) {}

    BindingId id;                   // 바인딩 고유 ID
    juce::String name;              // 사용자가 지정한 바인딩 이름

    IeumEndpoint source;               // 데이터 발생지
    IeumEndpoint target;               // 데이터 목적지

    IeumBindingMode mode = IeumBindingMode::Continuous;
    IeumValueType valueType = IeumValueType::Floating;
    
    bool enabled = true;            // 활성화 여부
    IeumBindingStatus status = IeumBindingStatus::Waiting;
    
    /** 상세 상태 내역 (Roadmap 6.3) */
    IeumStatusDetail statusDetail;

    /** 바인딩 그룹 ID (Roadmap 6.4) */
    GroupId groupId;

    /** 
     * 사용자 정의 매핑 범위 (Roadmap 3.1)
     * 소스 도메인(0~1 등)을 타겟 도메인(Hz, dB 등)으로 변환하기 위한 기준 정보.
     */
    IeumRange sourceRange = { 0.0, 1.0 };
    IeumRange targetRange = { 0.0, 1.0 };

    /** 
     * 변환 파이프라인 설정 (Roadmap Phase 4)
     * 현재는 간단히 TransformId 하나만 저장하도록 시작합니다.
     */
    TransformId transformId;

    /** 
     * 데이터의 동일성 비교 (직렬화 및 변경 감지용)
     */
    bool isSameAs(const IeumBindingSpec& other) const {
        return source == other.source && 
               target == other.target && 
               mode == other.mode && 
               valueType == other.valueType && 
               enabled == other.enabled && 
               transformId == other.transformId &&
               groupId == other.groupId &&
               sourceRange == other.sourceRange &&
               targetRange == other.targetRange;
    }
};

/** 
 * 바인딩들의 논리적 묶음 (Roadmap 6.4)
 */
class IeumBindingGroup {
public:
    GroupId id;
    juce::String name;
    
    /** 그룹에 속한 바인딩 ID 목록 */
    std::vector<BindingId> memberIds;
    
    /** 그룹 전체 활성/비활성 상태 */
    bool enabled = true;
};

} // namespace Ieum
