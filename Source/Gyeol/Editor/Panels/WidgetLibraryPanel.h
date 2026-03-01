#pragma once

#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class WidgetLibraryPanel : public juce::Component,
                               public juce::DragAndDropContainer,
                               private juce::ListBoxModel
    {
    public:
        explicit WidgetLibraryPanel(const Widgets::WidgetRegistry& registryIn);
        ~WidgetLibraryPanel() override;

        void refreshFromRegistry();
        void noteWidgetCreated(const juce::String& typeKey);

        void setCreateRequestedCallback(std::function<void(const juce::String&)> callback);
        void setFavoriteToggledCallback(std::function<void(const juce::String&, bool)> callback);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        struct DisplayItem
        {
            const Widgets::WidgetDescriptor* descriptor = nullptr;
            bool favorite = false;
            int recentRank = -1;
        };

        class RowComponent;

        int getNumRows() override;
        void paintListBoxItem(int rowNumber,
                              juce::Graphics& g,
                              int width,
                              int height,
                              bool rowIsSelected) override;
        juce::Component* refreshComponentForRow(int rowNumber,
                                                bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override;
        void selectedRowsChanged(int lastRowSelected) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

        void rebuildCategories();
        void rebuildVisibleItems();
        void triggerCreateForRow(int row);
        void startDragForRow(int row, juce::Component& sourceComponent, juce::Point<int> dragStartPos);
        void toggleFavoriteForRow(int row, bool favorite);
        void toggleFavoriteByTypeKey(const juce::String& typeKey, bool favorite);
        void updateRecentByTypeKey(const juce::String& typeKey);
        int recentRankFor(const juce::String& typeKey) const;
        bool isFavorite(const juce::String& typeKey) const;

        void loadSettings();
        void saveSettings();
        static juce::String toJsonArrayString(const juce::StringArray& values);
        static juce::StringArray fromJsonArrayString(const juce::String& serialized);

        static juce::String normalizeCategory(const juce::String& category);
        static juce::String iconGlyphForDescriptor(const Widgets::WidgetDescriptor& descriptor);
        static juce::Colour iconColorForDescriptor(const Widgets::WidgetDescriptor& descriptor);

        const Widgets::WidgetRegistry& registry;

        juce::Label titleLabel;
        juce::ComboBox categoryBox;
        juce::TextEditor searchBox;
        juce::ToggleButton favoritesOnlyToggle { "Favorites" };
        juce::ListBox listBox;
        juce::TextButton addSelectedButton { "Add Selected" };

        std::vector<DisplayItem> visibleItems;
        juce::StringArray recentTypeKeys;
        juce::StringArray favoriteTypeKeys;
        std::unique_ptr<juce::PropertiesFile> settingsFile;

        std::function<void(const juce::String&)> onCreateRequested;
        std::function<void(const juce::String&, bool)> onFavoriteToggled;

        static constexpr int maxRecentCount = 12;
    };
}
