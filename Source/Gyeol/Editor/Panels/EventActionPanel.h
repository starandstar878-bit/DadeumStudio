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

        int selectedBindingModelIndex() const;
        RuntimeBindingModel* selectedBinding();
        const RuntimeBindingModel* selectedBinding() const;
        RuntimeActionModel* selectedAction();
        const RuntimeActionModel* selectedAction() const;

        void rebuildWidgetOptions();
        void rebuildCreateCombos();
        void rebuildVisibleBindings();
        void restoreSelections();
        void refreshDetailEditors();
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

        void applyBindingMeta();
        void applyActionKind();
        void applySelectedAction();

        bool commitBindings(const juce::String& reason);
        void setStatus(const juce::String& text, juce::Colour colour);

        RuntimeBindingModel makeDefaultBinding(WidgetId sourceWidgetId, const juce::String& eventKey) const;
        RuntimeActionModel makeDefaultAction(RuntimeActionKind kind, WidgetId sourceWidgetId) const;
        WidgetId nextBindingId() const;

        std::optional<WidgetOption> findWidgetOption(WidgetId id) const;
        juce::String formatEventLabel(const Widgets::RuntimeEventSpec& eventSpec) const;
        juce::String formatEventLabel(WidgetId sourceWidgetId, const juce::String& eventKey) const;
        juce::String actionSummary(const RuntimeActionModel& action) const;

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

        std::function<void()> onBindingsChanged;

        std::vector<WidgetOption> widgetOptions;
        std::vector<RuntimeBindingModel> bindings;
        std::vector<int> visibleBindingIndices;
        WidgetId selectedBindingId = kRootId;
        int selectedActionRow = -1;
        bool suppressCallbacks = false;

        juce::Label titleLabel;
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

        juce::Label statusLabel;
    };
}
