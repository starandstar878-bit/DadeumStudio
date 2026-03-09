#pragma once

#include "../Model/TGraphDocument.h"

namespace Teul {

struct TSchemaMigrationReport {
  int sourceSchemaVersion = 0;
  int targetSchemaVersion = 0;
  bool migrated = false;
  bool usedLegacyAliases = false;
  bool degraded = false;
  juce::StringArray warnings;
};

class TSerializer {
public:
  static int currentSchemaVersion() noexcept;
  static juce::var toJson(const TGraphDocument &doc);
  static bool fromJson(TGraphDocument &doc,
                       const juce::var &json,
                       TSchemaMigrationReport *migrationReportOut = nullptr);

private:
  static juce::var migrateDocumentJson(const juce::var &json);
  static bool usesLegacyDocumentAliases(const juce::var &json);
  static juce::var migrateMetaJson(const juce::var &json);
  static juce::var migrateNodeJson(const juce::var &json);
  static juce::var migratePortJson(const juce::var &json);
  static juce::var migrateConnectionJson(const juce::var &json);
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