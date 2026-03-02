#pragma once

#include "Gyeol/Public/Types.h"
#include <JuceHeader.h>

namespace Gyeol::Serialization {
juce::Result serializeDocumentToJsonString(const DocumentModel &document,
                                           const EditorStateModel &editorState,
                                           juce::String &jsonOut);

juce::Result saveDocumentToFile(const juce::File &file,
                                const DocumentModel &document,
                                const EditorStateModel &editorState);

juce::Result loadDocumentFromFile(const juce::File &file,
                                  DocumentModel &documentOut,
                                  EditorStateModel &editorStateOut);
} // namespace Gyeol::Serialization
