#pragma once

#include "Gyeol/Editor/Canvas/CanvasComponent.h"
#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <functional>

namespace Gyeol::Shell
{
    class EditorModeCoordinator
    {
    public:
        struct State
        {
            bool previewBindingSimulationActive = false;
            bool suppressModeToggleCallbacks = false;
            bool suppressPreviewBindingToggleCallbacks = false;
        };

        struct Components
        {
            DocumentHandle& document;
            Ui::Canvas::CanvasComponent& canvas;
            juce::Button& previewModeButton;
            juce::Button& runModeButton;
            juce::ToggleButton& previewBindingSimToggle;
            juce::Component& leftPanels;
            juce::Component& rightPanels;
            juce::Component& gridSnapPanel;
            juce::Component& historyPanel;
            juce::Label& shortcutHint;
        };

        struct Callbacks
        {
            std::function<void()> refreshToolbarState;
            std::function<void()> refreshAllPanelsFromDocument;
            std::function<void()> syncInspectorTargetFromState;
            std::function<void(bool, bool)> requestDeferredUiRefresh;
            std::function<void(int)> beginRunModeSamplingWindow;
        };

        static constexpr const char* runSessionCoalescedEditKey =
            "gyeol.run.preview-session";
        static constexpr const char* previewBindingSimCoalescedEditKey =
            "gyeol.preview.binding-sim";

        void updateShortcutHintForMode(const State& state,
                                       const Components& components) const;

        void setPreviewBindingSimulation(State& state,
                                         Components& components,
                                         bool enabled,
                                         const Callbacks& callbacks) const;

        void setEditorMode(State& state,
                           Components& components,
                           Ui::Canvas::CanvasComponent::InteractionMode mode,
                           const Callbacks& callbacks) const;
    };
}
