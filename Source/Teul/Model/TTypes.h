#pragma once

#include <JuceHeader.h>
#include <cstdint>

namespace Teul {

// =============================================================================
//  기본 ID 타입
//
//  규칙: 삭제된 ID는 절대 재사용하지 않는다.
//  (Undo 스택에서 ID 동일성으로 객체를 추적하기 때문)
// =============================================================================
using NodeId = uint32_t;
using PortId = uint32_t;
using ConnectionId = uint32_t;

static constexpr NodeId kInvalidNodeId = 0;
static constexpr PortId kInvalidPortId = 0;
static constexpr ConnectionId kInvalidConnectionId = 0;

// =============================================================================
//  포트 방향
// =============================================================================
enum class TPortDirection {
  Input,
  Output,
};

// =============================================================================
//  포트 데이터 타입
//
//  색상 규약 (UI Phase 2 에서 사용):
//    Audio   → Green   (#4CAF50)
//    CV      → Amber   (#FFC107)
//    MIDI    → Blue    (#2196F3)
//    Gate    → Orange  (#FF9800)
//    Control → Purple  (#9C27B0)
// =============================================================================
enum class TPortDataType {
  Audio,   // 오디오 신호 (PCM float 샘플 버퍼)
  CV,      // Control Voltage — -1.0 ~ +1.0 float, 파라미터 변조용
  MIDI,    // MIDI 메시지 스트림
  Gate,    // 0 또는 1 (트리거 / 게이트)
  Control, // UI 직접 제어 값 (파라미터 노브 등)
};

} // namespace Teul
