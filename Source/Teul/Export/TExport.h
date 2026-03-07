#pragma once

#include "../Model/TGraphDocument.h"
#include "../Registry/TNodeRegistry.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace Teul {

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

struct TExportIssueLocation {
  NodeId nodeId = kInvalidNodeId;
  ConnectionId connectionId = kInvalidConnectionId;
  PortId portId = kInvalidPortId;
  TEndpoint from;
  TEndpoint to;
  juce::String nodeTypeKey;
  juce::String nodeDisplayName;
  float nodeX = 0.0f;
  float nodeY = 0.0f;

  bool hasNode() const noexcept { return nodeId != kInvalidNodeId; }
  bool hasConnection() const noexcept {
    return connectionId != kInvalidConnectionId;
  }
  bool hasPort() const noexcept { return portId != kInvalidPortId; }
};

struct TExportIssue {
  TExportIssueSeverity severity = TExportIssueSeverity::Info;
  TExportIssueCode code = TExportIssueCode::GraphEmpty;
  juce::String message;
  TExportIssueLocation location;
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

struct TExportScheduleEntry {
  int orderIndex = -1;
  NodeId nodeId = kInvalidNodeId;
  juce::String typeKey;
  juce::String displayName;
};

struct TExportBufferPlanEntry {
  int bufferIndex = -1;
  NodeId nodeId = kInvalidNodeId;
  PortId portId = kInvalidPortId;
  TPortDataType dataType = TPortDataType::Audio;
  juce::String portName;
  int channelIndex = 0;
};

struct TExportSummary {
  int nodeCount = 0;
  int connectionCount = 0;
  int liveNodeCount = 0;
  int prunedNodeCount = 0;
  int scheduleEntryCount = 0;
  int bufferEntryCount = 0;
  int normalizedPortCount = 0;
  int normalizedParamCount = 0;
  int exposedParamCount = 0;
  int automatableParamCount = 0;
  int apvtsParamCount = 0;
  int skippedApvtsParamCount = 0;
  int unsupportedNodeCount = 0;
  int assetReferenceCount = 0;
  int copiedAssetCount = 0;
  int missingAssetCount = 0;
};

struct TExportReport {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;
  bool backendImplemented = true;

  juce::String runtimeClassName;
  juce::File outputDirectory;
  juce::File graphFile;
  juce::File manifestFile;
  juce::File runtimeDataFile;
  juce::File generatedHeaderFile;
  juce::File generatedSourceFile;
  juce::File cmakeHintsFile;
  juce::File jucerHintsFile;
  juce::File reportFile;

  TExportSummary summary;
  std::vector<TExportIssue> issues;
  std::vector<TExportAssetIR> assets;

  void addIssue(TExportIssueSeverity severity,
                TExportIssueCode code,
                const juce::String &message,
                const TExportIssueLocation &location = {});
  int warningCount() const noexcept;
  int errorCount() const noexcept;
  bool hasErrors() const noexcept;
  juce::var toJson() const;
  juce::String toText() const;
};

struct TExportParamIR {
  juce::String paramId;
  NodeId nodeId = kInvalidNodeId;
  juce::String nodeTypeKey;
  juce::String nodeDisplayName;
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
  juce::String description;
  std::vector<TParamOptionSpec> enumOptions;
  TParamWidgetHint preferredWidget = TParamWidgetHint::Auto;
  bool showInNodeBody = false;
  bool showInPropertyPanel = true;
  bool isReadOnly = false;
  bool isAutomatable = true;
  bool isModulatable = true;
  bool isDiscrete = false;
  bool exposeToIeum = true;
  juce::String preferredBindingKey;
  juce::String exportSymbol;
  juce::String categoryPath;
};

struct TExportPortIR {
  PortId portId = kInvalidPortId;
  TPortDirection direction = TPortDirection::Input;
  TPortDataType dataType = TPortDataType::Audio;
  juce::String name;
  int channelIndex = 0;
};

struct TExportNodeIR {
  NodeId nodeId = kInvalidNodeId;
  juce::String typeKey;
  juce::String displayName;
  juce::String category;
  TNodeExportSupport exportSupport = TNodeExportSupport::Unsupported;
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
  juce::String sourceNodeTypeKey;
  juce::String targetNodeTypeKey;
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
  std::vector<TExportScheduleEntry> schedule;
  std::vector<TExportBufferPlanEntry> bufferPlan;
  std::vector<TExportAssetIR> assets;

  juce::var toJson() const;
};

struct TExportOptions {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;
  bool requirePrimaryAudioOutput = true;
  bool requireKnownNodeDescriptors = true;
  bool optimizeForRuntimeModule = true;
  bool overwriteExistingFiles = true;
  bool writeManifestJson = true;
  bool writeGraphJson = true;
  bool writeRuntimeDataJson = true;
  bool writeRuntimeModuleFiles = true;
  bool writeBuildHints = true;
  bool packageAssets = true;
  juce::File outputDirectory;
  juce::File projectRootDirectory;
  juce::String runtimeClassName = "TeulExportedRuntimeModule";
};

struct TExportResult {
  TExportReport report;
  std::unique_ptr<TExportGraphIR> graphIR;

  bool hasUsableGraph() const noexcept {
    return graphIR != nullptr && !report.hasErrors();
  }
};

class TExporter {
public:
  static TExportResult run(const TGraphDocument &document,
                           const TNodeRegistry &registry,
                           const TExportOptions &options = {});
  static juce::Result exportToDirectory(const TGraphDocument &document,
                                        const TNodeRegistry &registry,
                                        const TExportOptions &options,
                                        TExportReport &reportOut);
  static juce::Result importEditableGraphPackage(const juce::File &path,
                                                 TGraphDocument &documentOut);
};

} // namespace Teul