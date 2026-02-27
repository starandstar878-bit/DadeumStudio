#include "Gyeol/Editor/Panels/LayerTreePanel.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace
{
    juce::String widgetTypeLabel(Gyeol::WidgetType type)
    {
        switch (type)
        {
            case Gyeol::WidgetType::button: return "Button";
            case Gyeol::WidgetType::slider: return "Slider";
            case Gyeol::WidgetType::knob:   return "Knob";
            case Gyeol::WidgetType::label:  return "Label";
            case Gyeol::WidgetType::meter:  return "Meter";
        }

        return "Widget";
    }

    const Gyeol::GroupModel* findGroupById(const Gyeol::DocumentModel& document, Gyeol::WidgetId groupId) noexcept
    {
        const auto it = std::find_if(document.groups.begin(),
                                     document.groups.end(),
                                     [groupId](const Gyeol::GroupModel& group)
                                     {
                                         return group.id == groupId;
                                     });
        return it == document.groups.end() ? nullptr : &(*it);
    }

    const Gyeol::WidgetModel* findWidgetById(const Gyeol::DocumentModel& document, Gyeol::WidgetId widgetId) noexcept
    {
        const auto it = std::find_if(document.widgets.begin(),
                                     document.widgets.end(),
                                     [widgetId](const Gyeol::WidgetModel& widget)
                                     {
                                         return widget.id == widgetId;
                                     });
        return it == document.widgets.end() ? nullptr : &(*it);
    }

    std::unordered_map<Gyeol::WidgetId, Gyeol::WidgetId> directOwnerByWidgetId(const Gyeol::DocumentModel& document)
    {
        std::unordered_map<Gyeol::WidgetId, Gyeol::WidgetId> ownerByWidgetId;
        ownerByWidgetId.reserve(document.widgets.size());
        for (const auto& group : document.groups)
        {
            for (const auto memberId : group.memberWidgetIds)
                ownerByWidgetId[memberId] = group.id;
        }
        return ownerByWidgetId;
    }

    std::unordered_map<Gyeol::WidgetId, int> widgetOrderById(const Gyeol::DocumentModel& document)
    {
        std::unordered_map<Gyeol::WidgetId, int> order;
        order.reserve(document.widgets.size());
        for (size_t i = 0; i < document.widgets.size(); ++i)
            order[document.widgets[i].id] = static_cast<int>(i);
        return order;
    }

    int groupAnchorOrder(const Gyeol::DocumentModel& document,
                         Gyeol::WidgetId groupId,
                         const std::unordered_map<Gyeol::WidgetId, int>& widgetOrder,
                         std::unordered_map<Gyeol::WidgetId, int>& memo,
                         std::unordered_set<Gyeol::WidgetId>& visiting)
    {
        if (const auto memoIt = memo.find(groupId); memoIt != memo.end())
            return memoIt->second;

        if (!visiting.insert(groupId).second)
            return std::numeric_limits<int>::max();

        int anchor = std::numeric_limits<int>::max();
        if (const auto* group = findGroupById(document, groupId); group != nullptr)
        {
            for (const auto memberId : group->memberWidgetIds)
            {
                if (const auto it = widgetOrder.find(memberId); it != widgetOrder.end())
                    anchor = std::min(anchor, it->second);
            }

            for (const auto& child : document.groups)
            {
                if (child.parentGroupId.value_or(Gyeol::kRootId) != groupId)
                    continue;

                const auto childAnchor = groupAnchorOrder(document, child.id, widgetOrder, memo, visiting);
                anchor = std::min(anchor, childAnchor);
            }
        }

        visiting.erase(groupId);
        memo[groupId] = anchor;
        return anchor;
    }

    void collectGroupWidgetsRecursive(const Gyeol::DocumentModel& document,
                                      Gyeol::WidgetId groupId,
                                      std::unordered_set<Gyeol::WidgetId>& outWidgets,
                                      std::unordered_set<Gyeol::WidgetId>& visitedGroups)
    {
        if (!visitedGroups.insert(groupId).second)
            return;

        const auto* group = findGroupById(document, groupId);
        if (group == nullptr)
            return;

        for (const auto memberId : group->memberWidgetIds)
            outWidgets.insert(memberId);

        for (const auto& child : document.groups)
        {
            if (child.parentGroupId.value_or(Gyeol::kRootId) == groupId)
                collectGroupWidgetsRecursive(document, child.id, outWidgets, visitedGroups);
        }
    }

    std::vector<Gyeol::WidgetId> collectGroupWidgetsRecursive(const Gyeol::DocumentModel& document, Gyeol::WidgetId groupId)
    {
        std::unordered_set<Gyeol::WidgetId> widgets;
        std::unordered_set<Gyeol::WidgetId> visited;
        collectGroupWidgetsRecursive(document, groupId, widgets, visited);

        std::vector<Gyeol::WidgetId> ordered;
        ordered.reserve(widgets.size());
        for (const auto& widget : document.widgets)
        {
            if (widgets.count(widget.id) > 0)
                ordered.push_back(widget.id);
        }

        return ordered;
    }
}

namespace Gyeol::Ui::Panels
{
    class LayerTreePanel::TreeItem : public juce::TreeViewItem
    {
    public:
        TreeItem(LayerTreePanel& ownerIn, const ModelNode& modelNodeIn)
            : owner(ownerIn), modelNode(modelNodeIn)
        {
            for (const auto& child : modelNode.children)
                addSubItem(new TreeItem(owner, child));
        }

        juce::String getUniqueName() const override
        {
            return owner.keyForNode(modelNode.kind, modelNode.id);
        }

        bool mightContainSubItems() override
        {
            return !modelNode.children.empty();
        }

        int getItemHeight() const override
        {
            return 22;
        }

        void paintItem(juce::Graphics& g, int width, int height) override
        {
            const auto isSelectedRow = isSelected();
            if (isSelectedRow)
            {
                g.setColour(juce::Colour::fromRGB(56, 98, 160));
                g.fillRoundedRectangle(juce::Rectangle<float>(1.0f, 1.0f, static_cast<float>(width - 2), static_cast<float>(height - 2)), 4.0f);
            }

            g.setColour(modelNode.kind == ModelNodeKind::group
                            ? juce::Colour::fromRGB(233, 210, 160)
                            : juce::Colour::fromRGB(206, 212, 222));
            g.setFont(juce::FontOptions(modelNode.kind == ModelNodeKind::group ? 12.0f : 11.5f,
                                        modelNode.kind == ModelNodeKind::group ? juce::Font::bold : juce::Font::plain));
            g.drawFittedText(modelNode.label, juce::Rectangle<int>(0, 0, width, height).reduced(6, 1), juce::Justification::centredLeft, 1);
        }

        void itemSelectionChanged(bool isNowSelected) override
        {
            if (isNowSelected)
                owner.handleTreeSelection(getUniqueName());
        }

        void itemOpennessChanged(bool isNowOpen) override
        {
            if (modelNode.kind != ModelNodeKind::group)
                return;

            if (isNowOpen)
                owner.expandedGroupIds.insert(modelNode.id);
            else
                owner.expandedGroupIds.erase(modelNode.id);
        }

        const ModelNode& node() const noexcept { return modelNode; }

    private:
        LayerTreePanel& owner;
        ModelNode modelNode;
    };

    class LayerTreePanel::RootItem : public juce::TreeViewItem
    {
    public:
        RootItem(LayerTreePanel& owner, const std::vector<ModelNode>& nodes)
        {
            for (const auto& node : nodes)
                addSubItem(new TreeItem(owner, node));
        }

        juce::String getUniqueName() const override { return "layer-root"; }
        bool mightContainSubItems() override { return true; }
        bool canBeSelected() const override { return false; }
    };

    LayerTreePanel::LayerTreePanel(DocumentHandle& documentIn)
        : document(documentIn)
    {
        addAndMakeVisible(searchBox);
        searchBox.setTextToShowWhenEmpty("Search layers...", juce::Colour::fromRGB(118, 126, 140));
        searchBox.onTextChange = [this]
        {
            refreshFromDocument();
        };

        addAndMakeVisible(treeView);
        treeView.setRootItemVisible(false);
        treeView.setMultiSelectEnabled(false);
        treeView.setDefaultOpenness(true);
        treeView.setColour(juce::TreeView::backgroundColourId, juce::Colour::fromRGB(24, 28, 34));
        treeView.setColour(juce::TreeView::linesColourId, juce::Colour::fromRGBA(255, 255, 255, 18));
        treeView.addMouseListener(this, true);

        refreshFromDocument();
    }

    LayerTreePanel::~LayerTreePanel()
    {
        treeView.removeMouseListener(this);
        treeView.setRootItem(nullptr);
    }

    void LayerTreePanel::refreshFromDocument()
    {
        rebuildModel();
        rebuildTree();
        repaint();
    }

    void LayerTreePanel::setSelectionChangedCallback(std::function<void(std::vector<WidgetId>)> callback)
    {
        onSelectionChanged = std::move(callback);
    }

    void LayerTreePanel::setDropRequestCallback(std::function<juce::Result(const Interaction::LayerTreeDropRequest&)> callback)
    {
        onDropRequest = std::move(callback);
    }

    void LayerTreePanel::resized()
    {
        auto bounds = getLocalBounds().reduced(6);
        searchBox.setBounds(bounds.removeFromTop(26));
        bounds.removeFromTop(6);
        treeView.setBounds(bounds);
    }

    void LayerTreePanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(38, 45, 56));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);
    }

    void LayerTreePanel::paintOverChildren(juce::Graphics& g)
    {
        if (!dragActive || !dropPreview.has_value())
            return;

        const auto marker = dropPreview->markerBounds.translated(treeView.getX(), treeView.getY());
        const auto accent = juce::Colour::fromRGB(78, 156, 255);
        if (dropPreview->placement == Interaction::LayerDropPlacement::into)
        {
            g.setColour(accent.withAlpha(0.2f));
            g.fillRoundedRectangle(marker.toFloat().reduced(1.0f), 4.0f);
            g.setColour(accent);
            g.drawRoundedRectangle(marker.toFloat().reduced(1.0f), 4.0f, 1.2f);
        }
        else
        {
            g.setColour(accent);
            g.fillRect(marker);
        }
    }

    void LayerTreePanel::mouseDown(const juce::MouseEvent& event)
    {
        if (!event.mods.isLeftButtonDown())
            return;

        if (!treeView.isParentOf(event.eventComponent) && event.eventComponent != &treeView)
            return;

        const auto treePoint = event.getEventRelativeTo(&treeView).position.toInt();
        auto items = visibleTreeItems();
        auto* hit = static_cast<TreeItem*>(nullptr);
        for (auto* item : items)
        {
            if (item->getItemPosition(true).contains(treePoint))
            {
                hit = item;
                break;
            }
        }

        if (hit == nullptr)
            return;

        beginDrag(hit->node().kind, hit->node().id, treePoint);
    }

    void LayerTreePanel::mouseDrag(const juce::MouseEvent& event)
    {
        if (!dragCandidate)
            return;

        const auto treePoint = event.getEventRelativeTo(&treeView).position.toInt();
        updateDrag(treePoint);
    }

    void LayerTreePanel::mouseUp(const juce::MouseEvent&)
    {
        endDrag();
    }

    void LayerTreePanel::rebuildModel()
    {
        rootNodes = buildNodesForParent(kRootId);

        const auto filterLower = searchBox.getText().trim().toLowerCase();
        if (filterLower.isNotEmpty())
            rootNodes = filterNodes(rootNodes, filterLower);

        selectionByKey.clear();
        std::function<void(const std::vector<ModelNode>&)> indexNodes = [&](const std::vector<ModelNode>& nodes)
        {
            for (const auto& node : nodes)
            {
                selectionByKey[keyForNode(node.kind, node.id)] = node.selectionIds;
                indexNodes(node.children);
            }
        };
        indexNodes(rootNodes);
    }

    void LayerTreePanel::rebuildTree()
    {
        suppressTreeSelectionCallback = true;

        rootItem = std::make_unique<RootItem>(*this, rootNodes);
        treeView.setRootItem(rootItem.get());

        std::function<void(juce::TreeViewItem*)> applyOpen = [&](juce::TreeViewItem* item)
        {
            if (item == nullptr)
                return;

            for (int i = 0; i < item->getNumSubItems(); ++i)
            {
                auto* subItem = item->getSubItem(i);
                if (auto* treeItem = dynamic_cast<TreeItem*>(subItem))
                {
                    if (treeItem->node().kind == ModelNodeKind::group)
                        treeItem->setOpen(expandedGroupIds.count(treeItem->node().id) > 0);
                }

                applyOpen(subItem);
            }
        };
        applyOpen(rootItem.get());

        applyDocumentSelectionToTree();
        suppressTreeSelectionCallback = false;
    }

    std::vector<LayerTreePanel::ModelNode> LayerTreePanel::buildNodesForParent(WidgetId parentId) const
    {
        const auto& snapshot = document.snapshot();
        const auto ownerByWidgetId = directOwnerByWidgetId(snapshot);
        const auto orderByWidgetId = widgetOrderById(snapshot);

        std::unordered_map<WidgetId, int> groupAnchorMemo;
        groupAnchorMemo.reserve(snapshot.groups.size());

        struct Candidate
        {
            bool isGroup = false;
            WidgetId id = kRootId;
            int anchor = std::numeric_limits<int>::max();
        };

        std::vector<Candidate> candidates;

        for (const auto& group : snapshot.groups)
        {
            if (group.parentGroupId.value_or(kRootId) != parentId)
                continue;

            std::unordered_set<WidgetId> visiting;
            const auto anchor = groupAnchorOrder(snapshot, group.id, orderByWidgetId, groupAnchorMemo, visiting);
            candidates.push_back({ true, group.id, anchor });
        }

        for (const auto& widget : snapshot.widgets)
        {
            const auto owner = ownerByWidgetId.count(widget.id) > 0 ? ownerByWidgetId.at(widget.id) : kRootId;
            if (owner != parentId)
                continue;

            candidates.push_back({ false, widget.id, orderByWidgetId.at(widget.id) });
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs)
        {
            if (lhs.anchor != rhs.anchor)
                return lhs.anchor < rhs.anchor;
            if (lhs.isGroup != rhs.isGroup)
                return lhs.isGroup < rhs.isGroup;
            return lhs.id < rhs.id;
        });

        std::vector<ModelNode> nodes;
        nodes.reserve(candidates.size());

        for (auto it = candidates.rbegin(); it != candidates.rend(); ++it)
        {
            ModelNode node;
            node.id = it->id;
            node.parentId = parentId;

            if (it->isGroup)
            {
                const auto* group = findGroupById(snapshot, it->id);
                if (group == nullptr)
                    continue;

                node.kind = ModelNodeKind::group;
                node.label = group->name.isNotEmpty() ? group->name : juce::String("Group");
                node.filterKeyLower = (node.label + " group").toLowerCase();
                node.selectionIds = collectGroupSelectionIds(group->id);
                node.children = buildNodesForParent(group->id);
            }
            else
            {
                const auto* widget = findWidgetById(snapshot, it->id);
                if (widget == nullptr)
                    continue;

                node.kind = ModelNodeKind::widget;
                node.label = widgetTypeLabel(widget->type) + " #" + juce::String(widget->id);
                node.filterKeyLower = (node.label + " " + widgetTypeLabel(widget->type)).toLowerCase();
                node.selectionIds = { widget->id };
            }

            nodes.push_back(std::move(node));
        }

        return nodes;
    }

    std::vector<LayerTreePanel::ModelNode> LayerTreePanel::filterNodes(const std::vector<ModelNode>& source, const juce::String& filterLower) const
    {
        if (filterLower.isEmpty())
            return source;

        std::vector<ModelNode> filtered;
        for (const auto& node : source)
        {
            auto childFiltered = filterNodes(node.children, filterLower);
            const auto selfMatches = modelNodeMatchesFilter(node, filterLower);
            if (!selfMatches && childFiltered.empty())
                continue;

            auto copy = node;
            copy.children = std::move(childFiltered);
            filtered.push_back(std::move(copy));
        }

        return filtered;
    }

    bool LayerTreePanel::modelNodeMatchesFilter(const ModelNode& node, const juce::String& filterLower) const
    {
        return filterLower.isEmpty() || node.filterKeyLower.contains(filterLower);
    }

    std::vector<WidgetId> LayerTreePanel::collectGroupSelectionIds(WidgetId groupId) const
    {
        return ::collectGroupWidgetsRecursive(document.snapshot(), groupId);
    }

    juce::String LayerTreePanel::keyForNode(ModelNodeKind kind, WidgetId id) const
    {
        return (kind == ModelNodeKind::group ? "g:" : "w:") + juce::String(id);
    }

    std::optional<LayerTreePanel::ModelNode> LayerTreePanel::findNodeByKey(const juce::String& key) const
    {
        std::function<std::optional<ModelNode>(const std::vector<ModelNode>&)> findRecursive = [&](const std::vector<ModelNode>& nodes) -> std::optional<ModelNode>
        {
            for (const auto& node : nodes)
            {
                if (keyForNode(node.kind, node.id) == key)
                    return node;

                if (auto child = findRecursive(node.children); child.has_value())
                    return child;
            }

            return std::nullopt;
        };

        return findRecursive(rootNodes);
    }

    std::optional<juce::String> LayerTreePanel::resolveSelectedNodeKey(const std::vector<WidgetId>& selection) const
    {
        if (selection.empty())
            return std::nullopt;

        if (selection.size() == 1)
        {
            const auto key = keyForNode(ModelNodeKind::widget, selection.front());
            if (selectionByKey.count(key) > 0)
                return key;
        }

        for (const auto& [key, ids] : selectionByKey)
        {
            if (sameWidgetIdSet(ids, selection))
                return key;
        }

        return std::nullopt;
    }

    void LayerTreePanel::applyDocumentSelectionToTree()
    {
        treeView.clearSelectedItems();
        const auto selectionKey = resolveSelectedNodeKey(document.editorState().selection);
        if (!selectionKey.has_value())
            return;

        for (auto* item : visibleTreeItems())
        {
            if (item->getUniqueName() == *selectionKey)
            {
                item->setSelected(true, true);
                return;
            }
        }
    }

    void LayerTreePanel::beginDrag(ModelNodeKind kind, WidgetId id, juce::Point<int> treePoint)
    {
        dragCandidate = true;
        dragActive = false;
        dragNodeKind = kind;
        draggedNodeIds = { id };
        dragStartPoint = treePoint;
        dropPreview.reset();
    }

    void LayerTreePanel::updateDrag(juce::Point<int> treePoint)
    {
        if (!dragCandidate)
            return;

        if (!dragActive)
        {
            const auto delta = treePoint - dragStartPoint;
            if (delta.getDistanceFromOrigin() < 4)
                return;
            dragActive = true;
        }

        dropPreview = computeDropPreview(treePoint);
        repaint();
    }

    void LayerTreePanel::endDrag()
    {
        if (!dragCandidate)
            return;

        if (dragActive && dropPreview.has_value() && onDropRequest != nullptr)
        {
            if (auto request = buildDropRequest(*dropPreview); request.has_value())
            {
                const auto result = onDropRequest(*request);
                if (result.failed())
                    DBG("[Gyeol] Layer drop rejected: " + result.getErrorMessage());
            }
        }

        dragCandidate = false;
        dragActive = false;
        draggedNodeIds.clear();
        dropPreview.reset();
        repaint();
    }

    void LayerTreePanel::handleTreeSelection(const juce::String& key)
    {
        if (suppressTreeSelectionCallback)
            return;
        if (onSelectionChanged == nullptr)
            return;

        const auto it = selectionByKey.find(key);
        if (it == selectionByKey.end())
            return;

        onSelectionChanged(it->second);
    }

    std::optional<LayerTreePanel::DropPreview> LayerTreePanel::computeDropPreview(juce::Point<int> treePoint) const
    {
        auto items = visibleTreeItems();
        if (items.empty())
            return std::nullopt;

        TreeItem* target = nullptr;
        for (auto* item : items)
        {
            if (item->getItemPosition(true).contains(treePoint))
            {
                target = item;
                break;
            }
        }

        if (target == nullptr)
        {
            target = *std::min_element(items.begin(), items.end(), [treePoint](const TreeItem* lhs, const TreeItem* rhs)
            {
                const auto lhsDistance = std::abs(lhs->getItemPosition(true).getCentreY() - treePoint.y);
                const auto rhsDistance = std::abs(rhs->getItemPosition(true).getCentreY() - treePoint.y);
                return lhsDistance < rhsDistance;
            });
        }

        if (target == nullptr)
            return std::nullopt;

        DropPreview preview;
        preview.targetNodeKey = target->getUniqueName();

        const auto row = target->getItemPosition(true);
        const auto upperZone = row.getY() + row.getHeight() / 3;
        const auto lowerZone = row.getBottom() - row.getHeight() / 3;

        if (treePoint.y <= upperZone)
        {
            preview.placement = Interaction::LayerDropPlacement::before;
            preview.markerBounds = { row.getX() + 4, row.getY() - 1, std::max(8, row.getWidth() - 8), 2 };
        }
        else if (treePoint.y >= lowerZone)
        {
            preview.placement = Interaction::LayerDropPlacement::after;
            preview.markerBounds = { row.getX() + 4, row.getBottom() - 1, std::max(8, row.getWidth() - 8), 2 };
        }
        else if (target->node().kind == ModelNodeKind::group)
        {
            preview.placement = Interaction::LayerDropPlacement::into;
            preview.markerBounds = row.reduced(2, 1);
        }
        else
        {
            preview.placement = treePoint.y < row.getCentreY()
                                    ? Interaction::LayerDropPlacement::before
                                    : Interaction::LayerDropPlacement::after;
            const auto markerY = preview.placement == Interaction::LayerDropPlacement::before ? row.getY() : row.getBottom();
            preview.markerBounds = { row.getX() + 4, markerY - 1, std::max(8, row.getWidth() - 8), 2 };
        }

        if (isDropTargetInDraggedSubtree(preview.targetNodeKey))
            return std::nullopt;

        return preview;
    }

    std::optional<Interaction::LayerTreeDropRequest> LayerTreePanel::buildDropRequest(const DropPreview& preview) const
    {
        const auto parentAndInsert = resolveDropParentAndInsert(preview);
        if (!parentAndInsert.has_value())
            return std::nullopt;

        Interaction::LayerTreeDropRequest request;
        request.placement = preview.placement;
        request.parentId = parentAndInsert->first;
        request.insertIndex = parentAndInsert->second;

        const auto kind = dragNodeKind == ModelNodeKind::group
                              ? LayerNodeKind::group
                              : LayerNodeKind::widget;
        for (const auto draggedId : draggedNodeIds)
            request.dragged.push_back({ kind, draggedId });

        if (const auto targetNode = findNodeByKey(preview.targetNodeKey); targetNode.has_value())
        {
            request.target = LayerNodeRef { targetNode->kind == ModelNodeKind::group
                                                ? LayerNodeKind::group
                                                : LayerNodeKind::widget,
                                            targetNode->id };
        }

        return request;
    }

    std::optional<std::pair<WidgetId, int>> LayerTreePanel::resolveDropParentAndInsert(const DropPreview& preview) const
    {
        const auto targetNode = findNodeByKey(preview.targetNodeKey);
        if (!targetNode.has_value())
            return std::nullopt;

        if (preview.placement == Interaction::LayerDropPlacement::into)
        {
            if (targetNode->kind != ModelNodeKind::group)
                return std::nullopt;
            return std::make_pair(targetNode->id, -1);
        }

        const auto parentId = targetNode->parentId;
        const auto* siblings = static_cast<const std::vector<ModelNode>*>(nullptr);
        if (parentId == kRootId)
        {
            siblings = &rootNodes;
        }
        else
        {
            const auto parentNode = findNodeByKey(keyForNode(ModelNodeKind::group, parentId));
            if (!parentNode.has_value())
                return std::nullopt;
            siblings = &parentNode->children;
        }

        if (siblings == nullptr)
            return std::nullopt;

        std::unordered_set<WidgetId> draggedSet(draggedNodeIds.begin(), draggedNodeIds.end());
        std::vector<ModelNode> filtered;
        filtered.reserve(siblings->size());

        for (const auto& sibling : *siblings)
        {
            if (sibling.kind == dragNodeKind && draggedSet.count(sibling.id) > 0)
                continue;
            filtered.push_back(sibling);
        }

        const auto targetIt = std::find_if(filtered.begin(),
                                           filtered.end(),
                                           [this, &preview](const ModelNode& node)
                                           {
                                               return keyForNode(node.kind, node.id) == preview.targetNodeKey;
                                           });
        if (targetIt == filtered.end())
            return std::nullopt;

        const auto targetFrontIndex = static_cast<int>(std::distance(filtered.begin(), targetIt));
        const auto siblingCount = static_cast<int>(filtered.size());
        const auto targetBackIndex = siblingCount - 1 - targetFrontIndex;
        const auto insertIndex = preview.placement == Interaction::LayerDropPlacement::before
                                     ? targetBackIndex + 1
                                     : targetBackIndex;

        return std::make_pair(parentId, insertIndex);
    }

    bool LayerTreePanel::isDropTargetInDraggedSubtree(const juce::String& targetNodeKey) const
    {
        const auto targetNode = findNodeByKey(targetNodeKey);
        if (!targetNode.has_value())
            return true;

        if (dragNodeKind == ModelNodeKind::widget)
        {
            return targetNode->kind == ModelNodeKind::widget
                && std::find(draggedNodeIds.begin(), draggedNodeIds.end(), targetNode->id) != draggedNodeIds.end();
        }

        for (const auto draggedGroupId : draggedNodeIds)
        {
            if (targetNode->kind == ModelNodeKind::group && targetNode->id == draggedGroupId)
                return true;

            auto parentCursor = targetNode->parentId;
            while (parentCursor != kRootId)
            {
                if (parentCursor == draggedGroupId)
                    return true;

                const auto parentNode = findNodeByKey(keyForNode(ModelNodeKind::group, parentCursor));
                if (!parentNode.has_value())
                    break;
                parentCursor = parentNode->parentId;
            }
        }

        return false;
    }

    std::vector<LayerTreePanel::TreeItem*> LayerTreePanel::visibleTreeItems() const
    {
        std::vector<TreeItem*> items;
        collectVisibleTreeItems(rootItem.get(), items);
        return items;
    }

    void LayerTreePanel::collectVisibleTreeItems(juce::TreeViewItem* item, std::vector<TreeItem*>& out)
    {
        if (item == nullptr)
            return;

        for (int i = 0; i < item->getNumSubItems(); ++i)
        {
            auto* subItem = item->getSubItem(i);
            if (auto* treeItem = dynamic_cast<TreeItem*>(subItem))
            {
                if (treeItem->getItemPosition(true).getHeight() > 0)
                    out.push_back(treeItem);
            }

            collectVisibleTreeItems(subItem, out);
        }
    }

    bool LayerTreePanel::sameWidgetIdSet(const std::vector<WidgetId>& lhs, const std::vector<WidgetId>& rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        auto lhsSorted = lhs;
        auto rhsSorted = rhs;
        std::sort(lhsSorted.begin(), lhsSorted.end());
        std::sort(rhsSorted.begin(), rhsSorted.end());
        return lhsSorted == rhsSorted;
    }
}
