#include "TGraphRuntime.h"
#include <map>
#include <queue>

namespace Teul {

TGraphRuntime::TGraphRuntime(const TNodeRegistry *registry)
    : nodeRegistry(registry) {}

TGraphRuntime::~TGraphRuntime() { releaseResources(); }

bool TGraphRuntime::buildGraph(const TGraphDocument &doc) {
  // 1. 위상 정렬 (Topological Sort / Kahn's algorithm)
  std::map<NodeId, int> inDegree;
  std::map<NodeId, std::vector<NodeId>> adj;

  // 모든 노드 진입차수 초기화
  for (const auto &node : doc.nodes) {
    inDegree[node.nodeId] = 0;
  }

  // 엣지 생성 (단방향)
  for (const auto &conn : doc.connections) {
    adj[conn.from.nodeId].push_back(conn.to.nodeId);
    inDegree[conn.to.nodeId]++;
  }

  std::queue<NodeId> q;
  for (auto &pair : inDegree) {
    if (pair.second == 0)
      q.push(pair.first); // 진입차수가 0인(Source) 노드부터 큐에 삽입
  }

  std::vector<NodeId> sortedIds;
  while (!q.empty()) {
    NodeId curr = q.front();
    q.pop();
    sortedIds.push_back(curr);

    // 이웃 노드들의 진입차수 깎기
    for (NodeId neighbor : adj[curr]) {
      if (--inDegree[neighbor] == 0) {
        q.push(neighbor);
      }
    }
  }

  // 큐를 다 돌았는데 전체 노드 개수만큼 추출하지 못했다 = 사이클(순환 참조)
  // 감지
  if (sortedIds.size() != doc.nodes.size()) {
    return false;
  }

  // =========================================================================
  // 2. 오디오 채널 (버퍼) 라우팅
  // =========================================================================
  std::vector<NodeEntry> newSortedNodes;
  newSortedNodes.reserve(sortedIds.size());

  int portChannelCounter = 0;
  std::map<PortId, int> globalPortIndexMap;

  // 노드를 순회하며 고유 채널 인덱스 맵핑 및 인스턴스 팩토리 주입
  for (const auto &id : sortedIds) {
    const TNode *n = doc.findNode(id);
    if (!n)
      continue;

    NodeEntry entry;
    entry.nodeId = n->nodeId;
    entry.nodeData = n;

    for (const auto &p : n->ports) {
      entry.portChannels[p.portId] = portChannelCounter;
      globalPortIndexMap[p.portId] = portChannelCounter;
      portChannelCounter++;
    }

    // TNodeInstance 생성 (DSP 상태 컨테이너)
    if (nodeRegistry) {
      if (auto desc = nodeRegistry->descriptorFor(n->typeKey)) {
        if (desc->instanceFactory) {
          entry.instance = desc->instanceFactory();
          // 생성과 동시에 현재 런타임 샘플레이트 전달 (예: Oscillators 내부
          // 위상 초기화)
          if (entry.instance) {
            entry.instance->prepareToPlay(currentSampleRate, currentBlockSize);
          }
        }
      }
    }
    newSortedNodes.push_back(std::move(entry));
  }

  // UI 스레드 빌드 오퍼레이션이 다 끝나고 난 뒤, 새 RenderState 구성
  auto newState = new RenderState();
  newState->sortedNodes = std::move(newSortedNodes);
  newState->globalPortBuffer.setSize(
      totalAllocatedChannels > 0 ? totalAllocatedChannels : 1, currentBlockSize,
      false, false, true);
  newState->globalPortBuffer.clear();
  newState->totalAllocatedChannels = totalAllocatedChannels;

  // 락프리 방식으로 현재 렌더 상태 교환
  activeState.set(newState);
  return true;
}

void TGraphRuntime::prepareToPlay(double sampleRate,
                                  int maximumExpectedSamplesPerBlock) {
  currentSampleRate = sampleRate;
  currentBlockSize = maximumExpectedSamplesPerBlock;

  auto state = activeState.get();
  if (state) {
    for (auto &entry : state->sortedNodes) {
      if (entry.instance) {
        entry.instance->prepareToPlay(sampleRate,
                                      maximumExpectedSamplesPerBlock);
      }
    }
  }
}

void TGraphRuntime::releaseResources() {
  auto state = activeState.get();
  if (state) {
    for (auto &entry : state->sortedNodes) {
      if (entry.instance) {
        entry.instance->releaseResources();
      }
    }
  }
}

void TGraphRuntime::processBlock(juce::AudioBuffer<float> &deviceBuffer,
                                 juce::MidiBuffer &midiMessages) {
  auto state = activeState.get();
  if (!state) {
    deviceBuffer.clear();
    return;
  }

  // 이전 오디오 틱에서 사용한 포트 잔류값 일괄 청소
  state->globalPortBuffer.clear();

  int numSamples = deviceBuffer.getNumSamples();

  // 1. 파라미터 변경 사항(FIFO 큐) 처리 (Lock-free pop)
  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramQueueFifo.prepareToRead(paramQueueFifo.getNumReady(), start1, size1,
                               start2, size2);

  auto applyParamChange = [&](int index) {
    const auto &change = paramQueueData[index];
    juce::String key = juce::String::fromUTF8(change.paramKey);
    for (auto &entry : state->sortedNodes) {
      if (entry.nodeId == change.nodeId && entry.instance) {
        entry.instance->setParameterValue(key, change.value);
        break;
      }
    }
  };

  if (size1 > 0) {
    for (int i = 0; i < size1; ++i)
      applyParamChange(start1 + i);
  }
  if (size2 > 0) {
    for (int i = 0; i < size2; ++i)
      applyParamChange(start2 + i);
  }
  paramQueueFifo.finishedRead(size1 + size2);

  for (auto &entry : state->sortedNodes) {
    // 2. 해당 노드로 연결되어 들어온 다운스트림들을 합산 (Sum)
    for (const auto &mix : entry.preProcessMixes) {
      state->globalPortBuffer.addFrom(mix.dstChannelIndex, 0,
                                      state->globalPortBuffer,
                                      mix.srcChannelIndex, 0, numSamples);
    }

    // 2. 바이패스 상태가 아니면 실제 DSP 틱 연산
    if (entry.instance && !entry.nodeData->bypassed) {
      TProcessContext ctx;
      ctx.globalPortBuffer = &state->globalPortBuffer;
      ctx.deviceAudioBuffer =
          &deviceBuffer; // 최종 AudioOutput 노드가 쓸 하드웨어 버퍼
      ctx.midiMessages = &midiMessages;
      ctx.portToChannel = &entry.portChannels;
      ctx.nodeData = entry.nodeData; // 파라미터(값) 참조 읽기 제공

      entry.instance->processSamples(ctx);
    }
  }
}

// ===========================================================================
// juce::AudioIODeviceCallback 필수 오버라이드 구현
// ===========================================================================
void TGraphRuntime::audioDeviceAboutToStart(juce::AudioIODevice *device) {
  if (device) {
    prepareToPlay(device->getCurrentSampleRate(),
                  device->getCurrentBufferSizeSamples());
  }
}

void TGraphRuntime::audioDeviceStopped() { releaseResources(); }

void TGraphRuntime::audioDeviceIOCallback(const float **inputChannelData,
                                          int numInputChannels,
                                          float **outputChannelData,
                                          int numOutputChannels,
                                          int numSamples) {

  // AudioIODevice 의 채널 데이터를 랩퍼로 감싸 호환시킴
  juce::AudioBuffer<float> buffer(const_cast<float **>(outputChannelData),
                                  numOutputChannels, numSamples);

  // TODO [Phase 3 Phase 1.5]: Input Data 를 그래프에 넣는 경우 InputChannelData
  // 도 감싸야 함 현재는 출력에만 렌더링하도록 임시 구성
  buffer.clear();

  juce::MidiBuffer midi; // 독립 실행 환경에선 일단 비어있는 MIDI 버퍼 전달
                         // (추후 미디 인풋 장치와 연동)
  processBlock(buffer, midi);
}

// ===========================================================================
// Phase 3 단계 2: 파라미터 변경 Lock-free 큐잉
// ===========================================================================
void TGraphRuntime::queueParameterChange(NodeId nodeId,
                                         const juce::String &paramKey,
                                         float value) {
  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramQueueFifo.prepareToWrite(1, start1, size1, start2, size2);

  if (size1 > 0) {
    paramQueueData[start1].nodeId = nodeId;
    paramQueueData[start1].value = value;
    paramKey.copyToUTF8(paramQueueData[start1].paramKey,
                        sizeof(ParamChange::paramKey) - 1);
  }

  paramQueueFifo.finishedWrite(size1 + size2);
}

} // namespace Teul
