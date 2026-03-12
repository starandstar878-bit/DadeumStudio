#include "Teul/Editor/Panels/NodePropertiesPanel.h"

#include "Teul/Editor/TIssueState.h"
#include "Teul/Editor/Panels/Property/BindingSummaryPresenter.h"
#include "Teul/Editor/Panels/Property/ParamEditorFactory.h"
#include "Teul/Editor/Panels/Property/ParamValueFormatter.h"
#include "Teul/History/TCommands.h"
#include "Teul/Model/TGraphDocument.h"
#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace Teul {
namespace {

static juce::String nodeLabelForInspector(const TNode &node,
                                          const TNodeRegistry &registry) {
  if (node.label.isNotEmpty())
    return node.label;

  if (const auto *desc = registry.descriptorFor(node.typeKey)) {
    if (desc->displayName.isNotEmpty())
      return desc->displayName;
  }

  return node.typeKey;
}

static juce::String colorTagFromId(int id) {
  switch (id) {
  case 2:
    return "red";
  case 3:
    return "orange";
  case 4:
    return "green";
  case 5:
    return "blue";
  default:
    return {};
  }
}

static int colorTagToId(const juce::String &tagRaw) {
  const juce::String tag = tagRaw.trim().toLowerCase();
  if (tag == "red")
    return 2;
  if (tag == "orange")
    return 3;
  if (tag == "green")
    return 4;
  if (tag == "blue")
    return 5;
  return 1;
}

static juce::uint32 frameColorFromId(int id) {
  switch (id) {
  case 2:
    return 0x33ef4444;
  case 3:
    return 0x33f59e0b;
  case 4:
    return 0x3322c55e;
  case 5:
    return 0x334d8bf7;
  default:
    return 0x334d8bf7;
  }
}

static int frameColorToId(juce::uint32 colorArgb) {
  switch (colorArgb) {
  case 0x33ef4444:
    return 2;
  case 0x33f59e0b:
    return 3;
  case 0x3322c55e:
    return 4;
  case 0x334d8bf7:
  default:
    return 5;
  }
}

static juce::String frameTypeSummary(const TFrameRegion &frame) {
  return juce::String::formatted(
      "%s / %s / %d members",
      frame.logicalGroup ? "Logical Frame Group" : "Visual Frame",
      frame.membershipExplicit ? "explicit membership"
                               : "bounds membership",
      (int)frame.memberNodeIds.size());
}

static juce::String frameSummaryText(const TFrameRegion &frame) {
  return "UUID: " + frame.frameUuid + "\r\nMembers: " +
         juce::String((int)frame.memberNodeIds.size()) + "\r\nMembership: " +
         (frame.membershipExplicit ? "Explicit list" : "Bounds-derived") +
         "\r\nLocked: " + (frame.locked ? "Yes" : "No") +
         "\r\nCollapsed: " + (frame.collapsed ? "Yes" : "No") +
         "\r\nBounds: " +
         juce::String::formatted("%.0f, %.0f / %.0f x %.0f", frame.x, frame.y,
                                 frame.width, frame.height);
}

static bool shouldShowIeumBindingLine(const TParamSpec &spec,
                                      const juce::String &text) {
  return text.isNotEmpty() &&
         (spec.exposeToIeum || spec.isAutomatable || spec.isModulatable ||
          spec.isReadOnly || text != "Ieum: hidden");
}

static bool shouldShowGyeolBindingLine(const juce::String &text) {
  return text.isNotEmpty() && text != "Gyeol: hidden";
}

static bool splitStereoPortLabel(juce::StringRef name, bool &isLeft,
                                 juce::String &suffix) {
  const juce::String trimmed = juce::String(name).trim();
  if (trimmed.equalsIgnoreCase("L")) {
    isLeft = true;
    suffix.clear();
    return true;
  }
  if (trimmed.equalsIgnoreCase("R")) {
    isLeft = false;
    suffix.clear();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("L ")) {
    isLeft = true;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  if (trimmed.startsWithIgnoreCase("R ")) {
    isLeft = false;
    suffix = trimmed.substring(2).trim();
    return true;
  }
  return false;
}

struct GroupedNodePortSummary {
  juce::String displayName;
  TPortDirection direction = TPortDirection::Input;
  TPortDataType dataType = TPortDataType::Audio;
  std::vector<const TPort *> ports;
};

static std::vector<GroupedNodePortSummary>
groupNodePortsForSummary(const TNode &node) {
  std::vector<GroupedNodePortSummary> groups;
  std::vector<bool> used(node.ports.size(), false);

  const auto groupedDisplayName = [](const TPort &port) {
    bool ignoredIsLeft = false;
    juce::String suffix;
    if (splitStereoPortLabel(port.name, ignoredIsLeft, suffix) &&
        suffix.isNotEmpty()) {
      return suffix;
    }
    return port.name;
  };

  for (size_t index = 0; index < node.ports.size(); ++index) {
    if (used[index])
      continue;

    const auto &port = node.ports[index];
    GroupedNodePortSummary group;
    group.direction = port.direction;
    group.dataType = port.dataType;
    group.displayName = groupedDisplayName(port);
    group.ports.push_back(&port);
    used[index] = true;

    if (port.dataType == TPortDataType::Audio) {
      bool isLeft = false;
      juce::String suffix;
      if (splitStereoPortLabel(port.name, isLeft, suffix)) {
        for (size_t candidateIndex = index + 1; candidateIndex < node.ports.size();
             ++candidateIndex) {
          if (used[candidateIndex])
            continue;

          const auto &candidate = node.ports[candidateIndex];
          if (candidate.direction != port.direction ||
              candidate.dataType != port.dataType) {
            continue;
          }

          bool candidateIsLeft = false;
          juce::String candidateSuffix;
          if (!splitStereoPortLabel(candidate.name, candidateIsLeft,
                                    candidateSuffix) ||
              candidateIsLeft == isLeft || candidateSuffix != suffix) {
            continue;
          }

          if (isLeft) {
            group.ports = {&port, &candidate};
          } else {
            group.ports = {&candidate, &port};
          }
          group.displayName = suffix.isNotEmpty() ? suffix : port.name;
          used[candidateIndex] = true;
          break;
        }
      }
    }

    groups.push_back(std::move(group));
  }

  return groups;
}

static std::vector<int>
groupedPortConnectionCounts(const TGraphDocument &document,
                            const GroupedNodePortSummary &group,
                            bool incoming) {
  std::vector<int> counts(group.ports.size(), 0);
  for (const auto &connection : document.connections) {
    for (size_t index = 0; index < group.ports.size(); ++index) {
      const auto *port = group.ports[index];
      if (incoming) {
        if (connection.to.isNodePort() && connection.to.nodeId == port->ownerNodeId &&
            connection.to.portId == port->portId) {
          ++counts[index];
        }
      } else {
        if (connection.from.isNodePort() &&
            connection.from.nodeId == port->ownerNodeId &&
            connection.from.portId == port->portId) {
          ++counts[index];
        }
      }
    }
  }
  return counts;
}

static int groupedPortConnectionCount(const std::vector<int> &counts) {
  int total = 0;
  for (const int value : counts)
    total += value;
  return total;
}

static int groupedPortCapacity(const GroupedNodePortSummary &group, bool incoming) {
  int capacity = 0;
  for (const auto *port : group.ports) {
    const int value = incoming ? port->maxIncomingConnections
                               : port->maxOutgoingConnections;
    if (value < 0)
      return -1;
    capacity += value;
  }
  return capacity;
}

static juce::String groupedPortTypeLabel(const GroupedNodePortSummary &group) {
  const juce::String domain = [type = group.dataType]() {
    switch (type) {
    case TPortDataType::Audio:
      return juce::String("Audio");
    case TPortDataType::CV:
      return juce::String("CV");
    case TPortDataType::Gate:
      return juce::String("Gate");
    case TPortDataType::MIDI:
      return juce::String("MIDI");
    case TPortDataType::Control:
      return juce::String("Control");
    default:
      return juce::String("Signal");
    }
  }();

  if (group.ports.size() <= 1)
    return domain + " Mono";
  return domain + " Bus " + juce::String((int)group.ports.size()) + "ch";
}

static juce::String groupedPortStateLabel(int count, int capacity) {
  if (count <= 0)
    return "Free";
  if (capacity >= 0 && count >= capacity)
    return "Full";
  return "Partial";
}

static juce::String groupedPortOccupancyLabel(const GroupedNodePortSummary &group,
                                              const std::vector<int> &counts) {
  if (group.ports.size() <= 1 || counts.empty())
    return {};

  juce::StringArray parts;
  for (size_t index = 0; index < group.ports.size() && index < counts.size(); ++index) {
    const auto *port = group.ports[index];
    juce::String slotLabel = juce::String((int)index + 1);
    bool isLeft = false;
    juce::String suffix;
    if (splitStereoPortLabel(port->name, isLeft, suffix))
      slotLabel = isLeft ? "L" : "R";
    parts.add(slotLabel + ":" + juce::String(counts[index]));
  }

  return parts.joinIntoString(" ");
}

static juce::String groupedPortUsageLabel(const GroupedNodePortSummary &group,
                                          const std::vector<int> &counts,
                                          int capacity, bool incoming) {
  const int count = groupedPortConnectionCount(counts);
  juce::String text = group.displayName + " / " + groupedPortTypeLabel(group);
  text << " / " << groupedPortStateLabel(count, capacity);
  text << " / " << (incoming ? "In " : "Out ") << juce::String(count) << "/";
  text << (capacity < 0 ? juce::String("inf") : juce::String(capacity));
  const auto occupancy = groupedPortOccupancyLabel(group, counts);
  if (occupancy.isNotEmpty())
    text << " / " << occupancy;
  return text;
}

static juce::String groupedPortCapacityLabel(int capacity, bool incoming) {
  return (incoming ? juce::String("In cap ") : juce::String("Out cap ")) +
         (capacity < 0 ? juce::String("inf") : juce::String(capacity));
}

static juce::String groupedPortRuleLabel(const GroupedNodePortSummary &group,
                                         int capacity) {
  const bool incoming = group.direction == TPortDirection::Input;
  const int channelCount = (int)group.ports.size();

  juce::String text = group.displayName + " / " + groupedPortTypeLabel(group) +
                      " / ";

  if (incoming) {
    if (channelCount <= 1) {
      text << "Accepts matching mono outputs or single bus channels";
    } else {
      text << "Body accepts matching " << juce::String(channelCount)
           << "ch bundles when all target channels are free; channel circles accept matching single channels";
    }
  } else {
    if (channelCount <= 1) {
      text << "Can connect to matching mono inputs or bus channel circles";
    } else {
      text << "Body connects to matching " << juce::String(channelCount)
           << "ch bus bodies; channel circles connect individually";
    }
  }

  text << " / " << groupedPortCapacityLabel(capacity, incoming);
  return text;
}

struct GroupedPortIssueSummary {
  TIssueState state = TIssueState::none;
  juce::String detail;
};

static GroupedPortIssueSummary issueSummaryForConnectedEndpoint(
    const TGraphDocument &document, const TPort &port, const TEndpoint &otherEndpoint) {
  if (otherEndpoint.isRailPort()) {
    const auto *railEndpoint =
        document.controlState.findEndpoint(otherEndpoint.railEndpointId);
    if (railEndpoint == nullptr)
      return {TIssueState::invalidConfig, "Connected rail endpoint is missing from the document"};
    if (railEndpoint->missing)
      return {TIssueState::missing, "Connected rail endpoint is missing"};
    if (railEndpoint->degraded)
      return {TIssueState::degraded, "Connected rail endpoint is degraded"};

    const auto *railPort = document.findSystemRailPort(otherEndpoint.railEndpointId,
                                                       otherEndpoint.railPortId);
    if (railPort == nullptr)
      return {TIssueState::invalidConfig, "Connected rail channel no longer exists"};
    if (railPort->dataType != port.dataType)
      return {TIssueState::invalidConfig, "Connected rail channel type no longer matches"};
    return {};
  }

  if (!otherEndpoint.isNodePort())
    return {TIssueState::invalidConfig, "Connected endpoint type is no longer supported"};

  const auto *otherNode = document.findNode(otherEndpoint.nodeId);
  if (otherNode == nullptr)
    return {TIssueState::invalidConfig, "Connected node is missing"};

  const auto *otherPort = otherNode->findPort(otherEndpoint.portId);
  if (otherPort == nullptr)
    return {TIssueState::invalidConfig, "Connected port no longer exists"};
  if (otherPort->dataType != port.dataType)
    return {TIssueState::invalidConfig, "Connected port type no longer matches"};
  if (otherPort->direction == port.direction)
    return {TIssueState::invalidConfig, "Connected port direction is invalid"};

  return {};
}

static GroupedPortIssueSummary groupedPortIssueSummary(const TGraphDocument &document,
                                                       const TNode &node,
                                                       const GroupedNodePortSummary &group) {
  GroupedPortIssueSummary summary;
  for (const auto &connection : document.connections) {
    for (const auto *port : group.ports) {
      const TEndpoint *otherEndpoint = nullptr;
      if (connection.from.isNodePort() && connection.from.nodeId == node.nodeId &&
          connection.from.portId == port->portId) {
        otherEndpoint = &connection.to;
      } else if (connection.to.isNodePort() && connection.to.nodeId == node.nodeId &&
                 connection.to.portId == port->portId) {
        otherEndpoint = &connection.from;
      }

      if (otherEndpoint == nullptr)
        continue;

      const auto issue = issueSummaryForConnectedEndpoint(document, *port, *otherEndpoint);
      const auto merged = mergeIssueState(summary.state, issue.state);
      if (merged != summary.state ||
          (hasIssueState(issue.state) && summary.detail.isEmpty())) {
        summary.state = merged;
        if (hasIssueState(issue.state) && summary.detail.isEmpty())
          summary.detail = issue.detail;
        else if (issueStatePriority(issue.state) == issueStatePriority(summary.state) &&
                 issue.detail.isNotEmpty())
          summary.detail = issue.detail;
      }
    }
  }
  return summary;
}

static juce::String groupedPortIssueLabel(const GroupedNodePortSummary &group,
                                          const GroupedPortIssueSummary &summary) {
  if (!hasIssueState(summary.state))
    return {};

  juce::String text = group.displayName + " / " + groupedPortTypeLabel(group) +
                      " / " + issueStateLabel(summary.state);
  if (summary.detail.isNotEmpty())
    text << " / " << summary.detail;
  return text;
}

static TIssueState issueStateForRailEndpointLabel(const TControlSourceState &state,
                                             const TEndpoint &endpoint,
                                             const TSystemRailPort **railPortOut = nullptr) {
  if (railPortOut != nullptr)
    *railPortOut = nullptr;

  const auto *railEndpoint = state.findEndpoint(endpoint.railEndpointId);
  if (railEndpoint == nullptr)
    return TIssueState::invalidConfig;
  if (railEndpoint->missing)
    return TIssueState::missing;
  if (railEndpoint->degraded)
    return TIssueState::degraded;

  const auto *railPort = state.findEndpointPort(endpoint.railEndpointId, endpoint.railPortId);
  if (railPortOut != nullptr)
    *railPortOut = railPort;
  return railPort != nullptr ? TIssueState::none : TIssueState::invalidConfig;
}

static TIssueState issueStateForControlAssignment(const TControlSourceState &state,
                                                  const TControlSourceAssignment &assignment,
                                                  const TControlSource **sourceOut = nullptr) {
  if (sourceOut != nullptr)
    *sourceOut = nullptr;

  const auto *source = state.findSource(assignment.sourceId);
  if (sourceOut != nullptr)
    *sourceOut = source;
  if (source == nullptr)
    return TIssueState::invalidConfig;
  if (source->missing)
    return TIssueState::missing;
  if (source->degraded)
    return TIssueState::degraded;

  const bool hasPort = std::any_of(source->ports.begin(), source->ports.end(),
                                   [&](const TControlSourcePort &port) {
                                     return port.portId == assignment.portId;
                                   });
  return hasPort ? TIssueState::none : TIssueState::invalidConfig;
}

static juce::String formatControlAssignmentRange(float value) {
  return juce::String(value, 2);
}

static juce::String controlAssignmentSettingsText(const TControlSourceAssignment &assignment) {
  juce::StringArray parts;
  parts.add("Range " + formatControlAssignmentRange(assignment.rangeMin) + ".." +
            formatControlAssignmentRange(assignment.rangeMax));
  if (!assignment.enabled)
    parts.add("Off");
  if (assignment.inverted)
    parts.add("Inv");
  return parts.joinIntoString(" | ");
}

class NodePropertiesPanelImpl final : public NodePropertiesPanel,
                            private juce::Timer,
                            private ITeulParamProvider::Listener {
public:
  NodePropertiesPanelImpl(TGraphDocument &docIn, TGraphCanvas &canvasIn,
                      const TNodeRegistry &registryIn,
                      ITeulParamProvider *providerIn,
                      ParamBindingSummaryResolver bindingSummaryResolverIn)
      : document(docIn), canvas(canvasIn), registry(registryIn),
        bindingSummaryResolver(std::move(bindingSummaryResolverIn)) {
    addAndMakeVisible(headerLabel);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(nameEditor);
    addAndMakeVisible(colorBox);
    addAndMakeVisible(bypassToggle);
    addAndMakeVisible(collapsedToggle);
    addAndMakeVisible(applyButton);
    addAndMakeVisible(closeButton);
    addAndMakeVisible(paramViewport);
    addAndMakeVisible(nodeConnectionsBox);
    addAndMakeVisible(frameSummaryBox);
    addAndMakeVisible(frameCaptureButton);
    addAndMakeVisible(frameReleaseButton);
    addAndMakeVisible(frameFitButton);
    addAndMakeVisible(frameSavePresetButton);

    headerLabel.setText("Node Properties", juce::dontSendNotification);
    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.92f));
    headerLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));

    typeLabel.setJustificationType(juce::Justification::centredLeft);
    typeLabel.setColour(juce::Label::textColourId,
                        juce::Colours::white.withAlpha(0.54f));
    typeLabel.setFont(juce::FontOptions(10.8f, juce::Font::plain));

    nameEditor.setTextToShowWhenEmpty("Custom node label", juce::Colours::grey);
    nameEditor.setColour(juce::TextEditor::backgroundColourId,
                         juce::Colour(0xff171717));
    nameEditor.setColour(juce::TextEditor::textColourId,
                         juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::outlineColourId,
                         juce::Colour(0xff343434));

    colorBox.addItem("None", 1);
    colorBox.addItem("Red", 2);
    colorBox.addItem("Orange", 3);
    colorBox.addItem("Green", 4);
    colorBox.addItem("Blue", 5);
    colorBox.setSelectedId(1, juce::dontSendNotification);

    bypassToggle.setButtonText("Bypassed");
    collapsedToggle.setButtonText("Collapsed");
    applyButton.setButtonText("Apply");
    closeButton.setButtonText("Hide");

    paramsContent = std::make_unique<juce::Component>();
    paramViewport.setViewedComponent(paramsContent.get(), false);
    paramViewport.setScrollBarsShown(true, false);

    nodeConnectionsBox.setMultiLine(true);
    nodeConnectionsBox.setReadOnly(true);
    nodeConnectionsBox.setScrollbarsShown(true);
    nodeConnectionsBox.setColour(juce::TextEditor::backgroundColourId,
                                 juce::Colour(0x55111827));
    nodeConnectionsBox.setColour(juce::TextEditor::outlineColourId,
                                 juce::Colour(0xff2b394a));
    nodeConnectionsBox.setColour(juce::TextEditor::textColourId,
                                 juce::Colours::white.withAlpha(0.74f));
    nodeConnectionsBox.setVisible(false);

    frameSummaryBox.setMultiLine(true);
    frameSummaryBox.setReadOnly(true);
    frameSummaryBox.setScrollbarsShown(true);
    frameSummaryBox.setColour(juce::TextEditor::backgroundColourId,
                              juce::Colour(0x55111827));
    frameSummaryBox.setColour(juce::TextEditor::outlineColourId,
                              juce::Colour(0xff2b394a));
    frameSummaryBox.setColour(juce::TextEditor::textColourId,
                              juce::Colours::white.withAlpha(0.74f));
    frameSummaryBox.setVisible(false);

    frameCaptureButton.setButtonText("Capture Sel");
    frameReleaseButton.setButtonText("Release Sel");
    frameFitButton.setButtonText("Fit Members");
    frameSavePresetButton.setButtonText("Save Preset");

    frameCaptureButton.onClick = [this] {
      if (inspectedFrameId != 0 &&
          canvas.captureSelectedNodesIntoFrame(inspectedFrameId))
        rebuildFromDocument();
    };
    frameReleaseButton.onClick = [this] {
      if (inspectedFrameId != 0 &&
          canvas.releaseSelectedNodesFromFrame(inspectedFrameId))
        rebuildFromDocument();
    };
    frameFitButton.onClick = [this] {
      if (inspectedFrameId != 0 && canvas.fitFrameToMembers(inspectedFrameId))
        rebuildFromDocument();
    };
    frameSavePresetButton.onClick = [this] {
      if (inspectedFrameId != 0)
        canvas.saveFrameAsPatchPreset(inspectedFrameId);
    };

    applyButton.onClick = [this] { applyChanges(); };
    closeButton.onClick = [this] { hidePanel(); };

    setParamProvider(providerIn);
    setVisible(false);
  }

  ~NodePropertiesPanelImpl() override {
    stopTimer();
    if (paramProvider != nullptr)
      paramProvider->removeListener(this);
  }

  void setParamProvider(ITeulParamProvider *provider) {
    if (paramProvider == provider) {
      refreshRuntimeSurface();
      updateRuntimeValueLabels();
      return;
    }

    if (paramProvider != nullptr)
      paramProvider->removeListener(this);

    paramProvider = provider;
    runtimeParamsById.clear();

    if (paramProvider != nullptr) {
      paramProvider->addListener(this);
      refreshRuntimeSurface();
      startTimerHz(12);
    } else {
      stopTimer();
    }

    updateRuntimeValueLabels();
  }

  void setLayoutChangedCallback(std::function<void()> callback) {
    onLayoutChanged = std::move(callback);
  }

  bool isPanelOpen() const noexcept {
    return isVisible() &&
           (inspectedNodeId != kInvalidNodeId || inspectedFrameId != 0);
  }

  void inspectNode(NodeId nodeId) {
    inspectedFrameId = 0;
    inspectedNodeId = nodeId;
    rebuildFromDocument();
    setVisible(true);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void inspectFrame(int frameId) {
    inspectedNodeId = kInvalidNodeId;
    inspectedFrameId = frameId;
    rebuildFromDocument();
    setVisible(true);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void refreshFromDocument() {
    if (!isPanelOpen())
      return;

    if (inspectedFrameId != 0) {
      if (document.findFrame(inspectedFrameId) == nullptr) {
        hidePanel();
        return;
      }
    } else if (document.findNode(inspectedNodeId) == nullptr) {
      hidePanel();
      return;
    }

    rebuildFromDocument();
  }

  void refreshBindingSummaries() {
    if (!isPanelOpen())
      return;

    updateRuntimeValueLabels();
    if (!isInspectingFrame())
      refreshNodeConnectionSummary();
  }

  std::vector<AssignmentDropTarget>
  assignmentDropTargetsIn(juce::Component &target) const override {
    std::vector<AssignmentDropTarget> targets;
    if (!isPanelOpen() || isInspectingFrame() || paramsContent == nullptr)
      return targets;

    for (const auto &entry : paramEditors) {
      if (!isAssignableParam(*entry))
        continue;

      const auto bounds = boundsForParamEditor(*entry);
      if (bounds.isEmpty())
        continue;

      targets.push_back(
          {entry->paramId,
           target.getLocalArea(paramsContent.get(), bounds).toFloat()});
    }

    return targets;
  }

  void setAssignmentDropTarget(const juce::String &zoneId,
                               bool canConnect) override {
    if (assignmentDropTargetId == zoneId &&
        assignmentDropTargetCanConnect == canConnect)
      return;

    assignmentDropTargetId = zoneId;
    assignmentDropTargetCanConnect = canConnect;
    repaint();
  }

  void hidePanel() {
    if (!isVisible() && inspectedNodeId == kInvalidNodeId &&
        inspectedFrameId == 0)
      return;

    inspectedNodeId = kInvalidNodeId;
    inspectedFrameId = 0;
    assignmentDropTargetId.clear();
    assignmentDropTargetCanConnect = false;
    nodeConnectionsBox.clear();
    clearParamEditors();
    frameSummaryBox.clear();
    setVisible(false);
    if (onLayoutChanged)
      onLayoutChanged();
  }

  void paint(juce::Graphics &g) override {
    const auto panelBounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0b1220),
                                           panelBounds.getCentreX(),
                                           panelBounds.getY(),
                                           juce::Colour(0xff111827),
                                           panelBounds.getCentreX(),
                                           panelBounds.getBottom(), false));
    g.fillRoundedRectangle(panelBounds, 12.0f);
    g.setColour(juce::Colour(0xff324154));
    g.drawRoundedRectangle(panelBounds, 12.0f, 1.0f);

    if (!isPanelOpen())
      return;

    auto area = getLocalBounds().reduced(9);
    area.removeFromTop(22);
    area.removeFromTop(2);
    area.removeFromTop(16);
    area.removeFromTop(8);
    auto overview = area.removeFromTop(98);
    area.removeFromTop(8);
    auto state = area.removeFromTop(60);
    juce::Rectangle<int> connections;
    area.removeFromTop(10);
    if (!isInspectingFrame()) {
      connections = area.removeFromTop(94);
      area.removeFromTop(10);
    }
    auto params = area;

    auto drawSection = [&](juce::Rectangle<int> rect, const juce::String &title,
                           juce::Colour accent, const juce::String &subtitle) {
      g.setColour(juce::Colour(0x88182231));
      g.fillRoundedRectangle(rect.toFloat(), 10.0f);
      g.setColour(accent.withAlpha(0.32f));
      g.drawRoundedRectangle(rect.toFloat(), 10.0f, 1.0f);

      auto header = rect.reduced(10, 7)
                        .removeFromTop(subtitle.isNotEmpty() ? 24 : 16);
      g.setColour(juce::Colours::white.withAlpha(0.9f));
      g.setFont(juce::FontOptions(11.4f, juce::Font::bold));
      g.drawText(title, header.removeFromTop(15),
                 juce::Justification::centredLeft, false);
      if (subtitle.isNotEmpty()) {
        g.setColour(juce::Colours::white.withAlpha(0.48f));
        g.setFont(9.4f);
        g.drawText(subtitle, header, juce::Justification::centredLeft, false);
      }
    };

    drawSection(overview, "Overview", juce::Colour(0xff60a5fa), {});
    drawSection(state, "State", juce::Colour(0xfff59e0b), {});
    if (!isInspectingFrame()) {
      drawSection(connections, "Connection Setup", juce::Colour(0xfffbbf24),
                  "Signal routes and control assignments for the current node.");
    }
    drawSection(params, isInspectingFrame() ? "Group" : "Audio Parameters",
                juce::Colour(0xff22c55e),
                isInspectingFrame()
                    ? "Stored membership and identity metadata for this frame group."
                    : "Runtime and binding metadata appear only when relevant.");
  }
  void paintOverChildren(juce::Graphics &g) override {
    if (!isPanelOpen() || isInspectingFrame() || paramsContent == nullptr ||
        assignmentDropTargetId.isEmpty())
      return;

    const auto it = std::find_if(paramEditors.begin(), paramEditors.end(),
                                 [this](const auto &entry) {
                                   return entry->paramId == assignmentDropTargetId;
                                 });
    if (it == paramEditors.end())
      return;

    const auto bounds =
        getLocalArea(paramsContent.get(), boundsForParamEditor(*(*it))).toFloat();
    if (bounds.isEmpty())
      return;

    const auto accent = assignmentDropTargetCanConnect
                            ? juce::Colour(0xff22c55e)
                            : juce::Colour(0xfff97316);
    g.setColour(accent.withAlpha(assignmentDropTargetCanConnect ? 0.12f : 0.10f));
    g.fillRoundedRectangle(bounds.expanded(3.0f), 8.0f);
    g.setColour(accent.withAlpha(0.96f));
    g.drawRoundedRectangle(bounds.expanded(3.0f), 8.0f,
                           assignmentDropTargetCanConnect ? 2.0f : 1.6f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(9);
    auto header = area.removeFromTop(20);
    closeButton.setBounds(header.removeFromRight(48));
    headerLabel.setBounds(header);
    area.removeFromTop(2);
    typeLabel.setBounds(area.removeFromTop(14));
    area.removeFromTop(6);

    auto overview = area.removeFromTop(92).reduced(9, 20);
    nameEditor.setBounds(overview.removeFromTop(22));
    overview.removeFromTop(6);
    colorBox.setBounds(overview.removeFromTop(20));

    area.removeFromTop(6);
    auto state = area.removeFromTop(54).reduced(9, 20);
    auto toggles = state.removeFromTop(20);
    const int toggleWidth = juce::jmax(36, (toggles.getWidth() - 8) / 2);
    bypassToggle.setBounds(toggles.removeFromLeft(toggleWidth));
    toggles.removeFromLeft(8);
    collapsedToggle.setBounds(toggles);
    state.removeFromTop(6);
    applyButton.setBounds(state.removeFromTop(20).removeFromLeft(76));

    juce::Rectangle<int> connections;
    area.removeFromTop(8);
    if (!isInspectingFrame())
      connections = area.removeFromTop(88).reduced(9, 20);
    if (!isInspectingFrame())
      area.removeFromTop(8);
    auto params = area;
    params.removeFromTop(24);
    if (isInspectingFrame()) {
      nodeConnectionsBox.setBounds(0, 0, 0, 0);
      nodeConnectionsBox.setVisible(false);
      paramViewport.setBounds(0, 0, 0, 0);
      paramViewport.setVisible(false);
      frameSummaryBox.setVisible(true);

      auto actions = params.removeFromTop(44);
      auto topRow = actions.removeFromTop(20);
      const int topWidth = juce::jmax(76, (topRow.getWidth() - 6) / 2);
      frameCaptureButton.setVisible(true);
      frameCaptureButton.setBounds(topRow.removeFromLeft(topWidth));
      topRow.removeFromLeft(8);
      frameReleaseButton.setVisible(true);
      frameReleaseButton.setBounds(topRow);

      actions.removeFromTop(6);
      auto bottomRow = actions.removeFromTop(20);
      const int bottomWidth = juce::jmax(76, (bottomRow.getWidth() - 6) / 2);
      frameFitButton.setVisible(true);
      frameFitButton.setBounds(bottomRow.removeFromLeft(bottomWidth));
      bottomRow.removeFromLeft(8);
      frameSavePresetButton.setVisible(true);
      frameSavePresetButton.setBounds(bottomRow);

      params.removeFromTop(6);
      frameSummaryBox.setBounds(params);
    } else {
      frameCaptureButton.setVisible(false);
      frameReleaseButton.setVisible(false);
      frameFitButton.setVisible(false);
      frameSavePresetButton.setVisible(false);
      frameCaptureButton.setBounds(0, 0, 0, 0);
      frameReleaseButton.setBounds(0, 0, 0, 0);
      frameFitButton.setBounds(0, 0, 0, 0);
      frameSavePresetButton.setBounds(0, 0, 0, 0);
      frameSummaryBox.setBounds(0, 0, 0, 0);
      frameSummaryBox.setVisible(false);
      nodeConnectionsBox.setVisible(true);
      nodeConnectionsBox.setBounds(connections);
      paramViewport.setVisible(true);
      paramViewport.setBounds(params);
      layoutParamEditors();
    }
  }
private:
  bool isInspectingFrame() const noexcept { return inspectedFrameId != 0; }

  struct ParamEditor {
    TParamSpec spec;
    juce::String paramId;
    juce::var originalValue;
    std::unique_ptr<juce::Label> groupLabel;
    std::unique_ptr<juce::Label> caption;
    std::unique_ptr<juce::Label> descriptionLabel;
    std::unique_ptr<juce::Label> runtimeValueLabel;
    std::unique_ptr<juce::Label> bindingInfoLabel;
    std::unique_ptr<juce::Label> gyeolBindingLabel;
    std::unique_ptr<juce::Label> controlAssignmentLabel;
    std::unique_ptr<juce::Label> controlAssignmentSettingsLabel;
    std::unique_ptr<juce::TextEditor> controlAssignmentRangeMinEditor;
    std::unique_ptr<juce::TextEditor> controlAssignmentRangeMaxEditor;
    std::unique_ptr<juce::TextButton> controlAssignmentEnabledButton;
    std::unique_ptr<juce::TextButton> controlAssignmentInvertButton;
    std::unique_ptr<juce::TextButton> clearControlAssignmentButton;
    std::unique_ptr<juce::Component> editor;
    juce::String editableAssignmentSourceId;
    juce::String editableAssignmentPortId;
    NodeId editableAssignmentTargetNodeId = kInvalidNodeId;
  };

  void timerCallback() override {
    if (!isPanelOpen() || paramProvider == nullptr)
      return;

    bool didChange = false;
    for (auto &entry : paramEditors) {
      if (entry->paramId.isEmpty())
        continue;

      auto it = runtimeParamsById.find(entry->paramId);
      if (it == runtimeParamsById.end())
        continue;

      const juce::var nextValue = paramProvider->getParam(entry->paramId);
      if (!nextValue.isVoid() && !varEquals(nextValue, it->second.currentValue)) {
        it->second.currentValue = nextValue;
        didChange = true;
      }
    }

    if (didChange)
      updateRuntimeValueLabels();
  }

  void teulParamSurfaceChanged() override {
    refreshRuntimeSurface();
    if (isPanelOpen())
      updateRuntimeValueLabels();
  }

  void teulParamValueChanged(const TTeulExposedParam &param) override {
    runtimeParamsById[param.paramId] = param;
    if (isPanelOpen() && param.nodeId == inspectedNodeId)
      updateRuntimeValueLabels();
  }

  void refreshRuntimeSurface() {
    runtimeParamsById.clear();
    if (paramProvider == nullptr)
      return;

    for (const auto &param : paramProvider->listExposedParams())
      runtimeParamsById[param.paramId] = param;
  }

  void rebuildFromDocument() {
    if (isInspectingFrame()) {
      const TFrameRegion *frame = document.findFrame(inspectedFrameId);
      if (frame == nullptr) {
        hidePanel();
        return;
      }

      clearParamEditors();
      headerLabel.setText(frame->title, juce::dontSendNotification);
      typeLabel.setText(frameTypeSummary(*frame), juce::dontSendNotification);
      nameEditor.setText(frame->title, juce::dontSendNotification);
      colorBox.setSelectedId(frameColorToId(frame->colorArgb),
                             juce::dontSendNotification);
      bypassToggle.setButtonText("Locked");
      bypassToggle.setToggleState(frame->locked, juce::dontSendNotification);
      collapsedToggle.setButtonText("Collapsed");
      collapsedToggle.setToggleState(frame->collapsed, juce::dontSendNotification);
      frameSummaryBox.setText(frameSummaryText(*frame), juce::dontSendNotification);

      const bool hasSelectedNodes = !canvas.getSelectedNodeIds().empty();
      const bool hasSelectedMembers = std::any_of(
          canvas.getSelectedNodeIds().begin(), canvas.getSelectedNodeIds().end(),
          [&](NodeId nodeId) { return frame->containsNode(nodeId); });
      frameCaptureButton.setEnabled(hasSelectedNodes);
      frameReleaseButton.setEnabled(hasSelectedMembers);
      frameFitButton.setEnabled(frame->membershipExplicit &&
                                !frame->memberNodeIds.empty());
      frameSavePresetButton.setEnabled(!frame->memberNodeIds.empty());

      resized();
      repaint();
      return;
    }

    const TNode *node = document.findNode(inspectedNodeId);
    if (node == nullptr) {
      hidePanel();
      return;
    }

    refreshRuntimeSurface();

    const auto *desc = registry.descriptorFor(node->typeKey);
    headerLabel.setText(nodeLabelForInspector(*node, registry),
                        juce::dontSendNotification);
    typeLabel.setText(desc != nullptr ? desc->displayName + " / " + desc->typeKey
                                      : node->typeKey,
                      juce::dontSendNotification);
    nameEditor.setText(node->label, juce::dontSendNotification);
    colorBox.setSelectedId(colorTagToId(node->colorTag),
                           juce::dontSendNotification);
    bypassToggle.setButtonText("Bypassed");
    bypassToggle.setToggleState(node->bypassed, juce::dontSendNotification);
    collapsedToggle.setButtonText("Collapsed");
    collapsedToggle.setToggleState(node->collapsed, juce::dontSendNotification);

    refreshNodeConnectionSummary();

    rebuildParamEditors(*node, desc);
    resized();
    repaint();
  }

  void clearParamEditors() {
    if (paramsContent != nullptr)
      paramsContent->removeAllChildren();

    paramEditors.clear();
    if (paramsContent != nullptr)
      paramsContent->setSize(0, 0);
  }

  void rebuildParamEditors(const TNode &node, const TNodeDescriptor *desc) {
    clearParamEditors();
    std::set<juce::String> seenKeys;
    juce::String lastGroup;

    auto appendParam = [&](TParamSpec spec, const juce::var &value) {
      if (!spec.showInPropertyPanel)
        return;

      auto entry = std::make_unique<ParamEditor>();
      entry->spec = std::move(spec);
      entry->paramId = makeTeulParamId(node.nodeId, entry->spec.key);
      entry->originalValue = value;

      if (entry->spec.group.isNotEmpty() && entry->spec.group != lastGroup) {
        entry->groupLabel = std::make_unique<juce::Label>();
        entry->groupLabel->setText(entry->spec.group, juce::dontSendNotification);
        entry->groupLabel->setJustificationType(juce::Justification::centredLeft);
        entry->groupLabel->setColour(juce::Label::textColourId,
                                     juce::Colours::white.withAlpha(0.86f));
        entry->groupLabel->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        paramsContent->addAndMakeVisible(entry->groupLabel.get());
        lastGroup = entry->spec.group;
      }

      entry->caption = std::make_unique<juce::Label>();
      entry->caption->setText(entry->spec.label.isNotEmpty() ? entry->spec.label
                                                             : entry->spec.key,
                              juce::dontSendNotification);
      entry->caption->setJustificationType(juce::Justification::centredLeft);
      entry->caption->setColour(juce::Label::textColourId,
                                juce::Colours::white.withAlpha(0.74f));
      paramsContent->addAndMakeVisible(entry->caption.get());

      entry->editor = createParamEditor(entry->spec, value);

      entry->descriptionLabel = std::make_unique<juce::Label>();
      entry->descriptionLabel->setText(entry->spec.description,
                                       juce::dontSendNotification);
      entry->descriptionLabel->setJustificationType(juce::Justification::centredLeft);
      entry->descriptionLabel->setColour(juce::Label::textColourId,
                                         juce::Colours::white.withAlpha(0.46f));
      entry->descriptionLabel->setFont(juce::FontOptions(10.0f));
      entry->descriptionLabel->setVisible(entry->spec.description.isNotEmpty());
      paramsContent->addAndMakeVisible(entry->descriptionLabel.get());

      entry->runtimeValueLabel = std::make_unique<juce::Label>();
      entry->runtimeValueLabel->setJustificationType(juce::Justification::centredLeft);
      entry->runtimeValueLabel->setColour(juce::Label::textColourId,
                                          juce::Colour(0xff8fb8ff));
      entry->runtimeValueLabel->setFont(juce::FontOptions(9.6f, juce::Font::bold));
      entry->runtimeValueLabel->setBorderSize(juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->runtimeValueLabel.get());

      entry->bindingInfoLabel = std::make_unique<juce::Label>();
      entry->bindingInfoLabel->setJustificationType(juce::Justification::centredLeft);
      entry->bindingInfoLabel->setColour(juce::Label::textColourId,
                                         juce::Colours::white.withAlpha(0.44f));
      entry->bindingInfoLabel->setFont(juce::FontOptions(9.4f));
      entry->bindingInfoLabel->setBorderSize(juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->bindingInfoLabel.get());

      entry->gyeolBindingLabel = std::make_unique<juce::Label>();
      entry->gyeolBindingLabel->setJustificationType(juce::Justification::centredLeft);
      entry->gyeolBindingLabel->setColour(juce::Label::textColourId,
                                          juce::Colours::white.withAlpha(0.32f));
      entry->gyeolBindingLabel->setFont(juce::FontOptions(9.4f));
      entry->gyeolBindingLabel->setBorderSize(juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->gyeolBindingLabel.get());

      entry->controlAssignmentLabel = std::make_unique<juce::Label>();
      entry->controlAssignmentLabel->setJustificationType(
          juce::Justification::centredLeft);
      entry->controlAssignmentLabel->setColour(juce::Label::textColourId,
                                               juce::Colour(0xfffbbf24));
      entry->controlAssignmentLabel->setFont(juce::FontOptions(9.4f));
      entry->controlAssignmentLabel->setBorderSize(
          juce::BorderSize<int>(1, 6, 1, 6));
      paramsContent->addAndMakeVisible(entry->controlAssignmentLabel.get());

      entry->controlAssignmentSettingsLabel = std::make_unique<juce::Label>();
      entry->controlAssignmentSettingsLabel->setJustificationType(
          juce::Justification::centredLeft);
      entry->controlAssignmentSettingsLabel->setColour(
          juce::Label::textColourId, juce::Colours::white.withAlpha(0.58f));
      entry->controlAssignmentSettingsLabel->setFont(juce::FontOptions(9.1f));
      entry->controlAssignmentSettingsLabel->setBorderSize(
          juce::BorderSize<int>(1, 6, 1, 6));
      entry->controlAssignmentSettingsLabel->setVisible(false);
      paramsContent->addAndMakeVisible(entry->controlAssignmentSettingsLabel.get());

      entry->controlAssignmentRangeMinEditor = std::make_unique<juce::TextEditor>();
      entry->controlAssignmentRangeMinEditor->setTextToShowWhenEmpty("Min", juce::Colours::grey);
      entry->controlAssignmentRangeMinEditor->setColour(
          juce::TextEditor::backgroundColourId, juce::Colour(0xff171717));
      entry->controlAssignmentRangeMinEditor->setColour(
          juce::TextEditor::textColourId, juce::Colours::white);
      entry->controlAssignmentRangeMinEditor->setColour(
          juce::TextEditor::outlineColourId, juce::Colour(0xff324154));
      entry->controlAssignmentRangeMinEditor->setColour(
          juce::CaretComponent::caretColourId, juce::Colours::white);
      entry->controlAssignmentRangeMinEditor->setInputRestrictions(0, "0123456789.-");
      entry->controlAssignmentRangeMinEditor->setVisible(false);
      entry->controlAssignmentRangeMinEditor->setTooltip(
          "Assignment range minimum (0.00 to 1.00)");
      const auto rangeMinParamId = entry->paramId;
      entry->controlAssignmentRangeMinEditor->onReturnKey = [this, rangeMinParamId] {
        commitControlAssignmentRangeForParam(rangeMinParamId);
      };
      entry->controlAssignmentRangeMinEditor->onFocusLost = [this, rangeMinParamId] {
        commitControlAssignmentRangeForParam(rangeMinParamId);
      };
      paramsContent->addAndMakeVisible(entry->controlAssignmentRangeMinEditor.get());

      entry->controlAssignmentRangeMaxEditor = std::make_unique<juce::TextEditor>();
      entry->controlAssignmentRangeMaxEditor->setTextToShowWhenEmpty("Max", juce::Colours::grey);
      entry->controlAssignmentRangeMaxEditor->setColour(
          juce::TextEditor::backgroundColourId, juce::Colour(0xff171717));
      entry->controlAssignmentRangeMaxEditor->setColour(
          juce::TextEditor::textColourId, juce::Colours::white);
      entry->controlAssignmentRangeMaxEditor->setColour(
          juce::TextEditor::outlineColourId, juce::Colour(0xff324154));
      entry->controlAssignmentRangeMaxEditor->setColour(
          juce::CaretComponent::caretColourId, juce::Colours::white);
      entry->controlAssignmentRangeMaxEditor->setInputRestrictions(0, "0123456789.-");
      entry->controlAssignmentRangeMaxEditor->setVisible(false);
      entry->controlAssignmentRangeMaxEditor->setTooltip(
          "Assignment range maximum (0.00 to 1.00)");
      const auto rangeMaxParamId = entry->paramId;
      entry->controlAssignmentRangeMaxEditor->onReturnKey = [this, rangeMaxParamId] {
        commitControlAssignmentRangeForParam(rangeMaxParamId);
      };
      entry->controlAssignmentRangeMaxEditor->onFocusLost = [this, rangeMaxParamId] {
        commitControlAssignmentRangeForParam(rangeMaxParamId);
      };
      paramsContent->addAndMakeVisible(entry->controlAssignmentRangeMaxEditor.get());

      entry->controlAssignmentEnabledButton =
          std::make_unique<juce::TextButton>("On");
      entry->controlAssignmentEnabledButton->setClickingTogglesState(true);
      entry->controlAssignmentEnabledButton->setColour(
          juce::TextButton::buttonColourId, juce::Colour(0x2222c55e));
      entry->controlAssignmentEnabledButton->setColour(
          juce::TextButton::buttonOnColourId, juce::Colour(0x4422c55e));
      entry->controlAssignmentEnabledButton->setColour(
          juce::TextButton::textColourOffId,
          juce::Colours::white.withAlpha(0.84f));
      entry->controlAssignmentEnabledButton->setVisible(false);
      entry->controlAssignmentEnabledButton->setTooltip(
          "Enable or disable this control assignment");
      const auto enabledParamId = entry->paramId;
      auto *enabledButton = entry->controlAssignmentEnabledButton.get();
      entry->controlAssignmentEnabledButton->onClick =
          [this, enabledParamId, enabledButton] {
            setControlAssignmentEnabledForParam(enabledParamId,
                                                enabledButton->getToggleState());
          };
      paramsContent->addAndMakeVisible(entry->controlAssignmentEnabledButton.get());

      entry->controlAssignmentInvertButton =
          std::make_unique<juce::TextButton>("Inv");
      entry->controlAssignmentInvertButton->setClickingTogglesState(true);
      entry->controlAssignmentInvertButton->setColour(
          juce::TextButton::buttonColourId, juce::Colour(0x223b82f6));
      entry->controlAssignmentInvertButton->setColour(
          juce::TextButton::buttonOnColourId, juce::Colour(0x443b82f6));
      entry->controlAssignmentInvertButton->setColour(
          juce::TextButton::textColourOffId,
          juce::Colours::white.withAlpha(0.84f));
      entry->controlAssignmentInvertButton->setVisible(false);
      entry->controlAssignmentInvertButton->setTooltip(
          "Invert this control assignment");
      const auto invertParamId = entry->paramId;
      auto *invertButton = entry->controlAssignmentInvertButton.get();
      entry->controlAssignmentInvertButton->onClick =
          [this, invertParamId, invertButton] {
            setControlAssignmentInvertedForParam(invertParamId,
                                                 invertButton->getToggleState());
          };
      paramsContent->addAndMakeVisible(entry->controlAssignmentInvertButton.get());

      entry->clearControlAssignmentButton =
          std::make_unique<juce::TextButton>("Clear");
      entry->clearControlAssignmentButton->setTooltip(
          "Remove control assignments from this parameter");
      entry->clearControlAssignmentButton->setColour(
          juce::TextButton::buttonColourId, juce::Colour(0x22f59e0b));
      entry->clearControlAssignmentButton->setColour(
          juce::TextButton::buttonOnColourId, juce::Colour(0x33f59e0b));
      entry->clearControlAssignmentButton->setColour(
          juce::TextButton::textColourOffId,
          juce::Colours::white.withAlpha(0.84f));
      entry->clearControlAssignmentButton->setVisible(false);
      const auto targetParamId = entry->paramId;
      entry->clearControlAssignmentButton->onClick = [this, targetParamId] {
        clearControlAssignmentsForParam(targetParamId);
      };
      paramsContent->addAndMakeVisible(entry->clearControlAssignmentButton.get());

      paramsContent->addAndMakeVisible(entry->editor.get());
      paramEditors.push_back(std::move(entry));
    };

    if (desc != nullptr) {
      for (const auto &spec : desc->paramSpecs) {
        seenKeys.insert(spec.key);
        if (!spec.showInPropertyPanel)
          continue;

        const auto it = node.params.find(spec.key);
        appendParam(spec, it != node.params.end() ? it->second : spec.defaultValue);
      }
    }

    for (const auto &pair : node.params) {
      if (seenKeys.find(pair.first) != seenKeys.end())
        continue;

      TParamSpec spec = makeFallbackParamSpec(pair.first, pair.second);
      const juce::String paramId = makeTeulParamId(node.nodeId, pair.first);
      if (const auto it = runtimeParamsById.find(paramId); it != runtimeParamsById.end())
        spec = makeSpecFromExposedParam(it->second);

      appendParam(std::move(spec), pair.second);
    }

    updateRuntimeValueLabels();
  }

  void layoutParamEditors() {
    if (paramsContent == nullptr)
      return;

    const int width = juce::jmax(136, paramViewport.getWidth() - 12);
    int y = 0;

    for (auto &entry : paramEditors) {
      if (entry->groupLabel != nullptr) {
        entry->groupLabel->setBounds(0, y, width, 15);
        y += 16;
      }

      entry->caption->setBounds(0, y, width, 15);
      y += 17;

      const int editorHeight = editorHeightFor(*entry->editor);
      entry->editor->setBounds(0, y, width, editorHeight);
      y += editorHeight + 4;

      if (entry->descriptionLabel->isVisible()) {
        entry->descriptionLabel->setBounds(0, y, width, 12);
        y += 14;
      }

      if (entry->runtimeValueLabel->isVisible()) {
        entry->runtimeValueLabel->setBounds(0, y, width, 15);
        y += 16;
      } else {
        entry->runtimeValueLabel->setBounds(0, 0, 0, 0);
      }

      if (entry->bindingInfoLabel->isVisible()) {
        entry->bindingInfoLabel->setBounds(0, y, width, 15);
        y += 16;
      } else {
        entry->bindingInfoLabel->setBounds(0, 0, 0, 0);
      }

      if (entry->gyeolBindingLabel->isVisible()) {
        entry->gyeolBindingLabel->setBounds(0, y, width, 15);
        y += 16;
      } else {
        entry->gyeolBindingLabel->setBounds(0, 0, 0, 0);
      }

      if (entry->controlAssignmentLabel->isVisible()) {
        auto assignmentRow = juce::Rectangle<int>(0, y, width, 15);
        if (entry->clearControlAssignmentButton != nullptr &&
            entry->clearControlAssignmentButton->isVisible()) {
          auto buttonBounds = assignmentRow.removeFromRight(44);
          entry->clearControlAssignmentButton->setBounds(buttonBounds);
          assignmentRow.removeFromRight(4);
        } else if (entry->clearControlAssignmentButton != nullptr) {
          entry->clearControlAssignmentButton->setBounds(0, 0, 0, 0);
        }
        entry->controlAssignmentLabel->setBounds(assignmentRow);
        y += 18;

        if (entry->controlAssignmentSettingsLabel != nullptr &&
            entry->controlAssignmentSettingsLabel->isVisible()) {
          auto settingsRow = juce::Rectangle<int>(0, y, width, 16);
          if (entry->controlAssignmentInvertButton != nullptr &&
              entry->controlAssignmentInvertButton->isVisible()) {
            auto invertBounds = settingsRow.removeFromRight(36);
            entry->controlAssignmentInvertButton->setBounds(invertBounds);
            settingsRow.removeFromRight(3);
          } else if (entry->controlAssignmentInvertButton != nullptr) {
            entry->controlAssignmentInvertButton->setBounds(0, 0, 0, 0);
          }
          if (entry->controlAssignmentEnabledButton != nullptr &&
              entry->controlAssignmentEnabledButton->isVisible()) {
            auto enabledBounds = settingsRow.removeFromRight(36);
            entry->controlAssignmentEnabledButton->setBounds(enabledBounds);
            settingsRow.removeFromRight(4);
          } else if (entry->controlAssignmentEnabledButton != nullptr) {
            entry->controlAssignmentEnabledButton->setBounds(0, 0, 0, 0);
          }
          if (entry->controlAssignmentRangeMaxEditor != nullptr &&
              entry->controlAssignmentRangeMaxEditor->isVisible()) {
            auto maxBounds = settingsRow.removeFromRight(44);
            entry->controlAssignmentRangeMaxEditor->setBounds(maxBounds);
            settingsRow.removeFromRight(4);
          } else if (entry->controlAssignmentRangeMaxEditor != nullptr) {
            entry->controlAssignmentRangeMaxEditor->setBounds(0, 0, 0, 0);
          }
          if (entry->controlAssignmentRangeMinEditor != nullptr &&
              entry->controlAssignmentRangeMinEditor->isVisible()) {
            auto minBounds = settingsRow.removeFromRight(44);
            entry->controlAssignmentRangeMinEditor->setBounds(minBounds);
            settingsRow.removeFromRight(6);
          } else if (entry->controlAssignmentRangeMinEditor != nullptr) {
            entry->controlAssignmentRangeMinEditor->setBounds(0, 0, 0, 0);
          }
          entry->controlAssignmentSettingsLabel->setBounds(settingsRow);
          y += 18;
        } else {
          if (entry->controlAssignmentSettingsLabel != nullptr)
            entry->controlAssignmentSettingsLabel->setBounds(0, 0, 0, 0);
          if (entry->controlAssignmentRangeMinEditor != nullptr)
            entry->controlAssignmentRangeMinEditor->setBounds(0, 0, 0, 0);
          if (entry->controlAssignmentRangeMaxEditor != nullptr)
            entry->controlAssignmentRangeMaxEditor->setBounds(0, 0, 0, 0);
          if (entry->controlAssignmentEnabledButton != nullptr)
            entry->controlAssignmentEnabledButton->setBounds(0, 0, 0, 0);
          if (entry->controlAssignmentInvertButton != nullptr)
            entry->controlAssignmentInvertButton->setBounds(0, 0, 0, 0);
        }
      } else {
        entry->controlAssignmentLabel->setBounds(0, 0, 0, 0);
        if (entry->controlAssignmentSettingsLabel != nullptr)
          entry->controlAssignmentSettingsLabel->setBounds(0, 0, 0, 0);
        if (entry->controlAssignmentRangeMinEditor != nullptr)
          entry->controlAssignmentRangeMinEditor->setBounds(0, 0, 0, 0);
        if (entry->controlAssignmentRangeMaxEditor != nullptr)
          entry->controlAssignmentRangeMaxEditor->setBounds(0, 0, 0, 0);
        if (entry->controlAssignmentEnabledButton != nullptr)
          entry->controlAssignmentEnabledButton->setBounds(0, 0, 0, 0);
        if (entry->controlAssignmentInvertButton != nullptr)
          entry->controlAssignmentInvertButton->setBounds(0, 0, 0, 0);
        if (entry->clearControlAssignmentButton != nullptr)
          entry->clearControlAssignmentButton->setBounds(0, 0, 0, 0);
      }

      y += 5;
    }

    paramsContent->setSize(width, juce::jmax(y, paramViewport.getHeight()));
  }
  juce::var readEditorValue(const ParamEditor &entry) const {
    return Teul::readEditorValue(*entry.editor, entry.spec, entry.originalValue);
  }

  bool isAssignableParam(const ParamEditor &entry) const {
    return entry.paramId.isNotEmpty() && !entry.spec.isReadOnly &&
           (entry.spec.isAutomatable || entry.spec.isModulatable);
  }

  juce::Rectangle<int> boundsForParamEditor(const ParamEditor &entry) const {
    juce::Rectangle<int> bounds;
    auto unionWith = [&](const juce::Component *component) {
      if (component == nullptr || !component->isVisible())
        return;
      bounds = bounds.isEmpty() ? component->getBounds()
                               : bounds.getUnion(component->getBounds());
    };

    unionWith(entry.caption.get());
    unionWith(entry.editor.get());
    unionWith(entry.descriptionLabel.get());
    unionWith(entry.runtimeValueLabel.get());
    unionWith(entry.bindingInfoLabel.get());
    unionWith(entry.gyeolBindingLabel.get());
    unionWith(entry.controlAssignmentLabel.get());
    unionWith(entry.controlAssignmentSettingsLabel.get());
    unionWith(entry.controlAssignmentRangeMinEditor.get());
    unionWith(entry.controlAssignmentRangeMaxEditor.get());
    unionWith(entry.controlAssignmentEnabledButton.get());
    unionWith(entry.controlAssignmentInvertButton.get());
    unionWith(entry.clearControlAssignmentButton.get());
    return bounds.expanded(4, 4);
  }

  juce::String controlAssignmentSummaryForParam(const ParamEditor &entry,
                                             TIssueState *issueStateOut = nullptr) const {
    if (issueStateOut != nullptr)
      *issueStateOut = TIssueState::none;

    if (entry.paramId.isEmpty())
      return {};

    TIssueState issueState = TIssueState::none;
    juce::StringArray labels;
    for (const auto &assignment : document.controlState.assignments) {
      if (assignment.targetParamId != entry.paramId)
        continue;

      juce::String sourceLabel = assignment.sourceId;
      juce::String portLabel = assignment.portId;
      const TControlSource *source = nullptr;
      const auto assignmentIssue =
          issueStateForControlAssignment(document.controlState, assignment, &source);
      if (source != nullptr) {
        sourceLabel = source->displayName.isNotEmpty() ? source->displayName
                                                       : source->sourceId;
        for (const auto &port : source->ports) {
          if (port.portId == assignment.portId) {
            portLabel = port.displayName;
            break;
          }
        }
      }

      issueState = mergeIssueState(issueState, assignmentIssue);
      juce::String label = sourceLabel + " / " + portLabel + " / " +
                           controlAssignmentSettingsText(assignment);
      if (hasIssueState(assignmentIssue))
        label << " / " << issueStateLabel(assignmentIssue);
      labels.add(std::move(label));
    }

    if (issueStateOut != nullptr)
      *issueStateOut = issueState;

    if (labels.isEmpty())
      return {};

    juce::String text = "Control: " + labels[0];
    if (labels.size() > 1)
      text << " +" << juce::String(labels.size() - 1);
    return text;
  }
  ParamEditor *findParamEditorById(const juce::String &paramId) {
    const auto it = std::find_if(paramEditors.begin(), paramEditors.end(),
                                 [&](const auto &entry) {
                                   return entry->paramId == paramId;
                                 });
    return it != paramEditors.end() ? it->get() : nullptr;
  }

  TControlSourceAssignment *findEditableControlAssignment(ParamEditor &entry) {
    if (entry.editableAssignmentSourceId.isEmpty() ||
        entry.editableAssignmentPortId.isEmpty() ||
        entry.editableAssignmentTargetNodeId == kInvalidNodeId ||
        entry.paramId.isEmpty()) {
      return nullptr;
    }

    const auto it = std::find_if(
        document.controlState.assignments.begin(),
        document.controlState.assignments.end(),
        [&](TControlSourceAssignment &assignment) {
          return assignment.sourceId == entry.editableAssignmentSourceId &&
                 assignment.portId == entry.editableAssignmentPortId &&
                 assignment.targetNodeId == entry.editableAssignmentTargetNodeId &&
                 assignment.targetParamId == entry.paramId;
        });
    return it != document.controlState.assignments.end() ? &*it : nullptr;
  }

  void mutateSingleControlAssignmentForParam(
      const juce::String &paramId,
      const std::function<bool(TControlSourceAssignment &)> &mutate) {
    auto *entry = findParamEditorById(paramId);
    if (entry == nullptr)
      return;

    auto *assignment = findEditableControlAssignment(*entry);
    if (assignment == nullptr)
      return;

    if (!mutate(*assignment))
      return;

    document.touch(false);
    refreshNodeConnectionSummary();
    updateRuntimeValueLabels();
    canvas.repaint();
  }

  void setControlAssignmentEnabledForParam(const juce::String &paramId,
                                           bool enabled) {
    mutateSingleControlAssignmentForParam(
        paramId, [enabled](TControlSourceAssignment &assignment) {
          if (assignment.enabled == enabled)
            return false;
          assignment.enabled = enabled;
          return true;
        });
  }

  void setControlAssignmentInvertedForParam(const juce::String &paramId,
                                            bool inverted) {
    mutateSingleControlAssignmentForParam(
        paramId, [inverted](TControlSourceAssignment &assignment) {
          if (assignment.inverted == inverted)
            return false;
          assignment.inverted = inverted;
          return true;
        });
  }

  static float clampAssignmentRangeValue(float value) {
    return juce::jlimit(0.0f, 1.0f, value);
  }

  void commitControlAssignmentRangeForParam(const juce::String &paramId) {
    auto *entry = findParamEditorById(paramId);
    if (entry == nullptr || entry->controlAssignmentRangeMinEditor == nullptr ||
        entry->controlAssignmentRangeMaxEditor == nullptr) {
      return;
    }

    auto *assignment = findEditableControlAssignment(*entry);
    if (assignment == nullptr)
      return;

    const auto parseOrFallback = [](const juce::String &text, float fallback) {
      const auto trimmed = text.trim();
      if (trimmed.isEmpty())
        return fallback;
      const double parsed = trimmed.getDoubleValue();
      return std::isfinite(parsed) ? (float)parsed : fallback;
    };

    float nextMin = clampAssignmentRangeValue(parseOrFallback(
        entry->controlAssignmentRangeMinEditor->getText(), assignment->rangeMin));
    float nextMax = clampAssignmentRangeValue(parseOrFallback(
        entry->controlAssignmentRangeMaxEditor->getText(), assignment->rangeMax));
    if (nextMin > nextMax)
      std::swap(nextMin, nextMax);

    const bool changed = std::abs(nextMin - assignment->rangeMin) > 0.0001f ||
                         std::abs(nextMax - assignment->rangeMax) > 0.0001f;
    assignment->rangeMin = nextMin;
    assignment->rangeMax = nextMax;

    entry->controlAssignmentRangeMinEditor->setText(
        formatControlAssignmentRange(nextMin), juce::dontSendNotification);
    entry->controlAssignmentRangeMaxEditor->setText(
        formatControlAssignmentRange(nextMax), juce::dontSendNotification);

    if (!changed)
      return;

    document.touch(false);
    refreshNodeConnectionSummary();
    updateRuntimeValueLabels();
    canvas.repaint();
  }

  void clearControlAssignmentsForParam(const juce::String &paramId) {
    if (paramId.isEmpty())
      return;

    const auto beforeCount = document.controlState.assignments.size();
    document.controlState.assignments.erase(
        std::remove_if(document.controlState.assignments.begin(),
                       document.controlState.assignments.end(),
                       [&](const TControlSourceAssignment &assignment) {
                         return assignment.targetParamId == paramId;
                       }),
        document.controlState.assignments.end());

    if (document.controlState.assignments.size() == beforeCount)
      return;

    document.touch(false);
    refreshNodeConnectionSummary();
    updateRuntimeValueLabels();
    canvas.repaint();
  }

  juce::String nodeNameForSummary(const TNode &node) const {
    return nodeLabelForInspector(node, registry);
  }

  juce::String portNameForSummary(const TNode &node, PortId portId) const {
    if (const auto *port = node.findPort(portId))
      return port->name;
    return "Port";
  }

  juce::String buildNodeConnectionSummary(const TNode &node) const {
    juce::StringArray incoming;
    juce::StringArray outgoing;
    juce::StringArray portUsage;
    juce::StringArray portRules;
    juce::StringArray connectionIssues;
    juce::StringArray controls;

    for (const auto &group : groupNodePortsForSummary(node)) {
      const bool incomingDirection = group.direction == TPortDirection::Input;
      const auto counts =
          groupedPortConnectionCounts(document, group, incomingDirection);
      const int capacity = groupedPortCapacity(group, incomingDirection);
      portUsage.add(groupedPortUsageLabel(group, counts, capacity,
                                          incomingDirection));
      portRules.add(groupedPortRuleLabel(group, capacity));
      const auto issueSummary = groupedPortIssueSummary(document, node, group);
      const auto issueLabel = groupedPortIssueLabel(group, issueSummary);
      if (issueLabel.isNotEmpty())
        connectionIssues.add(issueLabel);
    }

    auto railPortLabel = [this](const TEndpoint &endpoint) {
      if (!endpoint.isRailPort())
        return juce::String();

      const TSystemRailPort *railPort = nullptr;
      const auto issueState = issueStateForRailEndpointLabel(
          document.controlState, endpoint, &railPort);
      const auto *railEndpoint =
          document.controlState.findEndpoint(endpoint.railEndpointId);
      if (railEndpoint == nullptr || railPort == nullptr)
        return endpoint.railEndpointId + " / " + endpoint.railPortId +
               (hasIssueState(issueState) ? " / " + issueStateLabel(issueState)
                                          : juce::String());

      juce::String text = railEndpoint->displayName + " / " + railPort->displayName;
      if (hasIssueState(issueState))
        text << " / " << issueStateLabel(issueState);
      return text;
    };

    for (const auto &conn : document.connections) {
      if (conn.to.isNodePort() && conn.to.nodeId == node.nodeId) {
        if (conn.from.isNodePort()) {
          if (const auto *sourceNode = document.findNode(conn.from.nodeId)) {
            incoming.add(nodeNameForSummary(*sourceNode) + " / " +
                         portNameForSummary(*sourceNode, conn.from.portId) +
                         " -> " + portNameForSummary(node, conn.to.portId));
          }
        } else if (conn.from.isRailPort()) {
          incoming.add(railPortLabel(conn.from) + " -> " +
                       portNameForSummary(node, conn.to.portId));
        }
      }

      if (conn.from.isNodePort() && conn.from.nodeId == node.nodeId) {
        if (conn.to.isNodePort()) {
          if (const auto *targetNode = document.findNode(conn.to.nodeId)) {
            outgoing.add(portNameForSummary(node, conn.from.portId) + " -> " +
                         nodeNameForSummary(*targetNode) + " / " +
                         portNameForSummary(*targetNode, conn.to.portId));
          }
        } else if (conn.to.isRailPort()) {
          outgoing.add(portNameForSummary(node, conn.from.portId) + " -> " +
                       railPortLabel(conn.to));
        }
      }
    }

    for (const auto &assignment : document.controlState.assignments) {
      if (assignment.targetNodeId != node.nodeId)
        continue;

      juce::String sourceLabel = assignment.sourceId;
      juce::String portLabel = assignment.portId;
      const TControlSource *source = nullptr;
      const auto assignmentIssue =
          issueStateForControlAssignment(document.controlState, assignment, &source);
      if (source != nullptr) {
        sourceLabel = source->displayName.isNotEmpty() ? source->displayName
                                                       : source->sourceId;
        for (const auto &port : source->ports) {
          if (port.portId == assignment.portId) {
            portLabel = port.displayName;
            break;
          }
        }
      }

      NodeId parsedNodeId = kInvalidNodeId;
      juce::String paramKey;
      juce::String targetLabel = assignment.targetParamId;
      if (parseTeulParamId(assignment.targetParamId, parsedNodeId, paramKey) &&
          parsedNodeId == node.nodeId && paramKey.isNotEmpty())
        targetLabel = paramKey;

      juce::String line = sourceLabel + " / " + portLabel + " -> " + targetLabel;
      line << " / " << controlAssignmentSettingsText(assignment);
      if (hasIssueState(assignmentIssue)) {
        line << " / " << issueStateLabel(assignmentIssue);
        connectionIssues.add(line);
      }
      controls.add(std::move(line));
    }

    auto joinSection = [](const juce::String &title,
                          const juce::StringArray &lines,
                          const juce::String &emptyText) {
      juce::String text = title + "\r\n";
      if (lines.isEmpty()) {
        text << "- " << emptyText;
      } else {
        for (const auto &line : lines)
          text << "- " << line << "\r\n";
        text = text.trimEnd();
      }
      return text;
    };

    juce::StringArray sections;
    sections.add(joinSection("Port Usage", portUsage, "No ports"));
    sections.add(joinSection("Connection Rules", portRules,
                             "No explicit port rules"));
    sections.add(joinSection("Connection Issues", connectionIssues,
                             "No active issues"));
    sections.add(joinSection("Incoming Wires", incoming, "No incoming wires"));
    sections.add(joinSection("Outgoing Wires", outgoing, "No outgoing wires"));
    sections.add(joinSection("Control Assignments", controls,
                             "Drag a control source onto a parameter row"));
    return sections.joinIntoString("\r\n\r\n");
  }

  void refreshNodeConnectionSummary() {
    if (isInspectingFrame() || inspectedNodeId == kInvalidNodeId) {
      nodeConnectionsBox.clear();
      return;
    }

    if (const auto *node = document.findNode(inspectedNodeId)) {
      nodeConnectionsBox.setText(buildNodeConnectionSummary(*node), false);
      return;
    }

    nodeConnectionsBox.clear();
  }
  void updateRuntimeValueLabels() {
    for (auto &entry : paramEditors) {
      juce::String runtimeText;
      bool hasRuntimeValue = false;
      const auto it = runtimeParamsById.find(entry->paramId);
      if (it != runtimeParamsById.end()) {
        runtimeText = "Runtime: " +
                      formatValueForDisplay(it->second.currentValue, entry->spec);
        hasRuntimeValue = true;
      } else if (entry->spec.exposeToIeum && paramProvider != nullptr) {
        runtimeText = "Runtime: unavailable";
      } else {
        runtimeText = "Document: " +
                      formatValueForDisplay(entry->originalValue, entry->spec);
      }
      entry->runtimeValueLabel->setVisible(runtimeText.isNotEmpty());
      entry->runtimeValueLabel->setColour(
          juce::Label::backgroundColourId,
          hasRuntimeValue ? juce::Colour(0x221d4ed8)
                          : juce::Colour(0x221f2937));
      entry->runtimeValueLabel->setColour(
          juce::Label::textColourId,
          hasRuntimeValue ? juce::Colour(0xff93c5fd)
                          : juce::Colours::white.withAlpha(0.62f));
      entry->runtimeValueLabel->setText(runtimeText, juce::dontSendNotification);

      const auto ieumText = makeIeumBindingText(entry->spec);
      const bool showIeum = shouldShowIeumBindingLine(entry->spec, ieumText);
      entry->bindingInfoLabel->setVisible(showIeum);
      entry->bindingInfoLabel->setColour(
          juce::Label::backgroundColourId,
          showIeum ? juce::Colour(0x16182232) : juce::Colours::transparentBlack);
      entry->bindingInfoLabel->setText(showIeum ? ieumText : juce::String(),
                                       juce::dontSendNotification);

      const auto gyeolBinding = makeGyeolBindingPresentation(
          entry->spec, entry->paramId, bindingSummaryResolver);
      const bool showGyeol = shouldShowGyeolBindingLine(gyeolBinding.text);
      entry->gyeolBindingLabel->setVisible(showGyeol);
      entry->gyeolBindingLabel->setColour(
          juce::Label::backgroundColourId,
          showGyeol ? gyeolBinding.colour.withAlpha(0.12f)
                    : juce::Colours::transparentBlack);
      entry->gyeolBindingLabel->setColour(juce::Label::textColourId,
                                          showGyeol ? gyeolBinding.colour
                                                    : juce::Colours::white.withAlpha(0.32f));
      entry->gyeolBindingLabel->setText(showGyeol ? gyeolBinding.text
                                                  : juce::String(),
                                        juce::dontSendNotification);

      TIssueState controlAssignmentIssue = TIssueState::none;
      const auto controlAssignmentText =
          controlAssignmentSummaryForParam(*entry, &controlAssignmentIssue);
      const bool showControlAssignment = controlAssignmentText.isNotEmpty();
      entry->controlAssignmentLabel->setVisible(showControlAssignment);
      entry->controlAssignmentLabel->setColour(
          juce::Label::backgroundColourId,
          showControlAssignment
              ? (hasIssueState(controlAssignmentIssue)
                     ? issueStateAccent(controlAssignmentIssue).withAlpha(0.16f)
                     : juce::Colour(0x16f59e0b))
              : juce::Colours::transparentBlack);
      entry->controlAssignmentLabel->setColour(
          juce::Label::textColourId,
          hasIssueState(controlAssignmentIssue)
              ? issueStateAccent(controlAssignmentIssue).brighter(0.22f)
              : juce::Colour(0xfffbbf24));
      entry->controlAssignmentLabel->setTooltip(
          hasIssueState(controlAssignmentIssue)
              ? "Assigned control source is " + issueStateLabel(controlAssignmentIssue).toLowerCase()
              : juce::String());
      entry->controlAssignmentLabel->setText(
          showControlAssignment ? controlAssignmentText : juce::String(),
          juce::dontSendNotification);
      if (entry->clearControlAssignmentButton != nullptr)
        entry->clearControlAssignmentButton->setVisible(showControlAssignment);

      std::vector<const TControlSourceAssignment *> matchingAssignments;
      for (const auto &assignment : document.controlState.assignments) {
        if (assignment.targetParamId == entry->paramId)
          matchingAssignments.push_back(&assignment);
      }

      const bool showSingleAssignmentControls = matchingAssignments.size() == 1;
      entry->editableAssignmentSourceId.clear();
      entry->editableAssignmentPortId.clear();
      entry->editableAssignmentTargetNodeId = kInvalidNodeId;
      if (showSingleAssignmentControls) {
        const auto &assignment = *matchingAssignments.front();
        entry->editableAssignmentSourceId = assignment.sourceId;
        entry->editableAssignmentPortId = assignment.portId;
        entry->editableAssignmentTargetNodeId = assignment.targetNodeId;
      }

      if (entry->controlAssignmentSettingsLabel != nullptr) {
        entry->controlAssignmentSettingsLabel->setVisible(showSingleAssignmentControls);
        entry->controlAssignmentSettingsLabel->setColour(
            juce::Label::backgroundColourId,
            showSingleAssignmentControls ? juce::Colour(0x16182232)
                                         : juce::Colours::transparentBlack);
        entry->controlAssignmentSettingsLabel->setText(
            showSingleAssignmentControls ? juce::String("Range")
                                         : juce::String(),
            juce::dontSendNotification);
      }
      if (entry->controlAssignmentRangeMinEditor != nullptr) {
        entry->controlAssignmentRangeMinEditor->setVisible(showSingleAssignmentControls);
        if (showSingleAssignmentControls) {
          entry->controlAssignmentRangeMinEditor->setText(
              formatControlAssignmentRange(matchingAssignments.front()->rangeMin),
              juce::dontSendNotification);
        }
      }
      if (entry->controlAssignmentRangeMaxEditor != nullptr) {
        entry->controlAssignmentRangeMaxEditor->setVisible(showSingleAssignmentControls);
        if (showSingleAssignmentControls) {
          entry->controlAssignmentRangeMaxEditor->setText(
              formatControlAssignmentRange(matchingAssignments.front()->rangeMax),
              juce::dontSendNotification);
        }
      }
      if (entry->controlAssignmentEnabledButton != nullptr) {
        entry->controlAssignmentEnabledButton->setVisible(showSingleAssignmentControls);
        if (showSingleAssignmentControls)
          entry->controlAssignmentEnabledButton->setToggleState(
              matchingAssignments.front()->enabled, juce::dontSendNotification);
      }
      if (entry->controlAssignmentInvertButton != nullptr) {
        entry->controlAssignmentInvertButton->setVisible(showSingleAssignmentControls);
        if (showSingleAssignmentControls)
          entry->controlAssignmentInvertButton->setToggleState(
              matchingAssignments.front()->inverted, juce::dontSendNotification);
      }
    }

    layoutParamEditors();
    repaint();
  }
  void applyChanges() {
    if (isInspectingFrame()) {
      TFrameRegion *frame = document.findFrame(inspectedFrameId);
      if (frame == nullptr)
        return;

      bool documentDirty = false;
      bool layoutDirty = false;

      const juce::String nextTitle = nameEditor.getText().trim();
      if (frame->title != nextTitle) {
        frame->title = nextTitle;
        documentDirty = true;
      }

      const auto nextColor = frameColorFromId(colorBox.getSelectedId());
      if (frame->colorArgb != nextColor) {
        frame->colorArgb = nextColor;
        documentDirty = true;
      }

      const bool nextLocked = bypassToggle.getToggleState();
      if (frame->locked != nextLocked) {
        frame->locked = nextLocked;
        documentDirty = true;
      }

      const bool nextCollapsed = collapsedToggle.getToggleState();
      if (frame->collapsed != nextCollapsed) {
        frame->collapsed = nextCollapsed;
        documentDirty = true;
        layoutDirty = true;
      }

      if (documentDirty)
        document.touch(false);

      if (layoutDirty)
        canvas.updateChildPositions();

      canvas.repaint();
      rebuildFromDocument();
      return;
    }

    TNode *node = document.findNode(inspectedNodeId);
    if (node == nullptr)
      return;

    bool documentDirty = false;
    bool runtimeDirty = false;

    const juce::String nextLabel = nameEditor.getText().trim();
    if (node->label != nextLabel) {
      node->label = nextLabel;
      documentDirty = true;
    }

    const juce::String nextColorTag = colorTagFromId(colorBox.getSelectedId());
    if (node->colorTag != nextColorTag) {
      node->colorTag = nextColorTag;
      documentDirty = true;
    }

    const bool nextBypassed = bypassToggle.getToggleState();
    if (node->bypassed != nextBypassed) {
      node->bypassed = nextBypassed;
      documentDirty = true;
      runtimeDirty = true;
    }

    const bool nextCollapsed = collapsedToggle.getToggleState();
    if (node->collapsed != nextCollapsed) {
      node->collapsed = nextCollapsed;
      documentDirty = true;
    }

    for (const auto &entry : paramEditors) {
      if (entry->spec.isReadOnly)
        continue;

      const juce::var nextValue = readEditorValue(*entry);
      auto currentIt = node->params.find(entry->spec.key);
      const juce::var currentValue = currentIt != node->params.end()
                                         ? currentIt->second
                                         : entry->originalValue;
      if (varEquals(currentValue, nextValue))
        continue;

      document.executeCommand(std::make_unique<SetParamCommand>(
          inspectedNodeId, entry->spec.key, currentValue, nextValue));
      if (paramProvider != nullptr && entry->spec.exposeToIeum)
        paramProvider->setParam(entry->paramId, nextValue);
    }

    if (documentDirty)
      document.touch(runtimeDirty);

    canvas.rebuildNodeComponents();
    canvas.repaint();
    rebuildFromDocument();
  }

  TGraphDocument &document;
  TGraphCanvas &canvas;
  const TNodeRegistry &registry;
  ParamBindingSummaryResolver bindingSummaryResolver;
  ITeulParamProvider *paramProvider = nullptr;
  std::function<void()> onLayoutChanged;

  NodeId inspectedNodeId = kInvalidNodeId;
  int inspectedFrameId = 0;
  juce::Label headerLabel;
  juce::Label typeLabel;
  juce::TextEditor nameEditor;
  juce::ComboBox colorBox;
  juce::ToggleButton bypassToggle;
  juce::ToggleButton collapsedToggle;
  juce::TextButton applyButton;
  juce::TextButton closeButton;
  juce::Viewport paramViewport;
  juce::TextEditor nodeConnectionsBox;
  juce::TextEditor frameSummaryBox;
  juce::TextButton frameCaptureButton;
  juce::TextButton frameReleaseButton;
  juce::TextButton frameFitButton;
  juce::TextButton frameSavePresetButton;
  std::unique_ptr<juce::Component> paramsContent;
  std::vector<std::unique_ptr<ParamEditor>> paramEditors;
  juce::String assignmentDropTargetId;
  bool assignmentDropTargetCanConnect = false;
  std::map<juce::String, TTeulExposedParam> runtimeParamsById;
};

} // namespace

std::unique_ptr<NodePropertiesPanel> NodePropertiesPanel::create(
    TGraphDocument &document, TGraphCanvas &canvas,
    const TNodeRegistry &registry, ITeulParamProvider *provider,
    ParamBindingSummaryResolver bindingSummaryResolver) {
  return std::make_unique<NodePropertiesPanelImpl>(
      document, canvas, registry, provider,
      std::move(bindingSummaryResolver));
}

} // namespace Teul
