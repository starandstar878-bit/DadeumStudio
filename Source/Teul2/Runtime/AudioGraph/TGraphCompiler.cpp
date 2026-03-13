#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"

#include "Teul2/Document/TDocumentSerializer.h"
#include "Teul2/Document/TTeulDocument.h"

namespace Teul {

juce::var TGraphCompiler::compileDocumentJson(const TTeulDocument &document) {
  return TDocumentSerializer::toJson(document);
}

} // namespace Teul