#include "Teul/Editor/EditorHandleImpl.h"

#include "Teul/Editor/Canvas/TGraphCanvas.h"
#include "Teul/Editor/Panels/DiagnosticsDrawer.h"
#include "Teul/Editor/Panels/NodeLibraryPanel.h"
#include "Teul/Editor/Panels/NodePropertiesPanel.h"
#include "Teul/Editor/Panels/PresetBrowserPanel.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/Serialization/TFileIo.h"
#include "Teul/Serialization/TStatePresetIO.h"

namespace Teul {

namespace {

juce::String teulGraphDisplayName(const TGraphDocument &document) {
  return document.meta.name.isNotEmpty() ? document.meta.name
                                        : juce::String("Untitled");
}

juce::String formatRecoveryCountDiff(const juce::String &label, int currentCount,
                                     int snapshotCount) {
  const int delta = snapshotCount - currentCount;
  juce::String text = label + " " + juce::String(currentCount) + " -> " +
                      juce::String(snapshotCount);
  if (delta > 0)
    text << " (+" << juce::String(delta) << ")";
  else if (delta < 0)
    text << " (" << juce::String(delta) << ")";
  else
    text << " (=)";
  return text;
}

juce::Result buildRecoveryDiffPreview(const TGraphDocument &currentDocument,
                                      const juce::File &recoveryFile,
                                      juce::String &summaryText,
                                      juce::String &detailText,
                                      bool &warning) {
  TGraphDocument recoveryDocument;
  TSchemaMigrationReport migrationReport;
  if (!TFileIo::loadFromFile(recoveryDocument, recoveryFile, &migrationReport))
    return juce::Result::fail(
        "Recovery preview failed: autosave snapshot could not be loaded.");

  const auto currentName = teulGraphDisplayName(currentDocument);
  const auto recoveryName = teulGraphDisplayName(recoveryDocument);
  const bool nameChanged = currentName != recoveryName;
  const bool nodeCountChanged =
      currentDocument.nodes.size() != recoveryDocument.nodes.size();
  const bool connectionCountChanged =
      currentDocument.connections.size() != recoveryDocument.connections.size();
  const bool frameCountChanged =
      currentDocument.frames.size() != recoveryDocument.frames.size();

  summaryText = "Recovery Diff Preview";

  juce::StringArray parts;
  parts.add(formatRecoveryCountDiff("Nodes", (int)currentDocument.nodes.size(),
                                    (int)recoveryDocument.nodes.size()));
  parts.add(formatRecoveryCountDiff(
      "Wires", (int)currentDocument.connections.size(),
      (int)recoveryDocument.connections.size()));
  if (!currentDocument.frames.empty() || !recoveryDocument.frames.empty()) {
    parts.add(formatRecoveryCountDiff("Frames",
                                      (int)currentDocument.frames.size(),
                                      (int)recoveryDocument.frames.size()));
  }

  detailText = parts.joinIntoString("  |  ");
  if (nameChanged)
    detailText << "\r\nGraph: " << currentName << " -> " << recoveryName;
  else
    detailText << "\r\nGraph: " << currentName;

  juce::StringArray compatibilityParts;
  if (migrationReport.migrated)
    compatibilityParts.add("migrated");
  if (migrationReport.usedLegacyAliases)
    compatibilityParts.add("legacy aliases");
  if (migrationReport.degraded)
    compatibilityParts.add("degraded");
  if (!compatibilityParts.isEmpty())
    detailText << "\r\nCompatibility: "
               << compatibilityParts.joinIntoString(" | ");

  if (!migrationReport.warnings.isEmpty())
    detailText << "\r\nWarnings: "
               << migrationReport.warnings.joinIntoString(" | ");

  if (!nameChanged && !nodeCountChanged && !connectionCountChanged &&
      !frameCountChanged && !migrationReport.degraded &&
      migrationReport.warnings.isEmpty()) {
    detailText << "\r\nStructure matches the current document.";
  }

  warning = nameChanged || nodeCountChanged || connectionCountChanged ||
            frameCountChanged || migrationReport.migrated ||
            migrationReport.degraded || !migrationReport.warnings.isEmpty();
  return juce::Result::ok();
}

} // namespace

class RuntimeStatusStrip : public juce::Component {
public:
  void setStats(const TGraphRuntime::RuntimeStats &newStats) {
    stats = newStats;
    repaint();
  }

  void setSessionStatus(const TEditorSessionStatus &newStatus,
                        bool dirtyState) {
    sessionStatus = newStatus;
    dirty = dirtyState;
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

    drawBadge(dirty ? "Dirty" : "Saved",
              dirty ? juce::Colour(0xfff59e0b) : juce::Colour(0xff64748b));
    if (sessionStatus.hasAutosaveSnapshot) {
      const auto timeLabel = sessionStatus.lastAutosaveTime.toMilliseconds() > 0
                                 ? sessionStatus.lastAutosaveTime.formatted("%H:%M")
                                 : juce::String("--:--");
      drawBadge("Autosave " + timeLabel, juce::Colour(0xff38bdf8));
    }
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
  TEditorSessionStatus sessionStatus;
  bool dirty = false;
  juce::String transientMessage;
  juce::Colour transientAccent = juce::Colour(0xff60a5fa);
};

class DocumentNoticeBanner : public juce::Component {
public:
  DocumentNoticeBanner() {
    addAndMakeVisible(dismissButton);
    dismissButton.setButtonText("Dismiss");
    dismissButton.onClick = [this] {
      if (dismissHandler != nullptr)
        dismissHandler();
    };
    setVisible(false);
  }

  void setDismissHandler(std::function<void()> handler) {
    dismissHandler = std::move(handler);
  }

  void setNotice(const TDocumentNotice &newNotice) {
    notice = newNotice;
    setVisible(notice.active);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    if (!notice.active)
      return;

    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    const auto accent = accentForLevel(notice.level);
    g.setColour(juce::Colour(0xf4121826));
    g.fillRoundedRectangle(bounds, 11.0f);
    g.setColour(accent.withAlpha(0.85f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 11.0f, 1.0f);

    auto textArea = getLocalBounds().reduced(14, 9);
    textArea.removeFromRight(92);

    g.setColour(juce::Colours::white.withAlpha(0.96f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(notice.title, textArea.removeFromTop(20),
               juce::Justification::centredLeft, false);

    if (notice.detail.isNotEmpty()) {
      g.setColour(juce::Colours::white.withAlpha(0.72f));
      g.setFont(11.0f);
      g.drawFittedText(notice.detail, textArea, juce::Justification::topLeft,
                       2, 0.92f);
    }
  }

  void resized() override {
    dismissButton.setBounds(getLocalBounds().removeFromRight(88).reduced(10, 10));
  }

private:
  static juce::Colour accentForLevel(TDocumentNoticeLevel level) {
    switch (level) {
    case TDocumentNoticeLevel::degraded:
      return juce::Colour(0xffef4444);
    case TDocumentNoticeLevel::warning:
      return juce::Colour(0xfff59e0b);
    case TDocumentNoticeLevel::info:
      return juce::Colour(0xff38bdf8);
    }

    return juce::Colour(0xff38bdf8);
  }

  TDocumentNotice notice;
  juce::TextButton dismissButton;
  std::function<void()> dismissHandler;
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
  diagnosticsDrawer->setFocusRequestHandler(
      [this](const juce::String &graphId, const juce::String &query) {
        return focusDiagnosticTarget(graphId, query);
      });
  owner.addAndMakeVisible(*diagnosticsDrawer);
  diagnosticsDrawer->setVisible(false);

  presetBrowserPanel = PresetBrowserPanel::create();
  presetBrowserPanel->setLayoutChangedCallback([this] { owner.resized(); });
  presetBrowserPanel->setPrimaryActionHandler(
      [this](const TPresetEntry &entry) -> juce::Result {
        if (canvas == nullptr)
          return juce::Result::fail("Preset action failed: canvas unavailable.");

        if (entry.presetKind == "teul.patch") {
          const auto result =
              canvas->insertPatchPresetFromFile(entry.file, canvas->getViewCenter());
          if (result.wasOk())
            pushRuntimeMessage("Patch preset inserted", juce::Colour(0xff22c55e),
                               44);
          return result;
        }

        if (entry.presetKind == "teul.state") {
          const auto result = canvas->applyStatePresetFromFile(entry.file);
          if (result.wasOk())
            pushRuntimeMessage("State preset applied", juce::Colour(0xff38bdf8),
                               44);
          return result;
        }

        if (entry.presetKind == "teul.recovery") {
          TSchemaMigrationReport migrationReport;
          if (!TFileIo::loadFromFile(doc, entry.file, &migrationReport)) {
            return juce::Result::fail(
                "Recovery restore failed: autosave snapshot could not be loaded.");
          }

          rebuildAll(true);
          pushRuntimeMessage("Autosave snapshot restored",
                             migrationReport.degraded
                                 ? juce::Colour(0xfff59e0b)
                                 : juce::Colour(0xff22c55e),
                             56);
          return juce::Result::ok();
        }

        return juce::Result::fail(
            "Preset action failed: unsupported preset kind.");
      });
  presetBrowserPanel->setSecondaryActionHandler(
      [this](const TPresetEntry &entry) -> juce::Result {
        if (entry.presetKind != "teul.recovery")
          return juce::Result::fail(
              "Preset action failed: no secondary action available.");

        if (!entry.file.existsAsFile())
          return juce::Result::fail(
              "Recovery discard failed: autosave snapshot file was not found.");

        const auto stateFile =
            entry.file.getParentDirectory().getChildFile("autosave-session-state.json");
        if (!entry.file.deleteFile())
          return juce::Result::fail(
              "Recovery discard failed: autosave snapshot file could not be removed.");
        if (stateFile.existsAsFile() && !stateFile.deleteFile())
          return juce::Result::fail(
              "Recovery discard failed: session marker file could not be removed.");

        sessionStatus.hasAutosaveSnapshot = false;
        sessionStatus.lastAutosaveTime = {};
        refreshSessionStatusUi(true);
        pushRuntimeMessage("Autosave snapshot discarded",
                           juce::Colour(0xff94a3b8), 44);
        return juce::Result::ok();
      });
  presetBrowserPanel->setEntryPreviewHandler(
      [this](const TPresetEntry &entry, juce::String &summaryText,
             juce::String &detailText, bool &warning) {
        if (entry.presetKind == "teul.recovery") {
          const auto result = buildRecoveryDiffPreview(doc, entry.file, summaryText,
                                                       detailText, warning);
          if (result.failed()) {
            summaryText = "Recovery Diff Preview Unavailable";
            detailText = result.getErrorMessage();
            warning = true;
          }
          return;
        }

        if (entry.presetKind != "teul.state")
          return;

        TStatePresetDiffPreview preview;
        const auto result =
            TStatePresetIO::previewAgainstDocument(doc, entry.file, &preview);
        if (result.failed()) {
          summaryText = "State Diff Preview Unavailable";
          detailText = result.getErrorMessage();
          warning = true;
          return;
        }

        summaryText = "State Diff Preview";
        juce::StringArray parts;
        parts.add(juce::String(preview.changedNodeCount) + " changed nodes");
        parts.add(juce::String(preview.changedParamValueCount) + " param deltas");
        if (preview.changedBypassCount > 0)
          parts.add(juce::String(preview.changedBypassCount) + " bypass deltas");
        if (preview.missingNodeCount > 0)
          parts.add(juce::String(preview.missingNodeCount) + " missing nodes");

        detailText = parts.joinIntoString("  |  ");
        if (!preview.changedNodeLabels.isEmpty()) {
          auto names = preview.changedNodeLabels;
          if (names.size() > 4)
            names.removeRange(4, names.size() - 4);
          detailText << "\r\nTargets: " << names.joinIntoString(", ");
          if (preview.changedNodeLabels.size() > names.size())
            detailText << " +" << juce::String(preview.changedNodeLabels.size() - names.size())
                       << " more";
        }
        if (!preview.warnings.isEmpty())
          detailText << "\r\nWarnings: " << preview.warnings.joinIntoString(" | ");
        warning = preview.degraded || preview.missingNodeCount > 0;
      });
  owner.addAndMakeVisible(*presetBrowserPanel);
  presetBrowserPanel->setVisible(false);
  presetBrowserPanel->setSessionPreview("Session: Saved",
                                        "Waiting for the first autosave snapshot.",
                                        false);

  runtimeStatusStrip = std::make_unique<RuntimeStatusStrip>();
  owner.addAndMakeVisible(*runtimeStatusStrip);

  documentNoticeBanner = std::make_unique<DocumentNoticeBanner>();
  documentNoticeBanner->setDismissHandler([this] {
    doc.clearTransientNotice();
    refreshDocumentNoticeUi(true);
  });
  owner.addAndMakeVisible(*documentNoticeBanner);
  documentNoticeBanner->setVisible(false);

  canvas->setNodeSelectionChangedHandler(
      [this](const std::vector<NodeId> &selectedNodeIds) {
        handleSelectionChanged(selectedNodeIds);
      });
  canvas->setFrameSelectionChangedHandler(
      [this](int frameId) { handleFrameSelectionChanged(frameId); });

  owner.addAndMakeVisible(toggleLibraryButton);
  owner.addAndMakeVisible(quickAddButton);
  owner.addAndMakeVisible(findNodeButton);
  owner.addAndMakeVisible(commandPaletteButton);
  owner.addAndMakeVisible(toggleHeatmapButton);
  owner.addAndMakeVisible(toggleProbeButton);
  owner.addAndMakeVisible(toggleOverlayButton);
  owner.addAndMakeVisible(toggleDiagnosticsButton);
  owner.addAndMakeVisible(togglePresetsButton);

  toggleLibraryButton.setButtonText("Library");
  quickAddButton.setButtonText("Quick Add");
  findNodeButton.setButtonText("Find Node");
  commandPaletteButton.setButtonText("Cmd");
  toggleHeatmapButton.setButtonText("Heat");
  toggleProbeButton.setButtonText("Probe");
  toggleOverlayButton.setButtonText("Overlay");
  toggleDiagnosticsButton.setButtonText("Diagnostics");
  togglePresetsButton.setButtonText("Presets");

  toggleHeatmapButton.setClickingTogglesState(true);
  toggleProbeButton.setClickingTogglesState(true);
  toggleOverlayButton.setClickingTogglesState(true);
  toggleDiagnosticsButton.setClickingTogglesState(true);
  togglePresetsButton.setClickingTogglesState(true);
  toggleHeatmapButton.setTooltip("Toggle node cost tint and heat rails");
  toggleProbeButton.setTooltip("Toggle node probe rails and selected readouts");
  toggleOverlayButton.setTooltip("Toggle runtime overlay card");
  toggleDiagnosticsButton.setTooltip("Show latest verification and benchmark results");
  togglePresetsButton.setTooltip(
      "Browse reusable presets and insert or apply them");

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
    if (toggleDiagnosticsButton.getToggleState() && presetBrowserPanel != nullptr)
      presetBrowserPanel->setBrowserOpen(false);
    if (diagnosticsDrawer != nullptr) {
      diagnosticsDrawer->setDrawerOpen(toggleDiagnosticsButton.getToggleState());
      if (toggleDiagnosticsButton.getToggleState())
        diagnosticsDrawer->refreshArtifacts(true);
    }
    syncRuntimeViewButtons();
  };

  togglePresetsButton.onClick = [this] {
    if (togglePresetsButton.getToggleState() && diagnosticsDrawer != nullptr)
      diagnosticsDrawer->setDrawerOpen(false);
    if (presetBrowserPanel != nullptr) {
      if (togglePresetsButton.getToggleState())
        presetBrowserPanel->refreshEntries(true);
      presetBrowserPanel->setBrowserOpen(togglePresetsButton.getToggleState());
    }
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
    canvas->setFrameSelectionChangedHandler({});
    canvas->setNodePropertiesRequestHandler({});
    canvas->setConnectionLevelProvider({});
    canvas->setPortLevelProvider({});
    canvas->setBindingSummaryResolver({});
  }

  if (propertiesPanel != nullptr) {
    propertiesPanel->setLayoutChangedCallback({});
    propertiesPanel->setParamProvider(nullptr);
  }

  if (diagnosticsDrawer != nullptr) {
    diagnosticsDrawer->setFocusRequestHandler({});
    diagnosticsDrawer->setLayoutChangedCallback({});
  }

  if (presetBrowserPanel != nullptr) {
    presetBrowserPanel->setPrimaryActionHandler({});
    presetBrowserPanel->setSecondaryActionHandler({});
    presetBrowserPanel->setEntryPreviewHandler({});
    presetBrowserPanel->setLayoutChangedCallback({});
  }

  if (documentNoticeBanner != nullptr)
    documentNoticeBanner->setDismissHandler({});

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

void EditorHandle::Impl::setSessionStatus(const TEditorSessionStatus &status) {
  sessionStatus = status;
  refreshSessionStatusUi(true);
}

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
  top.removeFromLeft(4);
  togglePresetsButton.setBounds(top.removeFromLeft(96));

  if (runtimeStatusStrip != nullptr) {
    auto statusArea = area.removeFromTop(60).reduced(6, 4);
    runtimeStatusStrip->setBounds(statusArea);
  }

  if (documentNoticeBanner != nullptr) {
    if (documentNoticeBanner->isVisible()) {
      auto bannerArea = area.removeFromTop(56).reduced(6, 3);
      documentNoticeBanner->setBounds(bannerArea);
    } else {
      documentNoticeBanner->setBounds({});
    }
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

  if (presetBrowserPanel != nullptr && presetBrowserPanel->isBrowserOpen()) {
    auto drawerArea = area.removeFromBottom(548).reduced(0, 2);
    presetBrowserPanel->setBounds(drawerArea);
  } else if (diagnosticsDrawer != nullptr && diagnosticsDrawer->isDrawerOpen()) {
    auto drawerArea = area.removeFromBottom(654).reduced(0, 2);
    diagnosticsDrawer->setBounds(drawerArea);
  }

  if (presetBrowserPanel != nullptr && !presetBrowserPanel->isBrowserOpen())
    presetBrowserPanel->setBounds({});
  if (diagnosticsDrawer != nullptr && !diagnosticsDrawer->isDrawerOpen())
    diagnosticsDrawer->setBounds({});

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
  refreshDocumentNoticeUi();
  refreshSessionStatusUi();
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
  refreshDocumentNoticeUi(true);
  refreshSessionStatusUi(true);
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

void EditorHandle::Impl::handleFrameSelectionChanged(int frameId) {
  if (propertiesPanel == nullptr)
    return;

  if (frameId > 0) {
    propertiesPanel->inspectFrame(frameId);
    return;
  }

  if (canvas == nullptr || canvas->getSelectedNodeIds().empty())
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

bool EditorHandle::Impl::focusDiagnosticTarget(const juce::String &graphId,
                                               const juce::String &query) {
  juce::ignoreUnused(graphId);
  if (canvas == nullptr)
    return false;

  return canvas->focusNodeByQuery(query);
}

void EditorHandle::Impl::refreshDocumentNoticeUi(bool force) {
  if (documentNoticeBanner == nullptr)
    return;

  const auto noticeRevision = doc.getTransientNoticeRevision();
  if (!force && noticeRevision == lastDocumentNoticeRevision)
    return;

  const bool wasVisible = documentNoticeBanner->isVisible();
  documentNoticeBanner->setNotice(doc.getTransientNotice());
  lastDocumentNoticeRevision = noticeRevision;

  if (wasVisible != documentNoticeBanner->isVisible())
    owner.resized();
}

void EditorHandle::Impl::refreshSessionStatusUi(bool force) {
  const bool dirty =
      doc.getDocumentRevision() != sessionStatus.lastPersistedDocumentRevision;
  const auto autosaveMillis = sessionStatus.lastAutosaveTime.toMilliseconds();
  if (!force && dirty == lastSessionDirty &&
      sessionStatus.hasAutosaveSnapshot == lastSessionHasAutosaveSnapshot &&
      autosaveMillis == lastSessionAutosaveMillis) {
    return;
  }

  if (runtimeStatusStrip != nullptr)
    runtimeStatusStrip->setSessionStatus(sessionStatus, dirty);

  if (presetBrowserPanel != nullptr) {
    juce::String summary;
    juce::String detail;
    if (dirty) {
      summary = "Session: Dirty";
      detail = sessionStatus.hasAutosaveSnapshot
                   ? "Latest autosave " +
                         (autosaveMillis > 0
                              ? sessionStatus.lastAutosaveTime.formatted(
                                    "%Y-%m-%d %H:%M")
                              : juce::String("time unavailable"))
                   : juce::String("No autosave snapshot has been written yet.");
    } else {
      summary = "Session: Saved";
      detail = sessionStatus.hasAutosaveSnapshot
                   ? "Autosave is up to date as of " +
                         (autosaveMillis > 0
                              ? sessionStatus.lastAutosaveTime.formatted(
                                    "%Y-%m-%d %H:%M")
                              : juce::String("time unavailable"))
                   : juce::String("Waiting for the first autosave snapshot.");
    }

    if (sessionStatus.hasAutosaveSnapshot)
      detail << " | Use Recovery to inspect or discard the snapshot.";

    presetBrowserPanel->setSessionPreview(summary, detail, dirty);
  }

  lastSessionDirty = dirty;
  lastSessionHasAutosaveSnapshot = sessionStatus.hasAutosaveSnapshot;
  lastSessionAutosaveMillis = autosaveMillis;
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
  syncButton(togglePresetsButton,
             presetBrowserPanel != nullptr && presetBrowserPanel->isBrowserOpen(),
             juce::Colour(0xff8b5cf6), "Presets On", "Presets");
}
void EditorHandle::Impl::pushRuntimeMessage(const juce::String &text,
                                            juce::Colour accent,
                                            int ticks) {
  runtimeMessageText = text;
  runtimeMessageAccent = accent;
  runtimeMessageTicksRemaining = juce::jmax(1, ticks);
}

} // namespace Teul
