#pragma once

#include "../Model/TNode.h"
#include "../Model/TTypes.h"
#include <JuceHeader.h>
#include <map>


namespace Teul {

// =============================================================================
//  TProcessContext — 런타임 오디오 콜백 중에 전달되는 데이터 참조 컨테이너
// =============================================================================
struct TProcessContext {
  juce::AudioBuffer<float> *globalPortBuffer = nullptr;
  juce::AudioBuffer<float> *deviceAudioBuffer = nullptr;
  juce::MidiBuffer *midiMessages = nullptr;

  // 현재 노드의 포트 ID -> globalPortBuffer 의 채널 인덱스 맵
  const std::map<PortId, int> *portToChannel = nullptr;

  // 런타임에 처리해야 할 대상 노드의 파라미터/메타 데이터 읽기 전용 참조
  const TNode *nodeData = nullptr;
};

// =============================================================================
//  TNodeInstance — 노드의 런타임 상태(DSP 버퍼, 위상 등) 및 연산 베이스
//  인터페이스
// =============================================================================
class TNodeInstance {
public:
  virtual ~TNodeInstance() = default;

  // 오디오 스트림 진입 시 호출
  virtual void prepareToPlay(double sampleRate,
                             int maximumExpectedSamplesPerBlock) {}

  // 오디오 스트림 중단 시 호출
  virtual void releaseResources() {}

  // 런타임 연산을 구체화하는 함수
  virtual void processSamples(const TProcessContext &context) {}

  // 노드 재설정 (예: LFO 위상 0으로 강제 리셋 등)
  virtual void reset() {}
};

} // namespace Teul
