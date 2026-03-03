#include "Gyeol/Editor/Panels/ExportPreviewPanel.h"

#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

#include <cmath>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        using Gyeol::GyeolPalette;

        juce::Colour palette(GyeolPalette id, float alpha = 1.0f)
        {
            return Gyeol::getGyeolColor(id).withAlpha(alpha);
        }

        void setupReadOnlyEditor(juce::TextEditor& editor)
        {
            editor.setMultiLine(true);
            editor.setReadOnly(true);
            editor.setScrollbarsShown(true);
            editor.setCaretVisible(false);
            editor.setPopupMenuEnabled(true);
            editor.setFont(juce::FontOptions("Consolas", 11.5f, juce::Font::plain));
            editor.setBorder(juce::BorderSize<int>(0, 36, 0, 0));
            editor.setColour(juce::TextEditor::backgroundColourId, palette(GyeolPalette::ControlBase));
            editor.setColour(juce::TextEditor::outlineColourId, palette(GyeolPalette::BorderDefault));
            editor.setColour(juce::TextEditor::focusedOutlineColourId, palette(GyeolPalette::BorderDefault));
            editor.setColour(juce::TextEditor::textColourId, palette(GyeolPalette::TextPrimary));
            editor.setColour(juce::TextEditor::highlightColourId, palette(GyeolPalette::SelectionBackground, 0.85f));
        }
    }

    ExportPreviewPanel::ExportPreviewPanel()
    {
        titleLabel.setText("Export Preview", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextPrimary));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        setStatusText("Stale", palette(GyeolPalette::TextSecondary));
        addAndMakeVisible(statusLabel);

        classNameEditor.setText("GyeolExportedComponent", juce::dontSendNotification);
        classNameEditor.setTextToShowWhenEmpty("ComponentClassName", palette(GyeolPalette::TextSecondary));
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

        refreshButton.setButtonText("Generate");
        refreshButton.onClick = [this]
        {
            refreshPreview();
        };
        addAndMakeVisible(refreshButton);

        exportButton.setButtonText("Export");
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

        tabs.setTabBarDepth(30);
        tabs.setOutline(0);
        tabs.setColour(juce::TabbedButtonBar::tabOutlineColourId, palette(GyeolPalette::BorderDefault));
        tabs.setColour(juce::TabbedButtonBar::frontOutlineColourId, palette(GyeolPalette::BorderDefault));
        tabs.setColour(juce::TabbedButtonBar::frontTextColourId, palette(GyeolPalette::TextPrimary));
        tabs.setColour(juce::TabbedButtonBar::tabTextColourId, palette(GyeolPalette::TextSecondary));
        tabs.setColour(juce::TabbedComponent::backgroundColourId, palette(GyeolPalette::PanelBackground));
        tabs.setColour(juce::TabbedComponent::outlineColourId, palette(GyeolPalette::BorderDefault));

        tabs.addTab("Report", palette(GyeolPalette::PanelBackground), &reportEditor, false);
        tabs.addTab("Header", palette(GyeolPalette::PanelBackground), &headerEditor, false);
        tabs.addTab("Source", palette(GyeolPalette::PanelBackground), &sourceEditor, false);
        tabs.addTab("Manifest", palette(GyeolPalette::PanelBackground), &manifestEditor, false);
        tabs.setCurrentTabIndex(0, juce::dontSendNotification);
        cachedActiveTabIndex = tabs.getCurrentTabIndex();
        addAndMakeVisible(tabs);

        footerLabel.setJustificationType(juce::Justification::centredLeft);
        footerLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextSecondary));
        addAndMakeVisible(footerLabel);

        updateFooterStatus();
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
            setStatusText("Stale (Generate)", palette(GyeolPalette::TextSecondary));

        updateFooterStatus();
    }

    void ExportPreviewPanel::refreshPreview()
    {
        if (onGeneratePreview == nullptr)
        {
            clearPreviewEditors();
            setStatusText("Preview callback is not connected", palette(GyeolPalette::ValidWarning));
            return;
        }

        PreviewData data;
        const auto result = onGeneratePreview(normalizedClassName(), data);
        if (result.failed())
        {
            clearPreviewEditors();
            setStatusText("Preview failed: " + result.getErrorMessage(), palette(GyeolPalette::ValidError));
            return;
        }

        applyPreviewData(data);
        dirty = false;
        lastGeneratedTime = juce::Time::getCurrentTime().formatted("%H:%M:%S");
        setStatusText("Preview generated", palette(GyeolPalette::ValidSuccess));
        updateFooterStatus();
    }

    bool ExportPreviewPanel::autoRefreshEnabled() const noexcept
    {
        return autoRefresh;
    }

    void ExportPreviewPanel::setAutoRefreshEnabled(bool enabled)
    {
        autoRefresh = enabled;
        if (autoRefresh && dirty)
            refreshPreview();
    }

    void ExportPreviewPanel::paint(juce::Graphics& g)
    {
        syncActiveTabState();

        g.fillAll(palette(GyeolPalette::PanelBackground));
        g.setColour(palette(GyeolPalette::BorderDefault));
        g.drawRect(getLocalBounds(), 1);

        if (!tabBarBoundsInPanel.isEmpty())
        {
            auto tabBar = tabBarBoundsInPanel.toFloat().reduced(0.0f, 1.0f);
            g.setColour(palette(GyeolPalette::HeaderBackground, 0.86f));
            g.fillRoundedRectangle(tabBar, 6.0f);
            g.setColour(palette(GyeolPalette::BorderDefault, 0.88f));
            g.drawRoundedRectangle(tabBar, 6.0f, 1.0f);

            const auto tabCount = juce::jmax(1, tabs.getNumTabs());
            const auto tabWidth = tabBarBoundsInPanel.getWidth() / tabCount;
            const auto active = juce::jlimit(0, tabCount - 1, tabs.getCurrentTabIndex());
            const auto indicatorWidth = juce::jmax(24, tabWidth - 20);
            const auto indicatorX = tabBarBoundsInPanel.getX() + active * tabWidth + (tabWidth - indicatorWidth) / 2;

            auto indicator = juce::Rectangle<float>(static_cast<float>(indicatorX),
                                                    static_cast<float>(tabBarBoundsInPanel.getBottom() - 4),
                                                    static_cast<float>(indicatorWidth),
                                                    3.0f);
            g.setColour(palette(GyeolPalette::AccentPrimary));
            g.fillRoundedRectangle(indicator, 2.0f);
        }

        if (!footerBounds.isEmpty())
        {
            auto footer = footerBounds.toFloat().reduced(0.0f, 1.0f);
            g.setColour(palette(GyeolPalette::HeaderBackground, 0.72f));
            g.fillRoundedRectangle(footer, 4.0f);
            g.setColour(palette(GyeolPalette::BorderDefault, 0.86f));
            g.drawRoundedRectangle(footer, 4.0f, 1.0f);
        }
    }

    void ExportPreviewPanel::paintOverChildren(juce::Graphics& g)
    {
        syncActiveTabState();

        drawCodeGutter(g, reportEditor);
        drawCodeGutter(g, headerEditor);
        drawCodeGutter(g, sourceEditor);
        drawCodeGutter(g, manifestEditor);

        if (hasPreviewData())
            return;

        auto emptyArea = tabs.getBounds().reduced(10);
        emptyArea.removeFromTop(tabs.getTabBarDepth());
        if (emptyArea.getHeight() < 48)
            return;

        g.setColour(palette(GyeolPalette::TextSecondary, 0.94f));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("< />", emptyArea.removeFromTop(20), juce::Justification::centred, true);

        g.setColour(palette(GyeolPalette::TextSecondary, 0.90f));
        g.setFont(juce::FontOptions(10.2f));
        g.drawText("Click Generate to preview export",
                   emptyArea.removeFromTop(18),
                   juce::Justification::centred,
                   true);
    }

    void ExportPreviewPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);

        auto row0 = area.removeFromTop(20);
        titleLabel.setBounds(row0.removeFromLeft(130));
        statusLabel.setBounds(row0);

        area.removeFromTop(4);
        auto controls = area.removeFromTop(24);
        constexpr int buttonWidth = 72;
        exportButton.setBounds(controls.removeFromRight(buttonWidth));
        controls.removeFromRight(6);
        refreshButton.setBounds(controls.removeFromRight(buttonWidth));
        controls.removeFromRight(8);
        classNameEditor.setBounds(controls);

        area.removeFromTop(6);
        footerBounds = area.removeFromBottom(18);
        footerLabel.setBounds(footerBounds.reduced(6, 0));

        tabs.setBounds(area);
        tabBarBoundsInPanel = tabs.getBounds();
        tabBarBoundsInPanel.setHeight(tabs.getTabBarDepth());

        updateFooterStatus();
    }

    void ExportPreviewPanel::applyPreviewData(const PreviewData& data)
    {
        reportEditor.setText(data.reportText, juce::dontSendNotification);
        headerEditor.setText(data.headerText, juce::dontSendNotification);
        sourceEditor.setText(data.sourceText, juce::dontSendNotification);
        manifestEditor.setText(data.manifestText, juce::dontSendNotification);

        if (data.outputPath.isNotEmpty())
            reportEditor.setText(data.reportText + "\n\n[Output]\n" + data.outputPath, juce::dontSendNotification);

        updateFooterStatus();
    }

    void ExportPreviewPanel::clearPreviewEditors()
    {
        reportEditor.clear();
        headerEditor.clear();
        sourceEditor.clear();
        manifestEditor.clear();
        lastGeneratedTime = "--:--:--";
        updateFooterStatus();
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

    void ExportPreviewPanel::updateFooterStatus()
    {
        const auto* editor = activeEditor();
        const auto lines = editor != nullptr ? lineCountForText(editor->getText()) : 0;
        footerLabel.setText("Generated at " + lastGeneratedTime + "  |  Lines: " + juce::String(lines),
                            juce::dontSendNotification);
    }

    int ExportPreviewPanel::lineCountForText(const juce::String& text) const
    {
        if (text.isEmpty())
            return 0;

        int lineCount = 1;
        for (int i = 0; i < text.length(); ++i)
        {
            if (text[i] == '\n')
                ++lineCount;
        }
        return lineCount;
    }

    bool ExportPreviewPanel::hasPreviewData() const
    {
        return reportEditor.getText().isNotEmpty()
            || headerEditor.getText().isNotEmpty()
            || sourceEditor.getText().isNotEmpty()
            || manifestEditor.getText().isNotEmpty();
    }

    juce::TextEditor* ExportPreviewPanel::activeEditor() noexcept
    {
        switch (tabs.getCurrentTabIndex())
        {
            case 0: return &reportEditor;
            case 1: return &headerEditor;
            case 2: return &sourceEditor;
            case 3: return &manifestEditor;
            default: return &reportEditor;
        }
    }

    const juce::TextEditor* ExportPreviewPanel::activeEditor() const noexcept
    {
        return const_cast<ExportPreviewPanel*>(this)->activeEditor();
    }

    void ExportPreviewPanel::syncActiveTabState()
    {
        const auto activeIndex = tabs.getCurrentTabIndex();
        if (activeIndex == cachedActiveTabIndex)
            return;

        cachedActiveTabIndex = activeIndex;
        updateFooterStatus();
    }

    void ExportPreviewPanel::drawCodeGutter(juce::Graphics& g, const juce::TextEditor& editor) const
    {
        if (!editor.isVisible())
            return;

        auto bounds = editor.getBounds();
        constexpr int gutterWidth = 34;
        auto gutter = bounds.removeFromLeft(gutterWidth).reduced(1, 1);
        if (gutter.isEmpty())
            return;

        g.setColour(palette(GyeolPalette::CanvasBackground, 0.95f));
        g.fillRoundedRectangle(gutter.toFloat(), 3.0f);
        g.setColour(palette(GyeolPalette::BorderDefault, 0.82f));
        g.drawLine(static_cast<float>(gutter.getRight()) + 0.5f,
                   static_cast<float>(gutter.getY()),
                   static_cast<float>(gutter.getRight()) + 0.5f,
                   static_cast<float>(gutter.getBottom()),
                   1.0f);

        const auto lineCount = lineCountForText(editor.getText());
        if (lineCount <= 0)
            return;

        const auto lineHeight = editor.getFont().getHeight() + 1.5f;
        const auto maxVisible = juce::jmax(1, static_cast<int>((gutter.getHeight() - 4) / lineHeight));
        const auto drawCount = juce::jmin(lineCount, maxVisible);

        g.setColour(palette(GyeolPalette::TextSecondary, 0.88f));
        g.setFont(juce::FontOptions(9.0f));

        for (int i = 0; i < drawCount; ++i)
        {
            const auto y = gutter.getY() + 2 + static_cast<int>(std::floor(i * lineHeight));
            g.drawText(juce::String(i + 1),
                       juce::Rectangle<int>(gutter.getX() + 2, y, gutter.getWidth() - 5, static_cast<int>(lineHeight) + 1),
                       juce::Justification::centredRight,
                       true);
        }
    }
}