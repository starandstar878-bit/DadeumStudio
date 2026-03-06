#pragma once

#include "../../Model/TGraphDocument.h"
#include <JuceHeader.h>

namespace Teul {

// =============================================================================
//  TGraphCanvas — 정밀한 오디오 DSP 그래프 인피니트 캔버스
//
//  [기능]
//  - 무한 확장/이동 가능한 World 토대를 렌더하고 사용자 View 영역을 투영
//  - 마우스 줌, 패닝(Pan) 기능
//  - 줌 레벨에 따라 LOD (가시성) 가 자동 조절되는 그리드(Grid) 렌더
// =============================================================================
class TGraphCanvas : public juce::Component {
public:
  explicit TGraphCanvas(TGraphDocument &doc);
  ~TGraphCanvas() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  void mouseDown(const juce::MouseEvent &event) override;
  void mouseDrag(const juce::MouseEvent &event) override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event,
                      const juce::MouseWheelDetails &wheel) override;

  // 키보드 입력 포커스용
  bool keyPressed(const juce::KeyPress &key) override;
  bool keyStateChanged(bool isKeyDown) override;

  // Viewport 조작 헬퍼
  void setZoomLevel(float newZoom, juce::Point<float> anchorPosView);

  juce::Point<float> viewToWorld(juce::Point<float> viewPos) const;
  juce::Point<float> worldToView(juce::Point<float> worldPos) const;

private:
  void drawInfiniteGrid(juce::Graphics &g);
  void drawZoomIndicator(juce::Graphics &g);

  // 문서 데이터 참조 (렌더링 및 상태 복원/저장용)
  TGraphDocument &document;

  float zoomLevel = 1.0f;
  juce::Point<float>
      viewOriginWorld; // 월드 좌표계 기준 현재 화면의 좌측 상단 위치

  struct PanState {
    bool active = false;
    juce::Point<float> startMouseView;
    juce::Point<float> startViewOriginWorld;
  } panState;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphCanvas)
};

} // namespace Teul
