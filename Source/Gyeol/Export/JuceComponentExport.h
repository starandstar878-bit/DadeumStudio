#pragma once

#include "Gyeol/Public/Types.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <vector>

namespace Gyeol::Export
{
    enum class IssueSeverity
    {
        info,
        warning,
        error
    };

    struct ExportIssue
    {
        IssueSeverity severity = IssueSeverity::info;
        juce::String message;
    };

    struct ExportReport
    {
        juce::String componentClassName { "GyeolExportedComponent" };
        juce::File outputDirectory;
        juce::File generatedHeaderFile;
        juce::File generatedSourceFile;
        juce::File manifestFile;
        juce::File runtimeDataFile;
        juce::File reportFile;

        int exportedWidgetCount = 0;
        int copiedResourceCount = 0;
        int totalAssetCount = 0;
        int skippedAssetCount = 0;
        int missingAssetCount = 0;
        int failedAssetCount = 0;
        int reusedAssetCount = 0;
        int warningCount = 0;
        int errorCount = 0;

        std::vector<ExportIssue> issues;

        void addIssue(IssueSeverity severity, juce::String message);
        bool hasErrors() const noexcept;
        juce::String toText() const;
    };

    struct ExportOptions
    {
        juce::File outputDirectory;
        juce::File projectRootDirectory;
        juce::String componentClassName { "GyeolExportedComponent" };
        bool overwriteExistingFiles = true;
        bool writeManifestJson = true;
        bool writeRuntimeDataJson = true;
    };

    juce::Result exportToJuceComponent(const DocumentModel& document,
                                       const Widgets::WidgetRegistry& registry,
                                       const ExportOptions& options,
                                       ExportReport& reportOut);
}
