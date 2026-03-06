#include "TFileIo.h"
#include "TSerializer.h"

namespace Teul {

bool TFileIo::saveToFile(const TGraphDocument &doc, const juce::File &file) {
  juce::var json = TSerializer::toJson(doc);
  juce::String jsonString = juce::JSON::toString(json);

  // UTF-8 without BOM 파일 작성
  return file.replaceWithText(jsonString, false, false, "\r\n");
}

bool TFileIo::loadFromFile(TGraphDocument &doc, const juce::File &file) {
  if (!file.existsAsFile())
    return false;

  juce::String jsonString = file.loadFileAsString();
  juce::var json;
  juce::Result result = juce::JSON::parse(jsonString, json);

  if (result.failed())
    return false;

  return TSerializer::fromJson(doc, json);
}

} // namespace Teul
