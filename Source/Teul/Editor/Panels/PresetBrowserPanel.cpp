#include "Teul/Editor/Panels/PresetBrowserPanel.h"

namespace Teul {
namespace {

constexpr int filterAll = 1;
constexpr int filterTeul = 2;
constexpr int filterPatch = 3;
constexpr int filterState = 4;
constexpr int filterRecovery = 5;
constexpr int filterFavorites = 6;
constexpr int filterRecent = 7;
constexpr int tagFilterAll = 1;

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
    addAndMakeVisible(tagFilterBox);
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(refreshButton);
    addAndMakeVisible(listBox);
    addAndMakeVisible(kindValueLabel);
    addAndMakeVisible(pathValueLabel);
    addAndMakeVisible(summaryValueLabel);
    addAndMakeVisible(tagsEditor);
    addAndMakeVisible(saveTagsButton);
    addAndMakeVisible(conflictLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(primaryActionButton);
    addAndMakeVisible(secondaryActionButton);
    addAndMakeVisible(clearRecentButton);
    addAndMakeVisible(cancelConflictButton);
    addAndMakeVisible(favoriteButton);
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
    filterBox.addItem("Favorites", filterFavorites);
    filterBox.addItem("Recent", filterRecent);
    filterBox.setSelectedId(filterAll, juce::dontSendNotification);
    filterBox.onChange = [this] { rebuildVisibleEntries(); };

    tagFilterBox.addItem("All Tags", tagFilterAll);
    tagFilterBox.setSelectedId(tagFilterAll, juce::dontSendNotification);
    tagFilterBox.onChange = [this] { rebuildVisibleEntries(); };

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
    tagsEditor.addKeyListener(this);

    refreshButton.setButtonText("Refresh");
    refreshButton.onClick = [this] { refreshEntries(true); };

    tagsEditor.setTextToShowWhenEmpty("tags, comma, separated",
                                      juce::Colours::grey);
    tagsEditor.setColour(juce::TextEditor::backgroundColourId,
                         juce::Colour(0xff111827));
    tagsEditor.setColour(juce::TextEditor::textColourId,
                         juce::Colours::white.withAlpha(0.95f));
    tagsEditor.setColour(juce::TextEditor::outlineColourId,
                         juce::Colour(0xff334155));
    tagsEditor.setEscapeAndReturnKeysConsumed(false);
    saveTagsButton.setButtonText("Save Tags");
    saveTagsButton.onClick = [this] { saveTags(); };

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
    configureLabel(conflictLabel, 11.0f, juce::Justification::centredLeft,
                   0.86f, true);
    conflictLabel.setColour(juce::Label::textColourId,
                            juce::Colour(0xfffcd34d));
    conflictLabel.setVisible(false);
    configureLabel(statusLabel, 11.0f, juce::Justification::centredLeft,
                   0.78f);

    primaryActionButton.onClick = [this] { performPrimaryAction(); };
    secondaryActionButton.onClick = [this] { performSecondaryAction(); };
    cancelConflictButton.setButtonText("Cancel");
    cancelConflictButton.onClick = [this] {
      cancelConflictArm("Conflict confirmation cleared.");
    };
    favoriteButton.onClick = [this] { toggleFavorite(); };
    clearRecentButton.setButtonText("Forget");
    clearRecentButton.onClick = [this] { clearRecent(); };
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
    tagFilterBox.setBounds(header.removeFromLeft(132));
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

    auto infoArea = area.removeFromTop(110);
    kindValueLabel.setBounds(infoArea.removeFromTop(22));
    infoArea.removeFromTop(2);
    pathValueLabel.setBounds(infoArea.removeFromTop(20));
    infoArea.removeFromTop(2);
    summaryValueLabel.setBounds(infoArea.removeFromTop(40));
    infoArea.removeFromTop(6);
    saveTagsButton.setBounds(infoArea.removeFromRight(90));
    infoArea.removeFromRight(8);
    tagsEditor.setBounds(infoArea.removeFromTop(20));

    area.removeFromTop(8);

    if (selectionPreviewEditor.isVisible()) {
      auto diffArea = area.removeFromTop(88);
      selectionPreviewEditor.setBounds(diffArea);
      area.removeFromTop(8);
    } else {
      selectionPreviewEditor.setBounds({});
    }

    if (isConflictArmed()) {
      auto conflictArea = area.removeFromTop(28);
      cancelConflictButton.setBounds(conflictArea.removeFromRight(84));
      conflictArea.removeFromRight(8);
      conflictLabel.setBounds(conflictArea);
      area.removeFromTop(8);
    } else {
      conflictLabel.setBounds({});
      cancelConflictButton.setBounds({});
    }

    auto actions = area.removeFromTop(28);
    primaryActionButton.setBounds(actions.removeFromLeft(112));
    actions.removeFromLeft(8);
    secondaryActionButton.setBounds(actions.removeFromLeft(100));
    actions.removeFromLeft(8);
    clearRecentButton.setBounds(actions.removeFromLeft(84));
    actions.removeFromLeft(8);
    favoriteButton.setBounds(actions.removeFromLeft(96));
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
    const auto titleText = entry->favorite ? juce::String("* ") + entry->displayName
                                           : entry->displayName;
    g.drawText(titleText, top, juce::Justification::centredLeft, false);

    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(tagArea.toFloat(), 7.0f);
    g.setColour(accent.withAlpha(0.92f));
    g.drawRoundedRectangle(tagArea.toFloat(), 7.0f, 1.0f);
    g.setColour(accent.brighter(0.12f));
    g.setFont(10.0f);
    g.drawText(entry->kindLabel, tagArea, juce::Justification::centred, false);

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.setFont(11.0f);
    juce::String footer = entry->summaryText.isNotEmpty()
                               ? entry->summaryText
                               : juce::String("No summary");
    if (!entry->tags.isEmpty())
      footer << "  |  Tags: " << entry->tags.joinIntoString(", ");
    if (entry->recent)
      footer = juce::String("Recent  |  ") + footer;
    g.drawText(footer, textArea, juce::Justification::centredLeft, false);
  }

  void selectedRowsChanged(int lastRowSelected) override {
    juce::ignoreUnused(lastRowSelected);
    const auto selectedRow = listBox.getSelectedRow();
    selectedEntry = (selectedRow >= 0 && selectedRow < (int)visibleEntries.size())
                        ? visibleEntries[(size_t)selectedRow].entry
                        : nullptr;
    clearConflictArmIfSelectionChanged();
    refreshDetailPanel();
  }

  void textEditorTextChanged(juce::TextEditor &editor) override {
    if (&editor == &searchEditor)
      rebuildVisibleEntries();
  }

  bool keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent) override {
    if (key == juce::KeyPress::returnKey) {
      if (originatingComponent == &tagsEditor) {
        saveTags();
        return true;
      }
      performPrimaryAction();
      return true;
    }
    return false;
  }

  void rebuildVisibleEntries(const juce::String &preferredEntryId = {}) {
    refreshTagFilterOptions();
    visibleEntries.clear();
    for (const auto &entry : catalog->getEntries()) {
      if (!passesFilter(entry))
        continue;
      visibleEntries.push_back({&entry});
    }

    listBox.updateContent();
    if (preferredEntryId.isEmpty())
      pendingConflictEntryId.clear();

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
    if (filterId == filterFavorites && !entry.favorite)
      return false;
    if (filterId == filterRecent && !entry.recent)
      return false;

    const auto selectedTagId = tagFilterBox.getSelectedId();
    if (selectedTagId > tagFilterAll) {
      const auto selectedTag = tagFilterBox.getText().trim();
      if (selectedTag.isNotEmpty() && !entry.tags.contains(selectedTag))
        return false;
    }

    const auto query = searchEditor.getText().trim().toLowerCase();
    if (query.isEmpty())
      return true;

    const auto haystack =
        (entry.displayName + " " + entry.summaryText + " " + entry.detailText +
         " " + entry.presetKind + " " + entry.domains.joinIntoString(" ") +
         " " + entry.tags.joinIntoString(" "))
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
      pendingConflictEntryId.clear();
      conflictLabel.setVisible(false);
      cancelConflictButton.setVisible(false);
      clearRecentButton.setVisible(false);
      tagsEditor.setText({}, false);
      selectionPreviewEditor.setVisible(false);
      selectionPreviewEditor.setText({}, false);
      detailEditor.setText(
          "Preset Browser MVP\r\n"
          "- Uses a shared PresetEntry/provider model.\r\n"
          "- Currently scans Teul patch, state, and recovery snapshot sources.\r\n"
          "- Future providers can add composite or multi-domain presets.",
          false);
      updateActionButtons();
      resized();
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
    tagsEditor.setText(selectedEntry->tags.joinIntoString(", "), false);

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

    if (requiresConflictConfirmation(*selectedEntry)) {
      if (previewSummary.isEmpty())
        previewSummary = "Conflict Resolution";
      if (previewDetail.isNotEmpty())
        previewDetail << "\r\n";
      previewDetail << conflictDetailText(*selectedEntry);
      if (pendingConflictEntryId == selectedEntry->entryId) {
        previewDetail << "\r\n\r\nConfirm or cancel before continuing.";
        previewSummary = "Conflict Armed";
      }
      previewWarning = true;
    }

    selectionPreviewEditor.setText(
        previewSummary.isNotEmpty() ? previewSummary + "\r\n" + previewDetail
                                    : previewDetail,
        false);
    selectionPreviewEditor.setColour(
        juce::TextEditor::outlineColourId,
        previewWarning ? juce::Colour(0xfff59e0b) : juce::Colour(0xff334155));
    selectionPreviewEditor.setVisible(previewSummary.isNotEmpty() ||
                                      previewDetail.isNotEmpty());

    conflictLabel.setVisible(isConflictArmed());
    detailEditor.setText(selectedEntry->detailText, false);
    if (isConflictArmed()) {
      conflictLabel.setText(
          "Unsaved session conflict is armed for this preset action.",
          juce::dontSendNotification);
      statusLabel.setText("Confirm with the primary action or cancel.",
                          juce::dontSendNotification);
    }
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

    if (requiresConflictConfirmation(*selectedEntry) &&
        pendingConflictEntryId != selectedEntry->entryId) {
      pendingConflictEntryId = selectedEntry->entryId;
      statusLabel.setText(
          selectedEntry->primaryActionLabel +
              " is armed. Review the conflict preview and press again to continue.",
          juce::dontSendNotification);
      updateActionButtons();
      return;
    }

    const auto selectedEntryId = selectedEntry->entryId;
    const auto result = primaryActionHandler(*selectedEntry);
    if (result.failed()) {
      statusLabel.setText(result.getErrorMessage(), juce::dontSendNotification);
      return;
    }

    pendingConflictEntryId.clear();
    catalog->markUsed(selectedEntryId);
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

    pendingConflictEntryId.clear();
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
    const bool armed =
        hasEntry && pendingConflictEntryId == selectedEntry->entryId;
    primaryActionButton.setButtonText(
        hasEntry ? (armed ? selectedEntry->primaryActionLabel + " Anyway"
                          : selectedEntry->primaryActionLabel)
                 : "Run");
    secondaryActionButton.setButtonText(
        hasEntry && selectedEntry->secondaryActionLabel.isNotEmpty()
            ? selectedEntry->secondaryActionLabel
            : "More");
    favoriteButton.setButtonText(
        hasEntry && selectedEntry->favorite ? "Unfavorite" : "Favorite");
    clearRecentButton.setEnabled(hasEntry && selectedEntry->recent);
    clearRecentButton.setVisible(hasEntry);
    saveTagsButton.setEnabled(hasEntry);
    primaryActionButton.setEnabled(allowPrimary);
    secondaryActionButton.setEnabled(allowSecondary);
    cancelConflictButton.setEnabled(isConflictArmed());
    cancelConflictButton.setVisible(isConflictArmed());
    favoriteButton.setEnabled(hasEntry);
    revealButton.setEnabled(hasEntry && selectedEntry->file.exists());
  }

  bool isConflictArmed() const {
    return selectedEntry != nullptr &&
           pendingConflictEntryId == selectedEntry->entryId;
  }

  void refreshTagFilterOptions() {
    const auto previousText = tagFilterBox.getText().trim();
    const auto previousSelected = tagFilterBox.getSelectedId();
    juce::StringArray tags;
    for (const auto &entry : catalog->getEntries()) {
      for (const auto &tag : entry.tags) {
        const auto trimmed = tag.trim();
        if (trimmed.isNotEmpty() && !tags.contains(trimmed))
          tags.add(trimmed);
      }
    }

    tags.sort(true);
    tagFilterBox.clear(juce::dontSendNotification);
    tagFilterBox.addItem("All Tags", tagFilterAll);
    for (int i = 0; i < tags.size(); ++i)
      tagFilterBox.addItem(tags[i], i + 2);

    int nextSelected = tagFilterAll;
    if (previousSelected > tagFilterAll && tags.contains(previousText))
      nextSelected = tags.indexOf(previousText) + 2;
    tagFilterBox.setSelectedId(nextSelected, juce::dontSendNotification);
  }

  void toggleFavorite() {
    if (selectedEntry == nullptr)
      return;

    const auto selectedEntryId = selectedEntry->entryId;
    const bool nowFavorite = catalog->toggleFavorite(selectedEntryId);
    statusLabel.setText(nowFavorite ? "Preset marked as favorite."
                                    : "Preset removed from favorites.",
                        juce::dontSendNotification);
    refreshEntries(true);
    rebuildVisibleEntries(selectedEntryId);
  }

  void saveTags() {
    if (selectedEntry == nullptr)
      return;

    juce::StringArray tags;
    tags.addTokens(tagsEditor.getText(), ",", {});
    tags.trim();
    tags.removeEmptyStrings();
    catalog->setTags(selectedEntry->entryId, tags);
    statusLabel.setText(tags.isEmpty() ? "Preset tags cleared."
                                       : "Preset tags saved.",
                        juce::dontSendNotification);
    refreshEntries(true);
    rebuildVisibleEntries(selectedEntry->entryId);
  }

  void clearRecent() {
    if (selectedEntry == nullptr)
      return;

    const auto selectedEntryId = selectedEntry->entryId;
    catalog->clearRecent(selectedEntryId);
    statusLabel.setText("Preset removed from recent list.",
                        juce::dontSendNotification);
    refreshEntries(true);
    rebuildVisibleEntries(selectedEntryId);
  }

  std::function<void()> layoutChangedCallback;
  PrimaryActionHandler primaryActionHandler;
  SecondaryActionHandler secondaryActionHandler;
  EntryPreviewHandler entryPreviewHandler;
  std::unique_ptr<TPresetCatalog> catalog;
  juce::String sessionPreviewSummary;
  juce::String sessionPreviewDetail;
  bool sessionPreviewWarning = false;
  juce::String pendingConflictEntryId;
  std::vector<VisibleEntry> visibleEntries;
  const TPresetEntry *selectedEntry = nullptr;
  bool browserOpen = false;

  juce::Label titleLabel;
  juce::ComboBox filterBox;
  juce::ComboBox tagFilterBox;
  juce::TextEditor searchEditor;
  juce::TextEditor tagsEditor;
  juce::TextButton refreshButton;
  juce::ListBox listBox;
  juce::Label kindValueLabel;
  juce::Label pathValueLabel;
  juce::Label summaryValueLabel;
  juce::Label conflictLabel;
  juce::Label statusLabel;
  juce::TextButton primaryActionButton;
  juce::TextButton secondaryActionButton;
  juce::TextButton clearRecentButton;
  juce::TextButton cancelConflictButton;
  juce::TextButton favoriteButton;
  juce::TextButton saveTagsButton;
  juce::TextButton revealButton;
  juce::TextEditor sessionPreviewEditor;
  juce::TextEditor selectionPreviewEditor;
  juce::TextEditor detailEditor;

  bool requiresConflictConfirmation(const TPresetEntry &entry) const {
    if (!sessionPreviewWarning)
      return false;
    return entry.presetKind == "teul.state" || entry.presetKind == "teul.recovery";
  }

  juce::String conflictDetailText(const TPresetEntry &entry) const {
    if (entry.presetKind == "teul.recovery")
      return "Current document has unsaved changes. Restoring the recovery snapshot will replace the current Teul graph.";
    if (entry.presetKind == "teul.state")
      return "Current document has unsaved changes. Applying this state preset will overwrite matching node values and bypass states.";
    return {};
  }

  void clearConflictArmIfSelectionChanged() {
    if (selectedEntry == nullptr || pendingConflictEntryId != selectedEntry->entryId)
      pendingConflictEntryId.clear();
  }

  void cancelConflictArm(const juce::String &statusText) {
    pendingConflictEntryId.clear();
    if (statusText.isNotEmpty())
      statusLabel.setText(statusText, juce::dontSendNotification);
    refreshDetailPanel();
  }
};

} // namespace

std::unique_ptr<PresetBrowserPanel> PresetBrowserPanel::create() {
  return std::make_unique<PresetBrowserPanelImpl>();
}

} // namespace Teul
