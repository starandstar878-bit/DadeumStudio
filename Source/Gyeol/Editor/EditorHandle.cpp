#include "Gyeol/Public/EditorHandle.h"

#include "Gyeol/Editor/Interaction/LayerOrderEngine.h"
#include "Gyeol/Editor/Panels/LayerTreePanel.h"
#include "Gyeol/Export/JuceComponentExport.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include "Gyeol/Serialization/DocumentJson.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Gyeol::Ui
{
    inline constexpr float kResizeHandleSize = 10.0f;
    inline constexpr float kBoundsEpsilon = 0.001f;

    inline bool containsWidgetId(const std::vector<WidgetId>& ids, WidgetId id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    inline bool areClose(float lhs, float rhs) noexcept
    {
        return std::abs(lhs - rhs) <= kBoundsEpsilon;
    }

    inline bool areRectsEqual(const juce::Rectangle<float>& lhs, const juce::Rectangle<float>& rhs) noexcept
    {
        return areClose(lhs.getX(), rhs.getX())
            && areClose(lhs.getY(), rhs.getY())
            && areClose(lhs.getWidth(), rhs.getWidth())
            && areClose(lhs.getHeight(), rhs.getHeight());
    }

    inline juce::Rectangle<float> unionRect(const juce::Rectangle<float>& lhs, const juce::Rectangle<float>& rhs) noexcept
    {
        const auto left = std::min(lhs.getX(), rhs.getX());
        const auto top = std::min(lhs.getY(), rhs.getY());
        const auto right = std::max(lhs.getRight(), rhs.getRight());
        const auto bottom = std::max(lhs.getBottom(), rhs.getBottom());
        return { left, top, right - left, bottom - top };
    }

    inline juce::Rectangle<float> makeNormalizedRect(juce::Point<float> a, juce::Point<float> b) noexcept
    {
        const auto left = std::min(a.x, b.x);
        const auto top = std::min(a.y, b.y);
        const auto right = std::max(a.x, b.x);
        const auto bottom = std::max(a.y, b.y);
        return { left, top, right - left, bottom - top };
    }

    inline void drawDashedRect(juce::Graphics& g,
                               const juce::Rectangle<float>& rect,
                               float dashLength = 5.0f,
                               float gapLength = 3.0f,
                               float thickness = 1.0f)
    {
        const float dashPattern[] { dashLength, gapLength };
        g.drawDashedLine(juce::Line<float>(rect.getX(), rect.getY(), rect.getRight(), rect.getY()),
                         dashPattern,
                         2,
                         thickness);
        g.drawDashedLine(juce::Line<float>(rect.getRight(), rect.getY(), rect.getRight(), rect.getBottom()),
                         dashPattern,
                         2,
                         thickness);
        g.drawDashedLine(juce::Line<float>(rect.getRight(), rect.getBottom(), rect.getX(), rect.getBottom()),
                         dashPattern,
                         2,
                         thickness);
        g.drawDashedLine(juce::Line<float>(rect.getX(), rect.getBottom(), rect.getX(), rect.getY()),
                         dashPattern,
                         2,
                         thickness);
    }

    inline void warnUnsupportedWidgetOnce(WidgetType type, const char* context)
    {
        static std::vector<juce::String> warnedKeys;
        const auto key = juce::String(context) + ":" + juce::String(static_cast<int>(type));
        if (std::find(warnedKeys.begin(), warnedKeys.end(), key) != warnedKeys.end())
            return;

        warnedKeys.push_back(key);
        DBG("[Gyeol] Unsupported widget fallback in " + juce::String(context)
            + " (type ordinal=" + juce::String(static_cast<int>(type)) + ")");
    }

    class CanvasRenderer
    {
    public:
        explicit CanvasRenderer(const Widgets::WidgetFactory& widgetFactoryIn) noexcept
            : widgetFactory(widgetFactoryIn)
        {
        }

        void paintCanvas(juce::Graphics& g, juce::Rectangle<int> bounds) const
        {
            g.fillAll(juce::Colour::fromRGB(18, 20, 25));

            g.setColour(juce::Colour::fromRGBA(255, 255, 255, 12));
            constexpr int kMajorGrid = 48;
            constexpr int kMinorGrid = 12;

            for (int x = bounds.getX(); x < bounds.getRight(); x += kMinorGrid)
                g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));

            for (int y = bounds.getY(); y < bounds.getBottom(); y += kMinorGrid)
                g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));

            g.setColour(juce::Colour::fromRGBA(255, 255, 255, 24));
            for (int x = bounds.getX(); x < bounds.getRight(); x += kMajorGrid)
                g.drawVerticalLine(x, static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));

            for (int y = bounds.getY(); y < bounds.getBottom(); y += kMajorGrid)
                g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
        }

        juce::Rectangle<float> resizeHandleBounds(const juce::Rectangle<float>& localBounds) const noexcept
        {
            const auto handleSize = std::min({ kResizeHandleSize, localBounds.getWidth(), localBounds.getHeight() });
            return { localBounds.getRight() - handleSize - 1.0f,
                     localBounds.getBottom() - handleSize - 1.0f,
                     handleSize,
                     handleSize };
        }

        bool hitResizeHandle(const juce::Rectangle<float>& localBounds, juce::Point<float> point) const noexcept
        {
            return resizeHandleBounds(localBounds).contains(point);
        }

        void paintWidget(juce::Graphics& g,
                         const WidgetModel& widget,
                         const juce::Rectangle<float>& localBounds,
                         bool selected,
                         bool showResizeHandle,
                         bool resizeHandleHot) const
        {
            const auto body = localBounds.reduced(1.0f);
            const auto outline = selected ? juce::Colour::fromRGB(78, 156, 255)
                                          : juce::Colour::fromRGB(95, 101, 114);

            if (const auto* descriptor = widgetFactory.descriptorFor(widget.type);
                descriptor != nullptr && static_cast<bool>(descriptor->painter))
            {
                descriptor->painter(g, widget, body);
            }
            else
            {
                warnUnsupportedWidgetOnce(widget.type, "CanvasRenderer::paintWidget");
                g.setColour(juce::Colour::fromRGB(44, 49, 60));
                g.fillRoundedRectangle(body, 4.0f);
                g.setColour(juce::Colour::fromRGB(228, 110, 110));
                g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                g.drawFittedText("Unsupported", body.reduced(4.0f).toNearestInt(), juce::Justification::centred, 1);
            }

            g.setColour(outline);
            g.drawRoundedRectangle(body, 5.0f, selected ? 2.0f : 1.0f);

            if (selected && showResizeHandle)
            {
                const auto handle = resizeHandleBounds(localBounds);
                g.setColour(resizeHandleHot ? outline.brighter(0.2f) : outline);
                g.fillRoundedRectangle(handle, 2.0f);
            }
        }

        void paintGroupBadge(juce::Graphics& g,
                             const juce::Rectangle<float>& localBounds,
                             bool selected,
                             bool groupedInActiveEdit) const
        {
            if (localBounds.getWidth() < 28.0f || localBounds.getHeight() < 20.0f)
                return;

            const auto badgeBounds = juce::Rectangle<float>(4.0f, 4.0f, 16.0f, 12.0f);
            const auto badgeFill = groupedInActiveEdit ? juce::Colour::fromRGB(255, 196, 112)
                                                       : juce::Colour::fromRGB(120, 170, 235);
            const auto badgeStroke = selected ? juce::Colour::fromRGB(78, 156, 255)
                                              : juce::Colour::fromRGB(56, 72, 96);

            g.setColour(badgeFill.withAlpha(0.92f));
            g.fillRoundedRectangle(badgeBounds, 3.0f);
            g.setColour(badgeStroke.withAlpha(0.95f));
            g.drawRoundedRectangle(badgeBounds, 3.0f, 1.0f);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawFittedText("G", badgeBounds.toNearestInt(), juce::Justification::centred, 1);
        }

    private:
        const Widgets::WidgetFactory& widgetFactory;
    };

    class CanvasComponent;

    class WidgetComponent : public juce::Component
    {
    public:
        WidgetComponent(CanvasComponent& ownerIn,
                        const CanvasRenderer& rendererIn,
                        const WidgetModel& widgetIn,
                        bool isSelected,
                        bool showResizeHandleIn,
                        bool isGroupedIn,
                        bool groupedInActiveEditIn)
            : owner(ownerIn),
              renderer(rendererIn)
        {
            updateFromModel(widgetIn,
                            isSelected,
                            showResizeHandleIn,
                            isGroupedIn,
                            groupedInActiveEditIn);
            setRepaintsOnMouseActivity(true);
        }

        void updateFromModel(const WidgetModel& widgetIn,
                             bool isSelected,
                             bool showResizeHandleIn,
                             bool isGroupedIn,
                             bool groupedInActiveEditIn)
        {
            widget = widgetIn;
            resizeHandleHot = false;
            setSelected(isSelected);
            showResizeHandle = showResizeHandleIn;
            grouped = isGroupedIn;
            groupedInActiveEdit = groupedInActiveEditIn;
            setViewBounds(widget.bounds);
        }

        void setSelected(bool isSelected)
        {
            if (selected == isSelected)
                return;

            selected = isSelected;
            repaint();
        }

        void setSelectionVisual(bool isSelected,
                                bool showResizeHandleIn,
                                bool isGroupedIn,
                                bool groupedInActiveEditIn)
        {
            if (selected == isSelected
                && showResizeHandle == showResizeHandleIn
                && grouped == isGroupedIn
                && groupedInActiveEdit == groupedInActiveEditIn)
            return;

            selected = isSelected;
            showResizeHandle = showResizeHandleIn;
            grouped = isGroupedIn;
            groupedInActiveEdit = groupedInActiveEditIn;
            if (!showResizeHandle)
                resizeHandleHot = false;
            repaint();
        }

        void setViewBounds(const juce::Rectangle<float>& bounds)
        {
            setBounds(bounds.getSmallestIntegerContainer());
            repaint();
        }

        WidgetId widgetId() const noexcept { return widget.id; }
        bool isResizeHandleHit(juce::Point<float> localPoint) const noexcept
        {
            return selected
                && showResizeHandle
                && renderer.hitResizeHandle(getLocalBounds().toFloat(), localPoint);
        }

        void paint(juce::Graphics& g) override
        {
            renderer.paintWidget(g,
                                widget,
                                getLocalBounds().toFloat(),
                                selected,
                                showResizeHandle,
                                resizeHandleHot);

            if (grouped)
                renderer.paintGroupBadge(g, getLocalBounds().toFloat(), selected, groupedInActiveEdit);
        }

        void mouseMove(const juce::MouseEvent& event) override
        {
            const auto hot = isResizeHandleHit(event.position);
            if (hot != resizeHandleHot)
            {
                resizeHandleHot = hot;
                repaint();
            }
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            if (resizeHandleHot)
            {
                resizeHandleHot = false;
                repaint();
            }
        }

        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        CanvasComponent& owner;
        const CanvasRenderer& renderer;
        WidgetModel widget;
        bool selected = false;
        bool showResizeHandle = false;
        bool resizeHandleHot = false;
        bool grouped = false;
        bool groupedInActiveEdit = false;
    };

    class CanvasComponent : public juce::Component
    {
    public:
        CanvasComponent(DocumentHandle& documentIn, const Widgets::WidgetFactory& widgetFactoryIn)
            : document(documentIn),
              widgetFactory(widgetFactoryIn),
              renderer(widgetFactoryIn)
        {
            setWantsKeyboardFocus(true);
            refreshFromDocument();
        }

        void setStateChangedCallback(std::function<void()> callback)
        {
            onStateChanged = std::move(callback);
        }

        WidgetId createWidget(WidgetType type)
        {
            const auto newWidgetId = widgetFactory.createWidget(document, type, createDefaultOrigin());
            if (newWidgetId <= kRootId)
                return 0;

            document.selectSingle(newWidgetId);
            refreshFromDocument();
            grabKeyboardFocus();
            return newWidgetId;
        }

        bool deleteSelection()
        {
            const auto selection = document.editorState().selection;
            if (selection.empty())
                return false;

            bool changed = false;
            for (const auto id : selection)
                changed = document.removeWidget(id) || changed;

            if (changed)
            {
                refreshFromDocument();
                grabKeyboardFocus();
            }

            return changed;
        }

        bool performUndo()
        {
            if (!document.undo())
                return false;

            refreshFromDocument();
            grabKeyboardFocus();
            return true;
        }

        bool performRedo()
        {
            if (!document.redo())
                return false;

            refreshFromDocument();
            grabKeyboardFocus();
            return true;
        }

        bool groupSelection()
        {
            if (!document.groupSelection())
                return false;

            activeGroupEditId.reset();
            refreshFromDocument();
            grabKeyboardFocus();
            return true;
        }

        bool ungroupSelection()
        {
            if (!document.ungroupSelection())
                return false;

            activeGroupEditId.reset();
            refreshFromDocument();
            grabKeyboardFocus();
            return true;
        }

        bool canGroupSelection() const noexcept
        {
            return document.editorState().selection.size() >= 2;
        }

        bool canUngroupSelection() const noexcept
        {
            const auto& selection = document.editorState().selection;
            if (selection.empty())
                return false;

            for (const auto id : selection)
            {
                if (findGroupByMember(id) != nullptr)
                    return true;
            }

            return false;
        }

        bool enterGroupEditMode()
        {
            const auto groupId = selectedWholeGroupId();
            if (!groupId.has_value())
                return false;

            activeGroupEditId = *groupId;
            repaint();
            notifyStateChanged();
            return true;
        }

        bool exitGroupEditMode(bool restoreWholeGroupSelection)
        {
            if (!activeGroupEditId.has_value())
                return false;

            const auto previousGroupId = *activeGroupEditId;
            activeGroupEditId.reset();

            if (restoreWholeGroupSelection)
            {
                if (const auto* group = findGroupById(previousGroupId))
                    document.setSelection(group->memberWidgetIds);
            }

            syncSelectionToViews();
            return true;
        }

        void refreshFromDocument()
        {
            dragState = {};
            marqueeState = {};

            if (activeGroupEditId.has_value() && findGroupById(*activeGroupEditId) == nullptr)
                activeGroupEditId.reset();

            widgetViews.clear();
            widgetViews.reserve(document.snapshot().widgets.size());

            const auto& selection = document.editorState().selection;
            const auto showWidgetHandles = selection.size() == 1;
            for (const auto& widget : document.snapshot().widgets)
            {
                const auto isSelected = containsWidgetId(selection, widget.id);
                const auto* group = findGroupByMember(widget.id);
                const auto isGrouped = group != nullptr;
                const auto groupedInActiveEdit = isGrouped
                                              && activeGroupEditId.has_value()
                                              && isWidgetInGroup(widget.id, *activeGroupEditId);
                auto view = std::make_unique<WidgetComponent>(*this,
                                                              renderer,
                                                              widget,
                                                              isSelected,
                                                              isSelected && showWidgetHandles,
                                                              isGrouped,
                                                              groupedInActiveEdit);
                addAndMakeVisible(*view);
                widgetViews.push_back(std::move(view));
            }

            repaint();
            notifyStateChanged();
        }

        void paint(juce::Graphics& g) override
        {
            renderer.paintCanvas(g, getLocalBounds());
        }

        void paintOverChildren(juce::Graphics& g) override
        {
            if (marqueeState.active)
            {
                const auto rect = marqueeState.bounds.toNearestInt();
                g.setColour(juce::Colour::fromRGBA(78, 156, 255, 34));
                g.fillRect(rect);
                g.setColour(juce::Colour::fromRGBA(78, 156, 255, 200));
                g.drawRect(rect, 1);
            }

            paintGroupOverlays(g);

            juce::Rectangle<float> selectionBounds;
            if (!computeCurrentSelectionUnionBounds(selectionBounds))
                return;

            const auto outline = juce::Colour::fromRGB(78, 156, 255);
            g.setColour(outline);
            g.drawRoundedRectangle(selectionBounds.reduced(0.5f), 5.0f, 1.5f);

            const auto handle = selectionResizeHandleBounds(selectionBounds);
            g.setColour(outline);
            g.fillRoundedRectangle(handle, 2.0f);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (!event.mods.isLeftButtonDown())
                return;

            grabKeyboardFocus();

            if (modifiersAllowResizeDrag(event.mods) && isMultiSelectionResizeHandleHit(event.position))
            {
                const auto& selection = document.editorState().selection;
                if (!selection.empty())
                {
                    marqueeState = {};
                    beginDragForSelection(selection.front(), DragMode::resize, event.position);
                    return;
                }
            }

            marqueeState.active = true;
            marqueeState.additive = event.mods.isShiftDown();
            marqueeState.toggle = event.mods.isCommandDown();
            marqueeState.startMouse = event.position;
            marqueeState.bounds = makeNormalizedRect(marqueeState.startMouse, marqueeState.startMouse);

            if (!marqueeState.additive && !marqueeState.toggle)
            {
                activeGroupEditId.reset();
                document.clearSelection();
                syncSelectionToViews();
            }
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (dragState.active)
            {
                handleWidgetMouseDrag(dragState.anchorWidgetId, event);
                return;
            }

            if (!marqueeState.active)
                return;

            marqueeState.bounds = makeNormalizedRect(marqueeState.startMouse, event.position);
            repaint();
        }

        void mouseUp(const juce::MouseEvent& event) override
        {
            if (dragState.active)
            {
                handleWidgetMouseUp(dragState.anchorWidgetId);
                return;
            }

            if (!marqueeState.active)
                return;

            marqueeState.bounds = makeNormalizedRect(marqueeState.startMouse, event.position);
            applyMarqueeSelection();
            marqueeState = {};

            if (normalizeSelectionAfterAltReleasePending && !altPreviewEnabled)
            {
                normalizeSelectionAfterAltReleasePending = false;
                normalizeSelectionForCurrentModifierState();
            }

            repaint();
        }

        bool keyStateChanged(bool isKeyDown) override
        {
            juce::ignoreUnused(isKeyDown);
            refreshAltPreviewState();
            return juce::Component::keyStateChanged(isKeyDown);
        }

        void modifierKeysChanged(const juce::ModifierKeys&) override
        {
            refreshAltPreviewState();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            const auto mods = key.getModifiers();
            const auto keyCode = key.getKeyCode();
            const auto isZ = keyCode == 'z' || keyCode == 'Z';
            const auto isY = keyCode == 'y' || keyCode == 'Y';
            const auto isG = keyCode == 'g' || keyCode == 'G';

            if (mods.isCommandDown() && isZ)
            {
                if (mods.isShiftDown())
                    return performRedo();
                return performUndo();
            }

            if (mods.isCommandDown() && isY)
                return performRedo();

            if (mods.isCommandDown() && isG)
            {
                if (mods.isShiftDown())
                    return ungroupSelection();
                return groupSelection();
            }

            if (!mods.isAnyModifierKeyDown() && keyCode == juce::KeyPress::returnKey)
                return enterGroupEditMode();

            if (!mods.isAnyModifierKeyDown() && keyCode == juce::KeyPress::escapeKey)
                return exitGroupEditMode(true);

            if (!mods.isCommandDown())
            {
                const auto step = mods.isShiftDown() ? 10.0f : 1.0f;
                juce::Point<float> delta;

                if (keyCode == juce::KeyPress::leftKey)
                    delta.x = -step;
                else if (keyCode == juce::KeyPress::rightKey)
                    delta.x = step;
                else if (keyCode == juce::KeyPress::upKey)
                    delta.y = -step;
                else if (keyCode == juce::KeyPress::downKey)
                    delta.y = step;

                if (!areClose(delta.x, 0.0f) || !areClose(delta.y, 0.0f))
                    return nudgeSelection(delta);
            }

            if (!mods.isAnyModifierKeyDown()
                && (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey))
            {
                return deleteSelection();
            }

            return false;
        }

    private:
        enum class DragMode
        {
            move,
            resize
        };

        struct DragItemState
        {
            WidgetId widgetId = 0;
            WidgetType widgetType = WidgetType::button;
            juce::Point<float> minSize { 1.0f, 1.0f };
            juce::Rectangle<float> startBounds;
            juce::Rectangle<float> currentBounds;
        };

        struct DragState
        {
            bool active = false;
            WidgetId anchorWidgetId = 0;
            DragMode mode = DragMode::move;
            juce::Point<float> startMouse;
            juce::Rectangle<float> startSelectionBounds;
            float minScaleX = 0.0f;
            float minScaleY = 0.0f;
            std::vector<DragItemState> items;
        };

        struct MarqueeState
        {
            bool active = false;
            bool additive = false;
            bool toggle = false;
            juce::Point<float> startMouse;
            juce::Rectangle<float> bounds;
        };

        juce::Point<float> createDefaultOrigin() const noexcept
        {
            const auto index = static_cast<int>(document.snapshot().widgets.size());
            const auto x = 24.0f + static_cast<float>((index % 10) * 20);
            const auto y = 24.0f + static_cast<float>(((index / 10) % 6) * 20);
            return { x, y };
        }

        WidgetComponent* findWidgetView(WidgetId id) noexcept
        {
            const auto it = std::find_if(widgetViews.begin(),
                                         widgetViews.end(),
                                         [id](const auto& view)
                                         {
                                             return view->widgetId() == id;
                                         });
            return it == widgetViews.end() ? nullptr : it->get();
        }

        const WidgetComponent* findWidgetView(WidgetId id) const noexcept
        {
            const auto it = std::find_if(widgetViews.begin(),
                                         widgetViews.end(),
                                         [id](const auto& view)
                                         {
                                             return view->widgetId() == id;
                                         });
            return it == widgetViews.end() ? nullptr : it->get();
        }

        const WidgetModel* findWidgetModel(WidgetId id) const noexcept
        {
            const auto& widgets = document.snapshot().widgets;
            const auto it = std::find_if(widgets.begin(),
                                         widgets.end(),
                                         [id](const WidgetModel& widget)
                                         {
                                             return widget.id == id;
                                         });
            return it == widgets.end() ? nullptr : &(*it);
        }

        const GroupModel* findGroupById(WidgetId id) const noexcept
        {
            const auto& groups = document.snapshot().groups;
            const auto it = std::find_if(groups.begin(),
                                         groups.end(),
                                         [id](const GroupModel& group)
                                         {
                                             return group.id == id;
                                         });
            return it == groups.end() ? nullptr : &(*it);
        }

        const GroupModel* findGroupByMember(WidgetId memberId) const noexcept
        {
            const auto& groups = document.snapshot().groups;
            const auto it = std::find_if(groups.begin(),
                                         groups.end(),
                                         [memberId](const GroupModel& group)
                                         {
                                             return containsWidgetId(group.memberWidgetIds, memberId);
                                         });
            return it == groups.end() ? nullptr : &(*it);
        }

        std::vector<WidgetId> childGroupIds(WidgetId parentGroupId) const
        {
            std::vector<WidgetId> children;
            for (const auto& group : document.snapshot().groups)
            {
                if (group.parentGroupId.has_value() && *group.parentGroupId == parentGroupId)
                    children.push_back(group.id);
            }

            return children;
        }

        void collectGroupWidgetIdsRecursive(WidgetId groupId,
                                            std::vector<WidgetId>& outWidgetIds,
                                            std::unordered_set<WidgetId>& visitedGroupIds) const
        {
            if (!visitedGroupIds.insert(groupId).second)
                return;

            const auto* group = findGroupById(groupId);
            if (group == nullptr)
                return;

            for (const auto widgetId : group->memberWidgetIds)
            {
                if (!containsWidgetId(outWidgetIds, widgetId))
                    outWidgetIds.push_back(widgetId);
            }

            for (const auto childGroupId : childGroupIds(groupId))
                collectGroupWidgetIdsRecursive(childGroupId, outWidgetIds, visitedGroupIds);
        }

        std::vector<WidgetId> collectGroupWidgetIdsRecursive(WidgetId groupId) const
        {
            std::vector<WidgetId> widgetIds;
            std::unordered_set<WidgetId> visitedGroupIds;
            collectGroupWidgetIdsRecursive(groupId, widgetIds, visitedGroupIds);
            return widgetIds;
        }

        bool isWidgetInGroup(WidgetId memberId, WidgetId groupId) const noexcept
        {
            const auto groupWidgets = collectGroupWidgetIdsRecursive(groupId);
            return containsWidgetId(groupWidgets, memberId);
        }

        bool selectionEqualsGroup(const std::vector<WidgetId>& selection,
                                  const GroupModel& group) const noexcept
        {
            const auto groupWidgetIds = collectGroupWidgetIdsRecursive(group.id);
            if (selection.size() != groupWidgetIds.size())
                return false;

            auto selectionSorted = selection;
            auto groupSorted = groupWidgetIds;
            std::sort(selectionSorted.begin(), selectionSorted.end());
            std::sort(groupSorted.begin(), groupSorted.end());
            return selectionSorted == groupSorted;
        }

        std::vector<WidgetId> groupAncestryForWidget(WidgetId widgetId) const
        {
            std::vector<WidgetId> ancestry;
            const auto* group = findGroupByMember(widgetId);
            if (group == nullptr)
                return ancestry;

            std::unordered_set<WidgetId> visited;
            while (group != nullptr && visited.insert(group->id).second)
            {
                ancestry.push_back(group->id); // leaf -> ... -> top-level
                if (!group->parentGroupId.has_value())
                    break;

                group = findGroupById(*group->parentGroupId);
            }

            return ancestry;
        }

        std::optional<WidgetId> topLevelGroupForWidget(WidgetId widgetId) const
        {
            const auto ancestry = groupAncestryForWidget(widgetId);
            if (ancestry.empty())
                return std::nullopt;

            return ancestry.back();
        }

        std::optional<WidgetId> altSelectableGroupForWidget(WidgetId widgetId) const
        {
            const auto ancestry = groupAncestryForWidget(widgetId);
            if (ancestry.empty())
                return std::nullopt;
            if (ancestry.size() == 1)
                return std::nullopt;

            // Alt preview exposes only one level below top-level.
            return ancestry[ancestry.size() - 2];
        }

        std::vector<WidgetId> altSelectionUnitForWidget(WidgetId id) const
        {
            if (activeGroupEditId.has_value() && isWidgetInGroup(id, *activeGroupEditId))
                return { id };

            if (const auto groupId = altSelectableGroupForWidget(id); groupId.has_value())
                return collectGroupWidgetIdsRecursive(*groupId);

            // Widgets directly under a top-level group stay selectable as single widgets in Alt mode.
            return { id };
        }

        bool modifiersAllowResizeDrag(const juce::ModifierKeys& mods) const noexcept
        {
            return !mods.isCommandDown() && !mods.isShiftDown();
        }

        void paintSingleGroupOverlay(juce::Graphics& g,
                                     const GroupModel& group,
                                     const std::vector<WidgetId>& selection,
                                     float alphaScale = 1.0f) const
        {
            const auto isActiveEdit = activeGroupEditId.has_value() && *activeGroupEditId == group.id;

            juce::Rectangle<float> bounds;
            if (!computeGroupBounds(group.id, bounds, true))
                return;

            const auto boundsInset = bounds.reduced(0.5f);
            const auto isWholeSelected = selectionEqualsGroup(selection, group);
            const auto overlayAlpha = juce::jlimit(0.0f, 1.0f, alphaScale);

            if (isActiveEdit)
            {
                g.setColour(juce::Colour::fromRGB(255, 196, 112)
                                .withAlpha(0.12f)
                                .withMultipliedAlpha(overlayAlpha));
                g.fillRoundedRectangle(boundsInset, 4.0f);
            }

            const auto outline = isActiveEdit ? juce::Colour::fromRGB(255, 196, 112)
                                              : (isWholeSelected ? juce::Colour::fromRGB(78, 156, 255)
                                                                 : juce::Colour::fromRGBA(150, 190, 235, 145));
            g.setColour(outline.withMultipliedAlpha(overlayAlpha));
            drawDashedRect(g, boundsInset, isActiveEdit ? 6.0f : 4.0f, 3.0f, isActiveEdit ? 1.6f : 1.1f);
        }

        std::optional<WidgetId> selectedWholeGroupId() const noexcept
        {
            const auto& selection = document.editorState().selection;
            if (selection.size() < 2)
                return std::nullopt;

            const auto& groups = document.snapshot().groups;
            for (const auto& group : groups)
            {
                if (selectionEqualsGroup(selection, group))
                    return group.id;
            }

            return std::nullopt;
        }

        bool computeGroupBounds(WidgetId groupId,
                                juce::Rectangle<float>& boundsOut,
                                bool useViewBounds) const noexcept
        {
            const auto widgetIds = collectGroupWidgetIdsRecursive(groupId);
            if (widgetIds.empty())
                return false;

            bool hasBounds = false;
            for (const auto memberId : widgetIds)
            {
                juce::Rectangle<float> memberBounds;
                bool hasMemberBounds = false;

                if (useViewBounds)
                {
                    if (const auto* view = findWidgetView(memberId); view != nullptr)
                    {
                        memberBounds = view->getBounds().toFloat();
                        hasMemberBounds = true;
                    }
                }

                if (!hasMemberBounds)
                {
                    if (const auto* widget = findWidgetModel(memberId); widget != nullptr)
                    {
                        memberBounds = widget->bounds;
                        hasMemberBounds = true;
                    }
                }

                if (!hasMemberBounds)
                    continue;

                if (!hasBounds)
                {
                    boundsOut = memberBounds;
                    hasBounds = true;
                }
                else
                {
                    boundsOut = unionRect(boundsOut, memberBounds);
                }
            }

            return hasBounds;
        }

        void paintGroupOverlays(juce::Graphics& g) const
        {
            const auto& groups = document.snapshot().groups;
            const auto& selection = document.editorState().selection;

            std::vector<const GroupModel*> topLevelGroups;
            topLevelGroups.reserve(groups.size());

            for (const auto& group : groups)
            {
                if (!group.parentGroupId.has_value())
                {
                    topLevelGroups.push_back(&group);
                    paintSingleGroupOverlay(g,
                                            group,
                                            selection,
                                            altPreviewEnabled ? 0.55f : 1.0f);
                }
            }

            if (!altPreviewEnabled)
                return;

            for (const auto* topLevelGroup : topLevelGroups)
            {
                if (topLevelGroup == nullptr)
                    continue;

                for (const auto childGroupId : childGroupIds(topLevelGroup->id))
                {
                    if (const auto* childGroup = findGroupById(childGroupId); childGroup != nullptr)
                        paintSingleGroupOverlay(g, *childGroup, selection);
                }
            }
        }

        void addUniqueSelectionIds(std::vector<WidgetId>& target, const std::vector<WidgetId>& ids) const
        {
            for (const auto id : ids)
            {
                if (!containsWidgetId(target, id))
                    target.push_back(id);
            }
        }

        bool selectionSetsEqual(const std::vector<WidgetId>& lhs, const std::vector<WidgetId>& rhs) const
        {
            if (lhs.size() != rhs.size())
                return false;

            auto lhsSorted = lhs;
            auto rhsSorted = rhs;
            std::sort(lhsSorted.begin(), lhsSorted.end());
            std::sort(rhsSorted.begin(), rhsSorted.end());
            return lhsSorted == rhsSorted;
        }

        void normalizeSelectionForCurrentModifierState()
        {
            if (altPreviewEnabled || activeGroupEditId.has_value())
                return;

            const auto& currentSelection = document.editorState().selection;
            if (currentSelection.empty())
                return;

            auto normalizedSelection = expandToSelectionUnits(currentSelection);
            if (selectionSetsEqual(currentSelection, normalizedSelection))
                return;

            document.setSelection(std::move(normalizedSelection));
            syncSelectionToViews();
        }

        void removeSelectionIds(std::vector<WidgetId>& target, const std::vector<WidgetId>& ids) const
        {
            target.erase(std::remove_if(target.begin(),
                                        target.end(),
                                        [&ids](WidgetId selectedId)
                                        {
                                            return containsWidgetId(ids, selectedId);
                                        }),
                         target.end());
        }

        std::vector<WidgetId> selectionUnitForWidget(WidgetId id) const
        {
            if (activeGroupEditId.has_value() && isWidgetInGroup(id, *activeGroupEditId))
                return { id };

            if (const auto groupId = topLevelGroupForWidget(id); groupId.has_value())
                return collectGroupWidgetIdsRecursive(*groupId);

            return { id };
        }

        std::vector<WidgetId> expandToSelectionUnits(const std::vector<WidgetId>& ids) const
        {
            std::vector<WidgetId> expanded;
            expanded.reserve(ids.size());
            for (const auto id : ids)
            {
                const auto unit = selectionUnitForWidget(id);
                addUniqueSelectionIds(expanded, unit);
            }

            return expanded;
        }

        bool computeCurrentSelectionUnionBounds(juce::Rectangle<float>& boundsOut) const noexcept
        {
            const auto& selection = document.editorState().selection;
            if (selection.size() <= 1)
                return false;

            bool hasBounds = false;
            for (const auto id : selection)
            {
                const auto* view = findWidgetView(id);
                if (view == nullptr)
                    continue;

                const auto viewBounds = view->getBounds().toFloat();
                if (!hasBounds)
                {
                    boundsOut = viewBounds;
                    hasBounds = true;
                }
                else
                {
                    boundsOut = unionRect(boundsOut, viewBounds);
                }
            }

            return hasBounds;
        }

        juce::Rectangle<float> selectionResizeHandleBounds(const juce::Rectangle<float>& selectionBounds) const noexcept
        {
            const auto handleSize = std::min({ kResizeHandleSize, selectionBounds.getWidth(), selectionBounds.getHeight() });
            return { selectionBounds.getRight() - handleSize - 1.0f,
                     selectionBounds.getBottom() - handleSize - 1.0f,
                     handleSize,
                     handleSize };
        }

        bool isMultiSelectionResizeHandleHit(juce::Point<float> canvasPoint) const noexcept
        {
            juce::Rectangle<float> selectionBounds;
            if (!computeCurrentSelectionUnionBounds(selectionBounds))
                return false;

            return selectionResizeHandleBounds(selectionBounds).contains(canvasPoint);
        }

        bool beginDragForSelection(WidgetId anchorId, DragMode mode, juce::Point<float> startMouse)
        {
            auto dragIds = document.editorState().selection;
            if (dragIds.empty() && anchorId > kRootId)
                dragIds.push_back(anchorId);

            DragState nextDrag;
            nextDrag.active = true;
            nextDrag.anchorWidgetId = anchorId;
            nextDrag.mode = mode;
            nextDrag.startMouse = startMouse;
            nextDrag.items.reserve(dragIds.size());

            for (const auto dragId : dragIds)
            {
                const auto* widget = findWidgetModel(dragId);
                if (widget == nullptr)
                    continue;

                DragItemState item;
                item.widgetId = dragId;
                item.widgetType = widget->type;
                item.minSize = widgetFactory.minSizeFor(widget->type);
                item.startBounds = widget->bounds;
                item.currentBounds = widget->bounds;
                nextDrag.items.push_back(item);
            }

            if (nextDrag.items.empty())
            {
                dragState = {};
                return false;
            }

            nextDrag.startSelectionBounds = nextDrag.items.front().startBounds;
            for (size_t i = 1; i < nextDrag.items.size(); ++i)
                nextDrag.startSelectionBounds = unionRect(nextDrag.startSelectionBounds, nextDrag.items[i].startBounds);

            constexpr float kScaleEpsilon = 0.0001f;
            if (nextDrag.startSelectionBounds.getWidth() > kScaleEpsilon)
            {
                float minScaleX = 0.0f;
                for (const auto& item : nextDrag.items)
                {
                    if (item.startBounds.getWidth() > kScaleEpsilon)
                        minScaleX = std::max(minScaleX, item.minSize.x / item.startBounds.getWidth());
                }

                nextDrag.minScaleX = minScaleX;
            }

            if (nextDrag.startSelectionBounds.getHeight() > kScaleEpsilon)
            {
                float minScaleY = 0.0f;
                for (const auto& item : nextDrag.items)
                {
                    if (item.startBounds.getHeight() > kScaleEpsilon)
                        minScaleY = std::max(minScaleY, item.minSize.y / item.startBounds.getHeight());
                }

                nextDrag.minScaleY = minScaleY;
            }

            dragState = std::move(nextDrag);
            return true;
        }

        void handleWidgetMouseDown(WidgetId id, bool resizeHit, const juce::MouseEvent& event)
        {
            grabKeyboardFocus();
            refreshAltPreviewState();

            if (!event.mods.isLeftButtonDown())
                return;

            marqueeState = {};
            const auto canvasPos = event.getEventRelativeTo(this).position;

            if (activeGroupEditId.has_value() && !isWidgetInGroup(id, *activeGroupEditId))
                activeGroupEditId.reset();

            if (modifiersAllowResizeDrag(event.mods) && isMultiSelectionResizeHandleHit(canvasPos))
            {
                beginDragForSelection(id, DragMode::resize, canvasPos);
                return;
            }

            const auto selectionUnit = event.mods.isAltDown()
                                         ? altSelectionUnitForWidget(id)
                                         : selectionUnitForWidget(id);

            if (event.mods.isCommandDown())
            {
                auto selection = document.editorState().selection;

                const auto unitFullySelected = std::all_of(selectionUnit.begin(),
                                                           selectionUnit.end(),
                                                           [&selection](WidgetId selectionId)
                                                           {
                                                               return containsWidgetId(selection, selectionId);
                                                           });

                if (unitFullySelected)
                    removeSelectionIds(selection, selectionUnit);
                else
                    addUniqueSelectionIds(selection, selectionUnit);

                document.setSelection(std::move(selection));
                syncSelectionToViews();
                dragState = {};
                return;
            }

            if (event.mods.isShiftDown())
            {
                auto selection = document.editorState().selection;
                addUniqueSelectionIds(selection, selectionUnit);

                document.setSelection(std::move(selection));
                syncSelectionToViews();
                dragState = {};
                return;
            }

            const auto& currentSelection = document.editorState().selection;
            const auto unitFullySelected = std::all_of(selectionUnit.begin(),
                                                       selectionUnit.end(),
                                                       [&currentSelection](WidgetId selectionId)
                                                       {
                                                           return containsWidgetId(currentSelection, selectionId);
                                                       });
            const auto altSelectionMatches = selectionSetsEqual(currentSelection, selectionUnit);

            const auto shouldUpdateSelection = event.mods.isAltDown() ? !altSelectionMatches : !unitFullySelected;
            if (shouldUpdateSelection)
            {
                document.setSelection(selectionUnit);
                syncSelectionToViews();
            }

            if (event.getNumberOfClicks() >= 2)
            {
                if (!activeGroupEditId.has_value())
                {
                    if (const auto selectedGroupId = selectedWholeGroupId(); selectedGroupId.has_value())
                    {
                        activeGroupEditId = *selectedGroupId;
                        document.selectSingle(id);
                        syncSelectionToViews();
                        dragState = {};
                        return;
                    }
                }
            }

            const auto useResize = resizeHit || (modifiersAllowResizeDrag(event.mods) && isMultiSelectionResizeHandleHit(canvasPos));
            beginDragForSelection(id, useResize ? DragMode::resize : DragMode::move, canvasPos);
        }

        void handleWidgetMouseDrag(WidgetId id, const juce::MouseEvent& event)
        {
            if (!dragState.active || dragState.anchorWidgetId != id)
                return;

            const auto canvasPos = event.getEventRelativeTo(this).position;
            const auto delta = canvasPos - dragState.startMouse;
            bool changed = false;

            constexpr float kScaleEpsilon = 0.0001f;
            const auto baseSelection = dragState.startSelectionBounds;
            const auto baseW = baseSelection.getWidth();
            const auto baseH = baseSelection.getHeight();

            float nextSelectionW = baseW + delta.x;
            float nextSelectionH = baseH + delta.y;
            nextSelectionW = std::max(0.0f, nextSelectionW);
            nextSelectionH = std::max(0.0f, nextSelectionH);

            if (baseW > kScaleEpsilon)
                nextSelectionW = std::max(nextSelectionW, baseW * dragState.minScaleX);
            if (baseH > kScaleEpsilon)
                nextSelectionH = std::max(nextSelectionH, baseH * dragState.minScaleY);

            for (auto& item : dragState.items)
            {
                auto nextBounds = item.startBounds;
                if (dragState.mode == DragMode::move)
                {
                    nextBounds = item.startBounds.translated(delta.x, delta.y);
                }
                else
                {
                    if (baseW > kScaleEpsilon)
                    {
                        const auto relX = (item.startBounds.getX() - baseSelection.getX()) / baseW;
                        const auto relW = item.startBounds.getWidth() / baseW;
                        nextBounds.setX(baseSelection.getX() + relX * nextSelectionW);
                        nextBounds.setWidth(relW * nextSelectionW);
                    }
                    else
                    {
                        nextBounds.setWidth(item.startBounds.getWidth());
                    }

                    if (baseH > kScaleEpsilon)
                    {
                        const auto relY = (item.startBounds.getY() - baseSelection.getY()) / baseH;
                        const auto relH = item.startBounds.getHeight() / baseH;
                        nextBounds.setY(baseSelection.getY() + relY * nextSelectionH);
                        nextBounds.setHeight(relH * nextSelectionH);
                    }
                    else
                    {
                        nextBounds.setHeight(item.startBounds.getHeight());
                    }

                    nextBounds.setWidth(std::max(item.minSize.x, nextBounds.getWidth()));
                    nextBounds.setHeight(std::max(item.minSize.y, nextBounds.getHeight()));
                }

                if (areRectsEqual(nextBounds, item.currentBounds))
                    continue;

                item.currentBounds = nextBounds;
                changed = true;
                if (auto* view = findWidgetView(item.widgetId))
                    view->setViewBounds(nextBounds);
            }

            if (changed)
                repaint();
        }

        void handleWidgetMouseUp(WidgetId id)
        {
            if (!dragState.active || dragState.anchorWidgetId != id)
                return;

            const auto drag = dragState;
            dragState = {};

            std::vector<WidgetBoundsUpdate> updates;
            updates.reserve(drag.items.size());
            for (const auto& item : drag.items)
            {
                if (!areRectsEqual(item.startBounds, item.currentBounds))
                    updates.push_back({ item.widgetId, item.currentBounds });
            }

            if (!updates.empty())
                document.setWidgetsBounds(updates);

            refreshFromDocument();

            if (normalizeSelectionAfterAltReleasePending && !altPreviewEnabled)
            {
                normalizeSelectionAfterAltReleasePending = false;
                normalizeSelectionForCurrentModifierState();
            }
        }

        std::vector<WidgetId> collectMarqueeHitIds(const juce::Rectangle<float>& marqueeBounds) const
        {
            std::vector<WidgetId> hits;
            if (marqueeBounds.getWidth() <= 0.0f && marqueeBounds.getHeight() <= 0.0f)
                return hits;

            const auto restrictToGroupId = activeGroupEditId;
            for (const auto& widget : document.snapshot().widgets)
            {
                if (restrictToGroupId.has_value() && !isWidgetInGroup(widget.id, *restrictToGroupId))
                    continue;

                if (widget.bounds.intersects(marqueeBounds))
                    hits.push_back(widget.id);
            }

            return hits;
        }

        void applyMarqueeSelection()
        {
            auto hits = collectMarqueeHitIds(marqueeState.bounds);
            if (!activeGroupEditId.has_value())
                hits = expandToSelectionUnits(hits);

            auto nextSelection = marqueeState.additive || marqueeState.toggle
                                     ? document.editorState().selection
                                     : std::vector<WidgetId> {};

            if (marqueeState.toggle)
            {
                for (const auto id : hits)
                {
                    if (const auto it = std::find(nextSelection.begin(), nextSelection.end(), id); it != nextSelection.end())
                        nextSelection.erase(it);
                    else
                        nextSelection.push_back(id);
                }
            }
            else
            {
                addUniqueSelectionIds(nextSelection, hits);
            }

            document.setSelection(std::move(nextSelection));
            syncSelectionToViews();
        }

        bool nudgeSelection(juce::Point<float> delta)
        {
            if (areClose(delta.x, 0.0f) && areClose(delta.y, 0.0f))
                return false;

            std::vector<WidgetBoundsUpdate> updates;
            const auto& selection = document.editorState().selection;
            updates.reserve(selection.size());

            for (const auto id : selection)
            {
                const auto* widget = findWidgetModel(id);
                if (widget == nullptr)
                    continue;

                updates.push_back({ id, widget->bounds.translated(delta.x, delta.y) });
            }

            if (updates.empty())
                return false;
            if (!document.setWidgetsBounds(updates))
                return false;

            refreshFromDocument();
            return true;
        }

        void notifyStateChanged()
        {
            if (onStateChanged != nullptr)
                onStateChanged();
        }

        void refreshAltPreviewState()
        {
            const auto wasAltDown = altPreviewEnabled;
            const auto nextAltDown = juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown();
            if (altPreviewEnabled == nextAltDown)
                return;

            altPreviewEnabled = nextAltDown;

            if (wasAltDown && !nextAltDown)
            {
                if (dragState.active || marqueeState.active)
                {
                    normalizeSelectionAfterAltReleasePending = true;
                }
                else
                {
                    normalizeSelectionForCurrentModifierState();
                }
            }
            else if (nextAltDown)
            {
                normalizeSelectionAfterAltReleasePending = false;
            }

            repaint();
        }

        void syncSelectionToViews()
        {
            if (activeGroupEditId.has_value())
            {
                const auto* activeGroup = findGroupById(*activeGroupEditId);
                if (activeGroup == nullptr)
                {
                    activeGroupEditId.reset();
                }
                else
                {
                    const auto hasOutsideMember = std::any_of(document.editorState().selection.begin(),
                                                              document.editorState().selection.end(),
                                                              [this](WidgetId selectedId)
                                                              {
                                                                  return !activeGroupEditId.has_value()
                                                                      || !isWidgetInGroup(selectedId, *activeGroupEditId);
                                                              });
                    if (hasOutsideMember)
                        activeGroupEditId.reset();
                }
            }

            const auto& selection = document.editorState().selection;
            const auto showWidgetHandles = selection.size() == 1;
            for (auto& view : widgetViews)
            {
                const auto isSelected = containsWidgetId(selection, view->widgetId());
                const auto* group = findGroupByMember(view->widgetId());
                const auto isGrouped = group != nullptr;
                const auto groupedInActiveEdit = isGrouped
                                              && activeGroupEditId.has_value()
                                              && isWidgetInGroup(view->widgetId(), *activeGroupEditId);
                view->setSelectionVisual(isSelected,
                                         isSelected && showWidgetHandles,
                                         isGrouped,
                                         groupedInActiveEdit);
            }

            repaint();
            notifyStateChanged();
        }

        DocumentHandle& document;
        const Widgets::WidgetFactory& widgetFactory;
        CanvasRenderer renderer;
        std::vector<std::unique_ptr<WidgetComponent>> widgetViews;
        std::function<void()> onStateChanged;
        DragState dragState;
        MarqueeState marqueeState;
        std::optional<WidgetId> activeGroupEditId;
        bool altPreviewEnabled = false;
        bool normalizeSelectionAfterAltReleasePending = false;

        friend class WidgetComponent;
    };

    void WidgetComponent::mouseDown(const juce::MouseEvent& event)
    {
        owner.handleWidgetMouseDown(widget.id, isResizeHandleHit(event.position), event);
    }

    void WidgetComponent::mouseDrag(const juce::MouseEvent& event)
    {
        owner.handleWidgetMouseDrag(widget.id, event);
    }

    void WidgetComponent::mouseUp(const juce::MouseEvent&)
    {
        owner.handleWidgetMouseUp(widget.id);
    }
}

namespace Gyeol
{
    class EditorHandle::Impl
    {
    public:
        explicit Impl(EditorHandle& ownerIn)
            : owner(ownerIn),
              widgetRegistry(Widgets::makeDefaultWidgetRegistry()),
              widgetFactory(widgetRegistry),
              canvas(docHandle, widgetFactory),
              layerTreePanel(docHandle),
              deleteSelected("Delete"),
              groupSelected("Group"),
              ungroupSelected("Ungroup"),
              dumpJsonButton("Dump JSON"),
              exportJuceButton("Export JUCE"),
              undoButton("Undo"),
              redoButton("Redo")
        {
            owner.setWantsKeyboardFocus(true);

            owner.addAndMakeVisible(canvas);
            owner.addAndMakeVisible(layerTreePanel);
            canvas.setStateChangedCallback([this]
                                           {
                                               refreshToolbarState();
                                               layerTreePanel.refreshFromDocument();
                                           });

            layerTreePanel.setSelectionChangedCallback([this](std::vector<WidgetId> selection)
                                                       {
                                                           if (selection == docHandle.editorState().selection)
                                                               return;

                                                           docHandle.setSelection(std::move(selection));
                                                           canvas.refreshFromDocument();
                                                           layerTreePanel.refreshFromDocument();
                                                           refreshToolbarState();
                                                       });
            layerTreePanel.setDropRequestCallback([this](const Ui::Interaction::LayerTreeDropRequest& request) -> juce::Result
                                                  {
                                                      const auto result = layerOrderEngine.applyTreeDrop(docHandle, request);
                                                      if (result.failed())
                                                          return result;

                                                      canvas.refreshFromDocument();
                                                      layerTreePanel.refreshFromDocument();
                                                      refreshToolbarState();
                                                      return juce::Result::ok();
                                                  });

            buildCreateButtons();

            owner.addAndMakeVisible(deleteSelected);
            owner.addAndMakeVisible(groupSelected);
            owner.addAndMakeVisible(ungroupSelected);
            owner.addAndMakeVisible(dumpJsonButton);
            owner.addAndMakeVisible(exportJuceButton);
            owner.addAndMakeVisible(undoButton);
            owner.addAndMakeVisible(redoButton);
            owner.addAndMakeVisible(shortcutHint);

            deleteSelected.onClick = [this]
            {
                canvas.deleteSelection();
            };

            groupSelected.onClick = [this]
            {
                canvas.groupSelection();
            };

            ungroupSelected.onClick = [this]
            {
                canvas.ungroupSelection();
            };

            dumpJsonButton.onClick = [this]
            {
                juce::String json;
                const auto result = Serialization::serializeDocumentToJsonString(docHandle.snapshot(),
                                                                                  docHandle.editorState(),
                                                                                  json);
                if (result.failed())
                {
                    DBG("[Gyeol] JSON dump failed: " + result.getErrorMessage());
                    return;
                }

                DBG("[Gyeol] ----- Document JSON BEGIN -----");
                DBG(json);
                DBG("[Gyeol] ----- Document JSON END -----");

                DBG("[Gyeol] ----- Export Mapping BEGIN -----");
                for (const auto& mapping : widgetFactory.exportMappings())
                    DBG("[Gyeol] " + mapping.typeKey + " -> " + mapping.exportTargetType);
                DBG("[Gyeol] ----- Export Mapping END -----");
            };

            undoButton.onClick = [this]
            {
                canvas.performUndo();
            };

            exportJuceButton.onClick = [this]
            {
                runJuceExport();
            };

            redoButton.onClick = [this]
            {
                canvas.performRedo();
            };

            shortcutHint.setText("Del: delete  Ctrl/Cmd+G: group  Ctrl/Cmd+Shift+G: ungroup  Ctrl/Cmd+[ ]: layer order  Ctrl/Cmd+Z/Y: undo/redo",
                                 juce::dontSendNotification);
            shortcutHint.setJustificationType(juce::Justification::centredRight);
            shortcutHint.setColour(juce::Label::textColourId, juce::Colour::fromRGB(170, 175, 186));
            shortcutHint.setInterceptsMouseClicks(false, false);

            canvas.refreshFromDocument();
            layerTreePanel.refreshFromDocument();
            refreshToolbarState();
        }

        DocumentHandle& document() noexcept { return docHandle; }
        const DocumentHandle& document() const noexcept { return docHandle; }

        void paint(juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            g.fillAll(juce::Colour::fromRGB(21, 24, 30));
            g.setColour(juce::Colour::fromRGB(33, 36, 44));
            g.fillRect(bounds.removeFromTop(toolbarHeight));
        }

        void resized(juce::Rectangle<int> bounds)
        {
            auto toolbar = bounds.removeFromTop(toolbarHeight).reduced(6, 6);

            auto place = [&toolbar](juce::Button& button, int width)
            {
                button.setBounds(toolbar.removeFromLeft(width));
                toolbar.removeFromLeft(4);
            };

            for (auto& entry : createButtons)
            {
                const auto width = juce::jlimit(74,
                                                140,
                                                24 + static_cast<int>(entry.button->getButtonText().length()) * 7);
                place(*entry.button, width);
            }

            place(deleteSelected, 80);
            place(groupSelected, 74);
            place(ungroupSelected, 84);
            place(dumpJsonButton, 94);
            place(exportJuceButton, 104);
            place(undoButton, 66);
            place(redoButton, 66);

            shortcutHint.setBounds(toolbar);

            auto content = bounds.reduced(6);
            auto layerPanelBounds = content.removeFromRight(layerPanelWidth);
            layerTreePanel.setBounds(layerPanelBounds);
            canvas.setBounds(content);
        }

        bool keyPressed(const juce::KeyPress& key)
        {
            const auto mods = key.getModifiers();
            const auto keyCode = key.getKeyCode();
            const auto isOpenBracket = keyCode == '[' || keyCode == '{';
            const auto isCloseBracket = keyCode == ']' || keyCode == '}';

            if (mods.isCommandDown() && !mods.isAltDown() && (isOpenBracket || isCloseBracket))
            {
                Ui::Interaction::LayerMoveCommand command = Ui::Interaction::LayerMoveCommand::bringForward;
                if (isCloseBracket)
                    command = mods.isShiftDown()
                                  ? Ui::Interaction::LayerMoveCommand::bringToFront
                                  : Ui::Interaction::LayerMoveCommand::bringForward;
                else
                    command = mods.isShiftDown()
                                  ? Ui::Interaction::LayerMoveCommand::sendToBack
                                  : Ui::Interaction::LayerMoveCommand::sendBackward;

                const auto result = layerOrderEngine.moveSelection(docHandle, command);
                if (result.wasOk())
                {
                    canvas.refreshFromDocument();
                    layerTreePanel.refreshFromDocument();
                    refreshToolbarState();
                }
                else
                {
                    DBG("[Gyeol] Layer move skipped: " + result.getErrorMessage());
                }

                return true;
            }

            return canvas.keyPressed(key);
        }

    private:
        struct CreateButtonEntry
        {
            WidgetType type = WidgetType::button;
            std::unique_ptr<juce::TextButton> button;
        };

        void buildCreateButtons()
        {
            createButtons.clear();
            createButtons.reserve(widgetRegistry.all().size());

            for (const auto& descriptor : widgetRegistry.all())
            {
                CreateButtonEntry entry;
                entry.type = descriptor.type;

                const auto name = descriptor.displayName.isNotEmpty() ? descriptor.displayName : descriptor.typeKey;
                entry.button = std::make_unique<juce::TextButton>("Add " + name);

                owner.addAndMakeVisible(*entry.button);
                entry.button->onClick = [this, type = entry.type]
                {
                    canvas.createWidget(type);
                };

                createButtons.push_back(std::move(entry));
            }
        }

        juce::File resolveProjectRootDirectory() const
        {
            auto searchDirectory = juce::File::getCurrentWorkingDirectory();

            for (int depth = 0; depth < 10; ++depth)
            {
                if (searchDirectory.getChildFile("DadeumStudio.jucer").existsAsFile())
                    return searchDirectory;

                const auto parent = searchDirectory.getParentDirectory();
                if (parent == searchDirectory)
                    break;

                searchDirectory = parent;
            }

            return juce::File::getCurrentWorkingDirectory();
        }

        juce::File makeExportOutputDirectory(const juce::File& exportRootDirectory,
                                             const juce::String& componentClassName) const
        {
            const auto safeClassName = juce::File::createLegalFileName(componentClassName).trim();
            const auto baseClassName = safeClassName.isNotEmpty() ? safeClassName : juce::String("ExportedComponent");
            const auto timestampUtc = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
            const auto baseFolderName = baseClassName + "_" + timestampUtc;

            auto candidate = exportRootDirectory.getChildFile(baseFolderName);
            int suffix = 1;
            while (candidate.exists())
                candidate = exportRootDirectory.getChildFile(baseFolderName + "_" + juce::String(suffix++));

            return candidate;
        }

        void runJuceExport()
        {
            const auto projectRoot = resolveProjectRootDirectory();
            const auto exportRootDirectory = projectRoot.getChildFile("Builds").getChildFile("GyeolExport");
            const auto componentClassName = juce::String("GyeolExportedComponent");
            const auto outputDirectory = makeExportOutputDirectory(exportRootDirectory, componentClassName);

            Export::ExportOptions options;
            options.outputDirectory = outputDirectory;
            options.projectRootDirectory = projectRoot;
            options.componentClassName = componentClassName;
            options.overwriteExistingFiles = true;
            options.writeManifestJson = true;

            Export::ExportReport report;
            const auto result = Export::exportToJuceComponent(docHandle.snapshot(),
                                                              widgetRegistry,
                                                              options,
                                                              report);

            DBG("[Gyeol] ----- Export Report BEGIN -----");
            DBG(report.toText());
            DBG("[Gyeol] ----- Export Report END -----");

            if (result.failed())
            {
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                            "Gyeol Export",
                                                            "Export failed.\n"
                                                            + result.getErrorMessage()
                                                            + "\n\nOutput: " + outputDirectory.getFullPathName()
                                                            + "\nSee: " + report.reportFile.getFullPathName());
                return;
            }

            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                        "Gyeol Export",
                                                        "Export complete.\n\nOutput: " + outputDirectory.getFullPathName() + "\n\n"
                                                        + report.generatedHeaderFile.getFileName() + "\n"
                                                        + report.generatedSourceFile.getFileName() + "\n"
                                                        + report.manifestFile.getFileName() + "\n"
                                                        + report.reportFile.getFileName());
        }

        void refreshToolbarState()
        {
            deleteSelected.setEnabled(!docHandle.editorState().selection.empty());
            groupSelected.setEnabled(canvas.canGroupSelection());
            ungroupSelected.setEnabled(canvas.canUngroupSelection());
            undoButton.setEnabled(docHandle.canUndo());
            redoButton.setEnabled(docHandle.canRedo());
        }

        EditorHandle& owner;
        static constexpr int toolbarHeight = 44;
        static constexpr int layerPanelWidth = 300;

        DocumentHandle docHandle;
        Widgets::WidgetRegistry widgetRegistry;
        Widgets::WidgetFactory widgetFactory;
        Ui::CanvasComponent canvas;
        Ui::Interaction::LayerOrderEngine layerOrderEngine;
        Ui::Panels::LayerTreePanel layerTreePanel;
        std::vector<CreateButtonEntry> createButtons;
        juce::TextButton deleteSelected;
        juce::TextButton groupSelected;
        juce::TextButton ungroupSelected;
        juce::TextButton dumpJsonButton;
        juce::TextButton exportJuceButton;
        juce::TextButton undoButton;
        juce::TextButton redoButton;
        juce::Label shortcutHint;
    };

    EditorHandle::EditorHandle()
        : impl(std::make_unique<Impl>(*this))
    {
    }

    EditorHandle::~EditorHandle() = default;

    DocumentHandle& EditorHandle::document() noexcept
    {
        return impl->document();
    }

    const DocumentHandle& EditorHandle::document() const noexcept
    {
        return impl->document();
    }

    void EditorHandle::paint(juce::Graphics& g)
    {
        impl->paint(g, getLocalBounds());
    }

    void EditorHandle::resized()
    {
        impl->resized(getLocalBounds());
    }

    bool EditorHandle::keyPressed(const juce::KeyPress& key)
    {
        if (impl->keyPressed(key))
            return true;

        return juce::Component::keyPressed(key);
    }

    std::unique_ptr<EditorHandle> createEditor()
    {
        return std::make_unique<EditorHandle>();
    }
}
