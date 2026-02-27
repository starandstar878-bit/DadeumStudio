#include "Gyeol/Editor/Canvas/CanvasComponent.h"

#include <algorithm>

namespace Gyeol::Ui::Canvas
{
    namespace
    {
        bool containsWidgetId(const std::vector<WidgetId>& ids, WidgetId id)
        {
            return std::find(ids.begin(), ids.end(), id) != ids.end();
        }
    }

    CanvasComponent::CanvasComponent(DocumentHandle& documentIn)
        : document(documentIn)
    {
        setWantsKeyboardFocus(true);
        addAndMakeVisible(marqueeOverlay);
        addAndMakeVisible(snapGuideOverlay);
        marqueeOverlay.setInterceptsMouseClicks(false, false);
        snapGuideOverlay.setInterceptsMouseClicks(false, false);
        refreshFromDocument();
    }

    void CanvasComponent::refreshFromDocument()
    {
        rebuildWidgetViews();
        notifyStateChanged();
        repaint();
    }

    bool CanvasComponent::deleteSelection()
    {
        const auto selection = document.editorState().selection;
        if (selection.empty())
            return false;

        bool changed = false;
        for (const auto id : selection)
            changed = document.removeWidget(id) || changed;

        if (changed)
            refreshFromDocument();
        return changed;
    }

    bool CanvasComponent::performUndo()
    {
        if (!document.undo())
            return false;

        refreshFromDocument();
        return true;
    }

    bool CanvasComponent::performRedo()
    {
        if (!document.redo())
            return false;

        refreshFromDocument();
        return true;
    }

    void CanvasComponent::setStateChangedCallback(std::function<void()> callback)
    {
        onStateChanged = std::move(callback);
    }

    void CanvasComponent::paint(juce::Graphics& g)
    {
        renderer.paintCanvas(g, getLocalBounds());
    }

    void CanvasComponent::resized()
    {
        for (const auto& widget : document.snapshot().widgets)
        {
            const auto it = std::find_if(widgetViews.begin(),
                                         widgetViews.end(),
                                         [id = widget.id](const auto& view)
                                         {
                                             return view->widgetId() == id;
                                         });
            if (it != widgetViews.end())
                (*it)->setBounds(widget.bounds.getSmallestIntegerContainer());
        }

        marqueeOverlay.setBounds(getLocalBounds());
        snapGuideOverlay.setBounds(getLocalBounds());
        marqueeOverlay.toFront(false);
        snapGuideOverlay.toFront(false);
    }

    bool CanvasComponent::keyPressed(const juce::KeyPress& key)
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
            && (keyCode == juce::KeyPress::deleteKey || keyCode == juce::KeyPress::backspaceKey))
        {
            return deleteSelection();
        }

        return false;
    }

    void CanvasComponent::rebuildWidgetViews()
    {
        widgetViews.clear();
        widgetViews.reserve(document.snapshot().widgets.size());

        const auto& selection = document.editorState().selection;
        for (const auto& widget : document.snapshot().widgets)
        {
            auto view = std::make_unique<WidgetComponent>(renderer);
            view->setModel(widget, containsWidgetId(selection, widget.id));
            addAndMakeVisible(*view);
            widgetViews.push_back(std::move(view));
        }

        marqueeOverlay.toFront(false);
        snapGuideOverlay.toFront(false);
    }

    void CanvasComponent::notifyStateChanged()
    {
        if (onStateChanged != nullptr)
            onStateChanged();
    }
}
