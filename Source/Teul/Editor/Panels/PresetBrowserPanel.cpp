#include "Teul/Editor/Panels/PresetBrowserPanel.h"

namespace Teul {
namespace {

constexpr int filterAll = 1;
constexpr int filterTeul = 2;
constexpr int filterPatch = 3;
constexpr int filterState = 4;
constexpr int filterRecovery = 5;

juce::String formatTimestamp(const juce::Time &time) {
  if (time.toMilliseconds() <= 0)
    return "Unknown";
  return time.formatted("%Y-%m-%d %H:%M");
}

class PresetBrowserPanelImpl final : public PresetBrowserPanel,
                                     private juce::ListBoxModel,
                                     private juce::TextEditor::Listener,
                                     private juce::KeyListener {
public:
  PresetBrowserPanelImpl() : catalog(makeDefaultPresetCatalog()) {
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(filterBox);
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(refreshButton);
    addAndMakeVisible(listBox);
    addAndMakeVisible(kindValueLabel);
    addAndMakeVisible(pathValueLabel);
    addAndMakeVisible(summaryValueLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(primaryActionButton);
    addAndMakeVisible(secondaryActionButton);
    addAndMakeVisible(revealButton);
    addAndMakeVisible(sessionPreviewEditor);
    addAndMakeVisible(selectionPreviewEditor);
    addAndMakeVisible(detailEditor);

    titleLabel.setText("Preset Browser", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.94f));
    titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    filterBox.addItem("All", filterAll);
    filterBox.addItem("Teul", filterTeul);
    filterBox.addItem("Patch", filterPatch);
    filterBox.addItem("State", filterState);
    filterBox.addItem("Recovery", filterRecovery);
    filterBox.setSelectedId(filterAll, juce::dontSendNotification);
    filterBox.onChange = [this] { rebuildVisibleEntries(); };

    searchEditor.setTextToShowWhenEmpty("Search presets...", juce::Colours::grey);
    searchEditor.setEscapeAndReturnKeysConsumed(false);
    searchEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(0xff111827));
    searchEditor.setColour(juce::TextEditor::textColourId,
                           juce::Colours::white.withAlpha(0.95f));
    searchEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colour(0xff334155));
    searchEditor.addListener(this);
    searchEditor.addKeyListener(this);

    refreshButton.setButtonText("Refresh");
    refreshButton.onClick = [this] { refreshEntries(true); };

    listBox.setModel(this);
    listBox.setRowHeight(50);
    listBox.setColour(juce::ListBox::backgroundColourId,
                      juce::Colour(0xff0f172a));
    listBox.setMultipleSelectionEnabled(false);
    listBox.addKeyListener(this);

    auto configureLabel = [](juce::Label &label, float fontHeight,
                             juce::Justification justification,
                             float alpha = 0.90f, bool bold = false) {
      label.setJustificationType(justification);
      label.setColour(juce::Label::textColourId,
                      juce::Colours::white.withAlpha(alpha));
      label.setFont(juce::FontOptions(fontHeight,
                                      bold ? juce::Font::bold
                                           : juce::Font::plain));
    };

    configureLabel(kindValueLabel, 14.0f, juce::Justification::centredLeft,
                   0.92f, true);
    configureLabel(pathValueLabel, 11.0f, juce::Justification::centredLeft,
                   0.68f);
    configureLabel(summaryValueLabel, 12.0f, juce::Justification::centredLeft,
                   0.78f);
    summaryValueLabel.setMinimumHorizontalScale(0.7f);
    configureLabel(statusLabel, 11.0f, juce::Justification::centredLeft,
                   0.78f);

    primaryActionButton.onClick = [this] { performPrimaryAction(); };
    secondaryActionButton.onClick = [this] { performSecondaryAction(); };
    revealButton.setButtonText("Reveal");
    revealButton.onClick = [this] { revealSelectedPreset(); };

    auto configurePreviewEditor = [](juce::TextEditor &editor,
                                     juce::Colour background) {
      editor.setMultiLine(true);
      editor.setReadOnly(true);
      editor.setScrollbarsShown(false);
      editor.setCaretVisible(false);
      editor.setPopupMenuEnabled(false);
      editor.setColour(juce::TextEditor::backgroundColourId, background);
      editor.setColour(juce::TextEditor::textColourId,
                       juce::Colours::white.withAlpha(0.88f));
      editor.setColour(juce::TextEditor::outlineColourId,
                       juce::Colour(0xff334155));
      editor.setVisible(false);
    };

    configurePreviewEditor(sessionPreviewEditor, juce::Colour(0xff111827));
    configurePreviewEditor(selectionPreviewEditor, juce::Colour(0xff0f172a));

    detailEditor.setMultiLine(true);
    detailEditor.setReadOnly(true);
    detailEditor.setScrollbarsShown(true);
    detailEditor.setCaretVisible(false);
    detailEditor.setPopupMenuEnabled(false);
    detailEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(0xff0b1220));
    detailEditor.setColour(juce::TextEditor::textColourId,
                           juce::Colours::white.withAlpha(0.84f));
    detailEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colour(0xff334155));

    setVisible(false);
    refreshDetailPanel();
  }

  void setLayoutChangedCallback(std::function<void()> callback) override {
    layoutChangedCallback = std::move(callback);
  }

  bool isBrowserOpen() const noexcept override { return browserOpen; }

  void setBrowserOpen(bool shouldOpen) override {
    if (browserOpen == shouldOpen) {
      setVisible(browserOpen);
      return;
    }

    browserOpen = shouldOpen;
    setVisible(browserOpen);
    if (browserOpen)
      refreshEntries(true);

    if (layoutChangedCallback != nullptr)
      layoutChangedCallback();
  }

  void refreshEntries(bool force) override {
    if (!force && !browserOpen)
      return;

    const auto previousEntryId = selectedEntry != nullptr ? selectedEntry->entryId
                                                          : juce::String();
    catalog->reload();
    rebuildVisibleEntries(previousEntryId);
  }

  void setPrimaryActionHandler(PrimaryActionHandler handler) override {
    primaryActionHandler = std::move(handler);
    updateActionButtons();
  }

  void setSecondaryActionHandler(SecondaryActionHandler handler) override {
    secondaryActionHandler = std::move(handler);
    updateActionButtons();
  }

  void setEntryPreviewHandler(EntryPreviewHandler handler) override {
    entryPreviewHandler = std::move(handler);
    refreshDetailPanel();
  }

  void setSessionPreview(const juce::String &summaryText,
                         const juce::String &detailText,
                         bool warning) override {
    sessionPreviewSummary = summaryText;
    sessionPreviewDetail = detailText;
    sessionPreviewWarning = warning;
    sessionPreviewEditor.setText(
        sessionPreviewSummary.isNotEmpty()
            ? sessionPreviewSummary + "\r\n" + sessionPreviewDetail
            : sessionPreviewDetail,
        false);
    sessionPreviewEditor.setColour(
        juce::TextEditor::outlineColourId,
        warning ? juce::Colour(0xfff59e0b) : juce::Colour(0xff334155));
    sessionPreviewEditor.setVisible(sessionPreviewSummary.isNotEmpty() ||
                                    sessionPreviewDetail.isNotEmpty());
    resized();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff07101d));
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRect(getLocalBounds(), 1);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);

    auto header = area.removeFromTop(28);
    titleLabel.setBounds(header.removeFromLeft(160));
    header.removeFromLeft(8);
    filterBox.setBounds(header.removeFromLeft(118));
    header.removeFromLeft(8);
    refreshButton.setBounds(header.removeFromRight(88));
    header.removeFromRight(8);
    searchEditor.setBounds(header);

    area.removeFromTop(8);

    auto left = area.removeFromLeft(298);
    listBox.setBounds(left);
    area.removeFromLeft(12);

    if (sessionPreviewEditor.isVisible()) {
      auto previewArea = area.removeFromTop(58);
      sessionPreviewEditor.setBounds(previewArea);
      area.removeFromTop(8);
    } else {
      sessionPreviewEditor.setBounds({});
    }

    auto infoArea = area.removeFromTop(84);
    kindValueLabel.setBounds(infoArea.removeFromTop(22));
    infoArea.removeFromTop(2);
    pathValueLabel.setBounds(infoArea.removeFromTop(20));
    infoArea.removeFromTop(2);
    summaryValueLabel.setBounds(infoArea.removeFromTop(40));

    area.removeFromTop(8);

    if (selectionPreviewEditor.isVisible()) {
      auto diffArea = area.removeFromTop(66);
      selectionPreviewEditor.setBounds(diffArea);
      area.removeFromTop(8);
    } else {
      selectionPreviewEditor.setBounds({});
    }

    auto actions = area.removeFromTop(28);
    primaryActionButton.setBounds(actions.removeFromLeft(112));
    actions.removeFromLeft(8);
    secondaryActionButton.setBounds(actions.removeFromLeft(100));
    actions.removeFromLeft(8);
    revealButton.setBounds(actions.removeFromLeft(88));
    actions.removeFromLeft(10);
    statusLabel.setBounds(actions);

    area.removeFromTop(8);
    detailEditor.setBounds(area);
  }

private:
  struct VisibleEntry {
    const TPresetEntry *entry = nullptr;
  };

  int getNumRows() override { return (int)visibleEntries.size(); }

  void paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                        bool selected) override {
    if (row < 0 || row >= (int)visibleEntries.size())
      return;

    const auto *entry = visibleEntries[(size_t)row].entry;
    if (entry == nullptr)
      return;

    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(4, 2);
    const auto accent = entry->degraded ? juce::Colour(0xfff59e0b)
                                        : juce::Colour(0xff38bdf8);
    if (selected) {
      g.setColour(accent.withAlpha(0.20f));
      g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
      g.setColour(accent.withAlpha(0.85f));
      g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);
    } else {
      g.setColour(juce::Colour(0xff101826));
      g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
      g.setColour(juce::Colour(0xff1e293b));
      g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.0f);
    }

    auto textArea = bounds.reduced(10, 6);
    auto top = textArea.removeFromTop(18);
    auto tagArea = top.removeFromRight(112);
    g.setColour(juce::Colours::white.withAlpha(entry->available ? 0.95f : 0.65f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(entry->displayName, top, juce::Justification::centredLeft, false);

    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(tagArea.toFloat(), 7.0f);
    g.setColour(accent.withAlpha(0.92f));
    g.drawRoundedRectangle(tagArea.toFloat(), 7.0f, 1.0f);
    g.setColour(accent.brighter(0.12f));
    g.setFont(10.0f);
    g.drawText(entry->kindLabel, tagArea, juce::Justification::centred, false);

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(11.0f);
    const auto footer = entry->summaryText.isNotEmpty()
                            ? entry->summaryText
                            : juce::String("No summary");
    g.drawText(footer, textArea, juce::Justification::centredLeft, false);
  }

  void selectedRowsChanged(int lastRowSelected) override {
    juce::ignoreUnused(lastRowSelected);
    const auto selectedRow = listBox.getSelectedRow();
    selectedEntry = (selectedRow >= 0 && selectedRow < (int)visibleEntries.size())
                        ? visibleEntries[(size_t)selectedRow].entry
                        : nullptr;
    refreshDetailPanel();
  }

  void textEditorTextChanged(juce::TextEditor &) override {
    rebuildVisibleEntries();
  }

  bool keyPressed(const juce::KeyPress &key, juce::Component *) override {
    if (key == juce::KeyPress::returnKey) {
      performPrimaryAction();
      return true;
    }
    return false;
  }

  void rebuildVisibleEntries(const juce::String &preferredEntryId = {}) {
    visibleEntries.clear();
    for (const auto &entry : catalog->getEntries()) {
      if (!passesFilter(entry))
        continue;
      visibleEntries.push_back({&entry});
    }

    listBox.updateContent();

    int rowToSelect = -1;
    if (preferredEntryId.isNotEmpty()) {
      for (int row = 0; row < (int)visibleEntries.size(); ++row) {
        if (visibleEntries[(size_t)row].entry != nullptr &&
            visibleEntries[(size_t)row].entry->entryId == preferredEntryId) {
          rowToSelect = row;
          break;
        }
      }
    }

    if (rowToSelect < 0 && !visibleEntries.empty())
      rowToSelect = 0;

    if (rowToSelect >= 0)
      listBox.selectRow(rowToSelect);
    else {
      listBox.deselectAllRows();
      selectedEntry = nullptr;
      refreshDetailPanel();
    }
  }

  bool passesFilter(const TPresetEntry &entry) const {
    const auto filterId = filterBox.getSelectedId();
    if (filterId == filterTeul && !entry.domains.contains("teul"))
      return false;
    if (filterId == filterPatch && entry.presetKind != "teul.patch")
      return false;
    if (filterId == filterState && entry.presetKind != "teul.state")
      return false;
    if (filterId == filterRecovery && entry.presetKind != "teul.recovery")
      return false;

    const auto query = searchEditor.getText().trim().toLowerCase();
    if (query.isEmpty())
      return true;

    const auto haystack =
        (entry.displayName + " " + entry.summaryText + " " + entry.detailText +
         " " + entry.presetKind + " " + entry.domains.joinIntoString(" "))
            .toLowerCase();
    return haystack.contains(query);
  }

  void refreshDetailPanel() {
    if (selectedEntry == nullptr) {
      kindValueLabel.setText("No preset selected", juce::dontSendNotification);
      pathValueLabel.setText({}, juce::dontSendNotification);
      summaryValueLabel.setText("Create or save presets, then refresh to browse.",
                                juce::dontSendNotification);
      statusLabel.setText("No preset selected", juce::dontSendNotification);
      selectionPreviewEditor.setVisible(false);
      selectionPreviewEditor.setText({}, false);
      detailEditor.setText(
          "Preset Browser MVP\r\n"
          "- Uses a shared PresetEntry/provider model.\r\n"
          "- Currently scans Teul patch, state, and recovery snapshot sources.\r\n"
          "- Future providers can add composite or multi-domain presets.",
          false);
      updateActionButtons();
      return;
    }

    const juce::String availability =
        selectedEntry->available ? juce::String("Ready") : juce::String("Invalid");
    const juce::String degradedSuffix = selectedEntry->degraded
                                            ? juce::String("  |  degraded")
                                            : juce::String();
    kindValueLabel.setText(selectedEntry->displayName + "  |  " +
                               selectedEntry->kindLabel,
                           juce::dontSendNotification);
    pathValueLabel.setText(joinFileLine(*selectedEntry,
                                        availability + degradedSuffix),
                           juce::dontSendNotification);
    summaryValueLabel.setText(selectedEntry->summaryText.isNotEmpty()
                                  ? selectedEntry->summaryText
                                  : "No summary available",
                              juce::dontSendNotification);

    if (selectedEntry->warningText.isNotEmpty()) {
      statusLabel.setText(selectedEntry->warningText, juce::dontSendNotification);
    } else if (selectedEntry->available) {
      statusLabel.setText("Primary action is ready.", juce::dontSendNotification);
    } else {
      statusLabel.setText("Invalid preset file; reveal it to inspect.",
                          juce::dontSendNotification);
    }

    juce::String previewSummary;
    juce::String previewDetail;
    bool previewWarning = false;
    if (entryPreviewHandler != nullptr && selectedEntry->available)
      entryPreviewHandler(*selectedEntry, previewSummary, previewDetail,
                          previewWarning);

    selectionPreviewEditor.setText(
        previewSummary.isNotEmpty() ? previewSummary + "\r\n" + previewDetail
                                    : previewDetail,
        false);
    selectionPreviewEditor.setColour(
        juce::TextEditor::outlineColourId,
        previewWarning ? juce::Colour(0xfff59e0b) : juce::Colour(0xff334155));
    selectionPreviewEditor.setVisible(previewSummary.isNotEmpty() ||
                                      previewDetail.isNotEmpty());

    detailEditor.setText(selectedEntry->detailText, false);
    updateActionButtons();
    resized();
  }

  static juce::String joinFileLine(const TPresetEntry &entry,
                                   const juce::String &statusText) {
    juce::StringArray parts;
    parts.add(statusText);
    parts.add(formatTimestamp(entry.modifiedTime));
    parts.add(entry.file.getFileName());
    return parts.joinIntoString("  |  ");
  }

  void performPrimaryAction() {
    if (selectedEntry == nullptr || primaryActionHandler == nullptr)
      return;

    const auto selectedEntryId = selectedEntry->entryId;
    const auto result = primaryActionHandler(*selectedEntry);
    if (result.failed()) {
      statusLabel.setText(result.getErrorMessage(), juce::dontSendNotification);
      return;
    }

    refreshEntries(true);
    rebuildVisibleEntries(selectedEntryId);
  }

  void performSecondaryAction() {
    if (selectedEntry == nullptr || secondaryActionHandler == nullptr ||
        selectedEntry->secondaryActionLabel.isEmpty())
      return;

    const auto selectedEntryId = selectedEntry->entryId;
    const auto result = secondaryActionHandler(*selectedEntry);
    if (result.failed()) {
      statusLabel.setText(result.getErrorMessage(), juce::dontSendNotification);
      return;
    }

    refreshEntries(true);
    rebuildVisibleEntries(selectedEntryId);
  }

  void revealSelectedPreset() {
    if (selectedEntry == nullptr)
      return;

    if (selectedEntry->file.exists())
      selectedEntry->file.revealToUser();
  }

  void updateActionButtons() {
    const bool hasEntry = selectedEntry != nullptr;
    const bool allowPrimary =
        hasEntry && selectedEntry->available && primaryActionHandler != nullptr;
    const bool allowSecondary =
        hasEntry && selectedEntry->secondaryActionLabel.isNotEmpty() &&
        secondaryActionHandler != nullptr;
    primaryActionButton.setButtonText(
        hasEntry ? selectedEntry->primaryActionLabel : "Run");
    secondaryActionButton.setButtonText(
        hasEntry && selectedEntry->secondaryActionLabel.isNotEmpty()
            ? selectedEntry->secondaryActionLabel
            : "More");
    primaryActionButton.setEnabled(allowPrimary);
    secondaryActionButton.setEnabled(allowSecondary);
    revealButton.setEnabled(hasEntry && selectedEntry->file.exists());
  }

  std::function<void()> layoutChangedCallback;
  PrimaryActionHandler primaryActionHandler;
  SecondaryActionHandler secondaryActionHandler;
  EntryPreviewHandler entryPreviewHandler;
  std::unique_ptr<TPresetCatalog> catalog;
  juce::String sessionPreviewSummary;
  juce::String sessionPreviewDetail;
  bool sessionPreviewWarning = false;
  std::vector<VisibleEntry> visibleEntries;
  const TPresetEntry *selectedEntry = nullptr;
  bool browserOpen = false;

  juce::Label titleLabel;
  juce::ComboBox filterBox;
  juce::TextEditor searchEditor;
  juce::TextButton refreshButton;
  juce::ListBox listBox;
  juce::Label kindValueLabel;
  juce::Label pathValueLabel;
  juce::Label summaryValueLabel;
  juce::Label statusLabel;
  juce::TextButton primaryActionButton;
  juce::TextButton secondaryActionButton;
  juce::TextButton revealButton;
  juce::TextEditor sessionPreviewEditor;
  juce::TextEditor selectionPreviewEditor;
  juce::TextEditor detailEditor;
};

} // namespace

std::unique_ptr<PresetBrowserPanel> PresetBrowserPanel::create() {
  return std::make_unique<PresetBrowserPanelImpl>();
}

} // namespace Teul
