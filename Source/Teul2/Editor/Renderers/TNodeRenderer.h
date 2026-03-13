#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include "Teul2/Editor/Renderers/TPortRenderer.h"

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

class TGraphCanvas;
struct TNodeDescriptor;

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

  const int headerHeight = 30;
  const int portRowHeight = 20;
  const int minNodeWidth = 132;
  const int cornerRadius = 7;

  bool isHoveringCollapse = false;

  void recalculateHeight();
  void applyViewScale();
  int scaledInt(int value) const noexcept;
  float scaledFloat(float value) const noexcept;
  void updatePortIssueStates();
  int inputLaneWidthPx() const noexcept;
  int outputLaneWidthPx() const noexcept;
  int portRowGapLogical() const noexcept;
  int laneCaptionLogicalHeight() const noexcept;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNodeComponent)
};

using PortLevelReader = std::function<float(PortId)>;

enum class InlinePreviewKind {
  none,
  oscillator,
  adsr,
  meter,
  meterTall,
};

InlinePreviewKind inlinePreviewKindFor(const TNodeDescriptor *descriptor);
int previewHeightForKind(InlinePreviewKind kind);
int minWidthForKind(InlinePreviewKind kind);
juce::Point<int> measureNodeSize(const TNodeDescriptor *descriptor,
                                 int inputPortCount,
                                 int outputPortCount,
                                 bool collapsed = false);
juce::Rectangle<float> makePreviewBounds(const juce::Rectangle<float> &bounds,
                                         InlinePreviewKind kind);
void drawOscillatorPreview(juce::Graphics &g,
                           const juce::Rectangle<float> &bounds,
                           const TNode &node);
void drawAdsrPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                     const TNode &node);
void drawMeterPreview(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                      const TNodeDescriptor *descriptor, const TNode &node,
                      const PortLevelReader &portLevelReader = {});

} // namespace Teul