#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Serialization/DocumentJson.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

namespace Gyeol::Ui
{
    inline constexpr float kResizeHandleSize = 10.0f;
    inline constexpr float kMinWidgetExtent = 18.0f;
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

    class CanvasRenderer
    {
    public:
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
                         bool resizeHandleHot) const
        {
            const auto body = localBounds.reduced(1.0f);
            const auto fill = juce::Colour::fromRGB(44, 49, 60);
            const auto outline = selected ? juce::Colour::fromRGB(78, 156, 255)
                                          : juce::Colour::fromRGB(95, 101, 114);

            g.setColour(fill);

            switch (widget.type)
            {
                case WidgetType::button:
                {
                    g.fillRoundedRectangle(body, 6.0f);
                    g.setColour(juce::Colour::fromRGB(228, 232, 238));
                    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
                    g.drawFittedText("Button", body.toNearestInt(), juce::Justification::centred, 1);
                    break;
                }

                case WidgetType::slider:
                {
                    g.fillRoundedRectangle(body, 4.0f);
                    const auto track = juce::Rectangle<float>(body.getX() + 10.0f,
                                                              body.getCentreY() - 2.0f,
                                                              std::max(8.0f, body.getWidth() - 20.0f),
                                                              4.0f);
                    g.setColour(juce::Colour::fromRGB(130, 136, 149));
                    g.fillRoundedRectangle(track, 2.0f);
                    g.setColour(juce::Colour::fromRGB(214, 220, 230));
                    g.fillEllipse(track.getCentreX() - 6.0f, track.getCentreY() - 6.0f, 12.0f, 12.0f);
                    break;
                }

                case WidgetType::knob:
                {
                    const auto diameter = std::max(12.0f, std::min(body.getWidth(), body.getHeight()) - 6.0f);
                    const auto knob = juce::Rectangle<float>(diameter, diameter).withCentre(body.getCentre());
                    g.fillEllipse(knob);
                    g.setColour(juce::Colour::fromRGB(214, 220, 230));
                    const auto angle = -juce::MathConstants<float>::pi * 0.25f;
                    const auto c = knob.getCentre();
                    const auto r = knob.getWidth() * 0.34f;
                    g.drawLine(c.x, c.y, c.x + std::cos(angle) * r, c.y + std::sin(angle) * r, 2.0f);
                    break;
                }

                case WidgetType::label:
                {
                    g.fillRoundedRectangle(body, 3.0f);
                    g.setColour(juce::Colour::fromRGB(236, 238, 242));
                    g.setFont(juce::FontOptions(12.0f));
                    const auto text = widget.properties.getWithDefault("text", juce::var("Label")).toString();
                    g.drawFittedText(text.isEmpty() ? "Label" : text,
                                     body.reduced(6.0f).toNearestInt(),
                                     juce::Justification::centredLeft,
                                     1);
                    break;
                }

                case WidgetType::meter:
                {
                    g.fillRoundedRectangle(body, 4.0f);
                    auto fillArea = body.reduced(4.0f);
                    const auto meterValue = 0.62f;
                    const auto level = fillArea.removeFromBottom(fillArea.getHeight() * meterValue);
                    g.setColour(juce::Colour::fromRGB(95, 210, 150));
                    g.fillRoundedRectangle(level, 2.0f);
                    break;
                }
            }

            g.setColour(outline);
            g.drawRoundedRectangle(body, 5.0f, selected ? 2.0f : 1.0f);

            if (selected)
            {
                const auto handle = resizeHandleBounds(localBounds);
                g.setColour(resizeHandleHot ? outline.brighter(0.2f) : outline);
                g.fillRoundedRectangle(handle, 2.0f);
            }
        }
    };

    class CanvasComponent;

    class WidgetComponent : public juce::Component
    {
    public:
        WidgetComponent(CanvasComponent& ownerIn,
                        const CanvasRenderer& rendererIn,
                        const WidgetModel& widgetIn,
                        bool isSelected)
            : owner(ownerIn),
              renderer(rendererIn)
        {
            updateFromModel(widgetIn, isSelected);
            setRepaintsOnMouseActivity(true);
        }

        void updateFromModel(const WidgetModel& widgetIn, bool isSelected)
        {
            widget = widgetIn;
            selected = isSelected;
            resizeHandleHot = false;
            setViewBounds(widget.bounds);
        }

        void setViewBounds(const juce::Rectangle<float>& bounds)
        {
            setBounds(bounds.getSmallestIntegerContainer());
            repaint();
        }

        WidgetId widgetId() const noexcept { return widget.id; }
        bool isResizeHandleHit(juce::Point<float> localPoint) const noexcept
        {
            return selected && renderer.hitResizeHandle(getLocalBounds().toFloat(), localPoint);
        }

        void paint(juce::Graphics& g) override
        {
            renderer.paintWidget(g, widget, getLocalBounds().toFloat(), selected, resizeHandleHot);
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
        bool resizeHandleHot = false;
    };

    class CanvasComponent : public juce::Component
    {
    public:
        explicit CanvasComponent(DocumentHandle& documentIn)
            : document(documentIn)
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
            PropertyBag widgetProps;
            if (type == WidgetType::label)
                widgetProps.set("text", juce::String("Label"));

            const auto newWidgetId = document.addWidget(type, createDefaultBounds(type), widgetProps);
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

        void refreshFromDocument()
        {
            dragState = {};

            widgetViews.clear();
            widgetViews.reserve(document.snapshot().widgets.size());

            const auto& selection = document.editorState().selection;
            for (const auto& widget : document.snapshot().widgets)
            {
                auto view = std::make_unique<WidgetComponent>(*this,
                                                              renderer,
                                                              widget,
                                                              containsWidgetId(selection, widget.id));
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

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (!event.mods.isLeftButtonDown())
                return;

            grabKeyboardFocus();
            document.clearSelection();
            refreshFromDocument();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            const auto mods = key.getModifiers();
            const auto keyCode = key.getKeyCode();
            const auto isZ = keyCode == 'z' || keyCode == 'Z';
            const auto isY = keyCode == 'y' || keyCode == 'Y';

            if (mods.isCommandDown() && isZ)
            {
                if (mods.isShiftDown())
                    return performRedo();
                return performUndo();
            }

            if (mods.isCommandDown() && isY)
                return performRedo();

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

        struct DragState
        {
            bool active = false;
            WidgetId widgetId = 0;
            DragMode mode = DragMode::move;
            juce::Point<float> startMouse;
            juce::Rectangle<float> startBounds;
            juce::Rectangle<float> currentBounds;
        };

        juce::Rectangle<float> createDefaultBounds(WidgetType type) const
        {
            const auto index = static_cast<int>(document.snapshot().widgets.size());
            const auto x = 24.0f + static_cast<float>((index % 10) * 20);
            const auto y = 24.0f + static_cast<float>(((index / 10) % 6) * 20);

            switch (type)
            {
                case WidgetType::button: return { x, y, 96.0f, 30.0f };
                case WidgetType::slider: return { x, y, 170.0f, 34.0f };
                case WidgetType::knob:   return { x, y, 56.0f, 56.0f };
                case WidgetType::label:  return { x, y, 120.0f, 28.0f };
                case WidgetType::meter:  return { x, y, 36.0f, 120.0f };
            }

            return { x, y, 96.0f, 30.0f };
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

        void handleWidgetMouseDown(WidgetId id, bool resizeHit, const juce::MouseEvent& event)
        {
            grabKeyboardFocus();

            if (!containsWidgetId(document.editorState().selection, id))
            {
                document.selectSingle(id);
                refreshFromDocument();
            }

            if (!event.mods.isLeftButtonDown())
                return;

            const auto* widget = findWidgetModel(id);
            if (widget == nullptr)
                return;

            dragState.active = true;
            dragState.widgetId = id;
            dragState.mode = resizeHit ? DragMode::resize : DragMode::move;
            dragState.startMouse = event.getEventRelativeTo(this).position;
            dragState.startBounds = widget->bounds;
            dragState.currentBounds = widget->bounds;
        }

        void handleWidgetMouseDrag(WidgetId id, const juce::MouseEvent& event)
        {
            if (!dragState.active || dragState.widgetId != id)
                return;

            const auto canvasPos = event.getEventRelativeTo(this).position;
            const auto delta = canvasPos - dragState.startMouse;

            auto nextBounds = dragState.startBounds;
            if (dragState.mode == DragMode::move)
            {
                nextBounds = dragState.startBounds.translated(delta.x, delta.y);
            }
            else
            {
                nextBounds.setWidth(std::max(kMinWidgetExtent, dragState.startBounds.getWidth() + delta.x));
                nextBounds.setHeight(std::max(kMinWidgetExtent, dragState.startBounds.getHeight() + delta.y));
            }

            if (areRectsEqual(nextBounds, dragState.currentBounds))
                return;

            dragState.currentBounds = nextBounds;
            if (auto* view = findWidgetView(id))
                view->setViewBounds(nextBounds);
        }

        void handleWidgetMouseUp(WidgetId id)
        {
            if (!dragState.active || dragState.widgetId != id)
                return;

            const auto drag = dragState;
            dragState = {};

            if (areRectsEqual(drag.startBounds, drag.currentBounds))
            {
                refreshFromDocument();
                return;
            }

            if (drag.mode == DragMode::move)
            {
                const auto delta = drag.currentBounds.getPosition() - drag.startBounds.getPosition();
                document.moveWidget(id, delta);
            }
            else
            {
                document.setWidgetBounds(id, drag.currentBounds);
            }

            refreshFromDocument();
        }

        void notifyStateChanged()
        {
            if (onStateChanged != nullptr)
                onStateChanged();
        }

        DocumentHandle& document;
        CanvasRenderer renderer;
        std::vector<std::unique_ptr<WidgetComponent>> widgetViews;
        std::function<void()> onStateChanged;
        DragState dragState;

        friend class WidgetComponent;
    };

    inline void WidgetComponent::mouseDown(const juce::MouseEvent& event)
    {
        owner.handleWidgetMouseDown(widget.id, isResizeHandleHit(event.position), event);
    }

    inline void WidgetComponent::mouseDrag(const juce::MouseEvent& event)
    {
        owner.handleWidgetMouseDrag(widget.id, event);
    }

    inline void WidgetComponent::mouseUp(const juce::MouseEvent&)
    {
        owner.handleWidgetMouseUp(widget.id);
    }
}

namespace Gyeol
{
    class EditorHandle : public juce::Component
    {
    public:
        EditorHandle()
            : canvas(docHandle),
              addButton("Add Button"),
              addSlider("Add Slider"),
              addKnob("Add Knob"),
              addLabel("Add Label"),
              addMeter("Add Meter"),
              deleteSelected("Delete"),
              dumpJsonButton("Dump JSON"),
              undoButton("Undo"),
              redoButton("Redo")
        {
            setWantsKeyboardFocus(true);

            addAndMakeVisible(canvas);
            canvas.setStateChangedCallback([this]
                                           {
                                               refreshToolbarState();
                                           });

            setupCreateButton(addButton, WidgetType::button);
            setupCreateButton(addSlider, WidgetType::slider);
            setupCreateButton(addKnob, WidgetType::knob);
            setupCreateButton(addLabel, WidgetType::label);
            setupCreateButton(addMeter, WidgetType::meter);

            addAndMakeVisible(deleteSelected);
            addAndMakeVisible(dumpJsonButton);
            addAndMakeVisible(undoButton);
            addAndMakeVisible(redoButton);
            addAndMakeVisible(shortcutHint);

            deleteSelected.onClick = [this]
            {
                canvas.deleteSelection();
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
            };

            undoButton.onClick = [this]
            {
                canvas.performUndo();
            };

            redoButton.onClick = [this]
            {
                canvas.performRedo();
            };

            shortcutHint.setText("Del: delete  Ctrl/Cmd+Z: undo  Ctrl/Cmd+Y or Shift+Z: redo",
                                 juce::dontSendNotification);
            shortcutHint.setJustificationType(juce::Justification::centredRight);
            shortcutHint.setColour(juce::Label::textColourId, juce::Colour::fromRGB(170, 175, 186));
            shortcutHint.setInterceptsMouseClicks(false, false);

            canvas.refreshFromDocument();
            refreshToolbarState();
        }

        ~EditorHandle() override = default;

        DocumentHandle& document() noexcept { return docHandle; }
        const DocumentHandle& document() const noexcept { return docHandle; }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(21, 24, 30));
            g.setColour(juce::Colour::fromRGB(33, 36, 44));
            g.fillRect(getLocalBounds().removeFromTop(toolbarHeight));
        }

        void resized() override
        {
            auto area = getLocalBounds();
            auto toolbar = area.removeFromTop(toolbarHeight).reduced(6, 6);

            auto place = [&toolbar](juce::Button& button, int width)
            {
                button.setBounds(toolbar.removeFromLeft(width));
                toolbar.removeFromLeft(4);
            };

            place(addButton, 86);
            place(addSlider, 86);
            place(addKnob, 80);
            place(addLabel, 80);
            place(addMeter, 80);
            place(deleteSelected, 80);
            place(dumpJsonButton, 94);
            place(undoButton, 66);
            place(redoButton, 66);

            shortcutHint.setBounds(toolbar);
            canvas.setBounds(area.reduced(6));
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            if (canvas.keyPressed(key))
                return true;

            return juce::Component::keyPressed(key);
        }

    private:
        void setupCreateButton(juce::TextButton& button, WidgetType type)
        {
            addAndMakeVisible(button);
            button.onClick = [this, type]
            {
                canvas.createWidget(type);
            };
        }

        void refreshToolbarState()
        {
            deleteSelected.setEnabled(!docHandle.editorState().selection.empty());
            undoButton.setEnabled(docHandle.canUndo());
            redoButton.setEnabled(docHandle.canRedo());
        }

        static constexpr int toolbarHeight = 44;

        DocumentHandle docHandle;
        Ui::CanvasComponent canvas;
        juce::TextButton addButton;
        juce::TextButton addSlider;
        juce::TextButton addKnob;
        juce::TextButton addLabel;
        juce::TextButton addMeter;
        juce::TextButton deleteSelected;
        juce::TextButton dumpJsonButton;
        juce::TextButton undoButton;
        juce::TextButton redoButton;
        juce::Label shortcutHint;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorHandle)
    };

    inline std::unique_ptr<EditorHandle> createEditor()
    {
        return std::make_unique<EditorHandle>();
    }
}
