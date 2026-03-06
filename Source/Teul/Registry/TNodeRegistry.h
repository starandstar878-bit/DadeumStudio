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
//  TNodeDescriptor — 특정 타입 노드의 카탈로그 정보
// =============================================================================
struct TNodeDescriptor {
  juce::String typeKey;
  juce::String displayName;
  juce::String category;

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
