#include "TIOControlNodeRenderer.h"
#include "Teul2/Editor/Theme/TeulPalette.h" // 가상의 팔레트 소유 가정

namespace Teul {

// =============================================================================
//  TRailNodeComponent Implementation
// =============================================================================

TRailNodeComponent::TRailNodeComponent(NodeId id, const TNode &nodeData)
    : nodeId(id), accentColor(juce::Colours::slategrey) {
  updateFromModel(nodeData);
}

TRailNodeComponent::~TRailNodeComponent() {}

void TRailNodeComponent::updateFromModel(const TNode &nodeData) {
  title = nodeData.label.isNotEmpty() ? nodeData.label : nodeData.typeKey;
  subtitle = nodeData.isRailNode ? "System Interface" : "DSP Node";
  isMissing = false; // 향후 TNode 상태에 따라 업데이트
  hasError = nodeData.hasError;
  repaint();
}

void TRailNodeComponent::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  
  // 실제 렌더러 호출
  TIOControlNodeRenderer::drawRailNodeBody(
      g, bounds, title, subtitle, badgeText, accentColor, 
      false, // isSelected (임시)
      isMissing);
}

void TRailNodeComponent::resized() {
  // 컴포넌트 내부 포트 배치 로직 등이 향후 추가될 수 있음
}

void TRailNodeComponent::mouseDown(const juce::MouseEvent &e) {
  // 레일 노드는 이동(Dragger)을 지원하지 않지만, 선택 이벤트를 에디터로 전달할 수 있음
}

void TRailNodeComponent::mouseDrag(const juce::MouseEvent &e) {
  // 드래그 시 캔버스로의 와이어 생성 등을 여기서 트리거할 수 있음
}

void TRailNodeComponent::mouseUp(const juce::MouseEvent &e) {}

// =============================================================================
//  TIOControlNodeRenderer Implementation
// =============================================================================

void TIOControlNodeRenderer::drawRailNodeBody(juce::Graphics &g,
                                              const juce::Rectangle<float> &bounds,
                                              const juce::String &title,
                                              const juce::String &subtitle,
                                              const juce::String &badge,
                                              juce::Colour accent,
                                              bool isSelected,
                                              bool isMissing) {
  g.saveState();

  // 1. 배경 (Glassmorphism 스타일)
  auto body = bounds.reduced(1.0f);
  g.setColour(juce::Colour(0xff1a1a1a).withAlpha(0.85f));
  g.fillRoundedRectangle(body, 6.0f);

  // 2. 외곽선 및 액센트
  g.setColour(isSelected ? accent.brighter(0.3f) : accent.withAlpha(0.4f));
  g.drawRoundedRectangle(body, 6.0f, isSelected ? 2.0f : 1.0f);

  if (isMissing) {
    g.setColour(juce::Colours::red.withAlpha(0.2f));
    g.fillRoundedRectangle(body, 6.0f);
  }

  // 3. 텍스트 영역 계산
  auto textArea = body.reduced(10.0f, 6.0f);
  auto badgeArea = textArea.removeFromRight(44.0f);
  
  // 4. 타이틀 및 서브타이틀
  g.setColour(isMissing ? juce::Colours::red.brighter(0.5f) : juce::Colours::white.withAlpha(0.9f));
  g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
  g.drawText(title, textArea.removeFromTop(18.0f), juce::Justification::bottomLeft, true);

  g.setColour(juce::Colours::white.withAlpha(0.5f));
  g.setFont(10.0f);
  g.drawText(subtitle, textArea, juce::Justification::topLeft, true);

  // 5. 배지 (Badge)
  if (badge.isNotEmpty()) {
    drawRailNodeBadge(g, badgeArea.withSizeKeepingCentre(36, 16), badge, accent);
  }

  g.restoreState();
}

void TIOControlNodeRenderer::drawRailNodeBadge(juce::Graphics &g,
                                               const juce::Rectangle<float> &bounds,
                                               const juce::String &text,
                                               juce::Colour color) {
  g.setColour(color.withAlpha(0.15f));
  g.fillRoundedRectangle(bounds, 4.0f);
  
  g.setColour(color.withAlpha(0.8f));
  g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
  
  g.setColour(color.brighter(0.5f));
  g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
  g.drawText(text, bounds, juce::Justification::centred, false);
}

} // namespace Teul
