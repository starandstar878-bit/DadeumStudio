#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Gyeol::Ui::Panels {
class ValidationPanel : public juce::Component,
                        private juce::ListBoxModel,
                        private juce::MultiTimer {
public:
  enum class IssueSeverity { info, warning, error };

  struct DependencyGraph {
    std::vector<juce::String> nodes;
    std::vector<std::pair<int, int>> edges;
  };

  struct Issue {
    juce::String uuid;
    IssueSeverity severity = IssueSeverity::info;
    juce::String title;
    juce::String message;
    WidgetId relatedWidgetId = kRootId;

    bool quickFixAvailable = false;
    WidgetId relatedAssetId = kRootId;
    AssetKind relatedAssetKind = AssetKind::file;
    juce::String missingAssetPath;

    DependencyGraph dependency;
  };

  struct ExternalIssue {
    IssueSeverity severity = IssueSeverity::warning;
    juce::String category;
    juce::String title;
    juce::String message;
    WidgetId relatedWidgetId = kRootId;
  };

  ValidationPanel(DocumentHandle &documentIn,
                  const Widgets::WidgetRegistry &registryIn);
  ~ValidationPanel() override;

  void markDirty();
  void refreshValidation();
  bool autoRefreshEnabled() const noexcept;
  void setAutoRefreshEnabled(bool enabled);
  void setExternalIssues(std::vector<ExternalIssue> issues);

  void setSelectWidgetCallback(std::function<void(WidgetId)> callback);
  void setHoverWidgetCallback(std::function<void(WidgetId)> callback);

  void paint(juce::Graphics &g) override;
  void resized() override;
  void lookAndFeelChanged() override;

private:
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void listBoxItemClicked(int row, const juce::MouseEvent &event) override;
  void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override;
  void selectedRowsChanged(int lastRowSelected) override;

  void timerCallback(int timerId) override;

  void mouseMove(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;

  void queueValidation(bool immediate);
  void launchValidationJob();
  void applyValidationResults(int requestSerial, std::vector<Issue> issues);

  void rebuildFilteredIssues();
  bool isIssueVisible(const Issue &issue) const;
  void updateFilterChipLabels();
  void updateSummaryText();
  void emitHoverForRow(int row);
  void toggleIssueExpanded(const juce::String &issueUuid);
  void toggleIssueMuted(const juce::String &issueUuid);
  bool performQuickFixForIssue(const Issue &issue);

  static std::vector<Issue>
  buildIssues(const DocumentModel &snapshot,
              const EditorStateModel &editorState,
              const Widgets::WidgetRegistry &registry,
              const juce::File &projectRoot,
              const std::vector<ExternalIssue> &externalIssues);

  static juce::Colour colorForSeverity(IssueSeverity severity);
  static juce::String labelForSeverity(IssueSeverity severity);

  static constexpr int debounceTimerId = 1;
  static constexpr int pulseTimerId = 2;

  DocumentHandle &document;
  const Widgets::WidgetRegistry &registry;

  std::vector<Issue> allIssues;
  std::vector<int> filteredRows;
  std::unordered_set<juce::String> mutedIssueUuids;
  std::unordered_set<juce::String> expandedIssueUuids;

  bool dirty = true;
  bool autoRefresh = true;
  bool showErrors = true;
  bool showWarnings = true;
  bool showInfo = true;

  std::vector<ExternalIssue> externalIssues;

  std::atomic<int> validationSerialCounter{1};
  int lastAppliedValidationSerial = 0;
  std::atomic<bool> validationJobRunning{false};
  std::atomic<bool> validationRerunQueued{false};

  int hoveredRow = -1;
  float pulsePhase = 0.0f;

  std::function<void(WidgetId)> onSelectWidget;
  std::function<void(WidgetId)> onHoverWidget;

  juce::Label titleLabel;
  juce::Label summaryLabel;
  juce::ToggleButton autoRefreshToggle{"Auto"};
  juce::TextButton runButton{"Run"};

  juce::TextButton filterErrorBtn{"Errors"};
  juce::TextButton filterWarningBtn{"Warnings"};
  juce::TextButton filterInfoBtn{"Infos"};

  juce::ListBox listBox;
  std::unique_ptr<juce::FileChooser> quickFixChooser;
};
} // namespace Gyeol::Ui::Panels
