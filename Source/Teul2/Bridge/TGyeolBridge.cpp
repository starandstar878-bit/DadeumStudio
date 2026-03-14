#include "TGyeolBridge.h"

namespace Teul {

juce::Result TGyeolBridge::dispatchCommand(const GraphCommand &cmd) {
  // TODO: Gyeol 엔진 인스턴스와 통신하여 명령어 처리
  juce::ignoreUnused(cmd);
  return juce::Result::ok();
}

juce::var TGyeolBridge::getDeltaState() {
  // TODO: 마지막 동기화 이후의 변경 사항 반환
  return {};
}

} // namespace Teul
