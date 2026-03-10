#include "Teul/Editor/Panels/NodePropertiesPanel.h"

#include "Teul/Editor/Panels/Property/BindingSummaryPresenter.h"
#include "Teul/Editor/Panels/Property/ParamEditorFactory.h"
#include "Teul/Editor/Panels/Property/ParamValueFormatter.h"
#include "Teul/History/TCommands.h"
#include "Teul/Model/TGraphDocument.h"
#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include <algorithm>
#include <map>
#include <set>

namespace Teul {
namespace {

static juce::String nodeLabelForInspector(const TNode &node,
                                          const TNodeRegistry &registry) {
  if (node.label.isNotEmpty())
    return node.label;

  if (const auto *desc = registry.descriptorFor(node.typeKey)) {
    if (desc->displayName.isNotEmpty())
      return desc->displayName;
  }

  return node.typeKey;
}

static juce::String colorTagFromId(int id) {
  switch (id) {
  case 2:
    return "red";
  case 3:
    return "orange";
  case 4:
    return "green";
  case 5:
    return "blue";
  default:
    return {};
  }
}

static int colorTagToId(const juce::String &tagRaw) {
  const juce::String tag = tagRaw.trim().toLowerCase();
  if (tag == "red")
    return 2;
  if (tag == "orange")
    return 3;
  if (tag == "green")
    return 4;
  if (tag == "blue")
    return 5;
  return 1;
}

static juce::uint32 frameColorFromId(int id) {
  switch (id) {
  case 2:
    return 0x33ef4444;
  case 3:
    return 0x33f59e0b;
  case 4:
    return 0x3322c55e;
  case 5:
    return 0x334d8bf7;
  default:
    return 0x334d8bf7;
  }
}

static int frameColorToId(juce::uint32 colorArgb) {
  switch (colorArgb) {
  case 0x33ef4444:
    return 2;
  case 0x33f59e0b:
    return 3;
  case 0x3322c55e:
    return 4;
  case 0x334d8bf7:
  default:
    return 5;
  }
}

static juce::String frameTypeSummary(const TFrameRegion &frame) {
  return juce::String::formatted(
      "%s / %s / %d members",
      frame.logicalGroup ? "Logical Frame Group" : "Visual Frame",
      frame.membershipExplicit ? "explicit membership"
                               : "bounds membership",
      (int)frame.memberNodeIds.size());
}

static juce::String frameSummaryText(const TFrameRegion &frame) {
  return "UUID: " + frame.frameUuid + "\nMembers: " +
         juce::String((int)frame.memberNodeIds.size()) + "\nMembership: " +
         (frame.membershipExplicit ? "Explicit list" : "Bounds-derived") +
         "\nLocked: " + (frame.locked ? "Yes" : "No") +
         "\nCollapsed: " + (frame.collapsed ? "Yes" : "No") +
         "\nBounds: " +
         juce::String::formatted("%.0f, %.0f / %.0f x %.0f", frame.x, frame.y,
                                 frame.width, frame.height);
}

static bool shouldShowIeumBindingLine(const TParamSpec &spec,
                                      const juce::String &text) {
  return text.isNotEmpty() &&
         (spec.exposeToIeum || spec.isAutomatable || spec.isModulatable ||
          spec.isReadOnly || text != "Ieum: hidden");
}

static bool shouldShowGyeolBindingLine(const juce::String &text) {
  return text.isNotEmpty() && text != "Gyeol: hidden";
}

class NodePropertiesPanelImpl final : public NodePropertiesPanel,
                            private juce::Timer,
                            private ITeulParamProvider::Listener {
public:
  NodePropertiesPanelImpl(TGraphDocument &docIn, TGraphCanvas &canvasIn,
                      const TNodeRegistry &registryIn,
                      ITeulParamProvider *providerIn,
                      ParamBindingSummaryResolver bindingSummaryResolverIn)
      : document(docIn), canvas(canvasIn), registry(registryIn),
        bindingSummaryResolver(std::move(bindingSummaryResolverIn)) {
    addAndMakeVisible(headerLabel);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(nameEditor);
    addAndMakeVisible(colorBox);
    addAndMakeVisible(bypassToggle);
    addAndMakeVisible(collapsedToggle);
    addAndMakeVisible(applyButton);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(paramViewport);
    addAndMakeVisible(nodeConnectionsBox);
    addAndMakeVisible(frameSummaryBox);
    addAndMakeVisible(frameCaptureButton);
    addAndMakeVisible(frameReleaseButton);
    addAndMakeVisible(frameFitButton);
    addAndMakeVisible(frameSavePresetButton);

    headerLabel.setText("Node Properties", juce::dontSendNotification);
    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.92f));
    headerLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    typeLabel.setJustificationType(juce::Justification::centredLeft);
    typeLabel.setColour(juce::Label::textColourId,
                        juce::Colours::white.withAlpha(0.58f));

    nameEditor.setTextToShowWhenEmpty("Custom node label", juce::Colours::grey);
    nameEditor.setColour(juce::TextEditor::backgroundColourId,
                         juce::Colour(0xff171717));
    nameEditor.setColour(juce::TextEditor::textColourId,
                         juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::outlineColourId,
                         juce::Colour(0xff343434));

    colorBox.addItem("None", 1);
    colorBox.addItem("Red", 2);
    colorBox.addItem("Orange", 3);
    colorBox.addItem("Green", 4);
    colorBox.addItem("Blue", 5);
    colorBox.setSelectedId(1, juce::dontSendNotification);

    bypassToggle.setButtonText("Bypassed");
    collapsedToggle.setButtonText("Collapsed");
    applyButton.setButtonText("Apply");
    closeButton.setButtonText("Hide");

    paramsContent = std::make_unique<juce::Component>();
    paramViewport.setViewedComponent(paramsContent.get(), false);
    paramViewport.setScrollBarsShown(true, false);

    nodeConnectionsBox.setMultiLine(true);
    nodeConnectionsBox.setReadOnly(true);
    nodeConnectionsBox.setScrollbarsShown(true);
    nodeConnectionsBox.setColour(juce::TextEditor::backgroundColourId,
                                 juce::Colour(0x66111827));
    nodeConnectionsBox.setColour(juce::TextEditor::outlineColourId,
                                 juce::Colour(0xff324154));
    nodeConnectionsBox.setColour(juce::TextEditor::textColourId,
                                 juce::Colours::white.withAlpha(0.78f));
    nodeConnectionsBox.setVisible(false);

    frameSummaryBox.setMultiLine(true);
    frameSummaryBox.setReadOnly(true);
    frameSummaryBox.setScrollbarsShown(true);
    frameSummaryBox.setColour(juce::TextEditor::backgroundColourId,
                              juce::Colour(0x66111827));
    frameSummaryBox.setColour(juce::TextEditor::outlineColourId,
                              juce::Colour(0xff324154));
    frameSummaryBox.setColour(juce::TextEditor::textColourId,
                              juce::Colours::white.withAlpha(0.78f));
    frameSummaryBox.setVisible(false);

    frameCaptureButton.setButtonText("Capture Sel");
    frameReleaseButton.setButtonText("Release Sel");
    frameFitButton.setButtonText("Fit Members");
    frameSavePresetButton.setButtonText("Save Preset");

    frameCaptureButton.onClick = [this] {
      if (inspectedFrameId != 0 &&
          canvas.captureSelectedNodesIntoFrame(inspectedFrameId))
        rebuildFromDocument();
    };
    frameReleaseButton.onClick = [this] {
      if (inspectedFrameId != 0 &&
          canvas.releaseSelectedNodesFromFrame(inspectedFrameId))
        rebuildFromDocument();
    };
    frameFitButton.onClick = [this] {
      if (inspectedFrameId != 0 && canvas.fitFrameToMembers(inspectedFrameId))
        rebuildFromDocument();
    };
    frameSavePresetButton.onClick = [this] {
      if (inspectedFrameId != 0)
        canvas.saveFrameAsPatchPreset(inspectedFrameId);
    };

    applyButton.onClick = [this] { applyChanges(); };
    closeButton.onClick = [this] { hidePanel(); };

    setParamProvider(providerIn);
    setVisible(false);
  }

  ~NodePropertiesPanelImpl() override {
    stopTimer();
    if (paramProvider != nullptr)
      paramProvider->removeListener(this);
  }

  void setParamProvider(ITeulParamProvider *provider) {
    if (paramProvider == provider) {
      refreshRuntimeSurface();
      updateRuntimeValueLabels();
      return;
    }

    if (paramProvider != nullptr)
      paramProvider->removeListener(this);

    paramProvider = provider;
    runtimeParamsById.clear();

    if (paramProvider != nullptr) {
      paramProvider->addListener(this);
      refreshRuntimeSurface();
      startTimerHz(12);
    } else {
      stopTimer();
    }

    updateRuntimeValueLabels();
  }

  void setLayoutChangedCallback(std::function<void()> callback) {
    onLayoutChanged = std::move(callback);
  }

  bool isPanelOpen() const noexcept {
    return isVisible() &&
           (inspectedNodeId != kInvalidNodeId || inspectedFrameId != 0);
  }

  void inspectNode(NodeId nodeId) {
    inspectedFrameId = 0;
    inspectedNodeId = nodeId;
    rebuildFromDocument();
    setVisible(true);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void inspectFrame(int frameId) {
    inspectedNodeId = kInvalidNodeId;
    inspectedFrameId = frameId;
    rebuildFromDocument();
    setVisible(true);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void refreshFromDocument() {
    if (!isPanelOpen())
      return;

    if (inspectedFrameId != 0) {
      if (document.findFrame(inspectedFrameId) == nullptr) {
        hidePanel();
        return;
      }
    } else if (document.findNode(inspectedNodeId) == nullptr) {
      hidePanel();
      return;
    }

    rebuildFromDocument();
  }

  void refreshBindingSummaries() {
    if (!isPanelOpen())
      return;

    updateRuntimeValueLabels();
    if (!isInspectingFrame())
      refreshNodeConnectionSummary();
  }

  std::vector<AssignmentDropTarget>
  assignmentDropTargetsIn(juce::Component &target) const override {
    std::vector<AssignmentDropTarget> targets;
    if (!isPanelOpen() || isInspectingFrame() || paramsContent == nullptr)
      return targets;

    for (const auto &entry : paramEditors) {
      if (!isAssignableParam(*entry))
        continue;

      const auto bounds = boundsForParamEditor(*entry);
      if (bounds.isEmpty())
        continue;

      targets.push_back(
          {entry->paramId,
           target.getLocalArea(paramsContent.get(), bounds).toFloat()});
    }

    return targets;
  }

  void setAssignmentDropTarget(const juce::String &zoneId,
                               bool canConnect) override {
    if (assignmentDropTargetId == zoneId &&
        assignmentDropTargetCanConnect == canConnect)
      return;

    assignmentDropTargetId = zoneId;
    assignmentDropTargetCanConnect = canConnect;
    repaint();
  }

  void hidePanel() {
    if (!isVisible() && inspectedNodeId == kInvalidNodeId &&
        inspectedFrameId == 0)
      return;

    inspectedNodeId = kInvalidNodeId;
    inspectedFrameId = 0;
    assignmentDropTargetId.clear();
    assignmentDropTargetCanConnect = false;
    nodeConnectionsBox.clear();
    clearParamEditors();
    frameSummaryBox.clear();
    setVisible(false);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void paint(juce::Graphics &g) override {
    const auto panelBounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0b1220),
                                           panelBounds.getCentreX(),
                                           panelBounds.getY(),
                                           juce::Colour(0xff111827),
                                           panelBounds.getCentreX(),
                                           panelBounds.getBottom(), false));
    g.fillRoundedRectangle(panelBounds, 12.0f);
    g.setColour(juce::Colour(0xff324154));
    g.drawRoundedRectangle(panelBounds, 12.0f, 1.0f);

    if (!isPanelOpen())
      return;

    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(24);
    area.removeFromTop(4);
    area.removeFromTop(18);
    area.removeFromTop(10);
    auto overview = area.removeFromTop(112);
    area.removeFromTop(10);
    auto state = area.removeFromTop(72);
    juce::Rectangle<int> connections;
    area.removeFromTop(12);
    if (!isInspectingFrame()) {
      connections = area.removeFromTop(108);
      area.removeFromTop(12);
    }
    auto params = area;

    auto drawSection = [&](juce::Rectangle<int> rect, const juce::String &title,
                           juce::Colour accent, const juce::String &subtitle) {
      g.setColour(juce::Colour(0x88182231));
      g.fillRoundedRectangle(rect.toFloat(), 10.0f);
      g.setColour(accent.withAlpha(0.32f));
      g.drawRoundedRectangle(rect.toFloat(), 10.0f, 1.0f);

      auto header = rect.reduced(12, 8)
                        .removeFromTop(subtitle.isNotEmpty() ? 28 : 18);
      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
      g.drawText(title, header.removeFromTop(16),
                 juce::Justification::centredLeft, false);
      if (subtitle.isNotEmpty()) {
        g.setColour(juce::Colours::white.withAlpha(0.48f));
        g.setFont(10.0f);
        g.drawText(subtitle, header, juce::Justification::centredLeft, false);
      }
    };

    drawSection(overview, "Overview", juce::Colour(0xff60a5fa), {});
    drawSection(state, "State", juce::Colour(0xfff59e0b), {});
    if (!isInspectingFrame()) {
      drawSection(connections, "Connection Setup", juce::Colour(0xfffbbf24),
                  "Signal routes and control assignments for the current node.");
    }
    drawSection(params, isInspectingFrame() ? "Group" : "Audio Parameters",
                juce::Colour(0xff22c55e),
                isInspectingFrame()
                    ? "Stored membership and identity metadata for this frame group."
                    : "Runtime and binding metadata appear only when relevant.");
  }
  void paintOverChildren(juce::Graphics &g) override {
    if (!isPanelOpen() || isInspectingFrame() || paramsContent == nullptr ||
        assignmentDropTargetId.isEmpty())
      return;

    const auto it = std::find_if(paramEditors.begin(), paramEditors.end(),
                                 [this](const auto &entry) {
                                   return entry->paramId == assignmentDropTargetId;
                                 });
    if (it == paramEditors.end())
      return;

    const auto bounds =
        getLocalArea(paramsContent.get(), boundsForParamEditor(*(*it))).toFloat();
    if (bounds.isEmpty())
      return;

    const auto accent = assignmentDropTargetCanConnect
                            ? juce::Colour(0xff22c55e)
                            : juce::Colour(0xfff97316);
    g.setColour(accent.withAlpha(assignmentDropTargetCanConnect ? 0.12f : 0.10f));
    g.fillRoundedRectangle(bounds.expanded(3.0f), 8.0f);
    g.setColour(accent.withAlpha(0.96f));
    g.drawRoundedRectangle(bounds.expanded(3.0f), 8.0f,
                           assignmentDropTargetCanConnect ? 2.0f : 1.6f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    auto header = area.removeFromTop(24);
    closeButton.setBounds(header.removeFromRight(56));
    headerLabel.setBounds(header);
    area.removeFromTop(4);
    typeLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(10);

    auto overview = area.removeFromTop(112).reduced(12, 28);
    nameEditor.setBounds(overview.removeFromTop(26));
    overview.removeFromTop(12);
    colorBox.setBounds(overview.removeFromTop(24));

    area.removeFromTop(10);
    auto state = area.removeFromTop(72).reduced(12, 28);
    auto toggles = state.removeFromTop(24);
    const int toggleWidth = juce::jmax(40, (toggles.getWidth() - 8) / 2);
    bypassToggle.setBounds(toggles.removeFromLeft(toggleWidth));
    toggles.removeFromLeft(8);
    collapsedToggle.setBounds(toggles);
    state.removeFromTop(10);
    applyButton.setBounds(state.removeFromTop(24).removeFromLeft(88));

    juce::Rectangle<int> connections;
    area.removeFromTop(12);
    if (!isInspectingFrame())
      connections = area.removeFromTop(108).reduced(12, 28);
    if (!isInspectingFrame())
      area.removeFromTop(12);
    auto params = area;
    params.removeFromTop(34);
    if (isInspectingFrame()) {
      nodeConnectionsBox.setBounds(0, 0, 0, 0);
      nodeConnectionsBox.setVisible(false);
      paramViewport.setBounds(0, 0, 0, 0);
      paramViewport.setVisible(false);
      frameSummaryBox.setVisible(true);

      auto actions = params.removeFromTop(56);
      auto topRow = actions.removeFromTop(24);
      const int topWidth = juce::jmax(80, (topRow.getWidth() - 8) / 2);
      frameCaptureButton.setVisible(true);
      frameCaptureButton.setBounds(topRow.removeFromLeft(topWidth));
      topRow.removeFromLeft(8);
      frameReleaseButton.setVisible(true);
      frameReleaseButton.setBounds(topRow);

      actions.removeFromTop(8);
      auto bottomRow = actions.removeFromTop(24);
      const int bottomWidth = juce::jmax(80, (bottomRow.getWidth() - 8) / 2);
      frameFitButton.setVisible(true);
      frameFitButton.setBounds(bottomRow.removeFromLeft(bottomWidth));
      bottomRow.removeFromLeft(8);
      frameSavePresetButton.setVisible(true);
      frameSavePresetButton.setBounds(bottomRow);

      params.removeFromTop(8);
      frameSummaryBox.setBounds(params);
    } else {
      frameCaptureButton.setVisible(false);
      frameReleaseButton.setVisible(false);
      frameFitButton.setVisible(false);
      frameSavePresetButton.setVisible(false);
      frameCaptureButton.setBounds(0, 0, 0, 0);
      frameReleaseButton.setBounds(0, 0, 0, 0);
      frameFitButton.setBounds(0, 0, 0, 0);
      frameSavePresetButton.setBounds(0, 0, 0, 0);
      frameSummaryBox.setBounds(0, 0, 0, 0);
      frameSummaryBox.setVisible(false);
      nodeConnectionsBox.setVisible(true);
      nodeConnectionsBox.setBounds(connections);
      paramViewport.setVisible(true);
      paramViewport.setBounds(params);
      layoutParamEditors();
    }
  }
private:
  bool isInspectingFrame() const noexcept { return inspectedFrameId != 0; }

  struct ParamEditor {
    TParamSpec spec;
    juce::String paramId;
    juce::var originalValue;
    std::unique_ptr<juce::Label> groupLabel;
    std::unique_ptr<juce::Label> caption;
    std::unique_ptr<juce::Label> descriptionLabel;
    std::unique_ptr<juce::Label> runtimeValueLabel;
    std::unique_ptr<juce::Label> bindingInfoLabel;
    std::unique_ptr<juce::Label> gyeolBindingLabel;
    std::unique_ptr<juce::Label> controlAssignmentLabel;
    std::unique_ptr<juce::Component> editor;
  };

  void timerCallback() override {
    if (!isPanelOpen() || paramProvider == nullptr)
      return;

    bool didChange = false;
    for (auto &entry : paramEditors) {
      if (entry->paramId.isEmpty())
        continue;

      auto it = runtimeParamsById.find(entry->paramId);
      if (it == runtimeParamsById.end())
        continue;

      const juce::var nextValue = paramProvider->getParam(entry->paramId);
      if (!nextValue.isVoid() && !varEquals(nextValue, it->second.currentValue)) {
        it->second.currentValue = nextValue;
        didChange = true;
      }
    }

    if (didChange)
      updateRuntimeValueLabels();
  }

  void teulParamSurfaceChanged() override {
    refreshRuntimeSurface();
    if (isPanelOpen())
      updateRuntimeValueLabels();
  }

  void teulParamValueChanged(const TTeulExposedParam &param) override {
    runtimeParamsById[param.paramId] = param;
    if (isPanelOpen() && param.nodeId == inspectedNodeId)
      updateRuntimeValueLabels();
  }

  void refreshRuntimeSurface() {
    runtimeParamsById.clear();
    if (paramProvider == nullptr)
      return;

    for (const auto &param : paramProvider->listExposedParams())
      runtimeParamsById[param.paramId] = param;
  }

  void rebuildFromDocument() {
    if (isInspectingFrame()) {
      const TFrameRegion *frame = document.findFrame(inspectedFrameId);
      if (frame == nullptr) {
        hidePanel();
        return;
      }

      clearParamEditors();
      headerLabel.setText(frame->title, juce::dontSendNotification);
      typeLabel.setText(frameTypeSummary(*frame), juce::dontSendNotification);
      nameEditor.setText(frame->title, juce::dontSendNotification);
      colorBox.setSelectedId(frameColorToId(frame->colorArgb),
                             juce::dontSendNotification);
      bypassToggle.setButtonText("Locked");
      bypassToggle.setToggleState(frame->locked, juce::dontSendNotification);
      collapsedToggle.setButtonText("Collapsed");
      collapsedToggle.setToggleState(frame->collapsed, juce::dontSendNotification);
      frameSummaryBox.setText(frameSummaryText(*frame), juce::dontSendNotification);

      const bool hasSelectedNodes = !canvas.getSelectedNodeIds().empty();
      const bool hasSelectedMembers = std::any_of(
          canvas.getSelectedNodeIds().begin(), canvas.getSelectedNodeIds().end(),
          [&](NodeId nodeId) { return frame->containsNode(nodeId); });
      frameCaptureButton.setEnabled(hasSelectedNodes);
      frameReleaseButton.setEnabled(hasSelectedMembers);
      frameFitButton.setEnabled(frame->membershipExplicit &&
                                !frame->memberNodeIds.empty());
      frameSavePresetButton.setEnabled(!frame->memberNodeIds.empty());

      resized();
      repaint();
      return;
    }

    const TNode *node = document.findNode(inspectedNodeId);
    if (node == nullptr) {
      hidePanel();
      return;
    }

    refreshRuntimeSurface();

    const auto *desc = registry.descriptorFor(node->typeKey);
    headerLabel.setText(nodeLabelForInspector(*node, registry),
                        juce::dontSendNotification);
    typeLabel.setText(desc != nullptr ? desc->displayName + " / " + desc->typeKey
                                      : node->typeKey,
                      juce::dontSendNotification);
    nameEditor.setText(node->label, juce::dontSendNotification);
    colorBox.setSelectedId(colorTagToId(node->colorTag),
                           juce::dontSendNotification);
    bypassToggle.setButtonText("Bypassed");
    bypassToggle.setToggleState(node->bypassed, juce::dontSendNotification);
    collapsedToggle.setButtonText("Collapsed");
    collapsedToggle.setToggleState(node->collapsed, juce::dontSendNotification);

    refreshNodeConnectionSummary();

    rebuildParamEditors(*node, desc);
    resized();
    repaint();
  }

  void clearParamEditors() {
    if (paramsContent != nullptr)
      paramsContent->removeAllChildren();

    paramEditors.clear();
    if (paramsContent != nullptr)
      paramsContent->setSize(0, 0);
  }

  void rebuildParamEditors(const TNode &node, const TNodeDescriptor *desc) {
    clearParamEditors();
    std::set<juce::String> seenKeys;
    juce::String lastGroup;

    auto appendParam = [&](TParamSpec spec, const juce::var &value) {
      if (!spec.showInPropertyPanel)
        return;

      auto entry = std::make_unique<ParamEditor>();
      entry->spec = std::move(spec);
      entry->paramId = makeTeulParamId(node.nodeId, entry->spec.key);
      entry->originalValue = value;

      if (entry->spec.group.isNotEmpty() && entry->spec.group != lastGroup) {
        entry->groupLabel = std::make_unique<juce::Label>();
        entry->groupLabel->setText(entry->spec.group, juce::dontSendNotification);
        entry->groupLabel->setJustificationType(juce::Justification::centredLeft);
        entry->groupLabel->setColour(juce::Label::textColourId,
                                     juce::Colours::white.withAlpha(0.86f));
        entry->groupLabel->setFont(juce::FontOptions(11.5f, juce::Font::bold));
        paramsContent->addAndMakeVisible(entry->groupLabel.get());
        lastGroup = entry->spec.group;
      }

      entry->caption = std::make_unique<juce::Label>();
      entry->caption->setText(entry->spec.label.isNotEmpty() ? entry->spec.label
                                                             : entry->spec.key,
                              juce::dontSendNotification);
      entry->caption->setJustificationType(juce::Justification::centredLeft);
      entry->caption->setColour(juce::Label::textColourId,
                                juce::Colours::white.withAlpha(0.74f));
      paramsContent->addAndMakeVisible(entry->caption.get());

      entry->editor = createParamEditor(entry->spec, value);

      entry->descriptionLabel = std::make_unique<juce::Label>();
      entry->descriptionLabel->setText(entry->spec.description,
                                       juce::dontSendNotification);
      entry->descriptionLabel->setJustificationType(juce::Justification::centredLeft);
      entry->descriptionLabel->setColour(juce::Label::textColourId,
                                         juce::Colours::white.withAlpha(0.46f));
      entry->descriptionLabel->setFont(juce::FontOptions(10.5f));
      entry->descriptionLabel->setVisible(entry->spec.description.isNotEmpty());
      paramsContent->addAndMakeVisible(entry->descriptionLabel.get());

      entry->runtimeValueLabel = std::make_unique<juce::Label>();
      entry->runtimeValueLabel->setJustificationType(juce::Justification::centredLeft);
      entry->runtimeValueLabel->setColour(juce::Label::textColourId,
                                          juce::Colour(0xff8fb8ff));
      entry->runtimeValueLabel->setFont(juce::FontOptions(10.0f, juce::Font::bold));
      entry->runtimeValueLabel->setBorderSize(juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->runtimeValueLabel.get());

      entry->bindingInfoLabel = std::make_unique<juce::Label>();
      entry->bindingInfoLabel->setJustificationType(juce::Justification::centredLeft);
      entry->bindingInfoLabel->setColour(juce::Label::textColourId,
                                         juce::Colours::white.withAlpha(0.44f));
      entry->bindingInfoLabel->setFont(juce::FontOptions(9.8f));
      entry->bindingInfoLabel->setBorderSize(juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->bindingInfoLabel.get());

      entry->gyeolBindingLabel = std::make_unique<juce::Label>();
      entry->gyeolBindingLabel->setJustificationType(juce::Justification::centredLeft);
      entry->gyeolBindingLabel->setColour(juce::Label::textColourId,
                                          juce::Colours::white.withAlpha(0.32f));
      entry->gyeolBindingLabel->setFont(juce::FontOptions(9.8f));
      entry->gyeolBindingLabel->setBorderSize(juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->gyeolBindingLabel.get());

      entry->controlAssignmentLabel = std::make_unique<juce::Label>();
      entry->controlAssignmentLabel->setJustificationType(
          juce::Justification::centredLeft);
      entry->controlAssignmentLabel->setColour(juce::Label::textColourId,
                                               juce::Colour(0xfffbbf24));
      entry->controlAssignmentLabel->setFont(juce::FontOptions(9.8f));
      entry->controlAssignmentLabel->setBorderSize(
          juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->controlAssignmentLabel.get());

      paramsContent->addAndMakeVisible(entry->editor.get());
      paramEditors.push_back(std::move(entry));
    };

    if (desc != nullptr) {
      for (const auto &spec : desc->paramSpecs) {
        seenKeys.insert(spec.key);
        if (!spec.showInPropertyPanel)
          continue;

        const auto it = node.params.find(spec.key);
        appendParam(spec, it != node.params.end() ? it->second : spec.defaultValue);
      }
    }

    for (const auto &pair : node.params) {
      if (seenKeys.find(pair.first) != seenKeys.end())
        continue;

      TParamSpec spec = makeFallbackParamSpec(pair.first, pair.second);
      const juce::String paramId = makeTeulParamId(node.nodeId, pair.first);
      if (const auto it = runtimeParamsById.find(paramId); it != runtimeParamsById.end())
        spec = makeSpecFromExposedParam(it->second);

      appendParam(std::move(spec), pair.second);
    }

    updateRuntimeValueLabels();
  }

  void layoutParamEditors() {
    if (paramsContent == nullptr)
      return;

    const int width = juce::jmax(140, paramViewport.getWidth() - 18);
    int y = 0;

    for (auto &entry : paramEditors) {
      if (entry->groupLabel != nullptr) {
        entry->groupLabel->setBounds(0, y, width, 18);
        y += 22;
      }

      entry->caption->setBounds(0, y, width, 18);
      y += 20;

      const int editorHeight = editorHeightFor(*entry->editor);
      entry->editor->setBounds(0, y, width, editorHeight);
      y += editorHeight + 6;

      if (entry->descriptionLabel->isVisible()) {
        entry->descriptionLabel->setBounds(0, y, width, 14);
        y += 18;
      }

      if (entry->runtimeValueLabel->isVisible()) {
        entry->runtimeValueLabel->setBounds(0, y, width, 18);
        y += 22;
      } else {
        entry->runtimeValueLabel->setBounds(0, 0, 0, 0);
      }

      if (entry->bindingInfoLabel->isVisible()) {
        entry->bindingInfoLabel->setBounds(0, y, width, 18);
        y += 22;
      } else {
        entry->bindingInfoLabel->setBounds(0, 0, 0, 0);
      }

      if (entry->gyeolBindingLabel->isVisible()) {
        entry->gyeolBindingLabel->setBounds(0, y, width, 18);
        y += 22;
      } else {
        entry->gyeolBindingLabel->setBounds(0, 0, 0, 0);
      }

      if (entry->controlAssignmentLabel->isVisible()) {
        entry->controlAssignmentLabel->setBounds(0, y, width, 18);
        y += 22;
      } else {
        entry->controlAssignmentLabel->setBounds(0, 0, 0, 0);
      }

      y += 8;
    }

    paramsContent->setSize(width, juce::jmax(y, paramViewport.getHeight()));
  }
  juce::var readEditorValue(const ParamEditor &entry) const {
    return Teul::readEditorValue(*entry.editor, entry.spec, entry.originalValue);
  }

  bool isAssignableParam(const ParamEditor &entry) const {
    return entry.paramId.isNotEmpty() && !entry.spec.isReadOnly &&
           (entry.spec.isAutomatable || entry.spec.isModulatable);
  }

  juce::Rectangle<int> boundsForParamEditor(const ParamEditor &entry) const {
    juce::Rectangle<int> bounds;
    auto unionWith = [&](const juce::Component *component) {
      if (component == nullptr || !component->isVisible())
        return;
      bounds = bounds.isEmpty() ? component->getBounds()
                               : bounds.getUnion(component->getBounds());
    };

    unionWith(entry.caption.get());
    unionWith(entry.editor.get());
    unionWith(entry.descriptionLabel.get());
    unionWith(entry.runtimeValueLabel.get());
    unionWith(entry.bindingInfoLabel.get());
    unionWith(entry.gyeolBindingLabel.get());
    unionWith(entry.controlAssignmentLabel.get());
    return bounds.expanded(4, 4);
  }

  juce::String controlAssignmentSummaryForParam(const ParamEditor &entry) const {
    if (entry.paramId.isEmpty())
      return {};

    juce::StringArray labels;
    for (const auto &assignment : document.controlState.assignments) {
      if (assignment.targetParamId != entry.paramId)
        continue;

      juce::String sourceLabel = assignment.sourceId;
      juce::String portLabel = assignment.portId;
      if (const auto *source = document.controlState.findSource(assignment.sourceId)) {
        sourceLabel = source->displayName.isNotEmpty() ? source->displayName
                                                       : source->sourceId;
        for (const auto &port : source->ports) {
          if (port.portId == assignment.portId) {
            portLabel = port.displayName;
            break;
          }
        }
      }

      labels.add(sourceLabel + " / " + portLabel);
    }

    if (labels.isEmpty())
      return {};

    juce::String text = "Control: " + labels[0];
    if (labels.size() > 1)
      text << " +" << juce::String(labels.size() - 1);
    return text;
  }

  juce::String nodeNameForSummary(const TNode &node) const {
    return nodeLabelForInspector(node, registry);
  }

  juce::String portNameForSummary(const TNode &node, PortId portId) const {
    if (const auto *port = node.findPort(portId))
      return port->name;
    return "Port";
  }

  juce::String buildNodeConnectionSummary(const TNode &node) const {
    juce::StringArray incoming;
    juce::StringArray outgoing;
    juce::StringArray controls;

    auto railPortLabel = [this](const TEndpoint &endpoint) {
      if (!endpoint.isRailPort())
        return juce::String();

      const auto *railEndpoint =
          document.controlState.findEndpoint(endpoint.railEndpointId);
      const auto *railPort =
          document.findSystemRailPort(endpoint.railEndpointId, endpoint.railPortId);
      if (railEndpoint == nullptr || railPort == nullptr)
        return endpoint.railEndpointId + " / " + endpoint.railPortId;

      return railEndpoint->displayName + " / " + railPort->displayName;
    };

    for (const auto &conn : document.connections) {
      if (conn.to.isNodePort() && conn.to.nodeId == node.nodeId) {
        if (conn.from.isNodePort()) {
          if (const auto *sourceNode = document.findNode(conn.from.nodeId)) {
            incoming.add(nodeNameForSummary(*sourceNode) + " / " +
                         portNameForSummary(*sourceNode, conn.from.portId) +
                         " -> " + portNameForSummary(node, conn.to.portId));
          }
        } else if (conn.from.isRailPort()) {
          incoming.add(railPortLabel(conn.from) + " -> " +
                       portNameForSummary(node, conn.to.portId));
        }
      }

      if (conn.from.isNodePort() && conn.from.nodeId == node.nodeId) {
        if (conn.to.isNodePort()) {
          if (const auto *targetNode = document.findNode(conn.to.nodeId)) {
            outgoing.add(portNameForSummary(node, conn.from.portId) + " -> " +
                         nodeNameForSummary(*targetNode) + " / " +
                         portNameForSummary(*targetNode, conn.to.portId));
          }
        } else if (conn.to.isRailPort()) {
          outgoing.add(portNameForSummary(node, conn.from.portId) + " -> " +
                       railPortLabel(conn.to));
        }
      }
    }

    for (const auto &assignment : document.controlState.assignments) {
      if (assignment.targetNodeId != node.nodeId)
        continue;

      juce::String sourceLabel = assignment.sourceId;
      juce::String portLabel = assignment.portId;
      if (const auto *source = document.controlState.findSource(assignment.sourceId)) {
        sourceLabel = source->displayName.isNotEmpty() ? source->displayName
                                                       : source->sourceId;
        for (const auto &port : source->ports) {
          if (port.portId == assignment.portId) {
            portLabel = port.displayName;
            break;
          }
        }
      }

      NodeId parsedNodeId = kInvalidNodeId;
      juce::String paramKey;
      juce::String targetLabel = assignment.targetParamId;
      if (parseTeulParamId(assignment.targetParamId, parsedNodeId, paramKey) &&
          parsedNodeId == node.nodeId && paramKey.isNotEmpty())
        targetLabel = paramKey;

      controls.add(sourceLabel + " / " + portLabel + " -> " + targetLabel);
    }

    auto joinSection = [](const juce::String &title,
                          const juce::StringArray &lines,
                          const juce::String &emptyText) {
      juce::String text = title + "\n";
      if (lines.isEmpty()) {
        text << "- " << emptyText;
      } else {
        for (const auto &line : lines)
          text << "- " << line << "\n";
        text = text.trimEnd();
      }
      return text;
    };

    juce::StringArray sections;
    sections.add(joinSection("Incoming Wires", incoming, "No incoming wires"));
    sections.add(joinSection("Outgoing Wires", outgoing, "No outgoing wires"));
    sections.add(joinSection("Control Assignments", controls,
                             "Drag a control source onto a parameter row"));
    return sections.joinIntoString("\n\n");
  }

  void refreshNodeConnectionSummary() {
    if (isInspectingFrame() || inspectedNodeId == kInvalidNodeId) {
      nodeConnectionsBox.clear();
      return;
    }

    if (const auto *node = document.findNode(inspectedNodeId)) {
      nodeConnectionsBox.setText(buildNodeConnectionSummary(*node), false);
      return;
    }

    nodeConnectionsBox.clear();
  }
  void updateRuntimeValueLabels() {
    for (auto &entry : paramEditors) {
      juce::String runtimeText;
      bool hasRuntimeValue = false;
      const auto it = runtimeParamsById.find(entry->paramId);
      if (it != runtimeParamsById.end()) {
        runtimeText = "Runtime: " +
                      formatValueForDisplay(it->second.currentValue, entry->spec);
        hasRuntimeValue = true;
      } else if (entry->spec.exposeToIeum && paramProvider != nullptr) {
        runtimeText = "Runtime: unavailable";
      } else {
        runtimeText = "Document: " +
                      formatValueForDisplay(entry->originalValue, entry->spec);
      }
      entry->runtimeValueLabel->setVisible(runtimeText.isNotEmpty());
      entry->runtimeValueLabel->setColour(
          juce::Label::backgroundColourId,
          hasRuntimeValue ? juce::Colour(0x221d4ed8)
                          : juce::Colour(0x221f2937));
      entry->runtimeValueLabel->setColour(
          juce::Label::textColourId,
          hasRuntimeValue ? juce::Colour(0xff93c5fd)
                          : juce::Colours::white.withAlpha(0.62f));
      entry->runtimeValueLabel->setText(runtimeText, juce::dontSendNotification);

      const auto ieumText = makeIeumBindingText(entry->spec);
      const bool showIeum = shouldShowIeumBindingLine(entry->spec, ieumText);
      entry->bindingInfoLabel->setVisible(showIeum);
      entry->bindingInfoLabel->setColour(
          juce::Label::backgroundColourId,
          showIeum ? juce::Colour(0x16182232) : juce::Colours::transparentBlack);
      entry->bindingInfoLabel->setText(showIeum ? ieumText : juce::String(),
                                       juce::dontSendNotification);

      const auto gyeolBinding = makeGyeolBindingPresentation(
          entry->spec, entry->paramId, bindingSummaryResolver);
      const bool showGyeol = shouldShowGyeolBindingLine(gyeolBinding.text);
      entry->gyeolBindingLabel->setVisible(showGyeol);
      entry->gyeolBindingLabel->setColour(
          juce::Label::backgroundColourId,
          showGyeol ? gyeolBinding.colour.withAlpha(0.12f)
                    : juce::Colours::transparentBlack);
      entry->gyeolBindingLabel->setColour(juce::Label::textColourId,
                                          showGyeol ? gyeolBinding.colour
                                                    : juce::Colours::white.withAlpha(0.32f));
      entry->gyeolBindingLabel->setText(showGyeol ? gyeolBinding.text
                                                  : juce::String(),
                                        juce::dontSendNotification);

      const auto controlAssignmentText = controlAssignmentSummaryForParam(*entry);
      const bool showControlAssignment = controlAssignmentText.isNotEmpty();
      entry->controlAssignmentLabel->setVisible(showControlAssignment);
      entry->controlAssignmentLabel->setColour(
          juce::Label::backgroundColourId,
          showControlAssignment ? juce::Colour(0x16f59e0b)
                                : juce::Colours::transparentBlack);
      entry->controlAssignmentLabel->setText(
          showControlAssignment ? controlAssignmentText : juce::String(),
          juce::dontSendNotification);
    }

    layoutParamEditors();
    repaint();
  }
  void applyChanges() {
    if (isInspectingFrame()) {
      TFrameRegion *frame = document.findFrame(inspectedFrameId);
      if (frame == nullptr)
        return;

      bool documentDirty = false;
      bool layoutDirty = false;

      const juce::String nextTitle = nameEditor.getText().trim();
      if (frame->title != nextTitle) {
        frame->title = nextTitle;
        documentDirty = true;
      }

      const auto nextColor = frameColorFromId(colorBox.getSelectedId());
      if (frame->colorArgb != nextColor) {
        frame->colorArgb = nextColor;
        documentDirty = true;
      }

      const bool nextLocked = bypassToggle.getToggleState();
      if (frame->locked != nextLocked) {
        frame->locked = nextLocked;
        documentDirty = true;
      }

      const bool nextCollapsed = collapsedToggle.getToggleState();
      if (frame->collapsed != nextCollapsed) {
        frame->collapsed = nextCollapsed;
        documentDirty = true;
        layoutDirty = true;
      }

      if (documentDirty)
        document.touch(false);

      if (layoutDirty)
        canvas.updateChildPositions();

      canvas.repaint();
      rebuildFromDocument();
      return;
    }

    TNode *node = document.findNode(inspectedNodeId);
    if (node == nullptr)
      return;

    bool documentDirty = false;
    bool runtimeDirty = false;

    const juce::String nextLabel = nameEditor.getText().trim();
    if (node->label != nextLabel) {
      node->label = nextLabel;
      documentDirty = true;
    }

    const juce::String nextColorTag = colorTagFromId(colorBox.getSelectedId());
    if (node->colorTag != nextColorTag) {
      node->colorTag = nextColorTag;
      documentDirty = true;
    }

    const bool nextBypassed = bypassToggle.getToggleState();
    if (node->bypassed != nextBypassed) {
      node->bypassed = nextBypassed;
      documentDirty = true;
      runtimeDirty = true;
    }

    const bool nextCollapsed = collapsedToggle.getToggleState();
    if (node->collapsed != nextCollapsed) {
      node->collapsed = nextCollapsed;
      documentDirty = true;
    }

    for (const auto &entry : paramEditors) {
      if (entry->spec.isReadOnly)
        continue;

      const juce::var nextValue = readEditorValue(*entry);
      auto currentIt = node->params.find(entry->spec.key);
      const juce::var currentValue = currentIt != node->params.end()
                                         ? currentIt->second
                                         : entry->originalValue;
      if (varEquals(currentValue, nextValue))
        continue;

      document.executeCommand(std::make_unique<SetParamCommand>(
          inspectedNodeId, entry->spec.key, currentValue, nextValue));
      if (paramProvider != nullptr && entry->spec.exposeToIeum)
        paramProvider->setParam(entry->paramId, nextValue);
    }

    if (documentDirty)
      document.touch(runtimeDirty);

    canvas.rebuildNodeComponents();
    canvas.repaint();
    rebuildFromDocument();
  }

  TGraphDocument &document;
  TGraphCanvas &canvas;
  const TNodeRegistry &registry;
  ParamBindingSummaryResolver bindingSummaryResolver;
  ITeulParamProvider *paramProvider = nullptr;
  std::function<void()> onLayoutChanged;

  NodeId inspectedNodeId = kInvalidNodeId;
  int inspectedFrameId = 0;
  juce::Label headerLabel;
  juce::Label typeLabel;
  juce::TextEditor nameEditor;
  juce::ComboBox colorBox;
  juce::ToggleButton bypassToggle;
  juce::ToggleButton collapsedToggle;
  juce::TextButton applyButton;
  juce::TextButton closeButton;
  juce::Viewport paramViewport;
  juce::TextEditor nodeConnectionsBox;
  juce::TextEditor frameSummaryBox;
  juce::TextButton frameCaptureButton;
  juce::TextButton frameReleaseButton;
  juce::TextButton frameFitButton;
  juce::TextButton frameSavePresetButton;
  std::unique_ptr<juce::Component> paramsContent;
  std::vector<std::unique_ptr<ParamEditor>> paramEditors;
  juce::String assignmentDropTargetId;
  bool assignmentDropTargetCanConnect = false;
  std::map<juce::String, TTeulExposedParam> runtimeParamsById;
};

} // namespace

std::unique_ptr<NodePropertiesPanel> NodePropertiesPanel::create(
    TGraphDocument &document, TGraphCanvas &canvas,
    const TNodeRegistry &registry, ITeulParamProvider *provider,
    ParamBindingSummaryResolver bindingSummaryResolver) {
  return std::make_unique<NodePropertiesPanelImpl>(
      document, canvas, registry, provider,
      std::move(bindingSummaryResolver));
}

} // namespace Teul
