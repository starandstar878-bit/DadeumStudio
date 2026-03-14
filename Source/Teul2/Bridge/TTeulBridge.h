#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace Teul {

// =============================================================================
//  TExport/Bridge Types (Teul/Export/TExport.h 에서 이관)
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

struct TExportIssueLocation {
  NodeId nodeId = kInvalidNodeId;
  ConnectionId connectionId = kInvalidConnectionId;
  PortId portId = kInvalidPortId;
  juce::String nodeTypeKey;
  juce::String nodeDisplayName;

  bool hasNode() const noexcept { return nodeId != kInvalidNodeId; }
};

struct TExportIssue {
  TExportIssueSeverity severity = TExportIssueSeverity::Info;
  juce::String message;
  TExportIssueLocation location;
};

struct TExportSummary {
  int nodeCount = 0;
  int connectionCount = 0;
  int exposedParamCount = 0;
};

struct TExportReport {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;

  TExportSummary summary;
  std::vector<TExportIssue> issues;

  bool hasErrors() const noexcept {
    for (const auto &i : issues)
      if (i.severity == TExportIssueSeverity::Error)
        return true;
    return false;
  }
};

struct TExportOptions {
  TExportMode mode = TExportMode::EditableGraph;
  bool dryRunOnly = true;
  juce::File outputDirectory;
  juce::String runtimeClassName = "TeulExportedRuntimeModule";
};

struct TExportResult {
  TExportReport report;
  bool success = false;

  bool hasUsableGraph() const noexcept {
    return success && !report.hasErrors();
  }
};

} // namespace Teul
