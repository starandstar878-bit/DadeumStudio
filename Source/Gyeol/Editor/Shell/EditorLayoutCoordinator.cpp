#include "Gyeol/Editor/Shell/EditorLayoutCoordinator.h"

#include <algorithm>

namespace Gyeol::Shell
{
    int EditorLayoutCoordinator::breakpointStorageKey(BreakpointClass bp) noexcept
    {
        switch (bp)
        {
            case BreakpointClass::wide: return 0;
            case BreakpointClass::desktop: return 1;
            case BreakpointClass::tablet: return 2;
            case BreakpointClass::narrow:
            default: return 3;
        }
    }

    EditorLayoutCoordinator::BreakpointClass
    EditorLayoutCoordinator::classifyBreakpoint(int width) noexcept
    {
        if (width >= 1600)
            return BreakpointClass::wide;
        if (width >= 1200)
            return BreakpointClass::desktop;
        if (width >= 900)
            return BreakpointClass::tablet;
        return BreakpointClass::narrow;
    }

    juce::String EditorLayoutCoordinator::breakpointName(BreakpointClass bp)
    {
        switch (bp)
        {
            case BreakpointClass::wide: return "wide >=1600";
            case BreakpointClass::desktop: return "desktop 1200-1599";
            case BreakpointClass::tablet: return "tablet 900-1199";
            case BreakpointClass::narrow:
            default: return "narrow <900";
        }
    }

    juce::String EditorLayoutCoordinator::breakpointToken(BreakpointClass bp)
    {
        switch (bp)
        {
            case BreakpointClass::wide: return "wide";
            case BreakpointClass::desktop: return "desktop";
            case BreakpointClass::tablet: return "tablet";
            case BreakpointClass::narrow:
            default: return "narrow";
        }
    }

    juce::String EditorLayoutCoordinator::densityName(DensityPreset preset)
    {
        switch (preset)
        {
            case DensityPreset::compact: return "Compact";
            case DensityPreset::comfortable: return "Comfortable";
            case DensityPreset::spacious:
            default: return "Spacious";
        }
    }

    float EditorLayoutCoordinator::uiScaleFactor(const State& state,
                                                  const juce::Component& owner) const
    {
        if (state.uiScaleOverride.has_value())
            return juce::jlimit(1.0f, 2.5f, *state.uiScaleOverride);

        return juce::jlimit(1.0f, 2.5f, owner.getDesktopScaleFactor());
    }

    EditorLayoutCoordinator::DensityTokens
    EditorLayoutCoordinator::currentDensityTokens(const State& state,
                                                  const juce::Component& owner) const
    {
        DensityTokens tokens;
        switch (state.densityPreset)
        {
            case DensityPreset::compact:
                tokens.spacing = 3;
                tokens.rowHeight = 24;
                tokens.controlHeight = 28;
                tokens.buttonWidth = 32;
                tokens.toolbarHeight = 38;
                tokens.tabDepth = 26;
                tokens.historyExpanded = 150;
                tokens.historyCollapsed = 30;
                tokens.navSize = 132;
                tokens.fontScale = 0.92f;
                break;
            case DensityPreset::comfortable:
                tokens.spacing = 4;
                tokens.rowHeight = 28;
                tokens.controlHeight = 32;
                tokens.buttonWidth = 36;
                tokens.toolbarHeight = 44;
                tokens.tabDepth = 30;
                tokens.historyExpanded = 190;
                tokens.historyCollapsed = 36;
                tokens.navSize = 160;
                tokens.fontScale = 1.0f;
                break;
            case DensityPreset::spacious:
            default:
                tokens.spacing = 6;
                tokens.rowHeight = 32;
                tokens.controlHeight = 36;
                tokens.buttonWidth = 42;
                tokens.toolbarHeight = 50;
                tokens.tabDepth = 34;
                tokens.historyExpanded = 230;
                tokens.historyCollapsed = 40;
                tokens.navSize = 188;
                tokens.fontScale = 1.08f;
                break;
        }

        const auto scale = uiScaleFactor(state, owner);
        const auto scaled = [scale](int value)
        {
            return juce::jmax(1, juce::roundToInt(static_cast<float>(value) * scale));
        };

        tokens.spacing = scaled(tokens.spacing);
        tokens.rowHeight = scaled(tokens.rowHeight);
        tokens.controlHeight = scaled(tokens.controlHeight);
        tokens.buttonWidth = scaled(tokens.buttonWidth);
        tokens.toolbarHeight = scaled(tokens.toolbarHeight);
        tokens.tabDepth = scaled(tokens.tabDepth);
        tokens.historyExpanded = scaled(tokens.historyExpanded);
        tokens.historyCollapsed = scaled(tokens.historyCollapsed);
        tokens.navSize = scaled(tokens.navSize);
        tokens.fontScale *= juce::jlimit(1.0f, 1.4f, scale);
        return tokens;
    }

    EditorLayoutCoordinator::LayoutProfile
    EditorLayoutCoordinator::defaultLayoutProfileFor(BreakpointClass bp,
                                                     juce::Rectangle<int> bounds) const
    {
        LayoutProfile profile;
        switch (bp)
        {
            case BreakpointClass::wide:
                profile.leftWidth = 320;
                profile.rightWidth = 400;
                profile.historyHeight = 230;
                break;
            case BreakpointClass::desktop:
                profile.leftWidth = 300;
                profile.rightWidth = 360;
                profile.historyHeight = 200;
                break;
            case BreakpointClass::tablet:
                profile.leftWidth = 240;
                profile.rightWidth = 300;
                profile.historyHeight = 170;
                break;
            case BreakpointClass::narrow:
            default:
                profile.leftWidth =
                    juce::roundToInt(static_cast<float>(bounds.getWidth()) * 0.72f);
                profile.rightWidth =
                    juce::roundToInt(static_cast<float>(bounds.getWidth()) * 0.72f);
                profile.historyHeight = 140;
                break;
        }

        return profile;
    }

    EditorLayoutCoordinator::LayoutProfile&
    EditorLayoutCoordinator::ensureLayoutProfile(State& state,
                                                 BreakpointClass bp,
                                                 juce::Rectangle<int> bounds) const
    {
        const auto key = breakpointStorageKey(bp);
        if (const auto it = state.layoutProfiles.find(key); it != state.layoutProfiles.end())
            return it->second;

        return state.layoutProfiles.emplace(key, defaultLayoutProfileFor(bp, bounds))
            .first->second;
    }

    EditorLayoutCoordinator::LayoutProfile&
    EditorLayoutCoordinator::activeLayoutProfile(State& state,
                                                 const juce::Component& owner) const
    {
        const auto bounds = state.lastContentBounds.isEmpty()
            ? owner.getLocalBounds()
            : state.lastContentBounds;
        return ensureLayoutProfile(state, state.breakpointClass, bounds);
    }

    void EditorLayoutCoordinator::resized(State& state,
                                          const juce::Component& owner,
                                          ToolbarComponents& toolbarComponents,
                                          PanelComponents& panelComponents,
                                          juce::Rectangle<int> bounds,
                                          const Callbacks& callbacks) const
    {
        const auto layoutStartMs = juce::Time::getMillisecondCounterHiRes();
        const auto nowMs = juce::Time::getMillisecondCounterHiRes();
        const auto throttledResize = (nowMs - state.lastResizeMs) < 18.0;
        if (throttledResize)
            ++state.resizeThrottleCount;
        state.lastResizeMs = nowMs;

        state.breakpointClass = classifyBreakpoint(bounds.getWidth());
        state.forceSingleColumnEditorsActive =
            state.breakpointClass != BreakpointClass::narrow && bounds.getWidth() < 1120;

        const auto tokens = currentDensityTokens(state, owner);
        if (callbacks.applyDensityTypography)
            callbacks.applyDensityTypography(tokens, state.forceSingleColumnEditorsActive);
        if (callbacks.applyCompactionRules)
            callbacks.applyCompactionRules(bounds.getWidth());

        auto& profile = ensureLayoutProfile(state, state.breakpointClass, bounds);

        state.separatorXs.clear();
        auto toolbar = bounds.removeFromTop(tokens.toolbarHeight)
                           .reduced(tokens.spacing * 2, tokens.spacing);

        const auto compactToolbar = bounds.getWidth() < 1280;
        const auto iconButtonWidth = tokens.buttonWidth;
        const auto compactButtonWidth = iconButtonWidth + tokens.spacing * 2;
        const auto modeButtonWidth =
            juce::roundToInt(static_cast<float>(tokens.buttonWidth) * 2.2f);

        const auto place = [&toolbar, tokens](juce::Button& button, int width)
        {
            auto slot = toolbar.removeFromLeft(width);
            const auto controlHeight = juce::jmin(tokens.controlHeight, slot.getHeight());
            const auto controlY = slot.getY() + (slot.getHeight() - controlHeight) / 2;
            button.setBounds(slot.withY(controlY).withHeight(controlHeight));
            toolbar.removeFromLeft(tokens.spacing);
        };

        const auto addSeparator = [&toolbar, &state, tokens]()
        {
            toolbar.removeFromLeft(tokens.spacing);
            state.separatorXs.push_back(toolbar.getX());
            toolbar.removeFromLeft(tokens.spacing * 2);
        };

        for (auto* const button : toolbarComponents.createButtons)
        {
            if (button != nullptr)
                place(*button, iconButtonWidth);
        }

        addSeparator();
        place(toolbarComponents.undoButton, iconButtonWidth);
        place(toolbarComponents.redoButton, iconButtonWidth);
        place(toolbarComponents.deleteSelected, iconButtonWidth);
        place(toolbarComponents.groupSelected, iconButtonWidth);
        place(toolbarComponents.ungroupSelected, iconButtonWidth);
        place(toolbarComponents.arrangeMenuButton, iconButtonWidth);

        addSeparator();
        place(toolbarComponents.dumpJsonButton,
              compactToolbar ? compactButtonWidth : compactButtonWidth + tokens.spacing);
        place(toolbarComponents.exportJuceButton,
              compactToolbar ? compactButtonWidth : compactButtonWidth + tokens.spacing);

        addSeparator();
        place(toolbarComponents.previewModeButton, modeButtonWidth);
        place(toolbarComponents.runModeButton, modeButtonWidth);
        place(toolbarComponents.previewBindingSimToggle, modeButtonWidth + tokens.spacing);
        place(toolbarComponents.gridSnapMenuButton,
              compactToolbar ? compactButtonWidth : compactButtonWidth + tokens.spacing);

        addSeparator();
        place(toolbarComponents.leftDockButton, compactButtonWidth);
        place(toolbarComponents.rightDockButton, compactButtonWidth);
        place(toolbarComponents.uiMenuButton, compactButtonWidth);

        toolbarComponents.shortcutHint.setBounds(toolbar);

        auto content = bounds.reduced(tokens.spacing * 2);
        state.lastContentBounds = content;

        const auto scale = uiScaleFactor(state, owner);
        const auto minLeft = juce::roundToInt(200.0f * scale);
        const auto maxLeft = juce::jmax(
            minLeft,
            juce::jmin(juce::roundToInt(420.0f * scale),
                       juce::roundToInt(content.getWidth() * 0.62f)));
        const auto minRight = juce::roundToInt(250.0f * scale);
        const auto maxRight = juce::jmax(
            minRight,
            juce::jmin(juce::roundToInt(520.0f * scale),
                       juce::roundToInt(content.getWidth() * 0.68f)));

        profile.leftWidth = juce::jlimit(minLeft, maxLeft, profile.leftWidth);
        profile.rightWidth = juce::jlimit(minRight, maxRight, profile.rightWidth);

        const auto historyMax =
            juce::jmax(tokens.historyCollapsed + tokens.spacing * 4,
                       juce::roundToInt(content.getHeight() * 0.40f));
        profile.historyHeight =
            juce::jlimit(tokens.historyCollapsed + tokens.spacing * 2,
                         historyMax,
                         profile.historyHeight);

        const auto historyDockHeight = panelComponents.historyPanel.isCollapsed()
            ? tokens.historyCollapsed
            : profile.historyHeight;
        auto historyBounds = content.removeFromBottom(historyDockHeight);

        state.panelLayoutTelemetryMs.clear();
        const auto setBoundsWithTelemetry = [&state](juce::Component& component,
                                                     const juce::Rectangle<int>& nextBounds,
                                                     const juce::String& name)
        {
            const auto beginMs = juce::Time::getMillisecondCounterHiRes();
            component.setBounds(nextBounds);
            state.panelLayoutTelemetryMs[name] =
                juce::Time::getMillisecondCounterHiRes() - beginMs;
        };

        if (state.breakpointClass == BreakpointClass::narrow)
        {
            profile.leftDockVisible = true;
            profile.rightDockVisible = true;

            setBoundsWithTelemetry(panelComponents.canvas, content, "canvas");
            setBoundsWithTelemetry(panelComponents.historyPanel, historyBounds, "history");

            panelComponents.leftPanels.setVisible(profile.leftOverlayOpen);
            panelComponents.rightPanels.setVisible(profile.rightOverlayOpen);

            if (profile.leftOverlayOpen)
            {
                auto overlayBounds = content.reduced(tokens.spacing);
                const auto overlayWidth = juce::jlimit(
                    minLeft,
                    juce::jmax(minLeft, content.getWidth() - tokens.spacing * 8),
                    profile.leftWidth);
                overlayBounds.setWidth(overlayWidth);
                setBoundsWithTelemetry(panelComponents.leftPanels, overlayBounds, "leftOverlay");
                panelComponents.leftPanels.toFront(false);
            }

            if (profile.rightOverlayOpen)
            {
                auto overlayBounds = content.reduced(tokens.spacing);
                const auto overlayWidth = juce::jlimit(
                    minRight,
                    juce::jmax(minRight, content.getWidth() - tokens.spacing * 8),
                    profile.rightWidth);
                overlayBounds.setWidth(overlayWidth);
                overlayBounds.setX(content.getRight() - overlayWidth - tokens.spacing);
                setBoundsWithTelemetry(panelComponents.rightPanels, overlayBounds, "rightOverlay");
                panelComponents.rightPanels.toFront(false);
            }
        }
        else
        {
            profile.leftOverlayOpen = false;

            auto canvasBounds = content;
            if (profile.leftDockVisible)
            {
                auto leftBounds = canvasBounds.removeFromLeft(profile.leftWidth);
                setBoundsWithTelemetry(panelComponents.leftPanels, leftBounds, "leftDock");
                panelComponents.leftPanels.setVisible(true);
            }
            else
            {
                panelComponents.leftPanels.setVisible(false);
            }

            if (state.forceSingleColumnEditorsActive)
            {
                if (profile.rightOverlayOpen)
                {
                    auto overlayBounds = content.reduced(tokens.spacing);
                    const auto overlayWidth = juce::jlimit(
                        minRight,
                        juce::jmax(minRight, content.getWidth() - tokens.spacing * 8),
                        profile.rightWidth);
                    overlayBounds.setWidth(overlayWidth);
                    overlayBounds.setX(content.getRight() - overlayWidth - tokens.spacing);
                    setBoundsWithTelemetry(panelComponents.rightPanels, overlayBounds, "rightOverlay");
                    panelComponents.rightPanels.setVisible(true);
                    panelComponents.rightPanels.toFront(false);
                }
                else
                {
                    panelComponents.rightPanels.setVisible(false);
                }
            }
            else
            {
                profile.rightOverlayOpen = false;
                if (profile.rightDockVisible)
                {
                    auto rightBounds = canvasBounds.removeFromRight(profile.rightWidth);
                    setBoundsWithTelemetry(panelComponents.rightPanels, rightBounds, "rightDock");
                    panelComponents.rightPanels.setVisible(true);
                }
                else
                {
                    panelComponents.rightPanels.setVisible(false);
                }
            }

            setBoundsWithTelemetry(panelComponents.canvas, canvasBounds, "canvas");
            setBoundsWithTelemetry(panelComponents.historyPanel, historyBounds, "history");
        }

        auto navBounds = panelComponents.canvas.getBounds();
        navBounds.setSize(tokens.navSize, tokens.navSize);
        navBounds.setPosition(panelComponents.canvas.getRight() - tokens.navSize - tokens.spacing * 4,
                              panelComponents.canvas.getBottom() - tokens.navSize - tokens.spacing * 4);
        setBoundsWithTelemetry(panelComponents.navigatorPanel, navBounds, "navigator");

        if (panelComponents.gridSnapPanel.isVisible())
        {
            const auto popupWidth = juce::roundToInt(220.0f * scale);
            const auto popupHeight = juce::roundToInt(170.0f * scale);
            panelComponents.gridSnapPanel.setBounds(toolbarComponents.gridSnapMenuButton.getRight() - popupWidth,
                                                    tokens.toolbarHeight + tokens.spacing,
                                                    popupWidth,
                                                    popupHeight);
        }

        juce::StringArray telemetryTokens;
        for (const auto& [name, elapsedMs] : state.panelLayoutTelemetryMs)
            telemetryTokens.add(name + "=" + juce::String(elapsedMs, 3) + "ms");
        state.panelLayoutSummary = telemetryTokens.joinIntoString(", ");

        if (!state.applyingLayoutForSnapshot && callbacks.saveUiSettings)
            callbacks.saveUiSettings();

        if (!throttledResize)
        {
            if (callbacks.refreshViewDiagnosticsPanels)
                callbacks.refreshViewDiagnosticsPanels();
        }
        else
        {
            const auto updatePending = callbacks.isAsyncUpdatePending
                ? callbacks.isAsyncUpdatePending()
                : false;
            if (!updatePending && callbacks.triggerAsyncUpdate)
                callbacks.triggerAsyncUpdate();
        }

        state.lastLayoutMs = juce::Time::getMillisecondCounterHiRes() - layoutStartMs;
    }

    void EditorLayoutCoordinator::toggleDockVisibility(
        State& state,
        const juce::Component& owner,
        bool leftDock,
        const std::function<void()>& onResized,
        const std::function<void()>& onRepaint,
        const std::function<void()>& onSaveUiSettings) const
    {
        auto& profile = activeLayoutProfile(state, owner);
        const auto rightPopoverMode =
            !leftDock
            && state.breakpointClass != BreakpointClass::narrow
            && state.forceSingleColumnEditorsActive;

        if (state.breakpointClass == BreakpointClass::narrow)
        {
            if (leftDock)
            {
                profile.leftOverlayOpen = !profile.leftOverlayOpen;
                if (profile.leftOverlayOpen)
                    profile.rightOverlayOpen = false;
            }
            else
            {
                profile.rightOverlayOpen = !profile.rightOverlayOpen;
                if (profile.rightOverlayOpen)
                    profile.leftOverlayOpen = false;
            }
        }
        else if (rightPopoverMode)
        {
            profile.rightOverlayOpen = !profile.rightOverlayOpen;
        }
        else
        {
            profile.rightOverlayOpen = false;
            if (leftDock)
                profile.leftDockVisible = !profile.leftDockVisible;
            else
                profile.rightDockVisible = !profile.rightDockVisible;
        }

        if (onResized)
            onResized();
        if (onRepaint)
            onRepaint();
        if (onSaveUiSettings)
            onSaveUiSettings();
    }
}
