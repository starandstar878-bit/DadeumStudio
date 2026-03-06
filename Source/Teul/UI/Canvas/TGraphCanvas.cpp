#include "TGraphCanvas.h"
#include "../TeulPalette.h"

#include "../../Registry/TNodeRegistry.h"
#include "../Node/TNodeComponent.h"

namespace Teul {

// 전역 레지스트리를 임시로 들고오기 위한 헬퍼 (AppServices 구조 연동 전까지)
static const TNodeRegistry *getSharedRegistry() {
  static auto reg = makeDefaultNodeRegistry();
  return reg.get();
}

TGraphCanvas::TGraphCanvas(TGraphDocument &doc) : document(doc) {
  setWantsKeyboardFocus(true);

  nodeRegistry = getSharedRegistry();

  // 이전 상태 복구 (문서 메타에서 위치/줌)
  viewOriginWorld = {doc.meta.canvasOffsetX, doc.meta.canvasOffsetY};
  zoomLevel = doc.meta.canvasZoom;

  // 최소치 방어
  if (zoomLevel < 0.1f)
    zoomLevel = 1.0f;

  // 컴포넌트들 초기 빌드
  rebuildNodeComponents();
}

TGraphCanvas::~TGraphCanvas() {
  // 컴포넌트 소멸 직전의 뷰 상태를 문서 메타에 보존
  document.meta.canvasOffsetX = viewOriginWorld.x;
  document.meta.canvasOffsetY = viewOriginWorld.y;
  document.meta.canvasZoom = zoomLevel;
}

void TGraphCanvas::rebuildNodeComponents() {
  nodeComponents.clear();

  for (const auto &n : document.nodes) {
    const TNodeDescriptor *desc = nullptr;
    if (nodeRegistry) {
      desc = nodeRegistry->descriptorFor(n.typeKey);
    }

    auto comp = std::make_unique<TNodeComponent>(*this, n.nodeId, desc);
    addAndMakeVisible(comp.get());
    nodeComponents.push_back(std::move(comp));
  }
}

void TGraphCanvas::updateChildPositions() {
  for (auto &compPtr : nodeComponents) {
    auto *tnc = dynamic_cast<TNodeComponent *>(compPtr.get());
    if (tnc) {
      // 뷰포트 변경될 때마다 월드->뷰 투영 다시 해줌
      const TNode *nodeData = document.findNode(tnc->getNodeId());
      if (nodeData) {
        tnc->setTopLeftPosition(
            worldToView(juce::Point<float>(nodeData->x, nodeData->y))
                .roundToInt());
        // 줌 레벨에 따라 스케일도 적용할 경우 (차후 아핀 변환 추가)
        tnc->setTransform(juce::AffineTransform::scale(zoomLevel));
      }
    }
  }
}

void TGraphCanvas::paint(juce::Graphics &g) {
  g.fillAll(TeulPalette::CanvasBackground);

  drawInfiniteGrid(g);

  // TODO [Phase 3]
  // - Connection 렌더링 호출 (nodeComponents 보다 아래에 그려져야 함)

  drawZoomIndicator(g);
}

void TGraphCanvas::resized() { updateChildPositions(); }

// =============================================================================
//  그리드 & HUD
// =============================================================================

void TGraphCanvas::drawInfiniteGrid(juce::Graphics &g) {
  g.saveState();

  // 줌 레벨별 그리드 크기 자동 조절 (LOD)
  // 가장 작은 칸 단위인 40px 을 기준으로 잡음.
  float baseGridSize = 40.0f;
  float scaledGridSize = baseGridSize * zoomLevel;

  // 줌-아웃이 너무 심해서 그리드가 조밀해지면 렌더링 스킵 (성능 보호)
  if (scaledGridSize < 5.0f) {
    g.restoreState();
    return;
  }

  auto localBounds = getLocalBounds().toFloat();

  // 현재 보이는 컴포넌트 화면을 월드 좌표상 직사각형으로 역산출
  juce::Rectangle<float> worldBounds(viewOriginWorld.x, viewOriginWorld.y,
                                     localBounds.getWidth() / zoomLevel,
                                     localBounds.getHeight() / zoomLevel);

  // 그리드 라인이 그려질 시작 지점 (위상 연산)
  float startX = std::floor(worldBounds.getX() / baseGridSize) * baseGridSize;
  float startY = std::floor(worldBounds.getY() / baseGridSize) * baseGridSize;

  g.setColour(TeulPalette::GridDot);

  for (float wx = startX; wx < worldBounds.getRight(); wx += baseGridSize) {
    for (float wy = startY; wy < worldBounds.getBottom(); wy += baseGridSize) {

      // 월드(wx, wy) 를 뷰 좌표로 변환
      float vx = (wx - viewOriginWorld.x) * zoomLevel;
      float vy = (wy - viewOriginWorld.y) * zoomLevel;

      // 점자 그리드
      g.fillRect(vx - 1.0f, vy - 1.0f, 2.0f, 2.0f);
    }
  }

  g.restoreState();
}

void TGraphCanvas::drawZoomIndicator(juce::Graphics &g) {
  juce::String zoomText =
      juce::String(juce::roundToInt(zoomLevel * 100.0f)) + "%";

  auto area =
      getLocalBounds().removeFromBottom(30).removeFromRight(80).reduced(5);
  g.setColour(juce::Colour(0x80000000));
  g.fillRoundedRectangle(area.toFloat(), 4.0f);

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(zoomText, area, juce::Justification::centred, false);
}

// =============================================================================
//  조작 인터랙션 (Zoom / Pan)
// =============================================================================

void TGraphCanvas::mouseDown(const juce::MouseEvent &event) {
  grabKeyboardFocus();

  // Space 홀드 + 클릭, 중간 클릭, Alt+클릭 시 Pan (패닝)
  bool isPanTrigger =
      event.mods.isMiddleButtonDown() || event.mods.isAltDown() ||
      juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);

  if (isPanTrigger) {
    panState.active = true;
    panState.startMouseView = event.position;
    panState.startViewOriginWorld = viewOriginWorld;
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
  }
}

void TGraphCanvas::mouseDrag(const juce::MouseEvent &event) {
  if (panState.active) {
    juce::Point<float> deltaView = event.position - panState.startMouseView;
    juce::Point<float> deltaWorld = deltaView / zoomLevel;

    // 패닝 반영 (마우스가 간 반대방향으로 뷰 원점이 이동해야 함)
    viewOriginWorld = panState.startViewOriginWorld - deltaWorld;
    repaint();
  }
}

void TGraphCanvas::mouseUp(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);
  if (panState.active) {
    panState.active = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
  }
}

void TGraphCanvas::mouseWheelMove(const juce::MouseEvent &event,
                                  const juce::MouseWheelDetails &wheel) {
  // Ctrl/Cmd 입력이 있거나, 휠 deltaY 가 특정 수준으로 왔을 때 모디파이어
  // 여부에 따라 분기
  if (event.mods.isCtrlDown() || event.mods.isCommandDown() ||
      event.mods.isAltDown()) {
    // Zoom in/out
    float zoomDelta = wheel.deltaY * 2.0f;
    float nextZoom = juce::jlimit(0.1f, 5.0f, zoomLevel + zoomDelta);
    setZoomLevel(nextZoom, event.position);
  } else {
    // 2D 터치패드 스크롤 또는 일반 휠 패닝 (위아래, 양옆)
    float panSpeedPixels = 50.0f; // 스크롤 민감도
    float deltaXWorld = (wheel.deltaX * panSpeedPixels) / zoomLevel;
    float deltaYWorld = (wheel.deltaY * panSpeedPixels) / zoomLevel;

    viewOriginWorld.x -= deltaXWorld;
    viewOriginWorld.y -= deltaYWorld;
    repaint();
  }
}

bool TGraphCanvas::keyPressed(const juce::KeyPress &key) {
  if (key == juce::KeyPress::spaceKey) {
    // 캔버스 자체에서 Space를 눌렀을 때 손바닥 커서로 일단 바꿔줌
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    return true;
  }
  return false;
}

bool TGraphCanvas::keyStateChanged(bool isKeyDown) {
  // Space가 풀렸는지 추적하여 기본 커서로 복원
  if (!juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)) {
    if (!panState.active) {
      setMouseCursor(juce::MouseCursor::NormalCursor);
    }
  } else {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
  }
  return false;
}

// =============================================================================
//  좌표계 변환 & 렌더링 헬퍼
// =============================================================================

void TGraphCanvas::setZoomLevel(float newZoom,
                                juce::Point<float> anchorPosView) {
  if (juce::exactlyEqual(newZoom, zoomLevel))
    return;

  // 1. 현재 화면 앵커(마우스커서)의 월드 좌표 백업
  juce::Point<float> anchorWorld = viewToWorld(anchorPosView);

  // 2. 줌 변경
  zoomLevel = newZoom;

  // 3. 백업했던 월드 좌표가 화면상 동일한 픽셀 위치(anchorPosView)에 남도록,
  // 뷰포트 원점을 보정
  viewOriginWorld.x = anchorWorld.x - (anchorPosView.x / zoomLevel);
  viewOriginWorld.y = anchorWorld.y - (anchorPosView.y / zoomLevel);

  repaint();
}

juce::Point<float> TGraphCanvas::viewToWorld(juce::Point<float> viewPos) const {
  return viewOriginWorld + (viewPos / zoomLevel);
}

juce::Point<float>
TGraphCanvas::worldToView(juce::Point<float> worldPos) const {
  return (worldPos - viewOriginWorld) * zoomLevel;
}

} // namespace Teul
