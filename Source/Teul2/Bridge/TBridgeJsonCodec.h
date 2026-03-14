#pragma once

#include "Teul2/Bridge/TTeulBridge.h"
#include <JuceHeader.h>

namespace Teul {

/**
 * \brief Teul2 브릿지 레이어의 JSON 인코딩/디코딩을 담당하는 유틸리티 클래스
 * 
 * TExportReport, TExportGraphIR 등 외부 전송용 데이터를 JSON(juce::var)으로 
 * 변환하거나 그 반대의 작업을 수행하며, 레거시 TExport의 JSON 로직을 정제하여 집중화합니다.
 */
class TBridgeJsonCodec {
public:
  // --- 인코딩 (Export IR -> JSON) ---
  
  static juce::var encodeReport(const TExportReport &report);
  static juce::var encodeGraphIR(const TExportGraphIR &graph);
  static juce::var encodeIssue(const TExportIssue &issue);
  static juce::var encodeAsset(const TExportAssetIR &asset);

  // --- 헬퍼 문자열 변환 ---
  
  static juce::String exportModeToString(TExportMode mode);
  static juce::String severityToString(TExportIssueSeverity severity);
  static juce::String issueCodeToString(TExportIssueCode code);
  static juce::String portDataTypeToString(TPortDataType type);
  static juce::String portDirectionToString(TPortDirection direction);
  static juce::String valueTypeToString(TParamValueType type);
  static juce::String widgetHintToString(TParamWidgetHint hint);

private:
  TBridgeJsonCodec() = delete;
};

} // namespace Teul
