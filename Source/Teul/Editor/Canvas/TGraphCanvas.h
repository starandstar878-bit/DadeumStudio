#pragma once

#include "Teul/Model/TGraphDocument.h"
#include "Teul/Registry/TNodeRegistry.h"
#include <JuceHeader.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace Teul {

class TNodeComponent;
class TPortComponent;
struct TNodeDescriptor;
class TNodeRegistry;
struct TPatchPresetSummary;
struct TStatePresetApplyReport;

class TGraphCanvas : public juce::Component,
                     public juce::DragAndDropTarget,
                     private juce::Timer {
public:
  explicit TGraphCanvas(TGraphDocument &doc, const TNodeRegistry &registry);
  ~TGraphCanvas() override;

  void paint(juce::Graphics &g) override;
  void paintOverChildren(juce::Graphics &g) override;
  void resized() override;

  void mouseDown(const juce::MouseEvent &event) override;
  void mouseDrag(const juce::MouseEvent &event) override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseDoubleClick(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event,
                      const juce::MouseWheelDetails &wheel) override;

  bool isInterestedInDragSource(
      const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
  void itemDragEnter(
      const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
  void itemDragMove(
      const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
  void itemDragExit(
      const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
  void itemDropped(
      const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
  bool shouldDrawDragImageWhenOver() override { return false; }

  bool keyPressed(const juce::KeyPress &key) override;
  bool keyStateChanged(bool isKeyDown) override;

  void setZoomLevel(float newZoom, juce::Point<float> anchorPosView);
  float getZoomLevel() const noexcept { return zoomLevel; }

  juce::Point<float> viewToWorld(juce::Point<float> viewPos) const;
  juce::Point<float> worldToView(juce::Point<float> worldPos) const;
  juce::Point<float> getViewCenter() const;

  TGraphDocument &getDocument() { return document; }
  const TGraphDocument &getDocument() const { return document; }
  const std::vector<NodeId> &getSelectedNodeIds() const noexcept {
    return selectedNodeIds;
  }
  int getSelectedFrameId() const noexcept { return selectedFrameId; }

  void rebuildNodeComponents();
  void updateChildPositions();

  enum class ExternalDragSourceKind {
    GraphConnection,
    Assignment,
  };

  struct ExternalDragSource {
    juce::String sourceId;
    juce::String sourcePortId;
    TPortDataType dataType = TPortDataType::Control;
    ExternalDragSourceKind kind = ExternalDragSourceKind::Assignment;
  };

  void beginConnectionDrag(const TPort &sourcePort,
                           juce::Point<float> mousePosView);
  void beginConnectionDragFromPoint(const TPort &sourcePort,
                                    juce::Point<float> sourcePosView,
                                    juce::Point<float> mousePosView);
  void beginExternalConnectionDragFromPoint(
      const ExternalDragSource &source, juce::Point<float> sourcePosView,
      juce::Point<float> mousePosView);
  void updateConnectionDrag(juce::Point<float> mousePosView);
  void endConnectionDrag(juce::Point<float> mousePosView);
  void cancelConnectionDrag();

  bool addNodeByTypeAtView(const juce::String &typeKey,
                           juce::Point<float> viewPos,
                           bool tryInsertOnWire = true);
  std::vector<const TNodeDescriptor *> getAllNodeDescriptors() const;

  void focusNode(NodeId nodeId);
  bool focusNodeByQuery(const juce::String &query);
  bool ensureNodeVisible(NodeId nodeId, float paddingView = 24.0f);

  using ConnectionLevelProvider = std::function<float(const TConnection &)>;
  void setConnectionLevelProvider(ConnectionLevelProvider provider);

  using PortLevelProvider = std::function<float(PortId)>;
  void setPortLevelProvider(PortLevelProvider provider);
  float getPortLevel(PortId portId) const;

  using BindingSummaryResolver =
      std::function<juce::String(const juce::String &paramId)>;
  void setBindingSummaryResolver(BindingSummaryResolver resolver);

  struct ExternalDropZone {
    juce::String zoneId;
    juce::Rectangle<float> boundsView;
    TPortDataType dataType = TPortDataType::Audio;
  };

  using ExternalDropZoneProvider =
      std::function<std::vector<ExternalDropZone>()>;
  using ExternalConnectionCommitHandler =
      std::function<bool(const TPort *sourcePort,
                         const ExternalDragSource *externalSource,
                         const juce::String &zoneId)>;
  using ExternalDragTargetChangedHandler =
      std::function<void(const juce::String &zoneId, bool canConnect)>;
  void setExternalDropZoneProvider(ExternalDropZoneProvider provider);
  void setExternalEndpointAnchorProvider(ExternalDropZoneProvider provider);
  void setExternalConnectionCommitHandler(
      ExternalConnectionCommitHandler handler);
  void setExternalDragTargetChangedHandler(
      ExternalDragTargetChangedHandler handler);

  struct RuntimeOverlayState {
    double sampleRate = 0.0;
    int blockSize = 0;
    int inputChannels = 0;
    int outputChannels = 0;
    int activeNodeCount = 0;
    int allocatedPortChannels = 0;
    int smoothingActiveCount = 0;
    std::uint64_t activeGeneration = 0;
    std::uint64_t pendingGeneration = 0;
    bool rebuildPending = false;
    bool clipDetected = false;
    bool denormalDetected = false;
    bool xrunDetected = false;
    bool mutedFallbackActive = false;
    float cpuLoadPercent = 0.0f;
  };

  struct RuntimeViewOptions {
    bool heatmapEnabled = true;
    bool liveProbeEnabled = true;
    bool debugOverlayEnabled = true;
  };

  void setRuntimeOverlayState(const RuntimeOverlayState &state);
  void setRuntimeViewOptions(const RuntimeViewOptions &options);
  const RuntimeViewOptions &getRuntimeViewOptions() const noexcept {
    return runtimeViewOptions;
  }
  void setRuntimeHeatmapEnabled(bool enabled);
  bool isRuntimeHeatmapEnabled() const noexcept {
    return runtimeViewOptions.heatmapEnabled;
  }
  void setLiveProbeEnabled(bool enabled);
  bool isLiveProbeEnabled() const noexcept {
    return runtimeViewOptions.liveProbeEnabled;
  }
  void setDebugOverlayEnabled(bool enabled);
  bool isDebugOverlayEnabled() const noexcept {
    return runtimeViewOptions.debugOverlayEnabled;
  }
  const RuntimeOverlayState &getRuntimeOverlayState() const noexcept {
    return runtimeOverlayState;
  }
  using NodePropertiesRequestHandler = std::function<void(NodeId)>;
  void setNodePropertiesRequestHandler(NodePropertiesRequestHandler handler);

  using NodeSelectionChangedHandler =
      std::function<void(const std::vector<NodeId> &)>;
  void setNodeSelectionChangedHandler(NodeSelectionChangedHandler handler);

  using FrameSelectionChangedHandler = std::function<void(int)>;
  void setFrameSelectionChangedHandler(FrameSelectionChangedHandler handler);

  void openQuickAddAt(juce::Point<float> pointView);
  void openNodeSearchPrompt();
  void openCommandPalette();

  const std::vector<juce::String> &getRecentNodeTypes() const noexcept {
    return recentNodeTypes;
  }

  void requestNodeMouseDown(NodeId nodeId, const juce::MouseEvent &event);
  void requestNodeMouseDrag(NodeId nodeId, const juce::MouseEvent &event);
  void requestNodeMouseUp(NodeId nodeId, const juce::MouseEvent &event);
  void requestNodeContextMenu(NodeId nodeId, juce::Point<float> pointView,
                              juce::Point<float> pointScreen);

  bool captureSelectedNodesIntoFrame(int frameId);
  bool releaseSelectedNodesFromFrame(int frameId);
  bool fitFrameToMembers(int frameId);
  void saveFrameAsPatchPreset(int frameId);
  void insertPatchPresetAt(juce::Point<float> pointView);
  juce::Result insertPatchPresetFromFile(const juce::File &file,
                                         juce::Point<float> pointView,
                                         TPatchPresetSummary *summaryOut = nullptr);
  void saveDocumentAsStatePreset();
  void applyStatePreset();
  juce::Result applyStatePresetFromFile(
      const juce::File &file,
      TStatePresetApplyReport *reportOut = nullptr);

  bool isNodeSelected(NodeId nodeId) const;
  bool isNodeMoveLocked(NodeId nodeId) const;

private:
  void timerCallback() override;

  void drawInfiniteGrid(juce::Graphics &g);
  void drawFrames(juce::Graphics &g);
  void drawZoomIndicator(juce::Graphics &g);
  void drawConnections(juce::Graphics &g);
  void drawMiniMap(juce::Graphics &g);
  void drawLibraryDropPreview(juce::Graphics &g);
  void drawSelectionOverlay(juce::Graphics &g);
  void drawRuntimeOverlay(juce::Graphics &g);
  void drawStatusHint(juce::Graphics &g);

  juce::Path makeWirePath(juce::Point<float> from,
                          juce::Point<float> to) const;
  juce::Colour colorForPortType(TPortDataType type) const;

  const TPort *findPortModel(NodeId nodeId, PortId portId) const;
  const TPort *findPortModel(const TEndpoint &endpoint) const;
  const TSystemRailPort *findRailPortModel(const TEndpoint &endpoint) const;
  TPortDataType dataTypeForEndpoint(const TEndpoint &endpoint) const;

  TNodeComponent *findNodeComponent(NodeId nodeId) noexcept;
  const TNodeComponent *findNodeComponent(NodeId nodeId) const noexcept;

  TPortComponent *findPortComponent(NodeId nodeId, PortId portId) noexcept;
  const TPortComponent *findPortComponent(NodeId nodeId,
                                          PortId portId) const noexcept;

  juce::Point<float> portCentreInCanvas(NodeId nodeId, PortId portId) const;
  juce::Point<float> portCentreInCanvas(const TEndpoint &endpoint) const;

  ConnectionId hitTestConnection(juce::Point<float> pointView,
                                 float hitThickness = 7.0f) const;

  void clearDragTargetHighlight();
  void updateDragTargetFromMouse(juce::Point<float> mousePosView);
  bool isCurrentDragTargetConnectable() const;
  void tryCreateConnectionFromDrag();

  void deleteConnectionWithAnimation(ConnectionId connectionId,
                                     juce::Point<float> breakPointView,
                                     juce::Point<float> impulseView);

  void startDisconnectAnimation(juce::Point<float> anchorPosView,
                                juce::Point<float> loosePosView,
                                juce::Point<float> impulseView,
                                TPortDataType type);

  void showCanvasContextMenu(juce::Point<float> pointView,
                             juce::Point<float> pointScreen);
  void showNodeContextMenu(NodeId nodeId, juce::Point<float> pointView,
                           juce::Point<float> pointScreen);
  void showFrameContextMenu(int frameId, juce::Point<float> pointView,
                            juce::Point<float> pointScreen);
  void showQuickAddPrompt(juce::Point<float> pointView);
  void showNodeSearchOverlay();
  void showCommandPaletteOverlay();
  void rememberRecentNode(const juce::String &typeKey);
  int scoreDescriptorMatch(const TNodeDescriptor &desc,
                           const juce::String &query) const;
  const TNodeDescriptor *
  findDescriptorByTypeKey(const juce::String &typeKey) const noexcept;
  std::vector<TTeulExposedParam> listExposedParamsForNode(
      const TNode &node) const;

  bool insertNodeOnConnection(ConnectionId connectionId,
                              NodeId insertedNodeId);
  bool replaceNode(NodeId nodeId, const juce::String &replacementTypeKey);
  bool isReplacementCompatible(const TNode &oldNode,
                               const TNodeDescriptor &replacement) const;

  void clearNodeSelection();
  void clearFrameSelection();
  void setNodeSelection(NodeId nodeId, bool selected);
  void selectOnlyNode(NodeId nodeId);
  void selectOnlyNodes(const std::vector<NodeId> &nodeIds);
  void selectOnlyFrame(int frameId);
  void syncNodeSelectionToComponents();
  std::vector<NodeId> collectMarqueeSelection() const;
  void applyMarqueeSelection();

  struct DeleteSelectionImpact {
    int touchingConnectionCount = 0;
    std::vector<juce::String> bindingSummaries;

    bool requiresConfirmation() const noexcept {
      return touchingConnectionCount > 0 || !bindingSummaries.empty();
    }
  };

  DeleteSelectionImpact
  analyzeDeleteSelectionImpact(const std::vector<NodeId> &targets) const;
  void deleteNodes(const std::vector<NodeId> &targets);
  void duplicateSelection();
  void deleteSelectionWithPrompt();
  void disconnectSelectionWithPrompt();
  void toggleBypassSelection();
  void alignSelectionLeft();
  void alignSelectionTop();
  void distributeSelectionHorizontally();
  void distributeSelectionVertically();

  void startNodeDrag(NodeId nodeId, juce::Point<float> mouseView);
  void updateNodeDrag(juce::Point<float> mouseView);
  void endNodeDrag();

  void startFrameDrag(int frameId, juce::Point<float> mouseView);
  void updateFrameDrag(juce::Point<float> mouseView);
  void endFrameDrag();

  int hitTestFrame(juce::Point<float> pointView) const;
  bool isPointInFrameTitle(int frameId, juce::Point<float> pointView) const;
  bool isNodeInsideFrame(const TNode &node, const TFrameRegion &frame) const;
  bool isNodeHiddenByCollapsedFrame(const TNode &node) const;
  juce::Rectangle<float> getFrameMemberBoundsWorld(
      const TFrameRegion &frame, float paddingWorld = 24.0f) const;

  void recalcMiniMapCache();
  juce::Point<float> miniMapToWorld(juce::Point<float> miniPoint) const;
  juce::Rectangle<float> getNodeBoundsInView(const TNodeComponent &nodeComponent) const;

  void pushStatusHint(const juce::String &text);

  TGraphDocument &document;

  float zoomLevel = 1.0f;
  juce::Point<float> viewOriginWorld;

  struct PanState {
    bool active = false;
    juce::Point<float> startMouseView;
    juce::Point<float> startViewOriginWorld;
  } panState;

  std::vector<std::unique_ptr<TNodeComponent>> nodeComponents;
  std::vector<NodeId> selectedNodeIds;
  int selectedFrameId = 0;

  struct MarqueeState {
    bool active = false;
    bool additive = false;
    juce::Point<float> startView;
    juce::Rectangle<float> rectView;
    std::vector<NodeId> baseSelection;
  } marqueeState;

  struct NodeDragState {
    bool active = false;
    NodeId anchorNodeId = kInvalidNodeId;
    juce::Point<float> startMouseView;
    std::unordered_map<NodeId, juce::Point<float>> startWorldByNode;
  } nodeDragState;

  struct FrameDragState {
    bool active = false;
    int frameId = 0;
    juce::Point<float> startMouseView;
    juce::Point<float> startFramePosWorld;
    std::unordered_map<NodeId, juce::Point<float>> containedNodeStartWorld;
  } frameDragState;

  struct SnapGuideState {
    bool xActive = false;
    bool yActive = false;
    float worldX = 0.0f;
    float worldY = 0.0f;
  } snapGuideState;

  struct WireDragState {
    bool active = false;
    NodeId sourceNodeId = kInvalidNodeId;
    PortId sourcePortId = kInvalidPortId;
    juce::String sourceExternalId;
    juce::String sourceExternalPortId;
    ExternalDragSourceKind sourceExternalKind =
        ExternalDragSourceKind::Assignment;
    TPortDataType sourceType = TPortDataType::Audio;

    juce::Point<float> sourcePosView;
    juce::Point<float> mousePosView;

    NodeId targetNodeId = kInvalidNodeId;
    PortId targetPortId = kInvalidPortId;
    juce::String targetExternalZoneId;

    bool targetTypeMatch = false;
    bool targetCycleFree = false;
  } wireDragState;

  ConnectionId selectedConnectionId = kInvalidConnectionId;
  ConnectionId pressedConnectionId = kInvalidConnectionId;
  juce::Point<float> pressedConnectionPointView;
  bool connectionBreakDragArmed = false;

  struct DisconnectAnimationState {
    bool active = false;
    juce::Point<float> anchorPosView;
    juce::Point<float> loosePosView;
    juce::Point<float> velocityViewPerSecond;
    float alpha = 0.0f;
    TPortDataType type = TPortDataType::Audio;
  } disconnectAnimation;

  float flowPhase = 0.0f;
  ConnectionLevelProvider connectionLevelProvider;
  PortLevelProvider portLevelProvider;
  BindingSummaryResolver bindingSummaryResolver;
  ExternalDropZoneProvider externalDropZoneProvider;
  ExternalDropZoneProvider externalEndpointAnchorProvider;
  ExternalConnectionCommitHandler externalConnectionCommitHandler;
  ExternalDragTargetChangedHandler externalDragTargetChangedHandler;
  FrameSelectionChangedHandler frameSelectionChangedHandler;

  std::vector<juce::String> recentNodeTypes;

  juce::Rectangle<float> miniMapRectView;
  juce::Rectangle<float> miniMapViewportRectView;
  juce::Rectangle<float> miniMapWorldBounds;
  bool miniMapCacheValid = false;

  struct MiniMapDragState {
    bool active = false;
    juce::Point<float> worldOffset;
  } miniMapDragState;

  juce::String statusHintText;
  float statusHintAlpha = 0.0f;
  RuntimeOverlayState runtimeOverlayState;
  RuntimeViewOptions runtimeViewOptions;

  std::vector<TNodeDescriptor> nodeDescriptors;
  NodePropertiesRequestHandler nodePropertiesRequestHandler;
  NodeSelectionChangedHandler nodeSelectionChangedHandler;

  class SearchOverlay;
  std::unique_ptr<SearchOverlay> searchOverlay;
  juce::Point<float> quickAddAnchorView;

  struct LibraryDropPreviewState {
    bool active = false;
    juce::Point<float> pointView;
    std::string typeKeyUtf8;
  } libraryDropPreview;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphCanvas)
};

} // namespace Teul
