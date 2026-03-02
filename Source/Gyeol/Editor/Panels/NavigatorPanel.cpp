#include "Gyeol/Editor/Panels/NavigatorPanel.h"

#include <algorithm>

namespace Gyeol::Ui::Panels
{
    NavigatorPanel::NavigatorPanel()
    {
        setWantsKeyboardFocus(false);
        setInterceptsMouseClicks(true, false);
    }

    void NavigatorPanel::setScene(juce::Rectangle<float> worldBoundsIn, std::vector<SceneItem> itemsIn)
    {
        if (worldBoundsIn.getWidth() > 0.0f && worldBoundsIn.getHeight() > 0.0f)
            worldBounds = worldBoundsIn;
        sceneItems = std::move(itemsIn);
        repaint();
    }

    void NavigatorPanel::setViewState(juce::Rectangle<float> visibleWorldBoundsIn, float zoomLevelIn)
    {
        visibleWorldBounds = visibleWorldBoundsIn;
        zoomLevel = zoomLevelIn;
        repaint();
    }

    void NavigatorPanel::setNavigateRequestedCallback(std::function<void(juce::Point<float>)> callback)
    {
        onNavigateRequested = std::move(callback);
    }

    void NavigatorPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(40, 46, 56));
        g.drawRect(getLocalBounds(), 1);

        auto headerArea = headerBounds;
        auto headerRight = headerArea.removeFromRight(100);
        g.setColour(juce::Colour::fromRGB(188, 195, 208));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("Navigator", headerArea, juce::Justification::centredLeft, true);

        g.setColour(juce::Colour::fromRGB(158, 166, 182));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Zoom " + juce::String(zoomLevel, 3),
                   headerRight,
                   juce::Justification::centredRight,
                   true);

        const auto transform = computeMapTransform();
        const auto mapBounds = transform.mapBounds;
        g.setColour(juce::Colour::fromRGB(16, 20, 26));
        g.fillRoundedRectangle(mapBounds, 4.0f);
        g.setColour(juce::Colour::fromRGB(60, 68, 82));
        g.drawRoundedRectangle(mapBounds, 4.0f, 1.0f);

        if (!transform.isValid())
            return;

        g.saveState();
        g.reduceClipRegion(mapBounds.toNearestInt());

        for (const auto& item : sceneItems)
        {
            if (!item.visible)
                continue;

            auto itemMapBounds = transform.worldToMap(item.bounds);
            if (itemMapBounds.getWidth() <= 0.0f || itemMapBounds.getHeight() <= 0.0f)
                continue;

            itemMapBounds = itemMapBounds.expanded(0.5f, 0.5f);
            juce::Colour fill = juce::Colour::fromRGB(98, 112, 132).withAlpha(0.46f);
            if (item.locked)
                fill = juce::Colour::fromRGB(124, 94, 90).withAlpha(0.55f);
            if (item.selected)
                fill = juce::Colour::fromRGB(82, 146, 236).withAlpha(0.78f);
            g.setColour(fill);
            g.fillRect(itemMapBounds);
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.drawRect(itemMapBounds, 1.0f);
        }

        auto viewMapBounds = transform.worldToMap(visibleWorldBounds);
        viewMapBounds = viewMapBounds.getIntersection(mapBounds);
        g.setColour(juce::Colour::fromRGB(90, 184, 255).withAlpha(0.18f));
        g.fillRect(viewMapBounds);
        g.setColour(juce::Colour::fromRGB(90, 184, 255));
        g.drawRect(viewMapBounds, 1.5f);

        g.restoreState();
    }

    void NavigatorPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);
        headerBounds = area.removeFromTop(18);
        area.removeFromTop(6);
        mapContainerBounds = area;
    }

    void NavigatorPanel::mouseDown(const juce::MouseEvent& event)
    {
        requestNavigateAt(event.position);
    }

    void NavigatorPanel::mouseDrag(const juce::MouseEvent& event)
    {
        requestNavigateAt(event.position);
    }

    NavigatorPanel::MapTransform NavigatorPanel::computeMapTransform() const noexcept
    {
        MapTransform transform;
        transform.worldBounds = worldBounds;

        auto mapArea = mapContainerBounds.toFloat();
        if (mapArea.isEmpty() || worldBounds.getWidth() <= 0.0f || worldBounds.getHeight() <= 0.0f)
            return transform;

        const auto scaleX = mapArea.getWidth() / worldBounds.getWidth();
        const auto scaleY = mapArea.getHeight() / worldBounds.getHeight();
        const auto scale = std::min(scaleX, scaleY);
        if (scale <= 0.0f)
            return transform;

        const auto mapWidth = worldBounds.getWidth() * scale;
        const auto mapHeight = worldBounds.getHeight() * scale;
        mapArea.setWidth(mapWidth);
        mapArea.setHeight(mapHeight);
        mapArea.setCentre(mapContainerBounds.toFloat().getCentre());

        transform.mapBounds = mapArea;
        transform.scale = scale;
        return transform;
    }

    void NavigatorPanel::requestNavigateAt(juce::Point<float> localPoint)
    {
        const auto transform = computeMapTransform();
        if (!transform.isValid())
            return;
        if (onNavigateRequested == nullptr)
            return;
        if (!transform.mapBounds.contains(localPoint))
            return;

        auto worldPoint = transform.mapToWorld(localPoint);
        worldPoint.x = juce::jlimit(worldBounds.getX(), worldBounds.getRight(), worldPoint.x);
        worldPoint.y = juce::jlimit(worldBounds.getY(), worldBounds.getBottom(), worldPoint.y);
        onNavigateRequested(worldPoint);
    }
}
