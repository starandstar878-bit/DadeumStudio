#include "Gyeol/Export/JuceComponentExport.h"

#include "Gyeol/Core/SceneValidator.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace
{
    struct ExportWidgetEntry
    {
        const Gyeol::WidgetModel* model = nullptr;
        const Gyeol::Widgets::WidgetDescriptor* descriptor = nullptr;
        juce::String typeKey;
        juce::String exportTargetType;
        juce::String memberName;
        juce::String memberType;
        juce::String codegenKind;
        bool supported = true;
        bool usesCustomCodegen = false;
        juce::StringArray constructorLines;
        juce::StringArray resizedLines;
    };

    struct CopiedAssetEntry
    {
        Gyeol::WidgetId assetId = 0;
        juce::String refKey;
        juce::String kind;
        juce::String mimeType;
        juce::String sourcePath;
        juce::String destinationRelativePath;
        bool copied = false;
        bool reused = false;
    };

    juce::String severityToString(Gyeol::Export::IssueSeverity severity)
    {
        switch (severity)
        {
            case Gyeol::Export::IssueSeverity::info: return "INFO";
            case Gyeol::Export::IssueSeverity::warning: return "WARN";
            case Gyeol::Export::IssueSeverity::error: return "ERROR";
        }

        return "INFO";
    }

    juce::String sanitizeIdentifier(const juce::String& raw)
    {
        juce::String sanitized;
        sanitized.preallocateBytes(raw.length() + 8);

        auto isAsciiAlpha = [](auto c) noexcept
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        };
        auto isAsciiDigit = [](auto c) noexcept
        {
            return c >= '0' && c <= '9';
        };

        for (int i = 0; i < raw.length(); ++i)
        {
            const auto c = raw[i];
            const auto keep = isAsciiAlpha(c) || isAsciiDigit(c) || c == '_';

            if (i == 0)
            {
                if (isAsciiDigit(c))
                    sanitized << "_";

                if (keep && !isAsciiDigit(c))
                    sanitized << juce::String::charToString(c);
                else if (keep)
                    sanitized << juce::String::charToString(c);
                else
                    sanitized << "_";
            }
            else
            {
                if (keep)
                    sanitized << juce::String::charToString(c);
                else
                    sanitized << "_";
            }
        }

        if (sanitized.isEmpty())
            sanitized = "_generated";

        return sanitized;
    }

    juce::String makeUniqueMemberName(const juce::String& preferredBase, std::set<juce::String>& usedNames)
    {
        const auto base = sanitizeIdentifier(preferredBase.toLowerCase());
        if (usedNames.insert(base).second)
            return base;

        for (int suffix = 2; suffix < 10000; ++suffix)
        {
            const auto candidate = base + "_" + juce::String(suffix);
            if (usedNames.insert(candidate).second)
                return candidate;
        }

        // Defensive fallback for pathological name collisions.
        const auto fallback = base + "_" + juce::String(juce::Random::getSystemRandom().nextInt(1'000'000));
        usedNames.insert(fallback);
        return fallback;
    }

    juce::String toCppStringLiteral(const juce::String& text)
    {
        return juce::JSON::toString(juce::var(text), false);
    }

    juce::Result ensureDirectory(const juce::File& directory)
    {
        if (directory.getFullPathName().isEmpty())
            return juce::Result::fail("Export output directory is empty");

        if (directory.exists())
        {
            if (!directory.isDirectory())
                return juce::Result::fail("Export output path is not a directory: " + directory.getFullPathName());
            return juce::Result::ok();
        }

        if (!directory.createDirectory())
            return juce::Result::fail("Failed to create directory: " + directory.getFullPathName());

        return juce::Result::ok();
    }

    juce::Result writeTextFile(const juce::File& file, const juce::String& text, bool overwriteExisting)
    {
        if (file.existsAsFile() && !overwriteExisting)
            return juce::Result::fail("Refusing to overwrite existing file: " + file.getFullPathName());

        if (!file.replaceWithText(text))
            return juce::Result::fail("Failed to write file: " + file.getFullPathName());

        return juce::Result::ok();
    }

    juce::String generateHeaderCode(const juce::String& className,
                                    const std::vector<ExportWidgetEntry>& widgets)
    {
        juce::StringArray lines;
        lines.add("#pragma once");
        lines.add("");
        lines.add("#include <JuceHeader.h>");
        lines.add("");
        lines.add("class " + className + " : public juce::Component");
        lines.add("{");
        lines.add("public:");
        lines.add("    " + className + "();");
        lines.add("    ~" + className + "() override = default;");
        lines.add("");
        lines.add("    void resized() override;");
        lines.add("");
        lines.add("private:");

        if (widgets.empty())
        {
            lines.add("    // Scene is empty.");
            lines.add("    juce::Label emptySceneLabel;");
        }
        else
        {
            for (const auto& widget : widgets)
            {
                const auto memberType = widget.memberType.isNotEmpty() ? widget.memberType : juce::String("juce::Label");
                lines.add("    " + memberType + " " + widget.memberName + ";");
            }
        }

        lines.add("");
        lines.add("    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (" + className + ")");
        lines.add("};");
        lines.add("");

        return lines.joinIntoString("\n");
    }

    double readNumericProperty(const Gyeol::PropertyBag& bag,
                               const juce::Identifier& key,
                               double fallback) noexcept
    {
        if (!bag.contains(key))
            return fallback;

        const auto& value = bag[key];
        if (!value.isInt() && !value.isInt64() && !value.isDouble())
            return fallback;

        const auto numeric = static_cast<double>(value);
        if (!std::isfinite(numeric))
            return fallback;

        return numeric;
    }

    juce::String readStringProperty(const Gyeol::PropertyBag& bag,
                                    const juce::Identifier& key,
                                    const juce::String& fallback)
    {
        if (!bag.contains(key))
            return fallback;

        const auto value = bag[key].toString();
        return value.isNotEmpty() ? value : fallback;
    }

    juce::String defaultResizedLine(const ExportWidgetEntry& widget)
    {
        const auto bounds = widget.model->bounds.getSmallestIntegerContainer();
        return "    " + widget.memberName
               + ".setBounds(" + juce::String(bounds.getX())
               + ", " + juce::String(bounds.getY())
               + ", " + juce::String(bounds.getWidth())
               + ", " + juce::String(bounds.getHeight()) + ");";
    }

    void applyBuiltinCodegen(ExportWidgetEntry& widget)
    {
        widget.memberType = "juce::Label";
        widget.codegenKind = "unsupported";
        widget.supported = false;
        widget.usesCustomCodegen = false;
        widget.constructorLines.clear();
        widget.resizedLines.clear();

        if (widget.exportTargetType == "juce::TextButton")
        {
            const auto text = readStringProperty(widget.model->properties, "text", "Button");
            widget.memberType = "juce::TextButton";
            widget.codegenKind = "juce_text_button";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setButtonText(" + toCppStringLiteral(text) + ");");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        if (widget.exportTargetType == "juce::Label")
        {
            const auto text = readStringProperty(widget.model->properties, "text", "Label");
            widget.memberType = "juce::Label";
            widget.codegenKind = "juce_label";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setText(" + toCppStringLiteral(text) + ", juce::dontSendNotification);");
            widget.constructorLines.add("    " + widget.memberName + ".setJustificationType(juce::Justification::centredLeft);");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        if (widget.exportTargetType == "juce::Slider::LinearHorizontal")
        {
            const auto value = juce::jlimit(0.0, 1.0, readNumericProperty(widget.model->properties, "value", 0.5));
            widget.memberType = "juce::Slider";
            widget.codegenKind = "juce_slider_linear";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setSliderStyle(juce::Slider::LinearHorizontal);");
            widget.constructorLines.add("    " + widget.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
            widget.constructorLines.add("    " + widget.memberName + ".setRange(0.0, 1.0, 0.0);");
            widget.constructorLines.add("    " + widget.memberName + ".setValue(" + juce::String(value, 6) + ");");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        if (widget.exportTargetType == "juce::Slider::RotaryVerticalDrag")
        {
            const auto value = juce::jlimit(0.0, 1.0, readNumericProperty(widget.model->properties, "value", 0.5));
            widget.memberType = "juce::Slider";
            widget.codegenKind = "juce_slider_rotary";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setSliderStyle(juce::Slider::RotaryVerticalDrag);");
            widget.constructorLines.add("    " + widget.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
            widget.constructorLines.add("    " + widget.memberName + ".setRange(0.0, 1.0, 0.0);");
            widget.constructorLines.add("    " + widget.memberName + ".setValue(" + juce::String(value, 6) + ");");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        const auto fallback = "Unsupported: " + widget.typeKey;
        widget.constructorLines.add("    " + widget.memberName + ".setText(" + toCppStringLiteral(fallback) + ", juce::dontSendNotification);");
        widget.constructorLines.add("    " + widget.memberName + ".setJustificationType(juce::Justification::centred);");
        widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
    }

    juce::Result applyCustomCodegen(const Gyeol::Widgets::WidgetDescriptor& descriptor, ExportWidgetEntry& widget)
    {
        if (!descriptor.exportCodegen)
            return juce::Result::fail("custom codegen callback is empty");

        Gyeol::Widgets::ExportCodegenContext context { *widget.model,
                                                       widget.memberName,
                                                       widget.typeKey,
                                                       widget.exportTargetType };
        Gyeol::Widgets::ExportCodegenOutput output;

        const auto result = descriptor.exportCodegen(context, output);
        if (result.failed())
            return result;

        if (output.memberType.trim().isEmpty())
            return juce::Result::fail("custom codegen returned empty memberType");

        widget.memberType = output.memberType.trim();
        widget.codegenKind = output.codegenKind.isNotEmpty() ? output.codegenKind : juce::String("custom");
        widget.constructorLines = output.constructorLines;
        widget.resizedLines = output.resizedLines;
        widget.supported = true;
        widget.usesCustomCodegen = true;
        return juce::Result::ok();
    }

    juce::String generateSourceCode(const juce::String& className,
                                    const std::vector<ExportWidgetEntry>& widgets,
                                    const std::vector<juce::String>& assetPreloadPaths)
    {
        juce::StringArray lines;
        lines.add("#include \"" + className + ".h\"");
        lines.add("");
        lines.add("namespace");
        lines.add("{");
        lines.add("    juce::File resolveExportAssetFile(const juce::String& relativePath)");
        lines.add("    {");
        lines.add("        auto baseDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();");
        lines.add("        return baseDir.getChildFile(relativePath);");
        lines.add("    }");
        lines.add("");
        lines.add("    juce::Image preloadExportAssetImage(const juce::String& relativePath)");
        lines.add("    {");
        lines.add("        const auto file = resolveExportAssetFile(relativePath);");
        lines.add("        if (!file.existsAsFile())");
        lines.add("            return {};");
        lines.add("        return juce::ImageFileFormat::loadFrom(file);");
        lines.add("    }");
        lines.add("}");
        lines.add("");
        lines.add(className + "::" + className + "()");
        lines.add("{");

        if (!assetPreloadPaths.empty())
        {
            lines.add("    // Preload exported asset binaries from Assets/.");
            for (const auto& relativePath : assetPreloadPaths)
            {
                lines.add("    juce::ignoreUnused(preloadExportAssetImage(" + toCppStringLiteral(relativePath) + "));");
            }
            lines.add("");
        }

        if (widgets.empty())
        {
            lines.add("    emptySceneLabel.setText(\"Scene is empty\", juce::dontSendNotification);");
            lines.add("    emptySceneLabel.setJustificationType(juce::Justification::centred);");
            lines.add("    addAndMakeVisible(emptySceneLabel);");
        }
        else
        {
            for (const auto& widget : widgets)
            {
                lines.add("    // Widget id=" + juce::String(widget.model->id)
                          + ", type=" + widget.typeKey
                          + ", target=" + widget.exportTargetType
                          + ", codegen=" + (widget.codegenKind.isNotEmpty() ? widget.codegenKind : juce::String("unknown")));

                for (const auto& constructorLine : widget.constructorLines)
                    lines.add(constructorLine);

                lines.add("");
            }
        }

        lines.add("}");
        lines.add("");
        lines.add("void " + className + "::resized()");
        lines.add("{");

        if (widgets.empty())
        {
            lines.add("    emptySceneLabel.setBounds(getLocalBounds());");
        }
        else
        {
            for (const auto& widget : widgets)
            {
                for (const auto& resizedLine : widget.resizedLines)
                    lines.add(resizedLine);
            }
        }

        lines.add("}");
        lines.add("");
        return lines.joinIntoString("\n");
    }

    juce::File resolveInputFilePath(const juce::String& value, const Gyeol::Export::ExportOptions& options)
    {
        if (juce::File::isAbsolutePath(value))
            return juce::File(value);

        if (options.projectRootDirectory.getFullPathName().isNotEmpty())
            return options.projectRootDirectory.getChildFile(value);

        return juce::File::getCurrentWorkingDirectory().getChildFile(value);
    }

    juce::String normalizeRelativePath(const juce::String& value)
    {
        auto normalized = value.trim().replaceCharacter('\\', '/');
        while (normalized.startsWith("/"))
            normalized = normalized.substring(1);
        while (normalized.contains("//"))
            normalized = normalized.replace("//", "/");
        while (normalized.startsWith("../"))
            normalized = normalized.substring(3);
        while (normalized.contains("/../"))
            normalized = normalized.replace("/../", "/");
        return normalized;
    }

    juce::String buildPreferredExportAssetRelativePath(const Gyeol::AssetModel& asset, const juce::File& sourceFile)
    {
        auto normalized = normalizeRelativePath(asset.relativePath);
        if (normalized.isEmpty())
            normalized = sourceFile.getFileName();

        const auto slash = normalized.lastIndexOfChar('/');
        auto parent = slash >= 0 ? normalized.substring(0, slash) : juce::String();
        auto fileName = slash >= 0 ? normalized.substring(slash + 1) : normalized;
        fileName = fileName.trim();
        if (fileName.isEmpty())
            fileName = sourceFile.getFileName();

        if (parent.startsWithIgnoreCase("Assets/"))
            parent = parent.fromFirstOccurrenceOf("Assets/", false, false);

        auto relative = juce::String("Assets");
        if (parent.isNotEmpty())
            relative << "/" << parent;
        relative << "/" << fileName;
        return normalizeRelativePath(relative);
    }

    bool isAssetExcludedFromExport(const Gyeol::AssetModel& asset)
    {
        static constexpr const char* kExportExcludeKey = "export.exclude";
        if (!asset.meta.contains(kExportExcludeKey))
            return false;

        const auto& raw = asset.meta[kExportExcludeKey];
        if (raw.isBool())
            return static_cast<bool>(raw);
        if (raw.isInt() || raw.isInt64() || raw.isDouble())
            return static_cast<double>(raw) != 0.0;

        const auto text = raw.toString().trim().toLowerCase();
        return text == "true" || text == "1" || text == "yes" || text == "on";
    }

    juce::File makeUniqueDestinationPath(const juce::File& outputDirectory,
                                         const juce::String& preferredRelativePath)
    {
        auto normalized = normalizeRelativePath(preferredRelativePath);
        auto candidate = outputDirectory.getChildFile(normalized);
        if (!candidate.existsAsFile())
            return candidate;

        const auto fileName = candidate.getFileName();
        const auto stem = candidate.getFileNameWithoutExtension();
        const auto ext = candidate.getFileExtension();
        auto parentPath = normalizeRelativePath(normalized.upToLastOccurrenceOf(fileName, false, false));
        if (parentPath.endsWith("/"))
            parentPath = parentPath.dropLastCharacters(1);

        for (int suffix = 2; suffix < 10000; ++suffix)
        {
            const auto suffixedName = stem + "_" + juce::String(suffix) + ext;
            auto relative = parentPath.isNotEmpty() ? (parentPath + "/" + suffixedName) : suffixedName;
            relative = normalizeRelativePath(relative);
            candidate = outputDirectory.getChildFile(relative);
            if (!candidate.existsAsFile())
                return candidate;
        }

        const auto fallback = stem + "_" + juce::String(juce::Time::currentTimeMillis()) + ext;
        auto relative = parentPath.isNotEmpty() ? (parentPath + "/" + fallback) : fallback;
        relative = normalizeRelativePath(relative);
        return outputDirectory.getChildFile(relative);
    }

    juce::var serializePropertiesForManifest(const Gyeol::PropertyBag& properties)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        for (int i = 0; i < properties.size(); ++i)
            object->setProperty(properties.getName(i), properties.getValueAt(i));

        return juce::var(object.release());
    }

    juce::var serializeBoundsForManifest(const juce::Rectangle<float>& bounds)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("x", bounds.getX());
        object->setProperty("y", bounds.getY());
        object->setProperty("w", bounds.getWidth());
        object->setProperty("h", bounds.getHeight());
        return juce::var(object.release());
    }

    juce::String relativePathOrAbsolute(const juce::File& target, const juce::File& baseDirectory)
    {
        if (baseDirectory.getFullPathName().isNotEmpty())
        {
            const auto relative = target.getRelativePathFrom(baseDirectory);
            if (relative.isNotEmpty())
                return relative.replaceCharacter('\\', '/');
        }

        return target.getFullPathName().replaceCharacter('\\', '/');
    }

    juce::String buildManifestJson(const Gyeol::DocumentModel& document,
                                   const juce::String& componentClassName,
                                   const std::vector<ExportWidgetEntry>& widgets,
                                   const std::vector<CopiedAssetEntry>& assets)
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("schemaVersion", document.schemaVersion.major * 10000
                                            + document.schemaVersion.minor * 100
                                            + document.schemaVersion.patch);
        root->setProperty("componentClassName", componentClassName);
        root->setProperty("generatedAtUtc", juce::Time::getCurrentTime().toISO8601(true));
        root->setProperty("groupCount", static_cast<int>(document.groups.size()));
        root->setProperty("groupsFlattened", true);

        juce::Array<juce::var> widgetArray;
        for (const auto& widget : widgets)
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("id", Gyeol::widgetIdToJsonString(widget.model->id));
            item->setProperty("typeKey", widget.typeKey);
            item->setProperty("exportTargetType", widget.exportTargetType);
            item->setProperty("codegenKind", widget.codegenKind);
            item->setProperty("memberName", widget.memberName);
            item->setProperty("supported", widget.supported);
            item->setProperty("usesCustomCodegen", widget.usesCustomCodegen);
            item->setProperty("bounds", serializeBoundsForManifest(widget.model->bounds));
            item->setProperty("properties", serializePropertiesForManifest(widget.model->properties));
            widgetArray.add(juce::var(item.release()));
        }
        root->setProperty("widgets", juce::var(widgetArray));

        juce::Array<juce::var> assetArray;
        for (const auto& asset : assets)
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("assetId", Gyeol::widgetIdToJsonString(asset.assetId));
            item->setProperty("refKey", asset.refKey);
            item->setProperty("kind", asset.kind);
            item->setProperty("mime", asset.mimeType);
            item->setProperty("sourcePath", asset.sourcePath);
            item->setProperty("destinationPath", asset.destinationRelativePath);
            item->setProperty("exportPath", asset.destinationRelativePath);
            item->setProperty("copied", asset.copied);
            item->setProperty("reused", asset.reused);
            assetArray.add(juce::var(item.release()));
        }
        root->setProperty("exportedAssets", juce::var(assetArray));

        // Legacy key retained for one release for tooling compatibility.
        root->setProperty("copiedResources", juce::var(assetArray));

        return juce::JSON::toString(juce::var(root.release()), true);
    }
}

namespace Gyeol::Export
{
    void ExportReport::addIssue(IssueSeverity severity, juce::String message)
    {
        if (severity == IssueSeverity::warning)
            ++warningCount;
        else if (severity == IssueSeverity::error)
            ++errorCount;

        issues.push_back(ExportIssue { severity, std::move(message) });
    }

    bool ExportReport::hasErrors() const noexcept
    {
        return errorCount > 0;
    }

    juce::String ExportReport::toText() const
    {
        juce::StringArray lines;
        lines.add("Gyeol Export Report");
        lines.add("===================");
        lines.add("Component Class: " + componentClassName);
        lines.add("Output Directory: " + outputDirectory.getFullPathName());
        lines.add("Generated Header: " + generatedHeaderFile.getFullPathName());
        lines.add("Generated Source: " + generatedSourceFile.getFullPathName());
        lines.add("Manifest File: " + manifestFile.getFullPathName());
        lines.add("Widgets Exported: " + juce::String(exportedWidgetCount));
        lines.add("Assets Copied: " + juce::String(copiedResourceCount));
        lines.add("Assets Total: " + juce::String(totalAssetCount));
        lines.add("Assets Reused: " + juce::String(reusedAssetCount));
        lines.add("Assets Skipped: " + juce::String(skippedAssetCount));
        lines.add("Assets Missing: " + juce::String(missingAssetCount));
        lines.add("Assets Copy Failed: " + juce::String(failedAssetCount));
        lines.add("Warnings: " + juce::String(warningCount));
        lines.add("Errors: " + juce::String(errorCount));
        lines.add("");
        lines.add("Assets Summary:");
        lines.add("- success(copied): " + juce::String(copiedResourceCount));
        lines.add("- success(reused): " + juce::String(reusedAssetCount));
        lines.add("- skipped(metadata): " + juce::String(skippedAssetCount));
        lines.add("- missing: " + juce::String(missingAssetCount));
        lines.add("- failed(copy): " + juce::String(failedAssetCount));
        lines.add("");
        lines.add("Issues:");

        if (issues.empty())
        {
            lines.add("- INFO: no issues");
        }
        else
        {
            for (const auto& issue : issues)
                lines.add("- " + severityToString(issue.severity) + ": " + issue.message);
        }

        lines.add("");
        return lines.joinIntoString("\n");
    }

    juce::Result exportToJuceComponent(const DocumentModel& document,
                                       const Widgets::WidgetRegistry& registry,
                                       const ExportOptions& options,
                                       ExportReport& reportOut)
    {
        reportOut = {};

        const auto classNameInput = options.componentClassName.isNotEmpty()
                                        ? options.componentClassName
                                        : juce::String("GyeolExportedComponent");
        reportOut.componentClassName = sanitizeIdentifier(classNameInput);
        reportOut.outputDirectory = options.outputDirectory;

        auto fail = [&](const juce::String& message)
        {
            reportOut.addIssue(IssueSeverity::error, message);
            return juce::Result::fail(message);
        };

        const auto sceneCheck = Core::SceneValidator::validateScene(document, nullptr);
        if (sceneCheck.failed())
            return fail("Scene validation failed: " + sceneCheck.getErrorMessage());

        if (!document.groups.empty())
        {
            reportOut.addIssue(IssueSeverity::info,
                               "Group metadata is flattened during export (groupCount="
                               + juce::String(static_cast<int>(document.groups.size())) + ").");
        }

        const auto outputDirCheck = ensureDirectory(options.outputDirectory);
        if (outputDirCheck.failed())
            return fail(outputDirCheck.getErrorMessage());

        const auto assetsDirectory = options.outputDirectory.getChildFile("Assets");
        if (assetsDirectory.exists() && !assetsDirectory.deleteRecursively())
            return fail("Failed to clear export Assets directory: " + assetsDirectory.getFullPathName());

        const auto assetsDirCheck = ensureDirectory(assetsDirectory);
        if (assetsDirCheck.failed())
            return fail(assetsDirCheck.getErrorMessage());

        reportOut.generatedHeaderFile = options.outputDirectory.getChildFile(reportOut.componentClassName + ".h");
        reportOut.generatedSourceFile = options.outputDirectory.getChildFile(reportOut.componentClassName + ".cpp");
        reportOut.manifestFile = options.outputDirectory.getChildFile("export-manifest.json");
        reportOut.reportFile = options.outputDirectory.getChildFile("ExportReport.txt");

        std::vector<ExportWidgetEntry> exportWidgets;
        exportWidgets.reserve(document.widgets.size());
        std::set<juce::String> usedMemberNames;

        for (const auto& widget : document.widgets)
        {
            ExportWidgetEntry entry;
            entry.model = &widget;
            entry.descriptor = registry.find(widget.type);

            if (entry.descriptor != nullptr)
            {
                entry.typeKey = entry.descriptor->typeKey.isNotEmpty()
                                    ? entry.descriptor->typeKey
                                    : juce::String("widget_") + juce::String(static_cast<int>(widget.type));
                entry.exportTargetType = entry.descriptor->exportTargetType.isNotEmpty()
                                             ? entry.descriptor->exportTargetType
                                             : entry.typeKey;
            }
            else
            {
                entry.typeKey = "unknown_" + juce::String(static_cast<int>(widget.type));
                entry.exportTargetType = "unsupported";
                reportOut.addIssue(IssueSeverity::warning,
                                   "Widget descriptor missing for widget id="
                                   + juce::String(widget.id)
                                   + " (type ordinal=" + juce::String(static_cast<int>(widget.type)) + ")");
            }

            entry.memberName = makeUniqueMemberName(entry.typeKey + "_" + juce::String(widget.id), usedMemberNames);

            bool customApplied = false;
            if (entry.descriptor != nullptr && static_cast<bool>(entry.descriptor->exportCodegen))
            {
                const auto customResult = applyCustomCodegen(*entry.descriptor, entry);
                if (customResult.wasOk())
                {
                    customApplied = true;
                }
                else
                {
                    reportOut.addIssue(IssueSeverity::warning,
                                       "Custom codegen failed for widget id=" + juce::String(widget.id)
                                       + " (" + entry.typeKey + "): "
                                       + customResult.getErrorMessage()
                                       + ". Falling back to built-in mapping.");
                }
            }

            if (!customApplied)
                applyBuiltinCodegen(entry);

            if (!entry.supported && entry.descriptor != nullptr)
            {
                reportOut.addIssue(IssueSeverity::warning,
                                   "Unsupported export target '" + entry.exportTargetType
                                   + "' for widget id=" + juce::String(widget.id)
                                   + ". Fallback Label will be generated.");
            }

            if (entry.resizedLines.isEmpty())
                entry.resizedLines.add(defaultResizedLine(entry));

            exportWidgets.push_back(std::move(entry));
        }

        std::vector<CopiedAssetEntry> copiedAssets;
        copiedAssets.reserve(document.assets.size());
        reportOut.totalAssetCount = static_cast<int>(document.assets.size());
        std::map<juce::String, juce::String> copiedBySourcePath;
        std::set<juce::String> preloadImagePathsSet;

        for (const auto& asset : document.assets)
        {
            CopiedAssetEntry copied;
            copied.assetId = asset.id;
            copied.refKey = asset.refKey;
            copied.kind = Gyeol::assetKindToKey(asset.kind);
            copied.mimeType = asset.mimeType;

            if (isAssetExcludedFromExport(asset))
            {
                ++reportOut.skippedAssetCount;
                copied.copied = false;
                copied.sourcePath = asset.relativePath;
                reportOut.addIssue(IssueSeverity::info,
                                   "Asset excluded from export by flag: refKey=" + asset.refKey);
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            if (asset.kind == AssetKind::colorPreset)
            {
                ++reportOut.skippedAssetCount;
                copied.copied = false;
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            if (asset.relativePath.trim().isEmpty())
            {
                ++reportOut.missingAssetCount;
                reportOut.addIssue(IssueSeverity::warning,
                                   "Asset path is empty for refKey=" + asset.refKey);
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            const auto source = resolveInputFilePath(asset.relativePath, options);
            auto sourceKey = source.getFullPathName().replaceCharacter('\\', '/');
            copied.sourcePath = sourceKey;

            if (!source.existsAsFile())
            {
                ++reportOut.missingAssetCount;
                reportOut.addIssue(IssueSeverity::warning,
                                   "Asset file not found: refKey=" + asset.refKey
                                   + ", path=" + asset.relativePath);
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            juce::String destinationRelativePath;
            const auto existing = copiedBySourcePath.find(sourceKey);
            if (existing != copiedBySourcePath.end())
            {
                destinationRelativePath = existing->second;
                copied.reused = true;
                ++reportOut.reusedAssetCount;
            }
            else
            {
                const auto preferredRelativePath = buildPreferredExportAssetRelativePath(asset, source);
                const auto destination = makeUniqueDestinationPath(options.outputDirectory,
                                                                   preferredRelativePath);
                const auto destinationParent = destination.getParentDirectory();
                if (!destinationParent.exists() && !destinationParent.createDirectory())
                {
                    ++reportOut.failedAssetCount;
                    reportOut.addIssue(IssueSeverity::warning,
                                       "Failed to create asset folder: " + destinationParent.getFullPathName());
                    copiedAssets.push_back(std::move(copied));
                    continue;
                }

                if (!source.copyFileTo(destination))
                {
                    ++reportOut.failedAssetCount;
                    reportOut.addIssue(IssueSeverity::warning,
                                       "Failed to copy asset file: " + source.getFullPathName());
                    copiedAssets.push_back(std::move(copied));
                    continue;
                }

                destinationRelativePath = relativePathOrAbsolute(destination, options.outputDirectory);
                copiedBySourcePath.emplace(sourceKey, destinationRelativePath);
                ++reportOut.copiedResourceCount;
            }

            copied.destinationRelativePath = destinationRelativePath;
            copied.copied = true;
            copiedAssets.push_back(copied);

            if (asset.kind == AssetKind::image && copied.destinationRelativePath.isNotEmpty())
                preloadImagePathsSet.insert(copied.destinationRelativePath);
        }

        std::vector<juce::String> preloadImagePaths(preloadImagePathsSet.begin(), preloadImagePathsSet.end());

        const auto headerCode = generateHeaderCode(reportOut.componentClassName, exportWidgets);
        const auto sourceCode = generateSourceCode(reportOut.componentClassName, exportWidgets, preloadImagePaths);

        const auto headerWrite = writeTextFile(reportOut.generatedHeaderFile, headerCode, options.overwriteExistingFiles);
        if (headerWrite.failed())
            return fail(headerWrite.getErrorMessage());

        const auto sourceWrite = writeTextFile(reportOut.generatedSourceFile, sourceCode, options.overwriteExistingFiles);
        if (sourceWrite.failed())
            return fail(sourceWrite.getErrorMessage());

        if (options.writeManifestJson)
        {
            const auto manifest = buildManifestJson(document,
                                                    reportOut.componentClassName,
                                                    exportWidgets,
                                                    copiedAssets);

            const auto manifestWrite = writeTextFile(reportOut.manifestFile, manifest, options.overwriteExistingFiles);
            if (manifestWrite.failed())
                return fail(manifestWrite.getErrorMessage());
        }

        reportOut.exportedWidgetCount = static_cast<int>(exportWidgets.size());

        const auto reportWrite = writeTextFile(reportOut.reportFile, reportOut.toText(), true);
        if (reportWrite.failed())
            return fail(reportWrite.getErrorMessage());

        if (reportOut.hasErrors())
            return juce::Result::fail("Export failed. See report: " + reportOut.reportFile.getFullPathName());

        return juce::Result::ok();
    }
}
