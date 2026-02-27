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

    struct CopiedResourceEntry
    {
        Gyeol::WidgetId widgetId = 0;
        juce::String propertyKey;
        juce::String sourcePath;
        juce::String destinationRelativePath;
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
                                    const std::vector<ExportWidgetEntry>& widgets)
    {
        juce::StringArray lines;
        lines.add("#include \"" + className + ".h\"");
        lines.add("");
        lines.add(className + "::" + className + "()");
        lines.add("{");

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

    bool isResourcePropertyKey(const juce::String& key)
    {
        return key.containsIgnoreCase("image")
            || key.containsIgnoreCase("asset")
            || key.endsWithIgnoreCase("path");
    }

    bool isLikelyPathValue(const juce::String& value)
    {
        return value.containsChar('/')
            || value.containsChar('\\')
            || value.containsChar('.');
    }

    juce::File resolveResourceFile(const juce::String& value, const Gyeol::Export::ExportOptions& options)
    {
        if (juce::File::isAbsolutePath(value))
            return juce::File(value);

        if (options.projectRootDirectory.getFullPathName().isNotEmpty())
            return options.projectRootDirectory.getChildFile(value);

        return juce::File::getCurrentWorkingDirectory().getChildFile(value);
    }

    juce::File makeUniqueDestinationPath(const juce::File& resourceDirectory, const juce::File& sourceFile)
    {
        auto candidate = resourceDirectory.getChildFile(sourceFile.getFileName());
        if (!candidate.existsAsFile())
            return candidate;

        const auto stem = sourceFile.getFileNameWithoutExtension();
        const auto ext = sourceFile.getFileExtension();

        for (int suffix = 2; suffix < 10000; ++suffix)
        {
            candidate = resourceDirectory.getChildFile(stem + "_" + juce::String(suffix) + ext);
            if (!candidate.existsAsFile())
                return candidate;
        }

        return resourceDirectory.getChildFile(stem + "_" + juce::String(juce::Time::currentTimeMillis()) + ext);
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
                                   const std::vector<CopiedResourceEntry>& resources)
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

        juce::Array<juce::var> resourceArray;
        for (const auto& resource : resources)
        {
            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("widgetId", Gyeol::widgetIdToJsonString(resource.widgetId));
            item->setProperty("propertyKey", resource.propertyKey);
            item->setProperty("sourcePath", resource.sourcePath);
            item->setProperty("destinationPath", resource.destinationRelativePath);
            resourceArray.add(juce::var(item.release()));
        }
        root->setProperty("copiedResources", juce::var(resourceArray));

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
        lines.add("Resources Copied: " + juce::String(copiedResourceCount));
        lines.add("Warnings: " + juce::String(warningCount));
        lines.add("Errors: " + juce::String(errorCount));
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

        const auto resourcesDirectory = options.outputDirectory.getChildFile("Resources");
        const auto resourcesDirCheck = ensureDirectory(resourcesDirectory);
        if (resourcesDirCheck.failed())
            return fail(resourcesDirCheck.getErrorMessage());

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

        const auto headerCode = generateHeaderCode(reportOut.componentClassName, exportWidgets);
        const auto sourceCode = generateSourceCode(reportOut.componentClassName, exportWidgets);

        const auto headerWrite = writeTextFile(reportOut.generatedHeaderFile, headerCode, options.overwriteExistingFiles);
        if (headerWrite.failed())
            return fail(headerWrite.getErrorMessage());

        const auto sourceWrite = writeTextFile(reportOut.generatedSourceFile, sourceCode, options.overwriteExistingFiles);
        if (sourceWrite.failed())
            return fail(sourceWrite.getErrorMessage());

        std::vector<CopiedResourceEntry> copiedResources;
        std::map<juce::String, juce::String> copiedBySourcePath;

        for (const auto& widget : exportWidgets)
        {
            const auto& properties = widget.model->properties;
            for (int i = 0; i < properties.size(); ++i)
            {
                const auto key = properties.getName(i).toString();
                const auto& valueVar = properties.getValueAt(i);

                if (!valueVar.isString())
                    continue;
                if (!isResourcePropertyKey(key))
                    continue;

                const auto value = valueVar.toString().trim();
                if (value.isEmpty() || !isLikelyPathValue(value))
                    continue;

                const auto source = resolveResourceFile(value, options);
                if (!source.existsAsFile())
                {
                    reportOut.addIssue(IssueSeverity::warning,
                                       "Resource not found for widget id="
                                       + juce::String(widget.model->id)
                                       + ", property=" + key
                                       + ", value=" + value);
                    continue;
                }

                auto sourceKey = source.getFullPathName();
                sourceKey = sourceKey.replaceCharacter('\\', '/');

                juce::String destinationRelativePath;
                const auto existingCopy = copiedBySourcePath.find(sourceKey);
                if (existingCopy != copiedBySourcePath.end())
                {
                    destinationRelativePath = existingCopy->second;
                }
                else
                {
                    const auto destination = makeUniqueDestinationPath(resourcesDirectory, source);
                    if (!source.copyFileTo(destination))
                    {
                        reportOut.addIssue(IssueSeverity::warning,
                                           "Failed to copy resource: " + source.getFullPathName());
                        continue;
                    }

                    destinationRelativePath = relativePathOrAbsolute(destination, options.outputDirectory);
                    copiedBySourcePath.emplace(sourceKey, destinationRelativePath);
                    ++reportOut.copiedResourceCount;
                }

                CopiedResourceEntry copied;
                copied.widgetId = widget.model->id;
                copied.propertyKey = key;
                copied.sourcePath = sourceKey;
                copied.destinationRelativePath = destinationRelativePath;
                copiedResources.push_back(std::move(copied));
            }
        }

        if (options.writeManifestJson)
        {
            const auto manifest = buildManifestJson(document,
                                                    reportOut.componentClassName,
                                                    exportWidgets,
                                                    copiedResources);

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
