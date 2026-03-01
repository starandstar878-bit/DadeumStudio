#pragma once

#include "Gyeol/Editor/Interaction/LayerOrderEngine.h"
#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class LayerTreePanel : public juce::Component,
                           public juce::DragAndDropContainer,
                           public juce::DragAndDropTarget
    {
    public:
        LayerTreePanel(DocumentHandle& documentIn, const Widgets::WidgetFactory& widgetFactoryIn);
        ~LayerTreePanel() override;

        void refreshFromDocument();
        std::optional<NodeRef> selectedNode() const;
        void setSelectionChangedCallback(std::function<void(std::vector<WidgetId>)> callback);
        void setActiveLayerChangedCallback(std::function<void(std::optional<WidgetId>)> callback);
        void setDropRequestCallback(std::function<juce::Result(const Interaction::LayerTreeDropRequest&)> callback);
        void setNodePropsChangedCallback(std::function<juce::Result(const SetPropsAction&)> callback);
        void setCreateLayerRequestedCallback(std::function<std::optional<WidgetId>()> callback);
        void setDeleteLayerRequestedCallback(std::function<juce::Result(WidgetId)> callback);

        void paint(juce::Graphics& g) override;
        void paintOverChildren(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;

        bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDragMove(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDragExit(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
        void itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;

    private:
        enum class RefreshReason
        {
            external,
            initial,
            searchChanged
        };

        enum class ModelNodeKind
        {
            layer,
            group,
            widget
        };

        enum class RowIcon
        {
            none,
            visible,
            locked
        };

        struct ModelNode
        {
            ModelNodeKind kind = ModelNodeKind::widget;
            WidgetId id = kRootId;
            ParentRef parent;
            juce::String label;
            juce::String filterKeyLower;
            bool visible = true;
            bool locked = false;
            std::vector<WidgetId> selectionIds;
            std::vector<ModelNode> children;
        };

        struct DropPreview
        {
            juce::String targetNodeKey;
            Interaction::LayerDropPlacement placement = Interaction::LayerDropPlacement::before;
            juce::Rectangle<int> markerBounds;
        };

        class TreeItem;
        class RootItem;

        static int countModelNodes(const std::vector<ModelNode>& nodes);
        static int countTreeItems(const juce::TreeViewItem* item);
        static const char* refreshReasonToString(RefreshReason reason) noexcept;
        static bool sameNodeStructure(const std::vector<ModelNode>& lhs, const std::vector<ModelNode>& rhs);
        static bool sameNodeVisuals(const std::vector<ModelNode>& lhs, const std::vector<ModelNode>& rhs);
        static const char* dragKindToken(ModelNodeKind kind) noexcept;
        static std::optional<ModelNodeKind> modelKindFromDragToken(const juce::String& token);
        juce::var buildDragDescription(ModelNodeKind kind, const std::vector<WidgetId>& ids) const;
        bool parseDragDescription(const juce::var& description,
                                  ModelNodeKind& outKind,
                                  std::vector<WidgetId>& outIds) const;
        void collectVisualChangedKeys(const std::vector<ModelNode>& previousNodes,
                                      const std::vector<ModelNode>& nextNodes,
                                      std::vector<juce::String>& outChangedKeys) const;

        void rebuildModel();
        void rebuildTree();
        bool applyModelToExistingTree();
        TreeItem* findVisibleTreeItemByKey(const juce::String& key) const;
        TreeItem* findTreeItemByKey(const juce::String& key) const;
        void repaintTreeRowForKey(const juce::String& key);
        void repaintRowsForKeys(const std::vector<juce::String>& keys);
        void repaintDropPreviewDiff(const std::optional<DropPreview>& previousPreview,
                                    const std::optional<DropPreview>& nextPreview);
        void handleDragMoveAt(juce::Point<int> treePoint);
        void updateAutoExpand(const std::optional<DropPreview>& preview);
        void resetAutoExpandState();
        void maybeAutoScroll(juce::Point<int> treePoint);
        std::vector<ModelNode> buildLayerNodes() const;
        std::vector<ModelNode> buildNodesForParent(const ParentRef& parent) const;
        std::vector<ModelNode> filterNodes(const std::vector<ModelNode>& source, const juce::String& filterLower) const;
        bool modelNodeMatchesFilter(const ModelNode& node, const juce::String& filterLower) const;
        std::vector<WidgetId> collectGroupSelectionIds(WidgetId groupId) const;
        juce::String keyForNode(ModelNodeKind kind, WidgetId id) const;
        std::optional<ModelNode> findNodeByKey(const juce::String& key, const std::vector<ModelNode>& nodes) const;
        std::optional<ModelNode> findNodeByKey(const juce::String& key) const;
        std::optional<juce::String> resolveSelectedNodeKey(const std::vector<WidgetId>& selection) const;
        void applyDocumentSelectionToTree();

        void beginDrag(ModelNodeKind kind, WidgetId id, juce::Point<int> treePoint);
        void updateDrag(juce::Point<int> treePoint);
        void endDrag();
        void handleTreeSelection(const juce::String& key);
        std::optional<DropPreview> computeDropPreview(juce::Point<int> treePoint) const;
        std::optional<Interaction::LayerTreeDropRequest> buildDropRequest(const DropPreview& preview) const;
        std::optional<std::pair<ParentRef, int>> resolveDropParentAndInsert(const DropPreview& preview) const;
        bool isDropTargetInDraggedSubtree(const juce::String& targetNodeKey) const;
        juce::Rectangle<int> rowBoundsForItem(const TreeItem& item) const;

        std::vector<TreeItem*> visibleTreeItems() const;
        static void collectVisibleTreeItems(juce::TreeViewItem* item, std::vector<TreeItem*>& out);
        static bool sameWidgetIdSet(const std::vector<WidgetId>& lhs, const std::vector<WidgetId>& rhs);

        RowIcon iconHitForTreeItem(const TreeItem& item, juce::Point<int> localPoint) const;
        void toggleNodeIcon(const ModelNode& node, RowIcon icon);
        void handleCreateLayerButton();
        void handleDeleteLayerButton();
        static NodeKind nodeKindFromModelKind(ModelNodeKind kind);

        DocumentHandle& document;
        const Widgets::WidgetFactory& widgetFactory;
        juce::TextButton createLayerButton { "+ Layer" };
        juce::TextButton deleteLayerButton { "- Layer" };
        juce::TextEditor searchBox;
        juce::TreeView treeView;
        std::unique_ptr<RootItem> rootItem;
        std::function<void(std::vector<WidgetId>)> onSelectionChanged;
        std::function<void(std::optional<WidgetId>)> onActiveLayerChanged;
        std::function<juce::Result(const Interaction::LayerTreeDropRequest&)> onDropRequest;
        std::function<juce::Result(const SetPropsAction&)> onNodePropsChanged;
        std::function<std::optional<WidgetId>()> onCreateLayerRequested;
        std::function<juce::Result(WidgetId)> onDeleteLayerRequested;

        std::vector<ModelNode> fullRootNodes;
        std::vector<ModelNode> rootNodes;
        std::unordered_set<WidgetId> expandedLayerIds;
        std::unordered_set<WidgetId> expandedGroupIds;
        std::unordered_set<WidgetId> knownLayerIds;
        std::unordered_set<WidgetId> knownGroupIds;
        std::unordered_map<juce::String, std::vector<WidgetId>> selectionByKey;
        std::optional<WidgetId> explicitActiveLayerId;
        std::optional<juce::String> lastAppliedSelectionKey;

        bool suppressTreeSelectionCallback = false;
        bool dragCandidate = false;
        bool dragActive = false;
        ModelNodeKind dragNodeKind = ModelNodeKind::widget;
        std::vector<WidgetId> draggedNodeIds;
        juce::Point<int> dragStartPoint;
        std::optional<DropPreview> dropPreview;
        juce::String dragHoverNodeKey;
        uint32_t dragHoverStartMs = 0;

        struct PerfStats
        {
            uint64_t refreshCount = 0;
            uint64_t dragPreviewUpdateCount = 0;
            uint64_t dragPreviewUpdateCountSinceRefresh = 0;
            int lastModelNodeCount = 0;
            int lastTreeItemCount = 0;
            int lastVisibleItemCount = 0;
            int lastSelectionEntryCount = 0;
            double lastRefreshMs = 0.0;
            double lastRebuildModelMs = 0.0;
            double lastRebuildTreeMs = 0.0;
            double maxRefreshMs = 0.0;
        };

        static constexpr double slowRefreshLogThresholdMs = 8.0;
        static constexpr uint64_t periodicPerfLogInterval = 120;
        static constexpr uint32_t autoExpandDelayMs = 320;
        static constexpr int autoScrollEdgePx = 18;
        static constexpr int autoScrollStepPx = 14;

        RefreshReason pendingRefreshReason = RefreshReason::external;
        PerfStats perf;
    };
}

