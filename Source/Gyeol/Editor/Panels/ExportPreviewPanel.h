#pragma once

#include <JuceHeader.h>
#include <functional>

namespace Gyeol::Ui::Panels
{
    class ExportPreviewPanel : public juce::Component
    {
    public:
        struct PreviewData
        {
            juce::String reportText;
            juce::String headerText;
            juce::String sourceText;
            juce::String manifestText;
            juce::String outputPath;
        };

        using GeneratePreviewCallback = std::function<juce::Result(const juce::String& componentClassName,
                                                                   PreviewData& outData)>;
        using ExportRequestedCallback = std::function<void(const juce::String& componentClassName)>;

        ExportPreviewPanel();
        ~ExportPreviewPanel() override;

        void setGeneratePreviewCallback(GeneratePreviewCallback callback);
        void setExportRequestedCallback(ExportRequestedCallback callback);

        void markDirty();
        void refreshPreview();
        bool autoRefreshEnabled() const noexcept;
        void setAutoRefreshEnabled(bool enabled);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        void applyPreviewData(const PreviewData& data);
        void clearPreviewEditors();
        void setStatusText(const juce::String& text, juce::Colour colour);
        juce::String normalizedClassName() const;

        bool dirty = true;
        bool autoRefresh = false;

        GeneratePreviewCallback onGeneratePreview;
        ExportRequestedCallback onExportRequested;

        juce::Label titleLabel;
        juce::Label statusLabel;
        juce::Label classNameLabel;
        juce::TextEditor classNameEditor;
        juce::ToggleButton autoRefreshToggle { "Auto" };
        juce::TextButton refreshButton { "Generate Preview" };
        juce::TextButton exportButton { "Export JUCE" };

        juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
        juce::TextEditor reportEditor;
        juce::TextEditor headerEditor;
        juce::TextEditor sourceEditor;
        juce::TextEditor manifestEditor;
    };
}
