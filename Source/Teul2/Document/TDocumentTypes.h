#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <map>
#include <vector>
#include <algorithm>
#include <memory>

namespace Teul {

// =============================================================================
//  기본 ID 타입
//
//  규칙: 삭제된 ID는 절대 재사용하지 않는다.
//  (Undo 스택에서 ID 동일성으로 객체를 추적하기 때문)
// =============================================================================
using NodeId = uint32_t;
using PortId = uint32_t;
using ConnectionId = uint32_t;

static constexpr NodeId kInvalidNodeId = 0;
static constexpr PortId kInvalidPortId = 0;
static constexpr ConnectionId kInvalidConnectionId = 0;

// =============================================================================
//  포트 방향
// =============================================================================
enum class TPortDirection {
  Input,
  Output,
};

// =============================================================================
//  포트 데이터 타입
//
//  색상 규약 (UI Phase 2 에서 사용):
//    Audio   -> Green   (#4CAF50)
//    CV      -> Amber   (#FFC107)
//    MIDI    -> Blue    (#2196F3)
//    Gate    -> Orange  (#FF9800)
//    Control -> Purple  (#9C27B0)
// =============================================================================
enum class TPortDataType {
  Audio,   // 오디오 신호 (PCM float 샘플 버퍼)
  CV,      // Control Voltage - -1.0 ~ +1.0 float, 파라미터 변조용
  MIDI,    // MIDI 메시지 스트림
  Gate,    // 0 또는 1 (트리거 / 게이트)
  Control, // UI 직접 제어 값 (파라미터 노브 등)
};

// =============================================================================
//  파라미터 관련 타입 (Teul/Bridge/ITeulParamProvider.h 에서 이관)
// =============================================================================

enum class TParamValueType {
  Auto,
  Float,
  Int,
  Bool,
  Enum,
  String,
};

enum class TParamWidgetHint {
  Auto,
  Slider,
  Combo,
  Toggle,
  Text,
};

struct TParamOptionSpec {
  juce::String id;
  juce::String label;
  juce::var value;
};

// =============================================================================
//  TParamSpec - 파라미터 상세 명세
// =============================================================================

struct TParamSpec {
  juce::String key;
  juce::String label;
  juce::var defaultValue;

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

// =============================================================================
//  TPortSpec - 포트 상세 명세
// =============================================================================

struct TPortSpec {
  TPortDirection direction;
  TPortDataType dataType;

  juce::String name;                      // 대표 이름
  int channelCount = 1;                   // 구조상 채널 수
  std::vector<juce::String> channelNames; // 채널별 세부 이름(optional)

  int maxIncomingConnections = 1;
  int maxOutgoingConnections = -1;
};

// =============================================================================
//  TNodeCapabilities - 노드 기능 및 제약 조건
// =============================================================================

struct TNodeCapabilities {
  bool canBypass = true;
  bool canMute = false;
  bool canSolo = false;
  bool isTimeDependent = false;

  int minInstances = 0;
  int maxInstances = -1;
  int maxPolyphony = 1;
  float processingLatencyMs = 0.0f;
  int estimatedCpuCost = 1;
};

enum class TNodeExportSupport {
  Unsupported,
  JsonOnly,
  RuntimeModuleOnly,
  Both,
};

class TNodeInstance;

// =============================================================================
//  TTeulExposedParam (Teul/Bridge/ITeulParamProvider.h 에서 이관)
// =============================================================================

struct TTeulExposedParam {
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

// =============================================================================
//  TNodeDescriptor - 노드 정의 (정적 정보)
// =============================================================================

struct TNode; // Forward declaration

struct TNodeDescriptor {
  juce::String typeKey;
  juce::String displayName;
  juce::String category;

  TNodeCapabilities capabilities;
  TNodeExportSupport exportSupport = TNodeExportSupport::Both;

  std::vector<TParamSpec> paramSpecs;
  std::vector<TPortSpec> portSpecs;

  std::function<std::unique_ptr<TNodeInstance>()> instanceFactory;
  std::function<std::vector<TTeulExposedParam>(const TNode &node)>
      exposedParamFactory;
};

// =============================================================================
//  Helper Functions - 파라미터/포트 명세 생성 헬퍼
// =============================================================================

inline TParamOptionSpec makeParamOption(const juce::String &id,
                                        const juce::String &label,
                                        const juce::var &value) {
  TParamOptionSpec option;
  option.id = id;
  option.label = label;
  option.value = value;
  return option;
}

inline TParamSpec
makeFloatParamSpec(const juce::String &key, const juce::String &label,
                   float defaultValue, float minValue, float maxValue,
                   float step = 0.0f, const juce::String &unitLabel = {},
                   int displayPrecision = 2, const juce::String &group = {},
                   const juce::String &description = {}) {
  TParamSpec spec;
  spec.key = key;
  spec.label = label;
  spec.defaultValue = defaultValue;
  spec.valueType = TParamValueType::Float;
  spec.minValue = minValue;
  spec.maxValue = maxValue;
  spec.step = step;
  spec.unitLabel = unitLabel;
  spec.displayPrecision = displayPrecision;
  spec.group = group;
  spec.description = description;
  spec.preferredWidget = TParamWidgetHint::Slider;
  return spec;
}

inline TParamSpec makeEnumParamSpec(const juce::String &key,
                                    const juce::String &label, int defaultValue,
                                    std::vector<TParamOptionSpec> enumOptions,
                                    const juce::String &group = {},
                                    const juce::String &description = {}) {
  TParamSpec spec;
  spec.key = key;
  spec.label = label;
  spec.defaultValue = defaultValue;
  spec.valueType = TParamValueType::Enum;
  spec.group = group;
  spec.description = description;
  spec.enumOptions = std::move(enumOptions);
  spec.preferredWidget = TParamWidgetHint::Combo;
  spec.isDiscrete = true;
  return spec;
}

inline TPortSpec makePortSpec(TPortDirection direction, TPortDataType dataType,
                              const juce::String &name) {
  TPortSpec spec;
  spec.direction = direction;
  spec.dataType = dataType;
  spec.name = name;
  spec.channelCount = 1;
  return spec;
}

inline TPortSpec makePortSpec(TPortDirection direction, TPortDataType dataType,
                              const juce::String &name, int channelCount) {
  TPortSpec spec;
  spec.direction = direction;
  spec.dataType = dataType;
  spec.name = name;
  spec.channelCount = juce::jmax(1, channelCount);
  for (int i = 0; i < spec.channelCount; ++i) {
    spec.channelNames.push_back(name + " " + juce::String(i + 1));
  }
  return spec;
}

inline TPortSpec makePortSpec(TPortDirection direction, TPortDataType dataType,
                              const juce::String &name, int channelCount,
                              std::vector<juce::String> channelNames) {
  TPortSpec spec;
  spec.direction = direction;
  spec.dataType = dataType;
  spec.name = name;
  spec.channelCount = juce::jmax(1, channelCount);
  spec.channelNames = std::move(channelNames);

  while ((int)spec.channelNames.size() < spec.channelCount) {
    spec.channelNames.push_back(
        spec.name + " " + juce::String((int)spec.channelNames.size() + 1));
  }
  return spec;
}

inline juce::String makeTeulParamId(NodeId nodeId,
                                    const juce::String &paramKey) {
  return "teul.node." + juce::String(nodeId) + "." + paramKey;
}

inline bool parseTeulParamId(const juce::String &paramId,
                             NodeId &outNodeId,
                             juce::String &outParamKey) {
  const juce::String prefix = "teul.node.";
  if (!paramId.startsWith(prefix))
    return false;

  const juce::String tail = paramId.fromFirstOccurrenceOf(prefix, false, false);
  const int split = tail.indexOfChar('.');
  if (split <= 0)
    return false;

  outNodeId = tail.substring(0, split).getIntValue();
  outParamKey = tail.substring(split + 1);
  return outNodeId != kInvalidNodeId && outParamKey.isNotEmpty();
}

// =============================================================================
//  TPort - 포트 데이터 모델
//
//  포트는 TNode 가 직접 소유한다 (std::vector<TPort> nodes).
//  ownerNodeId 는 역참조용으로 보관하며, 직렬화 시에도 기록된다.
//
//  channelIndex:
//    AudioInput / AudioOutput 노드에서 몇 번째 하드웨어 채널을 가리키는지.
//    일반 DSP 노드에서는 항상 0.
// =============================================================================
struct TPort {
  PortId portId = kInvalidPortId;
  TPortDirection direction = TPortDirection::Input;
  TPortDataType dataType = TPortDataType::Audio;
  juce::String name;
  NodeId ownerNodeId = kInvalidNodeId;
  int channelIndex = 0;
  int maxIncomingConnections = 1;
  int maxOutgoingConnections = -1;
};

// =============================================================================
//  TNode - 노드 데이터 모델
//
//  TNode 는 순수 직렬화 가능 데이터다.
//  DSP 상태(필터 계수, 딜레이 버퍼 등)는 Phase 3 에서 도입할
//  TNodeInstance 가 별도로 소유한다.
//
//  포트 소유:
//    ports 벡터가 이 노드의 모든 포트를 소유한다.
//    TGraphDocument 는 포트를 별도 컬렉션으로 관리하지 않으며,
//    항상 node.ports 를 통해 접근한다.
//
//  metadata:
//    직렬화 대상이지만 DSP 로직과 무관한 UI 상태값.
// =============================================================================
struct TNode {
  NodeId nodeId = kInvalidNodeId;
  juce::String typeKey; // 노드 종류 식별자 (예: "Oscillator", "LowPassFilter")
  float x = 0.0f;       // 캔버스 상의 위치 (월드 좌표)
  float y = 0.0f;

  // 파라미터 맵 - 타입 키에 따라 내용이 달라진다.
  // 값 타입: float, int, bool, String (juce::var 이 모두 수용)
  std::map<juce::String, juce::var> params;

  // 이 노드에 속한 포트 목록 (Input 먼저, Output 나중 정렬 권장)
  std::vector<TPort> ports;

  // UI 메타데이터
  bool collapsed = false; // 노드 최소화 상태
  bool bypassed = false;  // 바이패스 (DSP 우회)
  juce::String label;     // 사용자 지정 이름 (비어있으면 typeKey 사용)
  juce::String colorTag;  // node color tag for organization

  // 상태 메타데이터 (UI 에러 배지 표시용)
  bool hasError = false;
  juce::String errorMessage;

  // -------------------------------------------------------------------------
  //  포트 탐색 헬퍼
  // -------------------------------------------------------------------------
  TPort *findPort(PortId id) noexcept {
    for (auto &p : ports)
      if (p.portId == id)
        return &p;
    return nullptr;
  }

  const TPort *findPort(PortId id) const noexcept {
    for (const auto &p : ports)
      if (p.portId == id)
        return &p;
    return nullptr;
  }
};

enum class TEndpointOwnerKind {
  NodePort,
  RailPort,
};

struct TEndpoint {
  NodeId nodeId = kInvalidNodeId;
  PortId portId = kInvalidPortId;
  TEndpointOwnerKind ownerKind = TEndpointOwnerKind::NodePort;
  juce::String railEndpointId;
  juce::String railPortId;

  static TEndpoint makeNodePort(NodeId nodeIdIn, PortId portIdIn) {
    TEndpoint endpoint;
    endpoint.nodeId = nodeIdIn;
    endpoint.portId = portIdIn;
    endpoint.ownerKind = TEndpointOwnerKind::NodePort;
    return endpoint;
  }

  static TEndpoint makeRailPort(const juce::String &endpointId,
                                const juce::String &portIdIn) {
    TEndpoint endpoint;
    endpoint.ownerKind = TEndpointOwnerKind::RailPort;
    endpoint.railEndpointId = endpointId;
    endpoint.railPortId = portIdIn;
    return endpoint;
  }

  bool isNodePort() const noexcept {
    return ownerKind == TEndpointOwnerKind::NodePort;
  }

  bool isRailPort() const noexcept {
    return ownerKind == TEndpointOwnerKind::RailPort;
  }

  bool isValid() const noexcept {
    if (isNodePort())
      return nodeId != kInvalidNodeId && portId != kInvalidPortId;

    return railEndpointId.isNotEmpty() && railPortId.isNotEmpty();
  }

  bool referencesNode(NodeId id) const noexcept {
    return isNodePort() && nodeId == id;
  }
};

struct TConnection {
  ConnectionId connectionId = kInvalidConnectionId;
  TEndpoint from;
  TEndpoint to;

  bool isValid() const noexcept {
    return connectionId != kInvalidConnectionId && from.isValid() &&
           to.isValid();
  }
};

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

    auto *profileSource =
        findDeviceSourceProfile(normalizedProfileId, source->sourceId);
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

    profileSource->displayName =
        source.displayName.isNotEmpty() ? source.displayName : source.sourceId;
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

  juce::String
  findReconnectCandidateProfileId(const juce::String &presentProfileId,
                                  const juce::String &deviceId,
                                  const juce::String &displayName) const {
    const auto normalizedPresentId = presentProfileId.trim();
    if (normalizedPresentId.isEmpty())
      return {};

    auto isMarkedMissing = [&](const juce::String &profileId) {
      return std::any_of(missingDeviceProfileIds.begin(),
                         missingDeviceProfileIds.end(),
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
    auto &profile = ensureDeviceProfile(normalizedProfileId, deviceId,
                                        displayName, autoDetected);
    const auto previousDeviceId = profile.deviceId;
    const auto previousDisplayName = profile.displayName;
    const bool previousAutoDetected = profile.autoDetected;
    const bool wasMarkedMissing = std::any_of(
        missingDeviceProfileIds.begin(), missingDeviceProfileIds.end(),
        [&](const juce::String &existingId) {
          return existingId.trim() == normalizedProfileId;
        });

    profile.deviceId = deviceId.isNotEmpty() ? deviceId : profile.deviceId;
    profile.displayName =
        displayName.isNotEmpty() ? displayName : profile.displayName;
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
    if (normalizedProfileId.isEmpty() ||
        normalizedProfileId == "preview-device")
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
        const auto matchIt = std::find_if(
            profile.sources.begin(), profile.sources.end(),
            [&](const TDeviceSourceProfile &profileSource) {
              return matchesProfileSource(profileSource, allowEmptyPorts);
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

      const bool alreadyPresent =
          std::any_of(normalizedMissingIds.begin(), normalizedMissingIds.end(),
                      [&](const juce::String &existingId) {
                        return existingId == trimmedId;
                      });
      if (!alreadyPresent)
        normalizedMissingIds.push_back(trimmedId);
    };

    auto isMarkedMissing = [&](const juce::String &profileId) {
      const auto trimmedId = profileId.trim();
      if (trimmedId.isEmpty())
        return false;

      return std::any_of(normalizedMissingIds.begin(),
                         normalizedMissingIds.end(),
                         [&](const juce::String &existingId) {
                           return existingId == trimmedId;
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

      const auto *profileSource =
          findDeviceSourceProfile(profileId, source.sourceId);
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
} // namespace Teul
