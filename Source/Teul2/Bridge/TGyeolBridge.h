#pragma once

#include "Teul2/Bridge/TTeulBridge.h"
#include <JuceHeader.h>

namespace Teul {

/**
 * \brief Teul2와 Gyeol(그래프 로직/히스토리 엔진) 사이의 통신을 담당하는 브릿지
 * 
 * 그래프의 구조적 변경 요청을 Gyeol 엔진으로 전달하거나, 
 * Gyeol로부터 전송된 변경 사항을 Teul2 도큐먼트/런타임에 반영합니다.
 */
class TGyeolBridge {
public:
  struct GraphCommand {
    juce::String commandType;
    juce::var data;
  };

  static juce::Result dispatchCommand(const GraphCommand &cmd);
  static juce::var getDeltaState();

private:
  TGyeolBridge() = delete;
};

} // namespace Teul
