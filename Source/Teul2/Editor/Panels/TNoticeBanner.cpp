#include "Teul2/Editor/Panels/TNoticeBanner.h"
#include "Teul2/Editor/Theme/TeulPalette.h"

namespace Teul {

TNoticeBanner::TNoticeBanner() {
  addAndMakeVisible(dismissButton);
  dismissButton.setButtonText("Dismiss");
  dismissButton.onClick = [this] {
    if (dismissHandler != nullptr)
      dismissHandler();
  };
  setVisible(false);
}

void TNoticeBanner::setDismissHandler(std::function<void()> handler) {
  dismissHandler = std::move(handler);
}

void TNoticeBanner::setNotice(const TDocumentNotice &newNotice) {
  notice = newNotice;
  setVisible(notice.active);
  repaint();
}

void TNoticeBanner::paint(juce::Graphics &g) {
  if (!notice.active)
    return;

  const auto bounds = getLocalBounds().toFloat();
  if (bounds.isEmpty())
    return;

  const auto accent = accentForLevel(notice.level);
  g.setColour(TeulPalette::PanelBackgroundRaised().withAlpha(0.96f));
  g.fillRoundedRectangle(bounds, 11.0f);
  g.setColour(TeulPalette::PanelStrokeStrong()
                  .interpolatedWith(accent, 0.65f)
                  .withAlpha(0.85f));
  g.drawRoundedRectangle(bounds.reduced(0.5f), 11.0f, 1.0f);

  auto textArea = getLocalBounds().reduced(10, 6);
  textArea.removeFromRight(72);

  g.setColour(TeulPalette::PanelTextStrong().withAlpha(0.95f));
  g.setFont(juce::FontOptions(11.2f, juce::Font::bold));
  g.drawText(notice.title, textArea.removeFromTop(16),
             juce::Justification::centredLeft, false);

  if (notice.detail.isNotEmpty()) {
    g.setColour(TeulPalette::PanelTextMuted().withAlpha(0.64f));
    g.setFont(9.8f);
    g.drawFittedText(notice.detail, textArea.removeFromTop(12),
                     juce::Justification::centredLeft, 1, 0.9f);
  }
}

void TNoticeBanner::resized() {
  dismissButton.setBounds(getLocalBounds().removeFromRight(68).reduced(8, 7));
}

juce::Colour TNoticeBanner::accentForLevel(TDocumentNoticeLevel level) {
  switch (level) {
  case TDocumentNoticeLevel::degraded:
    return TeulPalette::AccentRed();
  case TDocumentNoticeLevel::warning:
    return TeulPalette::AccentAmber();
  case TDocumentNoticeLevel::info:
    return TeulPalette::AccentBlue();
  }

  return TeulPalette::AccentBlue();
}

} // namespace Teul
