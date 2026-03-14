#include "TBridgeCodegen.h"
#include "TBridgeJsonCodec.h"

namespace Teul {

namespace {

juce::String generateHeader(const juce::String &className) {
  juce::StringArray lines;
  lines.add("#pragma once");
  lines.add("");
  lines.add("#include <JuceHeader.h>");
  lines.add("#include \"Teul2/Runtime/TTeulRuntime.h\"");
  lines.add("");
  lines.add("/**");
  lines.add(" * \\brief Auto-generated Teul2 Runtime Module");
  lines.add(" */");
  lines.add("class " + className + " : public juce::AudioIODeviceCallback");
  lines.add("{");
  lines.add("public:");
  lines.add("    " + className + "();");
  lines.add("    ~" + className + "() override = default;");
  lines.add("");
  lines.add("    // --- juce::AudioIODeviceCallback ---");
  lines.add("    void audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,");
  lines.add("                               float** outputChannelData, int numOutputChannels,");
  lines.add("                               int numSamples) override;");
  lines.add("    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;");
  lines.add("    void audioDeviceStopped() override;");
  lines.add("");
  lines.add("    // --- Parameter Control ---");
  lines.add("    bool setParam(const juce::String& paramId, float value);");
  lines.add("    float getParam(const juce::String& paramId) const;");
  lines.add("");
  lines.add("    static juce::String getEmbeddedRuntimeJson();");
  lines.add("");
  lines.add("private:");
  lines.add("    Teul::TTeulRuntime runtime;");
  lines.add("};");
  lines.add("");
  return lines.joinIntoString("\r\n");
}

juce::String generateSource(const juce::String &className, const TExportGraphIR &graph) {
  juce::StringArray lines;
  lines.add("#include \"" + className + ".h\"");
  lines.add("");
  lines.add(className + "::" + className + "()");
  lines.add("{");
  lines.add("    // In a real implementation, we would load the graph from JSON here.");
  lines.add("    // For now, this is a placeholder for the codegen logic.");
  lines.add("}");
  lines.add("");
  lines.add("void " + className + "::audioDeviceIOCallback(const float** inputs, int numIn, float** outputs, int numOut, int numSamples)");
  lines.add("{");
  lines.add("    runtime.audioDeviceIOCallback(inputs, numIn, outputs, numOut, numSamples);");
  lines.add("}");
  lines.add("");
  lines.add("void " + className + "::audioDeviceAboutToStart(juce::AudioIODevice* device)");
  lines.add("{");
  lines.add("    runtime.audioDeviceAboutToStart(device);");
  lines.add("}");
  lines.add("");
  lines.add("void " + className + "::audioDeviceStopped()");
  lines.add("{");
  lines.add("    runtime.audioDeviceStopped();");
  lines.add("}");
  lines.add("");
  lines.add("bool " + className + "::setParam(const juce::String& paramId, float value)");
  lines.add("{");
  lines.add("    return runtime.setParam(paramId, value);");
  lines.add("}");
  lines.add("");
  lines.add("float " + className + "::getParam(const juce::String& paramId) const");
  lines.add("{");
  lines.add("    return runtime.getParam(paramId);");
  lines.add("}");
  lines.add("");
  lines.add("juce::String " + className + "::getEmbeddedRuntimeJson()");
  lines.add("{");
  lines.add("    return " + TBridgeCodegen::toCppStringLiteral(juce::JSON::toString(TBridgeJsonCodec::encodeGraphIR(graph))) + ";");
  lines.add("}");
  lines.add("");
  return lines.joinIntoString("\r\n");
}

} // namespace

TBridgeCodegen::CodegenResult TBridgeCodegen::generate(const TExportGraphIR &graph, 
                                                     const juce::String &className,
                                                     const juce::String &exportNamespace) {
  juce::ignoreUnused(exportNamespace);
  CodegenResult result;
  result.headerCode = generateHeader(className);
  result.sourceCode = generateSource(className, graph);
  
  juce::StringArray cmake;
  cmake.add("# Auto-generated CMake snippet for " + className);
  cmake.add("set(" + className + "_SOURCES " + className + ".cpp " + className + ".h)");
  result.cmakeSnippet = cmake.joinIntoString("\r\n");
  
  return result;
}

juce::String TBridgeCodegen::sanitizeIdentifier(const juce::String &raw) {
  juce::String sanitized;
  for (int i = 0; i < raw.length(); ++i) {
    auto c = raw[i];
    const bool isAlpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    const bool isDigit = (c >= '0' && c <= '9');
    if (isAlpha || (i > 0 && isDigit) || c == '_')
      sanitized << juce::String::charToString(c);
    else
      sanitized << "_";
  }
  return sanitized.isEmpty() ? "GeneratedModule" : sanitized;
}

juce::String TBridgeCodegen::toCppStringLiteral(const juce::String &text) {
  return juce::JSON::toString(juce::var(text), false);
}

} // namespace Teul
