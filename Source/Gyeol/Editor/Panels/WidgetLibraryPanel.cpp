#include "Gyeol/Editor/Panels/WidgetLibraryPanel.h"

#include <algorithm>
#include <array>

namespace Gyeol::Ui::Panels
{
    class WidgetLibraryPanel::RowComponent final : public juce::Component
    {
    public:
        explicit RowComponent(WidgetLibraryPanel& ownerIn)
            : owner(ownerIn)
        {
            iconLabel.setJustificationType(juce::Justification::centred);
            iconLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            iconLabel.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(iconLabel);

            nameLabel.setJustificationType(juce::Justification::centredLeft);
            nameLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            nameLabel.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(nameLabel);

            typeLabel.setJustificationType(juce::Justification::centredLeft);
            typeLabel.setFont(juce::FontOptions(10.5f));
            typeLabel.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(typeLabel);

            recentLabel.setText("Recent", juce::dontSendNotification);
            recentLabel.setJustificationType(juce::Justification::centredRight);
            recentLabel.setFont(juce::FontOptions(10.0f));
            recentLabel.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(recentLabel);

            favoriteToggle.setButtonText("F");
            favoriteToggle.setClickingTogglesState(true);
            favoriteToggle.onClick = [this]
            {
                if (suppressFavoriteCallback)
                    return;
                owner.toggleFavoriteForRow(rowIndex, favoriteToggle.getToggleState());
            };
            addAndMakeVisible(favoriteToggle);

            addButton.setButtonText("+");
            addButton.onClick = [this]
            {
                owner.triggerCreateForRow(rowIndex);
            };
            addAndMakeVisible(addButton);
        }

        void setRowData(int row,
                        const DisplayItem& item,
                        bool selected)
        {
            rowIndex = row;
            currentTypeKey = item.descriptor != nullptr ? item.descriptor->typeKey : juce::String();
            rowSelected = selected;
            iconColor = item.descriptor != nullptr
                          ? WidgetLibraryPanel::iconColorForDescriptor(*item.descriptor)
                          : juce::Colour::fromRGB(86, 96, 116);

            const auto displayName = item.descriptor != nullptr
                                       ? (item.descriptor->displayName.isNotEmpty() ? item.descriptor->displayName : item.descriptor->typeKey)
                                       : juce::String("Widget");
            nameLabel.setText(displayName, juce::dontSendNotification);
            typeLabel.setText(item.descriptor != nullptr ? item.descriptor->typeKey : juce::String(), juce::dontSendNotification);
            iconLabel.setText(item.descriptor != nullptr ? WidgetLibraryPanel::iconGlyphForDescriptor(*item.descriptor) : juce::String("W"),
                              juce::dontSendNotification);
            recentLabel.setVisible(item.recentRank >= 0);

            suppressFavoriteCallback = true;
            favoriteToggle.setToggleState(item.favorite, juce::dontSendNotification);
            suppressFavoriteCallback = false;

            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            const auto area = getLocalBounds().toFloat();
            const auto fill = rowSelected ? juce::Colour::fromRGB(50, 86, 150)
                                          : juce::Colour::fromRGB(27, 33, 43);
            g.setColour(fill.withAlpha(rowSelected ? 0.85f : 0.55f));
            g.fillRoundedRectangle(area.reduced(1.0f), 4.0f);

            g.setColour(juce::Colour::fromRGB(54, 66, 84));
            g.drawRoundedRectangle(area.reduced(1.0f), 4.0f, 1.0f);

            const auto iconBounds = juce::Rectangle<float>(8.0f, 8.0f, 26.0f, 26.0f);
            g.setColour(iconColor.withAlpha(0.9f));
            g.fillRoundedRectangle(iconBounds, 4.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(6, 4);
            iconLabel.setBounds(area.removeFromLeft(30).reduced(4, 2));
            area.removeFromLeft(4);

            auto controls = area.removeFromRight(76);
            favoriteToggle.setBounds(controls.removeFromLeft(30));
            controls.removeFromLeft(4);
            addButton.setBounds(controls.removeFromLeft(30));

            auto top = area.removeFromTop(18);
            auto recentArea = top.removeFromRight(52);
            nameLabel.setBounds(top);
            recentLabel.setBounds(recentArea);
            typeLabel.setBounds(area.removeFromTop(14));
        }

        void mouseDoubleClick(const juce::MouseEvent&) override
        {
            owner.triggerCreateForRow(rowIndex);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            dragStartPoint = event.getPosition();
            dragStarted = false;
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (dragStarted)
                return;

            if (event.getDistanceFromDragStart() < 4)
                return;

            dragStarted = true;
            owner.startDragForRow(rowIndex, *this, dragStartPoint);
        }

    private:
        WidgetLibraryPanel& owner;
        int rowIndex = -1;
        bool rowSelected = false;
        bool suppressFavoriteCallback = false;
        bool dragStarted = false;
        juce::Point<int> dragStartPoint;
        juce::String currentTypeKey;
        juce::Colour iconColor { juce::Colour::fromRGB(86, 96, 116) };

        juce::Label iconLabel;
        juce::Label nameLabel;
        juce::Label typeLabel;
        juce::Label recentLabel;
        juce::ToggleButton favoriteToggle;
        juce::TextButton addButton;
    };

    WidgetLibraryPanel::WidgetLibraryPanel(const Widgets::WidgetRegistry& registryIn)
        : registry(registryIn)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "DadeumStudio";
        options.folderName = "DadeumStudio";
        options.filenameSuffix = "settings";
        options.osxLibrarySubFolder = "Application Support";
        settingsFile = std::make_unique<juce::PropertiesFile>(options);
        loadSettings();

        titleLabel.setText("Widget Library", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(192, 200, 214));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        addAndMakeVisible(categoryBox);
        categoryBox.onChange = [this]
        {
            rebuildVisibleItems();
        };

        searchBox.setTextToShowWhenEmpty("Search widgets...", juce::Colour::fromRGB(126, 136, 152));
        searchBox.onTextChange = [this]
        {
            rebuildVisibleItems();
        };
        addAndMakeVisible(searchBox);

        favoritesOnlyToggle.setClickingTogglesState(true);
        favoritesOnlyToggle.onClick = [this]
        {
            rebuildVisibleItems();
        };
        addAndMakeVisible(favoritesOnlyToggle);

        listBox.setModel(this);
        listBox.setRowHeight(42);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
        listBox.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
        addAndMakeVisible(listBox);

        addSelectedButton.onClick = [this]
        {
            triggerCreateForRow(listBox.getSelectedRow());
        };
        addAndMakeVisible(addSelectedButton);

        rebuildCategories();
        rebuildVisibleItems();
    }

    WidgetLibraryPanel::~WidgetLibraryPanel()
    {
        saveSettings();
        listBox.setModel(nullptr);
    }

    void WidgetLibraryPanel::refreshFromRegistry()
    {
        rebuildCategories();
        rebuildVisibleItems();
    }

    void WidgetLibraryPanel::noteWidgetCreated(const juce::String& typeKey)
    {
        if (typeKey.isEmpty())
            return;
        updateRecentByTypeKey(typeKey);
    }

    void WidgetLibraryPanel::setCreateRequestedCallback(std::function<void(const juce::String&)> callback)
    {
        onCreateRequested = std::move(callback);
    }

    void WidgetLibraryPanel::setFavoriteToggledCallback(std::function<void(const juce::String&, bool)> callback)
    {
        onFavoriteToggled = std::move(callback);
    }

    void WidgetLibraryPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(40, 46, 56));
        g.drawRect(getLocalBounds(), 1);
    }

    void WidgetLibraryPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(8);
        titleLabel.setBounds(bounds.removeFromTop(20));

        auto topRow = bounds.removeFromTop(24);
        categoryBox.setBounds(topRow.removeFromLeft(160));
        topRow.removeFromLeft(8);
        favoritesOnlyToggle.setBounds(topRow);

        bounds.removeFromTop(4);
        searchBox.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(6);

        addSelectedButton.setBounds(bounds.removeFromBottom(24));
        bounds.removeFromBottom(4);
        listBox.setBounds(bounds);
    }

    int WidgetLibraryPanel::getNumRows()
    {
        return static_cast<int>(visibleItems.size());
    }

    void WidgetLibraryPanel::paintListBoxItem(int,
                                              juce::Graphics&,
                                              int,
                                              int,
                                              bool)
    {
        // Row components handle full painting.
    }

    juce::Component* WidgetLibraryPanel::refreshComponentForRow(int rowNumber,
                                                                bool isRowSelected,
                                                                juce::Component* existingComponentToUpdate)
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(visibleItems.size()))
            return nullptr;

        auto* rowComponent = dynamic_cast<RowComponent*>(existingComponentToUpdate);
        if (rowComponent == nullptr)
            rowComponent = new RowComponent(*this);

        rowComponent->setRowData(rowNumber, visibleItems[static_cast<size_t>(rowNumber)], isRowSelected);
        return rowComponent;
    }

    void WidgetLibraryPanel::selectedRowsChanged(int)
    {
        addSelectedButton.setEnabled(listBox.getSelectedRow() >= 0);
    }

    void WidgetLibraryPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
    {
        triggerCreateForRow(row);
    }

    void WidgetLibraryPanel::rebuildCategories()
    {
        const auto current = categoryBox.getText();
        categoryBox.clear(juce::dontSendNotification);

        juce::StringArray categories;
        categories.add("All");
        categories.add("Input");
        categories.add("Control");
        categories.add("Display");
        categories.add("Text");
        categories.add("Other");

        for (const auto* descriptor : registry.listDescriptors())
        {
            if (descriptor == nullptr)
                continue;
            const auto normalized = normalizeCategory(descriptor->category);
            if (!categories.contains(normalized))
                categories.add(normalized);
        }

        int id = 1;
        for (const auto& category : categories)
            categoryBox.addItem(category, id++);

        if (current.isNotEmpty() && categories.contains(current))
            categoryBox.setText(current, juce::dontSendNotification);
        else
            categoryBox.setSelectedId(1, juce::dontSendNotification);
    }

    void WidgetLibraryPanel::rebuildVisibleItems()
    {
        Widgets::LibraryFilter filter;
        filter.query = searchBox.getText();
        filter.category = categoryBox.getText();
        filter.includeFavoritesOnly = favoritesOnlyToggle.getToggleState();
        filter.favoriteTypeKeys = favoriteTypeKeys;

        const auto descriptors = registry.findByFilter(filter);

        visibleItems.clear();
        visibleItems.reserve(descriptors.size());
        for (const auto* descriptor : descriptors)
        {
            if (descriptor == nullptr)
                continue;

            DisplayItem item;
            item.descriptor = descriptor;
            item.favorite = isFavorite(descriptor->typeKey);
            item.recentRank = recentRankFor(descriptor->typeKey);
            visibleItems.push_back(std::move(item));
        }

        std::sort(visibleItems.begin(),
                  visibleItems.end(),
                  [](const DisplayItem& lhs, const DisplayItem& rhs)
                  {
                      if (lhs.favorite != rhs.favorite)
                          return lhs.favorite > rhs.favorite;

                      const auto lhsRecent = lhs.recentRank >= 0;
                      const auto rhsRecent = rhs.recentRank >= 0;
                      if (lhsRecent != rhsRecent)
                          return lhsRecent > rhsRecent;
                      if (lhsRecent && rhsRecent && lhs.recentRank != rhs.recentRank)
                          return lhs.recentRank < rhs.recentRank;

                      const auto lhsCategory = normalizeCategory(lhs.descriptor != nullptr ? lhs.descriptor->category : juce::String());
                      const auto rhsCategory = normalizeCategory(rhs.descriptor != nullptr ? rhs.descriptor->category : juce::String());
                      if (lhsCategory != rhsCategory)
                          return lhsCategory < rhsCategory;

                      const auto lhsName = lhs.descriptor != nullptr ? lhs.descriptor->displayName : juce::String();
                      const auto rhsName = rhs.descriptor != nullptr ? rhs.descriptor->displayName : juce::String();
                      if (lhsName != rhsName)
                          return lhsName < rhsName;

                      const auto lhsType = lhs.descriptor != nullptr ? lhs.descriptor->typeKey : juce::String();
                      const auto rhsType = rhs.descriptor != nullptr ? rhs.descriptor->typeKey : juce::String();
                      return lhsType < rhsType;
                  });

        listBox.updateContent();
        if (visibleItems.empty())
            listBox.deselectAllRows();
        else if (listBox.getSelectedRow() < 0)
            listBox.selectRow(0);
        addSelectedButton.setEnabled(listBox.getSelectedRow() >= 0);
        listBox.repaint();
    }

    void WidgetLibraryPanel::triggerCreateForRow(int row)
    {
        if (row < 0 || row >= static_cast<int>(visibleItems.size()))
            return;

        const auto& item = visibleItems[static_cast<size_t>(row)];
        if (item.descriptor == nullptr || item.descriptor->typeKey.isEmpty())
            return;

        if (onCreateRequested != nullptr)
            onCreateRequested(item.descriptor->typeKey);
        else
            updateRecentByTypeKey(item.descriptor->typeKey);
    }

    void WidgetLibraryPanel::startDragForRow(int row, juce::Component& sourceComponent, juce::Point<int> dragStartPos)
    {
        if (row < 0 || row >= static_cast<int>(visibleItems.size()))
            return;

        const auto& item = visibleItems[static_cast<size_t>(row)];
        if (item.descriptor == nullptr || item.descriptor->typeKey.isEmpty())
            return;

        auto payload = std::make_unique<juce::DynamicObject>();
        payload->setProperty("kind", "widgetLibraryType");
        payload->setProperty("typeKey", item.descriptor->typeKey);
        payload->setProperty("displayName", item.descriptor->displayName);
        payload->setProperty("source", "widgetLibrary");

        const juce::ScaledImage dragImage(sourceComponent.createComponentSnapshot(sourceComponent.getLocalBounds()));
        startDragging(juce::var(payload.release()),
                      &sourceComponent,
                      dragImage,
                      true,
                      &dragStartPos);
    }

    void WidgetLibraryPanel::toggleFavoriteForRow(int row, bool favorite)
    {
        if (row < 0 || row >= static_cast<int>(visibleItems.size()))
            return;
        const auto& item = visibleItems[static_cast<size_t>(row)];
        if (item.descriptor == nullptr)
            return;

        toggleFavoriteByTypeKey(item.descriptor->typeKey, favorite);
    }

    void WidgetLibraryPanel::toggleFavoriteByTypeKey(const juce::String& typeKey, bool favorite)
    {
        if (typeKey.isEmpty())
            return;

        if (favorite)
        {
            if (!favoriteTypeKeys.contains(typeKey))
                favoriteTypeKeys.add(typeKey);
        }
        else
        {
            favoriteTypeKeys.removeString(typeKey, false);
        }

        saveSettings();

        if (onFavoriteToggled != nullptr)
            onFavoriteToggled(typeKey, favorite);

        rebuildVisibleItems();
    }

    void WidgetLibraryPanel::updateRecentByTypeKey(const juce::String& typeKey)
    {
        if (typeKey.isEmpty())
            return;

        recentTypeKeys.removeString(typeKey, false);
        recentTypeKeys.insert(0, typeKey);
        while (recentTypeKeys.size() > maxRecentCount)
            recentTypeKeys.remove(recentTypeKeys.size() - 1);

        saveSettings();
        rebuildVisibleItems();
    }

    int WidgetLibraryPanel::recentRankFor(const juce::String& typeKey) const
    {
        for (int i = 0; i < recentTypeKeys.size(); ++i)
        {
            if (recentTypeKeys[i] == typeKey)
                return i;
        }

        return -1;
    }

    bool WidgetLibraryPanel::isFavorite(const juce::String& typeKey) const
    {
        return favoriteTypeKeys.contains(typeKey);
    }

    void WidgetLibraryPanel::loadSettings()
    {
        if (settingsFile == nullptr)
            return;

        favoriteTypeKeys = fromJsonArrayString(settingsFile->getValue("widgetLibrary.favorites"));
        recentTypeKeys = fromJsonArrayString(settingsFile->getValue("widgetLibrary.recents"));
    }

    void WidgetLibraryPanel::saveSettings()
    {
        if (settingsFile == nullptr)
            return;

        settingsFile->setValue("widgetLibrary.favorites", toJsonArrayString(favoriteTypeKeys));
        settingsFile->setValue("widgetLibrary.recents", toJsonArrayString(recentTypeKeys));
        settingsFile->saveIfNeeded();
    }

    juce::String WidgetLibraryPanel::toJsonArrayString(const juce::StringArray& values)
    {
        juce::Array<juce::var> jsonArray;
        for (const auto& value : values)
        {
            if (value.isNotEmpty())
                jsonArray.add(value);
        }

        return juce::JSON::toString(juce::var(jsonArray), false);
    }

    juce::StringArray WidgetLibraryPanel::fromJsonArrayString(const juce::String& serialized)
    {
        juce::StringArray parsedValues;
        const auto parsed = juce::JSON::parse(serialized);
        if (!parsed.isArray())
            return parsedValues;

        const auto* array = parsed.getArray();
        if (array == nullptr)
            return parsedValues;

        for (const auto& value : *array)
        {
            if (!value.isString())
                continue;
            const auto text = value.toString().trim();
            if (text.isNotEmpty() && !parsedValues.contains(text))
                parsedValues.add(text);
        }

        return parsedValues;
    }

    juce::String WidgetLibraryPanel::normalizeCategory(const juce::String& category)
    {
        const auto trimmed = category.trim();
        if (trimmed.isEmpty())
            return "Other";
        return trimmed;
    }

    juce::String WidgetLibraryPanel::iconGlyphForDescriptor(const Widgets::WidgetDescriptor& descriptor)
    {
        if (descriptor.iconKey.isNotEmpty())
        {
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

        const auto normalizedCategory = normalizeCategory(descriptor.category).toLowerCase();
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

    juce::Colour WidgetLibraryPanel::iconColorForDescriptor(const Widgets::WidgetDescriptor& descriptor)
    {
        const auto normalizedCategory = normalizeCategory(descriptor.category).toLowerCase();
        if (normalizedCategory == "input")
            return juce::Colour::fromRGB(72, 154, 236);
        if (normalizedCategory == "display")
            return juce::Colour::fromRGB(80, 198, 145);
        if (normalizedCategory == "text")
            return juce::Colour::fromRGB(218, 156, 90);
        if (normalizedCategory == "control")
            return juce::Colour::fromRGB(154, 132, 234);
        return juce::Colour::fromRGB(108, 122, 148);
    }
}
