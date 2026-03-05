#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Gyeol::Ui::Panels {
class NavigatorPanel : public juce::Component, private juce::Timer {
public:
  struct SceneItem {
    juce::Rectangle<float> bounds;
    bool selected = false;
    bool visible = true;
    bool locked = false;
  };

  NavigatorPanel();

  void setScene(juce::Rectangle<float> worldBoundsIn,
                std::vector<SceneItem> itemsIn);
  void setViewState(juce::Rectangle<float> visibleWorldBoundsIn,
                    float zoomLevelIn);
  void setNavigateRequestedCallback(
      std::function<void(juce::Point<float>)> callback);
  void setZoomRequestedCallback(
      std::function<void(float, juce::Point<float>)> callback);
  void setPanelBoundsRequestedCallback(
      std::function<void(juce::Rectangle<int>)> callback);

  void enqueueDirtyRect(juce::Rectangle<float> worldDirtyRect);

  void paint(juce::Graphics &g) override;
  void resized() override;
  void lookAndFeelChanged() override;
  bool hitTest(int x, int y) override;

  void mouseEnter(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;
  void mouseDown(const juce::MouseEvent &event) override;
  void mouseDrag(const juce::MouseEvent &event) override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseMove(const juce::MouseEvent &event) override;

private:
  struct MapTransform {
    juce::Rectangle<float> mapBounds;
    juce::Rectangle<float> worldBounds;
    float scale = 1.0f;

    bool isValid() const noexcept {
      return mapBounds.getWidth() > 0.0f && mapBounds.getHeight() > 0.0f &&
             worldBounds.getWidth() > 0.0f && worldBounds.getHeight() > 0.0f &&
             scale > 0.0f;
    }

    juce::Point<float> worldToMap(juce::Point<float> worldPoint) const noexcept {
      return {mapBounds.getX() + (worldPoint.x - worldBounds.getX()) * scale,
              mapBounds.getY() + (worldPoint.y - worldBounds.getY()) * scale};
    }

    juce::Point<float> mapToWorld(juce::Point<float> mapPoint) const noexcept {
      return {worldBounds.getX() + (mapPoint.x - mapBounds.getX()) / scale,
              worldBounds.getY() + (mapPoint.y - mapBounds.getY()) / scale};
    }

    juce::Rectangle<float> worldToMap(const juce::Rectangle<float> &worldRect) const noexcept {
      const auto topLeft = worldToMap(worldRect.getTopLeft());
      return {topLeft.x, topLeft.y, worldRect.getWidth() * scale,
              worldRect.getHeight() * scale};
    }
  };

  enum class ResizeEdge {
    none,
    left,
    right,
    top,
    bottom,
    topLeft,
    topRight,
    bottomLeft,
    bottomRight
  };

  struct DirtyRectPulse {
    juce::Rectangle<float> worldRect;
    float alpha = 0.55f;
  };

  MapTransform computeMapTransform() const noexcept;
  juce::Rectangle<float> computeMapContainer() const noexcept;
  juce::Rectangle<float> dragHandleBounds() const noexcept;

  void requestNavigateAt(juce::Point<float> localPoint);
  ResizeEdge hitResizeEdge(juce::Point<float> localPoint,
                           const MapTransform &transform,
                           juce::Rectangle<float> &ioViewRect) const;
  void requestZoomFromResizedView(const MapTransform &transform,
                                  const juce::Rectangle<float> &resizedViewRect);
  void drawOffscreenIndicators(juce::Graphics &g,
                               const MapTransform &transform,
                               juce::Rectangle<float> mapBounds) const;

  void timerCallback() override;

  juce::Rectangle<float> worldBounds{0.0f, 0.0f, 1600.0f, 1000.0f};
  juce::Rectangle<float> visibleWorldBounds;
  std::vector<SceneItem> sceneItems;
  std::vector<DirtyRectPulse> dirtyRectPulses;

  std::function<void(juce::Point<float>)> onNavigateRequested;
  std::function<void(float, juce::Point<float>)> onZoomRequested;
  std::function<void(juce::Rectangle<int>)> onPanelBoundsRequested;

  float zoomLevel = 1.0f;
  float shellOpacity = 0.30f;
  float targetOpacity = 0.30f;

  bool panelDragActive = false;
  juce::Point<int> panelDragStart;
  juce::Rectangle<int> panelBoundsStart;

  bool viewResizeActive = false;
  ResizeEdge activeResizeEdge = ResizeEdge::none;
  juce::Rectangle<float> resizeStartViewMapBounds;

  bool navigateDragActive = false;
};
} // namespace Gyeol::Ui::Panels
