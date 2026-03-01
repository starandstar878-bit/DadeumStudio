#include "Gyeol/Editor/Panels/LayerTreePanel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace
{
    void logLayerTreeDnd(const juce::String& message)
    {
        DBG("[Gyeol][LayerTreeDnD] " + message);
    }

    const char* dropPlacementLabel(Gyeol::Ui::Interaction::LayerDropPlacement placement) noexcept
    {
        switch (placement)
        {
            case Gyeol::Ui::Interaction::LayerDropPlacement::before: return "before";
            case Gyeol::Ui::Interaction::LayerDropPlacement::after: return "after";
            case Gyeol::Ui::Interaction::LayerDropPlacement::into: return "into";
        }

        return "unknown";
    }

    juce::String widgetIdsToDebugString(const std::vector<Gyeol::WidgetId>& ids)
    {
        juce::StringArray tokens;
        for (const auto id : ids)
            tokens.add(juce::String(id));
        return "[" + tokens.joinIntoString(",") + "]";
    }

    juce::String parentRefToDebugString(const Gyeol::ParentRef& parent)
    {
        juce::String kind = "unknown";
        if (parent.kind == Gyeol::ParentKind::root)
            kind = "root";
        else if (parent.kind == Gyeol::ParentKind::layer)
            kind = "layer";
        else if (parent.kind == Gyeol::ParentKind::group)
            kind = "group";

        return kind + ":" + juce::String(parent.id);
    }

    juce::String nodeRefToDebugString(const std::optional<Gyeol::NodeRef>& node)
    {
        if (!node.has_value())
            return "none";

        juce::String kind = "unknown";
        if (node->kind == Gyeol::NodeKind::widget)
            kind = "widget";
        else if (node->kind == Gyeol::NodeKind::group)
            kind = "group";
        else if (node->kind == Gyeol::NodeKind::layer)
            kind = "layer";

        return kind + ":" + juce::String(node->id);
    }

    juce::String widgetTypeLabel(const Gyeol::Widgets::WidgetFactory& widgetFactory, Gyeol::WidgetType type)
    {
        if (const auto* descriptor = widgetFactory.descriptorFor(type); descriptor != nullptr)
        {
            if (descriptor->displayName.isNotEmpty())
                return descriptor->displayName;
            if (descriptor->typeKey.isNotEmpty())
                return descriptor->typeKey;
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

    const Gyeol::LayerModel* findLayerById(const Gyeol::DocumentModel& document, Gyeol::WidgetId layerId) noexcept
    {
        const auto it = std::find_if(document.layers.begin(),
                                     document.layers.end(),
                                     [layerId](const Gyeol::LayerModel& layer)
                                     {
                                         return layer.id == layerId;
                                     });
        return it == document.layers.end() ? nullptr : &(*it);
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
                anchor = std::min(anchor, groupAnchorOrder(document, child.id, widgetOrder, memo, visiting));
            }
        }

        visiting.erase(groupId);
        memo[groupId] = anchor;
        return anchor;
    }

    void collectGroupWidgetsRecursive(const Gyeol::DocumentModel& document,
                                      Gyeol::WidgetId groupId,
                                      std::unordered_set<Gyeol::WidgetId>& out,
                                      std::unordered_set<Gyeol::WidgetId>& visited)
    {
        if (!visited.insert(groupId).second)
            return;

        const auto* group = findGroupById(document, groupId);
        if (group == nullptr)
            return;

        for (const auto memberId : group->memberWidgetIds)
            out.insert(memberId);

        for (const auto& candidate : document.groups)
        {
            if (candidate.parentGroupId.value_or(Gyeol::kRootId) == groupId)
                collectGroupWidgetsRecursive(document, candidate.id, out, visited);
        }
    }
}

namespace Gyeol::Ui::Panels
{
    class LayerTreePanel::TreeItem : public juce::TreeViewItem
    {
    public:
        TreeItem(LayerTreePanel& ownerIn, const LayerTreePanel::ModelNode& modelNodeIn)
            : owner(ownerIn), modelNode(modelNodeIn)
        {
            for (const auto& child : modelNode.children)
                addSubItem(new TreeItem(owner, child));
        }

        juce::String getUniqueName() const override { return owner.keyForNode(modelNode.kind, modelNode.id); }
        bool mightContainSubItems() override { return !modelNode.children.empty(); }
        int getItemHeight() const override { return 22; }
        const LayerTreePanel::ModelNode& node() const noexcept { return modelNode; }
        int paintWidth() const noexcept { return lastPaintWidth; }
        bool applyModel(const LayerTreePanel::ModelNode& nextModel)
        {
            if (modelNode.kind != nextModel.kind || modelNode.id != nextModel.id)
                return false;
            if (getNumSubItems() != static_cast<int>(nextModel.children.size()))
                return false;

            modelNode = nextModel;

            for (int i = 0; i < getNumSubItems(); ++i)
            {
                auto* childItem = dynamic_cast<TreeItem*>(getSubItem(i));
                if (childItem == nullptr)
                    return false;
                if (!childItem->applyModel(nextModel.children[static_cast<size_t>(i)]))
                    return false;
            }

            return true;
        }

        void paintItem(juce::Graphics& g, int width, int height) override
        {
            lastPaintWidth = width;
            if (isSelected())
            {
                g.setColour(juce::Colour::fromRGB(56, 98, 160));
                g.fillRoundedRectangle(juce::Rectangle<float>(1.0f, 1.0f, static_cast<float>(width - 2), static_cast<float>(height - 2)), 4.0f);
            }

            juce::Colour text = juce::Colour::fromRGB(206, 212, 222);
            float fontSize = 11.5f;
            int fontWeight = juce::Font::plain;
            if (modelNode.kind == ModelNodeKind::layer)
            {
                text = juce::Colour::fromRGB(180, 214, 252);
                fontSize = 12.0f;
                fontWeight = juce::Font::bold;
            }
            else if (modelNode.kind == ModelNodeKind::group)
            {
                text = juce::Colour::fromRGB(233, 210, 160);
                fontWeight = juce::Font::bold;
            }

            g.setColour(text);
            g.setFont(juce::FontOptions(fontSize, fontWeight));
            g.drawFittedText(modelNode.label, juce::Rectangle<int>(0, 0, width - 40, height).reduced(6, 1), juce::Justification::centredLeft, 1);

            const auto iconHeight = juce::jmax(12, height - 6);
            const auto visibleBounds = juce::Rectangle<int>(juce::jmax(0, width - 36), 3, 14, iconHeight);
            const auto lockedBounds = juce::Rectangle<int>(juce::jmax(0, width - 18), 3, 14, iconHeight);

            g.setColour(modelNode.visible ? juce::Colour::fromRGB(120, 220, 150) : juce::Colour::fromRGB(120, 120, 120));
            g.drawRoundedRectangle(visibleBounds.toFloat(), 2.0f, 1.0f);
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawFittedText("V", visibleBounds, juce::Justification::centred, 1);

            g.setColour(modelNode.locked ? juce::Colour::fromRGB(245, 180, 90) : juce::Colour::fromRGB(120, 120, 120));
            g.drawRoundedRectangle(lockedBounds.toFloat(), 2.0f, 1.0f);
            g.drawFittedText("L", lockedBounds, juce::Justification::centred, 1);
        }

        void itemSelectionChanged(bool isNowSelected) override
        {
            if (isNowSelected)
                owner.handleTreeSelection(getUniqueName());
        }

        void itemOpennessChanged(bool isNowOpen) override
        {
            if (modelNode.kind == ModelNodeKind::layer)
            {
                if (isNowOpen) owner.expandedLayerIds.insert(modelNode.id);
                else owner.expandedLayerIds.erase(modelNode.id);
                return;
            }
            if (modelNode.kind == ModelNodeKind::group)
            {
                if (isNowOpen) owner.expandedGroupIds.insert(modelNode.id);
                else owner.expandedGroupIds.erase(modelNode.id);
            }
        }

        void itemClicked(const juce::MouseEvent& event) override
        {
            const auto icon = owner.iconHitForTreeItem(*this, event.getPosition());
            if (icon != RowIcon::none)
            {
                owner.toggleNodeIcon(modelNode, icon);
                return;
            }

            juce::TreeViewItem::itemClicked(event);
        }

    private:
        LayerTreePanel& owner;
        LayerTreePanel::ModelNode modelNode;
        int lastPaintWidth = 0;
    };

    class LayerTreePanel::RootItem : public juce::TreeViewItem
    {
    public:
        RootItem(LayerTreePanel& owner, const std::vector<LayerTreePanel::ModelNode>& nodes)
        {
            for (const auto& node : nodes)
                addSubItem(new TreeItem(owner, node));
        }

        juce::String getUniqueName() const override { return "layer-root"; }
        bool mightContainSubItems() override { return true; }
        bool canBeSelected() const override { return false; }
    };

    LayerTreePanel::LayerTreePanel(DocumentHandle& documentIn, const Widgets::WidgetFactory& widgetFactoryIn)
        : document(documentIn),
          widgetFactory(widgetFactoryIn)
    {
        addAndMakeVisible(createLayerButton);
        createLayerButton.onClick = [this] { handleCreateLayerButton(); };

        addAndMakeVisible(deleteLayerButton);
        deleteLayerButton.onClick = [this] { handleDeleteLayerButton(); };

        addAndMakeVisible(searchBox);
        searchBox.setTextToShowWhenEmpty("Search layers...", juce::Colour::fromRGB(118, 126, 140));
        searchBox.onTextChange = [this]
        {
            pendingRefreshReason = RefreshReason::searchChanged;
            refreshFromDocument();
        };

        addAndMakeVisible(treeView);
        treeView.setRootItemVisible(false);
        treeView.setMultiSelectEnabled(false);
        treeView.setDefaultOpenness(true);
        treeView.setColour(juce::TreeView::backgroundColourId, juce::Colour::fromRGB(24, 28, 34));
        treeView.setColour(juce::TreeView::linesColourId, juce::Colour::fromRGBA(255, 255, 255, 18));
        treeView.addMouseListener(this, true);

        pendingRefreshReason = RefreshReason::initial;
        refreshFromDocument();
    }

    LayerTreePanel::~LayerTreePanel()
    {
        dragCandidate = false;
        dragActive = false;
        draggedNodeIds.clear();
        dropPreview.reset();
        resetAutoExpandState();
        treeView.removeMouseListener(this);
        treeView.setRootItem(nullptr);
        rootItem.reset();
    }

    void LayerTreePanel::refreshFromDocument()
    {
        const auto reason = pendingRefreshReason;
        pendingRefreshReason = RefreshReason::external;

        const auto refreshStart = std::chrono::steady_clock::now();
        const auto rebuildModelStart = refreshStart;

        const auto previousNodes = rootNodes;
        const auto previousSelectionByKey = selectionByKey;
        const auto previousSelectedKey = lastAppliedSelectionKey;

        rebuildModel();
        const auto rebuildModelEnd = std::chrono::steady_clock::now();

        const auto structureChanged = !sameNodeStructure(previousNodes, rootNodes);
        const auto visualsChanged = !sameNodeVisuals(previousNodes, rootNodes);
        const auto selectionMapChanged = !(previousSelectionByKey == selectionByKey);

        bool rebuiltTree = false;
        std::vector<juce::String> changedVisualKeys;
        if (structureChanged)
        {
            rebuildTree();
            rebuiltTree = true;
        }
        else if (visualsChanged)
        {
            if (!applyModelToExistingTree())
            {
                rebuildTree();
                rebuiltTree = true;
            }
            else
            {
                collectVisualChangedKeys(previousNodes, rootNodes, changedVisualKeys);
                repaintRowsForKeys(changedVisualKeys);
            }
        }

        const auto selectedKey = resolveSelectedNodeKey(document.editorState().selection);
        if (!rebuiltTree && (selectionMapChanged || previousSelectedKey != selectedKey))
        {
            suppressTreeSelectionCallback = true;
            applyDocumentSelectionToTree();
            suppressTreeSelectionCallback = false;
        }

        const auto rebuildTreeEnd = std::chrono::steady_clock::now();
        const auto refreshEnd = std::chrono::steady_clock::now();

        perf.refreshCount += 1;
        perf.lastRebuildModelMs = std::chrono::duration<double, std::milli>(rebuildModelEnd - rebuildModelStart).count();
        perf.lastRebuildTreeMs = std::chrono::duration<double, std::milli>(rebuildTreeEnd - rebuildModelEnd).count();
        perf.lastRefreshMs = std::chrono::duration<double, std::milli>(refreshEnd - refreshStart).count();
        perf.maxRefreshMs = std::max(perf.maxRefreshMs, perf.lastRefreshMs);
        perf.lastModelNodeCount = countModelNodes(rootNodes);
        perf.lastTreeItemCount = countTreeItems(rootItem.get());
        perf.lastVisibleItemCount = static_cast<int>(visibleTreeItems().size());
        perf.lastSelectionEntryCount = static_cast<int>(selectionByKey.size());
        deleteLayerButton.setEnabled(document.snapshot().layers.size() > 1);
        const auto dragUpdatesSinceRefresh = perf.dragPreviewUpdateCountSinceRefresh;
        perf.dragPreviewUpdateCountSinceRefresh = 0;

        const auto shouldLog = perf.lastRefreshMs >= slowRefreshLogThresholdMs
                            || (periodicPerfLogInterval > 0 && perf.refreshCount % periodicPerfLogInterval == 0);
        if (shouldLog)
        {
            DBG(juce::String("[Gyeol][LayerTreePanel][Perf] refresh#")
                + juce::String(static_cast<int64_t>(perf.refreshCount))
                + " reason=" + refreshReasonToString(reason)
                + " totalMs=" + juce::String(perf.lastRefreshMs, 3)
                + " modelMs=" + juce::String(perf.lastRebuildModelMs, 3)
                + " treeMs=" + juce::String(perf.lastRebuildTreeMs, 3)
                + " modelNodes=" + juce::String(perf.lastModelNodeCount)
                + " treeItems=" + juce::String(perf.lastTreeItemCount)
                + " visibleRows=" + juce::String(perf.lastVisibleItemCount)
                + " selectionKeys=" + juce::String(perf.lastSelectionEntryCount)
                + " dragUpdatesSinceRefresh=" + juce::String(static_cast<int64_t>(dragUpdatesSinceRefresh))
                + " dragUpdatesTotal=" + juce::String(static_cast<int64_t>(perf.dragPreviewUpdateCount))
                + " structureChanged=" + juce::String(structureChanged ? 1 : 0)
                + " visualsChanged=" + juce::String(visualsChanged ? 1 : 0)
                + " rebuiltTree=" + juce::String(rebuiltTree ? 1 : 0)
                + " maxRefreshMs=" + juce::String(perf.maxRefreshMs, 3));
        }
    }

    void LayerTreePanel::setSelectionChangedCallback(std::function<void(std::vector<WidgetId>)> callback)
    {
        onSelectionChanged = std::move(callback);
    }

    std::optional<NodeRef> LayerTreePanel::selectedNode() const
    {
        if (const auto* selected = dynamic_cast<TreeItem*>(treeView.getSelectedItem(0)); selected != nullptr)
            return NodeRef { nodeKindFromModelKind(selected->node().kind), selected->node().id };

        return std::nullopt;
    }

    void LayerTreePanel::setActiveLayerChangedCallback(std::function<void(std::optional<WidgetId>)> callback)
    {
        onActiveLayerChanged = std::move(callback);
    }

    void LayerTreePanel::setDropRequestCallback(std::function<juce::Result(const Interaction::LayerTreeDropRequest&)> callback)
    {
        onDropRequest = std::move(callback);
    }

    void LayerTreePanel::setNodePropsChangedCallback(std::function<juce::Result(const SetPropsAction&)> callback)
    {
        onNodePropsChanged = std::move(callback);
    }

    void LayerTreePanel::setCreateLayerRequestedCallback(std::function<std::optional<WidgetId>()> callback)
    {
        onCreateLayerRequested = std::move(callback);
    }

    void LayerTreePanel::setDeleteLayerRequestedCallback(std::function<juce::Result(WidgetId)> callback)
    {
        onDeleteLayerRequested = std::move(callback);
    }

    const char* LayerTreePanel::dragKindToken(ModelNodeKind kind) noexcept
    {
        switch (kind)
        {
            case ModelNodeKind::layer: return "layer";
            case ModelNodeKind::group: return "group";
            case ModelNodeKind::widget: return "widget";
        }

        return "widget";
    }

    std::optional<LayerTreePanel::ModelNodeKind> LayerTreePanel::modelKindFromDragToken(const juce::String& token)
    {
        if (token == "layer")
            return ModelNodeKind::layer;
        if (token == "group")
            return ModelNodeKind::group;
        if (token == "widget")
            return ModelNodeKind::widget;
        return std::nullopt;
    }

    juce::var LayerTreePanel::buildDragDescription(ModelNodeKind kind, const std::vector<WidgetId>& ids) const
    {
        juce::String payload = "gyeol-layer-tree-drag|";
        payload += dragKindToken(kind);
        payload += "|";
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (i > 0)
                payload += ",";
            payload += juce::String(ids[i]);
        }
        return juce::var(payload);
    }

    bool LayerTreePanel::parseDragDescription(const juce::var& description,
                                              ModelNodeKind& outKind,
                                              std::vector<WidgetId>& outIds) const
    {
        outIds.clear();
        if (!description.isString())
            return false;

        const auto text = description.toString();
        constexpr const char* kPrefix = "gyeol-layer-tree-drag|";
        if (!text.startsWith(kPrefix))
            return false;

        const auto payload = text.fromFirstOccurrenceOf(kPrefix, false, false);
        const auto separatorIndex = payload.indexOfChar('|');
        if (separatorIndex <= 0)
            return false;

        const auto token = payload.substring(0, separatorIndex);
        const auto parsedKind = modelKindFromDragToken(token);
        if (!parsedKind.has_value())
            return false;

        std::unordered_set<WidgetId> seenIds;
        const auto idCsv = payload.substring(separatorIndex + 1);
        juce::StringArray parts;
        parts.addTokens(idCsv, ",", "\"");
        for (const auto& part : parts)
        {
            const auto id = static_cast<WidgetId>(part.trim().getLargeIntValue());
            if (id <= kRootId)
                continue;
            if (seenIds.insert(id).second)
                outIds.push_back(id);
        }

        if (outIds.empty())
            return false;

        outKind = *parsedKind;
        return true;
    }

    void LayerTreePanel::resized()
    {
        auto bounds = getLocalBounds().reduced(6);
        auto commandRow = bounds.removeFromTop(24);
        createLayerButton.setBounds(commandRow.removeFromLeft(86));
        commandRow.removeFromLeft(6);
        deleteLayerButton.setBounds(commandRow.removeFromLeft(94));

        bounds.removeFromTop(4);
        searchBox.setBounds(bounds.removeFromTop(24));
        bounds.removeFromTop(4);
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

    void LayerTreePanel::mouseMove(const juce::MouseEvent& event)
    {
        juce::ignoreUnused(event);
    }

    void LayerTreePanel::mouseExit(const juce::MouseEvent&)
    {
    }

    void LayerTreePanel::mouseDown(const juce::MouseEvent& event)
    {
        if (!event.mods.isLeftButtonDown())
            return;
        if (!treeView.isParentOf(event.eventComponent) && event.eventComponent != &treeView)
            return;

        const auto treePoint = event.getEventRelativeTo(&treeView).position.toInt();
        for (auto* item : visibleTreeItems())
        {
            const auto row = rowBoundsForItem(*item);
            if (!row.contains(treePoint))
                continue;

            const auto local = treePoint - row.getPosition();
            if (iconHitForTreeItem(*item, local) != RowIcon::none)
            {
                logLayerTreeDnd("mouseDown ignored: icon click node=" + keyForNode(item->node().kind, item->node().id));
                return;
            }

            beginDrag(item->node().kind, item->node().id, treePoint);
            return;
        }
    }

    void LayerTreePanel::mouseDrag(const juce::MouseEvent& event)
    {
        if (!dragCandidate)
            return;
        updateDrag(event.getEventRelativeTo(&treeView).position.toInt());
    }

    void LayerTreePanel::mouseUp(const juce::MouseEvent&)
    {
        endDrag();
    }

    bool LayerTreePanel::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        ModelNodeKind kind = ModelNodeKind::widget;
        std::vector<WidgetId> ids;
        const auto ok = parseDragDescription(dragSourceDetails.description, kind, ids)
            && !ids.empty();
        logLayerTreeDnd("isInterestedInDragSource=" + juce::String(ok ? "true" : "false"));
        return ok;
    }

    void LayerTreePanel::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        itemDragMove(dragSourceDetails);
    }

    void LayerTreePanel::itemDragMove(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        ModelNodeKind kind = ModelNodeKind::widget;
        std::vector<WidgetId> ids;
        if (!parseDragDescription(dragSourceDetails.description, kind, ids) || ids.empty())
        {
            logLayerTreeDnd("itemDragMove ignored: cannot parse drag description");
            return;
        }

        dragCandidate = true;
        dragActive = true;
        dragNodeKind = kind;
        draggedNodeIds = std::move(ids);

        const auto treePoint = dragSourceDetails.localPosition.toInt() - treeView.getPosition();
        handleDragMoveAt(treePoint);
    }

    void LayerTreePanel::itemDragExit(const juce::DragAndDropTarget::SourceDetails&)
    {
        const auto previousPreview = dropPreview;
        dropPreview.reset();
        resetAutoExpandState();
        repaintDropPreviewDiff(previousPreview, dropPreview);
    }

    void LayerTreePanel::itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
    {
        ModelNodeKind kind = ModelNodeKind::widget;
        std::vector<WidgetId> ids;
        if (!parseDragDescription(dragSourceDetails.description, kind, ids) || ids.empty())
        {
            logLayerTreeDnd("itemDropped ignored: cannot parse drag description");
            endDrag();
            return;
        }

        dragCandidate = true;
        dragActive = true;
        dragNodeKind = kind;
        draggedNodeIds = std::move(ids);

        const auto treePoint = dragSourceDetails.localPosition.toInt() - treeView.getPosition();
        handleDragMoveAt(treePoint);
        endDrag();
    }

    std::vector<LayerTreePanel::ModelNode> LayerTreePanel::buildLayerNodes() const
    {
        const auto& snapshot = document.snapshot();
        std::vector<const LayerModel*> layers;
        layers.reserve(snapshot.layers.size());
        for (const auto& layer : snapshot.layers)
            layers.push_back(&layer);

        std::sort(layers.begin(), layers.end(), [](const LayerModel* lhs, const LayerModel* rhs)
        {
            if (lhs == nullptr || rhs == nullptr)
                return lhs != nullptr;
            if (lhs->order != rhs->order)
                return lhs->order > rhs->order;
            return lhs->id < rhs->id;
        });

        std::vector<LayerTreePanel::ModelNode> nodes;
        nodes.reserve(layers.size());
        for (const auto* layer : layers)
        {
            if (layer == nullptr)
                continue;

            LayerTreePanel::ModelNode node;
            node.kind = ModelNodeKind::layer;
            node.id = layer->id;
            node.parent.kind = ParentKind::root;
            node.parent.id = kRootId;
            node.label = layer->name.isNotEmpty() ? layer->name : juce::String("Layer");
            node.filterKeyLower = (node.label + " layer").toLowerCase();
            node.visible = layer->visible;
            node.locked = layer->locked;
            node.children = buildNodesForParent({ ParentKind::layer, layer->id });
            nodes.push_back(std::move(node));
        }
        return nodes;
    }

    std::vector<LayerTreePanel::ModelNode> LayerTreePanel::buildNodesForParent(const ParentRef& parent) const
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
        if (parent.kind == ParentKind::layer)
        {
            const auto* layer = findLayerById(snapshot, parent.id);
            if (layer == nullptr)
                return {};

            std::unordered_set<WidgetId> layerGroups(layer->memberGroupIds.begin(), layer->memberGroupIds.end());
            std::unordered_set<WidgetId> layerWidgets(layer->memberWidgetIds.begin(), layer->memberWidgetIds.end());

            for (const auto& group : snapshot.groups)
            {
                if (group.parentGroupId.has_value())
                    continue;
                if (layerGroups.count(group.id) == 0)
                    continue;

                std::unordered_set<WidgetId> visiting;
                const auto anchor = groupAnchorOrder(snapshot, group.id, orderByWidgetId, groupAnchorMemo, visiting);
                candidates.push_back({ true, group.id, anchor });
            }

            for (const auto& widget : snapshot.widgets)
            {
                const auto owner = ownerByWidgetId.count(widget.id) > 0 ? ownerByWidgetId.at(widget.id) : kRootId;
                if (owner != kRootId || layerWidgets.count(widget.id) == 0)
                    continue;
                candidates.push_back({ false, widget.id, orderByWidgetId.at(widget.id) });
            }
        }
        else if (parent.kind == ParentKind::group)
        {
            for (const auto& group : snapshot.groups)
            {
                if (group.parentGroupId.value_or(kRootId) != parent.id)
                    continue;

                std::unordered_set<WidgetId> visiting;
                const auto anchor = groupAnchorOrder(snapshot, group.id, orderByWidgetId, groupAnchorMemo, visiting);
                candidates.push_back({ true, group.id, anchor });
            }

            for (const auto& widget : snapshot.widgets)
            {
                const auto owner = ownerByWidgetId.count(widget.id) > 0 ? ownerByWidgetId.at(widget.id) : kRootId;
                if (owner != parent.id)
                    continue;
                candidates.push_back({ false, widget.id, orderByWidgetId.at(widget.id) });
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs)
        {
            if (lhs.anchor != rhs.anchor)
                return lhs.anchor < rhs.anchor;
            if (lhs.isGroup != rhs.isGroup)
                return lhs.isGroup < rhs.isGroup;
            return lhs.id < rhs.id;
        });

        std::vector<LayerTreePanel::ModelNode> nodes;
        nodes.reserve(candidates.size());
        for (auto it = candidates.rbegin(); it != candidates.rend(); ++it)
        {
            LayerTreePanel::ModelNode node;
            node.id = it->id;
            node.parent = parent;

            if (it->isGroup)
            {
                const auto* group = findGroupById(snapshot, it->id);
                if (group == nullptr)
                    continue;

                node.kind = ModelNodeKind::group;
                node.label = group->name.isNotEmpty() ? group->name : juce::String("Group");
                node.filterKeyLower = (node.label + " group").toLowerCase();
                node.visible = group->visible;
                node.locked = group->locked;
                node.selectionIds = collectGroupSelectionIds(group->id);
                node.children = buildNodesForParent({ ParentKind::group, group->id });
            }
            else
            {
                const auto* widget = findWidgetById(snapshot, it->id);
                if (widget == nullptr)
                    continue;

                node.kind = ModelNodeKind::widget;
                const auto typeLabel = widgetTypeLabel(widgetFactory, widget->type);
                node.label = typeLabel + " #" + juce::String(widget->id);
                node.filterKeyLower = (node.label + " " + typeLabel).toLowerCase();
                node.visible = widget->visible;
                node.locked = widget->locked;
                node.selectionIds = { widget->id };
            }

            nodes.push_back(std::move(node));
        }

        return nodes;
    }

    std::vector<LayerTreePanel::ModelNode> LayerTreePanel::filterNodes(const std::vector<LayerTreePanel::ModelNode>& source, const juce::String& filterLower) const
    {
        if (filterLower.isEmpty())
            return source;

        std::vector<LayerTreePanel::ModelNode> filtered;
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

    bool LayerTreePanel::modelNodeMatchesFilter(const LayerTreePanel::ModelNode& node, const juce::String& filterLower) const
    {
        return filterLower.isEmpty() || node.filterKeyLower.contains(filterLower);
    }

    std::vector<WidgetId> LayerTreePanel::collectGroupSelectionIds(WidgetId groupId) const
    {
        std::unordered_set<WidgetId> widgets;
        std::unordered_set<WidgetId> visited;
        collectGroupWidgetsRecursive(document.snapshot(), groupId, widgets, visited);

        std::vector<WidgetId> ordered;
        ordered.reserve(widgets.size());
        for (const auto& widget : document.snapshot().widgets)
        {
            if (widgets.count(widget.id) > 0)
                ordered.push_back(widget.id);
        }
        return ordered;
    }

    juce::String LayerTreePanel::keyForNode(ModelNodeKind kind, WidgetId id) const
    {
        if (kind == ModelNodeKind::layer)
            return "l:" + juce::String(id);
        if (kind == ModelNodeKind::group)
            return "g:" + juce::String(id);
        return "w:" + juce::String(id);
    }

    std::optional<LayerTreePanel::ModelNode> LayerTreePanel::findNodeByKey(const juce::String& key,
                                                                            const std::vector<LayerTreePanel::ModelNode>& nodes) const
    {
        std::function<std::optional<LayerTreePanel::ModelNode>(const std::vector<LayerTreePanel::ModelNode>&)> findRecursive = [&](const std::vector<LayerTreePanel::ModelNode>& currentNodes) -> std::optional<LayerTreePanel::ModelNode>
        {
            for (const auto& node : currentNodes)
            {
                if (keyForNode(node.kind, node.id) == key)
                    return node;
                if (auto child = findRecursive(node.children); child.has_value())
                    return child;
            }
            return std::nullopt;
        };

        return findRecursive(nodes);
    }

    std::optional<LayerTreePanel::ModelNode> LayerTreePanel::findNodeByKey(const juce::String& key) const
    {
        return findNodeByKey(key, fullRootNodes);
    }

    std::optional<juce::String> LayerTreePanel::resolveSelectedNodeKey(const std::vector<WidgetId>& selection) const
    {
        if (selection.empty())
        {
            if (explicitActiveLayerId.has_value())
            {
                const auto key = keyForNode(ModelNodeKind::layer, *explicitActiveLayerId);
                if (findNodeByKey(key, fullRootNodes).has_value())
                    return key;
            }
            return std::nullopt;
        }

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

    void LayerTreePanel::rebuildModel()
    {
        fullRootNodes = buildLayerNodes();

        std::unordered_set<WidgetId> currentLayerIds;
        std::unordered_set<WidgetId> currentGroupIds;
        std::function<void(const std::vector<LayerTreePanel::ModelNode>&)> collectIds = [&](const std::vector<LayerTreePanel::ModelNode>& nodes)
        {
            for (const auto& node : nodes)
            {
                if (node.kind == ModelNodeKind::layer)
                    currentLayerIds.insert(node.id);
                else if (node.kind == ModelNodeKind::group)
                    currentGroupIds.insert(node.id);

                collectIds(node.children);
            }
        };
        collectIds(fullRootNodes);

        for (const auto layerId : currentLayerIds)
        {
            if (knownLayerIds.insert(layerId).second)
                expandedLayerIds.insert(layerId);
        }
        for (const auto groupId : currentGroupIds)
        {
            if (knownGroupIds.insert(groupId).second)
                expandedGroupIds.insert(groupId);
        }

        for (auto it = knownLayerIds.begin(); it != knownLayerIds.end();)
        {
            if (currentLayerIds.count(*it) == 0)
            {
                expandedLayerIds.erase(*it);
                it = knownLayerIds.erase(it);
                continue;
            }
            ++it;
        }
        if (explicitActiveLayerId.has_value() && currentLayerIds.count(*explicitActiveLayerId) == 0)
            explicitActiveLayerId.reset();
        for (auto it = knownGroupIds.begin(); it != knownGroupIds.end();)
        {
            if (currentGroupIds.count(*it) == 0)
            {
                expandedGroupIds.erase(*it);
                it = knownGroupIds.erase(it);
                continue;
            }
            ++it;
        }

        rootNodes = fullRootNodes;
        const auto filterLower = searchBox.getText().trim().toLowerCase();
        if (filterLower.isNotEmpty())
            rootNodes = filterNodes(fullRootNodes, filterLower);

        selectionByKey.clear();
        std::function<void(const std::vector<LayerTreePanel::ModelNode>&)> indexNodes = [&](const std::vector<LayerTreePanel::ModelNode>& nodes)
        {
            for (const auto& node : nodes)
            {
                if (!node.selectionIds.empty())
                    selectionByKey[keyForNode(node.kind, node.id)] = node.selectionIds;
                indexNodes(node.children);
            }
        };
        indexNodes(fullRootNodes);
    }

    void LayerTreePanel::applyDocumentSelectionToTree()
    {
        const auto key = resolveSelectedNodeKey(document.editorState().selection);
        if (lastAppliedSelectionKey == key)
            return;

        if (lastAppliedSelectionKey.has_value())
        {
            if (auto* previousItem = findVisibleTreeItemByKey(*lastAppliedSelectionKey); previousItem != nullptr)
                previousItem->setSelected(false, false);
            repaintTreeRowForKey(*lastAppliedSelectionKey);
        }

        if (key.has_value())
        {
            if (auto* nextItem = findTreeItemByKey(*key); nextItem != nullptr)
            {
                for (auto* parent = nextItem->getParentItem(); parent != nullptr; parent = parent->getParentItem())
                    parent->setOpen(true);
                nextItem->setSelected(true, true);
            }
            repaintTreeRowForKey(*key);
        }

        lastAppliedSelectionKey = key;
    }

    void LayerTreePanel::rebuildTree()
    {
        suppressTreeSelectionCallback = true;
        treeView.setRootItem(nullptr);
        rootItem.reset();
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
                    if (treeItem->node().kind == ModelNodeKind::layer)
                        treeItem->setOpen(expandedLayerIds.count(treeItem->node().id) > 0);
                    if (treeItem->node().kind == ModelNodeKind::group)
                        treeItem->setOpen(expandedGroupIds.count(treeItem->node().id) > 0);
                }

                applyOpen(subItem);
            }
        };
        applyOpen(rootItem.get());

        lastAppliedSelectionKey.reset();
        applyDocumentSelectionToTree();
        suppressTreeSelectionCallback = false;
    }

    void LayerTreePanel::beginDrag(ModelNodeKind kind, WidgetId id, juce::Point<int> treePoint)
    {
        dragCandidate = true;
        dragActive = false;
        dragNodeKind = kind;
        draggedNodeIds = { id };
        dragStartPoint = treePoint;
        resetAutoExpandState();
        dropPreview.reset();
        logLayerTreeDnd("beginDrag kind=" + juce::String(dragKindToken(kind))
                        + " id=" + juce::String(id)
                        + " start=(" + juce::String(treePoint.x) + "," + juce::String(treePoint.y) + ")");
    }

    void LayerTreePanel::updateDrag(juce::Point<int> treePoint)
    {
        if (!dragCandidate)
            return;
        if (!dragActive)
        {
            if ((treePoint - dragStartPoint).getDistanceFromOrigin() < 4)
                return;
            dragActive = true;
            logLayerTreeDnd("updateDrag activated kind=" + juce::String(dragKindToken(dragNodeKind))
                            + " ids=" + widgetIdsToDebugString(draggedNodeIds));
        }
        handleDragMoveAt(treePoint);
    }

    void LayerTreePanel::handleDragMoveAt(juce::Point<int> treePoint)
    {
        perf.dragPreviewUpdateCount += 1;
        perf.dragPreviewUpdateCountSinceRefresh += 1;

        maybeAutoScroll(treePoint);
        const auto previousPreview = dropPreview;
        dropPreview = computeDropPreview(treePoint);
        updateAutoExpand(dropPreview);
        repaintDropPreviewDiff(previousPreview, dropPreview);

        const auto hasChanged = [&]() -> bool
        {
            if (previousPreview.has_value() != dropPreview.has_value())
                return true;
            if (!previousPreview.has_value())
                return false;
            return previousPreview->targetNodeKey != dropPreview->targetNodeKey
                || previousPreview->placement != dropPreview->placement
                || previousPreview->markerBounds != dropPreview->markerBounds;
        }();

        if (hasChanged)
        {
            if (dropPreview.has_value())
            {
                logLayerTreeDnd("preview target=" + dropPreview->targetNodeKey
                                + " placement=" + dropPlacementLabel(dropPreview->placement)
                                + " point=(" + juce::String(treePoint.x) + "," + juce::String(treePoint.y) + ")");
            }
            else
            {
                logLayerTreeDnd("preview cleared point=(" + juce::String(treePoint.x) + "," + juce::String(treePoint.y) + ")");
            }
        }
    }

    void LayerTreePanel::endDrag()
    {
        if (!dragCandidate)
            return;

        logLayerTreeDnd("endDrag active=" + juce::String(dragActive ? "true" : "false")
                        + " ids=" + widgetIdsToDebugString(draggedNodeIds)
                        + " hasPreview=" + juce::String(dropPreview.has_value() ? "true" : "false"));

        const auto previousPreview = dropPreview;

        if (dragActive && dropPreview.has_value() && onDropRequest != nullptr)
        {
            if (auto request = buildDropRequest(*dropPreview); request.has_value())
            {
                logLayerTreeDnd("commitDrop parent=" + parentRefToDebugString(request->parent)
                                + " insertIndex=" + juce::String(request->insertIndex)
                                + " placement=" + dropPlacementLabel(request->placement)
                                + " target=" + nodeRefToDebugString(request->target));
                const auto result = onDropRequest(*request);
                if (result.failed())
                    DBG("[Gyeol] Layer drop rejected: " + result.getErrorMessage());
                else
                    logLayerTreeDnd("commitDrop success");
            }
            else
            {
                logLayerTreeDnd("commitDrop skipped: buildDropRequest returned null");
            }
        }

        dragCandidate = false;
        dragActive = false;
        draggedNodeIds.clear();
        dropPreview.reset();
        resetAutoExpandState();
        repaintDropPreviewDiff(previousPreview, dropPreview);
    }

    void LayerTreePanel::repaintDropPreviewDiff(const std::optional<DropPreview>& previousPreview,
                                                const std::optional<DropPreview>& nextPreview)
    {
        juce::Rectangle<int> dirty;
        auto includePreview = [this, &dirty](const std::optional<DropPreview>& preview)
        {
            if (!preview.has_value())
                return;
            const auto marker = preview->markerBounds.translated(treeView.getX(), treeView.getY()).expanded(3);
            dirty = dirty.isEmpty() ? marker : dirty.getUnion(marker);
        };

        includePreview(previousPreview);
        includePreview(nextPreview);

        if (!dirty.isEmpty())
            repaint(dirty);
    }

    void LayerTreePanel::updateAutoExpand(const std::optional<DropPreview>& preview)
    {
        if (!preview.has_value() || preview->placement != Interaction::LayerDropPlacement::into)
        {
            resetAutoExpandState();
            return;
        }

        const auto& key = preview->targetNodeKey;
        if (key.isEmpty())
        {
            resetAutoExpandState();
            return;
        }

        const auto nowMs = juce::Time::getMillisecondCounter();
        if (dragHoverNodeKey != key)
        {
            dragHoverNodeKey = key;
            dragHoverStartMs = nowMs;
            return;
        }

        if ((nowMs - dragHoverStartMs) < autoExpandDelayMs)
            return;

        if (auto* treeItem = findTreeItemByKey(key); treeItem != nullptr)
        {
            if (!treeItem->isOpen() && treeItem->mightContainSubItems())
            {
                treeItem->setOpen(true);
                dragHoverStartMs = nowMs;
            }
        }
    }

    void LayerTreePanel::resetAutoExpandState()
    {
        dragHoverNodeKey.clear();
        dragHoverStartMs = 0;
    }

    void LayerTreePanel::maybeAutoScroll(juce::Point<int> treePoint)
    {
        juce::Viewport* viewport = nullptr;
        for (int i = 0; i < treeView.getNumChildComponents(); ++i)
        {
            if (auto* childViewport = dynamic_cast<juce::Viewport*>(treeView.getChildComponent(i)); childViewport != nullptr)
            {
                viewport = childViewport;
                break;
            }
        }

        if (viewport == nullptr)
            return;

        int deltaY = 0;
        if (treePoint.y < autoScrollEdgePx)
            deltaY = -autoScrollStepPx;
        else if (treePoint.y > (treeView.getHeight() - autoScrollEdgePx))
            deltaY = autoScrollStepPx;

        if (deltaY == 0)
            return;

        const auto* viewed = viewport->getViewedComponent();
        if (viewed == nullptr)
            return;

        const auto maxY = juce::jmax(0, viewed->getHeight() - viewport->getHeight());
        const auto nextY = juce::jlimit(0, maxY, viewport->getViewPositionY() + deltaY);
        if (nextY != viewport->getViewPositionY())
            viewport->setViewPosition(viewport->getViewPositionX(), nextY);
    }

    void LayerTreePanel::handleTreeSelection(const juce::String& key)
    {
        if (suppressTreeSelectionCallback)
            return;

        const auto node = findNodeByKey(key);
        if (!node.has_value())
            return;

        if (node->kind == ModelNodeKind::layer)
        {
            explicitActiveLayerId = node->id;
            if (onActiveLayerChanged != nullptr)
                onActiveLayerChanged(explicitActiveLayerId);
            return;
        }

        if (explicitActiveLayerId.has_value())
        {
            explicitActiveLayerId.reset();
            if (onActiveLayerChanged != nullptr)
                onActiveLayerChanged(std::nullopt);
        }

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
            if (rowBoundsForItem(*item).contains(treePoint))
            {
                target = item;
                break;
            }
        }
        if (target == nullptr)
        {
            target = *std::min_element(items.begin(), items.end(), [this, treePoint](const TreeItem* lhs, const TreeItem* rhs)
            {
                return std::abs(rowBoundsForItem(*lhs).getCentreY() - treePoint.y)
                    < std::abs(rowBoundsForItem(*rhs).getCentreY() - treePoint.y);
            });
        }
        if (target == nullptr)
            return std::nullopt;

        if (dragNodeKind == ModelNodeKind::layer && target->node().kind != ModelNodeKind::layer)
            return std::nullopt;

        DropPreview preview;
        preview.targetNodeKey = target->getUniqueName();

        const auto row = rowBoundsForItem(*target);
        const auto upperZone = row.getY() + row.getHeight() / 3;
        const auto lowerZone = row.getBottom() - row.getHeight() / 3;
        const auto canDropInto = target->node().kind != ModelNodeKind::widget
                              && dragNodeKind != ModelNodeKind::layer;

        // Non-layer drags should not resolve "before/after" against layer header rows.
        // For layer headers, force a deterministic "into layer" preview.
        if (dragNodeKind != ModelNodeKind::layer && target->node().kind == ModelNodeKind::layer)
        {
            DropPreview forcedLayerIntoPreview;
            forcedLayerIntoPreview.targetNodeKey = target->getUniqueName();
            forcedLayerIntoPreview.placement = Interaction::LayerDropPlacement::into;
            forcedLayerIntoPreview.markerBounds = row.reduced(2, 1);
            if (isDropTargetInDraggedSubtree(forcedLayerIntoPreview.targetNodeKey))
                return std::nullopt;
            return forcedLayerIntoPreview;
        }

        const auto horizontalIntentThreshold = row.getX() + 20;
        const auto intendsInto = treePoint.x >= horizontalIntentThreshold;
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
        else if (canDropInto && intendsInto)
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
        {
            logLayerTreeDnd("buildDropRequest failed: cannot resolve parent/insert for target=" + preview.targetNodeKey);
            return std::nullopt;
        }

        Interaction::LayerTreeDropRequest request;
        request.placement = preview.placement;
        request.parent = parentAndInsert->first;
        request.insertIndex = parentAndInsert->second;

        const auto draggedKind = nodeKindFromModelKind(dragNodeKind);
        for (const auto draggedId : draggedNodeIds)
            request.dragged.push_back({ draggedKind, draggedId });

        if (const auto targetNode = findNodeByKey(preview.targetNodeKey, fullRootNodes); targetNode.has_value())
            request.target = NodeRef { nodeKindFromModelKind(targetNode->kind), targetNode->id };

        logLayerTreeDnd("buildDropRequest ok draggedKind=" + juce::String(dragKindToken(dragNodeKind))
                        + " draggedIds=" + widgetIdsToDebugString(draggedNodeIds)
                        + " target=" + preview.targetNodeKey
                        + " parent=" + parentRefToDebugString(request.parent)
                        + " insertIndex=" + juce::String(request.insertIndex)
                        + " placement=" + dropPlacementLabel(request.placement));

        return request;
    }

    std::optional<std::pair<ParentRef, int>> LayerTreePanel::resolveDropParentAndInsert(const DropPreview& preview) const
    {
        const auto targetNode = findNodeByKey(preview.targetNodeKey, fullRootNodes);
        if (!targetNode.has_value())
            return std::nullopt;

        if (preview.placement == Interaction::LayerDropPlacement::into)
        {
            if (targetNode->kind == ModelNodeKind::widget)
                return std::nullopt;
            ParentRef parent;
            parent.kind = targetNode->kind == ModelNodeKind::layer ? ParentKind::layer : ParentKind::group;
            parent.id = targetNode->id;
            return std::make_pair(parent, -1);
        }

        const auto parent = targetNode->parent;
        const std::vector<LayerTreePanel::ModelNode>* siblings = nullptr;
        std::optional<LayerTreePanel::ModelNode> parentNodeStorage;
        if (parent.kind == ParentKind::root)
        {
            siblings = &fullRootNodes;
        }
        else
        {
            const auto parentKind = parent.kind == ParentKind::layer ? ModelNodeKind::layer : ModelNodeKind::group;
            parentNodeStorage = findNodeByKey(keyForNode(parentKind, parent.id), fullRootNodes);
            if (!parentNodeStorage.has_value())
                return std::nullopt;
            siblings = &parentNodeStorage->children;
        }

        std::unordered_set<WidgetId> draggedSet(draggedNodeIds.begin(), draggedNodeIds.end());
        std::vector<LayerTreePanel::ModelNode> filtered;
        filtered.reserve(siblings->size());
        for (const auto& sibling : *siblings)
        {
            if (sibling.kind == dragNodeKind && draggedSet.count(sibling.id) > 0)
                continue;
            filtered.push_back(sibling);
        }

        const auto targetIt = std::find_if(filtered.begin(), filtered.end(), [&](const LayerTreePanel::ModelNode& node)
        {
            return keyForNode(node.kind, node.id) == preview.targetNodeKey;
        });
        if (targetIt == filtered.end())
            return std::nullopt;

        const auto targetFrontIndex = static_cast<int>(std::distance(filtered.begin(), targetIt));
        const auto siblingCount = static_cast<int>(filtered.size());
        // Reducer insertIndex is back-to-front (0=back, end=front), while Tree UI is front-first.
        const auto targetBackIndex = siblingCount - 1 - targetFrontIndex;
        const auto insertIndex = preview.placement == Interaction::LayerDropPlacement::before
                                     ? targetBackIndex + 1
                                     : targetBackIndex;

        return std::make_pair(parent, insertIndex);
    }

    bool LayerTreePanel::isDropTargetInDraggedSubtree(const juce::String& targetNodeKey) const
    {
        const auto targetNode = findNodeByKey(targetNodeKey, fullRootNodes);
        if (!targetNode.has_value())
            return true;

        if (dragNodeKind == ModelNodeKind::widget)
            return targetNode->kind == ModelNodeKind::widget
                && std::find(draggedNodeIds.begin(), draggedNodeIds.end(), targetNode->id) != draggedNodeIds.end();

        if (dragNodeKind == ModelNodeKind::layer)
            return targetNode->kind == ModelNodeKind::layer
                && std::find(draggedNodeIds.begin(), draggedNodeIds.end(), targetNode->id) != draggedNodeIds.end();

        for (const auto draggedGroupId : draggedNodeIds)
        {
            if (targetNode->kind == ModelNodeKind::group && targetNode->id == draggedGroupId)
                return true;

            auto parent = targetNode->parent;
            while (parent.kind == ParentKind::group)
            {
                if (parent.id == draggedGroupId)
                    return true;

                const auto parentNode = findNodeByKey(keyForNode(ModelNodeKind::group, parent.id), fullRootNodes);
                if (!parentNode.has_value())
                    break;
                parent = parentNode->parent;
            }
        }

        return false;
    }

    juce::Rectangle<int> LayerTreePanel::rowBoundsForItem(const TreeItem& item) const
    {
        // Use the item's own row rect only (exclude expanded subtree height).
        auto row = item.getItemPosition(false);
        row.setHeight(item.getItemHeight());
        return row;
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
                if (treeItem->getItemPosition(false).getHeight() > 0)
                    out.push_back(treeItem);
            }

            // Closed parents must not expose children as visible DnD rows.
            if (subItem->isOpen())
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

    LayerTreePanel::RowIcon LayerTreePanel::iconHitForTreeItem(const TreeItem& item, juce::Point<int> localPoint) const
    {
        if (item.paintWidth() <= 0)
            return RowIcon::none;

        const auto h = juce::jmax(12, item.getItemHeight() - 6);
        const auto visibleBounds = juce::Rectangle<int>(juce::jmax(0, item.paintWidth() - 36), 3, 14, h);
        const auto lockedBounds = juce::Rectangle<int>(juce::jmax(0, item.paintWidth() - 18), 3, 14, h);
        if (visibleBounds.contains(localPoint))
            return RowIcon::visible;
        if (lockedBounds.contains(localPoint))
            return RowIcon::locked;
        return RowIcon::none;
    }

    void LayerTreePanel::toggleNodeIcon(const LayerTreePanel::ModelNode& node, RowIcon icon)
    {
        if (icon == RowIcon::none || onNodePropsChanged == nullptr)
            return;

        const auto nextValue = icon == RowIcon::visible ? !node.visible : !node.locked;
        SetPropsAction action;
        action.kind = nodeKindFromModelKind(node.kind);
        action.ids = { node.id };

        if (action.kind == NodeKind::widget)
        {
            WidgetPropsPatch patch;
            if (icon == RowIcon::visible) patch.visible = nextValue;
            else patch.locked = nextValue;
            action.patch = patch;
        }
        else if (action.kind == NodeKind::group)
        {
            GroupPropsPatch patch;
            if (icon == RowIcon::visible) patch.visible = nextValue;
            else patch.locked = nextValue;
            action.patch = patch;
        }
        else
        {
            LayerPropsPatch patch;
            if (icon == RowIcon::visible) patch.visible = nextValue;
            else patch.locked = nextValue;
            action.patch = patch;
        }

        const auto result = onNodePropsChanged(action);
        if (result.failed())
            DBG("[Gyeol] LayerTreePanel toggle failed: " + result.getErrorMessage());
    }

    void LayerTreePanel::handleCreateLayerButton()
    {
        if (onCreateLayerRequested == nullptr)
            return;

        const auto newLayerId = onCreateLayerRequested();
        if (!newLayerId.has_value() || *newLayerId <= kRootId)
        {
            DBG("[Gyeol] LayerTreePanel create layer failed");
            return;
        }

        explicitActiveLayerId = *newLayerId;
        if (onActiveLayerChanged != nullptr)
            onActiveLayerChanged(explicitActiveLayerId);
    }

    void LayerTreePanel::handleDeleteLayerButton()
    {
        if (onDeleteLayerRequested == nullptr)
            return;

        std::optional<WidgetId> targetLayerId = explicitActiveLayerId;
        if (!targetLayerId.has_value())
        {
            for (const auto& node : fullRootNodes)
            {
                if (node.kind == ModelNodeKind::layer)
                {
                    targetLayerId = node.id;
                    break;
                }
            }
        }

        if (!targetLayerId.has_value())
        {
            DBG("[Gyeol] LayerTreePanel delete layer skipped: no target layer");
            return;
        }

        const auto result = onDeleteLayerRequested(*targetLayerId);
        if (result.failed())
            DBG("[Gyeol] LayerTreePanel delete layer failed: " + result.getErrorMessage());
    }

    bool LayerTreePanel::sameNodeStructure(const std::vector<ModelNode>& lhs, const std::vector<ModelNode>& rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (size_t i = 0; i < lhs.size(); ++i)
        {
            const auto& leftNode = lhs[i];
            const auto& rightNode = rhs[i];
            if (leftNode.kind != rightNode.kind || leftNode.id != rightNode.id)
                return false;
            if (leftNode.parent.kind != rightNode.parent.kind || leftNode.parent.id != rightNode.parent.id)
                return false;
            if (!sameNodeStructure(leftNode.children, rightNode.children))
                return false;
        }

        return true;
    }

    bool LayerTreePanel::sameNodeVisuals(const std::vector<ModelNode>& lhs, const std::vector<ModelNode>& rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (size_t i = 0; i < lhs.size(); ++i)
        {
            const auto& leftNode = lhs[i];
            const auto& rightNode = rhs[i];
            if (leftNode.kind != rightNode.kind || leftNode.id != rightNode.id)
                return false;
            if (leftNode.label != rightNode.label)
                return false;
            if (leftNode.visible != rightNode.visible || leftNode.locked != rightNode.locked)
                return false;
            if (!sameNodeVisuals(leftNode.children, rightNode.children))
                return false;
        }

        return true;
    }

    void LayerTreePanel::collectVisualChangedKeys(const std::vector<ModelNode>& previousNodes,
                                                  const std::vector<ModelNode>& nextNodes,
                                                  std::vector<juce::String>& outChangedKeys) const
    {
        const auto count = std::min(previousNodes.size(), nextNodes.size());
        for (size_t i = 0; i < count; ++i)
        {
            const auto& previousNode = previousNodes[i];
            const auto& nextNode = nextNodes[i];
            if (previousNode.kind != nextNode.kind || previousNode.id != nextNode.id)
                continue;

            if (previousNode.label != nextNode.label
                || previousNode.visible != nextNode.visible
                || previousNode.locked != nextNode.locked)
            {
                outChangedKeys.push_back(keyForNode(nextNode.kind, nextNode.id));
            }

            collectVisualChangedKeys(previousNode.children, nextNode.children, outChangedKeys);
        }
    }

    bool LayerTreePanel::applyModelToExistingTree()
    {
        if (rootItem == nullptr)
            return false;
        if (rootItem->getNumSubItems() != static_cast<int>(rootNodes.size()))
            return false;

        for (int i = 0; i < rootItem->getNumSubItems(); ++i)
        {
            auto* childItem = dynamic_cast<TreeItem*>(rootItem->getSubItem(i));
            if (childItem == nullptr)
                return false;
            if (!childItem->applyModel(rootNodes[static_cast<size_t>(i)]))
                return false;
        }

        return true;
    }

    LayerTreePanel::TreeItem* LayerTreePanel::findTreeItemByKey(const juce::String& key) const
    {
        if (rootItem == nullptr || key.isEmpty())
            return nullptr;

        std::function<TreeItem*(juce::TreeViewItem*)> findRecursive = [&](juce::TreeViewItem* item) -> TreeItem*
        {
            if (item == nullptr)
                return nullptr;

            if (auto* treeItem = dynamic_cast<TreeItem*>(item); treeItem != nullptr)
            {
                if (treeItem->getUniqueName() == key)
                    return treeItem;
            }

            for (int i = 0; i < item->getNumSubItems(); ++i)
            {
                if (auto* match = findRecursive(item->getSubItem(i)); match != nullptr)
                    return match;
            }

            return nullptr;
        };

        return findRecursive(rootItem.get());
    }

    LayerTreePanel::TreeItem* LayerTreePanel::findVisibleTreeItemByKey(const juce::String& key) const
    {
        for (auto* item : visibleTreeItems())
        {
            if (item->getUniqueName() == key)
                return item;
        }

        return nullptr;
    }

    void LayerTreePanel::repaintTreeRowForKey(const juce::String& key)
    {
        if (auto* item = findVisibleTreeItemByKey(key); item != nullptr)
        {
            const auto row = rowBoundsForItem(*item);
            if (row.getWidth() > 0 && row.getHeight() > 0)
                treeView.repaint(row.expanded(2));
        }
    }

    void LayerTreePanel::repaintRowsForKeys(const std::vector<juce::String>& keys)
    {
        std::unordered_set<juce::String> deduped;
        deduped.reserve(keys.size());
        for (const auto& key : keys)
        {
            if (deduped.insert(key).second)
                repaintTreeRowForKey(key);
        }
    }

    int LayerTreePanel::countModelNodes(const std::vector<ModelNode>& nodes)
    {
        int count = 0;
        for (const auto& node : nodes)
            count += 1 + countModelNodes(node.children);
        return count;
    }

    int LayerTreePanel::countTreeItems(const juce::TreeViewItem* item)
    {
        if (item == nullptr)
            return 0;

        int count = 0;
        for (int i = 0; i < item->getNumSubItems(); ++i)
            count += 1 + countTreeItems(item->getSubItem(i));
        return count;
    }

    const char* LayerTreePanel::refreshReasonToString(RefreshReason reason) noexcept
    {
        switch (reason)
        {
            case RefreshReason::external: return "external";
            case RefreshReason::initial: return "initial";
            case RefreshReason::searchChanged: return "search"; 
        }
        return "unknown";
    }

    NodeKind LayerTreePanel::nodeKindFromModelKind(ModelNodeKind kind)
    {
        if (kind == ModelNodeKind::layer)
            return NodeKind::layer;
        if (kind == ModelNodeKind::group)
            return NodeKind::group;
        return NodeKind::widget;
    }
}
