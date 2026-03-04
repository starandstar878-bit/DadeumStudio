#include "Gyeol/Editor/Panels/ExportPreviewPanel.h"
#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"
#include <cmath>

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

void setupReadOnlyEditor(juce::CodeEditorComponent &editor) {
  editor.setReadOnly(true);
  editor.setScrollbarThickness(12);
  editor.setLineNumbersShown(true);

  // Adjust to larger font for readability
  editor.setFont(juce::FontOptions("Consolas", 12.5f, juce::Font::plain));

  editor.setColour(juce::CodeEditorComponent::backgroundColourId,
                   palette(GyeolPalette::ControlBase));
  editor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId,
                   palette(GyeolPalette::CanvasBackground, 0.95f));
  editor.setColour(juce::CodeEditorComponent::lineNumberTextId,
                   palette(GyeolPalette::TextSecondary, 0.88f));
  editor.setColour(juce::CodeEditorComponent::defaultTextColourId,
                   palette(GyeolPalette::TextPrimary));
  editor.setColour(juce::CodeEditorComponent::highlightColourId,
                   palette(GyeolPalette::SelectionBackground, 0.85f));
}
} // namespace

ExportPreviewPanel::ExportPreviewPanel() {
  titleLabel.setText("Export Preview", juce::dontSendNotification);
  titleLabel.setFont(makePanelFont(*this, 12.0f, true));
  titleLabel.setColour(juce::Label::textColourId,
                       palette(GyeolPalette::TextPrimary));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(titleLabel);

  setStatusText("Stale", palette(GyeolPalette::TextSecondary));
  addAndMakeVisible(statusLabel);

  classNameEditor.setText("GyeolExportedComponent", juce::dontSendNotification);
  classNameEditor.setTextToShowWhenEmpty("ComponentClassName",
                                         palette(GyeolPalette::TextSecondary));
  classNameEditor.setInputRestrictions(
      128, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
  classNameEditor.onTextChange = [this] { markDirty(); };
  classNameEditor.onReturnKey = [this] {
    markDirty();
    if (autoRefresh)
      refreshPreview();
  };
  addAndMakeVisible(classNameEditor);

  refreshButton.setButtonText("Generate");
  refreshButton.onClick = [this] { refreshPreview(); };
  addAndMakeVisible(refreshButton);

  exportButton.setButtonText("Export");
  exportButton.onClick = [this] {
    if (onExportRequested != nullptr)
      onExportRequested(normalizedClassName());
  };
  addAndMakeVisible(exportButton);

  setupReadOnlyEditor(reportEditor);
  setupReadOnlyEditor(headerEditor);
  setupReadOnlyEditor(sourceEditor);
  setupReadOnlyEditor(manifestEditor);

  tabs.setTabBarDepth(30);
  tabs.setOutline(0);
  tabs.setColour(juce::TabbedButtonBar::tabOutlineColourId,
                 palette(GyeolPalette::BorderDefault));
  tabs.setColour(juce::TabbedButtonBar::frontOutlineColourId,
                 palette(GyeolPalette::BorderDefault));
  tabs.setColour(juce::TabbedButtonBar::frontTextColourId,
                 palette(GyeolPalette::TextPrimary));
  tabs.setColour(juce::TabbedButtonBar::tabTextColourId,
                 palette(GyeolPalette::TextSecondary));
  tabs.setColour(juce::TabbedComponent::backgroundColourId,
                 palette(GyeolPalette::PanelBackground));
  tabs.setColour(juce::TabbedComponent::outlineColourId,
                 palette(GyeolPalette::BorderDefault));

  tabs.addTab("Report", palette(GyeolPalette::PanelBackground), &reportEditor,
              false);
  tabs.addTab("Header", palette(GyeolPalette::PanelBackground), &headerEditor,
              false);
  tabs.addTab("Source", palette(GyeolPalette::PanelBackground), &sourceEditor,
              false);
  tabs.addTab("Manifest", palette(GyeolPalette::PanelBackground),
              &manifestEditor, false);
  tabs.setCurrentTabIndex(0, juce::dontSendNotification);
  cachedActiveTabIndex = tabs.getCurrentTabIndex();
  addAndMakeVisible(tabs);

  footerLabel.setJustificationType(juce::Justification::centredLeft);
  footerLabel.setColour(juce::Label::textColourId,
                        palette(GyeolPalette::TextSecondary));
  addAndMakeVisible(footerLabel);

  updateFooterStatus();
}

ExportPreviewPanel::~ExportPreviewPanel() {
  // CRITICAL: Clear all tabs so that juce::TabbedButtonBar does not
  // hold pointers to child components that are about to be destroyed!
  // This completely eliminates read access violations during destruction.
  tabs.clearTabs();
}

void ExportPreviewPanel::setGeneratePreviewCallback(
    GeneratePreviewCallback callback) {
  onGeneratePreview = std::move(callback);
}

void ExportPreviewPanel::setExportRequestedCallback(
    ExportRequestedCallback callback) {
  onExportRequested = std::move(callback);
}

void ExportPreviewPanel::markDirty() {
  dirty = true;
  if (autoRefresh)
    refreshPreview();
  else
    setStatusText("Stale (Generate)", palette(GyeolPalette::TextSecondary));

  updateFooterStatus();
}

void ExportPreviewPanel::refreshPreview() {
  if (onGeneratePreview == nullptr) {
    clearPreviewEditors();
    setStatusText("Preview callback is not connected",
                  palette(GyeolPalette::ValidWarning));
    return;
  }

  PreviewData data;
  const auto result = onGeneratePreview(normalizedClassName(), data);
  if (result.failed()) {
    clearPreviewEditors();
    setStatusText("Preview failed: " + result.getErrorMessage(),
                  palette(GyeolPalette::ValidError));
    return;
  }

  applyPreviewData(data);
  dirty = false;
  lastGeneratedTime = juce::Time::getCurrentTime().formatted("%H:%M:%S");
  setStatusText("Preview generated", palette(GyeolPalette::ValidSuccess));
  updateFooterStatus();
}

bool ExportPreviewPanel::autoRefreshEnabled() const noexcept {
  return autoRefresh;
}

void ExportPreviewPanel::setAutoRefreshEnabled(bool enabled) {
  autoRefresh = enabled;
  if (autoRefresh && dirty)
    refreshPreview();
}

void ExportPreviewPanel::paint(juce::Graphics &g) {
  // Safe, fast polling. Called on UI layout thread. Prevents complicated async
  // bugs.
  syncActiveTabState();

  g.fillAll(palette(GyeolPalette::PanelBackground));
  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawRect(getLocalBounds(), 1);

  if (!tabBarBoundsInPanel.isEmpty()) {
    auto tabBar = tabBarBoundsInPanel.toFloat().reduced(0.0f, 1.0f);
    g.setColour(palette(GyeolPalette::HeaderBackground, 0.86f));
    g.fillRoundedRectangle(tabBar, 6.0f);
    g.setColour(palette(GyeolPalette::BorderDefault, 0.88f));
    g.drawRoundedRectangle(tabBar, 6.0f, 1.0f);

    const auto tabCount = juce::jmax(1, tabs.getNumTabs());
    const auto tabWidth = tabBarBoundsInPanel.getWidth() / tabCount;
    const auto active =
        juce::jlimit(0, tabCount - 1, tabs.getCurrentTabIndex());
    const auto indicatorWidth = juce::jmax(24, tabWidth - 20);
    const auto indicatorX = tabBarBoundsInPanel.getX() + active * tabWidth +
                            (tabWidth - indicatorWidth) / 2;

    auto indicator = juce::Rectangle<float>(
        static_cast<float>(indicatorX),
        static_cast<float>(tabBarBoundsInPanel.getBottom() - 4),
        static_cast<float>(indicatorWidth), 3.0f);
    g.setColour(palette(GyeolPalette::AccentPrimary));
    g.fillRoundedRectangle(indicator, 2.0f);
  }

  if (!footerBounds.isEmpty()) {
    auto footer = footerBounds.toFloat().reduced(0.0f, 1.0f);
    g.setColour(palette(GyeolPalette::HeaderBackground, 0.72f));
    g.fillRoundedRectangle(footer, 4.0f);
    g.setColour(palette(GyeolPalette::BorderDefault, 0.86f));
    g.drawRoundedRectangle(footer, 4.0f, 1.0f);
  }
}

void ExportPreviewPanel::paintOverChildren(juce::Graphics &g) {
  syncActiveTabState();

  if (hasPreviewData())
    return;

  auto emptyArea = tabs.getBounds().reduced(10);
  emptyArea.removeFromTop(tabs.getTabBarDepth());
  if (emptyArea.getHeight() < 48)
    return;

  g.setColour(palette(GyeolPalette::TextSecondary, 0.94f));
  g.setFont(makePanelFont(*this, 12.0f, true));
  g.drawText("< />", emptyArea.removeFromTop(20), juce::Justification::centred,
             true);

  g.setColour(palette(GyeolPalette::TextSecondary, 0.90f));
  g.setFont(makePanelFont(*this, 10.2f, false));
  g.drawText("Click Generate to preview export", emptyArea.removeFromTop(18),
             juce::Justification::centred, true);
}

void ExportPreviewPanel::resized() {
  auto area = getLocalBounds().reduced(8);

  auto row0 = area.removeFromTop(20);
  titleLabel.setBounds(row0.removeFromLeft(130));
  statusLabel.setBounds(row0);

  area.removeFromTop(4);
  auto controls = area.removeFromTop(24);
  constexpr int buttonWidth = 72;
  exportButton.setBounds(controls.removeFromRight(buttonWidth));
  controls.removeFromRight(6);
  refreshButton.setBounds(controls.removeFromRight(buttonWidth));
  controls.removeFromRight(8);
  classNameEditor.setBounds(controls);

  area.removeFromTop(6);
  footerBounds = area.removeFromBottom(18);
  footerLabel.setBounds(footerBounds.reduced(6, 0));

  tabs.setBounds(area);
  tabBarBoundsInPanel = tabs.getBounds();
  tabBarBoundsInPanel.setHeight(tabs.getTabBarDepth());

  updateFooterStatus();
}

void ExportPreviewPanel::applyPreviewData(const PreviewData &data) {
  reportDoc.replaceAllContent(data.reportText);
  headerDoc.replaceAllContent(data.headerText);
  sourceDoc.replaceAllContent(data.sourceText);

  juce::String formattedManifest = data.manifestText;
  juce::var parsedJson;
  if (juce::JSON::parse(data.manifestText, parsedJson).wasOk())
    formattedManifest = juce::JSON::toString(parsedJson, true);

  manifestDoc.replaceAllContent(formattedManifest);

  if (data.outputPath.isNotEmpty())
    reportDoc.replaceAllContent(data.reportText + "\n\n[Output]\n" +
                                data.outputPath);

  updateFooterStatus();
}

void ExportPreviewPanel::clearPreviewEditors() {
  reportDoc.replaceAllContent("");
  headerDoc.replaceAllContent("");
  sourceDoc.replaceAllContent("");
  manifestDoc.replaceAllContent("");
  lastGeneratedTime = "--:--:--";
  updateFooterStatus();
}

void ExportPreviewPanel::setStatusText(const juce::String &text,
                                       juce::Colour colour) {
  statusLabel.setText(text, juce::dontSendNotification);
  statusLabel.setColour(juce::Label::textColourId, colour);
  statusLabel.setJustificationType(juce::Justification::centredRight);
}

juce::String ExportPreviewPanel::normalizedClassName() const {
  const auto legal =
      juce::File::createLegalFileName(classNameEditor.getText().trim());
  if (legal.isEmpty())
    return "GyeolExportedComponent";
  return legal;
}

void ExportPreviewPanel::updateFooterStatus() {
  int lines = 0;
  switch (tabs.getCurrentTabIndex()) {
  case 0:
    lines = reportDoc.getNumLines();
    break;
  case 1:
    lines = headerDoc.getNumLines();
    break;
  case 2:
    lines = sourceDoc.getNumLines();
    break;
  case 3:
    lines = manifestDoc.getNumLines();
    break;
  }
  footerLabel.setText("Generated at " + lastGeneratedTime +
                          "  |  Lines: " + juce::String(lines),
                      juce::dontSendNotification);
}

bool ExportPreviewPanel::hasPreviewData() const {
  return reportDoc.getNumCharacters() > 0 || headerDoc.getNumCharacters() > 0 ||
         sourceDoc.getNumCharacters() > 0 || manifestDoc.getNumCharacters() > 0;
}

void ExportPreviewPanel::syncActiveTabState() {
  const auto activeIndex = tabs.getCurrentTabIndex();
  if (activeIndex == cachedActiveTabIndex)
    return;

  cachedActiveTabIndex = activeIndex;
  updateFooterStatus();
}

} // namespace Gyeol::Ui::Panels
