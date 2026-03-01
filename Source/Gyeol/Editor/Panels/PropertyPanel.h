#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include "Gyeol/Widgets/WidgetSDK.h"
#include <functional>
#include <JuceHeader.h>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class PropertyPanel : public juce::Component
    {
    public:
        enum class InspectorTargetKind
        {
            none,
            layer,
            group,
            widgetSingle,
            widgetMulti
        };

        struct InspectorTarget
        {
            InspectorTargetKind kind = InspectorTargetKind::none;
            WidgetId nodeId = kRootId;
            std::vector<WidgetId> widgetIds;
        };

        using SetPropsPreviewCallback = std::function<juce::Result(const SetPropsAction&)>;
        using SetBoundsPreviewCallback = std::function<juce::Result(const SetBoundsAction&)>;
        using GroupTransformPreviewCallback = std::function<juce::Result(WidgetId, const juce::Rectangle<float>&)>;

        struct CommitCallbacks
        {
            SetPropsPreviewCallback onSetPropsPreview;
            SetBoundsPreviewCallback onSetBoundsPreview;
            GroupTransformPreviewCallback onGroupTransformPreview;
            std::function<void()> onPreviewApplied;
            std::function<void()> onCommitted;
        };

        PropertyPanel(DocumentHandle& documentIn, const Widgets::WidgetFactory& widgetFactoryIn);
        ~PropertyPanel() override;

        void setInspectorTarget(InspectorTarget target);
        void setCommitCallbacks(CommitCallbacks callbacks);
        void refreshFromDocument();

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        struct LayoutEntry
        {
            juce::Component* left = nullptr;
            juce::Component* right = nullptr;
            int height = 24;
            bool fullWidth = false;
        };

        struct ValueState
        {
            juce::var value;
            bool mixed = false;
            bool valid = false;
        };

        struct WidgetRef
        {
            WidgetId id = kRootId;
            WidgetType type = WidgetType::button;
            juce::Rectangle<float> bounds;
            PropertyBag properties;
            bool visible = true;
            bool locked = false;
            float opacity = 1.0f;
        };

        void resetContent();
        void rebuildContent();
        void layoutContent();

        void addSectionHeader(const juce::String& text);
        void addInfoRow(const juce::String& label, const juce::String& value);
        void addExpanderRow(const juce::String& label, bool expanded, std::function<void()> onToggle);
        void addEditorRow(const Widgets::WidgetPropertySpec& spec,
                          const ValueState& valueState,
                          std::function<void(const juce::var&)> onPreview,
                          std::function<void(const juce::var&)> onCommit,
                          std::function<void()> onCancel);

        ValueState makeBooleanState(const std::vector<bool>& values) const;
        ValueState makeFloatState(const std::vector<float>& values) const;
        ValueState makeStringState(const std::vector<juce::String>& values) const;
        ValueState makeWidgetPropertyState(const std::vector<WidgetRef>& widgets, const juce::Identifier& key) const;

        std::optional<WidgetRef> findWidgetRef(WidgetId id) const;
        const GroupModel* findGroup(WidgetId id) const;
        const LayerModel* findLayer(WidgetId id) const;

        std::vector<WidgetRef> resolveTargetWidgets() const;
        std::vector<WidgetId> collectGroupWidgetIdsRecursive(WidgetId groupId) const;
        std::optional<juce::Rectangle<float>> computeUnionBounds(const std::vector<WidgetId>& widgetIds) const;
        std::optional<juce::Rectangle<float>> resolveCurrentTransformBounds() const;

        bool varEquals(const juce::var& lhs, const juce::var& rhs) const;
        bool beginEditSession(const juce::String& editKey);
        void commitEditSession(const juce::String& editKey);
        void cancelEditSession(const juce::String& editKey);

        bool applySetPropsPreview(const SetPropsAction& action, const juce::String& editKey);
        bool applySetBoundsPreview(const SetBoundsAction& action, const juce::String& editKey);
        bool applyTransformPreview(const juce::Rectangle<float>& targetBounds, const juce::String& editKey);

        juce::Rectangle<float> clampWidgetBounds(const WidgetRef& widget, juce::Rectangle<float> bounds) const;
        bool buildScaledBoundsUpdates(const std::vector<WidgetRef>& widgets,
                                      const juce::Rectangle<float>& sourceUnion,
                                      const juce::Rectangle<float>& targetUnion,
                                      std::vector<SetBoundsAction::Item>& updates) const;

        void buildNoneContent();
        void buildLayerContent(const LayerModel& layer);
        void buildGroupContent(const GroupModel& group);
        void buildWidgetContent(const std::vector<WidgetRef>& widgets, bool multiSelection);

        void buildCommonWidgetProperties(const std::vector<WidgetRef>& widgets);
        std::vector<Widgets::WidgetPropertySpec> commonPropertySpecs(const std::vector<WidgetRef>& widgets) const;

        DocumentHandle& document;
        const Widgets::WidgetFactory& widgetFactory;
        InspectorTarget inspectorTarget;
        CommitCallbacks commitCallbacks;

        juce::Label titleLabel;
        juce::Label subtitleLabel;
        juce::ToggleButton showAdvancedToggle { "Show Advanced" };
        juce::Viewport viewport;
        juce::Component content;

        std::vector<std::unique_ptr<juce::Label>> ownedLabels;
        std::vector<std::unique_ptr<juce::Component>> ownedEditors;
        std::vector<LayoutEntry> layoutEntries;

        bool showAdvancedProperties = false;
        juce::String activeEditKey;
        static constexpr float canvasWidth = 1600.0f;
        static constexpr float canvasHeight = 1000.0f;
    };
}
