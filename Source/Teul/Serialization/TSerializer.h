#pragma once

#include "../Model/TGraphDocument.h"

namespace Teul {

class TSerializer {
public:
  static juce::var toJson(const TGraphDocument &doc);
  static bool fromJson(TGraphDocument &doc, const juce::var &json);

private:
  static juce::var portToJson(const TPort &port);
  static juce::var nodeToJson(const TNode &node);
  static juce::var connectionToJson(const TConnection &conn);
  static juce::var frameToJson(const TFrameRegion &frame);
  static juce::var bookmarkToJson(const TBookmark &bookmark);

  static bool jsonToPort(TPort &port, const juce::var &json);
  static bool jsonToNode(TNode &node, const juce::var &json);
  static bool jsonToConnection(TConnection &conn, const juce::var &json);
  static bool jsonToFrame(TFrameRegion &frame, const juce::var &json);
  static bool jsonToBookmark(TBookmark &bookmark, const juce::var &json);
};

} // namespace Teul