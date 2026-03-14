#pragma once

#include "Teul2/Bridge/TTeulBridge.h"
#include <JuceHeader.h>

namespace Teul {

/**
 * \brief Teul2와 Naru(호스트 애플리케이션) 사이의 최상위 브릿지 클래스
 * 
 * 호스트 앱에서 엔진의 생명주기를 관리하고, 프로젝트 수준의 명령(Load, Save, Export)을
 * 수행할 수 있는 고수준 인터페이스를 제공합니다.
 */
class TNaruBridge {
public:
  TNaruBridge();
  ~TNaruBridge();

  // --- 프로젝트 관리 ---
  juce::Result loadProject(const juce::File &file);
  juce::Result saveProject(const juce::File &file);
  
  // --- 익스포트 연동 ---
  TExportResult runExport(const TExportOptions &options);

  // --- 상태 모니터링 ---
  juce::var getEngineStatus() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};

} // namespace Teul
