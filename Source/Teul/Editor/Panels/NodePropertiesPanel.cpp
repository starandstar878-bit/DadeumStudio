#include "Teul/Editor/Panels/NodePropertiesPanel.h"

#include "Teul/Editor/Panels/Property/BindingSummaryPresenter.h"
#include "Teul/Editor/Panels/Property/ParamEditorFactory.h"
#include "Teul/Editor/Panels/Property/ParamValueFormatter.h"
#include "Teul/History/TCommands.h"
#include "Teul/Model/TGraphDocument.h"
#include "Teul/Editor/Canvas/TGraphCanvas.h"

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
    return isVisible() && inspectedNodeId != kInvalidNodeId;
  }

  void inspectNode(NodeId nodeId) {
    inspectedNodeId = nodeId;
    rebuildFromDocument();
    setVisible(true);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void refreshFromDocument() {
    if (!isPanelOpen())
      return;

    if (document.findNode(inspectedNodeId) == nullptr) {
      hidePanel();
      return;
    }

    rebuildFromDocument();
  }

  void refreshBindingSummaries() {
    if (isPanelOpen())
      updateRuntimeValueLabels();
  }

  void hidePanel() {
    if (!isVisible() && inspectedNodeId == kInvalidNodeId)
      return;

    inspectedNodeId = kInvalidNodeId;
    clearParamEditors();
    setVisible(false);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff101010));
    g.setColour(juce::Colour(0xff2f2f2f));
    g.drawRect(getLocalBounds(), 1);

    if (!isPanelOpen())
      return;

    auto area = getLocalBounds().reduced(12, 0);
    area.removeFromTop(86);
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(11.0f);
    g.drawText("Label", area.removeFromTop(16), juce::Justification::centredLeft,
               false);
    area.removeFromTop(32);
    g.drawText("Color Tag", area.removeFromTop(16),
               juce::Justification::centredLeft, false);
    area.removeFromTop(30);
    g.drawText("State", area.removeFromTop(16), juce::Justification::centredLeft,
               false);
    area.removeFromTop(34);
    g.drawText("Parameters", area.removeFromTop(16),
               juce::Justification::centredLeft, false);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    auto header = area.removeFromTop(24);
    closeButton.setBounds(header.removeFromRight(54));
    headerLabel.setBounds(header);
    area.removeFromTop(4);
    typeLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(22);
    nameEditor.setBounds(area.removeFromTop(26));
    area.removeFromTop(22);
    colorBox.setBounds(area.removeFromTop(24));
    area.removeFromTop(22);
    auto toggles = area.removeFromTop(24);
    bypassToggle.setBounds(toggles.removeFromLeft(toggles.getWidth() / 2));
    collapsedToggle.setBounds(toggles);
    area.removeFromTop(12);
    applyButton.setBounds(area.removeFromTop(26).removeFromRight(72));
    area.removeFromTop(12);
    paramViewport.setBounds(area);
    layoutParamEditors();
  }

private:
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
    bypassToggle.setToggleState(node->bypassed, juce::dontSendNotification);
    collapsedToggle.setToggleState(node->collapsed, juce::dontSendNotification);

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
        entry->groupLabel->setFont(juce::FontOptions(12.0f, juce::Font::bold));
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
      entry->runtimeValueLabel->setFont(juce::FontOptions(10.5f));
      paramsContent->addAndMakeVisible(entry->runtimeValueLabel.get());

      entry->bindingInfoLabel = std::make_unique<juce::Label>();
      entry->bindingInfoLabel->setJustificationType(juce::Justification::centredLeft);
      entry->bindingInfoLabel->setColour(juce::Label::textColourId,
                                         juce::Colours::white.withAlpha(0.44f));
      entry->bindingInfoLabel->setFont(juce::FontOptions(10.0f));
      paramsContent->addAndMakeVisible(entry->bindingInfoLabel.get());

      entry->gyeolBindingLabel = std::make_unique<juce::Label>();
      entry->gyeolBindingLabel->setJustificationType(juce::Justification::topLeft);
      entry->gyeolBindingLabel->setColour(juce::Label::textColourId,
                                          juce::Colours::white.withAlpha(0.32f));
      entry->gyeolBindingLabel->setFont(juce::FontOptions(10.0f));
      paramsContent->addAndMakeVisible(entry->gyeolBindingLabel.get());

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

      entry->caption->setBounds(0, y, width, 16);
      y += 18;

      const int editorHeight = editorHeightFor(*entry->editor);
      entry->editor->setBounds(0, y, width, editorHeight);
      y += editorHeight + 4;

      if (entry->descriptionLabel->isVisible()) {
        entry->descriptionLabel->setBounds(0, y, width, 14);
        y += 16;
      }

      entry->runtimeValueLabel->setBounds(0, y, width, 14);
      y += 16;
      entry->bindingInfoLabel->setBounds(0, y, width, 14);
      y += 16;
      entry->gyeolBindingLabel->setBounds(0, y, width, 28);
      y += 32;
    }

    paramsContent->setSize(width, juce::jmax(y, paramViewport.getHeight()));
  }

  juce::var readEditorValue(const ParamEditor &entry) const {
    return Teul::readEditorValue(*entry.editor, entry.spec, entry.originalValue);
  }

  void updateRuntimeValueLabels() {
    for (auto &entry : paramEditors) {
      juce::String runtimeText;
      const auto it = runtimeParamsById.find(entry->paramId);
      if (it != runtimeParamsById.end()) {
        runtimeText = "Runtime: " + formatValueForDisplay(it->second.currentValue,
                                                           entry->spec);
      } else if (entry->spec.exposeToIeum && paramProvider != nullptr) {
        runtimeText = "Runtime: unavailable";
      } else {
        runtimeText = "Document: " + formatValueForDisplay(entry->originalValue,
                                                            entry->spec);
      }
      entry->runtimeValueLabel->setText(runtimeText, juce::dontSendNotification);

      entry->bindingInfoLabel->setText(makeIeumBindingText(entry->spec),
                                       juce::dontSendNotification);

      const auto gyeolBinding = makeGyeolBindingPresentation(
          entry->spec, entry->paramId, bindingSummaryResolver);
      entry->gyeolBindingLabel->setColour(juce::Label::textColourId,
                                          gyeolBinding.colour);
      entry->gyeolBindingLabel->setText(gyeolBinding.text,
                                        juce::dontSendNotification);
    }

    repaint();
  }

  void applyChanges() {
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
  juce::Label headerLabel;
  juce::Label typeLabel;
  juce::TextEditor nameEditor;
  juce::ComboBox colorBox;
  juce::ToggleButton bypassToggle;
  juce::ToggleButton collapsedToggle;
  juce::TextButton applyButton;
  juce::TextButton closeButton;
  juce::Viewport paramViewport;
  std::unique_ptr<juce::Component> paramsContent;
  std::vector<std::unique_ptr<ParamEditor>> paramEditors;
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
