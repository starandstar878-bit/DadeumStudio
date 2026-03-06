#pragma once

#include "../Model/TGraphDocument.h"
#include <JuceHeader.h>

namespace Teul {

// =============================================================================
//  TFileIo — .teul 확장자 파일 입출력
// =============================================================================
class TFileIo {
public:
  /** TGraphDocument 객체를 .teul (JSON 형식) 파일로 저장합니다. */
  static bool saveToFile(const TGraphDocument &doc, const juce::File &file);

  /** .teul 파일에서 JSON 데이터를 읽어 TGraphDocument 객체에 채웁니다. */
  static bool loadFromFile(TGraphDocument &doc, const juce::File &file);
};

} // namespace Teul
