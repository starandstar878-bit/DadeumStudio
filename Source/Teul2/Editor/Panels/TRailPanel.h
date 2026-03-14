#pragma once

#include "Teul2/Runtime/IOControl/TRuntimeDeviceManager.h"
#include "Teul2/Editor/Renderers/TIOControlNodeRenderer.h"
#include "Teul2/Editor/Canvas/TGraphCanvas.h"
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

namespace Teul {

class TTeulDocument;
class TSystemRailEndpoint;

// =============================================================================
//  TRailPanel
//
//  캔버스의 좌/우/하단에 고정되어 물리적 I/O 및 컨트롤 장치를 보여주는 패널.
//  TRuntimeDeviceManager::Listener를 상속받아 장치 상태 변화를 실시간으로 반영합니다.
// =============================================================================
class TRailPanel : public juce::Component,
                   public TRuntimeDeviceManager::Listener {
public:
  TRailPanel(TTeulDocument &doc, TRuntimeDeviceManager &deviceMgr, TRailType type);
  ~TRailPanel() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  // TRuntimeDeviceManager::Listener
  void deviceStateChanged(const TRuntimeDeviceState &newState) override;

  void updateRailNodes();
  TRailType getRailType() const noexcept { return railType; }

  // TTeulEditor 호환성 유지를 위한 임시 API
  bool isRailCollapsed() const noexcept { return false; }
  void setCollapseHandler(std::function<void()> ) {}
  void setCardSelectionHandler(std::function<void(const juce::String&)> ) {}
  void setDropTargetPort(const juce::String&, const juce::String&, bool) {}
  void setSelectedCardId(const juce::String&) {}
  void refreshFromDocument() { updateRailNodes(); }
  void setEndpointSubtitleProvider(std::function<juce::String(const TSystemRailEndpoint&)>) {}
  void setPortDragTargetComponent(juce::Component*) {}
  void setPortDragHandlers(std::function<bool(const juce::String&, const juce::String&, juce::Point<float>, juce::Point<float>)>,
                           std::function<void(juce::Point<float>)>,
                           std::function<void(juce::Point<float>)>) {}
  
  std::vector<TGraphCanvas::ExternalDropZone> portDropTargetsIn(juce::Component&) const { return {}; }

private:
  TTeulDocument &document;
  TRuntimeDeviceManager &deviceManager;
  TRailType railType;

  std::vector<std::unique_ptr<TRailNodeComponent>> railNodes;

  void refreshLayout();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TRailPanel)
};

} // namespace Teul
