#include "Teul.h"

#include "History/TCommands.h"
#include "Model/TGraphDocument.h"
#include "Registry/TNodeRegistry.h"
#include "UI/Canvas/TGraphCanvas.h"

#include <algorithm>
#include <limits>
#include <set>

// Teul sources are included here because the generated project currently compiles
// only Teul.cpp and TSerializer.cpp for the Teul module.
#include "Model/TGraphDocument.cpp"
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

class NodePropertiesPanel : public juce::Component {
public:
  NodePropertiesPanel(TGraphDocument &docIn, TGraphCanvas &canvasIn,
                      const TNodeRegistry &registryIn)
      : document(docIn), canvas(canvasIn), registry(registryIn) {
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

    setVisible(false);
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
    juce::String key;
    juce::var originalValue;
    bool isBool = false;
    std::unique_ptr<juce::Label> caption;
    std::unique_ptr<juce::Component> editor;
  };

  void rebuildFromDocument() {
    const TNode *node = document.findNode(inspectedNodeId);
    if (node == nullptr) {
      hidePanel();
      return;
    }

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
    for (auto &editor : paramEditors) {
      if (editor->caption)
        paramsContent->removeChildComponent(editor->caption.get());
      if (editor->editor)
        paramsContent->removeChildComponent(editor->editor.get());
    }

    paramEditors.clear();
    if (paramsContent != nullptr)
      paramsContent->setSize(0, 0);
  }

  void rebuildParamEditors(const TNode &node, const TNodeDescriptor *desc) {
    clearParamEditors();
    std::set<juce::String> seenKeys;

    auto appendParam = [&](const juce::String &key, const juce::String &label,
                           const juce::var &value) {
      auto entry = std::make_unique<ParamEditor>();
      entry->key = key;
      entry->originalValue = value;
      entry->isBool = value.isBool();
      entry->caption = std::make_unique<juce::Label>();
      entry->caption->setText(label.isNotEmpty() ? label : key,
                              juce::dontSendNotification);
      entry->caption->setJustificationType(juce::Justification::centredLeft);
      entry->caption->setColour(juce::Label::textColourId,
                                juce::Colours::white.withAlpha(0.72f));

      if (entry->isBool) {
        auto toggle = std::make_unique<juce::ToggleButton>();
        const juce::String lowered = value.toString().trim().toLowerCase();
        const bool isOn = lowered == "1" || lowered == "true" ||
                          lowered == "yes" || lowered == "on";
        toggle->setToggleState(isOn, juce::dontSendNotification);
        entry->editor = std::move(toggle);
      } else {
        auto textEditor = std::make_unique<juce::TextEditor>();
        textEditor->setText(value.toString(), juce::dontSendNotification);
        textEditor->setColour(juce::TextEditor::backgroundColourId,
                              juce::Colour(0xff171717));
        textEditor->setColour(juce::TextEditor::textColourId,
                              juce::Colours::white);
        textEditor->setColour(juce::TextEditor::outlineColourId,
                              juce::Colour(0xff343434));
        entry->editor = std::move(textEditor);
      }

      paramsContent->addAndMakeVisible(entry->caption.get());
      paramsContent->addAndMakeVisible(entry->editor.get());
      paramEditors.push_back(std::move(entry));
      seenKeys.insert(key);
    };

    if (desc != nullptr) {
      for (const auto &spec : desc->paramSpecs) {
        auto it = node.params.find(spec.key);
        appendParam(spec.key, spec.label,
                    it != node.params.end() ? it->second : spec.defaultValue);
      }
    }

    for (const auto &pair : node.params) {
      if (seenKeys.find(pair.first) != seenKeys.end())
        continue;
      appendParam(pair.first, pair.first, pair.second);
    }
  }

  void layoutParamEditors() {
    if (paramsContent == nullptr)
      return;

    const int width = juce::jmax(120, paramViewport.getMaximumVisibleWidth() - 10);
    int y = 0;
    for (auto &entry : paramEditors) {
      entry->caption->setBounds(0, y, width / 2 - 6, 24);
      entry->editor->setBounds(width / 2, y, width - width / 2, 24);
      y += 30;
    }

    paramsContent->setSize(width, juce::jmax(y, paramViewport.getHeight()));
  }

  void applyChanges() {
    TNode *node = document.findNode(inspectedNodeId);
    if (node == nullptr)
      return;

    node->label = nameEditor.getText().trim();
    node->colorTag = colorTagFromId(colorBox.getSelectedId());
    node->bypassed = bypassToggle.getToggleState();
    node->collapsed = collapsedToggle.getToggleState();

    for (const auto &entry : paramEditors) {
      juce::var nextValue;
      if (entry->isBool) {
        if (auto *toggle = dynamic_cast<juce::ToggleButton *>(entry->editor.get()))
          nextValue = toggle->getToggleState();
      } else if (auto *editor =
                     dynamic_cast<juce::TextEditor *>(entry->editor.get())) {
        nextValue = parseEditedValue(editor->getText(), entry->originalValue);
      }

      auto currentIt = node->params.find(entry->key);
      const juce::var currentValue =
          currentIt != node->params.end() ? currentIt->second : entry->originalValue;
      if (varEquals(currentValue, nextValue))
        continue;

      document.executeCommand(std::make_unique<SetParamCommand>(
          inspectedNodeId, entry->key, currentValue, nextValue));
    }

    canvas.rebuildNodeComponents();
    canvas.repaint();
    rebuildFromDocument();
  }

  TGraphDocument &document;
  TGraphCanvas &canvas;
  const TNodeRegistry &registry;
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
};

} // namespace

struct EditorHandle::Impl {
  explicit Impl(EditorHandle &ownerIn)
      : owner(ownerIn), registry(makeDefaultNodeRegistry()) {
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

    propertiesPanel = std::make_unique<NodePropertiesPanel>(doc, *canvas, *registry);
    propertiesPanel->setLayoutChangedCallback([this] { owner.resized(); });
    owner.addAndMakeVisible(*propertiesPanel);

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
  }

  void openProperties(NodeId nodeId) {
    if (!propertiesPanel)
      return;

    propertiesPanel->inspectNode(nodeId);
  }

  EditorHandle &owner;
  TGraphDocument doc;
  std::unique_ptr<TNodeRegistry> registry;
  std::unique_ptr<TGraphCanvas> canvas;
  std::unique_ptr<NodeLibraryPanel> libraryPanel;
  std::unique_ptr<NodePropertiesPanel> propertiesPanel;

  juce::TextButton toggleLibraryButton;
  juce::TextButton quickAddButton;
  juce::TextButton findNodeButton;
  juce::TextButton commandPaletteButton;

  bool libraryVisible = true;
};

EditorHandle::EditorHandle() : impl(std::make_unique<Impl>(*this)) {}
EditorHandle::~EditorHandle() = default;

TGraphDocument &EditorHandle::document() noexcept { return impl->doc; }
const TGraphDocument &EditorHandle::document() const noexcept { return impl->doc; }

void EditorHandle::refreshFromDocument() {
  if (impl->canvas)
    impl->canvas->rebuildNodeComponents();
  if (impl->propertiesPanel)
    impl->propertiesPanel->refreshFromDocument();
  resized();
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

std::unique_ptr<EditorHandle> createEditor() {
  return std::make_unique<EditorHandle>();
}

} // namespace Teul