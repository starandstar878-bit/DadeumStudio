#include "TRailPanel.h"
#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Editor/Theme/TeulPalette.h"

namespace Teul {

TRailPanel::TRailPanel(TTeulDocument &doc, TRuntimeDeviceManager &deviceMgr, TRailType type)
    : document(doc), deviceManager(deviceMgr), railType(type) {
  deviceManager.addListener(this);
  updateRailNodes();
}

TRailPanel::~TRailPanel() {
  deviceManager.removeListener(this);
}

void TRailPanel::paint(juce::Graphics &g) {
  // 레일 배경: Recessed Look (더 어두운 배경)
  g.setColour(juce::Colour(0xff0d0d0d));
  g.fillAll();

  // 경계선
  g.setColour(juce::Colours::white.withAlpha(0.1f));
  if (railType == TRailType::Left)
    g.drawVerticalLine(getWidth() - 1, 0, (float)getHeight());
  else if (railType == TRailType::Right)
    g.drawVerticalLine(0, 0, (float)getHeight());
  else if (railType == TRailType::Bottom)
    g.drawHorizontalLine(0, 0, (float)getWidth());
}

void TRailPanel::resized() {
  refreshLayout();
}

void TRailPanel::deviceStateChanged(const TRuntimeDeviceState &newState) {
  // 장치 변경 시 UI 갱신 (비동기 처리 권장되나 여기선 일단 직접 호출)
  juce::MessageManager::callAsync([this]() {
    updateRailNodes();
  });
}

void TRailPanel::updateRailNodes() {
  railNodes.clear();

  // Document에서 이 레일에 속한 노드들을 찾아 컴포넌트 생성
  for (const auto &node : document.nodes) {
    if (node.isRailNode && node.railType == railType) {
      auto comp = std::make_unique<TRailNodeComponent>(node.nodeId, node);
      
      // 노드 타입별 액센트 색상 설정 (임시 로직)
      if (railType == TRailType::Left) {
        comp->setAccentColor(juce::Colours::cyan);
        comp->setBadgeText("IN");
      } else if (railType == TRailType::Right) {
        comp->setAccentColor(juce::Colours::magenta);
        comp->setBadgeText("OUT");
      } else {
        comp->setAccentColor(juce::Colours::orange);
        comp->setBadgeText("CTRL");
      }

      addAndMakeVisible(comp.get());
      railNodes.push_back(std::move(comp));
    }
  }

  refreshLayout();
}

void TRailPanel::refreshLayout() {
  auto area = getLocalBounds().reduced(8);
  const int nodeHeight = 50;
  const int gap = 8;

  if (railType == TRailType::Bottom) {
    // 하단 레일: 가로 배치
    int x = area.getX();
    for (auto &node : railNodes) {
      node->setBounds(x, area.getY(), 120, area.getHeight());
      x += 120 + gap;
    }
  } else {
    // 좌/우 레일: 세로 배치
    int y = area.getY();
    for (auto &node : railNodes) {
      node->setBounds(area.getX(), y, area.getWidth(), nodeHeight);
      y += nodeHeight + gap;
    }
  }
}

} // namespace Teul
