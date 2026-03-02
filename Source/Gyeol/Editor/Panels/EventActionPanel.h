#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class EventActionPanel : public juce::Component
    {
    public:
        EventActionPanel(DocumentHandle& documentIn, const Widgets::WidgetRegistry& registryIn);
        ~EventActionPanel() override;

        void refreshFromDocument();
        void setBindingsChangedCallback(std::function<void()> callback);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        enum class PanelMode
        {
            eventAction,
            stateBinding
        };

        struct WidgetOption
        {
            WidgetId id = kRootId;
            juce::String label;
            std::vector<Widgets::RuntimeEventSpec> events;
        };

        class BindingListModel final : public juce::ListBoxModel
        {
        public:
            explicit BindingListModel(EventActionPanel& ownerIn) : owner(ownerIn) {}
            int getNumRows() override;
            void paintListBoxItem(int rowNumber,
                                  juce::Graphics& g,
                                  int width,
                                  int height,
                                  bool rowIsSelected) override;
            void selectedRowsChanged(int lastRowSelected) override;
            void listBoxItemClicked(int row, const juce::MouseEvent& event) override;

        private:
            EventActionPanel& owner;
        };

        class ActionListModel final : public juce::ListBoxModel
        {
        public:
            explicit ActionListModel(EventActionPanel& ownerIn) : owner(ownerIn) {}
            int getNumRows() override;
            void paintListBoxItem(int rowNumber,
                                  juce::Graphics& g,
                                  int width,
                                  int height,
                                  bool rowIsSelected) override;
            void selectedRowsChanged(int lastRowSelected) override;

        private:
            EventActionPanel& owner;
        };

        class RuntimeParamListModel final : public juce::ListBoxModel
        {
        public:
            explicit RuntimeParamListModel(EventActionPanel& ownerIn) : owner(ownerIn) {}
            int getNumRows() override;
            void paintListBoxItem(int rowNumber,
                                  juce::Graphics& g,
                                  int width,
                                  int height,
                                  bool rowIsSelected) override;
            void selectedRowsChanged(int lastRowSelected) override;

        private:
            EventActionPanel& owner;
        };

        class PropertyBindingListModel final : public juce::ListBoxModel
        {
        public:
            explicit PropertyBindingListModel(EventActionPanel& ownerIn) : owner(ownerIn) {}
            int getNumRows() override;
            void paintListBoxItem(int rowNumber,
                                  juce::Graphics& g,
                                  int width,
                                  int height,
                                  bool rowIsSelected) override;
            void selectedRowsChanged(int lastRowSelected) override;

        private:
            EventActionPanel& owner;
        };

        int selectedBindingModelIndex() const;
        RuntimeBindingModel* selectedBinding();
        const RuntimeBindingModel* selectedBinding() const;
        RuntimeActionModel* selectedAction();
        const RuntimeActionModel* selectedAction() const;
        int selectedRuntimeParamIndex() const;
        RuntimeParamModel* selectedRuntimeParam();
        const RuntimeParamModel* selectedRuntimeParam() const;
        int selectedPropertyBindingIndex() const;
        PropertyBindingModel* selectedPropertyBinding();
        const PropertyBindingModel* selectedPropertyBinding() const;

        void rebuildWidgetOptions();
        void rebuildCreateCombos();
        void rebuildVisibleBindings();
        void restoreSelections();
        void refreshDetailEditors();
        void refreshStateEditors();
        void updatePanelModeVisibility();
        void setPanelMode(PanelMode mode);
        void updateActionEditorVisibility(const RuntimeActionModel* action, bool hasAction);
        void rebuildAssetPatchEditors(const RuntimeActionModel* action);
        void syncAssetPatchValueEditor();
        void applyAssetPatchValue();

        void createBindingFromToolbar();
        void duplicateSelectedBinding();
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

        bool commitBindings(const juce::String& reason);
        bool commitRuntimeParams(const juce::String& reason);
        bool commitPropertyBindings(const juce::String& reason);
        void setStatus(const juce::String& text, juce::Colour colour);

        RuntimeBindingModel makeDefaultBinding(WidgetId sourceWidgetId, const juce::String& eventKey) const;
        RuntimeActionModel makeDefaultAction(RuntimeActionKind kind, WidgetId sourceWidgetId) const;
        WidgetId nextBindingId() const;
        WidgetId nextPropertyBindingId() const;

        std::optional<WidgetOption> findWidgetOption(WidgetId id) const;
        juce::String formatEventLabel(const Widgets::RuntimeEventSpec& eventSpec) const;
        juce::String formatEventLabel(WidgetId sourceWidgetId, const juce::String& eventKey) const;
        juce::String actionSummary(const RuntimeActionModel& action) const;
        juce::String validatePropertyBindingForUi(const PropertyBindingModel& binding) const;

        static juce::String actionKindLabel(RuntimeActionKind kind);
        static std::optional<RuntimeActionKind> actionKindFromLabel(const juce::String& label);
        static juce::String nodeKindLabel(NodeKind kind);
        static std::optional<NodeKind> nodeKindFromLabel(const juce::String& label);
        static juce::var parseRuntimeValue(const juce::String& text);
        static juce::String runtimeValueToString(const juce::var& value);
        static std::optional<double> parseNumber(const juce::String& text);
        static juce::Result parsePatchJson(const juce::String& text, PropertyBag& outPatch);

        DocumentHandle& document;
        const Widgets::WidgetRegistry& registry;
        BindingListModel bindingListModel;
        ActionListModel actionListModel;
        RuntimeParamListModel runtimeParamListModel;
        PropertyBindingListModel propertyBindingListModel;

        std::function<void()> onBindingsChanged;

        PanelMode panelMode = PanelMode::eventAction;
        std::vector<WidgetOption> widgetOptions;
        std::vector<RuntimeBindingModel> bindings;
        std::vector<RuntimeParamModel> runtimeParams;
        std::vector<PropertyBindingModel> propertyBindings;
        std::vector<int> visibleBindingIndices;
        WidgetId selectedBindingId = kRootId;
        int selectedActionRow = -1;
        int selectedRuntimeParamRow = -1;
        int selectedPropertyBindingRow = -1;
        bool suppressCallbacks = false;

        juce::Label titleLabel;
        juce::TextButton eventModeButton { "Events" };
        juce::TextButton stateModeButton { "State" };
        juce::ComboBox sourceCombo;
        juce::ComboBox eventCombo;
        juce::TextButton addBindingButton { "+ Binding" };
        juce::TextEditor searchEditor;

        juce::ListBox bindingList;

        juce::Label detailTitleLabel;
        juce::TextEditor bindingNameEditor;
        juce::ToggleButton bindingEnabledToggle { "Enabled" };
        juce::TextButton duplicateBindingButton { "Duplicate" };
        juce::TextButton deleteBindingButton { "Delete" };

        juce::ListBox actionList;
        juce::TextButton addActionButton { "+ Action" };
        juce::TextButton deleteActionButton { "Delete Action" };
        juce::TextButton actionUpButton { "Move Up" };
        juce::TextButton actionDownButton { "Move Down" };

        juce::ComboBox actionKindCombo;
        juce::TextEditor paramKeyEditor;
        juce::TextEditor valueEditor;
        juce::TextEditor deltaEditor;
        juce::ComboBox targetKindCombo;
        juce::TextEditor targetIdEditor;
        juce::ComboBox visibleCombo;
        juce::ComboBox lockedCombo;
        juce::TextEditor opacityEditor;
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
        juce::TextButton addRuntimeParamButton { "+ Param" };
        juce::TextButton deleteRuntimeParamButton { "Delete Param" };
        juce::TextEditor runtimeParamKeyEditor;
        juce::ComboBox runtimeParamTypeCombo;
        juce::TextEditor runtimeParamDefaultEditor;
        juce::TextEditor runtimeParamDescriptionEditor;
        juce::ToggleButton runtimeParamExposedToggle { "Exposed" };

        juce::Label propertyBindingTitleLabel;
        juce::ListBox propertyBindingList;
        juce::TextButton addPropertyBindingButton { "+ Link" };
        juce::TextButton deletePropertyBindingButton { "Delete Link" };
        juce::TextEditor propertyBindingNameEditor;
        juce::ToggleButton propertyBindingEnabledToggle { "Enabled" };
        juce::TextEditor propertyBindingTargetIdEditor;
        juce::TextEditor propertyBindingTargetPropertyEditor;
        juce::TextEditor propertyBindingExpressionEditor;

        juce::Label statusLabel;
    };
}
