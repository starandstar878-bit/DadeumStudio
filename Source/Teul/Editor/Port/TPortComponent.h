#pragma once

#include "Teul/Model/TPort.h"
#include <JuceHeader.h>

namespace Teul {

class TNodeComponent;

class TPortComponent : public juce::Component {
public:
  TPortComponent(TNodeComponent &owner, const TPort &port);
  ~TPortComponent() override;

  void paint(juce::Graphics &g) override;

  void mouseEnter(const juce::MouseEvent &e) override;
  void mouseExit(const juce::MouseEvent &e) override;
  void mouseDown(const juce::MouseEvent &e) override;
  void mouseDrag(const juce::MouseEvent &e) override;
  void mouseUp(const juce::MouseEvent &e) override;

  const TPort &getPortData() const { return portData; }
  void setDragTargetHighlight(bool enabled, bool validType);

private:
  TNodeComponent &ownerNode;
  TPort portData;

  bool isHovered = false;
  bool isDragTargetHighlighted = false;
  bool isDragTargetTypeValid = true;

  juce::Colour getPortColor() const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TPortComponent)
};

} // namespace Teul
