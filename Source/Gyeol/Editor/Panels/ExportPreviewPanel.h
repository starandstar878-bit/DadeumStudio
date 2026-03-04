#pragma once

#include <JuceHeader.h>
#include <functional>

namespace Gyeol::Ui::Panels {
class ExportPreviewPanel : public juce::Component {
public:
  struct PreviewData {
    juce::String reportText;
    juce::String headerText;
    juce::String sourceText;
    juce::String manifestText;
    juce::String outputPath;
  };

  using GeneratePreviewCallback = std::function<juce::Result(
      const juce::String &componentClassName, PreviewData &outData)>;
  using ExportRequestedCallback =
      std::function<void(const juce::String &componentClassName)>;

  ExportPreviewPanel();
  ~ExportPreviewPanel() override;

  void setGeneratePreviewCallback(GeneratePreviewCallback callback);
  void setExportRequestedCallback(ExportRequestedCallback callback);

  void markDirty();
  void refreshPreview();
  bool autoRefreshEnabled() const noexcept;
  void setAutoRefreshEnabled(bool enabled);

  void paint(juce::Graphics &g) override;
  void paintOverChildren(juce::Graphics &g) override;
  void resized() override;

private:
  void applyPreviewData(const PreviewData &data);
  void clearPreviewEditors();
  void setStatusText(const juce::String &text, juce::Colour colour);
  juce::String normalizedClassName() const;
  void updateFooterStatus();
  bool hasPreviewData() const;
  void syncActiveTabState();

  bool dirty = true;
  bool autoRefresh = false;
  juce::String lastGeneratedTime = "--:--:--";

  GeneratePreviewCallback onGeneratePreview;
  ExportRequestedCallback onExportRequested;

  juce::Label titleLabel;
  juce::Label statusLabel;
  juce::TextEditor classNameEditor;
  juce::TextButton refreshButton{"Generate"};
  juce::TextButton exportButton{"Export"};

  juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
  juce::CodeDocument reportDoc, headerDoc, sourceDoc, manifestDoc;
  juce::CodeEditorComponent reportEditor{reportDoc, nullptr};
  juce::CodeEditorComponent headerEditor{headerDoc, nullptr};
  juce::CodeEditorComponent sourceEditor{sourceDoc, nullptr};
  juce::CodeEditorComponent manifestEditor{manifestDoc, nullptr};
  juce::Label footerLabel;

  juce::Rectangle<int> tabBarBoundsInPanel;
  juce::Rectangle<int> footerBounds;
  int cachedActiveTabIndex = 0;
};
} // namespace Gyeol::Ui::Panels