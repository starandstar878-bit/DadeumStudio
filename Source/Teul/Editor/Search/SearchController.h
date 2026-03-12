#pragma once

#include "Teul/Editor/Canvas/TGraphCanvas.h"
#include "Teul/Registry/TNodeRegistry.h"

namespace Teul {

const TNodeRegistry *getSharedRegistry();
juce::String nodeLabelForUi(const TNode &node, const TNodeRegistry *registry);
juce::String categoryLabelForTypeKey(const juce::String &typeKey);
int scoreTextMatch(const juce::String &textRaw, const juce::String &queryRaw);
juce::String extractLibraryDragTypeKey(const juce::var &description);

struct SearchEntry {
  juce::String title;
  juce::String subtitle;
  std::function<void()> onSelect;
};

class TGraphCanvas::SearchOverlay final : public juce::Component,
                                          private juce::ListBoxModel,
                                          private juce::TextEditor::Listener,
                                          private juce::KeyListener {
public:
  using Provider = std::function<std::vector<SearchEntry>(const juce::String &)>;

  explicit SearchOverlay(TGraphCanvas &ownerIn) : owner(ownerIn) {
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(listBox);

    searchEditor.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
    searchEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(0xff141414));
    searchEditor.setColour(juce::TextEditor::textColourId,
                           juce::Colours::white);
    searchEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colour(0xff2d2d2d));
    searchEditor.setEscapeAndReturnKeysConsumed(false);
    searchEditor.addListener(this);
    searchEditor.addKeyListener(this);
    searchEditor.setSelectAllWhenFocused(true);

    listBox.setModel(this);
    listBox.setRowHeight(29);
    listBox.setColour(juce::ListBox::backgroundColourId,
                      juce::Colour(0x00000000));
    listBox.addKeyListener(this);

    setVisible(false);
  }

  void present(const juce::String &titleIn, const juce::String &placeholderIn,
               Provider providerIn, bool anchoredIn,
               juce::Point<float> anchorIn) {
    title = titleIn;
    provider = std::move(providerIn);
    anchored = anchoredIn;
    anchorPoint = anchorIn;
    searchEditor.setTextToShowWhenEmpty(placeholderIn, juce::Colours::grey);
    searchEditor.setText({}, juce::dontSendNotification);
    setVisible(true);
    toFront(false);
    refreshItems();
    searchEditor.grabKeyboardFocus();
  }

  bool isOpen() const noexcept { return isVisible(); }

  void dismiss() {
    if (!isVisible())
      return;

    setVisible(false);
    items.clear();
    listBox.updateContent();
    owner.grabKeyboardFocus();
  }

  void resized() override {
    const int width = 356;
    const int rows = juce::jlimit(5, 7, juce::jmax(5, (int)items.size()));
    const int desiredHeight = 82 + rows * listBox.getRowHeight();
    const int height = juce::jmin(desiredHeight, getHeight() - 32);

    if (anchored) {
      int x = juce::roundToInt(anchorPoint.x - width * 0.35f);
      int y = juce::roundToInt(anchorPoint.y + 12.0f);
      x = juce::jlimit(16, juce::jmax(16, getWidth() - width - 16), x);
      y = juce::jlimit(16, juce::jmax(16, getHeight() - height - 16), y);
      panelBounds = {x, y, width, height};
    } else {
      panelBounds = {(getWidth() - width) / 2,
                     juce::jmax(18, getHeight() / 7), width, height};
    }

    auto area = panelBounds.reduced(9);
    area.removeFromTop(16);
    searchEditor.setBounds(area.removeFromTop(22));
    area.removeFromTop(5);
    listBox.setBounds(area);
  }

  void paint(juce::Graphics &g) override {
    if (!isVisible())
      return;

    g.fillAll(juce::Colour(0x66000000));
    g.setColour(juce::Colour(0xff111111));
    g.fillRoundedRectangle(panelBounds.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xff2f2f2f));
    g.drawRoundedRectangle(panelBounds.toFloat(), 8.0f, 1.0f);

    auto titleBounds = panelBounds.withHeight(24).reduced(9, 5);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::FontOptions(12.6f, juce::Font::bold));
    g.drawText(title, titleBounds, juce::Justification::centredLeft, false);

    if (items.empty()) {
      g.setColour(juce::Colours::white.withAlpha(0.45f));
      g.setFont(10.4f);
      g.drawText("No matches", listBox.getBounds(),
                 juce::Justification::centred, false);
    }
  }

  void mouseDown(const juce::MouseEvent &event) override {
    if (!panelBounds.contains(event.getPosition()))
      dismiss();
  }

private:
  int getNumRows() override { return (int)items.size(); }

  void paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override {
    if (row < 0 || row >= (int)items.size())
      return;

    const auto &item = items[(size_t)row];
    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1);

    if (rowIsSelected) {
      g.setColour(juce::Colour(0xff234a7e));
      g.fillRoundedRectangle(bounds.toFloat(), 4.5f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(10.8f);
    g.drawText(item.title, bounds.removeFromTop(14).reduced(7, 0),
               juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.52f));
    g.setFont(9.1f);
    g.drawText(item.subtitle, bounds.reduced(7, 0),
               juce::Justification::centredLeft, false);
  }

  void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override {
    activateRow(row);
  }

  void returnKeyPressed(int row) override { activateRow(row); }

  void textEditorTextChanged(juce::TextEditor &) override { refreshItems(); }

  bool keyPressed(const juce::KeyPress &key, juce::Component *) override {
    if (key == juce::KeyPress::escapeKey) {
      dismiss();
      return true;
    }

    if (key == juce::KeyPress::returnKey) {
      activateRow(listBox.getSelectedRow());
      return true;
    }

    if (key == juce::KeyPress::downKey || key == juce::KeyPress::tabKey) {
      moveSelection(1);
      return true;
    }

    if (key == juce::KeyPress::upKey) {
      moveSelection(-1);
      return true;
    }

    return false;
  }

  void refreshItems() {
    items = provider ? provider(searchEditor.getText()) : std::vector<SearchEntry>{};
    listBox.updateContent();

    if (items.empty()) {
      listBox.selectRow(-1);
    } else {
      const int current = juce::jlimit(0, (int)items.size() - 1,
                                       juce::jmax(0, listBox.getSelectedRow()));
      listBox.selectRow(current);
      listBox.scrollToEnsureRowIsOnscreen(current);
    }

    resized();
    repaint();
  }

  void moveSelection(int delta) {
    if (items.empty())
      return;

    const int row = juce::jlimit(0, (int)items.size() - 1,
                                 juce::jmax(0, listBox.getSelectedRow()) + delta);
    listBox.selectRow(row);
    listBox.scrollToEnsureRowIsOnscreen(row);
  }

  void activateRow(int row) {
    if (row < 0 || row >= (int)items.size())
      return;

    auto action = items[(size_t)row].onSelect;
    dismiss();
    if (action)
      action();
  }

  TGraphCanvas &owner;
  juce::TextEditor searchEditor;
  juce::ListBox listBox;
  juce::Rectangle<int> panelBounds;
  juce::String title;
  Provider provider;
  std::vector<SearchEntry> items;
  bool anchored = false;
  juce::Point<float> anchorPoint;
};

} // namespace Teul
