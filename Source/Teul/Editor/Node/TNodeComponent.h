#pragma once

#include "Teul/Model/TNode.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/Editor/Port/TPortComponent.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace Teul {

class TGraphCanvas;

class TNodeComponent : public juce::Component {
public:
  TNodeComponent(TGraphCanvas &canvas, NodeId id, const TNodeDescriptor *desc);
  ~TNodeComponent() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  void mouseMove(const juce::MouseEvent &e) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  NodeId getNodeId() const { return nodeId; }
  TGraphCanvas &getOwnerCanvas() noexcept { return ownerCanvas; }
  const TGraphCanvas &getOwnerCanvas() const noexcept { return ownerCanvas; }

  void updateFromModel();
  void setViewScale(float newScale);
  float getViewScale() const noexcept { return viewScale; }
  juce::Rectangle<int> getCollapseButtonBounds() const;

  TPortComponent *findPortComponent(PortId portId) noexcept;
  const TPortComponent *findPortComponent(PortId portId) const noexcept;

  void setPortDragHighlight(PortId portId, bool enabled, bool validType);

  const std::vector<std::unique_ptr<TPortComponent>> &getInputPorts() const
      noexcept {
    return inPorts;
  }

  const std::vector<std::unique_ptr<TPortComponent>> &getOutputPorts() const
      noexcept {
    return outPorts;
  }

  bool isSelected = false;

private:
  TGraphCanvas &ownerCanvas;
  NodeId nodeId;
  const TNodeDescriptor *descriptor;

  juce::ComponentDragger dragger;
  juce::Point<int> logicalSize;
  float viewScale = 1.0f;

  std::vector<std::unique_ptr<TPortComponent>> inPorts;
  std::vector<std::unique_ptr<TPortComponent>> outPorts;

  const int headerHeight = 32;
  const int portRowHeight = 22;
  const int minNodeWidth = 140;
  const int cornerRadius = 8;

  bool isHoveringCollapse = false;

  void recalculateHeight();
  void applyViewScale();
  int scaledInt(int value) const noexcept;
  float scaledFloat(float value) const noexcept;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNodeComponent)
};

} // namespace Teul
