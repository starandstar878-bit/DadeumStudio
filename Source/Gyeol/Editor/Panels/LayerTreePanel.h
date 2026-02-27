#pragma once

#include "Gyeol/Editor/Interaction/LayerOrderEngine.h"
#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <functional>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class LayerTreePanel : public juce::Component
    {
    public:
        explicit LayerTreePanel(DocumentHandle& documentIn);
        ~LayerTreePanel() override;

        void refreshFromDocument();
        void setSelectionChangedCallback(std::function<void(std::vector<WidgetId>)> callback);
        void setDropRequestCallback(std::function<juce::Result(const Interaction::LayerTreeDropRequest&)> callback);

        void paint(juce::Graphics& g) override;
        void paintOverChildren(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        enum class ModelNodeKind
        {
            widget,
            group
        };

        struct ModelNode
        {
            ModelNodeKind kind = ModelNodeKind::widget;
            WidgetId id = kRootId;
            WidgetId parentId = kRootId;
            juce::String label;
            juce::String filterKeyLower;
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

        void rebuildModel();
        void rebuildTree();
        std::vector<ModelNode> buildNodesForParent(WidgetId parentId) const;
        std::vector<ModelNode> filterNodes(const std::vector<ModelNode>& source, const juce::String& filterLower) const;
        bool modelNodeMatchesFilter(const ModelNode& node, const juce::String& filterLower) const;
        std::vector<WidgetId> collectGroupSelectionIds(WidgetId groupId) const;
        juce::String keyForNode(ModelNodeKind kind, WidgetId id) const;
        std::optional<ModelNode> findNodeByKey(const juce::String& key) const;
        std::optional<juce::String> resolveSelectedNodeKey(const std::vector<WidgetId>& selection) const;
        void applyDocumentSelectionToTree();

        void beginDrag(ModelNodeKind kind, WidgetId id, juce::Point<int> treePoint);
        void updateDrag(juce::Point<int> treePoint);
        void endDrag();
        void handleTreeSelection(const juce::String& key);
        std::optional<DropPreview> computeDropPreview(juce::Point<int> treePoint) const;
        std::optional<Interaction::LayerTreeDropRequest> buildDropRequest(const DropPreview& preview) const;
        std::optional<std::pair<WidgetId, int>> resolveDropParentAndInsert(const DropPreview& preview) const;
        bool isDropTargetInDraggedSubtree(const juce::String& targetNodeKey) const;

        std::vector<TreeItem*> visibleTreeItems() const;
        static void collectVisibleTreeItems(juce::TreeViewItem* item, std::vector<TreeItem*>& out);
        static bool sameWidgetIdSet(const std::vector<WidgetId>& lhs, const std::vector<WidgetId>& rhs);

        DocumentHandle& document;
        juce::TextEditor searchBox;
        juce::TreeView treeView;
        std::unique_ptr<RootItem> rootItem;
        std::function<void(std::vector<WidgetId>)> onSelectionChanged;
        std::function<juce::Result(const Interaction::LayerTreeDropRequest&)> onDropRequest;

        std::vector<ModelNode> rootNodes;
        std::unordered_set<WidgetId> expandedGroupIds;
        std::unordered_map<juce::String, std::vector<WidgetId>> selectionByKey;

        bool suppressTreeSelectionCallback = false;
        bool dragCandidate = false;
        bool dragActive = false;
        ModelNodeKind dragNodeKind = ModelNodeKind::widget;
        std::vector<WidgetId> draggedNodeIds;
        juce::Point<int> dragStartPoint;
        std::optional<DropPreview> dropPreview;
    };
}
