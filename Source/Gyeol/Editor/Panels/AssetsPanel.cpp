#include "Gyeol/Editor/Panels/AssetsPanel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        const auto kPanelBg = juce::Colour::fromRGB(24, 28, 34);
        const auto kPanelOutline = juce::Colour::fromRGB(40, 46, 56);
        const auto kInfo = juce::Colour::fromRGB(160, 170, 186);
        const auto kOk = juce::Colour::fromRGB(112, 214, 156);
        const auto kWarn = juce::Colour::fromRGB(255, 196, 120);
        const auto kError = juce::Colour::fromRGB(255, 124, 124);
        constexpr auto kPackageSchema = "gyeol.assets.package";
        constexpr auto kPackageManifestFile = "assets-manifest.json";

        bool containsCaseInsensitive(const juce::String& haystack, const juce::String& needle)
        {
            if (needle.isEmpty())
                return true;
            return haystack.toLowerCase().contains(needle.toLowerCase());
        }

        bool isSupportedImportExtension(const juce::String& extension)
        {
            static const std::array<juce::String, 25> allowed {
                "png", "jpg", "jpeg", "bmp", "gif", "svg", "webp",
                "ttf", "otf", "woff", "woff2",
                "wav", "aif", "aiff", "ogg", "flac", "mp3",
                "json", "xml", "txt", "csv", "bin",
                "ico", "tga", "pdf"
            };

            const auto normalized = extension.trim().toLowerCase();
            return std::find(allowed.begin(), allowed.end(), normalized) != allowed.end();
        }

        bool isSupportedAudioExtension(const juce::String& extension)
        {
            static const std::array<juce::String, 6> audioExtensions { "wav", "aif", "aiff", "ogg", "flac", "mp3" };
            const auto normalized = extension.trim().toLowerCase();
            return std::find(audioExtensions.begin(), audioExtensions.end(), normalized) != audioExtensions.end();
        }
    }

    class AssetsPanel::RowComponent final : public juce::Component
    {
    public:
        explicit RowComponent(AssetsPanel& ownerIn)
            : owner(ownerIn)
        {
            kindBadge.setJustificationType(juce::Justification::centred);
            kindBadge.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            kindBadge.setColour(juce::Label::textColourId, juce::Colours::black.withAlpha(0.8f));
            kindBadge.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(kindBadge);

            nameLabel.setJustificationType(juce::Justification::centredLeft);
            nameLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            nameLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(194, 202, 216));
            nameLabel.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(nameLabel);

            detailLabel.setJustificationType(juce::Justification::centredLeft);
            detailLabel.setFont(juce::FontOptions(10.0f));
            detailLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 170, 186));
            detailLabel.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(detailLabel);

            previewButton.setTriggeredOnMouseDown(false);
            previewButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(58, 96, 144));
            previewButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(214, 108, 84));
            previewButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(228, 236, 248));
            previewButton.setColour(juce::TextButton::textColourOnId, juce::Colour::fromRGB(248, 236, 232));
            previewButton.onClick = [this]
            {
                if (assetId > kRootId)
                    owner.toggleAudioPreviewForAsset(assetId);
            };
            addAndMakeVisible(previewButton);
            previewButton.setVisible(false);
        }

        void setRowData(int row, const AssetModel& asset, bool selected, int usageCountIn, bool exportExcluded)
        {
            rowIndex = row;
            assetId = asset.id;
            rowSelected = selected;
            badgeColor = AssetsPanel::kindColor(asset.kind);
            usageCount = juce::jmax(0, usageCountIn);
            excludedFromExport = exportExcluded;

            kindBadge.setText(AssetsPanel::kindLabel(asset.kind), juce::dontSendNotification);
            nameLabel.setText(asset.name.isNotEmpty() ? asset.name : ("Asset #" + juce::String(asset.id)),
                              juce::dontSendNotification);

            auto detail = asset.refKey
                        + (asset.relativePath.isNotEmpty() ? (" | " + asset.relativePath) : juce::String());
            if (excludedFromExport)
                detail += " | Excluded";
            detailLabel.setText(detail, juce::dontSendNotification);

            showThumbnail = asset.kind == AssetKind::image;
            thumbnail = showThumbnail ? owner.getImageThumbnailForAsset(asset) : juce::Image();

            showAudioPreview = owner.audioPreviewAvailable && AssetsPanel::isAudioAsset(asset);
            const auto previewPlaying = showAudioPreview && owner.isAssetPreviewPlaying(asset.id);
            previewButton.setVisible(showAudioPreview);
            previewButton.setButtonText(previewPlaying ? "Stop" : "Play");

            resized();
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            const auto area = getLocalBounds().toFloat();
            const auto fill = rowSelected ? juce::Colour::fromRGB(49, 84, 142)
                                          : juce::Colour::fromRGB(24, 30, 40);
            g.setColour(fill.withAlpha(rowSelected ? 0.84f : 0.62f));
            g.fillRect(area);

            g.setColour(juce::Colour::fromRGB(44, 52, 66));
            g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));

            g.setColour(badgeColor);
            g.fillRoundedRectangle(kindBadge.getBounds().toFloat(), 3.0f);
            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.drawRoundedRectangle(kindBadge.getBounds().toFloat(), 3.0f, 1.0f);

            if (showThumbnail && !thumbnailBounds.isEmpty())
            {
                const auto thumbBounds = thumbnailBounds.toFloat();
                g.setColour(juce::Colour::fromRGB(16, 20, 26));
                g.fillRoundedRectangle(thumbBounds, 4.0f);
                g.setColour(juce::Colour::fromRGB(62, 70, 84));
                g.drawRoundedRectangle(thumbBounds, 4.0f, 1.0f);

                if (thumbnail.isValid())
                {
                    g.drawImageWithin(thumbnail,
                                      thumbnailBounds.getX() + 1,
                                      thumbnailBounds.getY() + 1,
                                      thumbnailBounds.getWidth() - 2,
                                      thumbnailBounds.getHeight() - 2,
                                      juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                                      false);
                }
                else
                {
                    g.setColour(juce::Colour::fromRGB(108, 118, 132));
                    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
                    g.drawFittedText("N/A", thumbnailBounds, juce::Justification::centred, 1);
                }
            }

            if (!usageBadgeBounds.isEmpty())
            {
                const auto hasUsage = usageCount > 0;
                const auto badgeFill = hasUsage ? juce::Colour::fromRGB(84, 166, 118)
                                                : juce::Colour::fromRGB(74, 82, 98);
                g.setColour(badgeFill.withAlpha(0.9f));
                g.fillRoundedRectangle(usageBadgeBounds.toFloat(), 3.0f);
                g.setColour(juce::Colours::black.withAlpha(0.55f));
                g.drawRoundedRectangle(usageBadgeBounds.toFloat(), 3.0f, 1.0f);

                g.setColour(juce::Colour::fromRGB(236, 242, 248));
                g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
                const auto text = "USED " + juce::String(usageCount);
                g.drawFittedText(text, usageBadgeBounds.reduced(2, 1), juce::Justification::centred, 1);
            }
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8, 5);

            if (showThumbnail)
            {
                thumbnailBounds = area.removeFromLeft(40).reduced(0, 2);
                area.removeFromLeft(8);
            }
            else
            {
                thumbnailBounds = {};
            }

            if (showAudioPreview)
            {
                auto buttonArea = area.removeFromRight(52);
                buttonArea = buttonArea.withTrimmedTop(8).withTrimmedBottom(8);
                previewButton.setBounds(buttonArea);
                area.removeFromRight(6);
            }
            else
            {
                previewButton.setBounds({});
            }

            auto usageArea = area.removeFromRight(60);
            usageBadgeBounds = usageArea.removeFromTop(14);
            area.removeFromRight(6);

            kindBadge.setBounds(area.removeFromTop(14).removeFromLeft(56));
            area.removeFromTop(4);

            nameLabel.setBounds(area.removeFromTop(16));
            area.removeFromTop(2);
            detailLabel.setBounds(area.removeFromTop(15));
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (rowIndex >= 0)
                owner.listBox.selectRow(rowIndex);
            dragStart = event.getPosition();
            dragStarted = false;
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (dragStarted || event.getDistanceFromDragStart() < 4)
                return;

            dragStarted = true;
            owner.startDragForRow(rowIndex, *this, dragStart);
        }

    private:
        AssetsPanel& owner;
        int rowIndex = -1;
        WidgetId assetId = kRootId;
        bool rowSelected = false;
        bool dragStarted = false;
        bool showThumbnail = false;
        bool showAudioPreview = false;
        bool excludedFromExport = false;
        int usageCount = 0;
        juce::Point<int> dragStart;
        juce::Colour badgeColor { juce::Colour::fromRGB(150, 160, 176) };
        juce::Rectangle<int> thumbnailBounds;
        juce::Rectangle<int> usageBadgeBounds;
        juce::Image thumbnail;

        juce::Label kindBadge;
        juce::Label nameLabel;
        juce::Label detailLabel;
        juce::TextButton previewButton;
    };

    class AssetsPanel::UsageListModel final : public juce::ListBoxModel
    {
    public:
        explicit UsageListModel(AssetsPanel& ownerIn)
            : owner(ownerIn)
        {
        }

        int getNumRows() override
        {
            return static_cast<int>(owner.selectedAssetUsageEntries.size());
        }

        void paintListBoxItem(int rowNumber,
                              juce::Graphics& g,
                              int width,
                              int height,
                              bool rowIsSelected) override
        {
            if (rowNumber < 0 || rowNumber >= static_cast<int>(owner.selectedAssetUsageEntries.size()))
                return;

            const auto& entry = owner.selectedAssetUsageEntries[static_cast<size_t>(rowNumber)];
            const auto fill = rowIsSelected ? juce::Colour::fromRGB(49, 84, 142)
                                            : juce::Colour::fromRGB(25, 31, 40);
            g.setColour(fill.withAlpha(rowIsSelected ? 0.84f : 0.62f));
            g.fillRect(0, 0, width, height);

            g.setColour(juce::Colour::fromRGB(44, 52, 66));
            g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

            g.setColour(juce::Colour::fromRGB(206, 216, 232));
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawFittedText(entry.widgetLabel + " - " + entry.propertyLabel,
                             8,
                             2,
                             width - 16,
                             14,
                             juce::Justification::centredLeft,
                             1);

            g.setColour(juce::Colour::fromRGB(158, 170, 188));
            g.setFont(juce::FontOptions(9.5f));
            g.drawFittedText(entry.contextLabel,
                             8,
                             16,
                             width - 16,
                             juce::jmax(10, height - 18),
                             juce::Justification::centredLeft,
                             1);
        }

        void listBoxItemClicked(int row, const juce::MouseEvent&) override
        {
            owner.activateUsageEntryAtRow(row);
        }

    private:
        AssetsPanel& owner;
    };

    AssetsPanel::AssetsPanel(DocumentHandle& documentIn, const Widgets::WidgetFactory& widgetFactoryIn)
        : document(documentIn),
          widgetFactory(widgetFactoryIn)
    {
        titleLabel.setText("Assets", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(192, 200, 214));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        kindFilterCombo.addItem("All", 1);
        kindFilterCombo.addItem("Image", 2);
        kindFilterCombo.addItem("Font", 3);
        kindFilterCombo.addItem("Color", 4);
        kindFilterCombo.addItem("File", 5);
        kindFilterCombo.addItem("Unused", 6);
        kindFilterCombo.setSelectedId(1, juce::dontSendNotification);
        kindFilterCombo.onChange = [this]
        {
            rebuildVisibleAssets();
        };
        addAndMakeVisible(kindFilterCombo);

        importConflictCombo.addItem("Conflict: Rename", 1);
        importConflictCombo.addItem("Conflict: Overwrite", 2);
        importConflictCombo.addItem("Conflict: Skip", 3);
        importConflictCombo.setSelectedId(1, juce::dontSendNotification);
        addAndMakeVisible(importConflictCombo);

        searchEditor.setMultiLine(false);
        searchEditor.setScrollbarsShown(true);
        searchEditor.setTextToShowWhenEmpty("Search name/ref/path...", juce::Colour::fromRGB(124, 132, 148));
        searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(28, 34, 44));
        searchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(66, 76, 92));
        searchEditor.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(214, 222, 234));
        searchEditor.onTextChange = [this]
        {
            rebuildVisibleAssets();
        };
        addAndMakeVisible(searchEditor);

        cleanupUnusedButton.onClick = [this] { removeUnusedAssets(); };
        mergeDuplicatesButton.onClick = [this] { mergeDuplicateAssets(); };
        relinkMissingButton.onClick = [this] { relinkMissingAssets(); };
        addFileButton.onClick = [this] { addFileAsset(); };
        addColorButton.onClick = [this] { addColorAsset(); };
        importPackageButton.onClick = [this] { importAssetPackage(); };
        exportPackageButton.onClick = [this] { exportAssetPackage(); };
        reimportButton.onClick = [this] { reimportSelectedAsset(); };
        replaceAssetButton.onClick = [this] { replaceSelectedAssetFile(); };
        deleteButton.onClick = [this] { deleteSelectedAsset(); };
        copyRefButton.onClick = [this] { copySelectedRefKey(); };
        applyRefButton.onClick = [this] { applyRefKeyEdit(); };
        exportIncludeToggle.onClick = [this] { applyExportIncludeToggle(); };
        addAndMakeVisible(cleanupUnusedButton);
        addAndMakeVisible(mergeDuplicatesButton);
        addAndMakeVisible(relinkMissingButton);
        addAndMakeVisible(addFileButton);
        addAndMakeVisible(addColorButton);
        addAndMakeVisible(importPackageButton);
        addAndMakeVisible(exportPackageButton);
        addAndMakeVisible(reimportButton);
        addAndMakeVisible(replaceAssetButton);
        addAndMakeVisible(deleteButton);
        addAndMakeVisible(exportIncludeToggle);
        addAndMakeVisible(copyRefButton);
        addAndMakeVisible(applyRefButton);

        refKeyEditor.setMultiLine(false);
        refKeyEditor.setScrollbarsShown(true);
        refKeyEditor.setTextToShowWhenEmpty("asset.refKey", juce::Colour::fromRGB(124, 132, 148));
        refKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(28, 34, 44));
        refKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(66, 76, 92));
        refKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(214, 222, 234));
        refKeyEditor.onReturnKey = [this] { applyRefKeyEdit(); };
        addAndMakeVisible(refKeyEditor);

        listBox.setModel(this);
        listBox.setRowHeight(56);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
        listBox.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
        addAndMakeVisible(listBox);

        usageTitleLabel.setText("Usage Trace", juce::dontSendNotification);
        usageTitleLabel.setJustificationType(juce::Justification::centredLeft);
        usageTitleLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        usageTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(178, 188, 202));
        addAndMakeVisible(usageTitleLabel);

        usageListModel = std::make_unique<UsageListModel>(*this);
        usageList.setModel(usageListModel.get());
        usageList.setRowHeight(32);
        usageList.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
        usageList.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
        addAndMakeVisible(usageList);

        statusLabel.setColour(juce::Label::textColourId, kInfo);
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setText("Ready", juce::dontSendNotification);
        addAndMakeVisible(statusLabel);

        audioFormatManager.registerBasicFormats();
        audioSourcePlayer.setSource(&audioTransportSource);
        const auto initError = audioDeviceManager.initialiseWithDefaultDevices(0, 2);
        if (initError.isEmpty())
        {
            audioPreviewAvailable = true;
            audioDeviceManager.addAudioCallback(&audioSourcePlayer);
        }
        else
        {
            setStatus("Audio preview unavailable: " + initError, kWarn);
        }

        refreshFromDocument();
    }

    AssetsPanel::~AssetsPanel()
    {
        stopAudioPreview();
        if (audioPreviewAvailable)
            audioDeviceManager.removeAudioCallback(&audioSourcePlayer);
        audioSourcePlayer.setSource(nullptr);
        usageList.setModel(nullptr);
        listBox.setModel(nullptr);
    }

    void AssetsPanel::refreshFromDocument()
    {
        assets = document.snapshot().assets;
        thumbnailCache.clear();
        rebuildUsageIndex();

        if (previewAssetId > kRootId)
        {
            const auto stillExists = std::any_of(assets.begin(),
                                                 assets.end(),
                                                 [this](const AssetModel& asset)
                                                 {
                                                     return asset.id == previewAssetId;
                                                 });
            if (!stillExists)
                stopAudioPreview();
        }

        rebuildVisibleAssets();
    }

    void AssetsPanel::setAssetsChangedCallback(std::function<void(const juce::String&)> callback)
    {
        onAssetsChanged = std::move(callback);
    }

    void AssetsPanel::setAssetUsageNavigateCallback(std::function<void(WidgetId)> callback)
    {
        onAssetUsageNavigate = std::move(callback);
    }

    void AssetsPanel::paint(juce::Graphics& g)
    {
        g.fillAll(kPanelBg);
        g.setColour(kPanelOutline);
        g.drawRect(getLocalBounds(), 1);

        if (fileDragHovering)
        {
            g.setColour(juce::Colour::fromRGBA(84, 212, 255, 36));
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(3.0f), 5.0f);
            g.setColour(juce::Colour::fromRGBA(84, 212, 255, 200));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(3.5f), 5.0f, 1.3f);
        }
    }

    void AssetsPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);

        auto titleRow = area.removeFromTop(20);
        cleanupUnusedButton.setBounds(titleRow.removeFromRight(94));
        titleRow.removeFromRight(4);
        mergeDuplicatesButton.setBounds(titleRow.removeFromRight(88));
        titleRow.removeFromRight(4);
        titleLabel.setBounds(titleRow);

        area.removeFromTop(4);
        auto filterRow = area.removeFromTop(24);
        kindFilterCombo.setBounds(filterRow.removeFromLeft(96));
        filterRow.removeFromLeft(4);
        addFileButton.setBounds(filterRow.removeFromLeft(62));
        filterRow.removeFromLeft(4);
        addColorButton.setBounds(filterRow.removeFromLeft(62));
        filterRow.removeFromLeft(4);
        relinkMissingButton.setBounds(filterRow.removeFromLeft(90));

        area.removeFromTop(4);
        auto packageRow = area.removeFromTop(24);
        importPackageButton.setBounds(packageRow.removeFromLeft(76));
        packageRow.removeFromLeft(4);
        exportPackageButton.setBounds(packageRow.removeFromLeft(76));
        packageRow.removeFromLeft(4);
        reimportButton.setBounds(packageRow.removeFromLeft(66));
        packageRow.removeFromLeft(4);
        replaceAssetButton.setBounds(packageRow.removeFromLeft(64));
        packageRow.removeFromLeft(4);
        importConflictCombo.setBounds(packageRow);

        area.removeFromTop(4);
        searchEditor.setBounds(area.removeFromTop(24));

        constexpr int usageTitleHeight = 18;
        constexpr int usageListHeight = 96;
        constexpr int refEditorHeight = 24;
        constexpr int bottomButtonsHeight = 24;
        constexpr int statusHeight = 18;

        const int reserved = 6
                           + refEditorHeight + 4
                           + bottomButtonsHeight + 4
                           + usageTitleHeight + 4
                           + usageListHeight + 4
                           + statusHeight;

        area.removeFromTop(6);
        listBox.setBounds(area.removeFromTop(juce::jmax(96, area.getHeight() - reserved)));

        area.removeFromTop(4);
        refKeyEditor.setBounds(area.removeFromTop(refEditorHeight));

        area.removeFromTop(4);
        auto bottomRow = area.removeFromTop(bottomButtonsHeight);
        applyRefButton.setBounds(bottomRow.removeFromLeft(72));
        bottomRow.removeFromLeft(4);
        copyRefButton.setBounds(bottomRow.removeFromLeft(72));
        bottomRow.removeFromLeft(4);
        deleteButton.setBounds(bottomRow.removeFromLeft(68));
        bottomRow.removeFromLeft(8);
        exportIncludeToggle.setBounds(bottomRow.removeFromLeft(126));

        area.removeFromTop(4);
        usageTitleLabel.setBounds(area.removeFromTop(usageTitleHeight));

        area.removeFromTop(4);
        usageList.setBounds(area.removeFromTop(usageListHeight));

        area.removeFromTop(4);
        statusLabel.setBounds(area.removeFromTop(statusHeight));
    }

    int AssetsPanel::getNumRows()
    {
        return static_cast<int>(visibleAssetIndices.size());
    }

    void AssetsPanel::paintListBoxItem(int rowNumber,
                                       juce::Graphics& g,
                                       int width,
                                       int height,
                                       bool rowIsSelected)
    {
        juce::ignoreUnused(rowNumber, g, width, height, rowIsSelected);
        // Rows are rendered by RowComponent via refreshComponentForRow().
    }

    juce::Component* AssetsPanel::refreshComponentForRow(int rowNumber,
                                                         bool isRowSelected,
                                                         juce::Component* existingComponentToUpdate)
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(visibleAssetIndices.size()))
            return nullptr;

        const auto modelIndex = visibleAssetIndices[static_cast<size_t>(rowNumber)];
        if (modelIndex < 0 || modelIndex >= static_cast<int>(assets.size()))
            return nullptr;

        auto* rowComponent = dynamic_cast<RowComponent*>(existingComponentToUpdate);
        if (rowComponent == nullptr)
            rowComponent = new RowComponent(*this);

        const auto& asset = assets[static_cast<size_t>(modelIndex)];
        rowComponent->setRowData(rowNumber,
                                 asset,
                                 isRowSelected,
                                 usageCountForAsset(asset.id),
                                 isAssetExcludedFromExport(asset));
        return rowComponent;
    }

    void AssetsPanel::selectedRowsChanged(int)
    {
        selectedAssetId = kRootId;
        if (const auto* asset = selectedAsset(); asset != nullptr)
            selectedAssetId = asset->id;
        syncRefEditorFromSelection();
        syncExportIncludeToggleFromSelection();
        refreshSelectedAssetUsageList();
        updateButtons();
    }

    void AssetsPanel::listBoxItemClicked(int row, const juce::MouseEvent& event)
    {
        juce::ignoreUnused(event);
        if (row >= 0 && row < static_cast<int>(visibleAssetIndices.size()))
            listBox.selectRow(row);
    }

    void AssetsPanel::timerCallback()
    {
        if (previewAssetId <= kRootId)
        {
            stopTimer();
            return;
        }

        if (audioTransportSource.isPlaying())
            return;

        audioTransportSource.setSource(nullptr);
        audioReaderSource.reset();
        previewAssetId = kRootId;
        stopTimer();
        listBox.repaint();
    }

    bool AssetsPanel::isInterestedInFileDrag(const juce::StringArray& files)
    {
        for (const auto& path : files)
        {
            if (isImportableFile(juce::File(path)))
                return true;
        }

        return false;
    }

    void AssetsPanel::fileDragEnter(const juce::StringArray& files, int x, int y)
    {
        juce::ignoreUnused(x, y);
        fileDragHovering = isInterestedInFileDrag(files);
        if (fileDragHovering)
            setStatus("Drop files to import into Assets.", kInfo);
        repaint();
    }

    void AssetsPanel::fileDragMove(const juce::StringArray& files, int x, int y)
    {
        juce::ignoreUnused(x, y);
        const auto interested = isInterestedInFileDrag(files);
        if (interested == fileDragHovering)
            return;

        fileDragHovering = interested;
        repaint();
    }

    void AssetsPanel::fileDragExit(const juce::StringArray& files)
    {
        juce::ignoreUnused(files);
        fileDragHovering = false;
        updateButtons();
        if (statusLabel.getText().isEmpty())
            setStatus("Ready", kInfo);
        repaint();
    }

    void AssetsPanel::filesDropped(const juce::StringArray& files, int x, int y)
    {
        juce::ignoreUnused(x, y);
        fileDragHovering = false;

        int imported = 0;
        int skipped = 0;
        const auto changed = addFilesAsAssets(files, &imported, &skipped);
        if (!changed)
        {
            setStatus("No importable files dropped.", kWarn);
            repaint();
            return;
        }

        setStatus("Imported " + juce::String(imported)
                  + " file(s)" + (skipped > 0 ? (" (" + juce::String(skipped) + " skipped)") : juce::String()),
                  kOk);
        repaint();
    }

    int AssetsPanel::selectedModelIndex() const
    {
        const auto row = listBox.getSelectedRow();
        if (row < 0 || row >= static_cast<int>(visibleAssetIndices.size()))
            return -1;
        return visibleAssetIndices[static_cast<size_t>(row)];
    }

    const AssetModel* AssetsPanel::selectedAsset() const
    {
        const auto index = selectedModelIndex();
        if (index < 0 || index >= static_cast<int>(assets.size()))
            return nullptr;
        return &assets[static_cast<size_t>(index)];
    }

    void AssetsPanel::rebuildVisibleAssets()
    {
        const auto previousSelectedId = selectedAssetId;
        visibleAssetIndices.clear();

        const auto filter = searchEditor.getText().trim();
        const auto selectedFilterId = kindFilterCombo.getSelectedId();

        for (size_t i = 0; i < assets.size(); ++i)
        {
            const auto& asset = assets[i];

            bool kindAllowed = true;
            switch (selectedFilterId)
            {
                case 2: kindAllowed = asset.kind == AssetKind::image; break;
                case 3: kindAllowed = asset.kind == AssetKind::font; break;
                case 4: kindAllowed = asset.kind == AssetKind::colorPreset; break;
                case 5: kindAllowed = asset.kind == AssetKind::file; break;
                case 6: kindAllowed = usageCountForAsset(asset.id) <= 0; break;
                default: break;
            }
            if (!kindAllowed)
                continue;

            const auto searchable = asset.name + " " + asset.refKey + " " + asset.relativePath + " " + asset.mimeType;
            if (!containsCaseInsensitive(searchable, filter))
                continue;

            visibleAssetIndices.push_back(static_cast<int>(i));
        }

        listBox.updateContent();

        int nextSelectedRow = -1;
        if (previousSelectedId > kRootId)
        {
            for (size_t row = 0; row < visibleAssetIndices.size(); ++row)
            {
                const auto modelIndex = visibleAssetIndices[row];
                if (modelIndex >= 0
                    && modelIndex < static_cast<int>(assets.size())
                    && assets[static_cast<size_t>(modelIndex)].id == previousSelectedId)
                {
                    nextSelectedRow = static_cast<int>(row);
                    break;
                }
            }
        }

        if (nextSelectedRow >= 0)
            listBox.selectRow(nextSelectedRow);
        else
            listBox.deselectAllRows();

        if (const auto* selected = selectedAsset(); selected != nullptr)
            selectedAssetId = selected->id;
        else
            selectedAssetId = kRootId;

        syncExportIncludeToggleFromSelection();
        refreshSelectedAssetUsageList();
        updateButtons();
        repaint();
    }

    void AssetsPanel::rebuildUsageIndex()
    {
        usageByAssetId.clear();
        usageCountByAssetId.clear();

        const auto& snapshot = document.snapshot();
        std::map<juce::String, WidgetId> assetIdByRef;
        for (const auto& asset : assets)
        {
            const auto key = asset.refKey.trim().toLowerCase();
            if (key.isEmpty())
                continue;

            assetIdByRef[key] = asset.id;
            usageByAssetId[asset.id];
            usageCountByAssetId[asset.id] = 0;
        }

        std::unordered_map<WidgetId, const WidgetModel*> widgetById;
        widgetById.reserve(snapshot.widgets.size());
        for (const auto& widget : snapshot.widgets)
            widgetById[widget.id] = &widget;

        const auto widgetLabelFor = [this](const WidgetModel& widget) -> juce::String
        {
            juce::String typeLabel = "Widget";
            if (const auto* descriptor = widgetFactory.descriptorFor(widget.type); descriptor != nullptr)
            {
                if (descriptor->displayName.isNotEmpty())
                    typeLabel = descriptor->displayName;
                else if (descriptor->typeKey.isNotEmpty())
                    typeLabel = descriptor->typeKey;
            }

            return typeLabel + " #" + juce::String(widget.id);
        };

        const auto registerUsage = [this](WidgetId assetId,
                                          WidgetId widgetId,
                                          const juce::Identifier& propertyKey,
                                          const juce::String& propertyLabel,
                                          const juce::String& widgetLabel,
                                          const juce::String& contextLabel,
                                          bool runtimePatch)
        {
            AssetUsageEntry entry;
            entry.widgetId = widgetId;
            entry.propertyKey = propertyKey;
            entry.propertyLabel = propertyLabel;
            entry.widgetLabel = widgetLabel;
            entry.contextLabel = contextLabel;
            entry.runtimePatch = runtimePatch;
            usageByAssetId[assetId].push_back(std::move(entry));
        };

        for (const auto& widget : snapshot.widgets)
        {
            const auto* specs = widgetFactory.propertySpecsFor(widget.type);
            if (specs == nullptr)
                continue;

            const auto widgetLabel = widgetLabelFor(widget);
            for (const auto& spec : *specs)
            {
                if (spec.kind != Widgets::WidgetPropertyKind::assetRef)
                    continue;

                const auto* value = widget.properties.getVarPointer(spec.key);
                if (value == nullptr)
                    continue;

                const auto refKey = value->toString().trim().toLowerCase();
                if (refKey.isEmpty())
                    continue;

                const auto foundAsset = assetIdByRef.find(refKey);
                if (foundAsset == assetIdByRef.end())
                    continue;

                const auto propertyLabel = spec.label.isNotEmpty() ? spec.label : spec.key.toString();
                registerUsage(foundAsset->second,
                              widget.id,
                              spec.key,
                              propertyLabel,
                              widgetLabel,
                              "Widget property",
                              false);
            }
        }

        for (const auto& binding : snapshot.runtimeBindings)
        {
            for (size_t actionIndex = 0; actionIndex < binding.actions.size(); ++actionIndex)
            {
                const auto& action = binding.actions[actionIndex];
                if (action.kind != RuntimeActionKind::setNodeProps)
                    continue;
                if (action.target.kind != NodeKind::widget || action.target.id <= kRootId)
                    continue;

                const auto widgetIt = widgetById.find(action.target.id);
                if (widgetIt == widgetById.end() || widgetIt->second == nullptr)
                    continue;

                const auto& targetWidget = *(widgetIt->second);
                const auto widgetLabel = widgetLabelFor(targetWidget);
                const auto bindingLabel = binding.name.trim().isNotEmpty()
                                        ? binding.name.trim()
                                        : ("Binding #" + juce::String(binding.id));
                const auto context = "Runtime patch: " + bindingLabel + " / Action " + juce::String(static_cast<int>(actionIndex + 1));

                for (int patchIndex = 0; patchIndex < action.patch.size(); ++patchIndex)
                {
                    const auto propertyKey = action.patch.getName(patchIndex);
                    const auto* spec = widgetFactory.propertySpecFor(targetWidget.type, propertyKey);
                    if (spec == nullptr || spec->kind != Widgets::WidgetPropertyKind::assetRef)
                        continue;

                    const auto refKey = action.patch.getValueAt(patchIndex).toString().trim().toLowerCase();
                    if (refKey.isEmpty())
                        continue;

                    const auto foundAsset = assetIdByRef.find(refKey);
                    if (foundAsset == assetIdByRef.end())
                        continue;

                    const auto propertyLabel = spec->label.isNotEmpty() ? spec->label : propertyKey.toString();
                    registerUsage(foundAsset->second,
                                  targetWidget.id,
                                  propertyKey,
                                  propertyLabel,
                                  widgetLabel,
                                  context,
                                  true);
                }
            }
        }

        for (auto& [assetId, entries] : usageByAssetId)
        {
            std::unordered_set<WidgetId> uniqueWidgetIds;
            for (const auto& entry : entries)
                uniqueWidgetIds.insert(entry.widgetId);
            usageCountByAssetId[assetId] = static_cast<int>(uniqueWidgetIds.size());

            std::sort(entries.begin(),
                      entries.end(),
                      [](const AssetUsageEntry& lhs, const AssetUsageEntry& rhs)
                      {
                          if (lhs.widgetId != rhs.widgetId)
                              return lhs.widgetId < rhs.widgetId;
                          if (lhs.runtimePatch != rhs.runtimePatch)
                              return !lhs.runtimePatch && rhs.runtimePatch;
                          if (lhs.propertyLabel != rhs.propertyLabel)
                              return lhs.propertyLabel.compareNatural(rhs.propertyLabel) < 0;
                          return lhs.contextLabel.compareNatural(rhs.contextLabel) < 0;
                      });
        }

        for (const auto& asset : assets)
        {
            usageByAssetId[asset.id];
            usageCountByAssetId.try_emplace(asset.id, 0);
        }
    }

    void AssetsPanel::refreshSelectedAssetUsageList()
    {
        selectedAssetUsageEntries.clear();
        if (selectedAssetId > kRootId)
        {
            if (const auto found = usageByAssetId.find(selectedAssetId); found != usageByAssetId.end())
                selectedAssetUsageEntries = found->second;
        }

        usageList.updateContent();
        usageList.deselectAllRows();
        usageTitleLabel.setText("Usage Trace (" + juce::String(selectedAssetUsageEntries.size()) + ")",
                                juce::dontSendNotification);
    }

    void AssetsPanel::activateUsageEntryAtRow(int row)
    {
        if (row < 0 || row >= static_cast<int>(selectedAssetUsageEntries.size()))
            return;

        const auto& entry = selectedAssetUsageEntries[static_cast<size_t>(row)];
        if (entry.widgetId <= kRootId)
            return;

        if (onAssetUsageNavigate != nullptr)
            onAssetUsageNavigate(entry.widgetId);
    }

    int AssetsPanel::usageCountForAsset(WidgetId assetId) const
    {
        if (assetId <= kRootId)
            return 0;

        const auto found = usageCountByAssetId.find(assetId);
        if (found == usageCountByAssetId.end())
            return 0;

        return found->second;
    }

    bool AssetsPanel::hasUnusedAssets() const
    {
        return std::any_of(assets.begin(),
                           assets.end(),
                           [this](const AssetModel& asset)
                           {
                               return usageCountForAsset(asset.id) <= 0;
                           });
    }

    bool AssetsPanel::commitAssets(const juce::String& reason)
    {
        if (!document.setAssets(assets))
        {
            setStatus("No document change.", kInfo);
            return false;
        }

        setStatus(reason, kOk);
        if (onAssetsChanged != nullptr)
            onAssetsChanged(reason);
        return true;
    }

    void AssetsPanel::updateButtons()
    {
        const auto hasSelection = selectedAsset() != nullptr;
        const auto hasAssets = !assets.empty();
        const auto hasUnused = hasUnusedAssets();
        const auto canReimport = [this, hasSelection]() -> bool
        {
            if (!hasSelection)
                return false;

            const auto* asset = selectedAsset();
            if (asset == nullptr || asset->kind == AssetKind::colorPreset)
                return false;

            return asset->relativePath.trim().isNotEmpty();
        }();

        const auto canReplace = [this, hasSelection]() -> bool
        {
            if (!hasSelection)
                return false;

            const auto* asset = selectedAsset();
            return asset != nullptr && asset->kind != AssetKind::colorPreset;
        }();

        deleteButton.setEnabled(hasSelection);
        copyRefButton.setEnabled(hasSelection);
        applyRefButton.setEnabled(hasSelection);
        refKeyEditor.setEnabled(hasSelection);
        exportPackageButton.setEnabled(hasAssets);
        cleanupUnusedButton.setEnabled(hasUnused);
        mergeDuplicatesButton.setEnabled(hasAssets);
        relinkMissingButton.setEnabled(hasAssets);
        reimportButton.setEnabled(canReimport);
        replaceAssetButton.setEnabled(canReplace);
        exportIncludeToggle.setEnabled(hasSelection);
        importConflictCombo.setEnabled(true);
    }

    void AssetsPanel::setStatus(const juce::String& text, juce::Colour colour)
    {
        statusLabel.setText(text, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, colour);
    }

    void AssetsPanel::addFileAsset()
    {
        if (pendingFileChooser != nullptr)
        {
            setStatus("File chooser already open.", kInfo);
            return;
        }

        pendingFileChooser = std::make_unique<juce::FileChooser>("Select asset file");
        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles;

        pendingFileChooser->launchAsync(chooserFlags,
                                        [safe = juce::Component::SafePointer<AssetsPanel>(this)](const juce::FileChooser& chooser)
                                        {
                                            if (safe == nullptr)
                                                return;

                                            const auto file = chooser.getResult();
                                            safe->pendingFileChooser.reset();

                                            if (!file.existsAsFile())
                                            {
                                                safe->setStatus("No file selected.", kInfo);
                                                return;
                                            }
                                            int imported = 0;
                                            int skipped = 0;
                                            const juce::StringArray files { file.getFullPathName() };
                                            if (safe->addFilesAsAssets(files, &imported, &skipped))
                                                return;

                                            safe->setStatus("Selected file is not importable.", kWarn);
                                        });
    }

    bool AssetsPanel::addFilesAsAssets(const juce::StringArray& files, int* importedCount, int* skippedCount)
    {
        int imported = 0;
        int skipped = 0;
        juce::String lastImportedName;

        for (const auto& path : files)
        {
            const juce::File file(path);
            if (!isImportableFile(file))
            {
                ++skipped;
                continue;
            }

            AssetModel asset;
            asset.id = allocateNextAssetId();
            asset.kind = inferAssetKindFromFile(file);
            asset.name = file.getFileNameWithoutExtension().trim();
            if (asset.name.isEmpty())
                asset.name = file.getFileName().trim();
            asset.refKey = makeUniqueRefKey(asset.name, asset.id);
            asset.relativePath = resolveRelativePath(file);
            asset.mimeType = inferMimeTypeFromFile(file);

            lastImportedName = asset.name;
            selectedAssetId = asset.id;
            assets.push_back(std::move(asset));
            ++imported;
        }

        if (importedCount != nullptr)
            *importedCount = imported;
        if (skippedCount != nullptr)
            *skippedCount = skipped;

        if (imported <= 0)
            return false;

        const auto reason = imported == 1
                          ? ("Asset added from file: " + lastImportedName)
                          : ("Assets added from files: " + juce::String(imported));

        if (commitAssets(reason))
        {
            refreshFromDocument();
            return true;
        }

        return false;
    }

    void AssetsPanel::addColorAsset()
    {
        AssetModel asset;
        asset.id = allocateNextAssetId();
        asset.kind = AssetKind::colorPreset;
        asset.name = "Color " + juce::String(static_cast<int>(assets.size() + 1));
        asset.refKey = makeUniqueRefKey(asset.name, asset.id);
        asset.relativePath.clear();
        asset.mimeType = "application/x-color-preset";
        asset.meta.set("value", "#FFFFFF");

        assets.push_back(std::move(asset));
        if (commitAssets("Color preset added"))
        {
            selectedAssetId = assets.back().id;
            refreshFromDocument();
        }
    }

    void AssetsPanel::reimportSelectedAsset()
    {
        const auto index = selectedModelIndex();
        if (index < 0 || index >= static_cast<int>(assets.size()))
        {
            setStatus("No asset selected for reimport.", kWarn);
            return;
        }

        auto& asset = assets[static_cast<size_t>(index)];
        if (asset.kind == AssetKind::colorPreset)
        {
            setStatus("Color preset does not support reimport.", kWarn);
            return;
        }

        if (asset.relativePath.trim().isEmpty())
        {
            setStatus("Reimport failed: asset path is empty.", kWarn);
            return;
        }

        const auto sourceFile = resolveInputFilePath(asset.relativePath);
        if (!sourceFile.existsAsFile())
        {
            setStatus("Reimport failed: source file not found.", kWarn);
            return;
        }

        const auto oldKind = asset.kind;
        const auto oldPath = asset.relativePath;
        const auto oldMime = asset.mimeType;

        asset.kind = inferAssetKindFromFile(sourceFile);
        asset.relativePath = normalizeRelativePath(resolveRelativePath(sourceFile));
        asset.mimeType = inferMimeTypeFromFile(sourceFile);
        thumbnailCache.clear();
        juce::ImageCache::releaseUnusedImages();
        if (previewAssetId == asset.id)
            stopAudioPreview();

        const auto changed = asset.kind != oldKind || asset.relativePath != oldPath || asset.mimeType != oldMime;
        const auto displayName = asset.name.isNotEmpty() ? asset.name : asset.refKey;
        if (!changed)
        {
            listBox.repaint();
            setStatus("Reimported (cache refreshed): " + displayName, kOk);
            return;
        }

        if (commitAssets("Asset reimported: " + displayName))
            refreshFromDocument();
    }

    void AssetsPanel::replaceSelectedAssetFile()
    {
        const auto index = selectedModelIndex();
        if (index < 0 || index >= static_cast<int>(assets.size()))
        {
            setStatus("No asset selected for replace.", kWarn);
            return;
        }

        if (pendingFileChooser != nullptr)
        {
            setStatus("File chooser already open.", kInfo);
            return;
        }

        const auto& selected = assets[static_cast<size_t>(index)];
        if (selected.kind == AssetKind::colorPreset)
        {
            setStatus("Color preset does not support replace.", kWarn);
            return;
        }

        const auto targetAssetId = selected.id;
        pendingFileChooser = std::make_unique<juce::FileChooser>("Replace asset file");
        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles;

        pendingFileChooser->launchAsync(chooserFlags,
                                        [safe = juce::Component::SafePointer<AssetsPanel>(this), targetAssetId](const juce::FileChooser& chooser)
                                        {
                                            if (safe == nullptr)
                                                return;

                                            const auto file = chooser.getResult();
                                            safe->pendingFileChooser.reset();
                                            if (!file.existsAsFile())
                                            {
                                                safe->setStatus("Replace cancelled.", kInfo);
                                                return;
                                            }
                                            if (!isImportableFile(file))
                                            {
                                                safe->setStatus("Selected file is not importable.", kWarn);
                                                return;
                                            }

                                            const auto it = std::find_if(safe->assets.begin(),
                                                                         safe->assets.end(),
                                                                         [targetAssetId](const AssetModel& asset)
                                                                         {
                                                                             return asset.id == targetAssetId;
                                                                         });
                                            if (it == safe->assets.end())
                                            {
                                                safe->setStatus("Replace failed: target asset missing.", kWarn);
                                                return;
                                            }

                                            it->kind = inferAssetKindFromFile(file);
                                            it->relativePath = normalizeRelativePath(resolveRelativePath(file));
                                            it->mimeType = inferMimeTypeFromFile(file);
                                            safe->thumbnailCache.clear();
                                            if (safe->previewAssetId == targetAssetId)
                                                safe->stopAudioPreview();

                                            const auto displayName = it->name.isNotEmpty() ? it->name : it->refKey;
                                            if (safe->commitAssets("Asset file replaced: " + displayName))
                                                safe->refreshFromDocument();
                                        });
    }

    void AssetsPanel::removeUnusedAssets()
    {
        std::unordered_set<WidgetId> removableIds;
        for (const auto& asset : assets)
        {
            if (usageCountForAsset(asset.id) <= 0)
                removableIds.insert(asset.id);
        }

        if (removableIds.empty())
        {
            setStatus("No unused assets to clean.", kInfo);
            return;
        }

        const auto previousCount = static_cast<int>(assets.size());
        assets.erase(std::remove_if(assets.begin(),
                                    assets.end(),
                                    [&removableIds](const AssetModel& asset)
                                    {
                                        return removableIds.count(asset.id) > 0;
                                    }),
                     assets.end());

        const auto removedCount = previousCount - static_cast<int>(assets.size());
        if (removedCount <= 0)
        {
            setStatus("No unused assets removed.", kInfo);
            return;
        }

        if (commitAssets("Unused assets removed: " + juce::String(removedCount)))
            refreshFromDocument();
    }

    void AssetsPanel::mergeDuplicateAssets()
    {
        if (assets.empty())
        {
            setStatus("No assets available to merge.", kInfo);
            return;
        }

        auto signatureForAsset = [this](const AssetModel& asset, juce::String& basisOut) -> juce::String
        {
            basisOut.clear();
            const auto kindKey = assetKindToKey(asset.kind);

            if (asset.kind == AssetKind::colorPreset)
            {
                const auto value = asset.meta.contains("value")
                                     ? asset.meta["value"].toString().trim().toLowerCase()
                                     : juce::String();
                if (value.isNotEmpty())
                {
                    basisOut = "color";
                    return "color:" + value;
                }
            }
            else
            {
                const auto sourceFile = resolveInputFilePath(asset.relativePath);
                const auto fingerprint = fingerprintForFile(sourceFile);
                if (fingerprint.isNotEmpty())
                {
                    basisOut = "hash";
                    return "hash:" + kindKey + ":" + fingerprint;
                }

                const auto normalizedPath = normalizeRelativePath(asset.relativePath).trim().toLowerCase();
                if (normalizedPath.isNotEmpty())
                {
                    basisOut = "path";
                    return "path:" + kindKey + ":" + normalizedPath;
                }
            }

            const auto normalizedName = asset.name.trim().toLowerCase();
            if (normalizedName.isNotEmpty())
            {
                basisOut = "name";
                return "name:" + kindKey + ":" + normalizedName + ":" + asset.mimeType.trim().toLowerCase();
            }

            return {};
        };

        std::unordered_map<WidgetId, juce::String> refByAssetId;
        refByAssetId.reserve(assets.size());
        for (const auto& asset : assets)
            refByAssetId[asset.id] = asset.refKey.trim();

        std::unordered_map<juce::String, WidgetId> keeperBySignature;
        std::unordered_map<WidgetId, std::vector<WidgetId>> duplicateIdsByKeeper;
        std::unordered_map<juce::String, juce::String> refRemap;
        std::unordered_set<WidgetId> duplicateIds;
        int hashMatchCount = 0;
        int pathMatchCount = 0;
        int nameMatchCount = 0;

        for (const auto& asset : assets)
        {
            juce::String matchBasis;
            const auto signature = signatureForAsset(asset, matchBasis);
            if (signature.isEmpty())
                continue;

            const auto [it, inserted] = keeperBySignature.emplace(signature, asset.id);
            if (inserted)
                continue;

            const auto keeperId = it->second;
            if (keeperId <= kRootId || keeperId == asset.id)
                continue;

            duplicateIds.insert(asset.id);
            duplicateIdsByKeeper[keeperId].push_back(asset.id);

            if (matchBasis == "hash")
                ++hashMatchCount;
            else if (matchBasis == "path")
                ++pathMatchCount;
            else
                ++nameMatchCount;

            const auto oldRef = asset.refKey.trim();
            const auto keeperRefIt = refByAssetId.find(keeperId);
            if (oldRef.isEmpty() || keeperRefIt == refByAssetId.end())
                continue;
            if (oldRef.equalsIgnoreCase(keeperRefIt->second))
                continue;

            refRemap[oldRef.toLowerCase()] = keeperRefIt->second;
        }

        if (duplicateIds.empty())
        {
            setStatus("No duplicates detected.", kInfo);
            return;
        }

        auto remapRefKey = [&refRemap](const juce::String& current) -> juce::String
        {
            const auto key = current.trim().toLowerCase();
            if (key.isEmpty())
                return {};

            if (const auto found = refRemap.find(key); found != refRemap.end())
                return found->second;
            return {};
        };

        bool widgetRefsChanged = false;
        if (!refRemap.empty())
        {
            constexpr const char* kCoalescedKey = "assets.merge.refs";
            if (!document.beginCoalescedEdit(kCoalescedKey))
            {
                setStatus("Failed to start reference remap edit.", kError);
                return;
            }

            const auto& snapshot = document.snapshot();
            bool remapFailed = false;
            for (const auto& widget : snapshot.widgets)
            {
                const auto* specs = widgetFactory.propertySpecsFor(widget.type);
                if (specs == nullptr)
                    continue;

                WidgetPropsPatch patch;
                bool patchChanged = false;

                for (const auto& spec : *specs)
                {
                    if (spec.kind != Widgets::WidgetPropertyKind::assetRef)
                        continue;

                    const auto* value = widget.properties.getVarPointer(spec.key);
                    if (value == nullptr)
                        continue;

                    const auto currentRef = value->toString().trim();
                    const auto remappedRef = remapRefKey(currentRef);
                    if (remappedRef.isEmpty() || currentRef.equalsIgnoreCase(remappedRef))
                        continue;

                    patch.patch.set(spec.key, remappedRef);
                    patchChanged = true;
                }

                if (!patchChanged)
                    continue;

                SetPropsAction action;
                action.kind = NodeKind::widget;
                action.ids = { widget.id };
                action.patch = patch;
                if (!document.previewSetProps(action))
                {
                    remapFailed = true;
                    break;
                }

                widgetRefsChanged = true;
            }

            if (remapFailed)
            {
                document.endCoalescedEdit(kCoalescedKey, false);
                setStatus("Failed to remap widget references for merge.", kError);
                return;
            }

            if (!document.endCoalescedEdit(kCoalescedKey, widgetRefsChanged))
            {
                setStatus("Failed to finalize widget reference remap.", kError);
                return;
            }
        }

        bool runtimeRefsChanged = false;
        if (!refRemap.empty())
        {
            const auto& snapshot = document.snapshot();
            auto nextBindings = snapshot.runtimeBindings;

            std::unordered_map<WidgetId, const WidgetModel*> widgetById;
            widgetById.reserve(snapshot.widgets.size());
            for (const auto& widget : snapshot.widgets)
                widgetById.emplace(widget.id, &widget);

            for (auto& binding : nextBindings)
            {
                for (auto& action : binding.actions)
                {
                    if (action.kind != RuntimeActionKind::setNodeProps)
                        continue;
                    if (action.target.kind != NodeKind::widget || action.target.id <= kRootId)
                        continue;

                    const auto widgetIt = widgetById.find(action.target.id);
                    if (widgetIt == widgetById.end() || widgetIt->second == nullptr)
                        continue;

                    const auto* targetWidget = widgetIt->second;
                    for (int i = 0; i < action.patch.size(); ++i)
                    {
                        const auto propertyKey = action.patch.getName(i);
                        const auto* spec = widgetFactory.propertySpecFor(targetWidget->type, propertyKey);
                        if (spec == nullptr || spec->kind != Widgets::WidgetPropertyKind::assetRef)
                            continue;

                        const auto currentRef = action.patch.getValueAt(i).toString().trim();
                        const auto remappedRef = remapRefKey(currentRef);
                        if (remappedRef.isEmpty() || currentRef.equalsIgnoreCase(remappedRef))
                            continue;

                        action.patch.set(propertyKey, remappedRef);
                        runtimeRefsChanged = true;
                    }
                }
            }

            if (runtimeRefsChanged && !document.setRuntimeBindings(std::move(nextBindings)))
            {
                setStatus("Failed to remap runtime binding references for merge.", kError);
                return;
            }
        }

        const auto& latestAssets = document.snapshot().assets;
        assets = latestAssets;

        const auto beforeCount = static_cast<int>(assets.size());
        assets.erase(std::remove_if(assets.begin(),
                                    assets.end(),
                                    [&duplicateIds](const AssetModel& asset)
                                    {
                                        return duplicateIds.count(asset.id) > 0;
                                    }),
                     assets.end());
        const auto removedCount = beforeCount - static_cast<int>(assets.size());
        if (removedCount <= 0)
        {
            setStatus("Duplicate merge skipped: no removable assets remained.", kInfo);
            return;
        }

        if (previewAssetId > kRootId && duplicateIds.count(previewAssetId) > 0)
            stopAudioPreview();
        if (duplicateIds.count(selectedAssetId) > 0)
            selectedAssetId = kRootId;

        if (commitAssets("Duplicate assets merged: " + juce::String(removedCount)))
        {
            refreshFromDocument();
            setStatus("Merged " + juce::String(removedCount)
                          + " duplicate assets (hash=" + juce::String(hashMatchCount)
                          + ", path=" + juce::String(pathMatchCount)
                          + ", name=" + juce::String(nameMatchCount)
                          + ").",
                      kOk);
        }
    }

    void AssetsPanel::relinkMissingAssets()
    {
        if (pendingFileChooser != nullptr)
        {
            setStatus("File chooser already open.", kInfo);
            return;
        }

        int missingCount = 0;
        for (const auto& asset : assets)
        {
            if (asset.kind == AssetKind::colorPreset)
                continue;
            if (asset.relativePath.trim().isEmpty())
                continue;
            if (!resolveInputFilePath(asset.relativePath).existsAsFile())
                ++missingCount;
        }

        if (missingCount <= 0)
        {
            setStatus("No missing asset paths found.", kInfo);
            return;
        }

        pendingFileChooser = std::make_unique<juce::FileChooser>("Select relink search root",
                                                                  resolveProjectRootDirectory());
        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories;
        pendingFileChooser->launchAsync(chooserFlags,
                                        [safe = juce::Component::SafePointer<AssetsPanel>(this)](const juce::FileChooser& chooser)
                                        {
                                            if (safe == nullptr)
                                                return;

                                            const auto searchRoot = chooser.getResult();
                                            safe->pendingFileChooser.reset();
                                            if (!searchRoot.isDirectory())
                                            {
                                                safe->setStatus("Relink cancelled.", kInfo);
                                                return;
                                            }

                                            std::unordered_map<juce::String, std::vector<juce::File>> filesByName;
                                            for (const auto& entry : juce::RangedDirectoryIterator(searchRoot,
                                                                                                    true,
                                                                                                    "*",
                                                                                                    juce::File::findFiles))
                                            {
                                                const auto file = entry.getFile();
                                                if (!file.existsAsFile())
                                                    continue;
                                                filesByName[file.getFileName().toLowerCase()].push_back(file);
                                            }

                                            const auto projectRoot = safe->resolveProjectRootDirectory();
                                            int relinkedCount = 0;
                                            int unresolvedCount = 0;
                                            for (auto& asset : safe->assets)
                                            {
                                                if (asset.kind == AssetKind::colorPreset)
                                                    continue;

                                                const auto normalizedCurrent = normalizeRelativePath(asset.relativePath);
                                                if (normalizedCurrent.isEmpty())
                                                    continue;

                                                if (safe->resolveInputFilePath(normalizedCurrent).existsAsFile())
                                                    continue;

                                                const auto fileName = juce::File(normalizedCurrent).getFileName().toLowerCase();
                                                if (fileName.isEmpty())
                                                {
                                                    ++unresolvedCount;
                                                    continue;
                                                }

                                                const auto found = filesByName.find(fileName);
                                                if (found == filesByName.end() || found->second.empty())
                                                {
                                                    ++unresolvedCount;
                                                    continue;
                                                }

                                                juce::File chosen = found->second.front();
                                                const auto normalizedHint = normalizedCurrent.toLowerCase();
                                                int bestScore = -1;
                                                for (const auto& candidate : found->second)
                                                {
                                                    auto candidateRelative = normalizeRelativePath(candidate.getRelativePathFrom(searchRoot))
                                                                 .toLowerCase();
                                                    int score = 0;
                                                    if (candidateRelative.equalsIgnoreCase(normalizedHint))
                                                        score += 1000;
                                                    if (normalizedHint.isNotEmpty() && candidateRelative.endsWithIgnoreCase(normalizedHint))
                                                        score += 200;
                                                    if (candidate.getFileExtension().equalsIgnoreCase(juce::File(normalizedCurrent).getFileExtension()))
                                                        score += 50;
                                                    score -= candidateRelative.length();

                                                    if (score > bestScore)
                                                    {
                                                        bestScore = score;
                                                        chosen = candidate;
                                                    }
                                                }

                                                auto nextRelative = normalizeRelativePath(chosen.getRelativePathFrom(projectRoot)
                                                                                              .replaceCharacter('\\', '/'));
                                                if (nextRelative.isEmpty())
                                                    nextRelative = normalizeRelativePath(chosen.getFileName());
                                                if (nextRelative.isEmpty())
                                                {
                                                    ++unresolvedCount;
                                                    continue;
                                                }

                                                asset.relativePath = nextRelative;
                                                asset.kind = inferAssetKindFromFile(chosen);
                                                asset.mimeType = inferMimeTypeFromFile(chosen);
                                                ++relinkedCount;
                                            }

                                            if (relinkedCount <= 0)
                                            {
                                                safe->setStatus("Relink finished: no paths updated (" + juce::String(unresolvedCount) + " unresolved).",
                                                                kWarn);
                                                return;
                                            }

                                            safe->thumbnailCache.clear();
                                            juce::ImageCache::releaseUnusedImages();
                                            if (safe->previewAssetId > kRootId)
                                            {
                                                const auto selectedPreviewIt = std::find_if(safe->assets.begin(),
                                                                                            safe->assets.end(),
                                                                                            [safe](const AssetModel& asset)
                                                                                            {
                                                                                                return asset.id == safe->previewAssetId;
                                                                                            });
                                                if (selectedPreviewIt != safe->assets.end())
                                                {
                                                    if (!safe->resolveInputFilePath(selectedPreviewIt->relativePath).existsAsFile())
                                                        safe->stopAudioPreview();
                                                }
                                                else
                                                {
                                                    safe->stopAudioPreview();
                                                }
                                            }

                                            if (safe->commitAssets("Missing assets relinked: " + juce::String(relinkedCount)))
                                            {
                                                safe->refreshFromDocument();
                                                safe->setStatus("Relinked " + juce::String(relinkedCount)
                                                                    + " assets (" + juce::String(unresolvedCount) + " unresolved).",
                                                                unresolvedCount > 0 ? kWarn : kOk);
                                            }
                                        });
    }

    void AssetsPanel::exportAssetPackage()
    {
        if (pendingFileChooser != nullptr)
        {
            setStatus("File chooser already open.", kInfo);
            return;
        }

        pendingFileChooser = std::make_unique<juce::FileChooser>("Export asset package", juce::File(), "*.zip");
        const auto chooserFlags = juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting;

        pendingFileChooser->launchAsync(chooserFlags,
                                        [safe = juce::Component::SafePointer<AssetsPanel>(this)](const juce::FileChooser& chooser)
                                        {
                                            if (safe == nullptr)
                                                return;

                                            auto packageFile = chooser.getResult();
                                            safe->pendingFileChooser.reset();

                                            if (packageFile.getFullPathName().trim().isEmpty())
                                            {
                                                safe->setStatus("Package export cancelled.", kInfo);
                                                return;
                                            }

                                            if (!packageFile.hasFileExtension("zip"))
                                                packageFile = packageFile.withFileExtension(".zip");

                                            const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                                                      .getChildFile("GyeolAssetPackage_" + juce::Uuid().toString());
                                            if (!tempRoot.createDirectory())
                                            {
                                                safe->setStatus("Failed to create temp folder for package.", kError);
                                                return;
                                            }

                                            const auto cleanup = [&tempRoot]() { tempRoot.deleteRecursively(); };

                                            juce::Array<juce::var> manifestAssets;
                                            std::set<juce::String> usedRelativePaths;
                                            int exportedCount = 0;
                                            int skippedCount = 0;

                                            for (const auto& asset : safe->assets)
                                            {
                                                auto item = std::make_unique<juce::DynamicObject>();
                                                item->setProperty("name", asset.name);
                                                item->setProperty("kind", assetKindToKey(asset.kind));
                                                item->setProperty("refKey", asset.refKey);
                                                item->setProperty("mime", asset.mimeType);
                                                item->setProperty("meta", propertyBagToVar(asset.meta));

                                                if (asset.kind == AssetKind::colorPreset)
                                                {
                                                    item->setProperty("path", juce::String());
                                                    manifestAssets.add(juce::var(item.release()));
                                                    ++exportedCount;
                                                    continue;
                                                }

                                                if (asset.relativePath.trim().isEmpty())
                                                {
                                                    ++skippedCount;
                                                    continue;
                                                }

                                                const auto source = safe->resolveInputFilePath(asset.relativePath);
                                                if (!source.existsAsFile())
                                                {
                                                    ++skippedCount;
                                                    continue;
                                                }

                                                auto relativePath = normalizeRelativePath(asset.relativePath);
                                                if (relativePath.startsWithIgnoreCase("Assets/"))
                                                    relativePath = relativePath.fromFirstOccurrenceOf("Assets/", false, false);
                                                if (relativePath.isEmpty())
                                                    relativePath = source.getFileName();

                                                relativePath = "Assets/" + normalizeRelativePath(relativePath);
                                                auto candidate = relativePath;
                                                if (usedRelativePaths.count(candidate) != 0)
                                                {
                                                    const auto fileName = juce::File(candidate).getFileName();
                                                    const auto stem = juce::File(fileName).getFileNameWithoutExtension();
                                                    const auto ext = juce::File(fileName).getFileExtension();
                                                    auto parent = candidate.upToLastOccurrenceOf(fileName, false, false).trimCharactersAtEnd("/");
                                                    int suffix = 2;
                                                    do
                                                    {
                                                        const auto suffixed = stem + "_" + juce::String(suffix++) + ext;
                                                        candidate = parent.isNotEmpty() ? (parent + "/" + suffixed) : suffixed;
                                                    }
                                                    while (usedRelativePaths.count(candidate) != 0);
                                                }

                                                usedRelativePaths.insert(candidate);
                                                const auto destination = tempRoot.getChildFile(candidate);
                                                const auto destinationParent = destination.getParentDirectory();
                                                if (!destinationParent.exists() && !destinationParent.createDirectory())
                                                {
                                                    ++skippedCount;
                                                    continue;
                                                }

                                                if (!source.copyFileTo(destination))
                                                {
                                                    ++skippedCount;
                                                    continue;
                                                }

                                                item->setProperty("path", candidate);
                                                manifestAssets.add(juce::var(item.release()));
                                                ++exportedCount;
                                            }

                                            auto manifestObject = std::make_unique<juce::DynamicObject>();
                                            manifestObject->setProperty("schema", kPackageSchema);

                                            auto versionObject = std::make_unique<juce::DynamicObject>();
                                            versionObject->setProperty("major", 1);
                                            versionObject->setProperty("minor", 0);
                                            versionObject->setProperty("patch", 0);
                                            manifestObject->setProperty("version", juce::var(versionObject.release()));

                                            manifestObject->setProperty("exportedAtUtc", juce::Time::getCurrentTime().toISO8601(true));
                                            manifestObject->setProperty("assets", juce::var(manifestAssets));

                                            const auto manifestFile = tempRoot.getChildFile(kPackageManifestFile);
                                            if (!manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifestObject.release()), true)))
                                            {
                                                cleanup();
                                                safe->setStatus("Failed to write package manifest.", kError);
                                                return;
                                            }

                                            juce::ZipFile::Builder builder;
                                            for (const auto& entry : juce::RangedDirectoryIterator(tempRoot,
                                                                                                    true,
                                                                                                    "*",
                                                                                                    juce::File::findFiles))
                                            {
                                                const auto file = entry.getFile();
                                                if (!file.existsAsFile())
                                                    continue;

                                                auto storedPath = file.getRelativePathFrom(tempRoot).replaceCharacter('\\', '/');
                                                if (storedPath.isEmpty())
                                                    storedPath = file.getFileName();
                                                builder.addFile(file, 9, storedPath);
                                            }

                                            if (packageFile.existsAsFile())
                                                packageFile.deleteFile();

                                            juce::FileOutputStream output(packageFile);
                                            if (!output.openedOk())
                                            {
                                                cleanup();
                                                safe->setStatus("Failed to open package file for writing.", kError);
                                                return;
                                            }

                                            double progress = 0.0;
                                            if (!builder.writeToStream(output, &progress))
                                            {
                                                cleanup();
                                                safe->setStatus("Failed to write zip package.", kError);
                                                return;
                                            }

                                            cleanup();
                                            safe->setStatus("Package exported: " + juce::String(exportedCount)
                                                            + " assets (" + juce::String(skippedCount) + " skipped).",
                                                            kOk);
                                        });
    }

    void AssetsPanel::importAssetPackage()
    {
        if (pendingFileChooser != nullptr)
        {
            setStatus("File chooser already open.", kInfo);
            return;
        }

        pendingFileChooser = std::make_unique<juce::FileChooser>("Import asset package", juce::File(), "*.zip");
        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles;

        pendingFileChooser->launchAsync(chooserFlags,
                                        [safe = juce::Component::SafePointer<AssetsPanel>(this)](const juce::FileChooser& chooser)
                                        {
                                            if (safe == nullptr)
                                                return;

                                            const auto packageFile = chooser.getResult();
                                            safe->pendingFileChooser.reset();

                                            if (!packageFile.existsAsFile())
                                            {
                                                safe->setStatus("Package import cancelled.", kInfo);
                                                return;
                                            }

                                            juce::ZipFile zip(packageFile);
                                            const auto manifestIndex = zip.getIndexOfFileName(kPackageManifestFile, true);
                                            if (manifestIndex < 0)
                                            {
                                                safe->setStatus("Invalid package: manifest not found.", kError);
                                                return;
                                            }

                                            auto manifestStream = std::unique_ptr<juce::InputStream>(zip.createStreamForEntry(manifestIndex));
                                            if (manifestStream == nullptr)
                                            {
                                                safe->setStatus("Invalid package: cannot read manifest.", kError);
                                                return;
                                            }

                                            const auto manifestText = manifestStream->readEntireStreamAsString();
                                            auto parsed = juce::JSON::parse(manifestText);
                                            const auto* manifestObject = parsed.getDynamicObject();
                                            if (manifestObject == nullptr)
                                            {
                                                safe->setStatus("Invalid package: malformed manifest JSON.", kError);
                                                return;
                                            }

                                            const auto& manifestProps = manifestObject->getProperties();
                                            if (manifestProps.getWithDefault("schema", juce::var()).toString() != kPackageSchema)
                                            {
                                                safe->setStatus("Invalid package: schema mismatch.", kError);
                                                return;
                                            }

                                            if (!manifestProps.contains("version") || !manifestProps["version"].isObject())
                                            {
                                                safe->setStatus("Invalid package: version is required.", kError);
                                                return;
                                            }

                                            const auto* versionObject = manifestProps["version"].getDynamicObject();
                                            const auto& versionProps = versionObject->getProperties();
                                            const auto major = static_cast<int>(versionProps.getWithDefault("major", juce::var(-1)));
                                            if (major != 1)
                                            {
                                                safe->setStatus("Unsupported package major version: " + juce::String(major), kError);
                                                return;
                                            }

                                            if (!manifestProps.contains("assets") || !manifestProps["assets"].isArray())
                                            {
                                                safe->setStatus("Invalid package: assets array is required.", kError);
                                                return;
                                            }

                                            const auto extractRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                                                        .getChildFile("GyeolAssetImport_" + juce::Uuid().toString());
                                            if (!extractRoot.createDirectory())
                                            {
                                                safe->setStatus("Failed to create temp import folder.", kError);
                                                return;
                                            }

                                            const auto unzipResult = zip.uncompressTo(extractRoot, true);
                                            if (unzipResult.failed())
                                            {
                                                extractRoot.deleteRecursively();
                                                safe->setStatus("Failed to extract package: " + unzipResult.getErrorMessage(), kError);
                                                return;
                                            }

                                            auto nextId = safe->allocateNextAssetId();
                                            std::vector<AssetModel> mergedAssets = safe->assets;

                                            const auto conflictPolicy = safe->selectedImportConflictPolicy();
                                            auto normalizeRefToken = [](const juce::String& seedInput) -> juce::String
                                            {
                                                auto seed = seedInput.trim().toLowerCase();
                                                if (seed.startsWith("asset."))
                                                    seed = seed.fromFirstOccurrenceOf("asset.", false, false);

                                                juce::String token;
                                                for (auto c : seed)
                                                {
                                                    if (juce::CharacterFunctions::isLetterOrDigit(c))
                                                        token += juce::String::charToString(c);
                                                    else if (c == '-' || c == '_' || c == '.')
                                                        token += juce::String::charToString(c);
                                                    else if (juce::CharacterFunctions::isWhitespace(c))
                                                        token += "_";
                                                }

                                                while (token.contains("__"))
                                                    token = token.replace("__", "_");
                                                token = token.trimCharactersAtStart("._").trimCharactersAtEnd("._");
                                                return "asset." + (token.isNotEmpty() ? token : juce::String("item"));
                                            };
                                            auto findAssetIndexByRef = [&mergedAssets](const juce::String& ref) -> int
                                            {
                                                for (size_t i = 0; i < mergedAssets.size(); ++i)
                                                {
                                                    if (mergedAssets[i].refKey.trim().equalsIgnoreCase(ref.trim()))
                                                        return static_cast<int>(i);
                                                }
                                                return -1;
                                            };
                                            auto makeUniqueImportedRef = [&mergedAssets, normalizeRefToken](const juce::String& seedInput) -> juce::String
                                            {
                                                auto candidate = normalizeRefToken(seedInput);
                                                const auto exists = [&mergedAssets](const juce::String& ref) -> bool
                                                {
                                                    return std::any_of(mergedAssets.begin(),
                                                                       mergedAssets.end(),
                                                                       [&ref](const AssetModel& model)
                                                                       {
                                                                           return model.refKey.trim().equalsIgnoreCase(ref.trim());
                                                                       });
                                                };

                                                if (!exists(candidate))
                                                    return candidate;

                                                int suffix = 2;
                                                while (exists(candidate + "_" + juce::String(suffix)))
                                                    ++suffix;

                                                return candidate + "_" + juce::String(suffix);
                                            };
                                            const auto projectRoot = safe->resolveProjectRootDirectory();
                                            const auto importRoot = projectRoot.getChildFile("Assets")
                                                                          .getChildFile("Imported")
                                                                          .getChildFile(packageFile.getFileNameWithoutExtension());
                                            importRoot.createDirectory();

                                            const auto* assetsArray = manifestProps["assets"].getArray();
                                            int importedCount = 0;
                                            int skippedCount = 0;
                                            int overwrittenCount = 0;
                                            int renamedRefCount = 0;
                                            int renamedPathCount = 0;

                                            for (const auto& assetVar : *assetsArray)
                                            {
                                                const auto* assetObj = assetVar.getDynamicObject();
                                                if (assetObj == nullptr)
                                                {
                                                    ++skippedCount;
                                                    continue;
                                                }

                                                const auto& assetProps = assetObj->getProperties();
                                                const auto kindString = assetProps.getWithDefault("kind", juce::var()).toString();
                                                const auto parsedKind = assetKindFromKey(kindString);
                                                if (!parsedKind.has_value())
                                                {
                                                    ++skippedCount;
                                                    continue;
                                                }

                                                auto refKeyRaw = assetProps.getWithDefault("refKey", juce::var()).toString().trim();
                                                if (refKeyRaw.isEmpty())
                                                {
                                                    ++skippedCount;
                                                    continue;
                                                }

                                                const auto normalizedRef = normalizeRefToken(refKeyRaw);
                                                auto targetRef = normalizedRef;
                                                auto existingIndex = findAssetIndexByRef(targetRef);
                                                bool overwriteExistingAsset = false;
                                                if (existingIndex >= 0)
                                                {
                                                    switch (conflictPolicy)
                                                    {
                                                        case ImportConflictPolicy::skip:
                                                            ++skippedCount;
                                                            continue;
                                                        case ImportConflictPolicy::overwrite:
                                                            overwriteExistingAsset = true;
                                                            break;
                                                        case ImportConflictPolicy::rename:
                                                            targetRef = makeUniqueImportedRef(targetRef);
                                                            if (!targetRef.equalsIgnoreCase(normalizedRef))
                                                                ++renamedRefCount;
                                                            existingIndex = -1;
                                                            break;
                                                    }
                                                }

                                                AssetModel imported;
                                                imported.kind = *parsedKind;
                                                imported.name = assetProps.getWithDefault("name", juce::var()).toString();
                                                imported.mimeType = assetProps.getWithDefault("mime", juce::var()).toString();
                                                if (overwriteExistingAsset)
                                                {
                                                    imported.id = mergedAssets[static_cast<size_t>(existingIndex)].id;
                                                    imported.refKey = mergedAssets[static_cast<size_t>(existingIndex)].refKey.trim();
                                                    if (imported.refKey.isEmpty())
                                                        imported.refKey = targetRef;
                                                }
                                                else
                                                {
                                                    imported.id = nextId++;
                                                    imported.refKey = targetRef;
                                                }
                                                if (assetProps.contains("meta"))
                                                {
                                                    PropertyBag meta;
                                                    if (varToPropertyBag(assetProps["meta"], meta).wasOk())
                                                        imported.meta = std::move(meta);
                                                }

                                                if (imported.kind != AssetKind::colorPreset)
                                                {
                                                    const auto packagePath = normalizeRelativePath(assetProps.getWithDefault("path", juce::var()).toString());
                                                    if (packagePath.isEmpty())
                                                    {
                                                        ++skippedCount;
                                                        continue;
                                                    }

                                                    const auto extractedFile = extractRoot.getChildFile(packagePath);
                                                    if (!extractedFile.existsAsFile())
                                                    {
                                                        ++skippedCount;
                                                        continue;
                                                    }

                                                    auto relativeTargetPath = packagePath;
                                                    if (relativeTargetPath.startsWithIgnoreCase("Assets/"))
                                                        relativeTargetPath = relativeTargetPath.fromFirstOccurrenceOf("Assets/", false, false);
                                                    if (relativeTargetPath.isEmpty())
                                                        relativeTargetPath = extractedFile.getFileName();

                                                    auto destination = importRoot.getChildFile(relativeTargetPath);
                                                    if (destination.existsAsFile())
                                                    {
                                                        if (conflictPolicy == ImportConflictPolicy::skip)
                                                        {
                                                            ++skippedCount;
                                                            continue;
                                                        }

                                                        if (conflictPolicy == ImportConflictPolicy::overwrite)
                                                        {
                                                            if (!destination.deleteFile())
                                                            {
                                                                ++skippedCount;
                                                                continue;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            ++renamedPathCount;
                                                            const auto stem = destination.getFileNameWithoutExtension();
                                                            const auto ext = destination.getFileExtension();
                                                            int suffix = 2;
                                                            do
                                                            {
                                                                destination = destination.getSiblingFile(stem + "_" + juce::String(suffix++) + ext);
                                                            }
                                                            while (destination.existsAsFile());
                                                        }
                                                    }

                                                    const auto destinationParent = destination.getParentDirectory();
                                                    if (!destinationParent.exists() && !destinationParent.createDirectory())
                                                    {
                                                        ++skippedCount;
                                                        continue;
                                                    }

                                                    if (!extractedFile.copyFileTo(destination))
                                                    {
                                                        ++skippedCount;
                                                        continue;
                                                    }

                                                    imported.relativePath = destination.getRelativePathFrom(projectRoot).replaceCharacter('\\', '/');
                                                    if (imported.mimeType.trim().isEmpty())
                                                        imported.mimeType = inferMimeTypeFromFile(destination);
                                                }
                                                else
                                                {
                                                    imported.relativePath.clear();
                                                    if (imported.mimeType.trim().isEmpty())
                                                        imported.mimeType = "application/x-color-preset";
                                                }

                                                if (imported.name.trim().isEmpty())
                                                    imported.name = "Imported Asset " + juce::String(imported.id);

                                                safe->selectedAssetId = imported.id;
                                                if (overwriteExistingAsset)
                                                {
                                                    if (safe->previewAssetId == imported.id)
                                                        safe->stopAudioPreview();
                                                    mergedAssets[static_cast<size_t>(existingIndex)] = std::move(imported);
                                                    ++overwrittenCount;
                                                }
                                                else
                                                {
                                                    mergedAssets.push_back(std::move(imported));
                                                    ++importedCount;
                                                }
                                            }

                                            extractRoot.deleteRecursively();

                                            if (importedCount == 0 && overwrittenCount == 0)
                                            {
                                                safe->setStatus("No assets imported (all entries skipped).", kWarn);
                                                return;
                                            }

                                            safe->assets = std::move(mergedAssets);
                                            if (safe->commitAssets("Assets package imported"))
                                            {
                                                safe->refreshFromDocument();
                                                safe->setStatus("Package import applied: +"
                                                                + juce::String(importedCount)
                                                                + ", overwritten=" + juce::String(overwrittenCount)
                                                                + ", skipped=" + juce::String(skippedCount)
                                                                + ", refRenamed=" + juce::String(renamedRefCount)
                                                                + ", pathRenamed=" + juce::String(renamedPathCount) + ".",
                                                                kOk);
                                            }
                                        });
    }

    void AssetsPanel::deleteSelectedAsset()
    {
        const auto index = selectedModelIndex();
        if (index < 0 || index >= static_cast<int>(assets.size()))
            return;

        const auto removedName = assets[static_cast<size_t>(index)].name;
        assets.erase(assets.begin() + index);
        selectedAssetId = kRootId;

        if (commitAssets("Asset deleted: " + removedName))
            refreshFromDocument();
    }

    void AssetsPanel::copySelectedRefKey()
    {
        const auto* asset = selectedAsset();
        if (asset == nullptr)
            return;

        juce::SystemClipboard::copyTextToClipboard(asset->refKey);
        setStatus("Copied refKey: " + asset->refKey, kInfo);
    }

    void AssetsPanel::startDragForRow(int row, juce::Component& sourceComponent, juce::Point<int> dragStartPos)
    {
        if (row < 0 || row >= static_cast<int>(visibleAssetIndices.size()))
            return;

        const auto modelIndex = visibleAssetIndices[static_cast<size_t>(row)];
        if (modelIndex < 0 || modelIndex >= static_cast<int>(assets.size()))
            return;

        const auto& asset = assets[static_cast<size_t>(modelIndex)];
        if (asset.refKey.trim().isEmpty())
            return;

        auto payload = std::make_unique<juce::DynamicObject>();
        payload->setProperty("kind", "assetRef");
        payload->setProperty("source", "assetsPanel");
        payload->setProperty("assetId", juce::String(asset.id));
        payload->setProperty("refKey", asset.refKey);
        payload->setProperty("name", asset.name);
        payload->setProperty("mime", asset.mimeType);
        payload->setProperty("assetKind", assetKindToKey(asset.kind));

        const juce::ScaledImage dragImage(sourceComponent.createComponentSnapshot(sourceComponent.getLocalBounds()));
        startDragging(juce::var(payload.release()),
                      &sourceComponent,
                      dragImage,
                      true,
                      &dragStartPos);
    }

    void AssetsPanel::syncRefEditorFromSelection()
    {
        const auto* asset = selectedAsset();
        if (asset == nullptr)
        {
            refKeyEditor.clear();
            return;
        }

        refKeyEditor.setText(asset->refKey, juce::dontSendNotification);
    }

    void AssetsPanel::syncExportIncludeToggleFromSelection()
    {
        const auto* asset = selectedAsset();
        if (asset == nullptr)
        {
            exportIncludeToggle.setToggleState(false, juce::dontSendNotification);
            return;
        }

        exportIncludeToggle.setToggleState(!isAssetExcludedFromExport(*asset), juce::dontSendNotification);
    }

    void AssetsPanel::applyExportIncludeToggle()
    {
        const auto index = selectedModelIndex();
        if (index < 0 || index >= static_cast<int>(assets.size()))
            return;

        auto& asset = assets[static_cast<size_t>(index)];
        const auto shouldInclude = exportIncludeToggle.getToggleState();
        const auto currentlyExcluded = isAssetExcludedFromExport(asset);
        const auto nextExcluded = !shouldInclude;
        if (currentlyExcluded == nextExcluded)
            return;

        if (nextExcluded)
            asset.meta.set("export.exclude", true);
        else
            asset.meta.remove("export.exclude");

        if (commitAssets(nextExcluded ? "Asset excluded from export" : "Asset included in export"))
            refreshFromDocument();
    }

    void AssetsPanel::applyRefKeyEdit()
    {
        const auto* asset = selectedAsset();
        if (asset == nullptr)
            return;

        auto userInput = refKeyEditor.getText().trim().toLowerCase();
        if (userInput.startsWith("asset."))
            userInput = userInput.fromFirstOccurrenceOf("asset.", false, false);

        const auto sanitized = sanitizeRefToken(userInput);
        if (sanitized.isEmpty())
        {
            setStatus("refKey must not be empty.", kWarn);
            return;
        }

        const auto newRefKey = makeUniqueRefKey(sanitized, asset->id);
        const auto oldRefKey = asset->refKey;
        if (oldRefKey == newRefKey)
        {
            setStatus("refKey unchanged.", kInfo);
            return;
        }

        if (!document.replaceAssetRefKey(oldRefKey, newRefKey))
        {
            setStatus("Failed to update refKey.", kError);
            return;
        }

        selectedAssetId = asset->id;
        refreshFromDocument();
        syncRefEditorFromSelection();

        juce::String status = "Updated refKey: " + oldRefKey + " -> " + newRefKey;
        if (!newRefKey.equalsIgnoreCase("asset." + sanitized))
            status += " (unique suffix applied)";

        setStatus(status, kOk);
        if (onAssetsChanged != nullptr)
            onAssetsChanged("Asset refKey updated");
    }

    WidgetId AssetsPanel::allocateNextAssetId() const
    {
        WidgetId maxId = kRootId;
        for (const auto& asset : assets)
            maxId = std::max(maxId, asset.id);

        const auto& snapshot = document.snapshot();
        for (const auto& widget : snapshot.widgets)
            maxId = std::max(maxId, widget.id);
        for (const auto& group : snapshot.groups)
            maxId = std::max(maxId, group.id);
        for (const auto& layer : snapshot.layers)
            maxId = std::max(maxId, layer.id);

        if (maxId >= std::numeric_limits<WidgetId>::max())
            return std::numeric_limits<WidgetId>::max();

        return maxId + 1;
    }

    juce::String AssetsPanel::makeUniqueRefKey(const juce::String& seed, WidgetId ignoreAssetId) const
    {
        const auto base = sanitizeRefToken(seed);
        juce::String candidate = "asset." + (base.isNotEmpty() ? base : juce::String("item"));

        auto exists = [this, ignoreAssetId](const juce::String& refKey) -> bool
        {
            return std::any_of(assets.begin(),
                               assets.end(),
                               [refKey, ignoreAssetId](const AssetModel& asset)
                               {
                                   if (ignoreAssetId > kRootId && asset.id == ignoreAssetId)
                                       return false;
                                   return asset.refKey.trim().equalsIgnoreCase(refKey.trim());
                               });
        };

        if (!exists(candidate))
            return candidate;

        int suffix = 2;
        while (exists(candidate + "_" + juce::String(suffix)))
            ++suffix;

        return candidate + "_" + juce::String(suffix);
    }

    juce::String AssetsPanel::sanitizeRefToken(const juce::String& text)
    {
        juce::String token;
        const auto raw = text.trim().toLowerCase();
        for (auto c : raw)
        {
            if (juce::CharacterFunctions::isLetterOrDigit(c))
                token += juce::String::charToString(c);
            else if (c == '-' || c == '_' || c == '.')
                token += juce::String::charToString(c);
            else if (juce::CharacterFunctions::isWhitespace(c))
                token += "_";
        }

        while (token.contains("__"))
            token = token.replace("__", "_");
        return token.trimCharactersAtStart("._").trimCharactersAtEnd("._");
    }

    juce::String AssetsPanel::normalizeRelativePath(const juce::String& value)
    {
        auto normalized = value.trim().replaceCharacter('\\', '/');
        while (normalized.startsWith("/"))
            normalized = normalized.substring(1);
        while (normalized.contains("//"))
            normalized = normalized.replace("//", "/");
        while (normalized.startsWith("../"))
            normalized = normalized.substring(3);
        while (normalized.contains("/../"))
            normalized = normalized.replace("/../", "/");
        return normalized;
    }

    juce::var AssetsPanel::propertyBagToVar(const PropertyBag& bag)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        for (int i = 0; i < bag.size(); ++i)
            object->setProperty(bag.getName(i), bag.getValueAt(i));

        return juce::var(object.release());
    }

    juce::Result AssetsPanel::varToPropertyBag(const juce::var& value, PropertyBag& outBag)
    {
        outBag.clear();

        if (value.isVoid())
            return juce::Result::ok();

        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("meta must be object");

        const auto& props = object->getProperties();
        for (int i = 0; i < props.size(); ++i)
            outBag.set(props.getName(i), props.getValueAt(i));

        return Gyeol::validatePropertyBag(outBag);
    }

    juce::File AssetsPanel::resolveProjectRootDirectory() const
    {
        auto projectRoot = juce::File::getCurrentWorkingDirectory();
        for (int depth = 0; depth < 10; ++depth)
        {
            if (projectRoot.getChildFile("DadeumStudio.jucer").existsAsFile())
                return projectRoot;

            const auto parent = projectRoot.getParentDirectory();
            if (parent == projectRoot)
                break;
            projectRoot = parent;
        }

        return juce::File::getCurrentWorkingDirectory();
    }

    juce::File AssetsPanel::resolveInputFilePath(const juce::String& value) const
    {
        if (juce::File::isAbsolutePath(value))
            return juce::File(value);

        return resolveProjectRootDirectory().getChildFile(value);
    }

    juce::Image AssetsPanel::getImageThumbnailForAsset(const AssetModel& asset) const
    {
        if (asset.kind != AssetKind::image || asset.relativePath.trim().isEmpty())
            return {};

        const auto cacheKey = normalizeRelativePath(asset.relativePath);
        if (const auto found = thumbnailCache.find(cacheKey); found != thumbnailCache.end())
            return found->second;

        juce::Image image;
        const auto sourceFile = resolveInputFilePath(asset.relativePath);
        if (sourceFile.existsAsFile())
            image = juce::ImageCache::getFromFile(sourceFile);

        thumbnailCache[cacheKey] = image;
        return image;
    }

    bool AssetsPanel::isAudioAsset(const AssetModel& asset)
    {
        const auto mime = asset.mimeType.trim().toLowerCase();
        if (mime.startsWith("audio/"))
            return true;

        const auto normalizedPath = asset.relativePath.trim().replaceCharacter('\\', '/');
        const auto fileName = normalizedPath.fromLastOccurrenceOf("/", false, false);
        juce::String extension;
        if (const auto dotIndex = fileName.lastIndexOfChar('.'); dotIndex >= 0 && dotIndex + 1 < fileName.length())
            extension = fileName.substring(dotIndex + 1).toLowerCase();

        return isSupportedAudioExtension(extension);
    }

    bool AssetsPanel::isAssetPreviewPlaying(WidgetId assetId) const
    {
        return assetId > kRootId
            && previewAssetId == assetId
            && audioTransportSource.isPlaying();
    }

    void AssetsPanel::toggleAudioPreviewForAsset(WidgetId assetId)
    {
        if (assetId <= kRootId)
            return;

        if (previewAssetId == assetId && audioTransportSource.isPlaying())
        {
            stopAudioPreview();
            return;
        }

        const auto it = std::find_if(assets.begin(),
                                     assets.end(),
                                     [assetId](const AssetModel& asset)
                                     {
                                         return asset.id == assetId;
                                     });
        if (it == assets.end())
            return;

        startAudioPreviewForAsset(*it);
    }

    void AssetsPanel::startAudioPreviewForAsset(const AssetModel& asset)
    {
        if (!audioPreviewAvailable)
        {
            setStatus("Audio preview unavailable on this device.", kWarn);
            return;
        }

        if (!isAudioAsset(asset))
        {
            setStatus("Selected asset is not an audio file.", kWarn);
            return;
        }

        if (asset.relativePath.trim().isEmpty())
        {
            setStatus("Audio preview failed: empty file path.", kWarn);
            return;
        }

        const auto sourceFile = resolveInputFilePath(asset.relativePath);
        if (!sourceFile.existsAsFile())
        {
            setStatus("Audio preview failed: file not found.", kWarn);
            return;
        }

        std::unique_ptr<juce::AudioFormatReader> reader(audioFormatManager.createReaderFor(sourceFile));
        if (reader == nullptr)
        {
            setStatus("Audio preview failed: unsupported format.", kWarn);
            return;
        }

        const auto sampleRate = reader->sampleRate;
        stopAudioPreview();

        audioReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
        audioTransportSource.setSource(audioReaderSource.get(), 0, nullptr, sampleRate);
        audioTransportSource.setPosition(0.0);
        previewAssetId = asset.id;
        audioTransportSource.start();
        startTimerHz(6);

        const auto displayName = asset.name.isNotEmpty() ? asset.name : asset.refKey;
        setStatus("Previewing audio: " + displayName, kInfo);
        listBox.repaint();
    }

    void AssetsPanel::stopAudioPreview()
    {
        const auto hadPreview = previewAssetId > kRootId;
        audioTransportSource.stop();
        audioTransportSource.setSource(nullptr);
        audioReaderSource.reset();
        previewAssetId = kRootId;
        stopTimer();

        if (hadPreview)
            listBox.repaint();
    }

    bool AssetsPanel::isImportableFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        const auto extension = file.getFileExtension().trimCharactersAtStart(".").toLowerCase();
        if (extension.isEmpty())
            return false;

        return isSupportedImportExtension(extension);
    }

    bool AssetsPanel::isAssetExcludedFromExport(const AssetModel& asset)
    {
        static constexpr const char* kExportExcludeKey = "export.exclude";
        if (!asset.meta.contains(kExportExcludeKey))
            return false;

        const auto& raw = asset.meta[kExportExcludeKey];
        if (raw.isBool())
            return static_cast<bool>(raw);
        if (raw.isInt() || raw.isInt64() || raw.isDouble())
            return static_cast<double>(raw) != 0.0;

        const auto text = raw.toString().trim().toLowerCase();
        return text == "true" || text == "1" || text == "yes" || text == "on";
    }

    juce::String AssetsPanel::fingerprintForFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return {};

        auto stream = std::unique_ptr<juce::InputStream>(file.createInputStream());
        if (stream == nullptr)
            return {};

        constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
        constexpr std::uint64_t kFnvPrime = 1099511628211ull;
        std::uint64_t hash = kFnvOffset;

        std::array<char, 8192> buffer {};
        while (!stream->isExhausted())
        {
            const auto bytesRead = stream->read(buffer.data(), static_cast<int>(buffer.size()));
            if (bytesRead <= 0)
                break;

            for (int i = 0; i < bytesRead; ++i)
            {
                const auto byte = static_cast<std::uint8_t>(buffer[static_cast<size_t>(i)]);
                hash ^= static_cast<std::uint64_t>(byte);
                hash *= kFnvPrime;
            }
        }

        const auto fileSize = file.getSize();
        return juce::String::toHexString(static_cast<juce::int64>(hash)) + ":" + juce::String(fileSize);
    }

    AssetsPanel::ImportConflictPolicy AssetsPanel::selectedImportConflictPolicy() const
    {
        switch (importConflictCombo.getSelectedId())
        {
            case 2: return ImportConflictPolicy::overwrite;
            case 3: return ImportConflictPolicy::skip;
            default: break;
        }

        return ImportConflictPolicy::rename;
    }

    AssetKind AssetsPanel::inferAssetKindFromFile(const juce::File& file)
    {
        const auto ext = file.getFileExtension().trimCharactersAtStart(".").toLowerCase();
        static const std::array<juce::String, 7> imageExts { "png", "jpg", "jpeg", "bmp", "gif", "svg", "webp" };
        static const std::array<juce::String, 4> fontExts { "ttf", "otf", "woff", "woff2" };

        if (std::find(imageExts.begin(), imageExts.end(), ext) != imageExts.end())
            return AssetKind::image;
        if (std::find(fontExts.begin(), fontExts.end(), ext) != fontExts.end())
            return AssetKind::font;
        return AssetKind::file;
    }

    juce::String AssetsPanel::inferMimeTypeFromFile(const juce::File& file)
    {
        const auto ext = file.getFileExtension().trimCharactersAtStart(".").toLowerCase();
        if (ext == "png") return "image/png";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "bmp") return "image/bmp";
        if (ext == "gif") return "image/gif";
        if (ext == "svg") return "image/svg+xml";
        if (ext == "webp") return "image/webp";
        if (ext == "ttf") return "font/ttf";
        if (ext == "otf") return "font/otf";
        if (ext == "woff") return "font/woff";
        if (ext == "woff2") return "font/woff2";
        if (ext == "wav") return "audio/wav";
        if (ext == "aif" || ext == "aiff") return "audio/aiff";
        if (ext == "ogg") return "audio/ogg";
        if (ext == "flac") return "audio/flac";
        if (ext == "mp3") return "audio/mpeg";
        if (ext == "json") return "application/json";
        return "application/octet-stream";
    }

    juce::String AssetsPanel::kindLabel(AssetKind kind)
    {
        switch (kind)
        {
            case AssetKind::image: return "IMAGE";
            case AssetKind::font: return "FONT";
            case AssetKind::colorPreset: return "COLOR";
            case AssetKind::file: return "FILE";
        }

        return "ASSET";
    }

    juce::Colour AssetsPanel::kindColor(AssetKind kind)
    {
        switch (kind)
        {
            case AssetKind::image: return juce::Colour::fromRGB(111, 177, 255);
            case AssetKind::font: return juce::Colour::fromRGB(189, 152, 255);
            case AssetKind::colorPreset: return juce::Colour::fromRGB(255, 186, 96);
            case AssetKind::file: return juce::Colour::fromRGB(146, 214, 168);
        }

        return juce::Colour::fromRGB(150, 160, 176);
    }

    juce::String AssetsPanel::resolveRelativePath(const juce::File& file)
    {
        auto projectRoot = juce::File::getCurrentWorkingDirectory();
        for (int depth = 0; depth < 10; ++depth)
        {
            if (projectRoot.getChildFile("DadeumStudio.jucer").existsAsFile())
                break;

            const auto parent = projectRoot.getParentDirectory();
            if (parent == projectRoot)
                break;
            projectRoot = parent;
        }

        auto relativePath = file.getRelativePathFrom(projectRoot);
        if (relativePath.isEmpty())
            relativePath = file.getFileName();

        return normalizeRelativePath(relativePath);
    }
}
