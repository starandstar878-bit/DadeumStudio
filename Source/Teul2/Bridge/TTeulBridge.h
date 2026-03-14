#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace Teul {

// =============================================================================
//  Export & Bridge Constants
// =============================================================================

enum class TExportMode {
  EditableGraph,
  RuntimeModule,
};

enum class TExportIssueSeverity {
  Info,
  Warning,
  Error,
};

enum class TExportIssueCode {
  GraphEmpty,
  UnknownNodeDescriptor,
  UnsupportedNodeForMode,
  NodePortLayoutMismatch,
  DuplicatePortId,
  InvalidPortOwner,
  CapabilityInstanceCountViolation,
  InvalidConnectionEndpoint,
  MissingNodeForConnection,
  MissingPortForConnection,
  ConnectionDirectionMismatch,
  ConnectionTypeMismatch,
  DuplicateConnection,
  CycleDetected,
  MissingPrimaryAudioOutput,
  InvalidChannelIndex,
  NodeRuntimeWarning,
  DeadNodePruned,
  AssetReferenceMissing,
  AssetReferenceCopied,
  ParamSkippedFromAPVTS,
};

// =============================================================================
//  Export IR Structures (Intermediate Representation)
// =============================================================================

struct TExportIssueLocation {
  NodeId nodeId = kInvalidNodeId;
  ConnectionId connectionId = kInvalidConnectionId;
  PortId portId = kInvalidPortId;
  juce::String nodeTypeKey;
  juce::String nodeDisplayName;
  float nodeX = 0.0f;
  float nodeY = 0.0f;

  bool hasNode() const noexcept { return nodeId != kInvalidNodeId; }
  bool hasConnection() const noexcept { return connectionId != kInvalidConnectionId; }
  bool hasPort() const noexcept { return portId != kInvalidPortId; }
};

struct TExportIssue {
  TExportIssueSeverity severity = TExportIssueSeverity::Info;
  TExportIssueCode code = TExportIssueCode::GraphEmpty;
  juce::String message;
  TExportIssueLocation location;
};

struct TExportSummary {
  int nodeCount = 0;
  int connectionCount = 0;
  int liveNodeCount = 0;
  int prunedNodeCount = 0;
  int exposedParamCount = 0;
  int assetReferenceCount = 0;
  int copiedAssetCount = 0;
  int missingAssetCount = 0;
  int warningCount = 0;
  int errorCount = 0;
};

struct TExportAssetIR {
  juce::String refKey;
  juce::String paramId;
  juce::String nodeTypeKey;
  juce::String paramKey;
  juce::String sourcePath;
  juce::String destinationRelativePath;
  bool exists = false;
  bool copied = false;
  bool reused = false;
};

struct TExportPortIR {
  PortId portId = kInvalidPortId;
  TPortDirection direction = TPortDirection::Input;
  TPortDataType dataType = TPortDataType::Audio;
  juce::String name;
  int channelIndex = 0;
};

struct TExportParamIR {
  juce::String paramId;
  NodeId nodeId = kInvalidNodeId;
  juce::String paramKey;
  juce::String paramLabel;
  juce::var defaultValue;
  juce::var currentValue;
  TParamValueType valueType = TParamValueType::Auto;
  juce::var minValue;
  juce::var maxValue;
  juce::var step;
  juce::String unitLabel;
  int displayPrecision = 2;
  juce::String group;
  TParamWidgetHint preferredWidget = TParamWidgetHint::Auto;
  bool isReadOnly = false;
  bool isAutomatable = true;
  bool isDiscrete = false;
  bool exposeToIeum = true;
};

struct TExportNodeIR {
  NodeId nodeId = kInvalidNodeId;
  juce::String typeKey;
  juce::String displayName;
  juce::String category;
  bool bypassed = false;
  bool collapsed = false;
  bool live = true;
  std::vector<TExportPortIR> ports;
  std::vector<TExportParamIR> params;
};

struct TExportConnectionIR {
  ConnectionId connectionId = kInvalidConnectionId;
  TEndpoint from;
  TEndpoint to;
  TPortDataType dataType = TPortDataType::Audio;
  juce::String sourcePortName;
  juce::String targetPortName;
};

struct TExportGraphIR {
  int schemaVersion = 0;
  TExportMode mode = TExportMode::EditableGraph;
  juce::String generatedAtUtc;
  TGraphMeta meta;
  std::vector<TExportNodeIR> nodes;
  std::vector<TExportConnectionIR> connections;
  std::vector<TExportAssetIR> assets;
};

// =============================================================================
//  Export Report & Options
// =============================================================================

struct TExportReport {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;
  juce::String runtimeClassName;
  juce::File outputDirectory;
  juce::File graphFile;
  juce::File manifestFile;
  juce::File runtimeDataFile;

  TExportSummary summary;
  std::vector<TExportIssue> issues;
  std::vector<TExportAssetIR> assets;

  bool hasErrors() const noexcept { return summary.errorCount > 0; }
};

struct TExportOptions {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;
  bool packageAssets = true;
  bool overwriteExistingFiles = true;
  juce::File outputDirectory;
  juce::File projectRootDirectory;
  juce::String runtimeClassName = "TeulRuntimeModule";
};

struct TExportResult {
  TExportReport report;
  std::unique_ptr<TExportGraphIR> graphIR;
  bool success = false;

  bool hasUsableGraph() const noexcept {
    return success && !report.hasErrors() && graphIR != nullptr;
  }
};

} // namespace Teul
