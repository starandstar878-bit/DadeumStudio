#pragma once

#include "../../Model/TNode.h"
#include "../../Registry/TNodeRegistry.h"
#include "../Port/TPortComponent.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>


namespace Teul {

class TGraphCanvas;

// =============================================================================
//  TNodeComponent — 개별 DSP 노드를 렌더링하는 컨테이너 화면 컴포넌트
//
//  [Phase 2 목표]
//  - 헤더(이름/기능) + 바디(포트/파라미터 슬롯) 랜더링
//  - 포트 리스트를 기반으로 좌/우에 TPortComponent를 동적 배치
//  - 캔버스 위에서 드래그하여 이동
//  - 선택 시 글로우 아웃라인 피드백
// =============================================================================
class TNodeComponent : public juce::Component {
public:
  // NodeId만 받아두고 모델 업데이트는 TGraphCanvas/TGraphDocument를 참조함
  TNodeComponent(TGraphCanvas &canvas, NodeId id, const TNodeDescriptor *desc);
  ~TNodeComponent() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  // 드래그 및 선택(Selection) 용도
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  NodeId getNodeId() const { return nodeId; }

  // TGraphDocument에서 현재 자신의 데이터를 가져와서 UI(포트 갯수, 에러 배지
  // 등)를 동기화
  void updateFromModel();

  bool isSelected = false; // Phase 4 선택기능 연동 타겟

private:
  TGraphCanvas &ownerCanvas;
  NodeId nodeId;
  const TNodeDescriptor *descriptor;

  juce::ComponentDragger dragger;

  std::vector<std::unique_ptr<TPortComponent>> inPorts;
  std::vector<std::unique_ptr<TPortComponent>> outPorts;

  // 디자인 레이아웃 상수들 (TeulPalette와 조합됨)
  const int headerHeight = 32;
  const int portRowHeight = 22;
  const int minNodeWidth = 140;
  const int cornerRadius = 8;

  // 바디 영역 높이를 자동 측정하는 유틸
  void recalculateHeight();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNodeComponent)
};

} // namespace Teul
