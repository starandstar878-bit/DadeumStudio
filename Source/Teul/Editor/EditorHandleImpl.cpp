#include "Teul/Editor/EditorHandleImpl.h"

#include "Teul/Editor/Canvas/TGraphCanvas.h"
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
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
      return;

    g.setColour(juce::Colour(0xdd0b1220));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    g.setColour(juce::Colour(0x4460a5fa));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 1.0f);

    auto content = bounds.reduced(10, 7);
    auto header = content.removeFromTop(15);
    auto summary = content.removeFromTop(14);
    auto badges = content.removeFromTop(16);

    const juce::String headerText = juce::String::formatted(
        "%.1f kHz  |  %d blk  |  %d in / %d out  |  CPU %.1f%%",
        stats.sampleRate * 0.001,
        stats.preparedBlockSize,
        stats.lastInputChannels,
        stats.lastOutputChannels,
        stats.cpuLoadPercent);

    juce::String summaryText = juce::String::formatted(
        "Gen %llu  |  Nodes %d  |  Buffers %d  |  Process %.2f ms",
        static_cast<unsigned long long>(stats.activeGeneration),
        stats.activeNodeCount,
        stats.allocatedPortChannels,
        stats.lastProcessMilliseconds);
    juce::Colour summaryColour = juce::Colours::white.withAlpha(0.62f);

    if (transientMessage.isNotEmpty()) {
      summaryText = transientMessage;
      summaryColour = transientAccent;
    }

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    g.drawText(headerText, header, juce::Justification::centredLeft, false);

    g.setColour(summaryColour);
    g.setFont(11.0f);
    g.drawText(summaryText, summary, juce::Justification::centredLeft, false);

    int badgeX = badges.getX();
    auto drawBadge = [&](const juce::String &text, juce::Colour colour) {
      if (text.isEmpty())
        return;

      const int badgeWidth = juce::jlimit(60, 150, 18 + text.length() * 7);
      if (badgeX + badgeWidth > badges.getRight())
        return;

      juce::Rectangle<int> badge(badgeX, badges.getY(), badgeWidth,
                                 badges.getHeight());
      badgeX += badgeWidth + 6;

      g.setColour(colour.withAlpha(0.18f));
      g.fillRoundedRectangle(badge.toFloat(), 7.0f);
      g.setColour(colour.withAlpha(0.88f));
      g.drawRoundedRectangle(badge.toFloat(), 7.0f, 1.0f);
      g.setColour(colour.brighter(0.15f));
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
  canvas = std::make_unique<TGraphCanvas>(doc);
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

  toggleLibraryButton.setButtonText("Library");
  quickAddButton.setButtonText("Quick Add");
  findNodeButton.setButtonText("Find Node");
  commandPaletteButton.setButtonText("Cmd");
  toggleHeatmapButton.setButtonText("Heat");
  toggleProbeButton.setButtonText("Probe");
  toggleOverlayButton.setButtonText("Overlay");

  toggleHeatmapButton.setClickingTogglesState(true);
  toggleProbeButton.setClickingTogglesState(true);
  toggleOverlayButton.setClickingTogglesState(true);
  toggleHeatmapButton.setTooltip("Toggle node heatmap tint");
  toggleProbeButton.setTooltip("Toggle live probe strips on selected nodes");
  toggleOverlayButton.setTooltip("Toggle runtime overlay card");

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
  auto top = area.removeFromTop(36).reduced(6, 4);

  toggleLibraryButton.setBounds(top.removeFromLeft(80));
  top.removeFromLeft(4);
  quickAddButton.setBounds(top.removeFromLeft(90));
  top.removeFromLeft(4);
  findNodeButton.setBounds(top.removeFromLeft(90));
  top.removeFromLeft(4);
  commandPaletteButton.setBounds(top.removeFromLeft(60));
  top.removeFromLeft(8);
  toggleHeatmapButton.setBounds(top.removeFromLeft(64));
  top.removeFromLeft(4);
  toggleProbeButton.setBounds(top.removeFromLeft(72));
  top.removeFromLeft(4);
  toggleOverlayButton.setBounds(top.removeFromLeft(84));

  if (runtimeStatusStrip != nullptr) {
    auto statusArea = area.removeFromTop(48).reduced(6, 4);
    runtimeStatusStrip->setBounds(statusArea);
  }

  if (libraryPanel != nullptr) {
    libraryPanel->setVisible(libraryVisible);
    if (libraryVisible) {
      auto left = area.removeFromLeft(276);
      libraryPanel->setBounds(left);
    }
  }

  if (propertiesPanel != nullptr && propertiesPanel->isPanelOpen()) {
    auto right = area.removeFromRight(316);
    propertiesPanel->setBounds(right);
  }

  if (canvas != nullptr)
    canvas->setBounds(area);
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
    propertiesPanel->inspectNode(selectedNodeIds.front());
  else
    propertiesPanel->hidePanel();
}

void EditorHandle::Impl::openProperties(NodeId nodeId) {
  if (propertiesPanel != nullptr)
    propertiesPanel->inspectNode(nodeId);
}

void EditorHandle::Impl::refreshRuntimeUi(bool forceMessage) {
  const auto stats = runtime.getRuntimeStats();

  if ((stats.xrunDetected && !lastRuntimeStats.xrunDetected)) {
    pushRuntimeMessage("Audio block exceeded budget", juce::Colour(0xffef4444),
                       60);
  } else if (stats.clipDetected && !lastRuntimeStats.clipDetected) {
    pushRuntimeMessage("Output clip detected", juce::Colour(0xfff97316), 60);
  } else if (stats.denormalDetected && !lastRuntimeStats.denormalDetected) {
    pushRuntimeMessage("Denormal activity detected", juce::Colour(0xffeab308),
                       60);
  } else if (stats.mutedFallbackActive && !lastRuntimeStats.mutedFallbackActive) {
    pushRuntimeMessage("Muted fallback is active", juce::Colour(0xff94a3b8),
                       50);
  } else if (stats.rebuildPending && !lastRuntimeStats.rebuildPending) {
    pushRuntimeMessage("Deferred apply queued for safe commit",
                       juce::Colour(0xfff59e0b), 55);
  } else if (!stats.rebuildPending && lastRuntimeStats.rebuildPending) {
    pushRuntimeMessage("Deferred apply committed", juce::Colour(0xff22c55e),
                       40);
  } else if (forceMessage ||
             stats.sampleRate != lastRuntimeStats.sampleRate ||
             stats.preparedBlockSize != lastRuntimeStats.preparedBlockSize ||
             stats.lastInputChannels != lastRuntimeStats.lastInputChannels ||
             stats.lastOutputChannels != lastRuntimeStats.lastOutputChannels) {
    pushRuntimeMessage(
        juce::String::formatted("Runtime prepared: %.1f kHz / %d blk / %d in / %d out",
                                stats.sampleRate * 0.001,
                                stats.preparedBlockSize,
                                stats.lastInputChannels,
                                stats.lastOutputChannels),
        juce::Colour(0xff60a5fa), 36);
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
  if (canvas == nullptr)
    return;

  auto syncButton = [](juce::TextButton &button, bool enabled,
                       juce::Colour accent) {
    button.setToggleState(enabled, juce::dontSendNotification);
    button.setColour(juce::TextButton::buttonOnColourId,
                     accent.withAlpha(0.75f));
    button.setColour(juce::TextButton::buttonColourId,
                     enabled ? accent.withAlpha(0.30f)
                             : juce::Colour(0xff1f2937));
    button.setColour(juce::TextButton::textColourOnId,
                     juce::Colours::white.withAlpha(0.98f));
    button.setColour(juce::TextButton::textColourOffId,
                     enabled ? juce::Colours::white.withAlpha(0.95f)
                             : juce::Colours::white.withAlpha(0.75f));
  };

  syncButton(toggleHeatmapButton, canvas->isRuntimeHeatmapEnabled(),
             juce::Colour(0xfff97316));
  syncButton(toggleProbeButton, canvas->isLiveProbeEnabled(),
             juce::Colour(0xff60a5fa));
  syncButton(toggleOverlayButton, canvas->isDebugOverlayEnabled(),
             juce::Colour(0xff22c55e));
}

void EditorHandle::Impl::pushRuntimeMessage(const juce::String &text,
                                            juce::Colour accent,
                                            int ticks) {
  runtimeMessageText = text;
  runtimeMessageAccent = accent;
  runtimeMessageTicksRemaining = juce::jmax(1, ticks);
}

} // namespace Teul
