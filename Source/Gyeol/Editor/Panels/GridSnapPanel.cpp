#include "Gyeol/Editor/Panels/GridSnapPanel.h"

#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

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

void configureDragNumberSlider(juce::Slider &slider, double min, double max,
                               double step, double defaultValue) {
  slider.setSliderStyle(juce::Slider::LinearBar);
  slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 52, 20);
  slider.setRange(min, max, step);
  slider.setNumDecimalPlacesToDisplay(0);
  slider.setMouseDragSensitivity(220);
  slider.setDoubleClickReturnValue(true, defaultValue,
                                   juce::ModifierKeys::noModifiers);
}
} // namespace

namespace Gyeol::Ui::Panels {
GridSnapPanel::GridSnapPanel() {
  addAndMakeVisible(snapEnabledToggle);
  addAndMakeVisible(smartSnapToggle);
  addAndMakeVisible(gridToggle);
  addAndMakeVisible(gridSnapToggle);
  addAndMakeVisible(gridSizeLabel);
  addAndMakeVisible(gridSizeSlider);
  addAndMakeVisible(gridSizeBadgeLabel);
  addAndMakeVisible(toleranceLabel);
  addAndMakeVisible(toleranceSlider);

  for (auto *toggle :
       {&snapEnabledToggle, &smartSnapToggle, &gridToggle, &gridSnapToggle})
    toggle->setClickingTogglesState(true);

  snapEnabledToggle.setTooltip("Toggle all snap behavior.");
  smartSnapToggle.setTooltip("Enable alignment against nearby widget edges.");
  gridToggle.setTooltip("Show or hide the canvas grid overlay.");
  gridSnapToggle.setTooltip("Snap movement/resizing to the grid.");

  const auto labelColour = palette(GyeolPalette::TextSecondary);

  gridSizeLabel.setText("Size", juce::dontSendNotification);
  gridSizeLabel.setColour(juce::Label::textColourId, labelColour);
  gridSizeLabel.setJustificationType(juce::Justification::centredLeft);

  toleranceLabel.setText("Tolerance", juce::dontSendNotification);
  toleranceLabel.setColour(juce::Label::textColourId, labelColour);
  toleranceLabel.setJustificationType(juce::Justification::centredLeft);

  gridSizeBadgeLabel.setInterceptsMouseClicks(false, false);
  gridSizeBadgeLabel.setJustificationType(juce::Justification::centred);
  gridSizeBadgeLabel.setColour(juce::Label::textColourId,
                               palette(GyeolPalette::AccentPrimary));

  configureDragNumberSlider(gridSizeSlider, 1.0, 100.0, 1.0, 8.0);
  configureDragNumberSlider(toleranceSlider, 0.0, 20.0, 1.0, 4.0);

  const auto onUiChanged = [this] { applySettingsFromUi(); };

  snapEnabledToggle.onClick = onUiChanged;
  smartSnapToggle.onClick = onUiChanged;
  gridToggle.onClick = onUiChanged;
  gridSnapToggle.onClick = onUiChanged;
  gridSizeSlider.onValueChange = onUiChanged;
  toleranceSlider.onValueChange = onUiChanged;

  syncUiFromSettings();
}

void GridSnapPanel::setSettings(const Interaction::SnapSettings &settingsIn) {
  snapSettings = settingsIn;
  syncUiFromSettings();
  repaint();
}

const Interaction::SnapSettings &GridSnapPanel::settings() const noexcept {
  return snapSettings;
}

void GridSnapPanel::setSettingsChangedCallback(
    std::function<void(const Interaction::SnapSettings &)> callback) {
  onSettingsChanged = std::move(callback);
}

void GridSnapPanel::paint(juce::Graphics &g) {
  g.fillAll(palette(GyeolPalette::PanelBackground));
  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawRect(getLocalBounds(), 1);

  g.setColour(palette(GyeolPalette::TextSecondary, 0.9f));
  g.setFont(makePanelFont(*this, 10.0f, true));
  g.drawText("MODES", gridSectionLabelBounds, juce::Justification::centredLeft,
             true);
  g.drawText("SNAP", snapSectionLabelBounds, juce::Justification::centredLeft,
             true);

  if (sectionDividerY > 0) {
    const auto left = static_cast<float>(getLocalBounds().getX() + 8);
    const auto right = static_cast<float>(getLocalBounds().getRight() - 8);
    g.setColour(palette(GyeolPalette::BorderDefault, 0.9f));
    g.drawHorizontalLine(sectionDividerY, left, right);
  }

  const auto badgeBounds = gridSizeBadgeLabel.getBounds().toFloat();
  g.setColour(palette(GyeolPalette::AccentPrimary, 0.2f));
  g.fillRoundedRectangle(badgeBounds, 10.0f);
  g.setColour(palette(GyeolPalette::AccentPrimary, 0.8f));
  g.drawRoundedRectangle(badgeBounds.reduced(0.5f), 10.0f, 1.0f);
}

void GridSnapPanel::resized() {
  auto area = getLocalBounds().reduced(8);

  auto toggleArea = area.removeFromTop(50);
  auto topRow = toggleArea.removeFromTop(23);
  toggleArea.removeFromTop(4);
  auto bottomRow = toggleArea.removeFromTop(23);

  constexpr int kToggleGap = 6;
  const auto leftToggleWidth =
      juce::jmax(32, (topRow.getWidth() - kToggleGap) / 2);

  snapEnabledToggle.setBounds(topRow.removeFromLeft(leftToggleWidth));
  topRow.removeFromLeft(kToggleGap);
  smartSnapToggle.setBounds(topRow);

  gridToggle.setBounds(bottomRow.removeFromLeft(leftToggleWidth));
  bottomRow.removeFromLeft(kToggleGap);
  gridSnapToggle.setBounds(bottomRow);

  area.removeFromTop(6);
  gridSectionLabelBounds = area.removeFromTop(14);
  area.removeFromTop(3);

  constexpr int kLabelWidth = 66;
  constexpr int kBadgeWidth = 52;

  auto gridRow = area.removeFromTop(24);
  gridSizeLabel.setBounds(gridRow.removeFromLeft(kLabelWidth));
  gridSizeBadgeLabel.setBounds(gridRow.removeFromRight(kBadgeWidth));
  gridRow.removeFromRight(6);
  gridSizeSlider.setBounds(gridRow);

  area.removeFromTop(8);
  sectionDividerY = area.getY();
  area.removeFromTop(8);

  snapSectionLabelBounds = area.removeFromTop(14);
  area.removeFromTop(3);

  auto toleranceRow = area.removeFromTop(24);
  toleranceLabel.setBounds(toleranceRow.removeFromLeft(kLabelWidth));
  toleranceSlider.setBounds(toleranceRow);
}

void GridSnapPanel::syncUiFromSettings() {
  snapEnabledToggle.setToggleState(snapSettings.snapEnabled,
                                   juce::dontSendNotification);
  smartSnapToggle.setToggleState(snapSettings.enableSmartSnap,
                                 juce::dontSendNotification);
  gridToggle.setToggleState(snapSettings.enableGrid,
                            juce::dontSendNotification);
  gridSnapToggle.setToggleState(snapSettings.enableGridSnap,
                                juce::dontSendNotification);
  gridSizeSlider.setValue(
      juce::jlimit(1.0, 100.0, static_cast<double>(snapSettings.gridSize)),
      juce::dontSendNotification);
  toleranceSlider.setValue(
      juce::jlimit(0.0, 20.0, static_cast<double>(snapSettings.tolerance)),
      juce::dontSendNotification);
  updateGridSizeBadge();
}

void GridSnapPanel::applySettingsFromUi() {
  snapSettings.snapEnabled = snapEnabledToggle.getToggleState();
  snapSettings.enableSmartSnap = smartSnapToggle.getToggleState();
  snapSettings.enableGrid = gridToggle.getToggleState();
  snapSettings.enableGridSnap = gridSnapToggle.getToggleState();
  snapSettings.gridSize = static_cast<float>(gridSizeSlider.getValue());
  snapSettings.tolerance = static_cast<float>(toleranceSlider.getValue());

  updateGridSizeBadge();
  notifySettingsChanged();
  repaint();
}

void GridSnapPanel::notifySettingsChanged() const {
  if (onSettingsChanged != nullptr)
    onSettingsChanged(snapSettings);
}

void GridSnapPanel::updateGridSizeBadge() {
  const auto valueText =
      juce::String(juce::roundToInt(gridSizeSlider.getValue())) + " px";
  gridSizeBadgeLabel.setText(valueText, juce::dontSendNotification);
}
} // namespace Gyeol::Ui::Panels
