#pragma once

#include "Teul/Bridge/ITeulParamProvider.h"
#include "Teul2/Editor/TTeulEditor.h"
#include "Teul/Registry/TNodeRegistry.h"

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Teul {

struct TTeulDocument;
class TGraphCanvas;

class NodePropertiesPanel : public juce::Component {
public:
  struct AssignmentDropTarget {
    juce::String zoneId;
    juce::Rectangle<float> boundsTarget;
  };

  ~NodePropertiesPanel() override = default;

  virtual void setParamProvider(ITeulParamProvider *provider) = 0;
  virtual void setLayoutChangedCallback(std::function<void()> callback) = 0;
  virtual bool isPanelOpen() const noexcept = 0;
  virtual void inspectNode(NodeId nodeId) = 0;
  virtual void inspectFrame(int frameId) = 0;
  virtual void refreshFromDocument() = 0;
  virtual void refreshBindingSummaries() = 0;
  virtual std::vector<AssignmentDropTarget>
  assignmentDropTargetsIn(juce::Component &target) const = 0;
  virtual void setAssignmentDropTarget(const juce::String &zoneId,
                                       bool canConnect) = 0;
  virtual void hidePanel() = 0;

  static std::unique_ptr<NodePropertiesPanel> create(
      TTeulDocument &document, TGraphCanvas &canvas,
      const TNodeRegistry &registry, ITeulParamProvider *provider,
      ParamBindingSummaryResolver bindingSummaryResolver);
};

} // namespace Teul
