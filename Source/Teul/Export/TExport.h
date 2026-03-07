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
  ExportBackendNotImplemented,
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

struct TExportSummary {
  int nodeCount = 0;
  int connectionCount = 0;
  int normalizedPortCount = 0;
  int normalizedParamCount = 0;
  int exposedParamCount = 0;
  int automatableParamCount = 0;
  int unsupportedNodeCount = 0;
};

struct TExportReport {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;
  bool backendImplemented = false;
  TExportSummary summary;
  std::vector<TExportIssue> issues;

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
  TGraphMeta meta;
  std::vector<TExportNodeIR> nodes;
  std::vector<TExportConnectionIR> connections;

  juce::var toJson() const;
};

struct TExportOptions {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;
  bool requirePrimaryAudioOutput = true;
  bool requireKnownNodeDescriptors = true;
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
};

} // namespace Teul