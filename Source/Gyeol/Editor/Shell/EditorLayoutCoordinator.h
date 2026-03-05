#pragma once

#include "Gyeol/Editor/Canvas/CanvasComponent.h"
#include "Gyeol/Editor/Panels/GridSnapPanel.h"
#include "Gyeol/Editor/Panels/HistoryPanel.h"
#include "Gyeol/Editor/Panels/NavigatorPanel.h"
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace Gyeol::Shell
{
    class EditorLayoutCoordinator
    {
    public:
        enum class BreakpointClass
        {
            wide,
            desktop,
            tablet,
            narrow
        };

        enum class DensityPreset
        {
            compact,
            comfortable,
            spacious
        };

        struct DensityTokens
        {
            int spacing = 4;
            int rowHeight = 28;
            int controlHeight = 32;
            int buttonWidth = 36;
            int toolbarHeight = 44;
            int tabDepth = 30;
            int historyExpanded = 190;
            int historyCollapsed = 36;
            int navSize = 160;
            float fontScale = 1.0f;
        };

        struct LayoutProfile
        {
            int leftWidth = 300;
            int rightWidth = 360;
            int historyHeight = 190;
            bool leftDockVisible = true;
            bool rightDockVisible = true;
            bool leftOverlayOpen = false;
            bool rightOverlayOpen = false;
        };

        struct State
        {
            BreakpointClass breakpointClass = BreakpointClass::desktop;
            DensityPreset densityPreset = DensityPreset::comfortable;
            std::map<int, LayoutProfile> layoutProfiles;
            uint64_t resizeThrottleCount = 0;
            double lastLayoutMs = 0.0;
            double lastResizeMs = 0.0;
            std::map<juce::String, double> panelLayoutTelemetryMs;
            juce::String panelLayoutSummary;
            bool applyingLayoutForSnapshot = false;
            bool forceSingleColumnEditorsActive = false;
            std::optional<float> uiScaleOverride;
            juce::Rectangle<int> lastContentBounds;
            std::vector<int> separatorXs;
        };

        struct ToolbarComponents
        {
            std::vector<juce::Button*> createButtons;
            juce::Button& undoButton;
            juce::Button& redoButton;
            juce::Button& deleteSelected;
            juce::Button& groupSelected;
            juce::Button& ungroupSelected;
            juce::Button& arrangeMenuButton;
            juce::Button& dumpJsonButton;
            juce::Button& exportJuceButton;
            juce::Button& previewModeButton;
            juce::Button& runModeButton;
            juce::Button& previewBindingSimToggle;
            juce::Button& gridSnapMenuButton;
            juce::Button& leftDockButton;
            juce::Button& rightDockButton;
            juce::Button& uiMenuButton;
            juce::Label& shortcutHint;
        };

        struct PanelComponents
        {
            juce::TabbedComponent& leftPanels;
            juce::TabbedComponent& rightPanels;
            Ui::Canvas::CanvasComponent& canvas;
            Ui::Panels::HistoryPanel& historyPanel;
            Ui::Panels::NavigatorPanel& navigatorPanel;
            Ui::Panels::GridSnapPanel& gridSnapPanel;
        };

        struct Callbacks
        {
            std::function<void(const DensityTokens&, bool)> applyDensityTypography;
            std::function<void(int)> applyCompactionRules;
            std::function<void()> refreshViewDiagnosticsPanels;
            std::function<void()> saveUiSettings;
            std::function<bool()> isAsyncUpdatePending;
            std::function<void()> triggerAsyncUpdate;
        };

        static int breakpointStorageKey(BreakpointClass bp) noexcept;
        static BreakpointClass classifyBreakpoint(int width) noexcept;
        static juce::String breakpointName(BreakpointClass bp);
        static juce::String breakpointToken(BreakpointClass bp);
        static juce::String densityName(DensityPreset preset);

        float uiScaleFactor(const State& state, const juce::Component& owner) const;
        DensityTokens currentDensityTokens(const State& state,
                                           const juce::Component& owner) const;
        LayoutProfile defaultLayoutProfileFor(BreakpointClass bp,
                                              juce::Rectangle<int> bounds) const;
        LayoutProfile& ensureLayoutProfile(State& state,
                                           BreakpointClass bp,
                                           juce::Rectangle<int> bounds) const;
        LayoutProfile& activeLayoutProfile(State& state,
                                           const juce::Component& owner) const;

        void resized(State& state,
                     const juce::Component& owner,
                     ToolbarComponents& toolbarComponents,
                     PanelComponents& panelComponents,
                     juce::Rectangle<int> bounds,
                     const Callbacks& callbacks) const;

        void toggleDockVisibility(State& state,
                                  const juce::Component& owner,
                                  bool leftDock,
                                  const std::function<void()>& onResized,
                                  const std::function<void()>& onRepaint,
                                  const std::function<void()>& onSaveUiSettings) const;
    };
}
