#include "Teul.h"

#include "Model/TGraphDocument.h"
#include "Registry/TNodeRegistry.h"
#include "UI/Canvas/TGraphCanvas.h"

#include <algorithm>
#include <limits>

// Teul sources are included here because the generated project currently compiles
// only Teul.cpp and TSerializer.cpp for the Teul module.
#include "Model/TGraphDocument.cpp"
#include "Registry/TNodeRegistry.cpp"
#include "UI/Port/TPortComponent.cpp"
#include "UI/Node/TNodeComponent.cpp"
#include "UI/Canvas/TGraphCanvas.cpp"

namespace Teul {
namespace {

class NodeLibraryPanel : public juce::Component,
                         private juce::ListBoxModel,
                         private juce::TextEditor::Listener,
                         private juce::KeyListener {
public:
  NodeLibraryPanel(const TNodeRegistry &registry,
                   std::function<void(const juce::String &)> onInsertFn)
      : nodeRegistry(registry), onInsert(std::move(onInsertFn)) {
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(listBox);

    searchEditor.setTextToShowWhenEmpty("Search nodes...", juce::Colours::grey);
    searchEditor.addListener(this);
    searchEditor.addKeyListener(this);
    searchEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(0xff1c1c1c));
    searchEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colour(0xff3a3a3a));

    listBox.setModel(this);
    listBox.setRowHeight(26);
    listBox.setColour(juce::ListBox::backgroundColourId,
                      juce::Colour(0xff151515));
    listBox.addKeyListener(this);

    for (const auto &desc : nodeRegistry.getAllDescriptors())
      allDescriptors.push_back(&desc);

    refreshFilter();
  }

  void resized() override {
    auto area = getLocalBounds().reduced(8);
    searchEditor.setBounds(area.removeFromTop(26));
    area.removeFromTop(6);
    listBox.setBounds(area);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff111111));
    g.setColour(juce::Colour(0xff2f2f2f));
    g.drawRect(getLocalBounds(), 1);
  }

private:
  int getNumRows() override { return static_cast<int>(filtered.size()); }

  void paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                        bool selected) override {
    if (row < 0 || row >= static_cast<int>(filtered.size()))
      return;

    const auto *desc = filtered[static_cast<size_t>(row)];
    if (!desc)
      return;

    if (selected) {
      g.setColour(juce::Colour(0xff355d9f));
      g.fillRoundedRectangle(2.0f, 2.0f, width - 4.0f, height - 4.0f, 4.0f);
    }

    g.setColour(juce::Colours::white);
    g.setFont(13.0f);
    g.drawText(desc->displayName, 8, 2, width - 16, 14,
               juce::Justification::centredLeft);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(11.0f);
    g.drawText(desc->category + " / " + desc->typeKey, 8, height - 13,
               width - 16, 11, juce::Justification::centredLeft);
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

  void refreshFilter() {
    filtered.clear();
    const juce::String q = searchEditor.getText().trim().toLowerCase();

    for (const auto *desc : allDescriptors) {
      if (!desc)
        continue;

      if (q.isEmpty() || desc->displayName.toLowerCase().contains(q) ||
          desc->typeKey.toLowerCase().contains(q) ||
          desc->category.toLowerCase().contains(q)) {
        filtered.push_back(desc);
      }
    }

    std::stable_sort(filtered.begin(), filtered.end(),
                     [](const TNodeDescriptor *a, const TNodeDescriptor *b) {
                       if (a->category != b->category)
                         return a->category < b->category;
                       return a->displayName < b->displayName;
                     });

    listBox.updateContent();
    if (!filtered.empty())
      listBox.selectRow(0);
  }

  void insertRow(int row) {
    if (row < 0 || row >= static_cast<int>(filtered.size()))
      return;

    const auto *desc = filtered[static_cast<size_t>(row)];
    if (desc == nullptr || onInsert == nullptr)
      return;

    onInsert(desc->typeKey);
  }

  const TNodeRegistry &nodeRegistry;
  std::function<void(const juce::String &)> onInsert;

  juce::TextEditor searchEditor;
  juce::ListBox listBox;

  std::vector<const TNodeDescriptor *> allDescriptors;
  std::vector<const TNodeDescriptor *> filtered;
};

int matchScore(const TNodeDescriptor &desc, const juce::String &query) {
  const juce::String q = query.trim().toLowerCase();
  if (q.isEmpty())
    return 1;

  const int idxDisplay = desc.displayName.toLowerCase().indexOf(q);
  if (idxDisplay >= 0)
    return 220 - idxDisplay * 2;

  const int idxType = desc.typeKey.toLowerCase().indexOf(q);
  if (idxType >= 0)
    return 190 - idxType * 2;

  const int idxCategory = desc.category.toLowerCase().indexOf(q);
  if (idxCategory >= 0)
    return 120 - idxCategory * 2;

  return std::numeric_limits<int>::min();
}

} // namespace

struct EditorHandle::Impl {
  explicit Impl(EditorHandle &ownerIn)
      : owner(ownerIn), registry(makeDefaultNodeRegistry()) {
    canvas = std::make_unique<TGraphCanvas>(doc);
    owner.addAndMakeVisible(*canvas);

    libraryPanel = std::make_unique<NodeLibraryPanel>(
        *registry, [this](const juce::String &typeKey) {
          if (canvas)
            canvas->addNodeByTypeAtView(typeKey, canvas->getViewCenter(), true);
        });

    owner.addAndMakeVisible(*libraryPanel);
    owner.addAndMakeVisible(toggleLibraryButton);
    owner.addAndMakeVisible(quickAddButton);
    owner.addAndMakeVisible(commandPaletteButton);

    toggleLibraryButton.setButtonText("Library");
    quickAddButton.setButtonText("Quick Add");
    commandPaletteButton.setButtonText("Cmd");

    toggleLibraryButton.onClick = [this] {
      libraryVisible = !libraryVisible;
      if (libraryPanel)
        libraryPanel->setVisible(libraryVisible);
      owner.resized();
    };

    quickAddButton.onClick = [this] { openQuickAddPrompt(); };
    commandPaletteButton.onClick = [this] { openCommandPalette(); };
  }

  void openQuickAddPrompt() {
    if (canvas == nullptr)
      return;

    juce::PopupMenu menu;
    std::vector<juce::String> keys;
    int id = 1;

    for (const auto &desc : registry->getAllDescriptors()) {
      menu.addItem(id, desc.displayName + " (" + desc.category + ")");
      keys.push_back(desc.typeKey);
      ++id;
      if (id > 40)
        break;
    }

    auto safeOwner = juce::Component::SafePointer<EditorHandle>(&owner);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&quickAddButton),
        [safeOwner, keys](int result) {
          if (safeOwner == nullptr || result <= 0 || result > (int)keys.size())
            return;
          if (auto *impl = safeOwner->impl.get(); impl != nullptr && impl->canvas != nullptr)
            impl->canvas->addNodeByTypeAtView(keys[(size_t)(result - 1)],
                                              impl->canvas->getViewCenter(), true);
        });
  }

  void openCommandPalette() {
    if (canvas == nullptr)
      return;

    juce::PopupMenu menu;
    menu.addItem(1, "Quick Add");
    menu.addItem(2, "Jump To Node");
    menu.addItem(3, "Add Bookmark");

    auto safeOwner = juce::Component::SafePointer<EditorHandle>(&owner);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&commandPaletteButton),
        [safeOwner](int result) {
          if (safeOwner == nullptr || result == 0)
            return;

          auto *impl = safeOwner->impl.get();
          if (impl == nullptr || impl->canvas == nullptr)
            return;

          if (result == 1) {
            impl->openQuickAddPrompt();
            return;
          }

          if (result == 2) {
            juce::PopupMenu nodeMenu;
            std::vector<NodeId> nodeIds;
            int id = 100;
            for (const auto &node : impl->doc.nodes) {
              const auto *desc = impl->registry->descriptorFor(node.typeKey);
              juce::String label = node.label.isNotEmpty() ? node.label :
                                   (desc ? desc->displayName : node.typeKey);
              nodeMenu.addItem(id, label);
              nodeIds.push_back(node.nodeId);
              ++id;
              if (id > 260)
                break;
            }

            nodeMenu.showMenuAsync(
                juce::PopupMenu::Options().withTargetComponent(&impl->commandPaletteButton),
                [safeOwner, nodeIds](int nodeResult) {
                  if (safeOwner == nullptr || nodeResult < 100)
                    return;
                  auto *safeImpl = safeOwner->impl.get();
                  if (safeImpl == nullptr || safeImpl->canvas == nullptr)
                    return;
                  const size_t index = (size_t)(nodeResult - 100);
                  if (index < nodeIds.size())
                    safeImpl->canvas->focusNode(nodeIds[index]);
                });
            return;
          }

          if (result == 3) {
            TBookmark bookmark;
            bookmark.bookmarkId = impl->doc.allocBookmarkId();
            bookmark.name = "Bookmark " + juce::String(bookmark.bookmarkId);
            bookmark.focusX = impl->canvas->viewToWorld({0.0f, 0.0f}).x;
            bookmark.focusY = impl->canvas->viewToWorld({0.0f, 0.0f}).y;
            bookmark.zoom = 1.0f;
            impl->doc.bookmarks.push_back(bookmark);
          }
        });
  }

  EditorHandle &owner;
  TGraphDocument doc;
  std::unique_ptr<TNodeRegistry> registry;
  std::unique_ptr<TGraphCanvas> canvas;
  std::unique_ptr<NodeLibraryPanel> libraryPanel;

  juce::TextButton toggleLibraryButton;
  juce::TextButton quickAddButton;
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
}

void EditorHandle::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff121212));
}

void EditorHandle::resized() {
  auto area = getLocalBounds();
  auto top = area.removeFromTop(34).reduced(6, 4);

  impl->toggleLibraryButton.setBounds(top.removeFromLeft(80));
  top.removeFromLeft(4);
  impl->quickAddButton.setBounds(top.removeFromLeft(90));
  top.removeFromLeft(4);
  impl->commandPaletteButton.setBounds(top.removeFromLeft(60));

  if (impl->libraryVisible) {
    auto left = area.removeFromLeft(260);
    impl->libraryPanel->setBounds(left);
  }

  if (impl->canvas)
    impl->canvas->setBounds(area);
}

std::unique_ptr<EditorHandle> createEditor() {
  return std::make_unique<EditorHandle>();
}

} // namespace Teul