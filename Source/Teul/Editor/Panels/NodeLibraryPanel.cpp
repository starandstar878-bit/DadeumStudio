#include "Teul/Editor/Panels/NodeLibraryPanel.h"

#include "Teul/Registry/TNodeRegistry.h"

#include <algorithm>
#include <limits>

namespace Teul {
namespace {

static const char *kLibraryDragPrefix = "teul.node:";

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

class NodeLibraryPanelImpl final : public NodeLibraryPanel,
                         public juce::DragAndDropContainer,
                         private juce::ListBoxModel,
                         private juce::TextEditor::Listener,
                         private juce::KeyListener {
public:
  NodeLibraryPanelImpl(const TNodeRegistry &registry,
                   std::function<void(const juce::String &)> onInsertFn)
      : nodeRegistry(registry), onInsert(std::move(onInsertFn)) {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(categoryBox);
    addAndMakeVisible(listBox);

    titleLabel.setText("Node Library", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.90f));
    titleLabel.setFont(juce::FontOptions(14.2f, juce::Font::bold));

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
    listBox.setRowHeight(26);
    listBox.setColour(juce::ListBox::backgroundColourId,
                      juce::Colour(0xff14181d));
    listBox.addKeyListener(this);
    listBox.setMultipleSelectionEnabled(false);

    for (const auto &desc : nodeRegistry.getAllDescriptors())
      allDescriptors.push_back(&desc);

    rebuildCategories();
    refreshFilter();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff101317));
    g.setColour(juce::Colour(0xff2a323c));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(8);
    titleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);
    searchEditor.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    categoryBox.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);
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
      g.setColour(juce::Colour(0xff355d9f).withAlpha(0.92f));
      g.fillRoundedRectangle(bounds.toFloat(), 4.5f);
    }

    auto titleRow = bounds.removeFromTop(14).reduced(7, 0);
    auto subtitleRow = bounds.reduced(7, 0);
    g.setColour(juce::Colours::white.withAlpha(0.96f));
    g.setFont(11.6f);
    g.drawText(desc->displayName, titleRow, juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.46f));
    g.setFont(9.2f);
    g.drawText(desc->category + " / " + desc->typeKey,
               subtitleRow, juce::Justification::centredLeft, false);
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

} // namespace

std::unique_ptr<NodeLibraryPanel> NodeLibraryPanel::create(
    const TNodeRegistry &registry, InsertCallback onInsert) {
  return std::make_unique<NodeLibraryPanelImpl>(registry, std::move(onInsert));
}

} // namespace Teul
