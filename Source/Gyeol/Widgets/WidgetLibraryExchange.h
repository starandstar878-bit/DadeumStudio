#pragma once

#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Widgets
{
    struct WidgetLibraryManifestVersion
    {
        int major = 1;
        int minor = 0;
        int patch = 0;
    };

    struct WidgetLibraryManifestWidget
    {
        juce::String typeKey;
        juce::String displayName;
        juce::String category;
        juce::StringArray tags;
        juce::String iconKey;
        juce::String defaultsSummary;
    };

    struct WidgetLibraryManifestAsset
    {
        juce::String assetId;
        juce::String relativePath;
        juce::String mime;
    };

    struct WidgetLibraryManifestModel
    {
        WidgetLibraryManifestVersion version;
        std::vector<WidgetLibraryManifestWidget> widgets;
        std::vector<WidgetLibraryManifestAsset> assets;
    };

    juce::Result serializeLibraryManifest(const WidgetRegistry& registry, const juce::File& outputDirectory);
    juce::Result loadLibraryManifest(const juce::File& manifestFile, WidgetLibraryManifestModel& outModel);
}

