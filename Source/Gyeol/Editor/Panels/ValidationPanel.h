#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Gyeol::Ui::Panels {
class ValidationPanel : public juce::Component, private juce::ListBoxModel {
public:
  enum class IssueSeverity { info, warning, error };

  struct Issue {
    IssueSeverity severity = IssueSeverity::info;
    juce::String title;
    juce::String message;
    WidgetId relatedWidgetId = kRootId; // 연관된 위젯 ID (없으면 kRootId)
  };

  ValidationPanel(DocumentHandle &documentIn,
                  const Widgets::WidgetRegistry &registryIn);
  ~ValidationPanel() override;

  void markDirty();
  void refreshValidation();
  bool autoRefreshEnabled() const noexcept;
  void setAutoRefreshEnabled(bool enabled);

  /** 이슈 항목 클릭 시 연관 위젯을 에디터에서 선택하도록 하는 콜백 */
  void setSelectWidgetCallback(std::function<void(WidgetId)> callback);

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override;
  void selectedRowsChanged(int lastRowSelected) override;

  void rebuildIssues();
  void rebuildFilteredIssues();
  void pushIssue(IssueSeverity severity, const juce::String &title,
                 const juce::String &message,
                 WidgetId relatedWidgetId = kRootId);
  static juce::Colour colorForSeverity(IssueSeverity severity);
  static juce::String labelForSeverity(IssueSeverity severity);

  DocumentHandle &document;
  const Widgets::WidgetRegistry &registry;

  std::vector<Issue> allIssues;  // 전체 이슈 (필터 전)
  std::vector<int> filteredRows; // 현재 필터에 보이는 인덱스
  bool dirty = true;
  bool autoRefresh = true;

  // 필터 상태
  bool showErrors = true;
  bool showWarnings = true;
  bool showInfo = true;

  std::function<void(WidgetId)> onSelectWidget;

  juce::Label titleLabel;
  juce::Label summaryLabel;
  juce::ToggleButton autoRefreshToggle{"Auto"};
  juce::TextButton runButton{"Run Validation"};

  // 카테고리 필터 버튼
  juce::TextButton filterErrorBtn{"E"};
  juce::TextButton filterWarningBtn{"W"};
  juce::TextButton filterInfoBtn{"I"};

  juce::ListBox listBox;
};
} // namespace Gyeol::Ui::Panels
