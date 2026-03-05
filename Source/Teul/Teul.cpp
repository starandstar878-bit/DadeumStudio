#include "Teul.h"
#include "Model/TGraphDocument.h"

#include <queue>
#include <unordered_set>

// =============================================================================
//  TGraphDocument::wouldCreateCycle
//
//  BFS 로 fromNodeId 에서 출발해 toNodeId 에 도달 가능한지 검사.
//  도달 가능하면 연결 추가 시 순환이 생기므로 true 반환.
// =============================================================================
bool Teul::TGraphDocument::wouldCreateCycle(NodeId fromNodeId,
                                            NodeId toNodeId) const noexcept {
  if (fromNodeId == toNodeId)
    return true;

  std::unordered_set<NodeId> visited;
  std::queue<NodeId> queue;
  queue.push(toNodeId);

  while (!queue.empty()) {
    const NodeId current = queue.front();
    queue.pop();

    if (current == fromNodeId)
      return true;

    if (!visited.insert(current).second)
      continue;

    for (const auto &c : connections) {
      if (c.from.nodeId == current &&
          visited.find(c.to.nodeId) == visited.end())
        queue.push(c.to.nodeId);
    }
  }

  return false;
}

// =============================================================================
//  Teul::EditorHandle
//
//  현재는 스텁. Phase 1 UI 작업 시 Impl(pimpl) 로 교체 예정.
// =============================================================================
Teul::EditorHandle::EditorHandle() = default;
Teul::EditorHandle::~EditorHandle() = default;

void Teul::EditorHandle::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a2e));
  g.setColour(juce::Colour(0xff555577));
  g.setFont(16.0f);
  g.drawText(juce::CharPointer_UTF8(
                 "\xf0\x9f\x94\xa7  Teul \xe2\x80\x94 DSP Graph Editor  |  "
                 "Phase 1 \xea\xb5\xac\xed\x98\x84 \xec\xa4\x91"),
             getLocalBounds(), juce::Justification::centred, true);
}

void Teul::EditorHandle::resized() {}

std::unique_ptr<Teul::EditorHandle> Teul::createEditor() {
  return std::make_unique<EditorHandle>();
}
