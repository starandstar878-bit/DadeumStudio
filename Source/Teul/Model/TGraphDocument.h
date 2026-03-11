#pragma once

#include "TConnection.h"
#include "TNode.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace Teul {

class TCommand;
class THistoryStack;

struct TGraphMeta {
  juce::String name = "Untitled";
  float canvasOffsetX = 0.0f;
  float canvasOffsetY = 0.0f;
  float canvasZoom = 1.0f;
  double sampleRate = 48000.0;
  int blockSize = 256;
};

enum class TDocumentNoticeLevel { info, warning, degraded };

struct TDocumentNotice {
  bool active = false;
  TDocumentNoticeLevel level = TDocumentNoticeLevel::info;
  juce::String title;
  juce::String detail;
};

struct TFrameRegion {
  int frameId = 0;
  juce::String frameUuid;
  juce::String title = "Frame";
  float x = 0.0f;
  float y = 0.0f;
  float width = 360.0f;
  float height = 220.0f;
  juce::uint32 colorArgb = 0x334d8bf7;
  bool collapsed = false;
  bool locked = false;
  bool logicalGroup = true;
  bool membershipExplicit = false;
  std::vector<NodeId> memberNodeIds;

  bool containsNode(NodeId nodeId) const noexcept {
    return std::find(memberNodeIds.begin(), memberNodeIds.end(), nodeId) !=
           memberNodeIds.end();
  }

  void addMember(NodeId nodeId) {
    if (!containsNode(nodeId))
      memberNodeIds.push_back(nodeId);
  }

  void removeMember(NodeId nodeId) noexcept {
    memberNodeIds.erase(
        std::remove(memberNodeIds.begin(), memberNodeIds.end(), nodeId),
        memberNodeIds.end());
  }
};

struct TBookmark {
  int bookmarkId = 0;
  juce::String name = "Bookmark";
  float focusX = 0.0f;
  float focusY = 0.0f;
  float zoom = 1.0f;
  juce::String colorTag;
};

enum class TRailKind { input, output, controlSource };

enum class TSystemRailEndpointKind {
  audioInput,
  audioOutput,
  midiInput,
  midiOutput
};

struct TSystemRailPort {
  juce::String portId;
  juce::String displayName;
  TPortDataType dataType = TPortDataType::Audio;
};

struct TSystemRailEndpoint {
  juce::String endpointId;
  juce::String railId;
  juce::String displayName;
  juce::String subtitle;
  TSystemRailEndpointKind kind = TSystemRailEndpointKind::audioInput;
  bool stereo = false;
  bool missing = false;
  bool degraded = false;
  int order = 0;
  std::vector<TSystemRailPort> ports;
};

enum class TControlSourceKind {
  expression,
  footswitch,
  trigger,
  midiCc,
  midiNote,
  macro
};

enum class TControlSourceMode { continuous, momentary, toggle };

enum class TControlPortKind { value, gate, trigger };

struct TControlRailLayout {
  juce::String railId;
  juce::String title;
  TRailKind kind = TRailKind::controlSource;
  bool collapsed = false;
  int order = 0;
};

struct TControlSourcePort {
  juce::String portId;
  juce::String displayName;
  TControlPortKind kind = TControlPortKind::value;
};

struct TDeviceBindingSignature {
  juce::String midiDeviceName;
  juce::String hardwareId;
  int midiChannel = 0;
  int controllerNumber = -1;
  int noteNumber = -1;
};

struct TDeviceSourceProfile {
  juce::String sourceId;
  juce::String displayName;
  TControlSourceKind kind = TControlSourceKind::expression;
  TControlSourceMode mode = TControlSourceMode::continuous;
  std::vector<TControlSourcePort> ports;
  std::vector<TDeviceBindingSignature> bindings;
};

struct TDeviceProfile {
  juce::String profileId;
  juce::String deviceId;
  juce::String displayName;
  bool autoDetected = false;
  std::vector<TDeviceSourceProfile> sources;
};

struct TControlSource {
  juce::String sourceId;
  juce::String deviceProfileId;
  juce::String railId = "control-rail";
  juce::String displayName;
  TControlSourceKind kind = TControlSourceKind::expression;
  TControlSourceMode mode = TControlSourceMode::continuous;
  std::vector<TControlSourcePort> ports;
  bool autoDetected = false;
  bool confirmed = false;
  bool missing = false;
  bool degraded = false;
};

struct TControlSourceAssignment {
  juce::String sourceId;
  juce::String portId;
  NodeId targetNodeId = kInvalidNodeId;
  juce::String targetParamId;
  bool enabled = true;
  bool inverted = false;
  float rangeMin = 0.0f;
  float rangeMax = 1.0f;
};

struct TControlSourceState {
  std::vector<TControlRailLayout> rails;
  std::vector<TSystemRailEndpoint> inputEndpoints;
  std::vector<TSystemRailEndpoint> outputEndpoints;
  std::vector<TControlSource> sources;
  std::vector<TDeviceProfile> deviceProfiles;
  std::vector<TControlSourceAssignment> assignments;
  std::vector<juce::String> missingDeviceProfileIds;

  TControlSourceState() { ensureDefaultRails(); }

  void ensureDefaultRails() {
    ensureRail("input-rail", "Inputs", TRailKind::input, 0);
    ensureRail("control-rail", "Controls", TRailKind::controlSource, 1);
    ensureRail("output-rail", "Outputs", TRailKind::output, 2);
  }

  void ensureRail(const juce::String &railId,
                  const juce::String &title,
                  TRailKind kind,
                  int order) {
    if (auto *rail = findRail(railId)) {
      rail->title = title;
      rail->kind = kind;
      rail->order = order;
      return;
    }

    TControlRailLayout rail;
    rail.railId = railId;
    rail.title = title;
    rail.kind = kind;
    rail.order = order;
    rails.push_back(std::move(rail));
  }

  bool hasAnyRailCards() const noexcept {
    return !inputEndpoints.empty() || !outputEndpoints.empty() ||
           !sources.empty();
  }

  void ensurePreviewDataIfEmpty() {
    if (hasAnyRailCards())
      return;

    TSystemRailEndpoint stereoIn;
    stereoIn.endpointId = "audio-in-main";
    stereoIn.railId = "input-rail";
    stereoIn.displayName = "Audio In";
    stereoIn.subtitle = "Host stereo";
    stereoIn.kind = TSystemRailEndpointKind::audioInput;
    stereoIn.stereo = true;
    stereoIn.order = 0;
    stereoIn.ports.push_back({"audio-in-l", "L", TPortDataType::Audio});
    stereoIn.ports.push_back({"audio-in-r", "R", TPortDataType::Audio});
    inputEndpoints.push_back(std::move(stereoIn));

    TSystemRailEndpoint midiIn;
    midiIn.endpointId = "midi-in-main";
    midiIn.railId = "input-rail";
    midiIn.displayName = "MIDI In";
    midiIn.subtitle = "Device bridge";
    midiIn.kind = TSystemRailEndpointKind::midiInput;
    midiIn.order = 1;
    midiIn.ports.push_back({"midi-in-port", "MIDI", TPortDataType::MIDI});
    inputEndpoints.push_back(std::move(midiIn));

    TSystemRailEndpoint stereoOut;
    stereoOut.endpointId = "audio-out-main";
    stereoOut.railId = "output-rail";
    stereoOut.displayName = "Audio Out";
    stereoOut.subtitle = "Main bus";
    stereoOut.kind = TSystemRailEndpointKind::audioOutput;
    stereoOut.stereo = true;
    stereoOut.order = 0;
    stereoOut.ports.push_back({"audio-out-l", "L", TPortDataType::Audio});
    stereoOut.ports.push_back({"audio-out-r", "R", TPortDataType::Audio});
    outputEndpoints.push_back(std::move(stereoOut));

    TControlSource expression;
    expression.sourceId = "exp-1";
    expression.deviceProfileId = "preview-device";
    expression.railId = "control-rail";
    expression.displayName = "EXP 1";
    expression.kind = TControlSourceKind::expression;
    expression.mode = TControlSourceMode::continuous;
    expression.autoDetected = true;
    expression.confirmed = false;
    expression.ports.push_back(
        {"exp-1-value", "Value", TControlPortKind::value});
    sources.push_back(std::move(expression));

    TControlSource footswitch;
    footswitch.sourceId = "fs-1";
    footswitch.deviceProfileId = "preview-device";
    footswitch.railId = "control-rail";
    footswitch.displayName = "FS 1";
    footswitch.kind = TControlSourceKind::footswitch;
    footswitch.mode = TControlSourceMode::momentary;
    footswitch.autoDetected = true;
    footswitch.confirmed = false;
    footswitch.ports.push_back({"fs-1-gate", "Gate", TControlPortKind::gate});
    footswitch.ports.push_back(
        {"fs-1-trigger", "Trigger", TControlPortKind::trigger});
    sources.push_back(std::move(footswitch));
  }

  TControlRailLayout *findRail(const juce::String &railId) noexcept {
    for (auto &rail : rails) {
      if (rail.railId == railId)
        return &rail;
    }

    return nullptr;
  }

  const TControlRailLayout *findRail(const juce::String &railId) const noexcept {
    for (const auto &rail : rails) {
      if (rail.railId == railId)
        return &rail;
    }

    return nullptr;
  }

  TSystemRailEndpoint *findEndpoint(const juce::String &endpointId) noexcept {
    for (auto &endpoint : inputEndpoints) {
      if (endpoint.endpointId == endpointId)
        return &endpoint;
    }

    for (auto &endpoint : outputEndpoints) {
      if (endpoint.endpointId == endpointId)
        return &endpoint;
    }

    return nullptr;
  }

  const TSystemRailEndpoint *
  findEndpoint(const juce::String &endpointId) const noexcept {
    for (const auto &endpoint : inputEndpoints) {
      if (endpoint.endpointId == endpointId)
        return &endpoint;
    }

    for (const auto &endpoint : outputEndpoints) {
      if (endpoint.endpointId == endpointId)
        return &endpoint;
    }

    return nullptr;
  }

  TSystemRailPort *findEndpointPort(const juce::String &endpointId,
                                    const juce::String &portId) noexcept {
    if (auto *endpoint = findEndpoint(endpointId)) {
      for (auto &port : endpoint->ports) {
        if (port.portId == portId)
          return &port;
      }
    }

    return nullptr;
  }

  const TSystemRailPort *findEndpointPort(const juce::String &endpointId,
                                          const juce::String &portId) const noexcept {
    if (const auto *endpoint = findEndpoint(endpointId)) {
      for (const auto &port : endpoint->ports) {
        if (port.portId == portId)
          return &port;
      }
    }

    return nullptr;
  }

  TControlSource *findSource(const juce::String &sourceId) noexcept {
    for (auto &source : sources) {
      if (source.sourceId == sourceId)
        return &source;
    }

    return nullptr;
  }

  const TControlSource *findSource(const juce::String &sourceId) const noexcept {
    for (const auto &source : sources) {
      if (source.sourceId == sourceId)
        return &source;
    }

    return nullptr;
  }

  TDeviceProfile *findDeviceProfile(const juce::String &profileId) noexcept {
    for (auto &profile : deviceProfiles) {
      if (profile.profileId == profileId)
        return &profile;
    }

    return nullptr;
  }

  const TDeviceProfile *
  findDeviceProfile(const juce::String &profileId) const noexcept {
    for (const auto &profile : deviceProfiles) {
      if (profile.profileId == profileId)
        return &profile;
    }

    return nullptr;
  }
};

struct TGraphDocument {
  TGraphDocument();
  ~TGraphDocument();

  TGraphDocument(const TGraphDocument &other);
  TGraphDocument &operator=(const TGraphDocument &other);

  TGraphDocument(TGraphDocument &&other) noexcept;
  TGraphDocument &operator=(TGraphDocument &&other) noexcept;

  int schemaVersion = 4;
  TGraphMeta meta;

  std::vector<TNode> nodes;
  std::vector<TConnection> connections;
  std::vector<TFrameRegion> frames;
  std::vector<TBookmark> bookmarks;
  TControlSourceState controlState;

  NodeId allocNodeId() noexcept { return nextNodeId++; }
  PortId allocPortId() noexcept { return nextPortId++; }
  ConnectionId allocConnectionId() noexcept { return nextConnectionId++; }
  int allocFrameId() noexcept { return nextFrameId++; }
  int allocBookmarkId() noexcept { return nextBookmarkId++; }

  TNode *findNode(NodeId id) noexcept {
    for (auto &n : nodes)
      if (n.nodeId == id)
        return &n;
    return nullptr;
  }

  const TNode *findNode(NodeId id) const noexcept {
    for (const auto &n : nodes)
      if (n.nodeId == id)
        return &n;
    return nullptr;
  }

  TConnection *findConnection(ConnectionId id) noexcept {
    for (auto &c : connections)
      if (c.connectionId == id)
        return &c;
    return nullptr;
  }

  const TConnection *findConnection(ConnectionId id) const noexcept {
    for (const auto &c : connections)
      if (c.connectionId == id)
        return &c;
    return nullptr;
  }

  TFrameRegion *findFrame(int frameId) noexcept {
    for (auto &frame : frames)
      if (frame.frameId == frameId)
        return &frame;
    return nullptr;
  }

  const TFrameRegion *findFrame(int frameId) const noexcept {
    for (const auto &frame : frames)
      if (frame.frameId == frameId)
        return &frame;
    return nullptr;
  }
  bool isNodeExplicitMemberOfFrame(NodeId nodeId, int frameId) const noexcept {
    if (const auto *frame = findFrame(frameId))
      return frame->containsNode(nodeId);
    return false;
  }

  void addNodeToFrame(NodeId nodeId, int frameId) {
    if (auto *frame = findFrame(frameId)) {
      frame->membershipExplicit = true;
      frame->addMember(nodeId);
    }
  }

  void addNodeToFrameExclusive(NodeId nodeId, int frameId) {
    for (auto &frame : frames) {
      if (frame.frameId == frameId)
        continue;
      if (!frame.logicalGroup)
        continue;
      if (!frame.containsNode(nodeId))
        continue;

      frame.membershipExplicit = true;
      frame.removeMember(nodeId);
    }

    addNodeToFrame(nodeId, frameId);
  }

  void removeNodeFromFrame(NodeId nodeId, int frameId) {
    if (auto *frame = findFrame(frameId)) {
      frame->membershipExplicit = true;
      frame->removeMember(nodeId);
    }
  }

  void removeNodeFromAllFrames(NodeId nodeId) {
    for (auto &frame : frames) {
      if (frame.containsNode(nodeId)) {
        frame.membershipExplicit = true;
        frame.removeMember(nodeId);
      }
    }
  }

  TBookmark *findBookmark(int bookmarkId) noexcept {
    for (auto &bookmark : bookmarks)
      if (bookmark.bookmarkId == bookmarkId)
        return &bookmark;
    return nullptr;
  }

  const TBookmark *findBookmark(int bookmarkId) const noexcept {
    for (const auto &bookmark : bookmarks)
      if (bookmark.bookmarkId == bookmarkId)
        return &bookmark;
    return nullptr;
  }

  std::vector<TConnection *> connectionsForPort(NodeId nodeId,
                                                PortId portId) noexcept {
    std::vector<TConnection *> result;
    for (auto &c : connections) {
      if ((c.from.isNodePort() && c.from.nodeId == nodeId &&
           c.from.portId == portId) ||
          (c.to.isNodePort() && c.to.nodeId == nodeId &&
           c.to.portId == portId)) {
        result.push_back(&c);
      }
    }
    return result;
  }

  TSystemRailPort *findSystemRailPort(const juce::String &endpointId,
                                      const juce::String &portId) noexcept {
    return controlState.findEndpointPort(endpointId, portId);
  }

  const TSystemRailPort *findSystemRailPort(
      const juce::String &endpointId,
      const juce::String &portId) const noexcept {
    return controlState.findEndpointPort(endpointId, portId);
  }

  bool wouldCreateCycle(NodeId fromNodeId, NodeId toNodeId) const noexcept;

  NodeId getNextNodeId() const noexcept { return nextNodeId; }
  PortId getNextPortId() const noexcept { return nextPortId; }
  ConnectionId getNextConnectionId() const noexcept { return nextConnectionId; }
  int getNextFrameId() const noexcept { return nextFrameId; }
  int getNextBookmarkId() const noexcept { return nextBookmarkId; }
  std::uint64_t getDocumentRevision() const noexcept { return documentRevision; }
  std::uint64_t getRuntimeRevision() const noexcept { return runtimeRevision; }
  const TDocumentNotice &getTransientNotice() const noexcept {
    return transientNotice;
  }
  std::uint64_t getTransientNoticeRevision() const noexcept {
    return transientNoticeRevision;
  }

  void setTransientNotice(TDocumentNoticeLevel level,
                          const juce::String &title,
                          const juce::String &detail = {}) noexcept {
    const auto normalizedTitle = title.trim();
    const auto normalizedDetail = detail.trim();
    if (normalizedTitle.isEmpty() && normalizedDetail.isEmpty()) {
      clearTransientNotice();
      return;
    }

    if (transientNotice.active && transientNotice.level == level &&
        transientNotice.title == normalizedTitle &&
        transientNotice.detail == normalizedDetail) {
      return;
    }

    transientNotice.active = true;
    transientNotice.level = level;
    transientNotice.title = normalizedTitle;
    transientNotice.detail = normalizedDetail;
    ++transientNoticeRevision;
  }

  void clearTransientNotice() noexcept {
    if (!transientNotice.active && transientNotice.title.isEmpty() &&
        transientNotice.detail.isEmpty()) {
      return;
    }

    transientNotice = {};
    ++transientNoticeRevision;
  }

  void setNextNodeId(NodeId id) noexcept { nextNodeId = id; }
  void setNextPortId(PortId id) noexcept { nextPortId = id; }
  void setNextConnectionId(ConnectionId id) noexcept { nextConnectionId = id; }
  void setNextFrameId(int id) noexcept { nextFrameId = id; }
  void setNextBookmarkId(int id) noexcept { nextBookmarkId = id; }

  void executeCommand(std::unique_ptr<TCommand> command);
  bool undo();
  bool redo();
  void clearHistory();
  void touch(bool runtimeRelevant = false) noexcept;

private:
  NodeId nextNodeId = 1;
  PortId nextPortId = 1;
  ConnectionId nextConnectionId = 1;
  int nextFrameId = 1;
  int nextBookmarkId = 1;
  std::uint64_t documentRevision = 0;
  std::uint64_t runtimeRevision = 0;
  TDocumentNotice transientNotice;
  std::uint64_t transientNoticeRevision = 0;

  std::unique_ptr<THistoryStack> historyStack;
};

} // namespace Teul
