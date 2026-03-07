#include "Teul.h"

#include "History/TCommands.h"
#include "Model/TGraphDocument.h"
#include "Registry/TNodeRegistry.h"
#include "UI/Canvas/TGraphCanvas.h"
#include "Export/TExport.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

// Teul sources are included here because the generated project currently compiles
// only Teul.cpp and TSerializer.cpp for the Teul module.
#include "Model/TGraphDocument.cpp"
#include "Export/TExport.cpp"
#include "Registry/TNodeRegistry.cpp"
#include "Runtime/TGraphRuntime.cpp"
#include "UI/Port/TPortComponent.cpp"
#include "UI/Node/TNodeComponent.cpp"
#include "UI/Canvas/TGraphCanvas.cpp"

namespace Teul {
namespace {

static const char *kLibraryDragPrefix = "teul.node:";

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

static int matchScore(const TNodeDescriptor &desc, const juce::String &query) {
  const juce::String q = query.trim().toLowerCase();
  if (q.isEmpty())
    return 1;

  const juce::String display = desc.displayName.toLowerCase();
  const juce::String typeKey = desc.typeKey.toLowerCase();
  const juce::String category = desc.category.toLowerCase();

  if (display == q)
    return 420;
  if (display.startsWith(q))
    return 320;

  const int idxDisplay = display.indexOf(q);
  if (idxDisplay >= 0)
    return 220 - idxDisplay * 2;

  const int idxType = typeKey.indexOf(q);
  if (idxType >= 0)
    return 190 - idxType * 2;

  const int idxCategory = category.indexOf(q);
  if (idxCategory >= 0)
    return 120 - idxCategory * 2;

  int scan = 0;
  int fuzzy = 0;
  for (int qi = 0; qi < q.length(); ++qi) {
    const auto qc = q[qi];
    bool found = false;
    for (int di = scan; di < display.length(); ++di) {
      if (display[di] != qc)
        continue;
      fuzzy += 16 - juce::jmin(12, di - scan);
      scan = di + 1;
      found = true;
      break;
    }
    if (!found)
      return std::numeric_limits<int>::min();
  }

  return 110 + fuzzy;
}

static bool varEquals(const juce::var &lhs, const juce::var &rhs) {
  return lhs.toString() == rhs.toString() && lhs.isBool() == rhs.isBool() &&
         lhs.isInt() == rhs.isInt() && lhs.isInt64() == rhs.isInt64() &&
         lhs.isDouble() == rhs.isDouble();
}

static juce::var parseEditedValue(const juce::String &text,
                                  const juce::var &prototype) {
  if (prototype.isBool()) {
    const juce::String lowered = text.trim().toLowerCase();
    return lowered == "1" || lowered == "true" || lowered == "yes" ||
           lowered == "on";
  }

  if (prototype.isInt())
    return text.getIntValue();

  if (prototype.isInt64())
    return static_cast<juce::int64>(text.getLargeIntValue());

  if (prototype.isDouble())
    return text.getDoubleValue();

  return text;
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

class NodeLibraryPanel : public juce::Component,
                         public juce::DragAndDropContainer,
                         private juce::ListBoxModel,
                         private juce::TextEditor::Listener,
                         private juce::KeyListener {
public:
  NodeLibraryPanel(const TNodeRegistry &registry,
                   std::function<void(const juce::String &)> onInsertFn)
      : nodeRegistry(registry), onInsert(std::move(onInsertFn)) {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(categoryBox);
    addAndMakeVisible(listBox);

    titleLabel.setText("Node Library", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.92f));
    titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    searchEditor.setTextToShowWhenEmpty("Search nodes...", juce::Colours::grey);
    searchEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(0xff1c1c1c));
    searchEditor.setColour(juce::TextEditor::textColourId,
                           juce::Colours::white);
    searchEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colour(0xff3a3a3a));
    searchEditor.setEscapeAndReturnKeysConsumed(false);
    searchEditor.addListener(this);
    searchEditor.addKeyListener(this);

    categoryBox.setTextWhenNothingSelected("All Categories");
    categoryBox.onChange = [this] { refreshFilter(); };

    listBox.setModel(this);
    listBox.setRowHeight(30);
    listBox.setColour(juce::ListBox::backgroundColourId,
                      juce::Colour(0xff151515));
    listBox.addKeyListener(this);
    listBox.setMultipleSelectionEnabled(false);

    for (const auto &desc : nodeRegistry.getAllDescriptors())
      allDescriptors.push_back(&desc);

    rebuildCategories();
    refreshFilter();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff111111));
    g.setColour(juce::Colour(0xff2f2f2f));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(10);
    titleLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);
    searchEditor.setBounds(area.removeFromTop(26));
    area.removeFromTop(6);
    categoryBox.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    listBox.setBounds(area);
  }

private:
  struct DisplayItem {
    const TNodeDescriptor *desc = nullptr;
    int score = std::numeric_limits<int>::min();
  };

  int getNumRows() override { return (int)filtered.size(); }

  void paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                        bool selected) override {
    if (row < 0 || row >= (int)filtered.size())
      return;

    const auto *desc = filtered[(size_t)row].desc;
    if (desc == nullptr)
      return;

    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1);
    if (selected) {
      g.setColour(juce::Colour(0xff355d9f));
      g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
    }

    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(desc->displayName, bounds.removeFromTop(16).reduced(8, 0),
               juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(10.5f);
    g.drawText(desc->category + " / " + desc->typeKey,
               bounds.reduced(8, 0), juce::Justification::centredLeft, false);
  }

  juce::var getDragSourceDescription(
      const juce::SparseSet<int> &currentlySelectedRows) override {
    if (currentlySelectedRows.size() <= 0)
      return {};

    const int row = currentlySelectedRows[0];
    if (row < 0 || row >= (int)filtered.size())
      return {};

    const auto *desc = filtered[(size_t)row].desc;
    if (desc == nullptr)
      return {};

    return juce::String(kLibraryDragPrefix) + desc->typeKey;
  }

  void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override {
    insertRow(row);
  }

  void returnKeyPressed(int row) override { insertRow(row); }

  void textEditorTextChanged(juce::TextEditor &) override { refreshFilter(); }

  bool keyPressed(const juce::KeyPress &key, juce::Component *) override {
    if (key == juce::KeyPress::returnKey || key == juce::KeyPress::tabKey) {
      insertRow(listBox.getSelectedRow());
      return true;
    }
    return false;
  }

  void rebuildCategories() {
    categoryBox.clear(juce::dontSendNotification);
    categoryBox.addItem("All", 1);

    juce::StringArray categories;
    for (const auto *desc : allDescriptors) {
      if (desc == nullptr)
        continue;
      if (!categories.contains(desc->category))
        categories.add(desc->category);
    }

    categories.sort(true);
    int id = 2;
    for (const auto &category : categories)
      categoryBox.addItem(category, id++);

    categoryBox.setSelectedId(1, juce::dontSendNotification);
  }

  void refreshFilter() {
    filtered.clear();

    const juce::String query = searchEditor.getText().trim();
    const juce::String selectedCategory = categoryBox.getText();
    const bool filterCategory = categoryBox.getSelectedId() > 1 &&
                                selectedCategory.isNotEmpty();

    for (const auto *desc : allDescriptors) {
      if (desc == nullptr)
        continue;
      if (filterCategory && desc->category != selectedCategory)
        continue;

      const int score = matchScore(*desc, query);
      if (score == std::numeric_limits<int>::min())
        continue;

      filtered.push_back({desc, score});
    }

    std::stable_sort(filtered.begin(), filtered.end(),
                     [query](const DisplayItem &a, const DisplayItem &b) {
                       if (query.isNotEmpty() && a.score != b.score)
                         return a.score > b.score;
                       if (a.desc->category != b.desc->category)
                         return a.desc->category < b.desc->category;
                       return a.desc->displayName < b.desc->displayName;
                     });

    listBox.updateContent();
    if (!filtered.empty())
      listBox.selectRow(0);
  }

  void insertRow(int row) {
    if (row < 0 || row >= (int)filtered.size())
      return;

    const auto *desc = filtered[(size_t)row].desc;
    if (desc == nullptr || onInsert == nullptr)
      return;

    onInsert(desc->typeKey);
  }

  const TNodeRegistry &nodeRegistry;
  std::function<void(const juce::String &)> onInsert;

  juce::Label titleLabel;
  juce::TextEditor searchEditor;
  juce::ComboBox categoryBox;
  juce::ListBox listBox;

  std::vector<const TNodeDescriptor *> allDescriptors;
  std::vector<DisplayItem> filtered;
};

class NodePropertiesPanel : public juce::Component,
                            private juce::Timer,
                            private ITeulParamProvider::Listener {
public:
  NodePropertiesPanel(TGraphDocument &docIn, TGraphCanvas &canvasIn,
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

  ~NodePropertiesPanel() override {
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

  static TParamValueType inferValueType(const juce::var &value) {
    if (value.isBool())
      return TParamValueType::Bool;
    if (value.isInt() || value.isInt64())
      return TParamValueType::Int;
    if (value.isString())
      return TParamValueType::String;
    return TParamValueType::Float;
  }

  static bool isNumericValue(const juce::var &value) {
    return value.isBool() || value.isInt() || value.isInt64() || value.isDouble();
  }

  static double numericValue(const juce::var &value) {
    if (value.isBool())
      return (bool)value ? 1.0 : 0.0;
    if (value.isInt())
      return (double)(int)value;
    if (value.isInt64())
      return (double)(juce::int64)value;
    if (value.isDouble())
      return (double)value;
    if (value.isString())
      return value.toString().getDoubleValue();
    return 0.0;
  }

  static bool hasNumericRange(const TParamSpec &spec) {
    return isNumericValue(spec.minValue) && isNumericValue(spec.maxValue);
  }

  static double sliderIntervalFor(const TParamSpec &spec) {
    if (isNumericValue(spec.step) && numericValue(spec.step) > 0.0)
      return numericValue(spec.step);

    if (spec.valueType == TParamValueType::Int || spec.valueType == TParamValueType::Bool ||
        spec.valueType == TParamValueType::Enum || spec.isDiscrete) {
      return 1.0;
    }

    return 0.0;
  }

  static TParamSpec makeFallbackParamSpec(const juce::String &key,
                                          const juce::var &value) {
    TParamSpec spec;
    spec.key = key;
    spec.label = key;
    spec.defaultValue = value;
    spec.valueType = inferValueType(value);
    spec.preferredWidget = spec.valueType == TParamValueType::Bool
                               ? TParamWidgetHint::Toggle
                               : TParamWidgetHint::Text;
    spec.showInPropertyPanel = true;
    spec.preferredBindingKey = key;
    spec.exportSymbol = key;
    return spec;
  }

  static TParamSpec makeSpecFromExposedParam(const TTeulExposedParam &param) {
    TParamSpec spec;
    spec.key = param.paramKey;
    spec.label = param.paramLabel;
    spec.defaultValue = param.defaultValue;
    spec.valueType = param.valueType;
    spec.minValue = param.minValue;
    spec.maxValue = param.maxValue;
    spec.step = param.step;
    spec.unitLabel = param.unitLabel;
    spec.displayPrecision = param.displayPrecision;
    spec.group = param.group;
    spec.description = param.description;
    spec.enumOptions = param.enumOptions;
    spec.preferredWidget = param.preferredWidget;
    spec.showInNodeBody = param.showInNodeBody;
    spec.showInPropertyPanel = param.showInPropertyPanel;
    spec.isReadOnly = param.isReadOnly;
    spec.isAutomatable = param.isAutomatable;
    spec.isModulatable = param.isModulatable;
    spec.isDiscrete = param.isDiscrete;
    spec.exposeToIeum = param.exposeToIeum;
    spec.preferredBindingKey = param.preferredBindingKey;
    spec.exportSymbol = param.exportSymbol;
    spec.categoryPath = param.categoryPath;
    return spec;
  }

  static juce::String formatValueForDisplay(const juce::var &value,
                                            const TParamSpec &spec) {
    if (value.isVoid())
      return "-";

    switch (spec.valueType) {
    case TParamValueType::Bool:
      return (bool)value ? "On" : "Off";
    case TParamValueType::Enum:
      for (const auto &option : spec.enumOptions) {
        if (varEquals(option.value, value))
          return option.label.isNotEmpty() ? option.label : option.id;
      }
      return value.toString();
    case TParamValueType::Int: {
      juce::String text((int)juce::roundToInt(numericValue(value)));
      if (spec.unitLabel.isNotEmpty())
        text << " " << spec.unitLabel;
      return text;
    }
    case TParamValueType::Float: {
      juce::String text(numericValue(value), juce::jlimit(0, 6, spec.displayPrecision));
      if (spec.unitLabel.isNotEmpty())
        text << " " << spec.unitLabel;
      return text;
    }
    case TParamValueType::String:
    case TParamValueType::Auto:
      break;
    }

    return value.toString();
  }

  static juce::var parseTextValue(const juce::String &text, const TParamSpec &spec,
                                  const juce::var &prototype) {
    if (spec.valueType == TParamValueType::Bool)
      return parseEditedValue(text, prototype.isVoid() ? juce::var(false) : prototype);

    if (spec.valueType == TParamValueType::Int)
      return juce::roundToInt(text.getDoubleValue());

    if (spec.valueType == TParamValueType::Float)
      return text.getDoubleValue();

    if (spec.valueType == TParamValueType::Enum) {
      for (const auto &option : spec.enumOptions) {
        if (option.id.equalsIgnoreCase(text) ||
            option.label.equalsIgnoreCase(text) ||
            option.value.toString().equalsIgnoreCase(text)) {
          return option.value;
        }
      }
    }

    return parseEditedValue(text, prototype);
  }

  static int editorHeightFor(const ParamEditor &entry) {
    if (dynamic_cast<juce::Slider *>(entry.editor.get()) != nullptr)
      return 32;
    return 24;
  }

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

      if (entry->spec.preferredWidget == TParamWidgetHint::Combo &&
          !entry->spec.enumOptions.empty()) {
        auto combo = std::make_unique<juce::ComboBox>();
        int id = 1;
        int selectedId = 0;
        for (const auto &option : entry->spec.enumOptions) {
          combo->addItem(option.label.isNotEmpty() ? option.label : option.id, id);
          if (selectedId == 0 && varEquals(option.value, value))
            selectedId = id;
          ++id;
        }
        combo->setSelectedId(selectedId > 0 ? selectedId : 1,
                             juce::dontSendNotification);
        combo->setEnabled(!entry->spec.isReadOnly);
        entry->editor = std::move(combo);
      } else if (entry->spec.preferredWidget == TParamWidgetHint::Toggle ||
                 entry->spec.valueType == TParamValueType::Bool) {
        auto toggle = std::make_unique<juce::ToggleButton>();
        toggle->setToggleState((bool)value, juce::dontSendNotification);
        toggle->setEnabled(!entry->spec.isReadOnly);
        entry->editor = std::move(toggle);
      } else if (entry->spec.preferredWidget == TParamWidgetHint::Slider &&
                 hasNumericRange(entry->spec)) {
        auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                     juce::Slider::TextBoxRight);
        slider->setRange(numericValue(entry->spec.minValue),
                         numericValue(entry->spec.maxValue),
                         sliderIntervalFor(entry->spec));
        slider->setValue(numericValue(value), juce::dontSendNotification);
        slider->setNumDecimalPlacesToDisplay(
            entry->spec.valueType == TParamValueType::Int || entry->spec.isDiscrete
                ? 0
                : juce::jlimit(0, 6, entry->spec.displayPrecision));
        slider->setTextValueSuffix(entry->spec.unitLabel.isNotEmpty()
                                       ? " " + entry->spec.unitLabel
                                       : juce::String());
        slider->setDoubleClickReturnValue(true, numericValue(entry->spec.defaultValue));
        slider->setEnabled(!entry->spec.isReadOnly);
        entry->editor = std::move(slider);
      } else {
        auto textEditor = std::make_unique<juce::TextEditor>();
        textEditor->setText(formatValueForDisplay(value, entry->spec),
                            juce::dontSendNotification);
        textEditor->setColour(juce::TextEditor::backgroundColourId,
                              juce::Colour(0xff171717));
        textEditor->setColour(juce::TextEditor::textColourId,
                              juce::Colours::white);
        textEditor->setColour(juce::TextEditor::outlineColourId,
                              juce::Colour(0xff343434));
        textEditor->setReadOnly(entry->spec.isReadOnly);
        entry->editor = std::move(textEditor);
      }

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

      entry->editor->setBounds(0, y, width, editorHeightFor(*entry));
      y += editorHeightFor(*entry) + 4;

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
    if (const auto *combo = dynamic_cast<juce::ComboBox *>(entry.editor.get())) {
      const int index = combo->getSelectedItemIndex();
      if (index >= 0 && index < (int)entry.spec.enumOptions.size())
        return entry.spec.enumOptions[(size_t)index].value;
      return entry.originalValue;
    }

    if (const auto *toggle = dynamic_cast<juce::ToggleButton *>(entry.editor.get()))
      return toggle->getToggleState();

    if (const auto *slider = dynamic_cast<juce::Slider *>(entry.editor.get())) {
      if (entry.spec.valueType == TParamValueType::Int || entry.spec.isDiscrete)
        return juce::roundToInt(slider->getValue());
      return slider->getValue();
    }

    if (const auto *editor = dynamic_cast<juce::TextEditor *>(entry.editor.get()))
      return parseTextValue(editor->getText(), entry.spec, entry.originalValue);

    return entry.originalValue;
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

      juce::String bindingText = entry->spec.exposeToIeum ? "Ieum: exposed"
                                                          : "Ieum: hidden";
      if (entry->spec.exposeToIeum && entry->spec.preferredBindingKey.isNotEmpty())
        bindingText << " / key: " << entry->spec.preferredBindingKey;
      if (entry->spec.isAutomatable)
        bindingText << " / auto";
      if (entry->spec.isModulatable)
        bindingText << " / mod";
      if (entry->spec.isReadOnly)
        bindingText << " / read-only";
      entry->bindingInfoLabel->setText(bindingText, juce::dontSendNotification);

      juce::String gyeolBindingText;
      if (!entry->spec.exposeToIeum) {
        gyeolBindingText = "Gyeol: hidden";
      } else if (bindingSummaryResolver == nullptr) {
        gyeolBindingText = "Gyeol: source unavailable";
      } else {
        const auto summary = bindingSummaryResolver(entry->paramId);
        if (summary.isNotEmpty())
          gyeolBindingText =
              juce::String(juce::CharPointer_UTF8("ð ")) + summary;
        else
          gyeolBindingText = "Gyeol: unbound";
      }

      const auto bindingColour = gyeolBindingText.startsWith("Gyeol: hidden")
                                     ? juce::Colours::white.withAlpha(0.28f)
                                     : (gyeolBindingText.startsWith("Gyeol: unbound") ||
                                                gyeolBindingText.startsWith("Gyeol: source")
                                            ? juce::Colours::white.withAlpha(0.36f)
                                            : juce::Colour(0xffd9c27c));
      entry->gyeolBindingLabel->setColour(juce::Label::textColourId,
                                          bindingColour);
      entry->gyeolBindingLabel->setText(gyeolBindingText,
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

struct EditorHandle::Impl : private juce::Timer {
  explicit Impl(EditorHandle &ownerIn,
                juce::AudioDeviceManager *audioDeviceManagerIn,
                ParamBindingSummaryResolver bindingSummaryResolverIn,
                ParamBindingRevisionProvider bindingRevisionProviderIn)
      : owner(ownerIn), registry(makeDefaultNodeRegistry()),
        runtime(registry.get()),
        audioDeviceManager(audioDeviceManagerIn),
        bindingRevisionProvider(std::move(bindingRevisionProviderIn)) {
    canvas = std::make_unique<TGraphCanvas>(doc);
    canvas->setNodePropertiesRequestHandler(
        [this](NodeId nodeId) { openProperties(nodeId); });
    owner.addAndMakeVisible(*canvas);

    libraryPanel = std::make_unique<NodeLibraryPanel>(
        *registry, [this](const juce::String &typeKey) {
          if (canvas)
            canvas->addNodeByTypeAtView(typeKey, canvas->getViewCenter(), true);
        });
    owner.addAndMakeVisible(*libraryPanel);

    propertiesPanel = std::make_unique<NodePropertiesPanel>(
        doc, *canvas, *registry, &runtime, std::move(bindingSummaryResolverIn));
    propertiesPanel->setLayoutChangedCallback([this] { owner.resized(); });
    owner.addAndMakeVisible(*propertiesPanel);

    canvas->setNodeSelectionChangedHandler(
        [this](const std::vector<NodeId> &selectedNodeIds) {
          handleSelectionChanged(selectedNodeIds);
        });

    owner.addAndMakeVisible(toggleLibraryButton);
    owner.addAndMakeVisible(quickAddButton);
    owner.addAndMakeVisible(findNodeButton);
    owner.addAndMakeVisible(commandPaletteButton);

    toggleLibraryButton.setButtonText("Library");
    quickAddButton.setButtonText("Quick Add");
    findNodeButton.setButtonText("Find Node");
    commandPaletteButton.setButtonText("Cmd");

    toggleLibraryButton.onClick = [this] {
      libraryVisible = !libraryVisible;
      if (libraryPanel)
        libraryPanel->setVisible(libraryVisible);
      owner.resized();
    };

    quickAddButton.onClick = [this] {
      if (canvas)
        canvas->openQuickAddAt(canvas->getViewCenter());
    };

    findNodeButton.onClick = [this] {
      if (canvas)
        canvas->openNodeSearchPrompt();
    };

    commandPaletteButton.onClick = [this] {
      if (canvas)
        canvas->openCommandPalette();
    };

    rebuildAll(true);

    if (audioDeviceManager != nullptr)
      audioDeviceManager->addAudioCallback(&runtime);

    startTimerHz(20);
  }

  ~Impl() override {
    stopTimer();
    if (audioDeviceManager != nullptr)
      audioDeviceManager->removeAudioCallback(&runtime);
  }

  void timerCallback() override {
    const auto currentRuntimeRevision = doc.getRuntimeRevision();
    if (currentRuntimeRevision != lastRuntimeRevision) {
      runtime.buildGraph(doc);
      lastRuntimeRevision = currentRuntimeRevision;
    }

    const auto currentDocumentRevision = doc.getDocumentRevision();
    if (currentDocumentRevision != lastDocumentRevision) {
      if (propertiesPanel)
        propertiesPanel->refreshFromDocument();
      lastDocumentRevision = currentDocumentRevision;
    }

    const auto currentBindingRevision =
        bindingRevisionProvider != nullptr ? bindingRevisionProvider() : 0;
    if (currentBindingRevision != lastBindingRevision) {
      if (propertiesPanel)
        propertiesPanel->refreshBindingSummaries();
      lastBindingRevision = currentBindingRevision;
    }
  }

  void rebuildAll(bool rebuildRuntime) {
    if (canvas)
      canvas->rebuildNodeComponents();
    if (rebuildRuntime)
      runtime.buildGraph(doc);
    if (propertiesPanel) {
      propertiesPanel->setParamProvider(&runtime);
      propertiesPanel->refreshFromDocument();
    }

    lastDocumentRevision = doc.getDocumentRevision();
    lastRuntimeRevision = doc.getRuntimeRevision();
    owner.resized();
  }

  void handleSelectionChanged(const std::vector<NodeId> &selectedNodeIds) {
    if (!propertiesPanel)
      return;

    if (selectedNodeIds.size() == 1)
      propertiesPanel->inspectNode(selectedNodeIds.front());
    else
      propertiesPanel->hidePanel();
  }

  void openProperties(NodeId nodeId) {
    if (!propertiesPanel)
      return;

    propertiesPanel->inspectNode(nodeId);
  }

  EditorHandle &owner;
  TGraphDocument doc;
  std::unique_ptr<TNodeRegistry> registry;
  TGraphRuntime runtime;
  std::unique_ptr<TGraphCanvas> canvas;
  std::unique_ptr<NodeLibraryPanel> libraryPanel;
  std::unique_ptr<NodePropertiesPanel> propertiesPanel;
  juce::AudioDeviceManager *audioDeviceManager = nullptr;

  juce::TextButton toggleLibraryButton;
  juce::TextButton quickAddButton;
  juce::TextButton findNodeButton;
  juce::TextButton commandPaletteButton;

  bool libraryVisible = true;
  std::uint64_t lastDocumentRevision = 0;
  std::uint64_t lastRuntimeRevision = 0;
  std::uint64_t lastBindingRevision = 0;
  ParamBindingRevisionProvider bindingRevisionProvider;
};

EditorHandle::EditorHandle(juce::AudioDeviceManager *audioDeviceManager,
                           ParamBindingSummaryResolver bindingSummaryResolver,
                           ParamBindingRevisionProvider bindingRevisionProvider)
    : impl(std::make_unique<Impl>(*this, audioDeviceManager,
                                  std::move(bindingSummaryResolver),
                                  std::move(bindingRevisionProvider))) {}
EditorHandle::~EditorHandle() = default;

TGraphDocument &EditorHandle::document() noexcept { return impl->doc; }
const TGraphDocument &EditorHandle::document() const noexcept { return impl->doc; }

void EditorHandle::refreshFromDocument() {
  impl->rebuildAll(true);
}

TExportResult EditorHandle::runExportDryRun(
    const TExportOptions &options) const {
  if (impl == nullptr || impl->registry == nullptr)
    return {};

  return TExporter::run(impl->doc, *impl->registry, options);
}

void EditorHandle::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff121212));
}

void EditorHandle::resized() {
  auto area = getLocalBounds();
  auto top = area.removeFromTop(36).reduced(6, 4);

  impl->toggleLibraryButton.setBounds(top.removeFromLeft(80));
  top.removeFromLeft(4);
  impl->quickAddButton.setBounds(top.removeFromLeft(90));
  top.removeFromLeft(4);
  impl->findNodeButton.setBounds(top.removeFromLeft(90));
  top.removeFromLeft(4);
  impl->commandPaletteButton.setBounds(top.removeFromLeft(60));

  if (impl->libraryPanel) {
    impl->libraryPanel->setVisible(impl->libraryVisible);
    if (impl->libraryVisible) {
      auto left = area.removeFromLeft(276);
      impl->libraryPanel->setBounds(left);
    }
  }

  if (impl->propertiesPanel && impl->propertiesPanel->isPanelOpen()) {
    auto right = area.removeFromRight(316);
    impl->propertiesPanel->setBounds(right);
  }

  if (impl->canvas)
    impl->canvas->setBounds(area);
}

std::unique_ptr<EditorHandle> createEditor(
    juce::AudioDeviceManager *audioDeviceManager,
    ParamBindingSummaryResolver bindingSummaryResolver,
    ParamBindingRevisionProvider bindingRevisionProvider) {
  return std::make_unique<EditorHandle>(audioDeviceManager,
                                        std::move(bindingSummaryResolver),
                                        std::move(bindingRevisionProvider));
}

} // namespace Teul
