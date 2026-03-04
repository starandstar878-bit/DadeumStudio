import re
import codecs

path = 'Source/Gyeol/Editor/Panels/ExportPreviewPanel.cpp'
with codecs.open(path, 'r', 'utf-8-sig') as f:
    content = f.read()

# Replace setupReadOnlyEditor
old_setup = """        void setupReadOnlyEditor(juce::TextEditor& editor)
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
        }"""
new_setup = """        void setupReadOnlyEditor(juce::CodeEditorComponent& editor)
        {
            editor.setReadOnly(true);
            editor.setScrollbarThickness(12);
            editor.setLineNumbersShown(true);
            
            // Adjust to larger font for readability
            editor.setFont(juce::FontOptions("Consolas", 13.5f, juce::Font::plain));
            
            // TextEditor has backgroundColourId but CodeEditorComponent has different names
            editor.setColour(juce::CodeEditorComponent::backgroundColourId, palette(GyeolPalette::ControlBase));
            editor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, palette(GyeolPalette::CanvasBackground, 0.95f));
            editor.setColour(juce::CodeEditorComponent::lineNumberTextId, palette(GyeolPalette::TextSecondary, 0.88f));
            editor.setColour(juce::CodeEditorComponent::defaultTextColourId, palette(GyeolPalette::TextPrimary));
            editor.setColour(juce::CodeEditorComponent::highlightColourId, palette(GyeolPalette::SelectionBackground, 0.85f));
        }"""
content = content.replace(old_setup.replace('\n', '\r\n'), new_setup.replace('\n', '\r\n'))
content = content.replace(old_setup, new_setup)

# Replace applyPreviewData
old_apply = """    void ExportPreviewPanel::applyPreviewData(const PreviewData& data)
    {
        reportEditor.setText(data.reportText, juce::dontSendNotification);
        headerEditor.setText(data.headerText, juce::dontSendNotification);
        sourceEditor.setText(data.sourceText, juce::dontSendNotification);
        manifestEditor.setText(data.manifestText, juce::dontSendNotification);

        if (data.outputPath.isNotEmpty())
            reportEditor.setText(data.reportText + "\\n\\n[Output]\\n" + data.outputPath, juce::dontSendNotification);

        updateFooterStatus();
    }"""
new_apply = """    void ExportPreviewPanel::applyPreviewData(const PreviewData& data)
    {
        reportDoc.replaceAllContent(data.reportText);
        headerDoc.replaceAllContent(data.headerText);
        sourceDoc.replaceAllContent(data.sourceText);
        manifestDoc.replaceAllContent(data.manifestText);

        if (data.outputPath.isNotEmpty())
            reportDoc.replaceAllContent(data.reportText + "\\n\\n[Output]\\n" + data.outputPath);

        updateFooterStatus();
    }"""
content = content.replace(old_apply.replace('\n', '\r\n'), new_apply.replace('\n', '\r\n'))
content = content.replace(old_apply, new_apply)

# Replace clearPreviewEditors
old_clear = """    void ExportPreviewPanel::clearPreviewEditors()
    {
        reportEditor.clear();
        headerEditor.clear();
        sourceEditor.clear();
        manifestEditor.clear();
        lastGeneratedTime = "--:--:--";
        updateFooterStatus();
    }"""
new_clear = """    void ExportPreviewPanel::clearPreviewEditors()
    {
        reportDoc.replaceAllContent("");
        headerDoc.replaceAllContent("");
        sourceDoc.replaceAllContent("");
        manifestDoc.replaceAllContent("");
        lastGeneratedTime = "--:--:--";
        updateFooterStatus();
    }"""
content = content.replace(old_clear.replace('\n', '\r\n'), new_clear.replace('\n', '\r\n'))
content = content.replace(old_clear, new_clear)

# Replace updateFooterStatus, lineCountForText, hasPreviewData, activeEditor, drawCodeGutter
old_footer_and_others = """    void ExportPreviewPanel::updateFooterStatus()
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
            if (text[i] == '\\n')
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
    }"""

new_footer_and_others = """    void ExportPreviewPanel::updateFooterStatus()
    {
        int lines = 0;
        switch (tabs.getCurrentTabIndex())
        {
            case 0: lines = reportDoc.getNumLines(); break;
            case 1: lines = headerDoc.getNumLines(); break;
            case 2: lines = sourceDoc.getNumLines(); break;
            case 3: lines = manifestDoc.getNumLines(); break;
        }
        footerLabel.setText("Generated at " + lastGeneratedTime + "  |  Lines: " + juce::String(lines),
                            juce::dontSendNotification);
    }

    bool ExportPreviewPanel::hasPreviewData() const
    {
        return reportDoc.getNumCharacters() > 0
            || headerDoc.getNumCharacters() > 0
            || sourceDoc.getNumCharacters() > 0
            || manifestDoc.getNumCharacters() > 0;
    }

    void ExportPreviewPanel::syncActiveTabState()
    {
        const auto activeIndex = tabs.getCurrentTabIndex();
        if (activeIndex == cachedActiveTabIndex)
            return;

        cachedActiveTabIndex = activeIndex;
        updateFooterStatus();
    }"""
content = content.replace(old_footer_and_others.replace('\n', '\r\n'), new_footer_and_others.replace('\n', '\r\n'))
content = content.replace(old_footer_and_others, new_footer_and_others)

# Remove drawCodeGutter from paintOverChildren
old_paint_over = """    void ExportPreviewPanel::paintOverChildren(juce::Graphics& g)
    {
        syncActiveTabState();

        drawCodeGutter(g, reportEditor);
        drawCodeGutter(g, headerEditor);
        drawCodeGutter(g, sourceEditor);
        drawCodeGutter(g, manifestEditor);

        if (hasPreviewData())
            return;"""
new_paint_over = """    void ExportPreviewPanel::paintOverChildren(juce::Graphics& g)
    {
        syncActiveTabState();

        if (hasPreviewData())
            return;"""
content = content.replace(old_paint_over.replace('\n', '\r\n'), new_paint_over.replace('\n', '\r\n'))
content = content.replace(old_paint_over, new_paint_over)

with codecs.open(path, 'w', 'utf-8-sig') as f:
    f.write(content)
