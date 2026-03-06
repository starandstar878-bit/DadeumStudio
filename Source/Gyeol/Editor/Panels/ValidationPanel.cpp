
#include "Gyeol/Editor/Panels/ValidationPanel.h"

#include "Gyeol/Core/SceneValidator.h"
#include "Gyeol/Editor/Theme/GyeolCustomLookAndFeel.h"

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace Gyeol::Ui::Panels {
namespace {
using Gyeol::GyeolPalette;

juce::Colour palette(GyeolPalette id, float alpha = 1.0f) {
  return Gyeol::getGyeolColor(id).withAlpha(alpha);
}

juce::Font makePanelFont(const juce::Component &component, float height,
                         bool bold) {
  if (auto *lf = dynamic_cast<const Gyeol::GyeolCustomLookAndFeel *>(
          &component.getLookAndFeel());
      lf != nullptr)
    return lf->makeFont(height, bold);

  auto fallback = juce::Font(juce::FontOptions(height));
  return bold ? fallback.boldened() : fallback;
}

juce::File resolveProjectRootDirectory() {
  auto projectRoot = juce::File::getCurrentWorkingDirectory();
  for (int depth = 0; depth < 10; ++depth) {
    if (projectRoot.getChildFile("DadeumStudio.jucer").existsAsFile())
      return projectRoot;

    const auto parent = projectRoot.getParentDirectory();
    if (parent == projectRoot)
      break;
    projectRoot = parent;
  }

  return juce::File::getCurrentWorkingDirectory();
}

juce::File resolveInputFilePath(const juce::String &value,
                                const juce::File &projectRoot) {
  if (juce::File::isAbsolutePath(value))
    return juce::File(value);
  return projectRoot.getChildFile(value);
}

juce::String inferMimeTypeFromFile(const juce::File &file) {
  const auto ext =
      file.getFileExtension().trimCharactersAtStart(".").toLowerCase();
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "bmp") return "image/bmp";
  if (ext == "gif") return "image/gif";
  if (ext == "svg") return "image/svg+xml";
  if (ext == "webp") return "image/webp";
  if (ext == "ttf") return "font/ttf";
  if (ext == "otf") return "font/otf";
  if (ext == "woff") return "font/woff";
  if (ext == "woff2") return "font/woff2";
  if (ext == "wav") return "audio/wav";
  if (ext == "aif" || ext == "aiff") return "audio/aiff";
  if (ext == "ogg") return "audio/ogg";
  if (ext == "flac") return "audio/flac";
  if (ext == "mp3") return "audio/mpeg";
  if (ext == "json") return "application/json";
  if (ext == "xml") return "application/xml";
  if (ext == "txt") return "text/plain";
  return {};
}

juce::String formatByteSize(juce::int64 bytes) {
  const auto positiveBytes = std::max<juce::int64>(bytes, 0);
  if (positiveBytes < 1024)
    return juce::String(positiveBytes) + " B";

  const auto asDouble = static_cast<double>(positiveBytes);
  if (positiveBytes < 1024 * 1024)
    return juce::String(asDouble / 1024.0, 1) + " KB";
  if (positiveBytes < 1024LL * 1024LL * 1024LL)
    return juce::String(asDouble / (1024.0 * 1024.0), 2) + " MB";
  return juce::String(asDouble / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
}

juce::String makeIssueUuid(const juce::String &category, WidgetId relatedId,
                           const juce::String &detail) {
  const auto seed = category + ":" + juce::String(relatedId) + ":" + detail;
  return juce::String::toHexString(static_cast<int64_t>(seed.hashCode64()));
}

juce::String widgetTypeLabel(WidgetType type) {
  switch (type) {
  case WidgetType::button: return "Button";
  case WidgetType::slider: return "Slider";
  case WidgetType::meter: return "Meter";
  case WidgetType::label: return "Label";
  case WidgetType::knob: return "Knob";
  case WidgetType::comboBox: return "ComboBox";
  case WidgetType::textInput: return "TextInput";
  }
  return "Widget";
}

void drawDependencyMap(juce::Graphics &g,
                       const ValidationPanel::DependencyGraph &graph,
                       juce::Rectangle<int> area,
                       const juce::Component &owner) {
  g.setColour(palette(GyeolPalette::HeaderBackground, 0.88f));
  g.fillRoundedRectangle(area.toFloat(), 4.0f);
  g.setColour(palette(GyeolPalette::BorderDefault, 0.9f));
  g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);

  if (graph.nodes.empty()) {
    g.setColour(palette(GyeolPalette::TextDisabled));
    g.setFont(makePanelFont(owner, 9.5f, false));
    g.drawText("No dependency links", area.reduced(8, 4),
               juce::Justification::centredLeft, true);
    return;
  }

  const auto count = juce::jmin(6, static_cast<int>(graph.nodes.size()));
  std::vector<juce::Rectangle<float>> nodes;
  nodes.reserve(static_cast<size_t>(count));

  const auto nodeWidth = juce::jlimit(56.0f, 86.0f,
      area.getWidth() / static_cast<float>(count + 1));
  const auto nodeHeight = 16.0f;
  const auto step = area.getWidth() / static_cast<float>(count + 1);

  for (int i = 0; i < count; ++i) {
    const auto cx = area.getX() + step * static_cast<float>(i + 1);
    nodes.emplace_back(0.0f, 0.0f, nodeWidth, nodeHeight);
    nodes.back().setCentre({cx, area.getCentreY() + 3.0f});
  }

  for (const auto &edge : graph.edges) {
    if (edge.first < 0 || edge.second < 0 || edge.first >= count || edge.second >= count)
      continue;

    const auto &fromNode = nodes[static_cast<size_t>(edge.first)];
    const auto &toNode = nodes[static_cast<size_t>(edge.second)];
    const auto from = juce::Point<float>(fromNode.getRight(), fromNode.getCentreY());
    const auto to = juce::Point<float>(toNode.getX(), toNode.getCentreY());
    g.setColour(palette(GyeolPalette::AccentPrimary, 0.78f));
    g.drawLine(from.x, from.y, to.x, to.y, 1.0f);
  }

  g.setFont(makePanelFont(owner, 8.8f, true));
  for (int i = 0; i < count; ++i) {
    const auto &node = nodes[static_cast<size_t>(i)];
    g.setColour(palette(GyeolPalette::ControlBase, 0.96f));
    g.fillRoundedRectangle(node, 3.0f);
    g.setColour(palette(GyeolPalette::BorderDefault));
    g.drawRoundedRectangle(node, 3.0f, 1.0f);
    g.setColour(palette(GyeolPalette::TextSecondary));
    g.drawFittedText(graph.nodes[static_cast<size_t>(i)], node.toNearestInt().reduced(3, 2),
                     juce::Justification::centred, 1);
  }
}
} // namespace

ValidationPanel::ValidationPanel(DocumentHandle &documentIn,
                                 const Widgets::WidgetRegistry &registryIn)
    : document(documentIn), registry(registryIn) {
  titleLabel.setText("Validation", juce::dontSendNotification);
  titleLabel.setFont(makePanelFont(*this, 12.0f, true));
  titleLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextPrimary));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(titleLabel);

  summaryLabel.setText("Stale", juce::dontSendNotification);
  summaryLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextSecondary));
  summaryLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(summaryLabel);

  autoRefreshToggle.setClickingTogglesState(true);
  autoRefreshToggle.setToggleState(autoRefresh, juce::dontSendNotification);
  autoRefreshToggle.onClick = [this] { setAutoRefreshEnabled(autoRefreshToggle.getToggleState()); };
  addAndMakeVisible(autoRefreshToggle);

  runButton.onClick = [this] { refreshValidation(); };
  addAndMakeVisible(runButton);

  auto setupFilterBtn = [this](juce::TextButton &btn, bool &flag, IssueSeverity severity) {
    btn.setClickingTogglesState(true);
    btn.setToggleState(true, juce::dontSendNotification);
    btn.onClick = [this, &btn, &flag] {
      flag = btn.getToggleState();
      rebuildFilteredIssues();
      listBox.updateContent();
      listBox.repaint();
      updateSummaryText();
    };

    const auto accent = colorForSeverity(severity);
    btn.setColour(juce::TextButton::buttonColourId, palette(GyeolPalette::ControlBase));
    btn.setColour(juce::TextButton::buttonOnColourId, accent.withAlpha(0.24f));
    btn.setColour(juce::TextButton::textColourOffId, palette(GyeolPalette::TextSecondary));
    btn.setColour(juce::TextButton::textColourOnId, accent);
    addAndMakeVisible(btn);
  };

  setupFilterBtn(filterErrorBtn, showErrors, IssueSeverity::error);
  setupFilterBtn(filterWarningBtn, showWarnings, IssueSeverity::warning);
  setupFilterBtn(filterInfoBtn, showInfo, IssueSeverity::info);

  listBox.setModel(this);
  listBox.setRowHeight(84);
  listBox.setMultipleSelectionEnabled(false);
  listBox.setColour(juce::ListBox::backgroundColourId, palette(GyeolPalette::CanvasBackground));
  listBox.setColour(juce::ListBox::outlineColourId, palette(GyeolPalette::BorderDefault));
  listBox.addMouseListener(this, true);
  addAndMakeVisible(listBox);

  startTimer(pulseTimerId, 70);
  lookAndFeelChanged();
}

ValidationPanel::~ValidationPanel() {
  stopTimer(debounceTimerId);
  stopTimer(pulseTimerId);
  listBox.removeMouseListener(this);
  listBox.setModel(nullptr);
}

void ValidationPanel::lookAndFeelChanged() {
  titleLabel.setFont(makePanelFont(*this, 12.0f, true));
  titleLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextPrimary));
  summaryLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextSecondary));

  updateFilterChipLabels();
  repaint();
}

void ValidationPanel::setSelectWidgetCallback(std::function<void(WidgetId)> callback) {
  onSelectWidget = std::move(callback);
}

void ValidationPanel::setHoverWidgetCallback(std::function<void(WidgetId)> callback) {
  onHoverWidget = std::move(callback);
}

void ValidationPanel::markDirty() {
  dirty = true;
  if (autoRefresh)
    queueValidation(false);
  else
    summaryLabel.setText("Stale (Run required)", juce::dontSendNotification);
}

void ValidationPanel::refreshValidation() {
  dirty = true;
  launchValidationJob();
}

bool ValidationPanel::autoRefreshEnabled() const noexcept { return autoRefresh; }

void ValidationPanel::setAutoRefreshEnabled(bool enabled) {
  autoRefresh = enabled;
  autoRefreshToggle.setToggleState(enabled, juce::dontSendNotification);
  if (autoRefresh && dirty)
    queueValidation(true);
}

void ValidationPanel::setExternalIssues(std::vector<ExternalIssue> issues) {
  externalIssues = std::move(issues);
  markDirty();
}

void ValidationPanel::paint(juce::Graphics &g) {
  g.fillAll(palette(GyeolPalette::PanelBackground));
  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawRect(getLocalBounds(), 1);
}

void ValidationPanel::resized() {
  auto area = getLocalBounds().reduced(8);

  auto top = area.removeFromTop(20);
  titleLabel.setBounds(top.removeFromLeft(120));
  summaryLabel.setBounds(top);

  area.removeFromTop(4);
  auto controls = area.removeFromTop(24);
  runButton.setBounds(controls.removeFromLeft(84));
  controls.removeFromLeft(6);
  autoRefreshToggle.setBounds(controls.removeFromLeft(58));

  area.removeFromTop(4);
  auto chips = area.removeFromTop(24);
  const auto chipWidth = (chips.getWidth() - 8) / 3;
  filterErrorBtn.setBounds(chips.removeFromLeft(chipWidth));
  chips.removeFromLeft(4);
  filterWarningBtn.setBounds(chips.removeFromLeft(chipWidth));
  chips.removeFromLeft(4);
  filterInfoBtn.setBounds(chips);

  area.removeFromTop(6);
  listBox.setBounds(area);
}

int ValidationPanel::getNumRows() { return static_cast<int>(filteredRows.size()); }

void ValidationPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                       int width, int height,
                                       bool rowIsSelected) {
  if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredRows.size()))
    return;

  const auto &issue = allIssues[static_cast<size_t>(filteredRows[rowNumber])];
  const auto expanded = expandedIssueUuids.count(issue.uuid) > 0;
  const auto hovered = rowNumber == hoveredRow;

  if (const auto *lf = dynamic_cast<const Gyeol::GyeolCustomLookAndFeel *>(
          &getLookAndFeel());
      lf != nullptr) {
    lf->drawValidationItem(g,
                           {0, 0, width, height},
                           labelForSeverity(issue.severity),
                           colorForSeverity(issue.severity),
                           issue.title,
                           issue.message,
                           rowIsSelected,
                           expanded,
                           hovered,
                           pulsePhase,
                           issue.quickFixAvailable,
                           mutedIssueUuids.count(issue.uuid) > 0);
  } else {
    g.setColour(rowIsSelected ? palette(GyeolPalette::SelectionBackground)
                              : palette(GyeolPalette::ControlBase));
    g.fillRect(0, 0, width, height);
  }

  g.setColour(palette(GyeolPalette::TextSecondary, 0.95f));
  g.setFont(makePanelFont(*this, 10.0f, true));
  g.drawText(expanded ? "v" : ">",
             juce::Rectangle<int>(6, 7, 12, 12),
             juce::Justification::centred,
             false);

  if (expanded)
    drawDependencyMap(g, issue.dependency, {8, height - 28, width - 16, 22}, *this);
}

void ValidationPanel::listBoxItemClicked(int row, const juce::MouseEvent &event) {
  if (row < 0 || row >= static_cast<int>(filteredRows.size()))
    return;

  const auto issueIndex = filteredRows[static_cast<size_t>(row)];
  if (issueIndex < 0 || issueIndex >= static_cast<int>(allIssues.size()))
    return;

  const auto &issue = allIssues[static_cast<size_t>(issueIndex)];
  const auto rowBounds = listBox.getRowPosition(row, false);
  const auto point = event.getEventRelativeTo(&listBox).position;
  const auto localX = point.x - static_cast<float>(rowBounds.getX());
  const auto localY = point.y - static_cast<float>(rowBounds.getY());
  const auto rowWidth = static_cast<float>(rowBounds.getWidth());

  if (localX >= 6.0f && localX <= 20.0f && localY >= 6.0f && localY <= 20.0f) {
    toggleIssueExpanded(issue.uuid);
    return;
  }

  if (issue.quickFixAvailable && localX >= rowWidth - 74.0f &&
      localX <= rowWidth - 44.0f && localY >= 6.0f && localY <= 20.0f) {
    performQuickFixForIssue(issue);
    return;
  }

  if (localX >= rowWidth - 40.0f && localX <= rowWidth - 8.0f &&
      localY >= 6.0f && localY <= 20.0f) {
    toggleIssueMuted(issue.uuid);
    return;
  }
}

void ValidationPanel::listBoxItemDoubleClicked(int row,
                                               const juce::MouseEvent &) {
  if (row < 0 || row >= static_cast<int>(filteredRows.size()))
    return;

  const auto &issue = allIssues[static_cast<size_t>(filteredRows[row])];
  if (issue.relatedWidgetId > kRootId && onSelectWidget)
    onSelectWidget(issue.relatedWidgetId);
}

void ValidationPanel::selectedRowsChanged(int lastRowSelected) {
  juce::ignoreUnused(lastRowSelected);
}

void ValidationPanel::timerCallback(int timerId) {
  if (timerId == debounceTimerId) {
    stopTimer(debounceTimerId);
    launchValidationJob();
    return;
  }

  if (timerId == pulseTimerId) {
    pulsePhase += 0.08f;
    if (pulsePhase > 1.0f)
      pulsePhase -= 1.0f;

    if (!filteredRows.empty())
      listBox.repaint();
  }
}

void ValidationPanel::mouseMove(const juce::MouseEvent &event) {
  const auto local = event.getEventRelativeTo(&listBox).position;
  if (!listBox.getLocalBounds().toFloat().contains(local)) {
    if (hoveredRow != -1) {
      hoveredRow = -1;
      listBox.repaint();
      emitHoverForRow(-1);
    }
    return;
  }

  const auto row = listBox.getRowContainingPosition(juce::roundToInt(local.x),
                                                     juce::roundToInt(local.y));
  if (row == hoveredRow)
    return;

  hoveredRow = row;
  listBox.repaint();
  emitHoverForRow(hoveredRow);
}

void ValidationPanel::mouseExit(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);
  if (hoveredRow == -1)
    return;

  hoveredRow = -1;
  listBox.repaint();
  emitHoverForRow(-1);
}

void ValidationPanel::queueValidation(bool immediate) {
  if (!autoRefresh)
    return;

  startTimer(debounceTimerId, immediate ? 12 : 500);
  summaryLabel.setText("Queued...", juce::dontSendNotification);
}

void ValidationPanel::launchValidationJob() {
  if (validationJobRunning.exchange(true)) {
    validationRerunQueued.store(true);
    return;
  }

  const auto requestSerial = validationSerialCounter.fetch_add(1);
  const auto snapshot = document.snapshot();
  const auto editorState = document.editorState();
  const auto projectRoot = resolveProjectRootDirectory();
  const auto externalIssueSnapshot = externalIssues;

  dirty = false;
  summaryLabel.setText("Scanning...", juce::dontSendNotification);

  juce::Component::SafePointer<ValidationPanel> safeThis(this);
  std::thread worker([safeThis, requestSerial, snapshot, editorState,
                      projectRoot, externalIssueSnapshot,
                      &registry = this->registry]() mutable {
    auto issues = ValidationPanel::buildIssues(snapshot, editorState, registry,
                                               projectRoot, externalIssueSnapshot);

    juce::MessageManager::callAsync(
        [safeThis, requestSerial, issues = std::move(issues)]() mutable {
          if (safeThis == nullptr)
            return;
          safeThis->applyValidationResults(requestSerial, std::move(issues));
        });
  });
  worker.detach();
}

void ValidationPanel::applyValidationResults(int requestSerial,
                                             std::vector<Issue> issues) {
  if (requestSerial >= lastAppliedValidationSerial) {
    lastAppliedValidationSerial = requestSerial;
    allIssues = std::move(issues);
    rebuildFilteredIssues();
    updateFilterChipLabels();
    updateSummaryText();

    listBox.updateContent();
    listBox.repaint();
    repaint();
  }

  validationJobRunning.store(false);
  if (validationRerunQueued.exchange(false))
    launchValidationJob();
}

void ValidationPanel::rebuildFilteredIssues() {
  filteredRows.clear();
  filteredRows.reserve(allIssues.size());

  for (int i = 0; i < static_cast<int>(allIssues.size()); ++i) {
    if (isIssueVisible(allIssues[static_cast<size_t>(i)]))
      filteredRows.push_back(i);
  }

  if (hoveredRow >= static_cast<int>(filteredRows.size()))
    hoveredRow = -1;
}

bool ValidationPanel::isIssueVisible(const Issue &issue) const {
  if (mutedIssueUuids.count(issue.uuid) > 0)
    return false;

  switch (issue.severity) {
  case IssueSeverity::error: return showErrors;
  case IssueSeverity::warning: return showWarnings;
  case IssueSeverity::info: return showInfo;
  }
  return true;
}

void ValidationPanel::updateFilterChipLabels() {
  int errorCount = 0;
  int warningCount = 0;
  int infoCount = 0;

  for (const auto &issue : allIssues) {
    if (mutedIssueUuids.count(issue.uuid) > 0)
      continue;

    switch (issue.severity) {
    case IssueSeverity::error: ++errorCount; break;
    case IssueSeverity::warning: ++warningCount; break;
    case IssueSeverity::info: ++infoCount; break;
    }
  }

  filterErrorBtn.setButtonText("Errors: " + juce::String(errorCount));
  filterWarningBtn.setButtonText("Warnings: " + juce::String(warningCount));
  filterInfoBtn.setButtonText("Infos: " + juce::String(infoCount));
}

void ValidationPanel::updateSummaryText() {
  int errorCount = 0;
  int warningCount = 0;
  int infoCount = 0;

  for (const auto row : filteredRows) {
    if (row < 0 || row >= static_cast<int>(allIssues.size()))
      continue;

    const auto &issue = allIssues[static_cast<size_t>(row)];
    switch (issue.severity) {
    case IssueSeverity::error: ++errorCount; break;
    case IssueSeverity::warning: ++warningCount; break;
    case IssueSeverity::info: ++infoCount; break;
    }
  }

  summaryLabel.setText("E " + juce::String(errorCount) + "  W " +
                           juce::String(warningCount) + "  I " +
                           juce::String(infoCount),
                       juce::dontSendNotification);
}

void ValidationPanel::emitHoverForRow(int row) {
  if (!onHoverWidget)
    return;

  if (row < 0 || row >= static_cast<int>(filteredRows.size())) {
    onHoverWidget(kRootId);
    return;
  }

  const auto idx = filteredRows[static_cast<size_t>(row)];
  if (idx < 0 || idx >= static_cast<int>(allIssues.size())) {
    onHoverWidget(kRootId);
    return;
  }

  const auto relatedId = allIssues[static_cast<size_t>(idx)].relatedWidgetId;
  onHoverWidget(relatedId > kRootId ? relatedId : kRootId);
}

void ValidationPanel::toggleIssueExpanded(const juce::String &issueUuid) {
  if (expandedIssueUuids.count(issueUuid) > 0)
    expandedIssueUuids.erase(issueUuid);
  else
    expandedIssueUuids.insert(issueUuid);

  listBox.repaint();
}

void ValidationPanel::toggleIssueMuted(const juce::String &issueUuid) {
  if (mutedIssueUuids.count(issueUuid) > 0)
    mutedIssueUuids.erase(issueUuid);
  else
    mutedIssueUuids.insert(issueUuid);

  rebuildFilteredIssues();
  updateFilterChipLabels();
  updateSummaryText();
  listBox.updateContent();
  listBox.repaint();
}

bool ValidationPanel::performQuickFixForIssue(const Issue &issue) {
  if (!issue.quickFixAvailable || issue.relatedAssetId <= kRootId)
    return false;

  juce::String wildcard;
  switch (issue.relatedAssetKind) {
  case AssetKind::image: wildcard = "*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.svg;*.webp"; break;
  case AssetKind::font: wildcard = "*.ttf;*.otf;*.woff;*.woff2"; break;
  default: wildcard = "*"; break;
  }

  constexpr int chooserFlags = juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles;

  quickFixChooser = std::make_unique<juce::FileChooser>("Choose replacement asset", juce::File(), wildcard);
  juce::Component::SafePointer<ValidationPanel> safeThis(this);
  const auto issueCopy = issue;

  quickFixChooser->launchAsync(chooserFlags,
                               [safeThis, issueCopy](const juce::FileChooser &chooser) {
    if (safeThis == nullptr)
      return;

    safeThis->quickFixChooser.reset();

    const auto selectedFile = chooser.getResult();
    if (!selectedFile.existsAsFile())
      return;

    auto assets = safeThis->document.snapshot().assets;
    const auto it = std::find_if(assets.begin(), assets.end(),
        [assetId = issueCopy.relatedAssetId](const AssetModel &asset) {
          return asset.id == assetId;
        });
    if (it == assets.end())
      return;

    const auto projectRoot = resolveProjectRootDirectory();
    auto relativePath = selectedFile.getRelativePathFrom(projectRoot);
    if (relativePath.startsWith(".."))
      relativePath = selectedFile.getFullPathName();

    it->relativePath = relativePath.replaceCharacter('\\', '/');

    const auto inferredMime = inferMimeTypeFromFile(selectedFile);
    if (inferredMime.isNotEmpty())
      it->mimeType = inferredMime;

    if (safeThis->document.setAssets(std::move(assets))) {
      safeThis->markDirty();
      safeThis->refreshValidation();
    }
  });

  return true;
}
std::vector<ValidationPanel::Issue>
ValidationPanel::buildIssues(const DocumentModel &snapshot,
                             const EditorStateModel &editorState,
                             const Widgets::WidgetRegistry &registry,
                             const juce::File &projectRoot,
                             const std::vector<ExternalIssue> &externalIssues) {
  std::vector<Issue> issues;

  std::unordered_map<WidgetId, const WidgetModel *> widgetById;
  widgetById.reserve(snapshot.widgets.size());
  for (const auto &widget : snapshot.widgets)
    widgetById.emplace(widget.id, &widget);

  const auto widgetName = [&widgetById](WidgetId widgetId) {
    if (const auto it = widgetById.find(widgetId);
        it != widgetById.end() && it->second != nullptr) {
      return widgetTypeLabel(it->second->type) + " #" + juce::String(widgetId);
    }
    return juce::String("Widget #") + juce::String(widgetId);
  };

  std::unordered_map<WidgetId, DependencyGraph> dependencyCache;
  const auto graphForWidget = [&](WidgetId widgetId) -> const DependencyGraph & {
    if (const auto it = dependencyCache.find(widgetId); it != dependencyCache.end())
      return it->second;

    DependencyGraph graph;
    std::unordered_map<juce::String, int> nodeIndex;

    const auto addNode = [&](const juce::String &label) {
      const auto key = label.trim();
      if (const auto it = nodeIndex.find(key); it != nodeIndex.end())
        return it->second;
      const auto idx = static_cast<int>(graph.nodes.size());
      graph.nodes.push_back(key);
      nodeIndex.emplace(key, idx);
      return idx;
    };

    const auto addEdge = [&](int from, int to) {
      if (from < 0 || to < 0 || from == to)
        return;
      const auto duplicated = std::any_of(graph.edges.begin(), graph.edges.end(),
          [from, to](const std::pair<int, int> &edge) { return edge.first == from && edge.second == to; });
      if (!duplicated)
        graph.edges.emplace_back(from, to);
    };

    for (const auto &binding : snapshot.runtimeBindings) {
      auto touches = binding.sourceWidgetId == widgetId;
      if (!touches) {
        for (const auto &action : binding.actions) {
          if ((action.target.kind == NodeKind::widget && action.target.id == widgetId) ||
              action.targetWidgetId == widgetId) {
            touches = true;
            break;
          }
        }
      }

      if (!touches)
        continue;

      const auto sourceIdx = addNode(widgetName(binding.sourceWidgetId));
      const auto bindingIdx =
          addNode("Binding #" + juce::String(binding.id) + " (" + binding.eventKey + ")");
      addEdge(sourceIdx, bindingIdx);

      for (const auto &action : binding.actions) {
        if (action.target.kind == NodeKind::widget && action.target.id > kRootId) {
          addEdge(bindingIdx, addNode(widgetName(action.target.id)));
        } else if (action.targetWidgetId > kRootId) {
          addEdge(bindingIdx, addNode(widgetName(action.targetWidgetId)));
        } else if (action.paramKey.trim().isNotEmpty()) {
          addEdge(bindingIdx, addNode("Param: " + action.paramKey.trim()));
        }
      }
    }

    for (const auto &binding : snapshot.propertyBindings) {
      if (binding.targetWidgetId != widgetId)
        continue;

      const auto expr = binding.expression.trim();
      const auto exprNode = addNode("Expr: " +
                                    (expr.length() > 20
                                         ? expr.substring(0, 20) + "..."
                                         : expr));
      addEdge(exprNode, addNode(widgetName(binding.targetWidgetId)));
    }

    return dependencyCache.emplace(widgetId, std::move(graph)).first->second;
  };

  const auto pushIssue = [&](IssueSeverity severity, const juce::String &category,
                             const juce::String &title,
                             const juce::String &message,
                             WidgetId relatedWidgetId = kRootId,
                             bool quickFixAvailable = false,
                             WidgetId relatedAssetId = kRootId,
                             AssetKind relatedAssetKind = AssetKind::file,
                             const juce::String &missingAssetPath = {}) {
    Issue issue;
    issue.uuid = makeIssueUuid(category, relatedWidgetId,
                               (title + ":" + message).trim().toLowerCase());
    issue.severity = severity;
    issue.title = title;
    issue.message = message;
    issue.relatedWidgetId = relatedWidgetId;
    issue.quickFixAvailable = quickFixAvailable;
    issue.relatedAssetId = relatedAssetId;
    issue.relatedAssetKind = relatedAssetKind;
    issue.missingAssetPath = missingAssetPath;

    if (relatedWidgetId > kRootId)
      issue.dependency = graphForWidget(relatedWidgetId);

    issues.push_back(std::move(issue));
  };

  const auto sceneResult = Core::SceneValidator::validateScene(snapshot, &editorState);
  if (sceneResult.failed()) {
    pushIssue(IssueSeverity::error,
              "scene-invalid",
              "Scene validation failed",
              sceneResult.getErrorMessage());
  } else {
    pushIssue(IssueSeverity::info,
              "scene-valid",
              "Scene validation passed",
              "Core model invariants are valid.");
  }

  if (snapshot.widgets.empty()) {
    pushIssue(IssueSeverity::warning,
              "empty-document",
              "Empty document",
              "No widgets in current document.");
  }

  int hiddenLayerCount = 0;
  int lockedLayerCount = 0;
  for (const auto &layer : snapshot.layers) {
    hiddenLayerCount += layer.visible ? 0 : 1;
    lockedLayerCount += layer.locked ? 1 : 0;

    if (layer.memberWidgetIds.empty() && layer.memberGroupIds.empty()) {
      pushIssue(IssueSeverity::info,
                "empty-layer",
                "Empty layer: " + layer.name,
                "Layer has no widget/group members.");
    }
  }

  if (!snapshot.layers.empty() &&
      hiddenLayerCount == static_cast<int>(snapshot.layers.size())) {
    pushIssue(IssueSeverity::warning,
              "all-hidden",
              "All layers hidden",
              "Canvas may render as empty.");
  }

  if (!snapshot.layers.empty() &&
      lockedLayerCount == static_cast<int>(snapshot.layers.size())) {
    pushIssue(IssueSeverity::warning,
              "all-locked",
              "All layers locked",
              "Editing interaction will be blocked.");
  }

  std::unordered_set<juce::String> seenLayerNames;
  for (const auto &layer : snapshot.layers) {
    const auto normalized = layer.name.trim().toLowerCase();
    if (normalized.isEmpty())
      continue;

    if (!seenLayerNames.insert(normalized).second) {
      pushIssue(IssueSeverity::warning,
                "duplicate-layer",
                "Duplicate layer name",
                "Layer name '" + layer.name + "' is duplicated.");
    }
  }

  for (const auto &widget : snapshot.widgets) {
    if (registry.findForWidget(widget) == nullptr) {
      pushIssue(IssueSeverity::warning,
                "unknown-widget",
                "Unknown widget descriptor",
                "Widget id=" + juce::String(widget.id) +
                    " has no descriptor in registry.",
                widget.id);
    }
  }

  const auto runtimeIssues = Core::SceneValidator::validateRuntimeBindings(snapshot);
  for (const auto &runtimeIssue : runtimeIssues) {
    const auto severity = runtimeIssue.severity ==
                                  Core::SceneValidator::RuntimeBindingIssueSeverity::error
                              ? IssueSeverity::error
                              : IssueSeverity::warning;
    pushIssue(severity,
              "runtime-binding",
              runtimeIssue.title,
              runtimeIssue.message);
  }

  for (const auto &binding : snapshot.runtimeBindings) {
    const auto widgetIt = widgetById.find(binding.sourceWidgetId);
    if (widgetIt == widgetById.end())
      continue;

    const auto *descriptor = registry.findForWidget(*widgetIt->second);
    if (descriptor == nullptr)
      continue;

    const auto supported = std::any_of(
        descriptor->runtimeEvents.begin(), descriptor->runtimeEvents.end(),
        [&binding](const Widgets::RuntimeEventSpec &eventSpec) {
          return eventSpec.key == binding.eventKey;
        });
    if (!supported) {
      pushIssue(IssueSeverity::warning,
                "unsupported-event",
                "Unsupported event key",
                "Binding id=" + juce::String(binding.id) + " event '" +
                    binding.eventKey +
                    "' is not supported by widget type '" +
                    descriptor->typeKey + "'.",
                binding.sourceWidgetId);
    }
  }

  constexpr juce::int64 kLargeAssetWarnBytes = 5 * 1024 * 1024;
  constexpr int kLargeImageDimension = 4096;

  for (const auto &asset : snapshot.assets) {
    const auto assetLabel = asset.name.trim().isNotEmpty() ? asset.name.trim()
                                                            : asset.refKey.trim();
    const auto displayLabel = assetLabel.isNotEmpty()
                                  ? assetLabel
                                  : ("Asset #" + juce::String(asset.id));

    if (asset.kind == AssetKind::colorPreset)
      continue;

    const auto normalizedPath = asset.relativePath.trim();
    if (normalizedPath.isEmpty()) {
      pushIssue(IssueSeverity::warning,
                "asset-path-empty",
                "Asset path missing",
                displayLabel + " has an empty relative path.",
                kRootId,
                asset.kind == AssetKind::image || asset.kind == AssetKind::font,
                asset.id,
                asset.kind,
                normalizedPath);
      continue;
    }

    const auto sourceFile = resolveInputFilePath(normalizedPath, projectRoot);
    if (!sourceFile.existsAsFile()) {
      pushIssue(IssueSeverity::warning,
                "asset-file-missing",
                "Asset file missing",
                displayLabel + " path not found: " + normalizedPath,
                kRootId,
                asset.kind == AssetKind::image || asset.kind == AssetKind::font,
                asset.id,
                asset.kind,
                normalizedPath);
      continue;
    }

    const auto fileSize = sourceFile.getSize();
    if (fileSize > kLargeAssetWarnBytes) {
      pushIssue(IssueSeverity::warning,
                "asset-large",
                "Large asset file",
                displayLabel + " is " + formatByteSize(fileSize) + " (" +
                    sourceFile.getFileName() + ").");
    }

    if (asset.kind == AssetKind::image) {
      const auto image = juce::ImageFileFormat::loadFrom(sourceFile);
      if (!image.isValid()) {
        pushIssue(IssueSeverity::warning,
                  "asset-image-decode",
                  "Image decode failed",
                  displayLabel + " could not be decoded as image (" +
                      sourceFile.getFileName() + ").");
      } else if (image.getWidth() > kLargeImageDimension ||
                 image.getHeight() > kLargeImageDimension) {
        pushIssue(IssueSeverity::warning,
                  "asset-large-image",
                  "Large image resolution",
                  displayLabel + " resolution is " +
                      juce::String(image.getWidth()) + "x" +
                      juce::String(image.getHeight()) + " (>" +
                      juce::String(kLargeImageDimension) + ").");
      }
    }

    const auto expectedMime = inferMimeTypeFromFile(sourceFile);
    const auto recordedMime = asset.mimeType.trim().toLowerCase();
    if (expectedMime.isNotEmpty() && recordedMime.isNotEmpty() &&
        !expectedMime.equalsIgnoreCase(recordedMime)) {
      pushIssue(IssueSeverity::warning,
                "asset-mime-mismatch",
                "Asset MIME mismatch",
                displayLabel + " MIME is '" + asset.mimeType +
                    "', expected '" + expectedMime + "'.");
    }
  }

  for (const auto &externalIssue : externalIssues) {
    const auto category = externalIssue.category.trim().isNotEmpty()
                              ? externalIssue.category.trim()
                              : juce::String("external-plugin");
    const auto title = externalIssue.title.trim().isNotEmpty()
                           ? externalIssue.title.trim()
                           : juce::String("External plugin issue");
    const auto message = externalIssue.message.trim().isNotEmpty()
                             ? externalIssue.message.trim()
                             : juce::String("Plugin load failure detected.");
    pushIssue(externalIssue.severity,
              category,
              title,
              message,
              externalIssue.relatedWidgetId);
  }

  return issues;
}

juce::Colour ValidationPanel::colorForSeverity(IssueSeverity severity) {
  switch (severity) {
  case IssueSeverity::info:
    return palette(GyeolPalette::AccentPrimary);
  case IssueSeverity::warning:
    return palette(GyeolPalette::ValidWarning);
  case IssueSeverity::error:
    return palette(GyeolPalette::ValidError);
  }

  return palette(GyeolPalette::TextDisabled);
}

juce::String ValidationPanel::labelForSeverity(IssueSeverity severity) {
  switch (severity) {
  case IssueSeverity::info:
    return "INFO";
  case IssueSeverity::warning:
    return "WARN";
  case IssueSeverity::error:
    return "ERROR";
  }

  return "INFO";
}
} // namespace Gyeol::Ui::Panels
