#pragma once

#include "../Model/TTypes.h"
#include <JuceHeader.h>
#include <vector>

namespace Teul {

// =============================================================================
//  TParamSpec — 노드가 갖는 기본 파라미터 구조 정의
// =============================================================================
struct TParamSpec {
  juce::String key;
  juce::String label;
  juce::var defaultValue;
};

// =============================================================================
//  TPortSpec — 노드가 갖는 포트 레이아웃(틀) 정의
// =============================================================================
struct TPortSpec {
  TPortDirection direction;
  TPortDataType dataType;
  juce::String name;
};

// =============================================================================
//  TNodeCapabilities — 런타임/UI에서 이 노드가 지원하는 특수 기능들
// =============================================================================
struct TNodeCapabilities {
  bool canBypass = true; // 바이패스(DSP 우회) 가능 여부
  bool canMute = false;  // 뮤트(출력 차단) 가능 여부
  bool canSolo = false;  // 솔로(다른 노드 음소거) 가능 여부
  bool isTimeDependent =
      false; // 딜레이/리버브 등 내부 상태(버퍼)가 있는지 여부

  // 추가적인 성능/동작 힌트 (Phase 3 런타임 최적화 & 시각화용)
  int minInstances = 0;  // 반드시 존재해야 하는 개수 (예: AudioOut은 1)
  int maxInstances = -1; // 최대 인스턴스 허용 개수 (-1 이면 무제한)
  int maxPolyphony = 1;  // 한 노드가 처리 가능한 동시 발음 수 (MIDI, 신스 계열)
  float processingLatencyMs =
      0.0f; // 노드가 유발하는 DSP 처리 지연 시간(Lookahead 등)
  int estimatedCpuCost =
      1; // 상대적인 CPU 소모량 (1: 아주 가벼움[Add], 10: 무거움[Reverb])
};

// =============================================================================
//  TNodeDescriptor — 특정 타입 노드의 카탈로그 정보
// =============================================================================
struct TNodeDescriptor {
  juce::String typeKey;
  juce::String displayName;
  juce::String category;

  TNodeCapabilities capabilities; // 노드의 능력치/허용되는 동작들

  std::vector<TParamSpec> paramSpecs;
  std::vector<TPortSpec> portSpecs;
};

// =============================================================================
//  TNodeRegistry — 내장 및 커스텀 노드의 메타데이터를 통합 관리
// =============================================================================
class TNodeRegistry {
public:
  TNodeRegistry() = default;

  void registerNode(const TNodeDescriptor &desc);

  /** typeKey 를 통해 등록된 노드 디스크립터를 반환 (없으면 nullptr) */
  const TNodeDescriptor *descriptorFor(const juce::String &typeKey) const;

  /** 전체 디스크립터 목록 반환 (GUI 메뉴/라이브러리 구성 용도) */
  const std::vector<TNodeDescriptor> &getAllDescriptors() const;

private:
  std::vector<TNodeDescriptor> descriptors;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNodeRegistry)
};

// =============================================================================
//  전역 팩토리 함수: 기본 내장 노드들이 모두 등록된 레지스트리 생성
// =============================================================================
std::unique_ptr<TNodeRegistry> makeDefaultNodeRegistry();

} // namespace Teul
