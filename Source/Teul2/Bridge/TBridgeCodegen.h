#pragma once

#include "Teul2/Bridge/TTeulBridge.h"
#include <JuceHeader.h>

namespace Teul {

/**
 * \brief Teul2 익스포트용 C++ 코드 생성을 담당하는 클래스
 * 
 * 컴파일된 그래프(TExportGraphIR)를 독립적인 C++ 클래스로 변환하여 
 * 외부 프로젝트에서 Teul2 런타임 없이도 DSP를 실행할 수 있게 합니다.
 */
class TBridgeCodegen {
public:
  struct CodegenResult {
    juce::String headerCode;
    juce::String sourceCode;
    juce::String cmakeSnippet;
  };

  static CodegenResult generate(const TExportGraphIR &graph, 
                                const juce::String &className,
                                const juce::String &exportNamespace = "TeulGenerated");

  // --- 유틸리티 ---
  static juce::String sanitizeIdentifier(const juce::String &raw);
  static juce::String toCppStringLiteral(const juce::String &text);

private:
  TBridgeCodegen() = delete;
};

} // namespace Teul
