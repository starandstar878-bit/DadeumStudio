#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    gyeolEditor = Gyeol::createEditor();
    addAndMakeVisible(*gyeolEditor);
    restoreSession();
    setSize (600, 400);
}

MainComponent::~MainComponent()
{
    persistSession();
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

juce::File MainComponent::sessionFilePath()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("DadeumStudio");
    if (!dir.exists())
        dir.createDirectory();

    if (!dir.exists())
    {
        dir = juce::File::getCurrentWorkingDirectory().getChildFile("Builds").getChildFile("GyeolSession");
        if (!dir.exists())
            dir.createDirectory();
    }

    return dir.getChildFile("autosave-session.json");
}

void MainComponent::restoreSession()
{
    if (gyeolEditor == nullptr)
        return;

    const auto file = sessionFilePath();
    if (!file.existsAsFile())
        return;

    const auto result = gyeolEditor->document().loadFromFile(file);
    if (result.failed())
    {
        DBG("[Gyeol] Session restore failed: " + result.getErrorMessage());
        return;
    }

    gyeolEditor->refreshFromDocument();
}

void MainComponent::persistSession() const
{
    if (gyeolEditor == nullptr)
        return;

    const auto file = sessionFilePath();
    const auto parent = file.getParentDirectory();
    if (!parent.exists())
        parent.createDirectory();

    const auto result = gyeolEditor->document().saveToFile(file);
    if (result.failed())
        DBG("[Gyeol] Session save failed: " + result.getErrorMessage());
}
