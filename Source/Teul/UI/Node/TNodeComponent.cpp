#include "TNodeComponent.h"
#include "../../UI/TeulPalette.h"
#include "../Canvas/TGraphCanvas.h"
#include <algorithm>

namespace Teul {

TNodeComponent::TNodeComponent(TGraphCanvas &canvas, NodeId id,
                               const TNodeDescriptor *desc)
    : ownerCanvas(canvas), nodeId(id), descriptor(desc) {
  setRepaintsOnMouseActivity(true);

  // 생성 직후 모델 데이터를 기반으로 포트 레이아웃 등 초기 렌더링 셋업
  updateFromModel();
}

TNodeComponent::~TNodeComponent() = default;

void TNodeComponent::updateFromModel() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  if (!nodePtr)
    return;

  // 1. 포트들(잭) 재생성 및 배치
  inPorts.clear();
  outPorts.clear();

  for (const auto &portData : nodePtr->ports) {
    auto comp = std::make_unique<TPortComponent>(*this, portData);
    addAndMakeVisible(comp.get());
    if (portData.direction == TPortDirection::Input) {
      inPorts.push_back(std::move(comp));
    } else {
      outPorts.push_back(std::move(comp));
    }
  }

  // 2. 바디 크기 산출
  recalculateHeight();

  // 3. 캔버스 투영 뷰 위치 부여
  setTopLeftPosition(
      ownerCanvas.worldToView({nodePtr->x, nodePtr->y}).roundToInt());
}

void TNodeComponent::recalculateHeight() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  bool collapsed = nodePtr ? nodePtr->collapsed : false;

  if (collapsed) {
    setSize(minNodeWidth, headerHeight);
    return;
  }

  const size_t maxPorts = std::max(inPorts.size(), outPorts.size());
  // 헤더 영역 + 포트 나열 로우(Row)들 + 하단 여백/파라미터 박스
  int totalHeight = headerHeight + (int)maxPorts * portRowHeight + 20;
  setSize(minNodeWidth, totalHeight);
}

void TNodeComponent::resized() {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  bool collapsed = nodePtr ? nodePtr->collapsed : false;

  // 헤더 하단에 포트들을 좌우로 줄 세움 (14x14 잭 사이즈 사용, 포트 여백 고려)
  int y = headerHeight + 8;
  for (auto &p : inPorts) {
    p->setBounds(-7, y, 14, 14); // 반쯤 노출된 스타일로 배치 (좌측면)
    p->setVisible(!collapsed);
    y += portRowHeight;
  }

  y = headerHeight + 8;
  for (auto &p : outPorts) {
    p->setBounds(getWidth() - 7, y, 14, 14); // 마찬가치로 우측면 튀어나오게
    p->setVisible(!collapsed);
    y += portRowHeight;
  }
}

void TNodeComponent::paint(juce::Graphics &g) {
  const TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  bool bypassed = nodePtr ? nodePtr->bypassed : false;
  bool collapsed = nodePtr ? nodePtr->collapsed : false;
  bool hasError = nodePtr ? nodePtr->hasError : false;

  // 노드 바운더리용 영역 (잭이 튀어나온 상태이므로 로컬 바운즈 내 축소 필요.
  // 실제 바디 너비 사용)
  auto bounds = getLocalBounds().toFloat();

  if (bypassed) {
    g.setOpacity(0.4f);
  }

  // 1. 바디 배경 렌더링
  g.setColour(TeulPalette::NodeBackground);
  g.fillRoundedRectangle(bounds, cornerRadius);

  // 2. 헤더 바 렌더링
  juce::Rectangle<float> headerBounds = bounds.withHeight((float)headerHeight);
  g.setColour(TeulPalette::NodeHeader);
  if (collapsed) {
    g.fillRoundedRectangle(headerBounds, cornerRadius);
  } else {
    g.fillRoundedRectangle(headerBounds, cornerRadius);
    // 헤더 아래쪽은 직각으로 바디와 합쳐지게 처리
    g.fillRect(headerBounds.withTop(headerBounds.getBottom() - cornerRadius));
  }

  // 헤더 이름 렌더
  juce::String title = descriptor ? descriptor->displayName : "Unknown";
  g.setFont(juce::FontOptions(14.0f, juce::Font::bold));

  if (hasError) {
    g.setColour(juce::Colour(0xfff87171)); // Red/Orange warning
    title =
        juce::String::fromUTF8("\xe2\x9a\xa0 ") + title; // ⚠️ 마이크로 펄스 배지
  } else {
    g.setColour(juce::Colours::white);
  }
  g.drawText(title, headerBounds.reduced(8.0f, 0).toNearestInt(),
             juce::Justification::centredLeft);

  // 미니마이즈 토글 버튼 렌더
  auto colBtn = getCollapseButtonBounds().toFloat();
  g.setColour(isHoveringCollapse ? juce::Colours::white
                                 : juce::Colours::lightgrey);
  g.drawText(collapsed ? "+" : "-", colBtn.toNearestInt(),
             juce::Justification::centred);

  // 3. 사이드 포트 라벨 (잭 렌더는 TPortComponent가 수행하고 여기선 텍스트만
  // 처리)
  if (!collapsed) {
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(11.0f));

    int yIn = headerHeight + 8 - 1; // 텍스트 렌더링 수직 정렬 미세조정
    for (auto &p : inPorts) {
      g.drawText(p->getPortData().name, 12, yIn, 60, 14,
                 juce::Justification::centredLeft);
      yIn += portRowHeight;
    }

    int yOut = headerHeight + 8 - 1;
    for (auto &p : outPorts) {
      g.drawText(p->getPortData().name, getWidth() - 12 - 60, yOut, 60, 14,
                 juce::Justification::centredRight);
      yOut += portRowHeight;
    }
  }

  // 4. 하이라이트 (Selection 상태) 외곽선 렌더
  if (isSelected) {
    g.setColour(TeulPalette::NodeBorderSelected);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.5f);
  } else {
    g.setColour(TeulPalette::NodeBorder);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
  }

  if (hasError) {
    // 빨간색 테두리 덧그리기 (에러 글로우)
    g.setColour(juce::Colour(0x60f87171));
    g.drawRoundedRectangle(bounds.expanded(2.0f), cornerRadius + 2.0f, 2.0f);
  }
}

juce::Rectangle<int> TNodeComponent::getCollapseButtonBounds() const {
  return juce::Rectangle<int>(getWidth() - 28, (headerHeight - 20) / 2, 20, 20);
}

void TNodeComponent::mouseMove(const juce::MouseEvent &e) {
  bool hover = getCollapseButtonBounds().contains(e.getPosition());
  if (isHoveringCollapse != hover) {
    isHoveringCollapse = hover;
    repaint(getCollapseButtonBounds());
  }
}

void TNodeComponent::mouseDown(const juce::MouseEvent &e) {
  if (getCollapseButtonBounds().contains(e.getPosition())) {
    TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
    if (nodePtr) {
      nodePtr->collapsed = !nodePtr->collapsed;
      recalculateHeight();
      resized();
      repaint();
      ownerCanvas.repaint();
    }
    return;
  }

  dragger.startDraggingComponent(this, e);
  isSelected = true; // [Phase 2] 일단 클릭하면 단일 선택됨으로 렌더용 플래그 On
  toFront(false);    // 컴포넌트 자신을 부모 내 최상단으로 올림
  repaint();
}

void TNodeComponent::mouseDrag(const juce::MouseEvent &e) {
  dragger.dragComponent(this, e, nullptr);

  // [Phase 2] 라이브 드래그 - 캔버스 월드 좌표계를 투영하여 실시간 문서 데이터
  // 동기화
  TNode *nodePtr = ownerCanvas.getDocument().findNode(nodeId);
  if (nodePtr) {
    auto newWorldPos = ownerCanvas.viewToWorld(getPosition().toFloat());
    nodePtr->x = newWorldPos.x;
    nodePtr->y = newWorldPos.y;
  }
}

void TNodeComponent::mouseUp(const juce::MouseEvent &e) {
  // TODO: 추후 THistoryStack / TCommand 패턴과 엮어 포지션 이동 커밋 및 Undo
  // 블럭 생성
}

} // namespace Teul
