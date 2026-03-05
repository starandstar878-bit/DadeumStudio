#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Gyeol::Ui::Panels {
class EventActionPanel : public juce::Component, private juce::Timer, private juce::KeyListener {
public:
  EventActionPanel(DocumentHandle &documentIn,
                   const Widgets::WidgetRegistry &registryIn);
  ~EventActionPanel() override;

  void refreshFromDocument();
  void setBindingsChangedCallback(std::function<void()> callback);

  void paint(juce::Graphics &g) override;
  void paintOverChildren(juce::Graphics &g) override;
  void resized() override;
  void lookAndFeelChanged() override;
  bool keyPressed(const juce::KeyPress &key) override;
  bool keyPressed(const juce::KeyPress &key,
                  juce::Component *originatingComponent) override;

private:
  enum class PanelMode { eventAction, stateBinding };

  struct WidgetOption {
    WidgetId id = kRootId;
    juce::String label;
    std::vector<Widgets::RuntimeEventSpec> events;
  };

  class BindingListModel final : public juce::ListBoxModel {
  public:
    explicit BindingListModel(EventActionPanel &ownerIn) : owner(ownerIn) {}
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width,
                          int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent &event) override;

  private:
    EventActionPanel &owner;
  };

  class ActionListModel final : public juce::ListBoxModel {
  public:
    explicit ActionListModel(EventActionPanel &ownerIn) : owner(ownerIn) {}
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width,
                          int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;

  private:
    EventActionPanel &owner;
  };

  class RuntimeParamListModel final : public juce::ListBoxModel {
  public:
    explicit RuntimeParamListModel(EventActionPanel &ownerIn)
        : owner(ownerIn) {}
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width,
                          int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;

  private:
    EventActionPanel &owner;
  };

  class PropertyBindingListModel final : public juce::ListBoxModel {
  public:
    explicit PropertyBindingListModel(EventActionPanel &ownerIn)
        : owner(ownerIn) {}
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width,
                          int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;

  private:
    EventActionPanel &owner;
  };

  int selectedBindingModelIndex() const;
  RuntimeBindingModel *selectedBinding();
  const RuntimeBindingModel *selectedBinding() const;
  RuntimeActionModel *selectedAction();
  const RuntimeActionModel *selectedAction() const;
  int selectedRuntimeParamIndex() const;
  RuntimeParamModel *selectedRuntimeParam();
  const RuntimeParamModel *selectedRuntimeParam() const;
  int selectedPropertyBindingIndex() const;
  PropertyBindingModel *selectedPropertyBinding();
  const PropertyBindingModel *selectedPropertyBinding() const;

  void rebuildWidgetOptions();
  void rebuildRuntimeParamKeyOptions();
  void rebuildActionTargetOptions();
  void rebuildPropertyBindingTargetOptions();
  void rebuildPropertyBindingTargetPropertyOptions(
      WidgetId targetWidgetId, const juce::String &selectedProperty = {});
  std::optional<WidgetId>
  selectedWidgetIdFromCombo(const juce::ComboBox &combo) const;
  void selectWidgetIdInCombo(juce::ComboBox &combo, WidgetId id);
  juce::String selectedPropertyBindingTargetPropertyKey() const;
  void rebuildCreateCombos(bool forceSelectionMatch = false);
  void scheduleSearchFilterRefresh();
  void applySearchFilterNow();
  void rebuildVisibleBindings();
  bool syncSelectionFilter();
  bool bindingMatchesSelectionFilter(const RuntimeBindingModel &binding) const;
  juce::String validateRuntimeBindingForUi(const RuntimeBindingModel &binding) const;
  bool hasImplicitPinFocus() const;
  bool isTextEditorPendingCommit(const juce::TextEditor *editor) const;
  bool isComboBoxPendingCommit(const juce::ComboBox *combo) const;
  void cancelPendingEdits();
  void restoreSelections();
  void refreshDetailEditors();
  void refreshStateEditors();
  void updatePanelModeVisibility();
  void setPanelMode(PanelMode mode);
  void updateActionEditorVisibility(const RuntimeActionModel *action,
                                    bool hasAction);
  void rebuildAssetPatchEditors(const RuntimeActionModel *action);
  void syncAssetPatchValueEditor();
  void applyAssetPatchValue();

  void createBindingFromToolbar();
  void duplicateSelectedBinding();
  void showCloneBindingMenu();
  void cloneSelectedBindingToWidget(WidgetId targetSourceWidgetId);
  void toggleBindingEnabledAtVisibleRow(int row);
  void deleteSelectedBinding();

  void addAction();
  void deleteAction();
  void moveActionUp();
  void moveActionDown();
  void addRuntimeParam();
  void deleteRuntimeParam();
  void applySelectedRuntimeParam();
  void addPropertyBinding();
  void deletePropertyBinding();
  void applySelectedPropertyBinding();

  void applyBindingMeta();
  void applyActionKind();
  void applySelectedAction();
  void rebuildActionTargetPropertyOptions(
      WidgetId targetId, const juce::String &selectedPropertyKey = {});
  juce::String selectedActionTargetPropertyKey() const;
  void rebuildDynamicPropEditor();
  juce::var getDynamicPropValue() const;

  bool commitBindings(const juce::String &reason);
  bool commitRuntimeParams(const juce::String &reason);
  bool commitPropertyBindings(const juce::String &reason);
  void updateValidationUi();
  void updatePropertyBindingExpressionPreview();
  void setStatus(const juce::String &text, juce::Colour colour);
  void timerCallback() override;

  RuntimeBindingModel makeDefaultBinding(WidgetId sourceWidgetId,
                                         const juce::String &eventKey) const;
  RuntimeActionModel makeDefaultAction(RuntimeActionKind kind,
                                       WidgetId sourceWidgetId) const;
  WidgetId nextBindingId() const;
  WidgetId nextPropertyBindingId() const;

  std::optional<WidgetOption> findWidgetOption(WidgetId id) const;
  juce::String
  formatEventLabel(const Widgets::RuntimeEventSpec &eventSpec) const;
  juce::String formatEventLabel(WidgetId sourceWidgetId,
                                const juce::String &eventKey) const;
  juce::String actionSummary(const RuntimeActionModel &action) const;
  juce::String
  validatePropertyBindingForUi(const PropertyBindingModel &binding) const;

  static juce::String actionKindLabel(RuntimeActionKind kind);
  static std::optional<RuntimeActionKind>
  actionKindFromLabel(const juce::String &label);
  static juce::String nodeKindLabel(NodeKind kind);
  static std::optional<NodeKind> nodeKindFromLabel(const juce::String &label);
  static juce::var parseRuntimeValue(const juce::String &text);
  static juce::String runtimeValueToString(const juce::var &value);
  static std::optional<double> parseNumber(const juce::String &text);
  static juce::Result parsePatchJson(const juce::String &text,
                                     PropertyBag &outPatch);

  DocumentHandle &document;
  const Widgets::WidgetRegistry &registry;
  BindingListModel bindingListModel;
  ActionListModel actionListModel;
  RuntimeParamListModel runtimeParamListModel;
  PropertyBindingListModel propertyBindingListModel;

  std::function<void()> onBindingsChanged;

  PanelMode panelMode = PanelMode::eventAction;
  std::vector<WidgetOption> widgetOptions;
  uint64_t widgetOptionsHash = 0;
  uint64_t bindingsFingerprint = 0;
  uint64_t runtimeParamsFingerprint = 0;
  uint64_t propertyBindingsFingerprint = 0;
  bool documentSnapshotFingerprintsReady = false;
  std::vector<RuntimeBindingModel> bindings;
  std::vector<RuntimeParamModel> runtimeParams;
  std::vector<PropertyBindingModel> propertyBindings;
  std::vector<int> visibleBindingIndices;
  WidgetId selectedBindingId = kRootId;
  int selectedActionRow = -1;
  int selectedRuntimeParamRow = -1;
  int selectedPropertyBindingRow = -1;
  bool suppressCallbacks = false;
  WidgetId selectionFilterWidgetId = kRootId;
  std::vector<WidgetId> lastEditorSelection;

  juce::Label titleLabel;
  juce::TextButton eventModeButton{"Events"};
  juce::TextButton stateModeButton{"State"};
  juce::ComboBox sourceCombo;
  juce::ComboBox eventCombo;
  juce::TextButton addBindingButton{"+ Binding"};
  juce::TextEditor searchEditor;
  juce::ToggleButton showAllWidgetsToggle{"All Widgets"};

  juce::ListBox bindingList;
  juce::Label bindingSectionLabel;

  juce::Label detailTitleLabel;
  juce::TextEditor bindingNameEditor;
  juce::ToggleButton bindingEnabledToggle{"Enabled"};
  juce::TextButton duplicateBindingButton{"Duplicate"};
  juce::TextButton cloneBindingButton{"Clone To..."};
  juce::TextButton deleteBindingButton{"Delete"};

  juce::ListBox actionList;
  juce::Label actionSectionLabel;
  juce::TextButton addActionButton{"+ Action"};
  juce::TextButton deleteActionButton{"Delete Action"};
  juce::TextButton actionUpButton{"Move Up"};
  juce::TextButton actionDownButton{"Move Down"};

  juce::ComboBox actionKindCombo;
  juce::ComboBox paramKeyCombo;
  juce::TextEditor valueEditor;
  juce::TextEditor deltaEditor;
  juce::ComboBox targetKindCombo;
  juce::ComboBox targetIdCombo;
  // ?�???�성 ?�택
  juce::Label targetPropertyLabel;
  juce::ComboBox targetPropertyCombo;
  std::vector<juce::String> targetPropertyKeys;
  // visible/locked/opacity ?�용 콘트�?
  juce::ComboBox visibleCombo;
  juce::ComboBox lockedCombo;
  juce::TextEditor opacityEditor;
  // ?�적 ?�집�?(?�성 kind???�라 PropertyEditorFactory가 ?�성)
  juce::Label dynamicPropLabel;
  std::unique_ptr<juce::Component> dynamicPropEditor;
  std::optional<Widgets::WidgetPropertySpec> currentDynamicPropSpec;
  juce::ComboBox assetPatchKeyCombo;
  juce::ComboBox assetPatchValueCombo;
  juce::TextEditor patchEditor;
  juce::TextEditor boundsXEditor;
  juce::TextEditor boundsYEditor;
  juce::TextEditor boundsWEditor;
  juce::TextEditor boundsHEditor;

  std::vector<juce::Identifier> assetPatchKeys;
  std::vector<juce::String> assetPatchValues;

  juce::Label stateHintLabel;
  juce::Label runtimeParamTitleLabel;
  juce::ListBox runtimeParamList;
  juce::TextButton addRuntimeParamButton{"+ Param"};
  juce::TextButton deleteRuntimeParamButton{"Delete Param"};
  juce::TextEditor runtimeParamKeyEditor;
  juce::ComboBox runtimeParamTypeCombo;
  juce::TextEditor runtimeParamDefaultEditor;
  juce::TextEditor runtimeParamDescriptionEditor;
  juce::ToggleButton runtimeParamExposedToggle{"Exposed"};

  juce::Label propertyBindingTitleLabel;
  juce::ListBox propertyBindingList;
  juce::TextButton addPropertyBindingButton{"+ Link"};
  juce::TextButton deletePropertyBindingButton{"Delete Link"};
  juce::TextEditor propertyBindingNameEditor;
  juce::ToggleButton propertyBindingEnabledToggle{"Enabled"};
  juce::ComboBox propertyBindingTargetIdCombo;
  juce::ComboBox propertyBindingTargetPropertyCombo;
  juce::TextEditor propertyBindingExpressionEditor;
  juce::Label propertyBindingExpressionPreviewLabel;
  std::vector<juce::String> propertyBindingTargetPropertyKeys;

  juce::Label paramKeyWarningLabel;
  juce::Label deltaWarningLabel;
  juce::Label boundsWarningLabel;
  juce::Label propertyBindingWarningLabel;

  juce::Label statusLabel;
};
} // namespace Gyeol::Ui::Panels


