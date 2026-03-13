#pragma once

#include <JuceHeader.h>

namespace Teul {

class TTeulDocument;
struct TGraphDocument;

class TGraphCompiler {
public:
  static juce::var compileDocumentJson(const TTeulDocument &document);
  static bool compileLegacyDocument(TGraphDocument &legacyDocument,
                                    const TTeulDocument &document);
};

} // namespace Teul