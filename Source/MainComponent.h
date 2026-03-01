#pragma once

#include <JuceHeader.h>
#include "Gyeol/Gyeol.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::Component
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    void restoreSession();
    void persistSession() const;
    static juce::File sessionFilePath();

    std::unique_ptr<Gyeol::EditorHandle> gyeolEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
