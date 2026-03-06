#pragma once

#include "TPort.h"

#include <map>
#include <vector>

namespace Teul {

// =============================================================================
//  TNode — 노드 데이터 모델
//
//  TNode 는 순수 직렬화 가능 데이터다.
//  DSP 상태(필터 계수, 딜레이 버퍼 등)는 Phase 3 에서 도입할
//  TNodeInstance 가 별도로 소유한다.
//
//  포트 소유:
//    ports 벡터가 이 노드의 모든 포트를 소유한다.
//    TGraphDocument 는 포트를 별도 컬렉션으로 관리하지 않으며,
//    항상 node.ports 를 통해 접근한다.
//
//  metadata:
//    직렬화 대상이지만 DSP 로직과 무관한 UI 상태값.
// =============================================================================
struct TNode {
  NodeId nodeId = kInvalidNodeId;
  juce::String typeKey; // 노드 종류 식별자 (예: "Oscillator", "LowPassFilter")
  float x = 0.0f;       // 캔버스 상의 위치 (월드 좌표)
  float y = 0.0f;

  // 파라미터 맵 — 타입 키에 따라 내용이 달라진다.
  // 값 타입: float, int, bool, String (juce::var 이 모두 수용)
  std::map<juce::String, juce::var> params;

  // 이 노드에 속한 포트 목록 (Input 먼저, Output 나중 정렬 권장)
  std::vector<TPort> ports;

  // UI 메타데이터
  bool collapsed = false; // 노드 최소화 상태
  bool bypassed = false;  // 바이패스 (DSP 우회)
  juce::String label;     // 사용자 지정 이름 (비어있으면 typeKey 사용)

  // 상태 메타데이터 (UI 에러 배지 표시용)
  bool hasError = false;
  juce::String errorMessage;

  // -------------------------------------------------------------------------
  //  포트 탐색 헬퍼
  // -------------------------------------------------------------------------
  TPort *findPort(PortId id) noexcept {
    for (auto &p : ports)
      if (p.portId == id)
        return &p;
    return nullptr;
  }

  const TPort *findPort(PortId id) const noexcept {
    for (const auto &p : ports)
      if (p.portId == id)
        return &p;
    return nullptr;
  }
};

} // namespace Teul
