#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    gyeolEditor = Gyeol::createEditor();
    addAndMakeVisible(*gyeolEditor);
    setSize (600, 400);
}

MainComponent::~MainComponent()
{
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    if (gyeolEditor != nullptr)
        gyeolEditor->setBounds(getLocalBounds());
}
