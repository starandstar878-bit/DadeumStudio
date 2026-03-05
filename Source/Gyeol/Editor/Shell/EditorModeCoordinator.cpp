#include "Gyeol/Editor/Shell/EditorModeCoordinator.h"

namespace Gyeol::Shell
{
    void EditorModeCoordinator::updateShortcutHintForMode(
        const State& state,
        const Components& components) const
    {
        const auto previewText =
            "Preview: Cmd/Ctrl+K palette  Cmd/Ctrl+P switcher  Del delete  "
            "Cmd/Ctrl+G group  Cmd/Ctrl+Shift+G ungroup  "
            "Cmd/Ctrl+Alt+Arrows/H/V align  Cmd/Ctrl+Alt+Shift+H/V distribute  "
            "Cmd/Ctrl+[ ] layer order  Cmd/Ctrl+Z/Y undo/redo";
        const auto previewSimText =
            "Preview(Sim Bind): bindings simulated as temporary preview. "
            "Toggle off to rollback.";
        const auto runText =
            "Run: interact with widgets, runtimeBindings execute, click Run "
            "again or Esc to return Preview(rollback)";

        if (components.canvas.isRunMode())
            components.shortcutHint.setText(runText, juce::dontSendNotification);
        else if (state.previewBindingSimulationActive)
            components.shortcutHint.setText(previewSimText, juce::dontSendNotification);
        else
            components.shortcutHint.setText(previewText, juce::dontSendNotification);
    }

    void EditorModeCoordinator::setPreviewBindingSimulation(
        State& state,
        Components& components,
        bool enabled,
        const Callbacks& callbacks) const
    {
        if (state.previewBindingSimulationActive == enabled)
            return;

        if (enabled && components.canvas.isRunMode())
            return;

        if (enabled)
        {
            if (!components.document.beginCoalescedEdit(previewBindingSimCoalescedEditKey))
            {
                DBG("[Gyeol][PreviewSim] beginCoalescedEdit failed");
                return;
            }
        }
        else
        {
            if (!components.document.endCoalescedEdit(previewBindingSimCoalescedEditKey, false))
                DBG("[Gyeol][PreviewSim] rollback failed");
        }

        state.previewBindingSimulationActive = enabled;
        components.canvas.setPreviewBindingSimulationEnabled(enabled);
        components.canvas.setEnabled(!state.previewBindingSimulationActive
                                     || components.canvas.isRunMode());

        state.suppressPreviewBindingToggleCallbacks = true;
        components.previewBindingSimToggle.setToggleState(state.previewBindingSimulationActive,
                                                          juce::dontSendNotification);
        state.suppressPreviewBindingToggleCallbacks = false;

        const auto interactionLocked =
            components.canvas.isRunMode() || state.previewBindingSimulationActive;
        components.leftPanels.setEnabled(!interactionLocked);
        components.rightPanels.setEnabled(!interactionLocked);
        components.gridSnapPanel.setEnabled(!interactionLocked);

        updateShortcutHintForMode(state, components);

        if (callbacks.refreshToolbarState)
            callbacks.refreshToolbarState();
        if (callbacks.refreshAllPanelsFromDocument)
            callbacks.refreshAllPanelsFromDocument();
    }

    void EditorModeCoordinator::setEditorMode(
        State& state,
        Components& components,
        Ui::Canvas::CanvasComponent::InteractionMode mode,
        const Callbacks& callbacks) const
    {
        if (mode == Ui::Canvas::CanvasComponent::InteractionMode::run
            && state.previewBindingSimulationActive)
            setPreviewBindingSimulation(state, components, false, callbacks);

        const auto previousMode = components.canvas.interactionMode();
        const auto modeChanged = previousMode != mode;

        if (modeChanged && mode == Ui::Canvas::CanvasComponent::InteractionMode::run)
        {
            if (!components.document.beginCoalescedEdit(runSessionCoalescedEditKey))
                DBG("[Gyeol][RunMode] beginCoalescedEdit failed");
        }
        else if (modeChanged
                 && previousMode == Ui::Canvas::CanvasComponent::InteractionMode::run)
        {
            if (!components.document.endCoalescedEdit(runSessionCoalescedEditKey, false))
                DBG("[Gyeol][RunMode] rollback failed");
        }

        components.canvas.setInteractionMode(mode);
        components.canvas.setEnabled(!state.previewBindingSimulationActive
                                     || components.canvas.isRunMode());

        state.suppressModeToggleCallbacks = true;
        components.previewModeButton.setToggleState(
            mode == Ui::Canvas::CanvasComponent::InteractionMode::preview,
            juce::dontSendNotification);
        components.runModeButton.setToggleState(
            mode == Ui::Canvas::CanvasComponent::InteractionMode::run,
            juce::dontSendNotification);
        state.suppressModeToggleCallbacks = false;

        const auto interactionLocked =
            components.canvas.isRunMode() || state.previewBindingSimulationActive;
        components.leftPanels.setEnabled(!interactionLocked);
        components.rightPanels.setEnabled(!interactionLocked);
        components.gridSnapPanel.setEnabled(!interactionLocked);
        components.historyPanel.setEnabled(true);

        updateShortcutHintForMode(state, components);

        if (callbacks.refreshAllPanelsFromDocument)
            callbacks.refreshAllPanelsFromDocument();
        if (callbacks.refreshToolbarState)
            callbacks.refreshToolbarState();
        if (callbacks.syncInspectorTargetFromState)
            callbacks.syncInspectorTargetFromState();
        if (callbacks.requestDeferredUiRefresh)
            callbacks.requestDeferredUiRefresh(false, true);
    }
}
