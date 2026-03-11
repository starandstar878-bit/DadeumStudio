#pragma once

#include "TTypes.h"

namespace Teul {

// =============================================================================
//  TPort — 포트 데이터 모델
//
//  포트는 TNode 가 직접 소유한다 (std::vector<TPort> nodes).
//  ownerNodeId 는 역참조용으로 보관하며, 직렬화 시에도 기록된다.
//
//  channelIndex:
//    AudioInput / AudioOutput 노드에서 몇 번째 하드웨어 채널을 가리키는지.
//    일반 DSP 노드에서는 항상 0.
// =============================================================================
struct TPort {
  PortId portId = kInvalidPortId;
  TPortDirection direction = TPortDirection::Input;
  TPortDataType dataType = TPortDataType::Audio;
  juce::String name;
  NodeId ownerNodeId = kInvalidNodeId;
  int channelIndex = 0;
  int maxIncomingConnections = 1;
  int maxOutgoingConnections = -1;
};

} // namespace Teul
