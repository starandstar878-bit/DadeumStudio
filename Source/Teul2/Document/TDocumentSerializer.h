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
  static juce::var migrateDocumentJson(
      const juce::var &json,
      int sourceSchemaVersion,
      TSchemaMigrationReport *migrationReportOut = nullptr);
  static juce::var normalizeDocumentJson(const juce::var &json);
  static juce::var migrateDocumentV1ToV2(const juce::var &json);
  static juce::var migrateDocumentV2ToV3(const juce::var &json);
  static juce::var migrateMetaJson(const juce::var &json);
  static juce::var migrateNodeJson(const juce::var &json);
  static juce::var migratePortJson(const juce::var &json);
  static juce::var migrateConnectionJson(const juce::var &json);
  static juce::var migrateFrameJsonV2(const juce::var &json);
  static juce::var migrateFrameJson(const juce::var &json);
  static juce::var migrateBookmarkJson(const juce::var &json);

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
