#pragma once

#include "Gyeol/Public/Action.h"
#include "Gyeol/Public/Types.h"
#include <memory>

namespace Gyeol
{
    struct WidgetBoundsUpdate
    {
        WidgetId id = kRootId;
        juce::Rectangle<float> bounds;
    };

    class DocumentHandle
    {
    public:
        DocumentHandle();
        ~DocumentHandle();
        DocumentHandle(DocumentHandle&&) noexcept;
        DocumentHandle& operator=(DocumentHandle&&) noexcept;

        DocumentHandle(const DocumentHandle&) = delete;
        DocumentHandle& operator=(const DocumentHandle&) = delete;

        const DocumentModel& snapshot() const noexcept;
        const EditorStateModel& editorState() const noexcept;

        WidgetId createNode(const CreateAction& action);
        bool deleteNodes(const DeleteAction& action);
        bool setProps(const SetPropsAction& action);
        bool setBounds(const SetBoundsAction& action);
        bool reparent(ReparentAction action);
        bool reorder(ReorderAction action);
        bool setAssets(std::vector<AssetModel> assets);
        bool replaceAssetRefKey(const juce::String& oldRefKey, const juce::String& newRefKey);
        bool setRuntimeParams(std::vector<RuntimeParamModel> params);
        bool setPropertyBindings(std::vector<PropertyBindingModel> bindings);
        bool setRuntimeBindings(std::vector<RuntimeBindingModel> bindings);

        bool beginCoalescedEdit(const juce::String& key);
        bool previewSetProps(const SetPropsAction& action);
        bool previewSetBounds(const SetBoundsAction& action);
        bool endCoalescedEdit(const juce::String& key, bool commit);

        // Compatibility wrappers (kept for one release).
        WidgetId addWidget(WidgetType type,
                           juce::Rectangle<float> bounds,
                           const PropertyBag& properties = {},
                           std::optional<WidgetId> layerId = std::nullopt);
        bool removeWidget(const WidgetId& id);
        bool moveWidget(const WidgetId& id, juce::Point<float> delta);
        bool setWidgetBounds(const WidgetId& id, juce::Rectangle<float> bounds);
        bool setWidgetsBounds(const std::vector<WidgetBoundsUpdate>& updates);
        bool groupSelection(std::optional<WidgetId> layerId = std::nullopt);
        bool ungroupSelection();

        void selectSingle(const WidgetId& id);
        void setSelection(std::vector<WidgetId> selection);
        void clearSelection();

        bool canUndo() const noexcept;
        bool canRedo() const noexcept;
        int undoDepth() const noexcept;
        int redoDepth() const noexcept;
        uint64_t historySerial() const noexcept;
        bool undo();
        bool redo();

        juce::Result saveToFile(const juce::File& file) const;
        juce::Result loadFromFile(const juce::File& file);

    private:
        class Impl;
        std::unique_ptr<Impl> impl;
    };
}
