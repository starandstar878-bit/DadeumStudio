#pragma once

#include "../../Model/TGraphDocument.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

class TNodeComponent;
class TPortComponent;
class TNodeRegistry;

class TGraphCanvas : public juce::Component, private juce::Timer {
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

  bool keyPressed(const juce::KeyPress &key) override;
  bool keyStateChanged(bool isKeyDown) override;

  void setZoomLevel(float newZoom, juce::Point<float> anchorPosView);

  juce::Point<float> viewToWorld(juce::Point<float> viewPos) const;
  juce::Point<float> worldToView(juce::Point<float> worldPos) const;

  TGraphDocument &getDocument() { return document; }
  const TGraphDocument &getDocument() const { return document; }

  void rebuildNodeComponents();
  void updateChildPositions();

  void beginConnectionDrag(const TPort &sourcePort,
                           juce::Point<float> mousePosView);
  void updateConnectionDrag(juce::Point<float> mousePosView);
  void endConnectionDrag(juce::Point<float> mousePosView);
  void cancelConnectionDrag();

  using ConnectionLevelProvider = std::function<float(const TConnection &)>;
  void setConnectionLevelProvider(ConnectionLevelProvider provider);

private:
  void timerCallback() override;

  void drawInfiniteGrid(juce::Graphics &g);
  void drawZoomIndicator(juce::Graphics &g);
  void drawConnections(juce::Graphics &g);

  juce::Path makeWirePath(juce::Point<float> from,
                          juce::Point<float> to) const;
  juce::Colour colorForPortType(TPortDataType type) const;

  const TPort *findPortModel(NodeId nodeId, PortId portId) const;

  TNodeComponent *findNodeComponent(NodeId nodeId) noexcept;
  const TNodeComponent *findNodeComponent(NodeId nodeId) const noexcept;

  TPortComponent *findPortComponent(NodeId nodeId, PortId portId) noexcept;
  const TPortComponent *findPortComponent(NodeId nodeId,
                                          PortId portId) const noexcept;

  juce::Point<float> portCentreInCanvas(NodeId nodeId, PortId portId) const;

  ConnectionId hitTestConnection(juce::Point<float> pointView,
                                 float hitThickness = 7.0f) const;

  void clearDragTargetHighlight();
  void updateDragTargetFromMouse(juce::Point<float> mousePosView);
  bool isCurrentDragTargetConnectable() const;
  void tryCreateConnectionFromDrag();

  void deleteConnectionWithAnimation(ConnectionId connectionId,
                                     juce::Point<float> breakPointView,
                                     juce::Point<float> impulseView);

  void startDisconnectAnimation(juce::Point<float> anchorPosView,
                                juce::Point<float> loosePosView,
                                juce::Point<float> impulseView,
                                TPortDataType type);

  TGraphDocument &document;

  float zoomLevel = 1.0f;
  juce::Point<float> viewOriginWorld;

  struct PanState {
    bool active = false;
    juce::Point<float> startMouseView;
    juce::Point<float> startViewOriginWorld;
  } panState;

  std::vector<std::unique_ptr<TNodeComponent>> nodeComponents;

  struct WireDragState {
    bool active = false;
    NodeId sourceNodeId = kInvalidNodeId;
    PortId sourcePortId = kInvalidPortId;
    TPortDataType sourceType = TPortDataType::Audio;

    juce::Point<float> sourcePosView;
    juce::Point<float> mousePosView;

    NodeId targetNodeId = kInvalidNodeId;
    PortId targetPortId = kInvalidPortId;

    bool targetTypeMatch = false;
    bool targetCycleFree = false;
  } wireDragState;

  ConnectionId selectedConnectionId = kInvalidConnectionId;
  ConnectionId pressedConnectionId = kInvalidConnectionId;
  juce::Point<float> pressedConnectionPointView;
  bool connectionBreakDragArmed = false;

  struct DisconnectAnimationState {
    bool active = false;
    juce::Point<float> anchorPosView;
    juce::Point<float> loosePosView;
    juce::Point<float> velocityViewPerSecond;
    float alpha = 0.0f;
    TPortDataType type = TPortDataType::Audio;
  } disconnectAnimation;

  float flowPhase = 0.0f;
  ConnectionLevelProvider connectionLevelProvider;

  const TNodeRegistry *nodeRegistry = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphCanvas)
};

} // namespace Teul
