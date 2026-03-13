#pragma once

#include <JuceHeader.h>

namespace Teul {

class TTeulDocument;

class TGraphCompiler {
public:
  static juce::var compileDocumentJson(const TTeulDocument &document);
};

} // namespace Teul