#include "Gyeol/Editor/Canvas/WidgetComponent.h"

#include "Gyeol/Editor/Canvas/CanvasComponent.h"

namespace Gyeol::Ui::Canvas
{
    WidgetComponent::WidgetComponent(CanvasComponent& ownerIn,
                                     CanvasRenderer& rendererIn)
        : owner(ownerIn),
          renderer(rendererIn)
    {
        setRepaintsOnMouseActivity(true);
    }

    void WidgetComponent::setModel(const WidgetModel& widgetIn,
                                   juce::Rectangle<float> viewBoundsIn,
                                   bool selectedIn,
                                   float effectiveOpacityIn,
                                   bool showResizeHandleIn)
    {
        const auto modelChanged = model.type != widgetIn.type
            || model.properties != widgetIn.properties
            || model.opacity != widgetIn.opacity
            || model.visible != widgetIn.visible
            || model.locked != widgetIn.locked;
        const auto opacityChanged = effectiveOpacity != effectiveOpacityIn;

        model = widgetIn;
        selected = selectedIn;
        showResizeHandle = showResizeHandleIn;
        effectiveOpacity = effectiveOpacityIn;
        resizeHandleHot = false;

        setViewBounds(viewBoundsIn);
        if (modelChanged || opacityChanged)
            repaint();
    }

    bool WidgetComponent::setSelectionVisual(bool selectedIn,
                                             bool showResizeHandleIn)
    {
        if (selected == selectedIn && showResizeHandle == showResizeHandleIn)
            return false;

        selected = selectedIn;
        showResizeHandle = showResizeHandleIn;
        if (!showResizeHandle)
            resizeHandleHot = false;
        repaint();
        return true;
    }

    void WidgetComponent::setViewBounds(const juce::Rectangle<float>& bounds)
    {
        const auto nextBounds = bounds.getSmallestIntegerContainer();
        if (getBounds() == nextBounds)
            return;
        setBounds(nextBounds);
    }

    WidgetId WidgetComponent::widgetId() const noexcept
    {
        return model.id;
    }

    bool WidgetComponent::isResizeHandleHit(juce::Point<float> localPoint) const noexcept
    {
        return selected && showResizeHandle
            && renderer.hitResizeHandle(getLocalBounds().toFloat(), localPoint);
    }

    void WidgetComponent::paint(juce::Graphics& g)
    {
        renderer.paintWidget(g,
                             model,
                             getLocalBounds().toFloat(),
                             selected,
                             effectiveOpacity,
                             showResizeHandle,
                             resizeHandleHot);
    }

    void WidgetComponent::mouseMove(const juce::MouseEvent& event)
    {
        owner.updateMouseTrackerFromChild(event.getEventRelativeTo(&owner).position);
        const auto hot = isResizeHandleHit(event.position);
        if (hot == resizeHandleHot)
            return;

        resizeHandleHot = hot;
        repaint();
    }

    void WidgetComponent::mouseExit(const juce::MouseEvent&)
    {
        owner.clearMouseTrackerFromChild();
        if (!resizeHandleHot)
            return;

        resizeHandleHot = false;
        repaint();
    }

    void WidgetComponent::mouseDown(const juce::MouseEvent& event)
    {
        owner.handleWidgetMouseDown(model.id,
                                    isResizeHandleHit(event.position),
                                    event);
    }

    void WidgetComponent::mouseDrag(const juce::MouseEvent& event)
    {
        owner.handleWidgetMouseDrag(model.id, event);
    }

    void WidgetComponent::mouseUp(const juce::MouseEvent& event)
    {
        owner.handleWidgetMouseUp(model.id, &event);
    }

    void WidgetComponent::mouseDoubleClick(const juce::MouseEvent& event)
    {
        owner.handleCanvasDoubleClick(event.getEventRelativeTo(&owner).position);
    }

    void WidgetComponent::mouseWheelMove(const juce::MouseEvent& event,
                                         const juce::MouseWheelDetails& wheel)
    {
        owner.applyWheelZoomAtPoint(event.getEventRelativeTo(&owner).position,
                                    wheel);
    }
}
