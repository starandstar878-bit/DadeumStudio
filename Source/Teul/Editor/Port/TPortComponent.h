#pragma once

#include "Teul/Editor/TIssueState.h"
#include "Teul/Model/TPort.h"
#include <JuceHeader.h>
#include <vector>

namespace Teul {

class TNodeComponent;

class TPortComponent : public juce::Component {
public:
  struct HitResult {
    bool hit = false;
    bool bundle = false;
    PortId portId = kInvalidPortId;
    int channelCount = 1;
  };

  struct PortIssueState {
    PortId portId = kInvalidPortId;
    TIssueState state = TIssueState::none;

    bool operator==(const PortIssueState &other) const noexcept {
      return portId == other.portId && state == other.state;
    }
  };

  TPortComponent(TNodeComponent &owner, const TPort &port);
  TPortComponent(TNodeComponent &owner, std::vector<TPort> ports);
  ~TPortComponent() override;

  void paint(juce::Graphics &g) override;

  void mouseEnter(const juce::MouseEvent &e) override;
  void mouseMove(const juce::MouseEvent &e) override;
  void mouseExit(const juce::MouseEvent &e) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  const TPort &getPortData() const { return portGroup.front(); }
  const std::vector<TPort> &getPortGroup() const noexcept { return portGroup; }
  bool containsPort(PortId portId) const noexcept;
  juce::Point<float> localAnchorForPort(PortId portId) const;
  juce::String getDisplayName() const;
  HitResult hitTestLocal(juce::Point<float> point) const;

  void setDragTargetHighlight(bool enabled, bool validType,
                              PortId highlightedPortId = kInvalidPortId,
                              bool highlightBundle = false);
  void setIssueState(std::vector<PortIssueState> issueStates,
                     TIssueState bundleIssueState = TIssueState::none);
  void setScaleFactor(float newScale);

private:
  TNodeComponent &ownerNode;
  std::vector<TPort> portGroup;

  float scaleFactor = 1.0f;
  PortId hoveredPortId = kInvalidPortId;
  bool hoveredBundle = false;
  bool isDragTargetHighlighted = false;
  bool isDragTargetTypeValid = true;
  PortId highlightedPortId = kInvalidPortId;
  bool highlightBundle = false;
  std::vector<PortIssueState> issueStates;
  TIssueState bundleIssueState = TIssueState::none;
  bool dragActive = false;
  PortId dragSourcePortId = kInvalidPortId;
  bool dragSourceBundle = false;

  bool isBus() const noexcept { return portGroup.size() > 1; }
  bool isInput() const noexcept {
    return getPortData().direction == TPortDirection::Input;
  }
  juce::Colour getPortColor() const;
  juce::Rectangle<float> interactionBounds() const;
  juce::Rectangle<float> socketArea() const;
  juce::Rectangle<float> monoBounds() const;
  juce::Rectangle<float> busOuterBounds() const;
  std::vector<juce::Rectangle<float>> channelBounds() const;
  TIssueState issueStateForPort(PortId portId) const noexcept;
  void updateHoverState(juce::Point<float> point);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TPortComponent)
};

} // namespace Teul
