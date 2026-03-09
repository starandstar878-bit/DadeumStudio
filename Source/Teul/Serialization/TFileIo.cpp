#include "TFileIo.h"
#include "TSerializer.h"

namespace Teul {

bool TFileIo::saveToFile(const TGraphDocument &doc, const juce::File &file) {
  juce::var json = TSerializer::toJson(doc);
  juce::String jsonString = juce::JSON::toString(json);

  // UTF-8 without BOM 파일 작성
  return file.replaceWithText(jsonString, false, false, "\r\n");
}

bool TFileIo::loadFromFile(TGraphDocument &doc, const juce::File &file,
                           TSchemaMigrationReport *migrationReportOut) {
  if (!file.existsAsFile())
    return false;

  juce::String jsonString = file.loadFileAsString();
  juce::var json;
  juce::Result result = juce::JSON::parse(jsonString, json);

  if (result.failed())
    return false;

  TSchemaMigrationReport migrationReport;
  if (!TSerializer::fromJson(doc, json, &migrationReport))
    return false;

  if (migrationReport.degraded || !migrationReport.warnings.isEmpty()) {
    const auto level = migrationReport.degraded
                           ? TDocumentNoticeLevel::degraded
                           : TDocumentNoticeLevel::warning;
    const auto title = migrationReport.degraded
                           ? juce::String("Document restored in degraded mode")
                           : juce::String("Document compatibility warnings");
    doc.setTransientNotice(level, title,
                           migrationReport.warnings.joinIntoString(" | "));
  } else {
    doc.clearTransientNotice();
  }

  if (migrationReportOut != nullptr)
    *migrationReportOut = migrationReport;

  return true;
}

} // namespace Teul
