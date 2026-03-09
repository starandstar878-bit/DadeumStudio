#include "Teul/Editor/EditorHandleImpl.h"

#include "Teul/Editor/Canvas/TGraphCanvas.h"
#include "Teul/Editor/Panels/DiagnosticsDrawer.h"
#include "Teul/Editor/Panels/NodeLibraryPanel.h"
#include "Teul/Editor/Panels/NodePropertiesPanel.h"
#include "Teul/Registry/TNodeRegistry.h"

namespace Teul {

class RuntimeStatusStrip : public juce::Component {
public:
  void setStats(const TGraphRuntime::RuntimeStats &newStats) {
    stats = newStats;
    repaint();
  }

  void setTransientMessage(const juce::String &text, juce::Colour accent) {
    transientMessage = text;
    transientAccent = accent;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    const juce::Colour frameAccent =
        (transientMessage.isNotEmpty() ? transientAccent : statusAccent())
            .withAlpha(0.30f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xee0b1220),
                                           bounds.getCentreX(), bounds.getY(),
                                           juce::Colour(0xee111827),
                                           bounds.getCentreX(),
                                           bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(frameAccent);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = getLocalBounds().reduced(12, 8);
    auto primaryRow = content.removeFromTop(20);
    auto secondaryRow = content.removeFromTop(16);
    content.removeFromTop(4);
    auto badgeRow = content.removeFromTop(20);

    auto cpuChip = primaryRow.removeFromRight(92);
    const juce::String primaryText = juce::String::formatted(
        "%.1f kHz  |  %d blk  |  %d in / %d out",
        stats.sampleRate * 0.001,
        stats.preparedBlockSize,
        stats.lastInputChannels,
        stats.lastOutputChannels);
    const juce::String summaryText = juce::String::formatted(
        "Gen %llu  |  Nodes %d  |  Buffers %d  |  Process %.2f ms",
        static_cast<unsigned long long>(stats.activeGeneration),
        stats.activeNodeCount,
        stats.allocatedPortChannels,
        stats.lastProcessMilliseconds);

    g.setColour(juce::Colours::white.withAlpha(0.97f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(primaryText, primaryRow, juce::Justification::centredLeft,
               false);

    drawCpuChip(g, cpuChip);

    auto summaryArea = secondaryRow;
    if (transientMessage.isNotEmpty() && secondaryRow.getWidth() > 180) {
      const int messageWidth = juce::jlimit(
          144, juce::jmax(190, secondaryRow.getWidth() / 2),
          30 + transientMessage.length() * 7);
      auto messageArea = summaryArea.removeFromRight(
          juce::jmin(messageWidth, summaryArea.getWidth()));
      summaryArea.removeFromRight(8);
      drawMessageChip(g, messageArea, transientMessage, transientAccent);
    }

    if (summaryArea.getWidth() > 40) {
      g.setColour(juce::Colours::white.withAlpha(0.62f));
      g.setFont(11.0f);
      g.drawText(summaryText, summaryArea, juce::Justification::centredLeft,
                 false);
    }

    int badgeX = badgeRow.getX();
    auto drawBadge = [&](const juce::String &text, juce::Colour colour) {
      if (text.isEmpty())
        return;

      const int badgeWidth = juce::jlimit(66, 168, 22 + text.length() * 7);
      if (badgeX + badgeWidth > badgeRow.getRight())
        return;

      juce::Rectangle<int> badge(badgeX, badgeRow.getY(), badgeWidth,
                                 badgeRow.getHeight());
      badgeX += badgeWidth + 6;

      g.setColour(colour.withAlpha(0.16f));
      g.fillRoundedRectangle(badge.toFloat(), 8.0f);
      g.setColour(colour.withAlpha(0.86f));
      g.drawRoundedRectangle(badge.toFloat(), 8.0f, 1.0f);
      g.setColour(colour.brighter(0.18f));
      g.setFont(10.0f);
      g.drawText(text, badge, juce::Justification::centred, false);
    };

    bool drewBadge = false;
    if (stats.rebuildPending) {
      drawBadge("Deferred Apply", juce::Colour(0xfff59e0b));
      drewBadge = true;
    }
    if (stats.smoothingActiveCount > 0) {
      drawBadge("Smooth " + juce::String(stats.smoothingActiveCount),
                juce::Colour(0xff60a5fa));
      drewBadge = true;
    }
    if (stats.xrunDetected) {
      drawBadge("XRUN", juce::Colour(0xffef4444));
      drewBadge = true;
    }
    if (stats.clipDetected) {
      drawBadge("Clip", juce::Colour(0xfff97316));
      drewBadge = true;
    }
    if (stats.denormalDetected) {
      drawBadge("Denormal", juce::Colour(0xffeab308));
      drewBadge = true;
    }
    if (stats.mutedFallbackActive) {
      drawBadge("Muted Fallback", juce::Colour(0xff94a3b8));
      drewBadge = true;
    }
    if (!drewBadge)
      drawBadge("Stable", juce::Colour(0xff22c55e));
  }

private:
  juce::Colour statusAccent() const {
    if (stats.xrunDetected)
      return juce::Colour(0xffef4444);
    if (stats.clipDetected)
      return juce::Colour(0xfff97316);
    if (stats.denormalDetected)
      return juce::Colour(0xffeab308);
    if (stats.rebuildPending)
      return juce::Colour(0xfff59e0b);
    if (stats.mutedFallbackActive)
      return juce::Colour(0xff94a3b8);
    return juce::Colour(0xff22c55e);
  }

  juce::Colour cpuAccent() const {
    if (stats.xrunDetected || stats.clipDetected)
      return juce::Colour(0xffef4444);
    if (stats.cpuLoadPercent >= 65.0f)
      return juce::Colour(0xfff97316);
    if (stats.cpuLoadPercent >= 35.0f)
      return juce::Colour(0xfff59e0b);
    return juce::Colour(0xff60a5fa);
  }

  void drawCpuChip(juce::Graphics &g, juce::Rectangle<int> area) const {
    const juce::Colour accent = cpuAccent();
    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(area.toFloat(), 9.0f);
    g.setColour(accent.withAlpha(0.90f));
    g.drawRoundedRectangle(area.toFloat(), 9.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.97f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(juce::String::formatted("CPU %.1f%%", stats.cpuLoadPercent),
               area, juce::Justification::centred, false);
  }

  void drawMessageChip(juce::Graphics &g, juce::Rectangle<int> area,
                       const juce::String &text, juce::Colour accent) const {
    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(area.toFloat(), 7.0f);
    g.setColour(accent.withAlpha(0.92f));
    g.drawRoundedRectangle(area.toFloat(), 7.0f, 1.0f);
    g.setColour(accent.brighter(0.18f));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft,
               false);
  }

  TGraphRuntime::RuntimeStats stats;
  juce::String transientMessage;
  juce::Colour transientAccent = juce::Colour(0xff60a5fa);
};
EditorHandle::Impl::Impl(
    EditorHandle &ownerIn, juce::AudioDeviceManager *audioDeviceManagerIn,
    ParamBindingSummaryResolver bindingSummaryResolverIn,
    ParamBindingRevisionProvider bindingRevisionProviderIn)
    : owner(ownerIn), registryStore(makeDefaultNodeRegistry()),
      runtime(registryStore.get()), audioDeviceManager(audioDeviceManagerIn),
      bindingRevisionProvider(std::move(bindingRevisionProviderIn)) {
  canvas = std::make_unique<TGraphCanvas>(doc, *registryStore);
  canvas->setConnectionLevelProvider([this](const TConnection &connection) {
    return runtime.getPortLevel(connection.from.portId);
  });
  canvas->setPortLevelProvider(
      [this](PortId portId) { return runtime.getPortLevel(portId); });
  canvas->setBindingSummaryResolver(bindingSummaryResolverIn);
  canvas->setNodePropertiesRequestHandler(
      [this](NodeId nodeId) { openProperties(nodeId); });
  owner.addAndMakeVisible(*canvas);

  libraryPanel = NodeLibraryPanel::create(
      *registryStore, [this](const juce::String &typeKey) {
        if (canvas != nullptr)
          canvas->addNodeByTypeAtView(typeKey, canvas->getViewCenter(), true);
      });
  owner.addAndMakeVisible(*libraryPanel);

  propertiesPanel = NodePropertiesPanel::create(
      doc, *canvas, *registryStore, &runtime,
      std::move(bindingSummaryResolverIn));
  propertiesPanel->setLayoutChangedCallback([this] { owner.resized(); });
  owner.addAndMakeVisible(*propertiesPanel);

  diagnosticsDrawer = DiagnosticsDrawer::create();
  diagnosticsDrawer->setLayoutChangedCallback([this] { owner.resized(); });
  owner.addAndMakeVisible(*diagnosticsDrawer);
  diagnosticsDrawer->setVisible(false);

  runtimeStatusStrip = std::make_unique<RuntimeStatusStrip>();
  owner.addAndMakeVisible(*runtimeStatusStrip);

  canvas->setNodeSelectionChangedHandler(
      [this](const std::vector<NodeId> &selectedNodeIds) {
        handleSelectionChanged(selectedNodeIds);
      });

  owner.addAndMakeVisible(toggleLibraryButton);
  owner.addAndMakeVisible(quickAddButton);
  owner.addAndMakeVisible(findNodeButton);
  owner.addAndMakeVisible(commandPaletteButton);
  owner.addAndMakeVisible(toggleHeatmapButton);
  owner.addAndMakeVisible(toggleProbeButton);
  owner.addAndMakeVisible(toggleOverlayButton);
  owner.addAndMakeVisible(toggleDiagnosticsButton);

  toggleLibraryButton.setButtonText("Library");
  quickAddButton.setButtonText("Quick Add");
  findNodeButton.setButtonText("Find Node");
  commandPaletteButton.setButtonText("Cmd");
  toggleHeatmapButton.setButtonText("Heat");
  toggleProbeButton.setButtonText("Probe");
  toggleOverlayButton.setButtonText("Overlay");
  toggleDiagnosticsButton.setButtonText("Diagnostics");

  toggleHeatmapButton.setClickingTogglesState(true);
  toggleProbeButton.setClickingTogglesState(true);
  toggleOverlayButton.setClickingTogglesState(true);
  toggleDiagnosticsButton.setClickingTogglesState(true);
  toggleHeatmapButton.setTooltip("Toggle node cost tint and heat rails");
  toggleProbeButton.setTooltip("Toggle node probe rails and selected readouts");
  toggleOverlayButton.setTooltip("Toggle runtime overlay card");
  toggleDiagnosticsButton.setTooltip("Show latest verification and benchmark results");

  toggleLibraryButton.onClick = [this] {
    libraryVisible = !libraryVisible;
    if (libraryPanel != nullptr)
      libraryPanel->setVisible(libraryVisible);
    owner.resized();
  };

  quickAddButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->openQuickAddAt(canvas->getViewCenter());
  };

  findNodeButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->openNodeSearchPrompt();
  };

  commandPaletteButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->openCommandPalette();
  };

  toggleHeatmapButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->setRuntimeHeatmapEnabled(toggleHeatmapButton.getToggleState());
    syncRuntimeViewButtons();
  };

  toggleProbeButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->setLiveProbeEnabled(toggleProbeButton.getToggleState());
    syncRuntimeViewButtons();
  };

  toggleOverlayButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->setDebugOverlayEnabled(toggleOverlayButton.getToggleState());
    syncRuntimeViewButtons();
  };

  toggleDiagnosticsButton.onClick = [this] {
    if (diagnosticsDrawer != nullptr)
      diagnosticsDrawer->setDrawerOpen(toggleDiagnosticsButton.getToggleState());
    syncRuntimeViewButtons();
  };

  syncRuntimeViewButtons();

  rebuildAll(true);

  if (audioDeviceManager != nullptr)
    audioDeviceManager->addAudioCallback(&runtime);

  startTimerHz(20);
}

EditorHandle::Impl::~Impl() {
  stopTimer();

  if (canvas != nullptr) {
    canvas->setNodeSelectionChangedHandler({});
    canvas->setNodePropertiesRequestHandler({});
    canvas->setConnectionLevelProvider({});
    canvas->setPortLevelProvider({});
    canvas->setBindingSummaryResolver({});
  }

  if (propertiesPanel != nullptr) {
    propertiesPanel->setLayoutChangedCallback({});
    propertiesPanel->setParamProvider(nullptr);
  }

  if (diagnosticsDrawer != nullptr)
    diagnosticsDrawer->setLayoutChangedCallback({});

  if (audioDeviceManager != nullptr)
    audioDeviceManager->removeAudioCallback(&runtime);
}

TGraphDocument &EditorHandle::Impl::document() noexcept { return doc; }

const TGraphDocument &EditorHandle::Impl::document() const noexcept {
  return doc;
}

const TNodeRegistry *EditorHandle::Impl::registry() const noexcept {
  return registryStore.get();
}

void EditorHandle::Impl::refreshFromDocument() { rebuildAll(true); }

void EditorHandle::Impl::layout(juce::Rectangle<int> area) {
  auto top = area.removeFromTop(40).reduced(6, 4);

  toggleLibraryButton.setBounds(top.removeFromLeft(78));
  top.removeFromLeft(4);
  quickAddButton.setBounds(top.removeFromLeft(92));
  top.removeFromLeft(4);
  findNodeButton.setBounds(top.removeFromLeft(92));
  top.removeFromLeft(4);
  commandPaletteButton.setBounds(top.removeFromLeft(60));
  top.removeFromLeft(8);
  toggleHeatmapButton.setBounds(top.removeFromLeft(82));
  top.removeFromLeft(4);
  toggleProbeButton.setBounds(top.removeFromLeft(92));
  top.removeFromLeft(4);
  toggleOverlayButton.setBounds(top.removeFromLeft(108));
  top.removeFromLeft(4);
  toggleDiagnosticsButton.setBounds(top.removeFromLeft(118));

  if (runtimeStatusStrip != nullptr) {
    auto statusArea = area.removeFromTop(60).reduced(6, 4);
    runtimeStatusStrip->setBounds(statusArea);
  }

  if (libraryPanel != nullptr) {
    libraryPanel->setVisible(libraryVisible);
    if (libraryVisible) {
      auto left = area.removeFromLeft(244);
      libraryPanel->setBounds(left.reduced(0, 2));
    }
  }

  if (propertiesPanel != nullptr && propertiesPanel->isPanelOpen()) {
    auto right = area.removeFromRight(336);
    propertiesPanel->setBounds(right.reduced(0, 2));
  }

  if (diagnosticsDrawer != nullptr && diagnosticsDrawer->isDrawerOpen()) {
    auto drawerArea = area.removeFromBottom(300).reduced(0, 2);
    diagnosticsDrawer->setBounds(drawerArea);
  }

  if (canvas != nullptr)
    canvas->setBounds(area.reduced(0, 2));
}

void EditorHandle::Impl::timerCallback() {
  const auto currentRuntimeRevision = doc.getRuntimeRevision();
  if (currentRuntimeRevision != lastRuntimeRevision) {
    runtime.buildGraph(doc);
    lastRuntimeRevision = currentRuntimeRevision;
  }

  const auto currentDocumentRevision = doc.getDocumentRevision();
  if (currentDocumentRevision != lastDocumentRevision) {
    if (propertiesPanel != nullptr)
      propertiesPanel->refreshFromDocument();
    lastDocumentRevision = currentDocumentRevision;
  }

  const auto currentBindingRevision =
      bindingRevisionProvider != nullptr ? bindingRevisionProvider() : 0;
  if (currentBindingRevision != lastBindingRevision) {
    if (propertiesPanel != nullptr)
      propertiesPanel->refreshBindingSummaries();
    lastBindingRevision = currentBindingRevision;
  }

  refreshRuntimeUi();
  if (diagnosticsDrawer != nullptr)
    diagnosticsDrawer->refreshArtifacts();
}

void EditorHandle::Impl::rebuildAll(bool rebuildRuntime) {
  if (canvas != nullptr)
    canvas->rebuildNodeComponents();
  if (rebuildRuntime)
    runtime.buildGraph(doc);
  if (propertiesPanel != nullptr) {
    propertiesPanel->setParamProvider(&runtime);
    propertiesPanel->refreshFromDocument();
  }

  lastDocumentRevision = doc.getDocumentRevision();
  lastRuntimeRevision = doc.getRuntimeRevision();
  refreshRuntimeUi(true);
  owner.resized();
}

void EditorHandle::Impl::handleSelectionChanged(
    const std::vector<NodeId> &selectedNodeIds) {
  if (propertiesPanel == nullptr)
    return;

  if (selectedNodeIds.size() == 1)
    inspectNodeWithReveal(selectedNodeIds.front());
  else
    propertiesPanel->hidePanel();
}

void EditorHandle::Impl::openProperties(NodeId nodeId) {
  if (propertiesPanel != nullptr)
    inspectNodeWithReveal(nodeId);
}

void EditorHandle::Impl::inspectNodeWithReveal(NodeId nodeId) {
  if (propertiesPanel == nullptr)
    return;

  const bool wasOpen = propertiesPanel->isPanelOpen();
  propertiesPanel->inspectNode(nodeId);

  if (!wasOpen && canvas != nullptr)
    canvas->ensureNodeVisible(nodeId, 28.0f);
}

void EditorHandle::Impl::refreshRuntimeUi(bool forceMessage) {
  const auto stats = runtime.getRuntimeStats();

  if ((stats.xrunDetected && !lastRuntimeStats.xrunDetected)) {
    pushRuntimeMessage("Audio block exceeded budget", juce::Colour(0xffef4444),
                       72);
  } else if (stats.clipDetected && !lastRuntimeStats.clipDetected) {
    pushRuntimeMessage("Output clip detected", juce::Colour(0xfff97316), 72);
  } else if (stats.denormalDetected && !lastRuntimeStats.denormalDetected) {
    pushRuntimeMessage("Denormal activity detected", juce::Colour(0xffeab308),
                       66);
  } else if (stats.mutedFallbackActive && !lastRuntimeStats.mutedFallbackActive) {
    pushRuntimeMessage("Muted fallback is active", juce::Colour(0xff94a3b8),
                       60);
  } else if (stats.rebuildPending && !lastRuntimeStats.rebuildPending) {
    pushRuntimeMessage("Deferred apply queued for safe commit",
                       juce::Colour(0xfff59e0b), 66);
  } else if (!stats.rebuildPending && lastRuntimeStats.rebuildPending) {
    pushRuntimeMessage("Deferred apply committed", juce::Colour(0xff22c55e),
                       48);
  } else if (forceMessage ||
             stats.sampleRate != lastRuntimeStats.sampleRate ||
             stats.preparedBlockSize != lastRuntimeStats.preparedBlockSize ||
             stats.lastInputChannels != lastRuntimeStats.lastInputChannels ||
             stats.lastOutputChannels != lastRuntimeStats.lastOutputChannels) {
    pushRuntimeMessage(
        juce::String::formatted(
            "Runtime prepared: %.1f kHz / %d blk / %d in / %d out",
            stats.sampleRate * 0.001, stats.preparedBlockSize,
            stats.lastInputChannels, stats.lastOutputChannels),
        juce::Colour(0xff60a5fa), 42);
  }

  if (runtimeMessageTicksRemaining > 0) {
    --runtimeMessageTicksRemaining;
  } else if (runtimeMessageText.isNotEmpty()) {
    runtimeMessageText.clear();
  }

  if (runtimeStatusStrip != nullptr) {
    runtimeStatusStrip->setStats(stats);
    runtimeStatusStrip->setTransientMessage(runtimeMessageText,
                                            runtimeMessageAccent);
  }

  syncRuntimeViewButtons();
  if (canvas != nullptr) {
    TGraphCanvas::RuntimeOverlayState overlay;
    overlay.sampleRate = stats.sampleRate;
    overlay.blockSize = stats.preparedBlockSize;
    overlay.inputChannels = stats.lastInputChannels;
    overlay.outputChannels = stats.lastOutputChannels;
    overlay.activeNodeCount = stats.activeNodeCount;
    overlay.allocatedPortChannels = stats.allocatedPortChannels;
    overlay.smoothingActiveCount = stats.smoothingActiveCount;
    overlay.activeGeneration = stats.activeGeneration;
    overlay.pendingGeneration = stats.pendingGeneration;
    overlay.rebuildPending = stats.rebuildPending;
    overlay.clipDetected = stats.clipDetected;
    overlay.denormalDetected = stats.denormalDetected;
    overlay.xrunDetected = stats.xrunDetected;
    overlay.mutedFallbackActive = stats.mutedFallbackActive;
    overlay.cpuLoadPercent = stats.cpuLoadPercent;
    canvas->setRuntimeOverlayState(overlay);
  }

  lastRuntimeStats = stats;
}
void EditorHandle::Impl::syncRuntimeViewButtons() {
  auto syncButton = [](juce::TextButton &button, bool enabled,
                       juce::Colour accent, const juce::String &onText,
                       const juce::String &offText) {
    button.setToggleState(enabled, juce::dontSendNotification);
    button.setButtonText(enabled ? onText : offText);
    button.setColour(juce::TextButton::buttonOnColourId,
                     accent.withAlpha(0.92f));
    button.setColour(juce::TextButton::buttonColourId,
                     enabled ? accent.withAlpha(0.58f)
                             : juce::Colour(0xff111827));
    button.setColour(juce::TextButton::textColourOnId,
                     juce::Colours::white.withAlpha(0.99f));
    button.setColour(juce::TextButton::textColourOffId,
                     enabled ? juce::Colours::white.withAlpha(0.97f)
                             : juce::Colours::white.withAlpha(0.78f));
  };

  if (canvas != nullptr) {
    syncButton(toggleHeatmapButton, canvas->isRuntimeHeatmapEnabled(),
               juce::Colour(0xfff97316), "Heat On", "Heat");
    syncButton(toggleProbeButton, canvas->isLiveProbeEnabled(),
               juce::Colour(0xff60a5fa), "Probe On", "Probe");
    syncButton(toggleOverlayButton, canvas->isDebugOverlayEnabled(),
               juce::Colour(0xff22c55e), "Overlay On", "Overlay");
  }

  syncButton(toggleDiagnosticsButton,
             diagnosticsDrawer != nullptr && diagnosticsDrawer->isDrawerOpen(),
             juce::Colour(0xff38bdf8), "Diagnostics On", "Diagnostics");
}
void EditorHandle::Impl::pushRuntimeMessage(const juce::String &text,
                                            juce::Colour accent,
                                            int ticks) {
  runtimeMessageText = text;
  runtimeMessageAccent = accent;
  runtimeMessageTicksRemaining = juce::jmax(1, ticks);
}

} // namespace Teul
