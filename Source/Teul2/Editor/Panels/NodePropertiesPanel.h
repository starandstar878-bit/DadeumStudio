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

struct GyeolBindingPresentation {
  juce::String text;
  juce::Colour colour;
};

juce::String makeIeumBindingText(const TParamSpec &spec);
GyeolBindingPresentation makeGyeolBindingPresentation(
    const TParamSpec &spec, const juce::String &paramId,
    ParamBindingSummaryResolver bindingSummaryResolver);

std::unique_ptr<juce::Component> createParamEditor(const TParamSpec &spec,
                                                   const juce::var &value);
int editorHeightFor(const juce::Component &editor);
juce::var readEditorValue(const juce::Component &editor,
                          const TParamSpec &spec,
                          const juce::var &originalValue);

bool varEquals(const juce::var &lhs, const juce::var &rhs);
TParamValueType inferValueType(const juce::var &value);
bool isNumericValue(const juce::var &value);
double numericValue(const juce::var &value);
bool hasNumericRange(const TParamSpec &spec);
double sliderIntervalFor(const TParamSpec &spec);
TParamSpec makeFallbackParamSpec(const juce::String &key,
                                 const juce::var &value);
TParamSpec makeSpecFromExposedParam(const TTeulExposedParam &param);
juce::String formatValueForDisplay(const juce::var &value,
                                   const TParamSpec &spec);
juce::var parseTextValue(const juce::String &text,
                         const TParamSpec &spec,
                         const juce::var &prototype);

} // namespace Teul