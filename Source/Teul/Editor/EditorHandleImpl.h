#pragma once

#include "Teul/Public/EditorHandle.h"
#include "Teul/Model/TGraphDocument.h"
#include "Teul/Runtime/TGraphRuntime.h"

#include <memory>
#include <vector>

namespace Teul {

class TGraphCanvas;
class TNodeRegistry;
class NodeLibraryPanel;
class NodePropertiesPanel;
class DiagnosticsDrawer;
class PresetBrowserPanel;
class RuntimeStatusStrip;
class DocumentNoticeBanner;

struct EditorHandle::Impl : private juce::Timer {
  explicit Impl(EditorHandle &owner,
                juce::AudioDeviceManager *audioDeviceManager,
                ParamBindingSummaryResolver bindingSummaryResolver,
                ParamBindingRevisionProvider bindingRevisionProvider);
  ~Impl() override;

  TGraphDocument &document() noexcept;
  const TGraphDocument &document() const noexcept;
  const TNodeRegistry *registry() const noexcept;
  void refreshFromDocument();
  void setSessionStatus(const TEditorSessionStatus &status);
  void layout(juce::Rectangle<int> area);

private:
  void timerCallback() override;
  void rebuildAll(bool rebuildRuntime);
  void handleSelectionChanged(const std::vector<NodeId> &selectedNodeIds);
  void handleFrameSelectionChanged(int frameId);
  void openProperties(NodeId nodeId);
  void inspectNodeWithReveal(NodeId nodeId);
  bool focusDiagnosticTarget(const juce::String &graphId,
                             const juce::String &query);
  void refreshRuntimeUi(bool forceMessage = false);
  void refreshDocumentNoticeUi(bool force = false);
  void refreshSessionStatusUi(bool force = false);
  void syncRuntimeViewButtons();
  void pushRuntimeMessage(const juce::String &text,
                          juce::Colour accent,
                          int ticks = 50);

  EditorHandle &owner;
  TGraphDocument doc;
  std::unique_ptr<TNodeRegistry> registryStore;
  TGraphRuntime runtime;
  std::unique_ptr<TGraphCanvas> canvas;
  std::unique_ptr<NodeLibraryPanel> libraryPanel;
  std::unique_ptr<NodePropertiesPanel> propertiesPanel;
  std::unique_ptr<DiagnosticsDrawer> diagnosticsDrawer;
  std::unique_ptr<PresetBrowserPanel> presetBrowserPanel;
  std::unique_ptr<RuntimeStatusStrip> runtimeStatusStrip;
  std::unique_ptr<DocumentNoticeBanner> documentNoticeBanner;
  juce::AudioDeviceManager *audioDeviceManager = nullptr;

  juce::TextButton toggleLibraryButton;
  juce::TextButton quickAddButton;
  juce::TextButton findNodeButton;
  juce::TextButton commandPaletteButton;
  juce::TextButton toggleHeatmapButton;
  juce::TextButton toggleProbeButton;
  juce::TextButton toggleOverlayButton;
  juce::TextButton toggleDiagnosticsButton;
  juce::TextButton togglePresetsButton;

  bool libraryVisible = true;
  std::uint64_t lastDocumentRevision = 0;
  std::uint64_t lastRuntimeRevision = 0;
  std::uint64_t lastDocumentNoticeRevision = 0;
  std::uint64_t lastBindingRevision = 0;
  bool lastSessionDirty = false;
  bool lastSessionHasAutosaveSnapshot = false;
  std::int64_t lastSessionAutosaveMillis = 0;
  TEditorSessionStatus sessionStatus;
  TGraphRuntime::RuntimeStats lastRuntimeStats;
  juce::String runtimeMessageText;
  juce::Colour runtimeMessageAccent = juce::Colour(0xff60a5fa);
  int runtimeMessageTicksRemaining = 0;
  ParamBindingRevisionProvider bindingRevisionProvider;
};

} // namespace Teul
