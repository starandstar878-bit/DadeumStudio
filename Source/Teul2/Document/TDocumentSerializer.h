#pragma once

#include "TDocumentMigration.h"
#include "TTeulDocument.h"

namespace Teul {

class TDocumentSerializer {
public:
  static int currentSchemaVersion() noexcept;
  static juce::var toJson(const TTeulDocument &doc);
  static bool fromJson(TTeulDocument &doc,
                       const juce::var &json,
                       TSchemaMigrationReport *migrationReportOut = nullptr);

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