#include "Gyeol/Widgets/WidgetLibraryExchange.h"

#include <algorithm>

namespace Gyeol::Widgets
{
    namespace
    {
        juce::String normalizeCategory(const juce::String& category)
        {
            const auto trimmed = category.trim();
            return trimmed.isNotEmpty() ? trimmed : juce::String("Other");
        }

        juce::String buildDefaultsSummary(const WidgetDescriptor& descriptor)
        {
            if (descriptor.defaultProperties.size() <= 0)
                return "No defaults";

            juce::StringArray keys;
            for (int i = 0; i < descriptor.defaultProperties.size(); ++i)
                keys.add(descriptor.defaultProperties.getName(i).toString());
            keys.sort(true);

            juce::StringArray previewKeys;
            constexpr int kPreviewCount = 3;
            for (int i = 0; i < std::min(keys.size(), kPreviewCount); ++i)
                previewKeys.add(keys[i]);

            juce::String summary = juce::String(descriptor.defaultProperties.size()) + " defaults";
            if (!previewKeys.isEmpty())
                summary += " (" + previewKeys.joinIntoString(", ") + ")";
            return summary;
        }

        juce::var toJson(const WidgetLibraryManifestModel& manifest)
        {
            auto root = std::make_unique<juce::DynamicObject>();

            auto version = std::make_unique<juce::DynamicObject>();
            version->setProperty("major", manifest.version.major);
            version->setProperty("minor", manifest.version.minor);
            version->setProperty("patch", manifest.version.patch);
            root->setProperty("version", juce::var(version.release()));

            juce::Array<juce::var> widgetsArray;
            widgetsArray.ensureStorageAllocated(static_cast<int>(manifest.widgets.size()));
            for (const auto& widget : manifest.widgets)
            {
                auto object = std::make_unique<juce::DynamicObject>();
                object->setProperty("typeKey", widget.typeKey);
                object->setProperty("displayName", widget.displayName);
                object->setProperty("category", widget.category);
                object->setProperty("iconKey", widget.iconKey);
                object->setProperty("defaultsSummary", widget.defaultsSummary);

                juce::Array<juce::var> tagsArray;
                tagsArray.ensureStorageAllocated(widget.tags.size());
                for (const auto& tag : widget.tags)
                    tagsArray.add(tag);
                object->setProperty("tags", juce::var(tagsArray));

                widgetsArray.add(juce::var(object.release()));
            }
            root->setProperty("widgets", juce::var(widgetsArray));

            juce::Array<juce::var> assetsArray;
            assetsArray.ensureStorageAllocated(static_cast<int>(manifest.assets.size()));
            for (const auto& asset : manifest.assets)
            {
                auto object = std::make_unique<juce::DynamicObject>();
                object->setProperty("assetId", asset.assetId);
                object->setProperty("path", asset.relativePath);
                object->setProperty("mime", asset.mime);
                assetsArray.add(juce::var(object.release()));
            }
            root->setProperty("assets", juce::var(assetsArray));

            return juce::var(root.release());
        }

        bool tryReadInt(const juce::NamedValueSet& props, const juce::Identifier& key, int& outValue)
        {
            if (!props.contains(key))
                return false;

            const auto& value = props[key];
            if (!isNumericVar(value))
                return false;

            outValue = static_cast<int>(value);
            return true;
        }
    }

    juce::Result serializeLibraryManifest(const WidgetRegistry& registry, const juce::File& outputDirectory)
    {
        if (!outputDirectory.exists() && !outputDirectory.createDirectory())
            return juce::Result::fail("Failed to create output directory: " + outputDirectory.getFullPathName());

        WidgetLibraryManifestModel manifest;
        manifest.widgets.reserve(registry.all().size());

        for (const auto* descriptor : registry.listDescriptors())
        {
            if (descriptor == nullptr)
                continue;

            WidgetLibraryManifestWidget widget;
            widget.typeKey = descriptor->typeKey;
            widget.displayName = descriptor->displayName;
            widget.category = normalizeCategory(descriptor->category);
            widget.tags = descriptor->tags;
            widget.iconKey = descriptor->iconKey;
            widget.defaultsSummary = buildDefaultsSummary(*descriptor);
            manifest.widgets.push_back(std::move(widget));
        }

        std::sort(manifest.widgets.begin(),
                  manifest.widgets.end(),
                  [](const WidgetLibraryManifestWidget& lhs, const WidgetLibraryManifestWidget& rhs)
                  {
                      if (lhs.category != rhs.category)
                          return lhs.category < rhs.category;
                      return lhs.displayName < rhs.displayName;
                  });

        const auto json = juce::JSON::toString(toJson(manifest), true);
        const auto manifestFile = outputDirectory.getChildFile("manifest.json");
        if (!manifestFile.replaceWithText(json, false, false, "\n"))
            return juce::Result::fail("Failed to write manifest: " + manifestFile.getFullPathName());

        return juce::Result::ok();
    }

    juce::Result loadLibraryManifest(const juce::File& manifestFile, WidgetLibraryManifestModel& outModel)
    {
        if (!manifestFile.existsAsFile())
            return juce::Result::fail("Manifest file does not exist: " + manifestFile.getFullPathName());

        const auto text = manifestFile.loadFileAsString();
        const auto parsed = juce::JSON::parse(text);
        const auto* rootObject = parsed.getDynamicObject();
        if (rootObject == nullptr)
            return juce::Result::fail("Manifest root must be JSON object");

        WidgetLibraryManifestModel parsedModel;
        const auto& rootProps = rootObject->getProperties();

        if (!rootProps.contains("version"))
            return juce::Result::fail("Manifest.version is missing");
        const auto* versionObject = rootProps["version"].getDynamicObject();
        if (versionObject == nullptr)
            return juce::Result::fail("Manifest.version must be object");

        const auto& versionProps = versionObject->getProperties();
        if (!tryReadInt(versionProps, "major", parsedModel.version.major)
            || !tryReadInt(versionProps, "minor", parsedModel.version.minor)
            || !tryReadInt(versionProps, "patch", parsedModel.version.patch))
        {
            return juce::Result::fail("Manifest.version must include numeric major/minor/patch");
        }

        if (!rootProps.contains("widgets") || !rootProps["widgets"].isArray())
            return juce::Result::fail("Manifest.widgets must be array");

        if (const auto* widgetsArray = rootProps["widgets"].getArray())
        {
            parsedModel.widgets.reserve(static_cast<size_t>(widgetsArray->size()));
            for (const auto& widgetVar : *widgetsArray)
            {
                const auto* widgetObject = widgetVar.getDynamicObject();
                if (widgetObject == nullptr)
                    return juce::Result::fail("Manifest.widgets entry must be object");

                const auto& widgetProps = widgetObject->getProperties();
                if (!widgetProps.contains("typeKey") || !widgetProps["typeKey"].isString())
                    return juce::Result::fail("Manifest.widgets[].typeKey must be string");
                if (!widgetProps.contains("displayName") || !widgetProps["displayName"].isString())
                    return juce::Result::fail("Manifest.widgets[].displayName must be string");

                WidgetLibraryManifestWidget widget;
                widget.typeKey = widgetProps["typeKey"].toString();
                widget.displayName = widgetProps["displayName"].toString();
                widget.category = normalizeCategory(widgetProps.contains("category")
                                                      ? widgetProps["category"].toString()
                                                      : juce::String());
                widget.iconKey = widgetProps.contains("iconKey") ? widgetProps["iconKey"].toString() : juce::String();
                widget.defaultsSummary = widgetProps.contains("defaultsSummary")
                                           ? widgetProps["defaultsSummary"].toString()
                                           : juce::String();

                if (widgetProps.contains("tags") && widgetProps["tags"].isArray())
                {
                    if (const auto* tagsArray = widgetProps["tags"].getArray())
                    {
                        for (const auto& tagVar : *tagsArray)
                        {
                            if (tagVar.isString())
                                widget.tags.add(tagVar.toString());
                        }
                    }
                }

                parsedModel.widgets.push_back(std::move(widget));
            }
        }

        if (rootProps.contains("assets"))
        {
            if (!rootProps["assets"].isArray())
                return juce::Result::fail("Manifest.assets must be array");

            if (const auto* assetsArray = rootProps["assets"].getArray())
            {
                parsedModel.assets.reserve(static_cast<size_t>(assetsArray->size()));
                for (const auto& assetVar : *assetsArray)
                {
                    const auto* assetObject = assetVar.getDynamicObject();
                    if (assetObject == nullptr)
                        return juce::Result::fail("Manifest.assets entry must be object");

                    const auto& assetProps = assetObject->getProperties();
                    WidgetLibraryManifestAsset asset;
                    asset.assetId = assetProps.contains("assetId") ? assetProps["assetId"].toString() : juce::String();
                    asset.relativePath = assetProps.contains("path") ? assetProps["path"].toString() : juce::String();
                    asset.mime = assetProps.contains("mime") ? assetProps["mime"].toString() : juce::String();
                    parsedModel.assets.push_back(std::move(asset));
                }
            }
        }

        outModel = std::move(parsedModel);
        return juce::Result::ok();
    }
}

