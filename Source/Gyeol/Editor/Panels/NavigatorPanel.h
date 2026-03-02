#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class NavigatorPanel : public juce::Component
    {
    public:
        struct SceneItem
        {
            juce::Rectangle<float> bounds;
            bool selected = false;
            bool visible = true;
            bool locked = false;
        };

        NavigatorPanel();

        void setScene(juce::Rectangle<float> worldBoundsIn, std::vector<SceneItem> itemsIn);
        void setViewState(juce::Rectangle<float> visibleWorldBoundsIn, float zoomLevelIn);
        void setNavigateRequestedCallback(std::function<void(juce::Point<float>)> callback);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;

    private:
        struct MapTransform
        {
            juce::Rectangle<float> mapBounds;
            juce::Rectangle<float> worldBounds;
            float scale = 1.0f;

            bool isValid() const noexcept
            {
                return mapBounds.getWidth() > 0.0f
                    && mapBounds.getHeight() > 0.0f
                    && worldBounds.getWidth() > 0.0f
                    && worldBounds.getHeight() > 0.0f
                    && scale > 0.0f;
            }

            juce::Point<float> worldToMap(juce::Point<float> worldPoint) const noexcept
            {
                return { mapBounds.getX() + (worldPoint.x - worldBounds.getX()) * scale,
                         mapBounds.getY() + (worldPoint.y - worldBounds.getY()) * scale };
            }

            juce::Point<float> mapToWorld(juce::Point<float> mapPoint) const noexcept
            {
                return { worldBounds.getX() + (mapPoint.x - mapBounds.getX()) / scale,
                         worldBounds.getY() + (mapPoint.y - mapBounds.getY()) / scale };
            }

            juce::Rectangle<float> worldToMap(const juce::Rectangle<float>& worldRect) const noexcept
            {
                const auto topLeft = worldToMap(worldRect.getTopLeft());
                return { topLeft.x,
                         topLeft.y,
                         worldRect.getWidth() * scale,
                         worldRect.getHeight() * scale };
            }
        };

        MapTransform computeMapTransform() const noexcept;
        void requestNavigateAt(juce::Point<float> localPoint);

        juce::Rectangle<float> worldBounds { 0.0f, 0.0f, 1600.0f, 1000.0f };
        juce::Rectangle<float> visibleWorldBounds;
        std::vector<SceneItem> sceneItems;
        std::function<void(juce::Point<float>)> onNavigateRequested;
        float zoomLevel = 1.0f;
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> mapContainerBounds;
    };
}
