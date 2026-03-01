#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class AssetsPanel : public juce::Component,
                        public juce::DragAndDropContainer,
                        public juce::FileDragAndDropTarget,
                        private juce::ListBoxModel,
                        private juce::Timer
    {
    public:
        AssetsPanel(DocumentHandle& documentIn, const Widgets::WidgetFactory& widgetFactoryIn);
        ~AssetsPanel() override;

        void refreshFromDocument();
        void setAssetsChangedCallback(std::function<void(const juce::String&)> callback);
        void setAssetUsageNavigateCallback(std::function<void(WidgetId)> callback);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        class RowComponent;
        class UsageListModel;
        enum class ImportConflictPolicy
        {
            rename,
            overwrite,
            skip
        };

        struct AssetUsageEntry
        {
            WidgetId widgetId = kRootId;
            juce::Identifier propertyKey;
            juce::String propertyLabel;
            juce::String widgetLabel;
            juce::String contextLabel;
            bool runtimePatch = false;
        };

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
        void listBoxItemClicked(int row, const juce::MouseEvent& event) override;
        void timerCallback() override;

        bool isInterestedInFileDrag(const juce::StringArray& files) override;
        void fileDragEnter(const juce::StringArray& files, int x, int y) override;
        void fileDragMove(const juce::StringArray& files, int x, int y) override;
        void fileDragExit(const juce::StringArray& files) override;
        void filesDropped(const juce::StringArray& files, int x, int y) override;

        int selectedModelIndex() const;
        const AssetModel* selectedAsset() const;

        void rebuildVisibleAssets();
        void rebuildUsageIndex();
        void refreshSelectedAssetUsageList();
        void activateUsageEntryAtRow(int row);
        int usageCountForAsset(WidgetId assetId) const;
        bool hasUnusedAssets() const;
        bool commitAssets(const juce::String& reason);
        void updateButtons();
        void setStatus(const juce::String& text, juce::Colour colour);

        void addFileAsset();
        bool addFilesAsAssets(const juce::StringArray& files,
                              int* importedCount = nullptr,
                              int* skippedCount = nullptr);
        void addColorAsset();
        void importAssetPackage();
        void exportAssetPackage();
        void reimportSelectedAsset();
        void replaceSelectedAssetFile();
        void removeUnusedAssets();
        void mergeDuplicateAssets();
        void relinkMissingAssets();
        void deleteSelectedAsset();
        void copySelectedRefKey();
        void applyRefKeyEdit();
        void syncRefEditorFromSelection();
        void syncExportIncludeToggleFromSelection();
        void applyExportIncludeToggle();

        WidgetId allocateNextAssetId() const;
        juce::String makeUniqueRefKey(const juce::String& seed, WidgetId ignoreAssetId = kRootId) const;
        juce::File resolveProjectRootDirectory() const;
        juce::File resolveInputFilePath(const juce::String& value) const;
        juce::Image getImageThumbnailForAsset(const AssetModel& asset) const;
        static bool isAudioAsset(const AssetModel& asset);
        bool isAssetPreviewPlaying(WidgetId assetId) const;
        void toggleAudioPreviewForAsset(WidgetId assetId);
        void startAudioPreviewForAsset(const AssetModel& asset);
        void stopAudioPreview();
        void startDragForRow(int row, juce::Component& sourceComponent, juce::Point<int> dragStartPos);

        static juce::String sanitizeRefToken(const juce::String& text);
        static juce::String normalizeRelativePath(const juce::String& value);
        static juce::var propertyBagToVar(const PropertyBag& bag);
        static juce::Result varToPropertyBag(const juce::var& value, PropertyBag& outBag);
        static AssetKind inferAssetKindFromFile(const juce::File& file);
        static juce::String inferMimeTypeFromFile(const juce::File& file);
        static juce::String kindLabel(AssetKind kind);
        static juce::Colour kindColor(AssetKind kind);
        static juce::String resolveRelativePath(const juce::File& file);
        static bool isImportableFile(const juce::File& file);
        static bool isAssetExcludedFromExport(const AssetModel& asset);
        static juce::String fingerprintForFile(const juce::File& file);
        ImportConflictPolicy selectedImportConflictPolicy() const;

        DocumentHandle& document;
        const Widgets::WidgetFactory& widgetFactory;
        std::vector<AssetModel> assets;
        std::vector<int> visibleAssetIndices;
        std::unordered_map<WidgetId, std::vector<AssetUsageEntry>> usageByAssetId;
        std::unordered_map<WidgetId, int> usageCountByAssetId;
        std::vector<AssetUsageEntry> selectedAssetUsageEntries;
        mutable std::map<juce::String, juce::Image> thumbnailCache;
        WidgetId selectedAssetId = kRootId;
        bool fileDragHovering = false;
        bool audioPreviewAvailable = false;
        WidgetId previewAssetId = kRootId;

        std::function<void(const juce::String&)> onAssetsChanged;
        std::function<void(WidgetId)> onAssetUsageNavigate;

        juce::Label titleLabel;
        juce::ComboBox kindFilterCombo;
        juce::ComboBox importConflictCombo;
        juce::TextEditor searchEditor;
        juce::TextButton cleanupUnusedButton { "Clean Unused" };
        juce::TextButton mergeDuplicatesButton { "Merge Dups" };
        juce::TextButton relinkMissingButton { "Relink Missing" };
        juce::TextButton addFileButton { "+ File" };
        juce::TextButton addColorButton { "+ Color" };
        juce::TextButton importPackageButton { "Import Pkg" };
        juce::TextButton exportPackageButton { "Export Pkg" };
        juce::TextButton reimportButton { "Reimport" };
        juce::TextButton replaceAssetButton { "Replace" };
        juce::TextButton deleteButton { "Delete" };
        juce::ToggleButton exportIncludeToggle { "Include In Export" };
        juce::TextEditor refKeyEditor;
        juce::TextButton applyRefButton { "Apply Ref" };
        juce::TextButton copyRefButton { "Copy Ref" };
        juce::ListBox listBox;
        juce::Label usageTitleLabel;
        std::unique_ptr<UsageListModel> usageListModel;
        juce::ListBox usageList;
        juce::Label statusLabel;
        juce::AudioFormatManager audioFormatManager;
        juce::AudioTransportSource audioTransportSource;
        juce::AudioSourcePlayer audioSourcePlayer;
        std::unique_ptr<juce::AudioFormatReaderSource> audioReaderSource;
        juce::AudioDeviceManager audioDeviceManager;
        std::unique_ptr<juce::FileChooser> pendingFileChooser;
    };
}
