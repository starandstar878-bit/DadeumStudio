#pragma once

#include "TTypes.h"

namespace Teul {

// =============================================================================
//  TEndpoint — 연결의 한쪽 끝 (노드 + 포트)
// =============================================================================
struct TEndpoint {
  NodeId nodeId = kInvalidNodeId;
  PortId portId = kInvalidPortId;

  bool isValid() const noexcept {
    return nodeId != kInvalidNodeId && portId != kInvalidPortId;
  }
};

// =============================================================================
//  TConnection — 두 포트를 잇는 연결선 데이터 모델
//
//  방향: from (Output 포트) → to (Input 포트)
//  타입 호환성 검증은 TGraphDocument 또는 팩토리 레이어에서 수행한다.
// =============================================================================
struct TConnection {
  ConnectionId connectionId = kInvalidConnectionId;
  TEndpoint from; // 소스: 출력 포트
  TEndpoint to;   // 대상: 입력 포트

  bool isValid() const noexcept {
    return connectionId != kInvalidConnectionId && from.isValid() &&
           to.isValid();
  }
};

} // namespace Teul
