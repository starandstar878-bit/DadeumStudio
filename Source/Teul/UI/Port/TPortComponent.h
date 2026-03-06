#pragma once

#include "../../Model/TPort.h"
#include <JuceHeader.h>


namespace Teul {

class TNodeComponent;

// =============================================================================
//  TPortComponent — 오디오/CV/MIDI/Control 단자 렌더링 및 연결 시각화 처리
//
//  [기능]
//  - 포트 타입별 색상을 가진 동그란 잭 형태로 렌더링
//  - 마우스 오버 시 글로우 효과 (툴팁 기능은 추후 연동)
//  - 드래그 앤 드롭으로 연결선(Wire) 생성을 주도 (Phase 3 대상)
// =============================================================================
class TPortComponent : public juce::Component {
public:
  TPortComponent(TNodeComponent &owner, const TPort &port);
  ~TPortComponent() override;

  void paint(juce::Graphics &g) override;

  void mouseEnter(const juce::MouseEvent &e) override;
  void mouseExit(const juce::MouseEvent &e) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  const TPort &getPortData() const { return portData; }

private:
  TNodeComponent &ownerNode;
  TPort portData;

  bool isHovered = false;
  juce::Colour getPortColor() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TPortComponent)
};

} // namespace Teul
