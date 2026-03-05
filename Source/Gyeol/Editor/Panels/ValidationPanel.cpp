#include "Gyeol/Editor/Panels/ValidationPanel.h"

#include "Gyeol/Core/SceneValidator.h"
#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"
#include <algorithm>
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
  if (ext == "png")
    return "image/png";
  if (ext == "jpg" || ext == "jpeg")
    return "image/jpeg";
  if (ext == "bmp")
    return "image/bmp";
  if (ext == "gif")
    return "image/gif";
  if (ext == "svg")
    return "image/svg+xml";
  if (ext == "webp")
    return "image/webp";
  if (ext == "ttf")
    return "font/ttf";
  if (ext == "otf")
    return "font/otf";
  if (ext == "woff")
    return "font/woff";
  if (ext == "woff2")
    return "font/woff2";
  if (ext == "wav")
    return "audio/wav";
  if (ext == "aif" || ext == "aiff")
    return "audio/aiff";
  if (ext == "ogg")
    return "audio/ogg";
  if (ext == "flac")
    return "audio/flac";
  if (ext == "mp3")
    return "audio/mpeg";
  if (ext == "json")
    return "application/json";
  if (ext == "xml")
    return "application/xml";
  if (ext == "txt")
    return "text/plain";
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

juce::String makeUtf8String(const char *utf8Text) {
  return juce::String(juce::CharPointer_UTF8(utf8Text));
}

// 필터 버튼 색상 헬퍼
juce::Colour filterButtonActiveColour(ValidationPanel::IssueSeverity s) {
  switch (s) {
  case ValidationPanel::IssueSeverity::error:
    return palette(GyeolPalette::ValidError);
  case ValidationPanel::IssueSeverity::warning:
    return palette(GyeolPalette::ValidWarning);
  case ValidationPanel::IssueSeverity::info:
    return palette(GyeolPalette::AccentPrimary);
  }
  return palette(GyeolPalette::TextSecondary);
}
} // namespace

// ==============================================================================
// 생성자 / 소멸자
// ==============================================================================
ValidationPanel::ValidationPanel(DocumentHandle &documentIn,
                                 const Widgets::WidgetRegistry &registryIn)
    : document(documentIn), registry(registryIn) {
  titleLabel.setText("Validation", juce::dontSendNotification);
  titleLabel.setFont(makePanelFont(*this, 12.0f, true));
  titleLabel.setColour(juce::Label::textColourId,
                       palette(GyeolPalette::TextPrimary));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(titleLabel);

  summaryLabel.setText("Stale", juce::dontSendNotification);
  summaryLabel.setColour(juce::Label::textColourId,
                         palette(GyeolPalette::TextSecondary));
  summaryLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(summaryLabel);

  autoRefreshToggle.setClickingTogglesState(true);
  autoRefreshToggle.setToggleState(autoRefresh, juce::dontSendNotification);
  autoRefreshToggle.onClick = [this] {
    setAutoRefreshEnabled(autoRefreshToggle.getToggleState());
  };
  addAndMakeVisible(autoRefreshToggle);

  runButton.onClick = [this] { refreshValidation(); };
  addAndMakeVisible(runButton);

  // ------------------------------------------------------------------
  // 카테고리 필터 버튼 3개 (E / W / I)
  // ------------------------------------------------------------------
  auto setupFilterBtn = [this](juce::TextButton &btn, IssueSeverity severity,
                               bool &flag) {
    btn.setClickingTogglesState(true);
    btn.setToggleState(true, juce::dontSendNotification);
    const auto activeCol = filterButtonActiveColour(severity);
    btn.setColour(juce::TextButton::buttonOnColourId,
                  activeCol.withAlpha(0.30f));
    btn.setColour(juce::TextButton::buttonColourId,
                  palette(GyeolPalette::ControlBase));
    btn.setColour(juce::TextButton::textColourOnId, activeCol);
    btn.setColour(juce::TextButton::textColourOffId,
                  palette(GyeolPalette::TextDisabled));
    btn.onClick = [this, &flag, &btn] {
      flag = btn.getToggleState();
      rebuildFilteredIssues();
      listBox.updateContent();
      listBox.repaint();
    };
    addAndMakeVisible(btn);
  };

  setupFilterBtn(filterErrorBtn, IssueSeverity::error, showErrors);
  setupFilterBtn(filterWarningBtn, IssueSeverity::warning, showWarnings);
  setupFilterBtn(filterInfoBtn, IssueSeverity::info, showInfo);

  listBox.setModel(this);
  listBox.setRowHeight(42);
  listBox.setColour(juce::ListBox::backgroundColourId,
                    palette(GyeolPalette::CanvasBackground));
  listBox.setColour(juce::ListBox::outlineColourId,
                    palette(GyeolPalette::BorderDefault));
  addAndMakeVisible(listBox);

  lookAndFeelChanged();
}

ValidationPanel::~ValidationPanel() { listBox.setModel(nullptr); }

// ==============================================================================
// 공개 메서드
// ==============================================================================

void ValidationPanel::lookAndFeelChanged() {
  titleLabel.setFont(makePanelFont(*this, 12.0f, true));
  titleLabel.setColour(juce::Label::textColourId,
                       palette(GyeolPalette::TextPrimary));
  summaryLabel.setColour(juce::Label::textColourId,
                         palette(GyeolPalette::TextSecondary));

  const auto applyFilterPalette = [this](juce::TextButton &btn,
                                         IssueSeverity severity) {
    const auto activeCol = filterButtonActiveColour(severity);
    btn.setColour(juce::TextButton::buttonOnColourId,
                  activeCol.withAlpha(0.30f));
    btn.setColour(juce::TextButton::buttonColourId,
                  palette(GyeolPalette::ControlBase));
    btn.setColour(juce::TextButton::textColourOnId, activeCol);
    btn.setColour(juce::TextButton::textColourOffId,
                  palette(GyeolPalette::TextDisabled));
  };

  applyFilterPalette(filterErrorBtn, IssueSeverity::error);
  applyFilterPalette(filterWarningBtn, IssueSeverity::warning);
  applyFilterPalette(filterInfoBtn, IssueSeverity::info);

  listBox.setColour(juce::ListBox::backgroundColourId,
                    palette(GyeolPalette::CanvasBackground));
  listBox.setColour(juce::ListBox::outlineColourId,
                    palette(GyeolPalette::BorderDefault));

  listBox.repaint();
  repaint();
}
void ValidationPanel::setSelectWidgetCallback(
    std::function<void(WidgetId)> callback) {
  onSelectWidget = std::move(callback);
}

void ValidationPanel::markDirty() {
  dirty = true;
  if (autoRefresh)
    refreshValidation();
  else
    summaryLabel.setText("Stale (Run required)", juce::dontSendNotification);
}

void ValidationPanel::refreshValidation() {
  rebuildIssues();
  rebuildFilteredIssues();
  dirty = false;

  int errorCount = 0;
  int warningCount = 0;
  for (const auto &issue : allIssues) {
    if (issue.severity == IssueSeverity::error)
      ++errorCount;
    else if (issue.severity == IssueSeverity::warning)
      ++warningCount;
  }

  if (errorCount > 0) {
    const auto errorText = makeUtf8String("\xEC\x98\xA4\xEB\xA5\x98"); // "오류"
    const auto warningText = makeUtf8String("\xEA\xB2\xBD\xEA\xB3\xA0"); // "경고"
    summaryLabel.setText(errorText + " " + juce::String(errorCount) +
                             "  " + warningText + " " +
                             juce::String(warningCount),
                         juce::dontSendNotification);
  } else if (warningCount > 0) {
    const auto warningText = makeUtf8String("\xEA\xB2\xBD\xEA\xB3\xA0"); // "경고"
    summaryLabel.setText(warningText + " " + juce::String(warningCount),
                         juce::dontSendNotification);
  } else {
    summaryLabel.setText("OK", juce::dontSendNotification);
  }

  listBox.updateContent();
  repaint();
}

bool ValidationPanel::autoRefreshEnabled() const noexcept {
  return autoRefresh;
}

void ValidationPanel::setAutoRefreshEnabled(bool enabled) {
  autoRefresh = enabled;
  autoRefreshToggle.setToggleState(autoRefresh, juce::dontSendNotification);
  if (autoRefresh && dirty)
    refreshValidation();
}

// ==============================================================================
// paint / resized
// ==============================================================================
void ValidationPanel::paint(juce::Graphics &g) {
  g.fillAll(palette(GyeolPalette::PanelBackground));
  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawRect(getLocalBounds(), 1);

  const bool allClear =
      filteredRows.empty() ||
      (filteredRows.size() == 1 &&
       allIssues[static_cast<size_t>(filteredRows[0])].severity ==
           IssueSeverity::info);

  if (allClear && !dirty) {
    auto area = listBox.getBounds().toFloat();
    if (area.getHeight() > 60.0f) {
      auto centerX = area.getCentreX();
      auto centerY = area.getCentreY() - 8.0f;

      juce::Path checkPath;
      checkPath.startNewSubPath(centerX - 10.0f, centerY);
      checkPath.lineTo(centerX - 3.0f, centerY + 7.0f);
      checkPath.lineTo(centerX + 10.0f, centerY - 6.0f);

      g.setColour(palette(GyeolPalette::ValidSuccess, 0.12f));
      g.fillEllipse(centerX - 18.0f, centerY - 15.0f, 36.0f, 36.0f);
      g.setColour(palette(GyeolPalette::ValidSuccess, 0.67f));
      g.strokePath(checkPath,
                   juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));

      g.setColour(palette(GyeolPalette::TextSecondary));
      g.setFont(makePanelFont(*this, 11.5f, false));
      g.drawText(
          "All checks passed",
          juce::Rectangle<int>(0, (int)(centerY + 18.0f), getWidth(), 18),
          juce::Justification::centred, true);
    }
  }
}

void ValidationPanel::resized() {
  auto area = getLocalBounds().reduced(8);

  // 상단 타이틀 행
  auto top = area.removeFromTop(20);
  titleLabel.setBounds(top.removeFromLeft(120));
  summaryLabel.setBounds(top);

  area.removeFromTop(4);

  // 컨트롤 행 1: Run + Auto
  auto controls = area.removeFromTop(24);
  runButton.setBounds(controls.removeFromLeft(110));
  controls.removeFromLeft(6);
  autoRefreshToggle.setBounds(controls.removeFromLeft(60));

  area.removeFromTop(4);

  // 컨트롤 행 2: 필터 버튼 (E / W / I)
  auto filterRow = area.removeFromTop(22);
  const int btnW = 36;
  filterErrorBtn.setBounds(filterRow.removeFromLeft(btnW));
  filterRow.removeFromLeft(4);
  filterWarningBtn.setBounds(filterRow.removeFromLeft(btnW));
  filterRow.removeFromLeft(4);
  filterInfoBtn.setBounds(filterRow.removeFromLeft(btnW));

  area.removeFromTop(6);
  listBox.setBounds(area);
}

// ==============================================================================
// ListBoxModel
// ==============================================================================
int ValidationPanel::getNumRows() {
  return static_cast<int>(filteredRows.size());
}

void ValidationPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                       int width, int height,
                                       bool rowIsSelected) {
  if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredRows.size()))
    return;

  const auto &issue = allIssues[static_cast<size_t>(filteredRows[rowNumber])];
  const auto bounds = juce::Rectangle<int>(0, 0, width, height);

  // 배경
  const auto baseFill = rowIsSelected
                            ? palette(GyeolPalette::SelectionBackground)
                            : palette(GyeolPalette::ControlBase);
  g.setColour(baseFill.withAlpha(rowIsSelected ? 1.0f : 0.55f));
  g.fillRect(bounds);

  // 선택 시 좌측 Accent 바
  if (rowIsSelected) {
    g.setColour(colorForSeverity(issue.severity));
    g.fillRect(0, 0, 3, height);
  }

  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

  auto textArea = bounds.reduced(8, 4);
  auto header = textArea.removeFromTop(14);
  const auto severityColour = colorForSeverity(issue.severity);

  // 심각도 배지
  g.setColour(severityColour.withAlpha(0.85f));
  g.fillRoundedRectangle(
      juce::Rectangle<float>(static_cast<float>(header.getX()),
                             static_cast<float>(header.getY() + 1), 46.0f,
                             12.0f),
      3.0f);
  g.setColour(palette(GyeolPalette::TextPrimary).contrasting(1.0f).withAlpha(0.9f));
  g.setFont(makePanelFont(*this, 9.0f, true));
  g.drawText(labelForSeverity(issue.severity),
             juce::Rectangle<int>(header.getX(), header.getY() + 1, 46, 12),
             juce::Justification::centred, true);

  // 연관 위젯이 있을 경우 우측에 "위젯 이동" 힌트 아이콘 (▶)
  int titleRightOffset = 0;
  if (issue.relatedWidgetId > kRootId) {
    g.setColour(palette(GyeolPalette::AccentPrimary, 0.6f));
    g.setFont(makePanelFont(*this, 10.0f, false));
    const juce::String hint = makeUtf8String("  \xE2\x96\xB6"); // UTF-8 right-pointing triangle
    g.drawText(hint,
               juce::Rectangle<int>(header.getRight() - 24, header.getY(), 24,
                                    header.getHeight()),
               juce::Justification::centredRight, false);
    titleRightOffset = 26;
  }

  // 이슈 타이틀
  g.setColour(palette(GyeolPalette::TextPrimary));
  g.setFont(makePanelFont(*this, 11.0f, true));
  g.drawText(issue.title,
             juce::Rectangle<int>(header.getX() + 52, header.getY(),
                                  header.getWidth() - 52 - titleRightOffset,
                                  header.getHeight()),
             juce::Justification::centredLeft, true);

  // 이슈 메시지
  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(makePanelFont(*this, 10.5f, false));
  g.drawText(issue.message, textArea, juce::Justification::centredLeft, true);
}

void ValidationPanel::selectedRowsChanged(int lastRowSelected) {
  // 단순 선택만으로는 위젯 포커스를 강제하지 않음 (더블클릭으로 처리)
  juce::ignoreUnused(lastRowSelected);
}

void ValidationPanel::listBoxItemDoubleClicked(int row,
                                               const juce::MouseEvent &) {
  if (row < 0 || row >= static_cast<int>(filteredRows.size()))
    return;

  const auto &issue = allIssues[static_cast<size_t>(filteredRows[row])];
  if (issue.relatedWidgetId > kRootId && onSelectWidget)
    onSelectWidget(issue.relatedWidgetId);
}

// ==============================================================================
// 내부 구현
// ==============================================================================
void ValidationPanel::rebuildFilteredIssues() {
  filteredRows.clear();
  filteredRows.reserve(allIssues.size());

  for (int i = 0; i < static_cast<int>(allIssues.size()); ++i) {
    const auto &issue = allIssues[static_cast<size_t>(i)];
    switch (issue.severity) {
    case IssueSeverity::error:
      if (showErrors)
        filteredRows.push_back(i);
      break;
    case IssueSeverity::warning:
      if (showWarnings)
        filteredRows.push_back(i);
      break;
    case IssueSeverity::info:
      if (showInfo)
        filteredRows.push_back(i);
      break;
    }
  }
}

void ValidationPanel::rebuildIssues() {
  allIssues.clear();

  const auto &snapshot = document.snapshot();
  const auto &editorState = document.editorState();
  const auto sceneResult =
      Core::SceneValidator::validateScene(snapshot, &editorState);
  if (sceneResult.failed())
    pushIssue(IssueSeverity::error, "Scene validation failed",
              sceneResult.getErrorMessage());
  else
    pushIssue(IssueSeverity::info, "Scene validation passed",
              "Core model invariants are valid.");

  if (snapshot.widgets.empty())
    pushIssue(IssueSeverity::warning, "Empty document",
              "No widgets in current document.");

  int hiddenLayerCount = 0;
  int lockedLayerCount = 0;
  for (const auto &layer : snapshot.layers) {
    hiddenLayerCount += layer.visible ? 0 : 1;
    lockedLayerCount += layer.locked ? 1 : 0;
    if (layer.memberWidgetIds.empty() && layer.memberGroupIds.empty()) {
      pushIssue(IssueSeverity::info, "Empty layer: " + layer.name,
                "Layer has no widget/group members.");
    }
  }

  if (!snapshot.layers.empty() &&
      hiddenLayerCount == static_cast<int>(snapshot.layers.size()))
    pushIssue(IssueSeverity::warning, "All layers hidden",
              "Canvas may render as empty.");
  if (!snapshot.layers.empty() &&
      lockedLayerCount == static_cast<int>(snapshot.layers.size()))
    pushIssue(IssueSeverity::warning, "All layers locked",
              "Editing interaction will be blocked.");

  std::unordered_set<juce::String> seenLayerNames;
  for (const auto &layer : snapshot.layers) {
    const auto normalized = layer.name.trim().toLowerCase();
    if (normalized.isEmpty())
      continue;
    if (!seenLayerNames.insert(normalized).second) {
      pushIssue(IssueSeverity::warning, "Duplicate layer name",
                "Layer name '" + layer.name + "' is duplicated.");
    }
  }

  for (const auto &widget : snapshot.widgets) {
    if (registry.find(widget.type) == nullptr) {
      pushIssue(IssueSeverity::warning, "Unknown widget descriptor",
                "Widget id=" + juce::String(widget.id) +
                    " has no descriptor in registry.",
                widget.id);
    }
  }

  const auto runtimeIssues =
      Core::SceneValidator::validateRuntimeBindings(snapshot);
  for (const auto &issue : runtimeIssues) {
    const auto severity =
        issue.severity ==
                Core::SceneValidator::RuntimeBindingIssueSeverity::error
            ? IssueSeverity::error
            : IssueSeverity::warning;
    pushIssue(severity, issue.title, issue.message);
  }

  std::unordered_map<WidgetId, const WidgetModel *> widgetById;
  widgetById.reserve(snapshot.widgets.size());
  for (const auto &widget : snapshot.widgets)
    widgetById.emplace(widget.id, &widget);

  for (const auto &binding : snapshot.runtimeBindings) {
    const auto widgetIt = widgetById.find(binding.sourceWidgetId);
    if (widgetIt == widgetById.end())
      continue;

    const auto *descriptor = registry.find(widgetIt->second->type);
    if (descriptor == nullptr)
      continue;

    const auto supported = std::any_of(
        descriptor->runtimeEvents.begin(), descriptor->runtimeEvents.end(),
        [&binding](const Widgets::RuntimeEventSpec &eventSpec) {
          return eventSpec.key == binding.eventKey;
        });
    if (!supported) {
      pushIssue(IssueSeverity::warning, "Unsupported event key",
                "Binding id=" + juce::String(binding.id) + " event '" +
                    binding.eventKey + "' is not supported by widget type '" +
                    descriptor->typeKey + "'.",
                binding.sourceWidgetId);
    }
  }

  const auto projectRoot = resolveProjectRootDirectory();
  constexpr juce::int64 kLargeAssetWarnBytes = 5 * 1024 * 1024;
  constexpr int kLargeImageDimension = 4096;

  for (const auto &asset : snapshot.assets) {
    const auto assetLabel = asset.name.trim().isNotEmpty()
                                ? asset.name.trim()
                                : asset.refKey.trim();
    const auto displayLabel = assetLabel.isNotEmpty()
                                  ? assetLabel
                                  : ("Asset #" + juce::String(asset.id));

    if (asset.kind == AssetKind::colorPreset)
      continue;

    const auto normalizedPath = asset.relativePath.trim();
    if (normalizedPath.isEmpty()) {
      pushIssue(IssueSeverity::warning, "Asset path missing",
                displayLabel + " has an empty relative path.");
      continue;
    }

    const auto sourceFile = resolveInputFilePath(normalizedPath, projectRoot);
    if (!sourceFile.existsAsFile()) {
      pushIssue(IssueSeverity::warning, "Asset file missing",
                displayLabel + " path not found: " + normalizedPath);
      continue;
    }

    const auto fileSize = sourceFile.getSize();
    if (fileSize > kLargeAssetWarnBytes) {
      pushIssue(IssueSeverity::warning, "Large asset file",
                displayLabel + " is " + formatByteSize(fileSize) + " (" +
                    sourceFile.getFileName() + ").");
    }

    if (asset.kind == AssetKind::image) {
      const auto image = juce::ImageFileFormat::loadFrom(sourceFile);
      if (!image.isValid()) {
        pushIssue(IssueSeverity::warning, "Image decode failed",
                  displayLabel + " could not be decoded as image (" +
                      sourceFile.getFileName() + ").");
      } else if (image.getWidth() > kLargeImageDimension ||
                 image.getHeight() > kLargeImageDimension) {
        pushIssue(IssueSeverity::warning, "Large image resolution",
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
      pushIssue(IssueSeverity::warning, "Asset MIME mismatch",
                displayLabel + " MIME is '" + asset.mimeType + "', expected '" +
                    expectedMime + "'.");
    }
  }
}

void ValidationPanel::pushIssue(IssueSeverity severity,
                                const juce::String &title,
                                const juce::String &message,
                                WidgetId relatedWidgetId) {
  Issue issue;
  issue.severity = severity;
  issue.title = title;
  issue.message = message;
  issue.relatedWidgetId = relatedWidgetId;
  allIssues.push_back(std::move(issue));
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
