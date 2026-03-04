#pragma once

#include "Gyeol/Editor/Interaction/SnapEngine.h"
#include <JuceHeader.h>
#include <functional>

namespace Gyeol::Ui::Panels {
class GridSnapPanel : public juce::Component {
public:
  GridSnapPanel();

  void setSettings(const Interaction::SnapSettings &settingsIn);
  const Interaction::SnapSettings &settings() const noexcept;

  void setSettingsChangedCallback(
      std::function<void(const Interaction::SnapSettings &)> callback);

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  void syncUiFromSettings();
  void applySettingsFromUi();
  void notifySettingsChanged() const;
  void updateGridSizeBadge();

  Interaction::SnapSettings snapSettings;
  std::function<void(const Interaction::SnapSettings &)> onSettingsChanged;
  juce::ToggleButton snapEnabledToggle{"[M] Snap"};
  juce::ToggleButton smartSnapToggle{"[*] Smart"};
  juce::ToggleButton gridToggle{"[#] Grid"};
  juce::ToggleButton gridSnapToggle{"[#] Grid Snap"};
  juce::Label gridSizeLabel;
  juce::Slider gridSizeSlider;
  juce::Label gridSizeBadgeLabel;
  juce::Label toleranceLabel;
  juce::Slider toleranceSlider;
  juce::Rectangle<int> gridSectionLabelBounds;
  juce::Rectangle<int> snapSectionLabelBounds;
  int sectionDividerY = -1;
};
} // namespace Gyeol::Ui::Panels
