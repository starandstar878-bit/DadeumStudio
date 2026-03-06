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

  totalAllocatedChannels = portChannelCounter;
  globalPortBuffer.setSize(totalAllocatedChannels > 0 ? totalAllocatedChannels
                                                      : 1,
                           currentBlockSize, false, false, true);
  globalPortBuffer.clear();

  // =========================================================================
  // 3. 다중 입력 믹싱(Summing) 로직 수집
  // =========================================================================
  // 다수의 출력이 하나의 대상 포트로 쏟아질 수 있다 (N:1 연결)
  // 대상 노드가 process 틱을 굴리기 전에, src 채널 값을 dst 채널로 합산하는
  // 믹스 옵코드 저장
  for (const auto &conn : doc.connections) {
    NodeId dstNodeId = conn.to.nodeId;
    int srcChannel = globalPortIndexMap[conn.from.portId];
    int dstChannel = globalPortIndexMap[conn.to.portId];

    for (auto &entry : newSortedNodes) {
      if (entry.nodeId == dstNodeId) {
        entry.preProcessMixes.push_back({srcChannel, dstChannel});
        break;
      }
    }
  }

  // UI 스레드 빌드 오퍼레이션이 다 끝나고 난 뒤, 실제 오디오 런타임 배열을 일괄
  // 교체
  // TODO [Phase 3 단계 2]: 이 교체(swap) 구간에 mutex/SpinLock/Double Buffer
  // 등을 도입하여 락-프리 보강
  sortedNodes = std::move(newSortedNodes);
  return true;
}

void TGraphRuntime::prepareToPlay(double sampleRate,
                                  int maximumExpectedSamplesPerBlock) {
  currentSampleRate = sampleRate;
  currentBlockSize = maximumExpectedSamplesPerBlock;

  if (totalAllocatedChannels > 0) {
    globalPortBuffer.setSize(totalAllocatedChannels, currentBlockSize, false,
                             false, true);
  }

  for (auto &entry : sortedNodes) {
    if (entry.instance) {
      entry.instance->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
    }
  }
}

void TGraphRuntime::releaseResources() {
  for (auto &entry : sortedNodes) {
    if (entry.instance) {
      entry.instance->releaseResources();
    }
  }
}

void TGraphRuntime::processBlock(juce::AudioBuffer<float> &deviceBuffer,
                                 juce::MidiBuffer &midiMessages) {
  // 이전 오디오 틱에서 사용한 포트 잔류값 일괄 청소
  globalPortBuffer.clear();

  int numSamples = deviceBuffer.getNumSamples();

  for (auto &entry : sortedNodes) {
    // 1. 해당 노드로 연결되어 들어온 다운스트림들을 합산 (Sum)
    for (const auto &mix : entry.preProcessMixes) {
      globalPortBuffer.addFrom(mix.dstChannelIndex, 0, globalPortBuffer,
                               mix.srcChannelIndex, 0, numSamples);
    }

    // 2. 바이패스 상태가 아니면 실제 DSP 틱 연산
    if (entry.instance && !entry.nodeData->bypassed) {
      TProcessContext ctx;
      ctx.globalPortBuffer = &globalPortBuffer;
      ctx.deviceAudioBuffer =
          &deviceBuffer; // 최종 AudioOutput 노드가 쓸 하드웨어 버퍼
      ctx.midiMessages = &midiMessages;
      ctx.portToChannel = &entry.portChannels;
      ctx.nodeData = entry.nodeData; // 파라미터(값) 참조 읽기 제공

      entry.instance->processSamples(ctx);
    }
  }
}

} // namespace Teul
