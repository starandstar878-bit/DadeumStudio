#pragma once

#include "Gyeol/Public/Types.h"

namespace Gyeol::Serialization
{
    juce::Result saveDocumentToFile(const juce::File& file,
                                    const DocumentModel& document,
                                    const EditorStateModel& editorState);

    juce::Result loadDocumentFromFile(const juce::File& file,
                                      DocumentModel& documentOut,
                                      EditorStateModel& editorStateOut);
}
