#pragma once
#include "../Model/TGraphDocument.h"
#include "../Registry/TNodeRegistry.h"
#include "TNodeInstance.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>


namespace Teul {

// =============================================================================
//  TGraphRuntime — 위상 정렬 기반 오디오 그래프 실행 및 DSP 관리 엔진
// =============================================================================
class TGraphRuntime {
public:
  TGraphRuntime(const TNodeRegistry *registry);
  ~TGraphRuntime();

  // ===========================================================================
  // 빌드 및 위상 정렬 (UI/메인 스레드 계열에서 동작)
  // ===========================================================================

  // 성공 시 true, 순환 참조(Cycle) 감지로 인한 실패 시 false 반환
  bool buildGraph(const TGraphDocument &doc);

  // ===========================================================================
  // 오디오/DSP 생명주기 (렌더 스레드에서 접근)
  // ===========================================================================
  void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock);
  void releaseResources();
  void processBlock(juce::AudioBuffer<float> &deviceBuffer,
                    juce::MidiBuffer &midiMessages);

private:
  const TNodeRegistry *nodeRegistry;

  double currentSampleRate = 48000.0;
  int currentBlockSize = 256;

  // 한 포트 채널에서 다른 포트 채널로 데이터를 믹싱(합산)해야 하는 연결 정보
  struct MixOp {
    int srcChannelIndex;
    int dstChannelIndex;
  };

  // 런타임에 정렬되어 실행될 노드 엔트리 단위
  struct NodeEntry {
    NodeId nodeId;
    const TNode *nodeData = nullptr;
    std::unique_ptr<TNodeInstance> instance;

    // 이 노드가 processSamples() 를 처리하기 직전에 선행되어야 할 버퍼
    // 다운스트림 믹스
    std::vector<MixOp> preProcessMixes;

    // TProcessContext 에 넘길 포트 -> 글로벌 채널 인덱스 맵
    std::map<PortId, int> portChannels;
  };

  // 위상 정렬이 완료된 실행 순서 큐
  std::vector<NodeEntry> sortedNodes;

  // 모든 노드의 모든 포트 버퍼를 한 곳에 모은 평면화 오디오 버퍼
  juce::AudioBuffer<float> globalPortBuffer;
  int totalAllocatedChannels = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TGraphRuntime)
};

} // namespace Teul
