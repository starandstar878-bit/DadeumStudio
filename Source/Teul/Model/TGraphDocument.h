#pragma once

#include "TConnection.h"
#include "TNode.h"

#include <vector>

namespace Teul {

// =============================================================================
//  TGraphMeta — 그래프 문서 메타데이터
//
//  canvasOffset / canvasZoom: 마지막으로 저장 시점의 뷰 상태.
//  sampleRate / blockSize:    런타임 힌트 (실제 장치 설정은
//  AudioDeviceManager).
// =============================================================================
struct TGraphMeta {
  juce::String name = "Untitled";
  float canvasOffsetX = 0.0f;
  float canvasOffsetY = 0.0f;
  float canvasZoom = 1.0f;
  double sampleRate = 48000.0;
  int blockSize = 256;
};

// =============================================================================
//  TGraphDocument — Teul 그래프의 최상위 문서 객체
//
//  직렬화 단위: 하나의 .teul 파일 = 하나의 TGraphDocument.
//
//  ID 발급:
//    allocNodeId / allocPortId / allocConnectionId 가 단조 증가 ID 를 반환.
//    삭제된 ID 는 절대 재사용하지 않는다.
//    (Undo 스택이 ID 동일성으로 객체를 추적하기 때문)
//
//  노드 소유:
//    nodes 벡터가 TNode 를 값 소유한다.
//    포트는 각 TNode 내부의 ports 벡터가 소유한다.
// =============================================================================
struct TGraphDocument {
  int schemaVersion = 1;
  TGraphMeta meta;

  std::vector<TNode> nodes;
  std::vector<TConnection> connections;

  // -------------------------------------------------------------------------
  //  ID 발급기
  // -------------------------------------------------------------------------
  NodeId allocNodeId() noexcept { return nextNodeId++; }
  PortId allocPortId() noexcept { return nextPortId++; }
  ConnectionId allocConnectionId() noexcept { return nextConnectionId++; }

  // -------------------------------------------------------------------------
  //  노드 탐색
  // -------------------------------------------------------------------------
  TNode *findNode(NodeId id) noexcept {
    for (auto &n : nodes)
      if (n.nodeId == id)
        return &n;
    return nullptr;
  }

  const TNode *findNode(NodeId id) const noexcept {
    for (const auto &n : nodes)
      if (n.nodeId == id)
        return &n;
    return nullptr;
  }

  // -------------------------------------------------------------------------
  //  연결 탐색
  // -------------------------------------------------------------------------
  TConnection *findConnection(ConnectionId id) noexcept {
    for (auto &c : connections)
      if (c.connectionId == id)
        return &c;
    return nullptr;
  }

  const TConnection *findConnection(ConnectionId id) const noexcept {
    for (const auto &c : connections)
      if (c.connectionId == id)
        return &c;
    return nullptr;
  }

  // -------------------------------------------------------------------------
  //  특정 포트에 연결된 모든 Connection 조회
  // -------------------------------------------------------------------------
  std::vector<TConnection *> connectionsForPort(NodeId nodeId,
                                                PortId portId) noexcept {
    std::vector<TConnection *> result;
    for (auto &c : connections) {
      if ((c.from.nodeId == nodeId && c.from.portId == portId) ||
          (c.to.nodeId == nodeId && c.to.portId == portId))
        result.push_back(&c);
    }
    return result;
  }

  // -------------------------------------------------------------------------
  //  순환 참조 감지 (Phase 3 위상 정렬 전 사전 검사용)
  //  from 노드에서 to 노드로 가는 경로가 이미 존재하면 true 반환.
  // -------------------------------------------------------------------------
  bool wouldCreateCycle(NodeId fromNodeId, NodeId toNodeId) const noexcept;

private:
  NodeId nextNodeId = 1;
  PortId nextPortId = 1;
  ConnectionId nextConnectionId = 1;
};

} // namespace Teul
