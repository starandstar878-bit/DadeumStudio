#pragma once

#include <JuceHeader.h>
#include <cstdint>

namespace Ieum {

/** 바인딩 시스템 전역에서 사용되는 식별자 타입들 */
using BindingId = juce::String;
using ProviderId = juce::Identifier;
using EntityId = juce::String;
using ParamId = juce::String;
using TransformId = juce::Identifier;
using GroupId = juce::String;

/** 바인딩 시스템의 핵심 정밀도 정의: 상용 수준의 정밀도를 위해 double 사용 */
using IeumValue = double;

/** 바인딩 가능 데이터의 물리적 성격 (Roadmap 6.1, 6.2) */
enum class IeumValueType {
    Floating,  // 연속적인 수치 (0.0 ~ 1.0 등)
    Integer,   // 이산적인 숫자 (Index, Count)
    Boolean,   // On/Off 상태
    Trigger,   // 값 없는 단순 이벤트 (Action/Event)
    Color,     // 시각적 피드백 시스템용
    String,    // 텍스트 속성 제어용
    Unknown
};

/** 바인딩 모드: 값이 전달되는 로직 패턴 (Roadmap 6.3) */
enum class IeumBindingMode {
    Continuous, // 실시간 업데이트
    Toggle,     // 이벤트 발생 시 상태 반전
    Trigger,    // 한 번만 실행
    Relative,   // 현재 값 기반 상대적 증감
    Momentary   // 누르고 있는 동안만 활성화
};

/** 합산 처리 방식 (Roadmap Phase 8) */
enum class IeumAggregationMode {
    Sum,        // 단순 합산
    Average,    // 평균
    Max,        // 최대값
    Min,        // 최소값
    Latest,     // 최신값 우선
    Multiply,   // 곱하기
    Expression  // 사용자 정의 수식 (예: "A + B * 0.5")
};

/** 합산 상세 정책 */
struct IeumAggregationPolicy {
    IeumAggregationMode mode = IeumAggregationMode::Sum;
    juce::String customExpression; // mode가 Expression일 때 사용될 실제 수식 문자열
    
    bool operator==(const IeumAggregationPolicy& other) const {
        return mode == other.mode && customExpression == other.customExpression;
    }
    bool operator!=(const IeumAggregationPolicy& other) const { return !(*this == other); }
};

/** 
 * 조건부 매핑 명세 (Roadmap Phase 8/12 확장용)
 * 특정 조건이 충족될 때만 바인딩이 활성화되거나 값이 반영되는 로직을 정의합니다.
 */
struct IeumConditionSpec {
    juce::String expression; // 조건식 (예: "Gyeol.Slider1 > 0.5")
    bool enabled = false;    // 조건 사용 여부

    bool operator==(const IeumConditionSpec& other) const {
        return expression == other.expression && enabled == other.enabled;
    }
    bool operator!=(const IeumConditionSpec& other) const { return !(*this == other); }
};

/** 바인딩 건강 상태: 상용급 진단의 핵심 (Roadmap 8.1) */
enum class IeumBindingStatus {
    Ok,         // 정상
    Missing,    // 소스나 타겟을 찾을 수 없음
    Degraded,   // 연결은 되었으나 성능/정밀도 제한 발생
    Invalid,    // 타입 불일치 등 계약 위반
    Waiting     // 초기화 중 또는 지연 로딩 상태
};

/** 바인딩 실패 원인에 대한 상세 설명 (Roadmap 6.3, 8.1) */
struct IeumStatusDetail {
    juce::String message;           // 사용자에게 보여줄 메시지 (예: "Source entity 'Slider1' not found")
    juce::String missingEntityId;   // 누락된 엔티티 ID (추적용)
    juce::String missingParamId;    // 누락된 파라미터 ID
    std::uint64_t timestamp = 0;    // 발생 시각
};

/** 수치 매핑 범위를 나타내는 구조체 */
struct IeumRange {
    IeumValue min = 0.0;
    IeumValue max = 1.0;
    
    bool operator==(const IeumRange& other) const { return min == other.min && max == other.max; }
    bool operator!=(const IeumRange& other) const { return !(*this == other); }
};

/** 오디오 스레드 전송 시 사용되는 데이터 패킷 구조 */
struct IeumRuntimePayload {
    IeumValue value = 0.0;
    std::uint64_t timestamp = 0;
    bool isActive = true;
};

} // namespace Ieum
