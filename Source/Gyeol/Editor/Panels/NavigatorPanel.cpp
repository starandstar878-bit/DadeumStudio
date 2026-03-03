#include "Gyeol/Editor/Panels/NavigatorPanel.h"

#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        using Gyeol::GyeolPalette;

        juce::Colour palette(GyeolPalette id, float alpha = 1.0f)
        {
            return Gyeol::getGyeolColor(id).withAlpha(alpha);
        }

        juce::String formatZoomBadgeText(float zoomLevel)
        {
            return "x" + juce::String(zoomLevel, 1);
        }
    }

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
        g.fillAll(palette(GyeolPalette::PanelBackground));
        g.setColour(palette(GyeolPalette::BorderDefault));
        g.drawRect(getLocalBounds(), 1);

        auto headerArea = headerBounds;
        g.setColour(palette(GyeolPalette::TextPrimary));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("Navigator", headerArea.removeFromLeft(86), juce::Justification::centredLeft, true);

        const auto zoomText = formatZoomBadgeText(zoomLevel);
        const juce::Font badgeFont(juce::FontOptions(10.0f, juce::Font::bold));
        const auto badgeWidth = juce::jmax(52.0f, badgeFont.getStringWidthFloat(zoomText) + 20.0f);
        auto badgeBounds = headerArea.removeFromRight(static_cast<int>(std::ceil(badgeWidth))).toFloat().reduced(0.0f, 2.0f);

        g.setColour(palette(GyeolPalette::HeaderBackground, 0.90f));
        g.fillRoundedRectangle(badgeBounds, 6.0f);
        g.setColour(palette(GyeolPalette::BorderDefault, 0.96f));
        g.drawRoundedRectangle(badgeBounds, 6.0f, 1.0f);

        g.setColour(palette(GyeolPalette::TextSecondary, 0.96f));
        g.setFont(badgeFont);
        g.drawText(zoomText, badgeBounds.toNearestInt(), juce::Justification::centred, true);

        const auto transform = computeMapTransform();
        const auto mapBounds = transform.mapBounds;
        if (mapBounds.isEmpty())
            return;

        Gyeol::GyeolCustomLookAndFeel::drawSoftDropShadow(g, mapBounds, 10.0f, 0.22f);
        g.setColour(palette(GyeolPalette::OverlayBackground, 0.78f));
        g.fillRoundedRectangle(mapBounds, 6.0f);
        g.setColour(palette(GyeolPalette::BorderDefault, 0.96f));
        g.drawRoundedRectangle(mapBounds, 6.0f, 1.0f);

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

            itemMapBounds = itemMapBounds.expanded(0.4f, 0.4f);
            if (itemMapBounds.getWidth() < 1.8f || itemMapBounds.getHeight() < 1.8f)
                itemMapBounds = itemMapBounds.withSizeKeepingCentre(
                    juce::jmax(itemMapBounds.getWidth(), 1.8f),
                    juce::jmax(itemMapBounds.getHeight(), 1.8f));

            juce::Colour fill = palette(GyeolPalette::ControlHover, 0.36f);
            if (item.locked)
                fill = palette(GyeolPalette::ValidWarning, 0.34f);
            if (item.selected)
                fill = palette(GyeolPalette::AccentPrimary, 0.78f);

            g.setColour(fill);
            g.fillRoundedRectangle(itemMapBounds, 2.0f);

            const auto outline = item.selected ? palette(GyeolPalette::BorderActive, 0.96f)
                                               : palette(GyeolPalette::BorderDefault, 0.74f);
            g.setColour(outline);
            g.drawRoundedRectangle(itemMapBounds, 2.0f, 1.0f);
        }

        auto viewMapBounds = transform.worldToMap(visibleWorldBounds);
        viewMapBounds = viewMapBounds.getIntersection(mapBounds);
        if (!viewMapBounds.isEmpty())
        {
            g.setColour(palette(GyeolPalette::AccentPrimary, 0.08f));
            g.fillRoundedRectangle(viewMapBounds.expanded(2.0f, 2.0f), 3.0f);
            g.setColour(palette(GyeolPalette::AccentPrimary, 0.18f));
            g.fillRoundedRectangle(viewMapBounds, 2.5f);
            g.setColour(palette(GyeolPalette::AccentPrimary, 0.96f));
            g.drawRoundedRectangle(viewMapBounds, 2.5f, 1.5f);
        }

        g.restoreState();
    }

    void NavigatorPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);
        headerBounds = area.removeFromTop(20);
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