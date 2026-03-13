#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"

#include "Teul2/Document/TDocumentSerializer.h"
#include "Teul2/Document/TTeulDocument.h"
#include "Teul/Serialization/TSerializer.h"

namespace Teul {

juce::var TGraphCompiler::compileDocumentJson(const TTeulDocument &document) {
  return TDocumentSerializer::toJson(document);
}

bool TGraphCompiler::compileLegacyDocument(TGraphDocument &legacyDocument,
                                           const TTeulDocument &document) {
  return TSerializer::fromJson(legacyDocument, compileDocumentJson(document),
                               nullptr);
}

} // namespace Teul