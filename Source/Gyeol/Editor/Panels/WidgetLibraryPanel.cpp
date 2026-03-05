#include "Gyeol/Editor/Panels/WidgetLibraryPanel.h"
#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

#include <algorithm>

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

class PanelViewportContent final : public juce::Component {
public:
  void paint(juce::Graphics &g) override {
    g.fillAll(palette(GyeolPalette::PanelBackground));
  }
};
} // namespace
class WidgetLibraryPanel::CardComponent final : public juce::Component {
public:
  explicit CardComponent(WidgetLibraryPanel &ownerIn) : owner(ownerIn) {
    iconLabel.setJustificationType(juce::Justification::centred);
    iconLabel.setFont(makePanelFont(*this, 16.0f, true));
    iconLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(iconLabel);

    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setFont(makePanelFont(*this, 11.0f, false));
    nameLabel.setInterceptsMouseClicks(false, false);
    nameLabel.setColour(juce::Label::textColourId,
                        palette(GyeolPalette::TextPrimary));
    addAndMakeVisible(nameLabel);

    favoriteToggle.setButtonText("");
    favoriteToggle.setClickingTogglesState(true);
    favoriteToggle.onClick = [this] {
      if (suppressFavoriteCallback)
        return;
      owner.toggleFavoriteByTypeKey(currentTypeKey,
                                    favoriteToggle.getToggleState());
    };
    addAndMakeVisible(favoriteToggle);
  }

  void refreshTheme() {
    iconLabel.setFont(makePanelFont(*this, 16.0f, true));
    nameLabel.setFont(makePanelFont(*this, 11.0f, false));
    nameLabel.setColour(juce::Label::textColourId,
                        palette(GyeolPalette::TextPrimary));
    repaint();
  }

  void setItemData(int index, const DisplayItem &item) {
    itemIndex = index;
    currentTypeKey =
        item.descriptor != nullptr ? item.descriptor->typeKey : juce::String();
    iconColor =
        item.descriptor != nullptr
            ? WidgetLibraryPanel::iconColorForDescriptor(*item.descriptor)
            : palette(GyeolPalette::TextSecondary);

    const auto displayName = item.descriptor != nullptr
                                 ? (item.descriptor->displayName.isNotEmpty()
                                        ? item.descriptor->displayName
                                        : item.descriptor->typeKey)
                                 : juce::String("Widget");
    nameLabel.setText(displayName, juce::dontSendNotification);

    if (item.descriptor != nullptr && item.descriptor->iconPainter != nullptr) {
      iconLabel.setVisible(false);
    } else {
      iconLabel.setVisible(true);
      iconLabel.setText(
          item.descriptor != nullptr
              ? WidgetLibraryPanel::iconGlyphForDescriptor(*item.descriptor)
              : juce::String("W"),
          juce::dontSendNotification);
    }

    suppressFavoriteCallback = true;
    favoriteToggle.setToggleState(item.favorite, juce::dontSendNotification);
    suppressFavoriteCallback = false;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto area = getLocalBounds().toFloat();
    const bool isHovered = isMouseOverOrDragging();

    // Soft & Clean: 遺?쒕윭??諛곌꼍 諛??쇱슫??
    g.setColour(isHovered ? palette(GyeolPalette::ControlHover)
                          : palette(GyeolPalette::ControlBase));
    g.fillRoundedRectangle(area, 8.0f);

    if (isHovered) {
      g.setColour(palette(GyeolPalette::BorderActive).withAlpha(0.6f));
      g.drawRoundedRectangle(area, 8.0f, 1.5f);
    } else {
      g.setColour(palette(GyeolPalette::BorderDefault));
      g.drawRoundedRectangle(area, 8.0f, 1.0f);
    }

    // ?꾩씠肄??곸뿭 ?곷떒 (??而⑦뀒?대꼫)
    auto iconContainer = area.removeFromTop(area.getHeight() * 0.65f);

    // ?멸낸 ?뚮몢由?(?꾩씠肄섏쓽 ??諛곌꼍 諛뺤뒪)
    auto iconInnerBox = iconContainer.reduced(12.0f);
    g.setColour(isHovered ? palette(GyeolPalette::CanvasBackground)
                          : palette(GyeolPalette::ControlDown));
    g.fillRoundedRectangle(iconInnerBox, 6.0f);

    if (isHovered) {
      g.setColour(palette(GyeolPalette::BorderActive).withAlpha(0.3f));
      g.drawRoundedRectangle(iconInnerBox, 6.0f, 1.0f);
    } else {
      g.setColour(palette(GyeolPalette::BorderDefault).withAlpha(0.5f));
      g.drawRoundedRectangle(iconInnerBox, 6.0f, 1.0f);
    }

    const auto iconBounds = juce::Rectangle<float>(0, 0, 32.0f, 32.0f)
                                .withCentre(iconInnerBox.getCentre());
    g.setColour(iconColor.withAlpha(isHovered ? 1.0f : 0.85f));

    if (currentTypeKey.isNotEmpty()) {
      Widgets::LibraryFilter filter;
      filter.query = currentTypeKey;
      const auto matching = owner.registry.findByFilter(filter);
      for (const auto *descriptor : matching) {
        if (descriptor != nullptr && descriptor->typeKey == currentTypeKey &&
            descriptor->iconPainter != nullptr) {
          descriptor->iconPainter(g, iconBounds);
          break;
        }
      }
    }

    // 利먭꺼李얘린 蹂꾨え??洹몃━湲?
    auto favArea = favoriteToggle.getBounds().toFloat();
    if (favoriteToggle.getToggleState() || isMouseOver(true)) {
      juce::Path star;
      star.addStar({favArea.getCentreX(), favArea.getCentreY()}, 5,
                   favArea.getWidth() * 0.2f, favArea.getWidth() * 0.45f);
      g.setColour(favoriteToggle.getToggleState()
                      ? palette(GyeolPalette::ValidWarning)
                      : palette(GyeolPalette::TextSecondary));
      g.fillPath(star);
    }
  }

  void resized() override {
    auto area = getLocalBounds();
    favoriteToggle.setBounds(area.removeFromTop(24).removeFromRight(24));

    auto topArea =
        getLocalBounds().removeFromTop(static_cast<int>(getHeight() * 0.65f));
    iconLabel.setBounds(
        juce::Rectangle<int>(0, 0, 32, 32).withCentre(topArea.getCentre()));

    auto textArea = getLocalBounds().removeFromBottom(
        static_cast<int>(getHeight() * 0.35f));
    nameLabel.setBounds(textArea.reduced(4, 0));
  }

  void mouseDoubleClick(const juce::MouseEvent &) override {
    owner.triggerCreateForRow(itemIndex);
  }

  void mouseDown(const juce::MouseEvent &event) override {
    dragStartPoint = event.getPosition();
    dragStarted = false;
  }

  void mouseDrag(const juce::MouseEvent &event) override {
    if (dragStarted)
      return;

    if (event.getDistanceFromDragStart() < 4)
      return;

    dragStarted = true;
    owner.startDragForRow(itemIndex, *this, dragStartPoint);
  }

  void mouseEnter(const juce::MouseEvent &) override { repaint(); }
  void mouseExit(const juce::MouseEvent &) override { repaint(); }

private:
  WidgetLibraryPanel &owner;
  int itemIndex = -1;
  bool suppressFavoriteCallback = false;
  bool dragStarted = false;
  juce::Point<int> dragStartPoint;
  juce::String currentTypeKey;
  juce::Colour iconColor{palette(GyeolPalette::TextDisabled)};

  juce::Label iconLabel;
  juce::Label nameLabel;
  juce::ToggleButton favoriteToggle;
};

WidgetLibraryPanel::WidgetLibraryPanel(
    const Widgets::WidgetRegistry &registryIn)
    : registry(registryIn) {
  juce::PropertiesFile::Options options;
  options.applicationName = "DadeumStudio";
  options.folderName = "DadeumStudio";
  options.filenameSuffix = "settings";
  options.osxLibrarySubFolder = "Application Support";
  settingsFile = std::make_unique<juce::PropertiesFile>(options);
  loadSettings();

  titleLabel.setText("Widget Library", juce::dontSendNotification);
  titleLabel.setFont(makePanelFont(*this, 13.0f, true));
  titleLabel.setColour(juce::Label::textColourId,
                       palette(GyeolPalette::TextPrimary));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(titleLabel);

  addAndMakeVisible(categoryBox);
  categoryBox.onChange = [this] { rebuildVisibleItems(); };

  searchBox.setTextToShowWhenEmpty("Search widgets...",
                                   palette(GyeolPalette::TextSecondary));
  searchBox.onTextChange = [this] { rebuildVisibleItems(); };
  addAndMakeVisible(searchBox);

  favoritesOnlyToggle.setClickingTogglesState(true);
  favoritesOnlyToggle.onClick = [this] { rebuildVisibleItems(); };
  addAndMakeVisible(favoritesOnlyToggle);

  viewport.setScrollBarsShown(true, false, true, false);
  addAndMakeVisible(viewport);

  contentComp = std::make_unique<PanelViewportContent>();
  viewport.setViewedComponent(contentComp.get(), false);

  lookAndFeelChanged();
  rebuildCategories();
  rebuildVisibleItems();
}

WidgetLibraryPanel::~WidgetLibraryPanel() {
  viewport.setViewedComponent(nullptr, false);
  contentComp->deleteAllChildren();
  contentComp.reset();
  saveSettings();
}


void WidgetLibraryPanel::lookAndFeelChanged() {
  titleLabel.setFont(makePanelFont(*this, 13.0f, true));
  titleLabel.setColour(juce::Label::textColourId,
                       palette(GyeolPalette::TextPrimary));

  categoryBox.setColour(juce::ComboBox::backgroundColourId,
                        palette(GyeolPalette::ControlBase));
  categoryBox.setColour(juce::ComboBox::textColourId,
                        palette(GyeolPalette::TextPrimary));
  categoryBox.setColour(juce::ComboBox::outlineColourId,
                        palette(GyeolPalette::BorderDefault));

  searchBox.setTextToShowWhenEmpty("Search widgets...",
                                   palette(GyeolPalette::TextSecondary));
  searchBox.setColour(juce::TextEditor::backgroundColourId,
                      palette(GyeolPalette::ControlBase));
  searchBox.setColour(juce::TextEditor::outlineColourId,
                      palette(GyeolPalette::BorderDefault));
  searchBox.setColour(juce::TextEditor::textColourId,
                      palette(GyeolPalette::TextPrimary));

  favoritesOnlyToggle.setColour(juce::ToggleButton::textColourId,
                                palette(GyeolPalette::TextPrimary));

  if (contentComp != nullptr) {
    for (auto *child : contentComp->getChildren()) {
      if (auto *card = dynamic_cast<CardComponent *>(child); card != nullptr)
        card->refreshTheme();
    }
  }

  repaint();
}
void WidgetLibraryPanel::refreshFromRegistry() {
  lookAndFeelChanged();
  rebuildCategories();
  rebuildVisibleItems();
}

void WidgetLibraryPanel::noteWidgetCreated(const juce::String &typeKey) {
  if (typeKey.isEmpty())
    return;
  updateRecentByTypeKey(typeKey);
}

void WidgetLibraryPanel::setCreateRequestedCallback(
    std::function<void(const juce::String &)> callback) {
  onCreateRequested = std::move(callback);
}

void WidgetLibraryPanel::setFavoriteToggledCallback(
    std::function<void(const juce::String &, bool)> callback) {
  onFavoriteToggled = std::move(callback);
}

void WidgetLibraryPanel::paint(juce::Graphics &g) {
  g.fillAll(palette(GyeolPalette::PanelBackground));
  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
}

void WidgetLibraryPanel::resized() {
  auto bounds = getLocalBounds().reduced(8);
  titleLabel.setBounds(bounds.removeFromTop(20));

  auto topRow = bounds.removeFromTop(24);
  categoryBox.setBounds(topRow.removeFromLeft(160));
  topRow.removeFromLeft(8);
  favoritesOnlyToggle.setBounds(topRow);

  bounds.removeFromTop(4);
  searchBox.setBounds(bounds.removeFromTop(24));
  bounds.removeFromTop(6);

  bounds.removeFromBottom(4); // ?쎄컙???섎떒 ?щ갚

  viewport.setBounds(bounds);
  if (contentComp != nullptr) {
    updateGridLayout();
  }
}

void WidgetLibraryPanel::updateGridLayout() {
  if (contentComp == nullptr)
    return;
  const int viewportWidth = viewport.getMaximumVisibleWidth();
  const int padding = 12;
  const int itemWidth = 100; // 媛濡??ш린 怨좎젙 (100px)
  const int numColumns =
      std::max(1, (viewportWidth - padding) / (itemWidth + padding));
  const int dynamicPadding =
      (viewportWidth - (numColumns * itemWidth)) / (numColumns + 1);
  const int itemHeight = 110; // ?몃줈 ?ш린 怨좎젙 (110px)

  int row = 0;
  int col = 0;

  for (int i = 0; i < contentComp->getNumChildComponents(); ++i) {
    auto *child = contentComp->getChildComponent(i);
    child->setBounds(dynamicPadding + col * (itemWidth + dynamicPadding),
                     padding + row * (itemHeight + padding), itemWidth,
                     itemHeight);
    col++;
    if (col >= numColumns) {
      col = 0;
      row++;
    }
  }

  int totalRows =
      (contentComp->getNumChildComponents() + numColumns - 1) / numColumns;
  contentComp->setSize(viewportWidth,
                       padding + totalRows * (itemHeight + padding));
}

void WidgetLibraryPanel::rebuildCategories() {
  const auto current = categoryBox.getText();
  categoryBox.clear(juce::dontSendNotification);

  juce::StringArray categories;
  categories.add("All");
  categories.add("Input");
  categories.add("Control");
  categories.add("Display");
  categories.add("Text");
  categories.add("Other");

  for (const auto *descriptor : registry.listDescriptors()) {
    if (descriptor == nullptr)
      continue;
    const auto normalized = normalizeCategory(descriptor->category);
    if (!categories.contains(normalized))
      categories.add(normalized);
  }

  int id = 1;
  for (const auto &category : categories)
    categoryBox.addItem(category, id++);

  if (current.isNotEmpty() && categories.contains(current))
    categoryBox.setText(current, juce::dontSendNotification);
  else
    categoryBox.setSelectedId(1, juce::dontSendNotification);
}

void WidgetLibraryPanel::rebuildVisibleItems() {
  Widgets::LibraryFilter filter;
  filter.query = searchBox.getText();
  filter.category = categoryBox.getText();
  filter.includeFavoritesOnly = favoritesOnlyToggle.getToggleState();
  filter.favoriteTypeKeys = favoriteTypeKeys;

  const auto descriptors = registry.findByFilter(filter);

  visibleItems.clear();
  visibleItems.reserve(descriptors.size());
  for (const auto *descriptor : descriptors) {
    if (descriptor == nullptr)
      continue;

    DisplayItem item;
    item.descriptor = descriptor;
    item.favorite = isFavorite(descriptor->typeKey);
    item.recentRank = recentRankFor(descriptor->typeKey);
    visibleItems.push_back(std::move(item));
  }

  std::sort(visibleItems.begin(), visibleItems.end(),
            [](const DisplayItem &lhs, const DisplayItem &rhs) {
              if (lhs.favorite != rhs.favorite)
                return lhs.favorite > rhs.favorite;

              const auto lhsRecent = lhs.recentRank >= 0;
              const auto rhsRecent = rhs.recentRank >= 0;
              if (lhsRecent != rhsRecent)
                return lhsRecent > rhsRecent;
              if (lhsRecent && rhsRecent && lhs.recentRank != rhs.recentRank)
                return lhs.recentRank < rhs.recentRank;

              const auto lhsCategory = normalizeCategory(
                  lhs.descriptor != nullptr ? lhs.descriptor->category
                                            : juce::String());
              const auto rhsCategory = normalizeCategory(
                  rhs.descriptor != nullptr ? rhs.descriptor->category
                                            : juce::String());
              if (lhsCategory != rhsCategory)
                return lhsCategory < rhsCategory;

              const auto lhsName = lhs.descriptor != nullptr
                                       ? lhs.descriptor->displayName
                                       : juce::String();
              const auto rhsName = rhs.descriptor != nullptr
                                       ? rhs.descriptor->displayName
                                       : juce::String();
              if (lhsName != rhsName)
                return lhsName < rhsName;

              const auto lhsType = lhs.descriptor != nullptr
                                       ? lhs.descriptor->typeKey
                                       : juce::String();
              const auto rhsType = rhs.descriptor != nullptr
                                       ? rhs.descriptor->typeKey
                                       : juce::String();
              return lhsType < rhsType;
            });

  contentComp->deleteAllChildren();

  for (size_t i = 0; i < visibleItems.size(); ++i) {
    auto *card = new CardComponent(*this);
    card->setItemData(static_cast<int>(i), visibleItems[i]);
    contentComp->addAndMakeVisible(card);
  }

  updateGridLayout();
  repaint();
}

void WidgetLibraryPanel::triggerCreateForRow(int row) {
  if (row < 0 || row >= static_cast<int>(visibleItems.size()))
    return;

  const auto &item = visibleItems[static_cast<size_t>(row)];
  if (item.descriptor == nullptr || item.descriptor->typeKey.isEmpty())
    return;

  if (onCreateRequested != nullptr)
    onCreateRequested(item.descriptor->typeKey);
  else
    updateRecentByTypeKey(item.descriptor->typeKey);
}

void WidgetLibraryPanel::startDragForRow(int row,
                                         juce::Component &sourceComponent,
                                         juce::Point<int> dragStartPos) {
  if (row < 0 || row >= static_cast<int>(visibleItems.size()))
    return;

  const auto &item = visibleItems[static_cast<size_t>(row)];
  if (item.descriptor == nullptr || item.descriptor->typeKey.isEmpty())
    return;

  auto payload = std::make_unique<juce::DynamicObject>();
  payload->setProperty("kind", "widgetLibraryType");
  payload->setProperty("typeKey", item.descriptor->typeKey);
  payload->setProperty("displayName", item.descriptor->displayName);
  payload->setProperty("source", "widgetLibrary");

  const juce::ScaledImage dragImage(sourceComponent.createComponentSnapshot(
      sourceComponent.getLocalBounds()));
  startDragging(juce::var(payload.release()), &sourceComponent, dragImage, true,
                &dragStartPos);
}

void WidgetLibraryPanel::toggleFavoriteForRow(int row, bool favorite) {
  if (row < 0 || row >= static_cast<int>(visibleItems.size()))
    return;
  const auto &item = visibleItems[static_cast<size_t>(row)];
  if (item.descriptor == nullptr)
    return;

  toggleFavoriteByTypeKey(item.descriptor->typeKey, favorite);
}

void WidgetLibraryPanel::toggleFavoriteByTypeKey(const juce::String &typeKey,
                                                 bool favorite) {
  if (typeKey.isEmpty())
    return;

  if (favorite) {
    if (!favoriteTypeKeys.contains(typeKey))
      favoriteTypeKeys.add(typeKey);
  } else {
    favoriteTypeKeys.removeString(typeKey, false);
  }

  saveSettings();

  if (onFavoriteToggled != nullptr)
    onFavoriteToggled(typeKey, favorite);

  rebuildVisibleItems();
}

void WidgetLibraryPanel::updateRecentByTypeKey(const juce::String &typeKey) {
  if (typeKey.isEmpty())
    return;

  recentTypeKeys.removeString(typeKey, false);
  recentTypeKeys.insert(0, typeKey);
  while (recentTypeKeys.size() > maxRecentCount)
    recentTypeKeys.remove(recentTypeKeys.size() - 1);

  saveSettings();
  rebuildVisibleItems();
}

int WidgetLibraryPanel::recentRankFor(const juce::String &typeKey) const {
  for (int i = 0; i < recentTypeKeys.size(); ++i) {
    if (recentTypeKeys[i] == typeKey)
      return i;
  }

  return -1;
}

bool WidgetLibraryPanel::isFavorite(const juce::String &typeKey) const {
  return favoriteTypeKeys.contains(typeKey);
}

void WidgetLibraryPanel::loadSettings() {
  if (settingsFile == nullptr)
    return;

  favoriteTypeKeys =
      fromJsonArrayString(settingsFile->getValue("widgetLibrary.favorites"));
  recentTypeKeys =
      fromJsonArrayString(settingsFile->getValue("widgetLibrary.recents"));
}

void WidgetLibraryPanel::saveSettings() {
  if (settingsFile == nullptr)
    return;

  settingsFile->setValue("widgetLibrary.favorites",
                         toJsonArrayString(favoriteTypeKeys));
  settingsFile->setValue("widgetLibrary.recents",
                         toJsonArrayString(recentTypeKeys));
  settingsFile->saveIfNeeded();
}

juce::String
WidgetLibraryPanel::toJsonArrayString(const juce::StringArray &values) {
  juce::Array<juce::var> jsonArray;
  for (const auto &value : values) {
    if (value.isNotEmpty())
      jsonArray.add(value);
  }

  return juce::JSON::toString(juce::var(jsonArray), false);
}

juce::StringArray
WidgetLibraryPanel::fromJsonArrayString(const juce::String &serialized) {
  juce::StringArray parsedValues;
  const auto parsed = juce::JSON::parse(serialized);
  if (!parsed.isArray())
    return parsedValues;

  const auto *array = parsed.getArray();
  if (array == nullptr)
    return parsedValues;

  for (const auto &value : *array) {
    if (!value.isString())
      continue;
    const auto text = value.toString().trim();
    if (text.isNotEmpty() && !parsedValues.contains(text))
      parsedValues.add(text);
  }

  return parsedValues;
}

juce::String
WidgetLibraryPanel::normalizeCategory(const juce::String &category) {
  const auto trimmed = category.trim();
  if (trimmed.isEmpty())
    return "Other";
  return trimmed;
}

juce::String WidgetLibraryPanel::iconGlyphForDescriptor(
    const Widgets::WidgetDescriptor &descriptor) {
  if (descriptor.iconKey.isNotEmpty()) {
    const auto key = descriptor.iconKey.toLowerCase();
    if (key.contains("slider"))
      return "S";
    if (key.contains("knob"))
      return "K";
    if (key.contains("meter"))
      return "M";
    if (key.contains("toggle"))
      return "T";
    if (key.contains("combo"))
      return "C";
    if (key.contains("text"))
      return "Tx";
    if (key.contains("label"))
      return "Lb";
    if (key.contains("button"))
      return "B";
  }

  const auto normalizedCategory =
      normalizeCategory(descriptor.category).toLowerCase();
  if (normalizedCategory == "input")
    return "I";
  if (normalizedCategory == "display")
    return "D";
  if (normalizedCategory == "text")
    return "Tx";
  if (normalizedCategory == "control")
    return "C";
  return "W";
}

juce::Colour WidgetLibraryPanel::iconColorForDescriptor(
    const Widgets::WidgetDescriptor &descriptor) {
  const auto normalizedCategory =
      normalizeCategory(descriptor.category).toLowerCase();
  if (normalizedCategory == "input")
    return palette(GyeolPalette::AccentPrimary);
  if (normalizedCategory == "display")
    return palette(GyeolPalette::ValidSuccess);
  if (normalizedCategory == "text")
    return palette(GyeolPalette::ValidWarning);
  if (normalizedCategory == "control")
    return palette(GyeolPalette::AccentHover);
  return palette(GyeolPalette::TextDisabled);
}
} // namespace Gyeol::Ui::Panels
