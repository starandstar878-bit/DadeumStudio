#include "Gyeol/Editor/Panels/ValidationPanel.h"

#include "Gyeol/Core/SceneValidator.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        juce::File resolveProjectRootDirectory()
        {
            auto projectRoot = juce::File::getCurrentWorkingDirectory();
            for (int depth = 0; depth < 10; ++depth)
            {
                if (projectRoot.getChildFile("DadeumStudio.jucer").existsAsFile())
                    return projectRoot;

                const auto parent = projectRoot.getParentDirectory();
                if (parent == projectRoot)
                    break;
                projectRoot = parent;
            }

            return juce::File::getCurrentWorkingDirectory();
        }

        juce::File resolveInputFilePath(const juce::String& value, const juce::File& projectRoot)
        {
            if (juce::File::isAbsolutePath(value))
                return juce::File(value);
            return projectRoot.getChildFile(value);
        }

        juce::String inferMimeTypeFromFile(const juce::File& file)
        {
            const auto ext = file.getFileExtension().trimCharactersAtStart(".").toLowerCase();
            if (ext == "png") return "image/png";
            if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
            if (ext == "bmp") return "image/bmp";
            if (ext == "gif") return "image/gif";
            if (ext == "svg") return "image/svg+xml";
            if (ext == "webp") return "image/webp";
            if (ext == "ttf") return "font/ttf";
            if (ext == "otf") return "font/otf";
            if (ext == "woff") return "font/woff";
            if (ext == "woff2") return "font/woff2";
            if (ext == "wav") return "audio/wav";
            if (ext == "aif" || ext == "aiff") return "audio/aiff";
            if (ext == "ogg") return "audio/ogg";
            if (ext == "flac") return "audio/flac";
            if (ext == "mp3") return "audio/mpeg";
            if (ext == "json") return "application/json";
            if (ext == "xml") return "application/xml";
            if (ext == "txt") return "text/plain";
            return {};
        }

        juce::String formatByteSize(juce::int64 bytes)
        {
            const auto positiveBytes = std::max<juce::int64>(bytes, 0);
            if (positiveBytes < 1024)
                return juce::String(positiveBytes) + " B";

            const auto asDouble = static_cast<double>(positiveBytes);
            if (positiveBytes < 1024 * 1024)
                return juce::String(asDouble / 1024.0, 1) + " KB";
            if (positiveBytes < 1024LL * 1024LL * 1024LL)
                return juce::String(asDouble / (1024.0 * 1024.0), 2) + " MB";
            return juce::String(asDouble / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
        }
    }

    ValidationPanel::ValidationPanel(DocumentHandle& documentIn, const Widgets::WidgetRegistry& registryIn)
        : document(documentIn),
          registry(registryIn)
    {
        titleLabel.setText("Validation", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(192, 200, 214));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        summaryLabel.setText("Stale", juce::dontSendNotification);
        summaryLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 170, 186));
        summaryLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(summaryLabel);

        autoRefreshToggle.setClickingTogglesState(true);
        autoRefreshToggle.setToggleState(autoRefresh, juce::dontSendNotification);
        autoRefreshToggle.onClick = [this]
        {
            setAutoRefreshEnabled(autoRefreshToggle.getToggleState());
        };
        addAndMakeVisible(autoRefreshToggle);

        runButton.onClick = [this]
        {
            refreshValidation();
        };
        addAndMakeVisible(runButton);

        listBox.setModel(this);
        listBox.setRowHeight(40);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
        listBox.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
        addAndMakeVisible(listBox);
    }

    ValidationPanel::~ValidationPanel()
    {
        listBox.setModel(nullptr);
    }

    void ValidationPanel::markDirty()
    {
        dirty = true;
        if (autoRefresh)
            refreshValidation();
        else
            summaryLabel.setText("Stale (Run required)", juce::dontSendNotification);
    }

    void ValidationPanel::refreshValidation()
    {
        rebuildIssues();
        dirty = false;

        int errorCount = 0;
        int warningCount = 0;
        for (const auto& issue : issues)
        {
            if (issue.severity == IssueSeverity::error)
                ++errorCount;
            else if (issue.severity == IssueSeverity::warning)
                ++warningCount;
        }

        if (errorCount > 0)
            summaryLabel.setText("Errors: " + juce::String(errorCount) + ", Warnings: " + juce::String(warningCount),
                                 juce::dontSendNotification);
        else if (warningCount > 0)
            summaryLabel.setText("Warnings: " + juce::String(warningCount), juce::dontSendNotification);
        else
            summaryLabel.setText("OK", juce::dontSendNotification);

        listBox.updateContent();
        repaint();
    }

    bool ValidationPanel::autoRefreshEnabled() const noexcept
    {
        return autoRefresh;
    }

    void ValidationPanel::setAutoRefreshEnabled(bool enabled)
    {
        autoRefresh = enabled;
        autoRefreshToggle.setToggleState(autoRefresh, juce::dontSendNotification);
        if (autoRefresh && dirty)
            refreshValidation();
    }

    void ValidationPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(40, 46, 56));
        g.drawRect(getLocalBounds(), 1);
    }

    void ValidationPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);
        auto top = area.removeFromTop(20);
        titleLabel.setBounds(top.removeFromLeft(120));
        summaryLabel.setBounds(top);

        area.removeFromTop(4);
        auto controls = area.removeFromTop(24);
        runButton.setBounds(controls.removeFromLeft(130));
        controls.removeFromLeft(8);
        autoRefreshToggle.setBounds(controls.removeFromLeft(70));

        area.removeFromTop(6);
        listBox.setBounds(area);
    }

    int ValidationPanel::getNumRows()
    {
        return static_cast<int>(issues.size());
    }

    void ValidationPanel::paintListBoxItem(int rowNumber,
                                           juce::Graphics& g,
                                           int width,
                                           int height,
                                           bool rowIsSelected)
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(issues.size()))
            return;

        const auto& issue = issues[static_cast<size_t>(rowNumber)];
        const auto bounds = juce::Rectangle<int>(0, 0, width, height);

        const auto baseFill = rowIsSelected ? juce::Colour::fromRGB(49, 84, 142)
                                            : juce::Colour::fromRGB(24, 30, 40);
        g.setColour(baseFill.withAlpha(rowIsSelected ? 0.84f : 0.62f));
        g.fillRect(bounds);

        g.setColour(juce::Colour::fromRGB(44, 52, 66));
        g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

        auto textArea = bounds.reduced(8, 4);
        auto header = textArea.removeFromTop(14);
        const auto severityColour = colorForSeverity(issue.severity);

        g.setColour(severityColour);
        g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(header.getX()),
                                                      static_cast<float>(header.getY() + 1),
                                                      50.0f,
                                                      12.0f),
                               3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(labelForSeverity(issue.severity),
                   juce::Rectangle<int>(header.getX(), header.getY() + 1, 50, 12),
                   juce::Justification::centred,
                   true);

        g.setColour(juce::Colour::fromRGB(194, 202, 216));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(issue.title,
                   juce::Rectangle<int>(header.getX() + 56, header.getY(), header.getWidth() - 56, header.getHeight()),
                   juce::Justification::centredLeft,
                   true);

        g.setColour(juce::Colour::fromRGB(162, 172, 188));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(issue.message, textArea, juce::Justification::centredLeft, true);
    }

    void ValidationPanel::rebuildIssues()
    {
        issues.clear();

        const auto& snapshot = document.snapshot();
        const auto& editorState = document.editorState();
        const auto sceneResult = Core::SceneValidator::validateScene(snapshot, &editorState);
        if (sceneResult.failed())
            pushIssue(IssueSeverity::error, "Scene validation failed", sceneResult.getErrorMessage());
        else
            pushIssue(IssueSeverity::info, "Scene validation passed", "Core model invariants are valid.");

        if (snapshot.widgets.empty())
            pushIssue(IssueSeverity::warning, "Empty document", "No widgets in current document.");

        int hiddenLayerCount = 0;
        int lockedLayerCount = 0;
        for (const auto& layer : snapshot.layers)
        {
            hiddenLayerCount += layer.visible ? 0 : 1;
            lockedLayerCount += layer.locked ? 1 : 0;
            if (layer.memberWidgetIds.empty() && layer.memberGroupIds.empty())
            {
                pushIssue(IssueSeverity::info,
                          "Empty layer: " + layer.name,
                          "Layer has no widget/group members.");
            }
        }

        if (!snapshot.layers.empty() && hiddenLayerCount == static_cast<int>(snapshot.layers.size()))
            pushIssue(IssueSeverity::warning, "All layers hidden", "Canvas may render as empty.");
        if (!snapshot.layers.empty() && lockedLayerCount == static_cast<int>(snapshot.layers.size()))
            pushIssue(IssueSeverity::warning, "All layers locked", "Editing interaction will be blocked.");

        std::unordered_set<juce::String> seenLayerNames;
        for (const auto& layer : snapshot.layers)
        {
            const auto normalized = layer.name.trim().toLowerCase();
            if (normalized.isEmpty())
                continue;
            if (!seenLayerNames.insert(normalized).second)
            {
                pushIssue(IssueSeverity::warning,
                          "Duplicate layer name",
                          "Layer name '" + layer.name + "' is duplicated.");
            }
        }

        for (const auto& widget : snapshot.widgets)
        {
            if (registry.find(widget.type) == nullptr)
            {
                pushIssue(IssueSeverity::warning,
                          "Unknown widget descriptor",
                          "Widget id=" + juce::String(widget.id) + " has no descriptor in registry.");
            }
        }

        const auto runtimeIssues = Core::SceneValidator::validateRuntimeBindings(snapshot);
        for (const auto& issue : runtimeIssues)
        {
            const auto severity = issue.severity == Core::SceneValidator::RuntimeBindingIssueSeverity::error
                                      ? IssueSeverity::error
                                      : IssueSeverity::warning;
            pushIssue(severity, issue.title, issue.message);
        }

        std::unordered_map<WidgetId, const WidgetModel*> widgetById;
        widgetById.reserve(snapshot.widgets.size());
        for (const auto& widget : snapshot.widgets)
            widgetById.emplace(widget.id, &widget);

        for (const auto& binding : snapshot.runtimeBindings)
        {
            const auto widgetIt = widgetById.find(binding.sourceWidgetId);
            if (widgetIt == widgetById.end())
                continue;

            const auto* descriptor = registry.find(widgetIt->second->type);
            if (descriptor == nullptr)
                continue;

            const auto supported = std::any_of(descriptor->runtimeEvents.begin(),
                                               descriptor->runtimeEvents.end(),
                                               [&binding](const Widgets::RuntimeEventSpec& eventSpec)
                                               {
                                                   return eventSpec.key == binding.eventKey;
                                               });
            if (!supported)
            {
                pushIssue(IssueSeverity::warning,
                          "Unsupported event key",
                          "Binding id=" + juce::String(binding.id) + " event '" + binding.eventKey
                              + "' is not supported by widget type '" + descriptor->typeKey + "'.");
            }
        }

        const auto projectRoot = resolveProjectRootDirectory();
        constexpr juce::int64 kLargeAssetWarnBytes = 5 * 1024 * 1024;
        constexpr int kLargeImageDimension = 4096;

        for (const auto& asset : snapshot.assets)
        {
            const auto assetLabel = asset.name.trim().isNotEmpty() ? asset.name.trim() : asset.refKey.trim();
            const auto displayLabel = assetLabel.isNotEmpty()
                                        ? assetLabel
                                        : ("Asset #" + juce::String(asset.id));

            if (asset.kind == AssetKind::colorPreset)
                continue;

            const auto normalizedPath = asset.relativePath.trim();
            if (normalizedPath.isEmpty())
            {
                pushIssue(IssueSeverity::warning,
                          "Asset path missing",
                          displayLabel + " has an empty relative path.");
                continue;
            }

            const auto sourceFile = resolveInputFilePath(normalizedPath, projectRoot);
            if (!sourceFile.existsAsFile())
            {
                pushIssue(IssueSeverity::warning,
                          "Asset file missing",
                          displayLabel + " path not found: " + normalizedPath);
                continue;
            }

            const auto fileSize = sourceFile.getSize();
            if (fileSize > kLargeAssetWarnBytes)
            {
                pushIssue(IssueSeverity::warning,
                          "Large asset file",
                          displayLabel + " is " + formatByteSize(fileSize) + " (" + sourceFile.getFileName() + ").");
            }

            if (asset.kind == AssetKind::image)
            {
                const auto image = juce::ImageFileFormat::loadFrom(sourceFile);
                if (!image.isValid())
                {
                    pushIssue(IssueSeverity::warning,
                              "Image decode failed",
                              displayLabel + " could not be decoded as image (" + sourceFile.getFileName() + ").");
                }
                else if (image.getWidth() > kLargeImageDimension || image.getHeight() > kLargeImageDimension)
                {
                    pushIssue(IssueSeverity::warning,
                              "Large image resolution",
                              displayLabel + " resolution is " + juce::String(image.getWidth()) + "x"
                                  + juce::String(image.getHeight()) + " (>" + juce::String(kLargeImageDimension) + ").");
                }
            }

            const auto expectedMime = inferMimeTypeFromFile(sourceFile);
            const auto recordedMime = asset.mimeType.trim().toLowerCase();
            if (expectedMime.isNotEmpty() && recordedMime.isNotEmpty() && !expectedMime.equalsIgnoreCase(recordedMime))
            {
                pushIssue(IssueSeverity::warning,
                          "Asset MIME mismatch",
                          displayLabel + " MIME is '" + asset.mimeType + "', expected '" + expectedMime + "'.");
            }
        }
    }

    void ValidationPanel::pushIssue(IssueSeverity severity, const juce::String& title, const juce::String& message)
    {
        Issue issue;
        issue.severity = severity;
        issue.title = title;
        issue.message = message;
        issues.push_back(std::move(issue));
    }

    juce::Colour ValidationPanel::colorForSeverity(IssueSeverity severity)
    {
        switch (severity)
        {
            case IssueSeverity::info: return juce::Colour::fromRGB(86, 168, 255);
            case IssueSeverity::warning: return juce::Colour::fromRGB(255, 198, 92);
            case IssueSeverity::error: return juce::Colour::fromRGB(255, 112, 112);
        }

        return juce::Colour::fromRGB(128, 136, 150);
    }

    juce::String ValidationPanel::labelForSeverity(IssueSeverity severity)
    {
        switch (severity)
        {
            case IssueSeverity::info: return "INFO";
            case IssueSeverity::warning: return "WARN";
            case IssueSeverity::error: return "ERROR";
        }

        return "INFO";
    }
}
