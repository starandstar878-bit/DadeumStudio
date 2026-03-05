#pragma once

#include "../Model/TGraphDocument.h"

namespace Teul {

// =============================================================================
//  TSerializer — 그래프 문서의 JSON 직렬화/역직렬화 담당
//
//  JUCE 의 juce::var / juce::DynamicObject 를 사용하여
//  JSON 포맷과의 변환을 수행한다.
// =============================================================================
class TSerializer {
public:
  /** TGraphDocument 객체를 JSON (juce::var) 으로 변환 */
  static juce::var toJson(const TGraphDocument &doc);

  /** JSON (juce::var) 데이터를 TGraphDocument 객체에 채움. 성공 시 true 반환.
   */
  static bool fromJson(TGraphDocument &doc, const juce::var &json);

private:
  static juce::var portToJson(const TPort &port);
  static juce::var nodeToJson(const TNode &node);
  static juce::var connectionToJson(const TConnection &conn);

  static bool jsonToPort(TPort &port, const juce::var &json);
  static bool jsonToNode(TNode &node, const juce::var &json);
  static bool jsonToConnection(TConnection &conn, const juce::var &json);
};

} // namespace Teul
