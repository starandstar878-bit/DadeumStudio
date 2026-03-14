#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include <JuceHeader.h>

namespace Teul {

class TTeulEditor;

// =============================================================================
//  TRailNodeComponent
//
//  레일에 고착된 노드를 위한 전용 UI 컴포넌트.
//  일반 노드(TNodeComponent)와 달리 드래그 이동이 불가능하며,
//  레일 레이아웃에 최적화된 시각화 로직을 가집니다.
// =============================================================================
class TRailNodeComponent : public juce::Component {
public:
  TRailNodeComponent(NodeId id, const TNode &nodeData);
  ~TRailNodeComponent() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  NodeId getNodeId() const noexcept { return nodeId; }
  void updateFromModel(const TNode &nodeData);

  // 시각화 관련 설정
  void setAccentColor(juce::Colour color) { accentColor = color; repaint(); }
  void setBadgeText(const juce::String &text) { badgeText = text; repaint(); }
  void setLocked(bool locked) { isLocked = locked; repaint(); }

private:
  NodeId nodeId;
  juce::String title;
  juce::String subtitle;
  juce::String badgeText;
  juce::Colour accentColor;
  bool isLocked = false;
  bool isMissing = false;
  bool hasError = false;

  const int cornerRadius = 6;
  const int badgeWidth = 40;
  
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TRailNodeComponent)
};

// =============================================================================
//  TIOControlNodeRenderer (통합 렌더링 도구)
//
//  TRailNodeComponent 를 위한 실질적인 그리기 함수들을 담고 있습니다.
//  TNodeRenderer 패턴을 따라 그리기 로직을 전역 함수로 분리하여 재사용성을 높였습니다.
// =============================================================================
namespace TIOControlNodeRenderer {

  void drawRailNodeBody(juce::Graphics &g, 
                        const juce::Rectangle<float> &bounds,
                        const juce::String &title,
                        const juce::String &subtitle,
                        const juce::String &badge,
                        juce::Colour accent,
                        bool isSelected,
                        bool isMissing);

  void drawRailNodeBadge(juce::Graphics &g,
                         const juce::Rectangle<float> &bounds,
                         const juce::String &text,
                         juce::Colour color);

} // namespace TIOControlNodeRenderer

} // namespace Teul
