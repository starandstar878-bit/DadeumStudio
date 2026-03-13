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
  juce::String armedLearnSourceId;

  TControlSourceState() { ensureDefaultRails(); }

  bool isLearnArmed(const juce::String &sourceId) const noexcept {
    return armedLearnSourceId.isNotEmpty() && armedLearnSourceId == sourceId.trim();
  }

  bool setLearnArmed(const juce::String &sourceId, bool shouldArm) {
    const auto normalizedId = sourceId.trim();
    if (normalizedId.isEmpty())
      return false;

    if (!shouldArm) {
      if (armedLearnSourceId != normalizedId)
        return false;
      armedLearnSourceId.clear();
      return true;
    }

    if (findSource(normalizedId) == nullptr)
      return false;

    if (armedLearnSourceId == normalizedId)
      return false;

    armedLearnSourceId = normalizedId;
    return true;
  }

  bool clearLearnArm() {
    if (armedLearnSourceId.isEmpty())
      return false;

    armedLearnSourceId.clear();
    return true;
  }

  void pruneTransientLearnState() {
    if (armedLearnSourceId.isNotEmpty() && findSource(armedLearnSourceId) == nullptr)
      armedLearnSourceId.clear();
  }

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

  static bool controlPortsMatch(const std::vector<TControlSourcePort> &lhs,
                                const std::vector<TControlSourcePort> &rhs) noexcept {
    if (lhs.size() != rhs.size())
      return false;

    for (size_t index = 0; index < lhs.size(); ++index) {
      if (lhs[index].portId != rhs[index].portId ||
          lhs[index].kind != rhs[index].kind) {
        return false;
      }
    }

    return true;
  }

  static std::vector<TControlSourcePort>
  defaultPortsForSource(const juce::String &sourceId,
                        TControlSourceKind kind) {
    auto makePort = [&](const juce::String &suffix, const juce::String &name,
                        TControlPortKind portKind) {
      return TControlSourcePort{sourceId + "-" + suffix, name, portKind};
    };

    switch (kind) {
    case TControlSourceKind::expression:
      return {makePort("value", "Value", TControlPortKind::value)};
    case TControlSourceKind::footswitch:
      return {makePort("gate", "Gate", TControlPortKind::gate),
              makePort("trigger", "Trigger", TControlPortKind::trigger)};
    case TControlSourceKind::trigger:
      return {makePort("trigger", "Trigger", TControlPortKind::trigger)};
    case TControlSourceKind::midiCc:
      return {makePort("value", "Value", TControlPortKind::value)};
    case TControlSourceKind::midiNote:
      return {makePort("gate", "Gate", TControlPortKind::gate),
              makePort("trigger", "Trigger", TControlPortKind::trigger)};
    case TControlSourceKind::macro:
      return {makePort("value", "Value", TControlPortKind::value)};
    }

    return {makePort("value", "Value", TControlPortKind::value)};
  }

  static bool sourceKindSupportsDiscreteMode(TControlSourceKind kind) noexcept {
    switch (kind) {
    case TControlSourceKind::footswitch:
    case TControlSourceKind::trigger:
    case TControlSourceKind::midiNote:
      return true;
    case TControlSourceKind::expression:
    case TControlSourceKind::midiCc:
    case TControlSourceKind::macro:
      return false;
    }

    return false;
  }

  TControlSourceMode normalizedModeForKind(TControlSourceKind kind,
                                           TControlSourceMode mode) const noexcept {
    if (!sourceKindSupportsDiscreteMode(kind))
      return TControlSourceMode::continuous;
    return mode;
  }

  bool reconfigureSourceShape(const juce::String &sourceId,
                              TControlSourceKind kind,
                              TControlSourceMode mode) {
    auto *source = findSource(sourceId.trim());
    if (source == nullptr)
      return false;

    const auto normalizedMode = normalizedModeForKind(kind, mode);
    const auto updatedPorts = defaultPortsForSource(source->sourceId, kind);
    if (source->kind == kind && source->mode == normalizedMode &&
        controlPortsMatch(source->ports, updatedPorts)) {
      return false;
    }

    source->kind = kind;
    source->mode = normalizedMode;
    source->ports = updatedPorts;

    assignments.erase(
        std::remove_if(assignments.begin(), assignments.end(),
                       [&](const TControlSourceAssignment &assignment) {
                         if (assignment.sourceId != source->sourceId)
                           return false;

                         return std::none_of(updatedPorts.begin(), updatedPorts.end(),
                                             [&](const TControlSourcePort &port) {
                                               return port.portId == assignment.portId;
                                             });
                       }),
        assignments.end());

    syncSourceIntoDeviceProfile(*source);
    reconcileDeviceProfilesAndSources();
    return true;
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

    TSystemRailEndpoint midiOut;
    midiOut.endpointId = "midi-out-main";
    midiOut.railId = "output-rail";
    midiOut.displayName = "MIDI Out";
    midiOut.subtitle = "External device";
    midiOut.kind = TSystemRailEndpointKind::midiOutput;
    midiOut.order = 1;
    midiOut.ports.push_back({"midi-out-port", "MIDI", TPortDataType::MIDI});
    outputEndpoints.push_back(std::move(midiOut));

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

    ensurePreviewDeviceProfile();
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

  TDeviceSourceProfile *findDeviceSourceProfile(const juce::String &profileId,
                                                const juce::String &sourceId) noexcept {
    if (auto *profile = findDeviceProfile(profileId)) {
      for (auto &source : profile->sources) {
        if (source.sourceId == sourceId)
          return &source;
      }
    }

    return nullptr;
  }

  const TDeviceSourceProfile *
  findDeviceSourceProfile(const juce::String &profileId,
                          const juce::String &sourceId) const noexcept {
    if (const auto *profile = findDeviceProfile(profileId)) {
      for (const auto &source : profile->sources) {
        if (source.sourceId == sourceId)
          return &source;
      }
    }

    return nullptr;
  }

  TDeviceProfile &ensureDeviceProfile(const juce::String &profileId,
                                      const juce::String &deviceId,
                                      const juce::String &displayName,
                                      bool autoDetected) {
    auto normalizedProfileId = profileId.trim();
    if (normalizedProfileId.isEmpty())
      normalizedProfileId = "transient-device";

    if (auto *existingProfile = findDeviceProfile(normalizedProfileId)) {
      if (deviceId.isNotEmpty())
        existingProfile->deviceId = deviceId;
      if (displayName.isNotEmpty())
        existingProfile->displayName = displayName;
      existingProfile->autoDetected = autoDetected;
      return *existingProfile;
    }

    TDeviceProfile profile;
    profile.profileId = normalizedProfileId;
    profile.deviceId = deviceId.isNotEmpty() ? deviceId : normalizedProfileId;
    profile.displayName = displayName.isNotEmpty() ? displayName : normalizedProfileId;
    profile.autoDetected = autoDetected;
    deviceProfiles.push_back(std::move(profile));
    return deviceProfiles.back();
  }

  static bool deviceBindingMatches(const TDeviceBindingSignature &lhs,
                                   const TDeviceBindingSignature &rhs) noexcept {
    return lhs.midiDeviceName == rhs.midiDeviceName &&
           lhs.hardwareId == rhs.hardwareId &&
           lhs.midiChannel == rhs.midiChannel &&
           lhs.controllerNumber == rhs.controllerNumber &&
           lhs.noteNumber == rhs.noteNumber;
  }

  static bool bindingSignatureHasIdentity(const TDeviceBindingSignature &binding) noexcept {
    return binding.midiDeviceName.isNotEmpty() || binding.hardwareId.isNotEmpty() ||
           binding.midiChannel != 0 || binding.controllerNumber >= 0 ||
           binding.noteNumber >= 0;
  }

  bool applyLearnedBindingToArmedSource(
      const TDeviceBindingSignature &binding,
      const juce::String &profileId,
      const juce::String &deviceId,
      const juce::String &profileDisplayName,
      TControlSourceKind kind,
      TControlSourceMode mode,
      const juce::String &sourceDisplayName = {},
      bool autoDetected = true,
      bool confirmed = true) {
    const auto sourceId = armedLearnSourceId.trim();
    if (sourceId.isEmpty())
      return false;

    auto *source = findSource(sourceId);
    if (source == nullptr)
      return false;

    auto normalizedProfileId = profileId.trim();
    if (normalizedProfileId.isEmpty())
      normalizedProfileId = sourceId + "-device";

    auto normalizedDeviceId = deviceId.trim();
    if (normalizedDeviceId.isEmpty())
      normalizedDeviceId = normalizedProfileId;

    auto normalizedProfileName = profileDisplayName.trim();
    if (normalizedProfileName.isEmpty())
      normalizedProfileName = normalizedProfileId;

    ensureDeviceProfile(normalizedProfileId, normalizedDeviceId,
                        normalizedProfileName, autoDetected);
    reconfigureSourceShape(sourceId, kind, mode);

    source = findSource(sourceId);
    if (source == nullptr)
      return false;

    if (sourceDisplayName.trim().isNotEmpty())
      source->displayName = sourceDisplayName.trim();
    source->deviceProfileId = normalizedProfileId;
    source->autoDetected = autoDetected;
    source->confirmed = confirmed;
    source->missing = false;
    source->degraded = false;

    syncSourceIntoDeviceProfile(*source);

    auto *profileSource = findDeviceSourceProfile(normalizedProfileId, source->sourceId);
    if (profileSource != nullptr && bindingSignatureHasIdentity(binding)) {
      const bool alreadyPresent = std::any_of(
          profileSource->bindings.begin(), profileSource->bindings.end(),
          [&](const TDeviceBindingSignature &existingBinding) {
            return deviceBindingMatches(existingBinding, binding);
          });
      if (!alreadyPresent)
        profileSource->bindings.push_back(binding);
    }

    if (auto *profile = findDeviceProfile(normalizedProfileId)) {
      profile->deviceId = normalizedDeviceId;
      profile->displayName = normalizedProfileName;
      profile->autoDetected = autoDetected;
    }

    clearLearnArm();
    reconcileDeviceProfilesAndSources();
    return true;
  }

  void removeMissingDeviceProfileId(const juce::String &profileId) {
    const auto trimmedId = profileId.trim();
    if (trimmedId.isEmpty())
      return;

    missingDeviceProfileIds.erase(
        std::remove_if(missingDeviceProfileIds.begin(),
                       missingDeviceProfileIds.end(),
                       [&](const juce::String &existingId) {
                         return existingId.trim() == trimmedId;
                       }),
        missingDeviceProfileIds.end());
  }

  void refreshProfileAutoDetectedState(const juce::String &profileId) {
    auto *profile = findDeviceProfile(profileId);
    if (profile == nullptr)
      return;

    bool hasMatchedSource = false;
    bool allAutoDetected = true;
    for (const auto &source : sources) {
      if (source.deviceProfileId != profileId)
        continue;

      hasMatchedSource = true;
      if (!source.autoDetected) {
        allAutoDetected = false;
        break;
      }
    }

    if (hasMatchedSource)
      profile->autoDetected = allAutoDetected;
  }

  void syncSourceIntoDeviceProfile(const TControlSource &source) {
    const auto profileId = source.deviceProfileId.trim();
    if (profileId.isEmpty())
      return;

    auto *profile = findDeviceProfile(profileId);
    if (profile == nullptr)
      return;

    auto *profileSource = findDeviceSourceProfile(profileId, source.sourceId);
    if (profileSource == nullptr) {
      TDeviceSourceProfile newProfileSource;
      newProfileSource.sourceId = source.sourceId;
      newProfileSource.displayName = source.displayName.isNotEmpty()
                                         ? source.displayName
                                         : source.sourceId;
      newProfileSource.kind = source.kind;
      newProfileSource.mode = source.mode;
      newProfileSource.ports = source.ports;
      profile->sources.push_back(std::move(newProfileSource));
      profileSource = &profile->sources.back();
    }

    profileSource->displayName = source.displayName.isNotEmpty()
                                     ? source.displayName
                                     : source.sourceId;
    profileSource->kind = source.kind;
    profileSource->mode = source.mode;
    profileSource->ports = source.ports;
    removeMissingDeviceProfileId(profileId);
    refreshProfileAutoDetectedState(profileId);
  }

  bool migrateDeviceProfileIdentity(const juce::String &fromProfileId,
                                    const juce::String &toProfileId,
                                    const juce::String &deviceId,
                                    const juce::String &displayName,
                                    bool autoDetected) {
    const auto normalizedFromId = fromProfileId.trim();
    const auto normalizedToId = toProfileId.trim();
    if (normalizedFromId.isEmpty() || normalizedToId.isEmpty() ||
        normalizedFromId == normalizedToId) {
      return false;
    }

    auto *fromProfile = findDeviceProfile(normalizedFromId);
    if (fromProfile == nullptr)
      return false;

    if (auto *toProfile = findDeviceProfile(normalizedToId)) {
      for (const auto &profileSource : fromProfile->sources) {
        auto existingIt =
            std::find_if(toProfile->sources.begin(), toProfile->sources.end(),
                         [&](const TDeviceSourceProfile &candidate) {
                           return candidate.sourceId == profileSource.sourceId;
                         });
        if (existingIt == toProfile->sources.end()) {
          toProfile->sources.push_back(profileSource);
          continue;
        }

        if (existingIt->displayName.isEmpty())
          existingIt->displayName = profileSource.displayName;
        if (existingIt->ports.empty())
          existingIt->ports = profileSource.ports;

        for (const auto &binding : profileSource.bindings) {
          const bool alreadyPresent = std::any_of(
              existingIt->bindings.begin(), existingIt->bindings.end(),
              [&](const TDeviceBindingSignature &existingBinding) {
                return deviceBindingMatches(existingBinding, binding);
              });
          if (!alreadyPresent)
            existingIt->bindings.push_back(binding);
        }
      }

      if (deviceId.isNotEmpty())
        toProfile->deviceId = deviceId;
      if (displayName.isNotEmpty())
        toProfile->displayName = displayName;
      toProfile->autoDetected = autoDetected;

      for (auto &source : sources) {
        if (source.deviceProfileId == normalizedFromId)
          source.deviceProfileId = normalizedToId;
      }

      deviceProfiles.erase(
          std::remove_if(deviceProfiles.begin(), deviceProfiles.end(),
                         [&](const TDeviceProfile &profile) {
                           return profile.profileId == normalizedFromId;
                         }),
          deviceProfiles.end());
      removeMissingDeviceProfileId(normalizedFromId);
      removeMissingDeviceProfileId(normalizedToId);
      return true;
    }

    fromProfile->profileId = normalizedToId;
    if (deviceId.isNotEmpty())
      fromProfile->deviceId = deviceId;
    if (displayName.isNotEmpty())
      fromProfile->displayName = displayName;
    fromProfile->autoDetected = autoDetected;

    for (auto &source : sources) {
      if (source.deviceProfileId == normalizedFromId)
        source.deviceProfileId = normalizedToId;
    }

    removeMissingDeviceProfileId(normalizedFromId);
    removeMissingDeviceProfileId(normalizedToId);
    return true;
  }

  juce::String findReconnectCandidateProfileId(
      const juce::String &presentProfileId, const juce::String &deviceId,
      const juce::String &displayName) const {
    const auto normalizedPresentId = presentProfileId.trim();
    if (normalizedPresentId.isEmpty())
      return {};

    auto isMarkedMissing = [&](const juce::String &profileId) {
      return std::any_of(
          missingDeviceProfileIds.begin(), missingDeviceProfileIds.end(),
          [&](const juce::String &existingId) {
            return existingId.trim() == profileId.trim();
          });
    };

    juce::String matchedProfileId;
    int matchCount = 0;
    for (const auto &profile : deviceProfiles) {
      const auto candidateId = profile.profileId.trim();
      if (candidateId.isEmpty() || candidateId == normalizedPresentId)
        continue;
      if (!isMarkedMissing(candidateId))
        continue;

      bool matches = false;
      if (deviceId.isNotEmpty() && profile.deviceId.trim() == deviceId.trim())
        matches = true;
      else if (deviceId.isEmpty() && displayName.isNotEmpty() &&
               profile.displayName.trim() == displayName.trim())
        matches = true;

      if (!matches)
        continue;

      matchedProfileId = candidateId;
      ++matchCount;
      if (matchCount > 1)
        return {};
    }

    return matchCount == 1 ? matchedProfileId : juce::String{};
  }

  bool markDeviceProfilePresent(const juce::String &profileId,
                                const juce::String &deviceId,
                                const juce::String &displayName,
                                bool autoDetected) {
    auto normalizedProfileId = profileId.trim();
    if (normalizedProfileId.isEmpty())
      return false;

    const auto reconnectCandidateId = findReconnectCandidateProfileId(
        normalizedProfileId, deviceId.trim(), displayName.trim());
    if (reconnectCandidateId.isNotEmpty()) {
      const bool migrated = migrateDeviceProfileIdentity(
          reconnectCandidateId, normalizedProfileId, deviceId.trim(),
          displayName.trim(), autoDetected);
      if (migrated) {
        reconcileDeviceProfilesAndSources();
        return true;
      }
    }

    const bool hadProfile = findDeviceProfile(normalizedProfileId) != nullptr;
    auto &profile = ensureDeviceProfile(normalizedProfileId, deviceId, displayName,
                                        autoDetected);
    const auto previousDeviceId = profile.deviceId;
    const auto previousDisplayName = profile.displayName;
    const bool previousAutoDetected = profile.autoDetected;
    const bool wasMarkedMissing = std::any_of(
        missingDeviceProfileIds.begin(), missingDeviceProfileIds.end(),
        [&](const juce::String &existingId) {
          return existingId.trim() == normalizedProfileId;
        });

    profile.deviceId = deviceId.isNotEmpty() ? deviceId : profile.deviceId;
    profile.displayName = displayName.isNotEmpty() ? displayName : profile.displayName;
    profile.autoDetected = autoDetected;
    removeMissingDeviceProfileId(normalizedProfileId);
    reconcileDeviceProfilesAndSources();

    return !hadProfile || wasMarkedMissing ||
           previousDeviceId != profile.deviceId ||
           previousDisplayName != profile.displayName ||
           previousAutoDetected != profile.autoDetected;
  }
  bool markDeviceProfileMissing(const juce::String &profileId) {
    const auto normalizedProfileId = profileId.trim();
    if (normalizedProfileId.isEmpty() || normalizedProfileId == "preview-device")
      return false;

    const bool alreadyMissing = std::any_of(
        missingDeviceProfileIds.begin(), missingDeviceProfileIds.end(),
        [&](const juce::String &existingId) {
          return existingId.trim() == normalizedProfileId;
        });
    if (alreadyMissing)
      return false;

    missingDeviceProfileIds.push_back(normalizedProfileId);
    reconcileDeviceProfilesAndSources();
    return true;
  }

  bool tryRelinkSourceToCompatibleProfile(TControlSource &source) {
    juce::String matchedProfileId;
    int candidateCount = 0;

    const auto matchesProfileSource =
        [&](const TDeviceSourceProfile &profileSource,
            bool allowEmptyPorts) noexcept {
          if (profileSource.sourceId != source.sourceId)
            return false;
          if (profileSource.kind != source.kind ||
              profileSource.mode != source.mode) {
            return false;
          }
          if (!allowEmptyPorts || !source.ports.empty())
            return controlPortsMatch(profileSource.ports, source.ports);
          return true;
        };

    const auto findUniqueCandidate = [&](bool allowEmptyPorts) {
      matchedProfileId.clear();
      candidateCount = 0;
      for (const auto &profile : deviceProfiles) {
        const auto matchIt =
            std::find_if(profile.sources.begin(), profile.sources.end(),
                         [&](const TDeviceSourceProfile &profileSource) {
                           return matchesProfileSource(profileSource,
                                                       allowEmptyPorts);
                         });
        if (matchIt == profile.sources.end())
          continue;

        matchedProfileId = profile.profileId;
        ++candidateCount;
        if (candidateCount > 1)
          break;
      }
      return candidateCount == 1;
    };

    bool foundUniqueCandidate = findUniqueCandidate(false);
    if (!foundUniqueCandidate && source.ports.empty())
      foundUniqueCandidate = findUniqueCandidate(true);

    if (!foundUniqueCandidate || matchedProfileId.isEmpty())
      return false;

    source.deviceProfileId = matchedProfileId;
    if (const auto *profileSource =
            findDeviceSourceProfile(matchedProfileId, source.sourceId)) {
      if (source.displayName.isEmpty())
        source.displayName = profileSource->displayName;
      if (source.ports.empty())
        source.ports = profileSource->ports;
    }

    removeMissingDeviceProfileId(matchedProfileId);
    return true;
  }

  void ensurePreviewDeviceProfile() {
    bool hasPreviewSource = false;
    for (const auto &source : sources) {
      if (source.deviceProfileId == "preview-device") {
        hasPreviewSource = true;
        break;
      }
    }

    if (!hasPreviewSource)
      return;

    auto *profile = findDeviceProfile("preview-device");
    if (profile == nullptr) {
      TDeviceProfile previewProfile;
      previewProfile.profileId = "preview-device";
      previewProfile.deviceId = "preview-device";
      previewProfile.displayName = "Preview Device";
      previewProfile.autoDetected = true;
      deviceProfiles.push_back(std::move(previewProfile));
      profile = &deviceProfiles.back();
    }

    profile->profileId = "preview-device";
    profile->deviceId = "preview-device";
    if (profile->displayName.isEmpty())
      profile->displayName = "Preview Device";
    profile->autoDetected = true;

    profile->sources.erase(
        std::remove_if(profile->sources.begin(), profile->sources.end(),
                       [&](const TDeviceSourceProfile &profileSource) {
                         return findSource(profileSource.sourceId) == nullptr;
                       }),
        profile->sources.end());

    for (const auto &source : sources) {
      if (source.deviceProfileId != "preview-device")
        continue;

      auto *profileSource =
          findDeviceSourceProfile("preview-device", source.sourceId);
      if (profileSource == nullptr) {
        TDeviceSourceProfile previewSource;
        previewSource.sourceId = source.sourceId;
        previewSource.displayName = source.displayName.isNotEmpty()
                                        ? source.displayName
                                        : source.sourceId;
        previewSource.kind = source.kind;
        previewSource.mode = source.mode;
        previewSource.ports = source.ports;
        profile->sources.push_back(std::move(previewSource));
        continue;
      }

      profileSource->displayName = source.displayName.isNotEmpty()
                                       ? source.displayName
                                       : source.sourceId;
      profileSource->kind = source.kind;
      profileSource->mode = source.mode;
      profileSource->ports = source.ports;
    }
  }

  void reconcileDeviceProfilesAndSources() {
    ensureDefaultRails();
    ensurePreviewDataIfEmpty();
    ensurePreviewDeviceProfile();
    pruneTransientLearnState();

    std::vector<juce::String> normalizedMissingIds;
    auto appendMissingProfileId = [&](const juce::String &profileId) {
      const auto trimmedId = profileId.trim();
      if (trimmedId.isEmpty() || trimmedId == "preview-device")
        return;

      const bool alreadyPresent = std::any_of(
          normalizedMissingIds.begin(), normalizedMissingIds.end(),
          [&](const juce::String &existingId) { return existingId == trimmedId; });
      if (!alreadyPresent)
        normalizedMissingIds.push_back(trimmedId);
    };

    auto isMarkedMissing = [&](const juce::String &profileId) {
      const auto trimmedId = profileId.trim();
      if (trimmedId.isEmpty())
        return false;

      return std::any_of(
          normalizedMissingIds.begin(), normalizedMissingIds.end(),
          [&](const juce::String &existingId) { return existingId == trimmedId; });
    };

    auto profileIdReferencedByAnySource = [&](const juce::String &profileId) {
      const auto trimmedId = profileId.trim();
      if (trimmedId.isEmpty())
        return false;

      return std::any_of(
          sources.begin(), sources.end(), [&](const TControlSource &source) {
            return source.deviceProfileId.trim() == trimmedId;
          });
    };

    for (const auto &profileId : missingDeviceProfileIds) {
      const auto trimmedId = profileId.trim();
      if (trimmedId.isEmpty() || trimmedId == "preview-device")
        continue;
      appendMissingProfileId(trimmedId);
    }

    for (auto &source : sources) {
      source.missing = false;
      source.degraded = false;

      auto profileId = source.deviceProfileId.trim();
      if (profileId.isEmpty())
        continue;

      if (findDeviceProfile(profileId) == nullptr)
        tryRelinkSourceToCompatibleProfile(source);

      profileId = source.deviceProfileId.trim();
      if (profileId.isEmpty())
        continue;

      const auto *profile = findDeviceProfile(profileId);
      if (profile == nullptr) {
        source.missing = true;
        appendMissingProfileId(profileId);
        continue;
      }

      const auto *profileSource = findDeviceSourceProfile(profileId, source.sourceId);
      if (profileSource == nullptr) {
        source.degraded = true;
        if (isMarkedMissing(profileId))
          source.missing = true;
        continue;
      }

      if (source.displayName.isEmpty())
        source.displayName = profileSource->displayName;
      if (source.ports.empty())
        source.ports = profileSource->ports;

      if (source.kind != profileSource->kind ||
          source.mode != profileSource->mode ||
          !controlPortsMatch(source.ports, profileSource->ports)) {
        source.degraded = true;
      }

      if (isMarkedMissing(profileId))
        source.missing = true;
    }

    for (const auto &profile : deviceProfiles)
      refreshProfileAutoDetectedState(profile.profileId);

    missingDeviceProfileIds = std::move(normalizedMissingIds);
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
