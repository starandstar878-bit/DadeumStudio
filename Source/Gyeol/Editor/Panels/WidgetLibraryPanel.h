#pragma once

#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Gyeol::Ui::Panels {
class WidgetLibraryPanel : public juce::Component,
                           public juce::DragAndDropContainer {
public:
  explicit WidgetLibraryPanel(const Widgets::WidgetRegistry &registryIn);
  ~WidgetLibraryPanel() override;

  void refreshFromRegistry();
  void noteWidgetCreated(const juce::String &typeKey);

  void setCreateRequestedCallback(
      std::function<void(const juce::String &)> callback);
  void setFavoriteToggledCallback(
      std::function<void(const juce::String &, bool)> callback);

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  struct DisplayItem {
    const Widgets::WidgetDescriptor *descriptor = nullptr;
    bool favorite = false;
    int recentRank = -1;
  };

  class CardComponent;
  friend class CardComponent;

  void rebuildCategories();
  void rebuildVisibleItems();
  void triggerCreateForRow(int row);
  void startDragForRow(int row, juce::Component &sourceComponent,
                       juce::Point<int> dragStartPos);
  void toggleFavoriteForRow(int row, bool favorite);
  void toggleFavoriteByTypeKey(const juce::String &typeKey, bool favorite);
  void updateRecentByTypeKey(const juce::String &typeKey);
  int recentRankFor(const juce::String &typeKey) const;
  bool isFavorite(const juce::String &typeKey) const;

  void updateGridLayout();

  void loadSettings();
  void saveSettings();
  static juce::String toJsonArrayString(const juce::StringArray &values);
  static juce::StringArray fromJsonArrayString(const juce::String &serialized);

  static juce::String normalizeCategory(const juce::String &category);
  static juce::String
  iconGlyphForDescriptor(const Widgets::WidgetDescriptor &descriptor);
  static juce::Colour
  iconColorForDescriptor(const Widgets::WidgetDescriptor &descriptor);

  const Widgets::WidgetRegistry &registry;

  juce::Label titleLabel;
  juce::ComboBox categoryBox;
  juce::TextEditor searchBox;
  juce::ToggleButton favoritesOnlyToggle{"Favorites"};
  juce::Viewport viewport;
  std::unique_ptr<juce::Component> contentComp;

  std::vector<DisplayItem> visibleItems;
  juce::StringArray recentTypeKeys;
  juce::StringArray favoriteTypeKeys;
  std::unique_ptr<juce::PropertiesFile> settingsFile;

  std::function<void(const juce::String &)> onCreateRequested;
  std::function<void(const juce::String &, bool)> onFavoriteToggled;

  static constexpr int maxRecentCount = 12;
};
} // namespace Gyeol::Ui::Panels
