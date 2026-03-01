#include "Gyeol/Editor/Panels/ExportPreviewPanel.h"

namespace Gyeol::Ui::Panels
{
    namespace
    {
        void setupReadOnlyEditor(juce::TextEditor& editor)
        {
            editor.setMultiLine(true);
            editor.setReadOnly(true);
            editor.setScrollbarsShown(true);
            editor.setCaretVisible(false);
            editor.setPopupMenuEnabled(true);
            editor.setFont(juce::FontOptions("Consolas", 12.0f, juce::Font::plain));
            editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
            editor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
            editor.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(196, 206, 222));
        }
    }

    ExportPreviewPanel::ExportPreviewPanel()
    {
        titleLabel.setText("Export Preview", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(192, 200, 214));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        setStatusText("Stale", juce::Colour::fromRGB(160, 170, 186));
        addAndMakeVisible(statusLabel);

        classNameLabel.setText("Class", juce::dontSendNotification);
        classNameLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(170, 180, 196));
        classNameLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(classNameLabel);

        classNameEditor.setText("GyeolExportedComponent", juce::dontSendNotification);
        classNameEditor.setInputRestrictions(128, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
        classNameEditor.onTextChange = [this]
        {
            markDirty();
        };
        classNameEditor.onReturnKey = [this]
        {
            markDirty();
            if (autoRefresh)
                refreshPreview();
        };
        addAndMakeVisible(classNameEditor);

        autoRefreshToggle.setClickingTogglesState(true);
        autoRefreshToggle.setToggleState(autoRefresh, juce::dontSendNotification);
        autoRefreshToggle.onClick = [this]
        {
            setAutoRefreshEnabled(autoRefreshToggle.getToggleState());
        };
        addAndMakeVisible(autoRefreshToggle);

        refreshButton.onClick = [this]
        {
            refreshPreview();
        };
        addAndMakeVisible(refreshButton);

        exportButton.onClick = [this]
        {
            if (onExportRequested != nullptr)
                onExportRequested(normalizedClassName());
        };
        addAndMakeVisible(exportButton);

        setupReadOnlyEditor(reportEditor);
        setupReadOnlyEditor(headerEditor);
        setupReadOnlyEditor(sourceEditor);
        setupReadOnlyEditor(manifestEditor);

        tabs.setTabBarDepth(28);
        tabs.addTab("Report", juce::Colour::fromRGB(24, 28, 34), &reportEditor, false);
        tabs.addTab("Header", juce::Colour::fromRGB(24, 28, 34), &headerEditor, false);
        tabs.addTab("Source", juce::Colour::fromRGB(24, 28, 34), &sourceEditor, false);
        tabs.addTab("Manifest", juce::Colour::fromRGB(24, 28, 34), &manifestEditor, false);
        tabs.setCurrentTabIndex(0, juce::dontSendNotification);
        addAndMakeVisible(tabs);
    }

    ExportPreviewPanel::~ExportPreviewPanel() = default;

    void ExportPreviewPanel::setGeneratePreviewCallback(GeneratePreviewCallback callback)
    {
        onGeneratePreview = std::move(callback);
    }

    void ExportPreviewPanel::setExportRequestedCallback(ExportRequestedCallback callback)
    {
        onExportRequested = std::move(callback);
    }

    void ExportPreviewPanel::markDirty()
    {
        dirty = true;
        if (autoRefresh)
            refreshPreview();
        else
            setStatusText("Stale (Generate Preview)", juce::Colour::fromRGB(160, 170, 186));
    }

    void ExportPreviewPanel::refreshPreview()
    {
        if (onGeneratePreview == nullptr)
        {
            clearPreviewEditors();
            setStatusText("Preview callback is not connected", juce::Colour::fromRGB(255, 166, 96));
            return;
        }

        PreviewData data;
        const auto result = onGeneratePreview(normalizedClassName(), data);
        if (result.failed())
        {
            clearPreviewEditors();
            setStatusText("Preview failed: " + result.getErrorMessage(), juce::Colour::fromRGB(255, 122, 122));
            return;
        }

        applyPreviewData(data);
        dirty = false;
        setStatusText("Preview generated", juce::Colour::fromRGB(112, 214, 156));
    }

    bool ExportPreviewPanel::autoRefreshEnabled() const noexcept
    {
        return autoRefresh;
    }

    void ExportPreviewPanel::setAutoRefreshEnabled(bool enabled)
    {
        autoRefresh = enabled;
        autoRefreshToggle.setToggleState(autoRefresh, juce::dontSendNotification);
        if (autoRefresh && dirty)
            refreshPreview();
    }

    void ExportPreviewPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(40, 46, 56));
        g.drawRect(getLocalBounds(), 1);
    }

    void ExportPreviewPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);

        auto row0 = area.removeFromTop(20);
        titleLabel.setBounds(row0.removeFromLeft(130));
        statusLabel.setBounds(row0);

        area.removeFromTop(4);
        auto row1 = area.removeFromTop(24);
        classNameLabel.setBounds(row1.removeFromLeft(40));
        classNameEditor.setBounds(row1.removeFromLeft(170));
        row1.removeFromLeft(6);
        autoRefreshToggle.setBounds(row1.removeFromLeft(54));

        area.removeFromTop(4);
        auto row2 = area.removeFromTop(24);
        refreshButton.setBounds(row2.removeFromLeft(136));
        row2.removeFromLeft(6);
        exportButton.setBounds(row2.removeFromLeft(96));

        area.removeFromTop(6);
        tabs.setBounds(area);
    }

    void ExportPreviewPanel::applyPreviewData(const PreviewData& data)
    {
        reportEditor.setText(data.reportText, juce::dontSendNotification);
        headerEditor.setText(data.headerText, juce::dontSendNotification);
        sourceEditor.setText(data.sourceText, juce::dontSendNotification);
        manifestEditor.setText(data.manifestText, juce::dontSendNotification);

        if (data.outputPath.isNotEmpty())
            reportEditor.setText(data.reportText + "\n\n[Output]\n" + data.outputPath, juce::dontSendNotification);
    }

    void ExportPreviewPanel::clearPreviewEditors()
    {
        reportEditor.clear();
        headerEditor.clear();
        sourceEditor.clear();
        manifestEditor.clear();
    }

    void ExportPreviewPanel::setStatusText(const juce::String& text, juce::Colour colour)
    {
        statusLabel.setText(text, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, colour);
        statusLabel.setJustificationType(juce::Justification::centredRight);
    }

    juce::String ExportPreviewPanel::normalizedClassName() const
    {
        const auto legal = juce::File::createLegalFileName(classNameEditor.getText().trim());
        if (legal.isEmpty())
            return "GyeolExportedComponent";
        return legal;
    }
}
