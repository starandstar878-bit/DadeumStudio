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
class RailPanel;
class PlaceholderInspectorPanel;
class ControlSourceInspectorPanel;
class SystemEndpointInspectorPanel;

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
  bool applyLearnedControlBinding(const TDeviceBindingSignature &binding,
                                  const juce::String &profileId,
                                  const juce::String &deviceId,
                                  const juce::String &profileDisplayName,
                                  TControlSourceKind kind,
                                  TControlSourceMode mode,
                                  const juce::String &sourceDisplayName,
                                  bool autoDetected,
                                  bool confirmed);
  bool applyLearnedMidiMessage(const juce::MidiMessage &message,
                              const juce::String &midiDeviceName,
                              const juce::String &hardwareId,
                              const juce::String &profileId,
                              const juce::String &profileDisplayName,
                              bool autoDetected,
                              bool confirmed);
  bool reportControlDeviceProfilePresent(const juce::String &profileId,
                                       const juce::String &deviceId,
                                       const juce::String &displayName,
                                       bool autoDetected);
  bool reportControlDeviceProfileMissing(const juce::String &profileId);
  bool syncControlDeviceProfiles(
      const std::vector<TControlDeviceProfilePresence> &profiles,
      bool autoMarkMissing);
  void queueLearnedControlBinding(const TDeviceBindingSignature &binding,
                                  const juce::String &profileId,
                                  const juce::String &deviceId,
                                  const juce::String &profileDisplayName,
                                  TControlSourceKind kind,
                                  TControlSourceMode mode,
                                  const juce::String &sourceDisplayName,
                                  bool autoDetected,
                                  bool confirmed);
  void queueControlDeviceProfileSync(
      const std::vector<TControlDeviceProfilePresence> &profiles,
      bool autoMarkMissing);
  void queueControlDeviceProfilePresent(const juce::String &profileId,
                                        const juce::String &deviceId,
                                        const juce::String &displayName,
                                        bool autoDetected);
  void queueControlDeviceProfileMissing(const juce::String &profileId);
  void layout(juce::Rectangle<int> area);

private:
  struct ControlInputAdapter;
  struct MidiControlInputAdapter;

  struct PendingLearnBindingEvent {
    TDeviceBindingSignature binding;
    juce::String profileId;
    juce::String deviceId;
    juce::String profileDisplayName;
    juce::String sourceDisplayName;
    TControlSourceKind kind = TControlSourceKind::expression;
    TControlSourceMode mode = TControlSourceMode::continuous;
    bool autoDetected = true;
    bool confirmed = true;
  };

  struct PendingProfileSyncEvent {
    std::vector<TControlDeviceProfilePresence> profiles;
    bool autoMarkMissing = true;
  };

  struct PendingProfileDeltaEvent {
    enum class Kind { present, missing };

    Kind kind = Kind::present;
    juce::String profileId;
    juce::String deviceId;
    juce::String displayName;
    bool autoDetected = true;
  };

  void timerCallback() override;
  void rebuildAll(bool rebuildRuntime);
  void handleSelectionChanged(const std::vector<NodeId> &selectedNodeIds);
  void handleFrameSelectionChanged(int frameId);
  void openProperties(NodeId nodeId);
  void inspectNodeWithReveal(NodeId nodeId);
  void inspectControlSource(const juce::String &sourceId);
  void inspectSystemEndpoint(const juce::String &endpointId);
  void clearControlSourceInspector();
  void clearSystemEndpointInspector();
  void clearRailInspectors();
  bool focusDiagnosticTarget(const juce::String &graphId,
                             const juce::String &query);
  void refreshRuntimeUi(bool forceMessage = false);
  void refreshDocumentNoticeUi(bool force = false);
  void refreshSessionStatusUi(bool force = false);
  void refreshRailUi(bool relayout = false);
  void syncRuntimeViewButtons();
  void pushRuntimeMessage(const juce::String &text,
                          juce::Colour accent,
                          int ticks = 50);
  void refreshControlInputAdapters(bool announceChanges);
  void drainPendingProfileSyncEvents();
  void drainPendingProfileDeltaEvents();
  void drainPendingLearnBindings();

  EditorHandle &owner;
  TGraphDocument doc;
  std::unique_ptr<TNodeRegistry> registryStore;
  TGraphRuntime runtime;
  std::unique_ptr<TGraphCanvas> canvas;
  std::unique_ptr<juce::Component> canvasOverlayLayer;
  std::unique_ptr<NodeLibraryPanel> libraryPanel;
  std::unique_ptr<NodePropertiesPanel> propertiesPanel;
  std::unique_ptr<DiagnosticsDrawer> diagnosticsDrawer;
  std::unique_ptr<PresetBrowserPanel> presetBrowserPanel;
  std::unique_ptr<RuntimeStatusStrip> runtimeStatusStrip;
  std::unique_ptr<DocumentNoticeBanner> documentNoticeBanner;
  std::unique_ptr<PlaceholderInspectorPanel> placeholderInspector;
  std::unique_ptr<ControlSourceInspectorPanel> controlSourceInspector;
  std::unique_ptr<SystemEndpointInspectorPanel> systemEndpointInspector;
  std::unique_ptr<RailPanel> inputRail;
  std::unique_ptr<RailPanel> outputRail;
  std::unique_ptr<RailPanel> controlRail;
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
  juce::String lastSessionControlSummary;
  TEditorSessionStatus sessionStatus;
  TGraphRuntime::RuntimeStats lastRuntimeStats;
  juce::String runtimeMessageText;
  juce::Colour runtimeMessageAccent = juce::Colour(0xff60a5fa);
  int runtimeMessageTicksRemaining = 0;
  int controlInputRefreshCounter = 0;
  juce::CriticalSection controlLearnStateLock;
    std::vector<PendingProfileSyncEvent> pendingProfileSyncEvents;
  std::vector<PendingProfileDeltaEvent> pendingProfileDeltaEvents;
  std::vector<PendingLearnBindingEvent> pendingLearnBindingEvents;
  std::vector<std::unique_ptr<ControlInputAdapter>> controlInputAdapters;
  ParamBindingRevisionProvider bindingRevisionProvider;
};

} // namespace Teul
