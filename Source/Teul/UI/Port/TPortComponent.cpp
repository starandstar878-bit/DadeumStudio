#include "TPortComponent.h"
#include "../../UI/TeulPalette.h"
#include "../Node/TNodeComponent.h"

namespace Teul {

TPortComponent::TPortComponent(TNodeComponent &owner, const TPort &port)
    : ownerNode(owner), portData(port) {
  setSize(14, 14); // 잭의 기본 사이즈
}

TPortComponent::~TPortComponent() {}

juce::Colour TPortComponent::getPortColor() const {
  switch (portData.dataType) {
  case TPortDataType::Audio:
    return TeulPalette::PortAudio;
  case TPortDataType::CV:
    return TeulPalette::PortCV;
  case TPortDataType::MIDI:
    return TeulPalette::PortMIDI;
  case TPortDataType::Control:
    return TeulPalette::PortControl;
  default:
    return juce::Colours::grey;
  }
}

void TPortComponent::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();
  juce::Colour baseColor = getPortColor();

  if (isHovered) {
    // 호버 시 살짝 밝게 + 미세한 외곽 광원 효과
    g.setColour(baseColor.brighter(0.2f));
    g.fillEllipse(bounds.reduced(1.0f));

    g.setColour(baseColor.withAlpha(0.4f));
    g.drawEllipse(bounds, 1.5f);
  } else {
    // 기본 포트 렌더링
    g.setColour(baseColor);
    g.fillEllipse(bounds.reduced(2.0f));

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(bounds.reduced(2.0f), 1.0f);
  }
}

void TPortComponent::mouseEnter(const juce::MouseEvent &) {
  isHovered = true;
  repaint();
}

void TPortComponent::mouseExit(const juce::MouseEvent &) {
  isHovered = false;
  repaint();
}

// TODO: Phase 3 (연결선 드로잉 시스템)용 핸들러 예약
void TPortComponent::mouseDown(const juce::MouseEvent &) {}
void TPortComponent::mouseDrag(const juce::MouseEvent &) {}
void TPortComponent::mouseUp(const juce::MouseEvent &) {}

} // namespace Teul
