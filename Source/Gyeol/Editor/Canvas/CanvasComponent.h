#pragma once

#include "Gyeol/Editor/Canvas/CanvasRenderer.h"
#include "Gyeol/Editor/Canvas/MarqueeSelectionOverlay.h"
#include "Gyeol/Editor/Canvas/SnapGuideOverlay.h"
#include "Gyeol/Editor/Canvas/WidgetComponent.h"
#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

namespace Gyeol::Ui::Canvas
{
    class CanvasComponent : public juce::Component
    {
    public:
        explicit CanvasComponent(DocumentHandle& documentIn);

        void refreshFromDocument();
        bool deleteSelection();
        bool performUndo();
        bool performRedo();

        void setStateChangedCallback(std::function<void()> callback);

        void paint(juce::Graphics& g) override;
        void resized() override;
        bool keyPressed(const juce::KeyPress& key) override;

    private:
        void rebuildWidgetViews();
        void notifyStateChanged();

        DocumentHandle& document;
        CanvasRenderer renderer;
        std::vector<std::unique_ptr<WidgetComponent>> widgetViews;
        MarqueeSelectionOverlay marqueeOverlay;
        SnapGuideOverlay snapGuideOverlay;
        std::function<void()> onStateChanged;
    };
}
