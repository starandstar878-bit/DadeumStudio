#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/ButtonWidget.h"
#include "Gyeol/Widgets/KnobWidget.h"
#include "Gyeol/Widgets/LabelWidget.h"
#include "Gyeol/Widgets/MeterWidget.h"
#include "Gyeol/Widgets/SliderWidget.h"
#include "Gyeol/Widgets/ToggleWidget.h"
#include "Gyeol/Widgets/ComboBoxWidget.h"
#include "Gyeol/Widgets/TextInputWidget.h"
#include "Gyeol/Widgets/WidgetSDK.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace Gyeol::Widgets
{
    struct ExportWidgetMapping
    {
        WidgetType type = WidgetType::button;
        juce::String typeKey;
        juce::String exportTargetType;
    };

    struct LibraryFilter
    {
        juce::String query;
        juce::String category;
        bool includeFavoritesOnly = false;
        juce::StringArray favoriteTypeKeys;
    };

    class WidgetRegistry
    {
    public:
        bool registerWidget(WidgetDescriptor descriptor)
        {
            const auto normalizeStringArray = [](juce::StringArray& values)
            {
                for (int i = values.size() - 1; i >= 0; --i)
                {
                    const auto trimmed = values[i].trim();
                    if (trimmed.isEmpty())
                    {
                        values.remove(i);
                        continue;
                    }

                    values.set(i, trimmed);
                }
                values.removeDuplicates(false);
            };

            const auto defaultValueForPropertyKind = [](const WidgetPropertySpec& spec) -> juce::var
            {
                switch (spec.kind)
                {
                    case WidgetPropertyKind::text:
                        return juce::var(juce::String());
                    case WidgetPropertyKind::integer:
                        return juce::var(0);
                    case WidgetPropertyKind::number:
                        return juce::var(0.0);
                    case WidgetPropertyKind::boolean:
                        return juce::var(false);
                    case WidgetPropertyKind::enumChoice:
                        if (!spec.enumOptions.empty())
                            return juce::var(spec.enumOptions.front().value);
                        return juce::var(juce::String());
                    case WidgetPropertyKind::color:
                        return juce::var(juce::String("#FFFFFFFF"));
                    case WidgetPropertyKind::vec2:
                    {
                        auto object = std::make_unique<juce::DynamicObject>();
                        object->setProperty("x", 0.0);
                        object->setProperty("y", 0.0);
                        return juce::var(object.release());
                    }
                    case WidgetPropertyKind::rect:
                    {
                        auto object = std::make_unique<juce::DynamicObject>();
                        object->setProperty("x", 0.0);
                        object->setProperty("y", 0.0);
                        object->setProperty("w", 0.0);
                        object->setProperty("h", 0.0);
                        return juce::var(object.release());
                    }
                    case WidgetPropertyKind::assetRef:
                        return juce::var(juce::String());
                }

                return juce::var(juce::String());
            };

            const auto ensurePayloadSchemaRequiredArray = [](juce::var& payloadSchema)
            {
                if (payloadSchema.isVoid())
                {
                    payloadSchema = emptyRequiredPayloadSchema();
                    return;
                }

                auto* object = payloadSchema.getDynamicObject();
                if (object == nullptr)
                {
                    payloadSchema = emptyRequiredPayloadSchema();
                    return;
                }

                auto& props = object->getProperties();
                if (!props.contains("required") || !props["required"].isArray())
                {
                    juce::Array<juce::var> required;
                    object->setProperty("required", juce::var(required));
                }
            };

            const auto inferSdkMajor = [](const juce::String& sdkVersion) -> juce::String
            {
                const auto major = sdkVersion.trim().upToFirstOccurrenceOf(".", false, false);
                if (major.containsOnly("0123456789") && major.isNotEmpty())
                    return major;
                return "1";
            };

            const auto makeAbiHash = [](const WidgetDescriptor& descriptor) -> juce::String
            {
                juce::String seed;
                seed << descriptor.typeKey << "|";
                seed << descriptor.pluginId << "|";
                seed << descriptor.pluginVersion << "|";
                seed << descriptor.abiVersion << "|";
                seed << descriptor.schemaVersion << "|";
                seed << descriptor.manifestVersion << "|";

                for (const auto& spec : descriptor.propertySpecs)
                {
                    seed << spec.key.toString() << ":";
                    seed << widgetPropertyKindToKey(spec.kind) << ";";
                }

                for (const auto& eventSpec : descriptor.runtimeEvents)
                    seed << eventSpec.key << ";";

                constexpr std::uint64_t kOffset = 14695981039346656037ull;
                constexpr std::uint64_t kPrime = 1099511628211ull;
                std::uint64_t hash = kOffset;

                const auto utf8 = seed.toRawUTF8();
                for (size_t i = 0; utf8[i] != 0; ++i)
                {
                    hash ^= static_cast<std::uint8_t>(utf8[i]);
                    hash *= kPrime;
                }

                juce::String hex;
                hex.preallocateBytes(16);
                for (int shift = 60; shift >= 0; shift -= 4)
                {
                    const auto nibble = static_cast<int>((hash >> shift) & 0x0full);
                    const auto digit = static_cast<char>(nibble < 10
                                                             ? ('0' + nibble)
                                                             : ('a' + (nibble - 10)));
                    hex << juce::String::fromUTF8(&digit, 1);
                }

                return hex;
            };

            const auto warnRecommendedMissing = [](const WidgetDescriptor& descriptor)
            {
#if JUCE_DEBUG
                if (descriptor.capabilities.isEmpty())
                {
                    DBG("[Gyeol][WidgetRegistry] Recommended field missing: capabilities (" + descriptor.typeKey + ")");
                }

                if (descriptor.a11yRole.isEmpty() || descriptor.a11yLabelKey.isEmpty())
                {
                    DBG("[Gyeol][WidgetRegistry] Recommended accessibility fields missing: a11yRole/a11yLabelKey (" + descriptor.typeKey + ")");
                }

                if (descriptor.tags.isEmpty() || descriptor.iconKey.isEmpty())
                {
                    DBG("[Gyeol][WidgetRegistry] Recommended discoverability fields missing: tags/iconKey (" + descriptor.typeKey + ")");
                }
#endif
            };

            descriptor.typeKey = descriptor.typeKey.trim();
            descriptor.displayName = descriptor.displayName.trim();
            descriptor.category = descriptor.category.trim().isNotEmpty() ? descriptor.category.trim() : juce::String("Other");

            descriptor.schemaVersion = descriptor.schemaVersion.trim().isNotEmpty() ? descriptor.schemaVersion.trim() : juce::String("2.0.0");
            descriptor.manifestVersion = descriptor.manifestVersion.trim().isNotEmpty() ? descriptor.manifestVersion.trim() : juce::String("1.0.0");
            descriptor.pluginVersion = descriptor.pluginVersion.trim().isNotEmpty() ? descriptor.pluginVersion.trim() : juce::String("1.0.0");
            descriptor.abiVersion = descriptor.abiVersion.trim().isNotEmpty() ? descriptor.abiVersion.trim() : juce::String("1");
            descriptor.sdkMinVersion = descriptor.sdkMinVersion.trim().isNotEmpty() ? descriptor.sdkMinVersion.trim() : juce::String("1.0.0");
            descriptor.releaseChannel = descriptor.releaseChannel.trim().isNotEmpty() ? descriptor.releaseChannel.trim() : juce::String("stable");
            descriptor.repaintPolicy = descriptor.repaintPolicy.trim().isNotEmpty() ? descriptor.repaintPolicy.trim() : juce::String("onDemand");
            descriptor.threadingModel = descriptor.threadingModel.trim().isNotEmpty() ? descriptor.threadingModel.trim() : juce::String("main-thread");
            descriptor.sandboxLevel = descriptor.sandboxLevel.trim().isNotEmpty() ? descriptor.sandboxLevel.trim() : juce::String("strict");

            if (descriptor.widgetTypeVersion <= 0)
                descriptor.widgetTypeVersion = 1;

            if (descriptor.pluginId.trim().isEmpty() && descriptor.typeKey.isNotEmpty())
                descriptor.pluginId = "com.dadeum.gyeol.builtin." + descriptor.typeKey;
            else
                descriptor.pluginId = descriptor.pluginId.trim();

            if (descriptor.vendor.trim().isEmpty())
                descriptor.vendor = "Dadeum";
            else
                descriptor.vendor = descriptor.vendor.trim();

            descriptor.pluginBinaryPath = descriptor.pluginBinaryPath.trim();

            normalizeStringArray(descriptor.tags);
            normalizeStringArray(descriptor.supportedHostVersions);
            normalizeStringArray(descriptor.platformTargets);
            normalizeStringArray(descriptor.architectureTargets);
            normalizeStringArray(descriptor.capabilities);
            normalizeStringArray(descriptor.supportedActions);
            normalizeStringArray(descriptor.propertyBindings);
            normalizeStringArray(descriptor.stateInputs);
            normalizeStringArray(descriptor.stateOutputs);
            normalizeStringArray(descriptor.telemetryTags);
            normalizeStringArray(descriptor.persistedKeys);
            normalizeStringArray(descriptor.permissions);
            normalizeStringArray(descriptor.fileAccess);
            normalizeStringArray(descriptor.networkAccess);
            normalizeStringArray(descriptor.midiAccess);
            normalizeStringArray(descriptor.scriptAccess);
            normalizeStringArray(descriptor.requiredJuceModules);
            normalizeStringArray(descriptor.requiredHeaders);
            normalizeStringArray(descriptor.requiredLibraries);
            normalizeStringArray(descriptor.requiredCompileDefinitions);
            normalizeStringArray(descriptor.requiredLinkOptions);

            if (descriptor.capabilities.isEmpty())
            {
                if (descriptor.dropOptions || descriptor.applyDrop)
                    descriptor.capabilities.add("acceptsAssetDrop");
                if (!descriptor.runtimeEvents.empty())
                    descriptor.capabilities.add("emitsRuntimeEvents");
                if (descriptor.exportCodegen)
                    descriptor.capabilities.add("customCodegen");
                normalizeStringArray(descriptor.capabilities);
            }

            if (descriptor.sdkMaxVersion.trim().isEmpty())
                descriptor.sdkMaxVersion = inferSdkMajor(descriptor.sdkMinVersion) + ".x";
            else
                descriptor.sdkMaxVersion = descriptor.sdkMaxVersion.trim();

            if (descriptor.supportedHostVersions.isEmpty())
                descriptor.supportedHostVersions.add(descriptor.sdkMinVersion);

            if (descriptor.platformTargets.isEmpty())
            {
#if JUCE_WINDOWS
                descriptor.platformTargets.add("windows");
#elif JUCE_MAC
                descriptor.platformTargets.add("macos");
#elif JUCE_LINUX
                descriptor.platformTargets.add("linux");
#else
                descriptor.platformTargets.add("unknown");
#endif
            }

            if (descriptor.architectureTargets.isEmpty())
            {
#if defined(_M_ARM64) || defined(__aarch64__)
                descriptor.architectureTargets.add("arm64");
#elif defined(_M_X64) || defined(__x86_64__)
                descriptor.architectureTargets.add("x64");
#elif defined(_M_IX86) || defined(__i386__)
                descriptor.architectureTargets.add("x86");
#else
                descriptor.architectureTargets.add("unknown");
#endif
            }

            if (descriptor.requiredJuceModules.isEmpty())
                descriptor.requiredJuceModules.add("juce_gui_basics");

            descriptor.codegenApiVersion = descriptor.codegenApiVersion.trim().isNotEmpty()
                                               ? descriptor.codegenApiVersion.trim()
                                               : juce::String("2.1");

            if (descriptor.abiHash.trim().isEmpty())
                descriptor.abiHash = makeAbiHash(descriptor);
            else
                descriptor.abiHash = descriptor.abiHash.trim();

            if (descriptor.testId.trim().isEmpty())
                descriptor.testId = descriptor.pluginId + "." + descriptor.typeKey;
            else
                descriptor.testId = descriptor.testId.trim();

            if (descriptor.typeKey.isEmpty() || descriptor.displayName.isEmpty())
                return false;
            if (descriptor.schemaVersion.isEmpty() || descriptor.manifestVersion.isEmpty())
                return false;
            if (descriptor.pluginId.isEmpty() || descriptor.pluginVersion.isEmpty())
                return false;
            if (descriptor.sdkMinVersion.isEmpty() || descriptor.abiVersion.isEmpty() || descriptor.abiHash.isEmpty())
                return false;
            if (descriptor.defaultBounds.getWidth() <= 0.0f || descriptor.defaultBounds.getHeight() <= 0.0f)
                return false;
            if (descriptor.minSize.x <= 0.0f || descriptor.minSize.y <= 0.0f)
                return false;
            if (!descriptor.painter)
                return false;
            if (validatePropertyBag(descriptor.defaultProperties).failed())
                return false;

            warnRecommendedMissing(descriptor);

            std::vector<juce::String> runtimeEventKeys;
            runtimeEventKeys.reserve(descriptor.runtimeEvents.size());
            for (auto& eventSpec : descriptor.runtimeEvents)
            {
                eventSpec.key = eventSpec.key.trim();
                eventSpec.displayLabel = eventSpec.displayLabel.trim();
                eventSpec.description = eventSpec.description.trim();
                eventSpec.reliability = eventSpec.reliability.trim().isNotEmpty()
                                            ? eventSpec.reliability.trim()
                                            : juce::String("bestEffort");
                eventSpec.channel = eventSpec.channel.trim().isNotEmpty()
                                        ? eventSpec.channel.trim()
                                        : juce::String("ui");
                ensurePayloadSchemaRequiredArray(eventSpec.payloadSchema);

                if (eventSpec.key.isEmpty())
                    return false;
                if (eventSpec.throttleMs.has_value() && *eventSpec.throttleMs < 0)
                    return false;
                if (eventSpec.debounceMs.has_value() && *eventSpec.debounceMs < 0)
                    return false;
                if (std::find(runtimeEventKeys.begin(), runtimeEventKeys.end(), eventSpec.key) != runtimeEventKeys.end())
                    return false;
                runtimeEventKeys.push_back(eventSpec.key);

                if (eventSpec.displayLabel.isEmpty())
                    eventSpec.displayLabel = eventSpec.key;
            }

            const auto isNumericFinite = [](const juce::var& value) noexcept
            {
                return isNumericVar(value) && std::isfinite(static_cast<double>(value));
            };

            const auto isIntegerLike = [](const juce::var& value) noexcept
            {
                if (value.isInt() || value.isInt64())
                    return true;
                if (!value.isDouble())
                    return false;

                const auto numeric = static_cast<double>(value);
                return std::isfinite(numeric) && std::abs(numeric - std::round(numeric)) <= 0.000001;
            };

            const auto validateVec2Var = [](const juce::var& value) noexcept
            {
                const auto* object = value.getDynamicObject();
                if (object == nullptr)
                    return false;

                const auto& props = object->getProperties();
                if (props.size() != 2)
                    return false;
                if (!props.contains("x") || !props.contains("y"))
                    return false;
                return isNumericVar(props["x"]) && isNumericVar(props["y"]);
            };

            const auto validateColorVar = [&](const juce::var& value, const WidgetPropertySpec& spec) noexcept
            {
                if (spec.colorStorage == ColorStorage::token)
                    return value.isString();

                if (spec.colorStorage == ColorStorage::argbInt)
                    return isIntegerLike(value);

                if (spec.colorStorage == ColorStorage::hexString)
                {
                    if (!value.isString())
                        return false;

                    const auto text = value.toString().trim();
                    if (!text.startsWithChar('#'))
                        return false;
                    const auto body = text.substring(1);
                    const auto len = body.length();
                    if (len != 6 && len != 8)
                        return false;
                    return body.containsOnly("0123456789abcdefABCDEF");
                }

                const auto* object = value.getDynamicObject();
                if (object == nullptr)
                    return false;

                const auto& props = object->getProperties();
                if (spec.colorStorage == ColorStorage::rgbaObject255 || spec.colorStorage == ColorStorage::rgbaObject01)
                {
                    for (const auto channel : { juce::Identifier("r"), juce::Identifier("g"), juce::Identifier("b") })
                    {
                        if (!props.contains(channel) || !isNumericVar(props[channel]))
                            return false;
                    }

                    const auto hasAlpha = props.contains("a");
                    if (spec.colorAllowAlpha && (!hasAlpha || !isNumericVar(props["a"])))
                        return false;
                    if (!spec.colorAllowAlpha && hasAlpha)
                        return false;

                    const auto minV = spec.colorStorage == ColorStorage::rgbaObject01 ? 0.0 : 0.0;
                    const auto maxV = spec.colorStorage == ColorStorage::rgbaObject01 ? 1.0 : 255.0;

                    for (const auto channel : { juce::Identifier("r"), juce::Identifier("g"), juce::Identifier("b") })
                    {
                        const auto v = static_cast<double>(props[channel]);
                        if (!std::isfinite(v) || (!spec.colorAllowHdr && (v < minV || v > maxV)))
                            return false;
                    }

                    if (hasAlpha)
                    {
                        const auto alpha = static_cast<double>(props["a"]);
                        if (!std::isfinite(alpha) || alpha < minV || alpha > maxV)
                            return false;
                    }

                    return true;
                }

                if (spec.colorStorage == ColorStorage::hslaObject)
                {
                    for (const auto channel : { juce::Identifier("h"), juce::Identifier("s"), juce::Identifier("l"), juce::Identifier("a") })
                    {
                        if (!props.contains(channel) || !isNumericVar(props[channel]))
                            return false;
                    }

                    const auto h = static_cast<double>(props["h"]);
                    const auto s = static_cast<double>(props["s"]);
                    const auto l = static_cast<double>(props["l"]);
                    const auto a = static_cast<double>(props["a"]);
                    if (!std::isfinite(h) || !std::isfinite(s) || !std::isfinite(l) || !std::isfinite(a))
                        return false;
                    if (!spec.colorAllowHdr && (h < 0.0 || h > 360.0 || s < 0.0 || s > 1.0 || l < 0.0 || l > 1.0))
                        return false;
                    if (a < 0.0 || a > 1.0)
                        return false;
                    return true;
                }

                return false;
            };

            const auto isUiHintCompatible = [](WidgetPropertyKind kind, WidgetPropertyUiHint hint) noexcept
            {
                if (hint == WidgetPropertyUiHint::autoHint)
                    return true;

                switch (kind)
                {
                    case WidgetPropertyKind::text:
                        return hint == WidgetPropertyUiHint::lineEdit
                            || hint == WidgetPropertyUiHint::multiLine
                            || hint == WidgetPropertyUiHint::dropdown;
                    case WidgetPropertyKind::integer:
                    case WidgetPropertyKind::number:
                        return hint == WidgetPropertyUiHint::lineEdit
                            || hint == WidgetPropertyUiHint::spinBox
                            || hint == WidgetPropertyUiHint::slider;
                    case WidgetPropertyKind::boolean:
                        return hint == WidgetPropertyUiHint::toggle
                            || hint == WidgetPropertyUiHint::dropdown;
                    case WidgetPropertyKind::enumChoice:
                        return hint == WidgetPropertyUiHint::dropdown
                            || hint == WidgetPropertyUiHint::segmented;
                    case WidgetPropertyKind::color:
                        return hint == WidgetPropertyUiHint::colorPicker
                            || hint == WidgetPropertyUiHint::lineEdit;
                    case WidgetPropertyKind::vec2:
                        return hint == WidgetPropertyUiHint::vec2Editor
                            || hint == WidgetPropertyUiHint::lineEdit;
                    case WidgetPropertyKind::rect:
                        return hint == WidgetPropertyUiHint::rectEditor
                            || hint == WidgetPropertyUiHint::lineEdit;
                    case WidgetPropertyKind::assetRef:
                        return hint == WidgetPropertyUiHint::assetPicker
                            || hint == WidgetPropertyUiHint::lineEdit
                            || hint == WidgetPropertyUiHint::dropdown;
                }

                return false;
            };

            std::vector<juce::Identifier> seenSpecKeys;
            seenSpecKeys.reserve(descriptor.propertySpecs.size());
            for (auto& spec : descriptor.propertySpecs)
            {
                spec.label = spec.label.trim();
                spec.group = spec.group.trim().isNotEmpty() ? spec.group.trim() : juce::String("Widget");
                spec.hint = spec.hint.trim();
                spec.unit = spec.unit.trim();
                spec.displayFormat = spec.displayFormat.trim();
                spec.valueCurve = spec.valueCurve.trim();
                spec.regex = spec.regex.trim();
                spec.localeKey = spec.localeKey.trim();
                normalizeStringArray(spec.acceptedMimeTypes);
                normalizeStringArray(spec.preloadAssets);

                if (spec.key.toString().isEmpty() || spec.label.isEmpty())
                    return false;
                if (std::find(seenSpecKeys.begin(), seenSpecKeys.end(), spec.key) != seenSpecKeys.end())
                    return false;
                seenSpecKeys.push_back(spec.key);
                if (!isUiHintCompatible(spec.kind, spec.uiHint))
                    return false;
                if (spec.decimals < 0)
                    return false;
                if (spec.dependsOnKey.has_value() && spec.dependsOnKey->toString().isEmpty())
                    return false;
                if (spec.minLength.has_value() && *spec.minLength < 0)
                    return false;
                if (spec.maxLength.has_value() && *spec.maxLength < 0)
                    return false;
                if (spec.minLength.has_value() && spec.maxLength.has_value() && *spec.minLength > *spec.maxLength)
                    return false;
                if (spec.maxAssetBytes.has_value() && *spec.maxAssetBytes <= 0)
                    return false;

                if (spec.kind == WidgetPropertyKind::integer || spec.kind == WidgetPropertyKind::number)
                {
                    if (spec.minValue.has_value() && !std::isfinite(*spec.minValue))
                        return false;
                    if (spec.maxValue.has_value() && !std::isfinite(*spec.maxValue))
                        return false;
                    if (spec.step.has_value() && (!std::isfinite(*spec.step) || *spec.step <= 0.0))
                        return false;
                    if (spec.minValue.has_value()
                        && spec.maxValue.has_value()
                        && *spec.minValue > *spec.maxValue)
                    {
                        return false;
                    }

                    if (spec.kind == WidgetPropertyKind::integer)
                    {
                        if (spec.minValue.has_value()
                            && std::abs(*spec.minValue - std::round(*spec.minValue)) > 0.000001)
                        {
                            return false;
                        }
                        if (spec.maxValue.has_value()
                            && std::abs(*spec.maxValue - std::round(*spec.maxValue)) > 0.000001)
                        {
                            return false;
                        }
                        if (spec.step.has_value()
                            && std::abs(*spec.step - std::round(*spec.step)) > 0.000001)
                        {
                            return false;
                        }
                    }
                }

                if (spec.kind == WidgetPropertyKind::enumChoice)
                {
                    if (spec.enumOptions.empty())
                        return false;

                    std::vector<juce::String> optionValues;
                    optionValues.reserve(spec.enumOptions.size());
                    for (auto& option : spec.enumOptions)
                    {
                        option.value = option.value.trim();
                        option.label = option.label.trim().isNotEmpty() ? option.label.trim() : option.value;

                        if (option.value.isEmpty())
                            return false;
                        if (std::find(optionValues.begin(), optionValues.end(), option.value) != optionValues.end())
                            return false;
                        optionValues.push_back(option.value);
                    }
                }

                if (spec.defaultValue.isVoid())
                    spec.defaultValue = defaultValueForPropertyKind(spec);

                if (!descriptor.defaultProperties.contains(spec.key))
                    descriptor.defaultProperties.set(spec.key, spec.defaultValue);

                if (!descriptor.defaultProperties.contains(spec.key))
                {
                    if (spec.required)
                        return false;
                    continue;
                }

                const auto& value = descriptor.defaultProperties[spec.key];
                switch (spec.kind)
                {
                    case WidgetPropertyKind::text:
                        if (!value.isString())
                            return false;
                        if (spec.minLength.has_value() && value.toString().length() < *spec.minLength)
                            return false;
                        if (spec.maxLength.has_value() && value.toString().length() > *spec.maxLength)
                            return false;
                        break;
                    case WidgetPropertyKind::integer:
                        if (!isIntegerLike(value))
                            return false;
                        break;
                    case WidgetPropertyKind::number:
                        if (!isNumericFinite(value))
                            return false;
                        break;
                    case WidgetPropertyKind::boolean:
                        if (!value.isBool())
                            return false;
                        break;
                    case WidgetPropertyKind::enumChoice:
                    {
                        if (!value.isString())
                            return false;
                        const auto asText = value.toString();
                        const auto it = std::find_if(spec.enumOptions.begin(),
                                                     spec.enumOptions.end(),
                                                     [&asText](const WidgetEnumOption& option)
                                                     {
                                                         return option.value == asText;
                                                     });
                        if (it == spec.enumOptions.end())
                            return false;
                        break;
                    }
                    case WidgetPropertyKind::color:
                        if (!validateColorVar(value, spec))
                            return false;
                        break;
                    case WidgetPropertyKind::vec2:
                        if (!validateVec2Var(value))
                            return false;
                        break;
                    case WidgetPropertyKind::rect:
                        if (!isRectFVar(value))
                            return false;
                        break;
                    case WidgetPropertyKind::assetRef:
                        if (!value.isString())
                            return false;
                        break;
                }

                if ((spec.kind == WidgetPropertyKind::integer || spec.kind == WidgetPropertyKind::number)
                    && (spec.minValue.has_value() || spec.maxValue.has_value()))
                {
                    const auto numericValue = static_cast<double>(value);
                    if (spec.minValue.has_value() && numericValue < *spec.minValue)
                        return false;
                    if (spec.maxValue.has_value() && numericValue > *spec.maxValue)
                        return false;
                }
            }

            if (findByKey(descriptor.typeKey) != nullptr)
                return false;

            descriptors.push_back(std::move(descriptor));
            return true;
        }

        bool registerWidgetClass(const WidgetClass& widgetClass)
        {
            return registerWidget(widgetClass.makeDescriptor());
        }

        const WidgetDescriptor* find(WidgetType type) const noexcept
        {
            const auto it = std::find_if(descriptors.begin(),
                                         descriptors.end(),
                                         [type](const WidgetDescriptor& descriptor)
                                         {
                                             return descriptor.type == type;
                                         });
            return it == descriptors.end() ? nullptr : &(*it);
        }

        const WidgetDescriptor* findByKey(const juce::String& typeKey) const noexcept
        {
            const auto it = std::find_if(descriptors.begin(),
                                         descriptors.end(),
                                         [&typeKey](const WidgetDescriptor& descriptor)
                                         {
                                             return descriptor.typeKey == typeKey;
                                         });
            return it == descriptors.end() ? nullptr : &(*it);
        }

        const WidgetDescriptor* findForWidget(const WidgetModel& widget) const noexcept
        {
            const auto typeKey = widgetTypeKeyForWidget(widget);
            if (typeKey.isNotEmpty())
            {
                if (const auto* descriptor = findByKey(typeKey))
                    return descriptor;
                return nullptr;
            }

            return find(widget.type);
        }
        const std::vector<WidgetDescriptor>& all() const noexcept
        {
            return descriptors;
        }

        std::vector<const WidgetDescriptor*> listDescriptors() const
        {
            std::vector<const WidgetDescriptor*> listed;
            listed.reserve(descriptors.size());
            for (const auto& descriptor : descriptors)
                listed.push_back(&descriptor);

            return listed;
        }

        std::vector<const WidgetDescriptor*> findByFilter(const LibraryFilter& filter) const
        {
            const auto queryLower = filter.query.trim().toLowerCase();
            const auto categoryLower = filter.category.trim().toLowerCase();

            const auto inCategory = [&categoryLower](const WidgetDescriptor& descriptor) noexcept
            {
                if (categoryLower.isEmpty() || categoryLower == "all")
                    return true;

                const auto descriptorCategory = descriptor.category.trim().isNotEmpty()
                                                    ? descriptor.category.trim().toLowerCase()
                                                    : juce::String("other");
                return descriptorCategory == categoryLower;
            };

            const auto matchesQuery = [&queryLower](const WidgetDescriptor& descriptor) noexcept
            {
                if (queryLower.isEmpty())
                    return true;

                if (descriptor.displayName.toLowerCase().contains(queryLower))
                    return true;
                if (descriptor.typeKey.toLowerCase().contains(queryLower))
                    return true;
                for (const auto& tag : descriptor.tags)
                {
                    if (tag.toLowerCase().contains(queryLower))
                        return true;
                }

                return false;
            };

            std::vector<const WidgetDescriptor*> filtered;
            filtered.reserve(descriptors.size());

            for (const auto& descriptor : descriptors)
            {
                if (filter.includeFavoritesOnly
                    && !filter.favoriteTypeKeys.contains(descriptor.typeKey))
                {
                    continue;
                }

                if (!inCategory(descriptor))
                    continue;

                if (!matchesQuery(descriptor))
                    continue;

                filtered.push_back(&descriptor);
            }

            return filtered;
        }

        const std::vector<WidgetPropertySpec>* propertySpecs(WidgetType type) const noexcept
        {
            if (const auto* descriptor = find(type))
                return &descriptor->propertySpecs;
            return nullptr;
        }

        const WidgetPropertySpec* propertySpec(WidgetType type, const juce::Identifier& key) const noexcept
        {
            if (const auto* descriptor = find(type))
                return findPropertySpec(*descriptor, key);
            return nullptr;
        }

        const std::vector<WidgetPropertySpec>* propertySpecs(const WidgetModel& widget) const noexcept
        {
            if (const auto* descriptor = findForWidget(widget))
                return &descriptor->propertySpecs;
            return nullptr;
        }

        const WidgetPropertySpec* propertySpec(const WidgetModel& widget,
                                               const juce::Identifier& key) const noexcept
        {
            if (const auto* descriptor = findForWidget(widget))
                return findPropertySpec(*descriptor, key);
            return nullptr;
        }
        std::vector<ExportWidgetMapping> exportMappings() const
        {
            std::vector<ExportWidgetMapping> mappings;
            mappings.reserve(descriptors.size());
            for (const auto& descriptor : descriptors)
            {
                ExportWidgetMapping mapping;
                mapping.type = descriptor.type;
                mapping.typeKey = descriptor.typeKey;
                mapping.exportTargetType = descriptor.exportTargetType.isNotEmpty()
                                               ? descriptor.exportTargetType
                                               : descriptor.typeKey;
                mappings.push_back(std::move(mapping));
            }

            return mappings;
        }

    private:
        std::vector<WidgetDescriptor> descriptors;
    };

    class WidgetFactory
    {
    public:
        explicit WidgetFactory(const WidgetRegistry& registryIn) noexcept
            : registry(registryIn)
        {
        }

        const WidgetDescriptor* descriptorFor(WidgetType type) const noexcept
        {
            const auto* descriptor = registry.find(type);
            if (descriptor == nullptr)
                warnOnce(type, "descriptorFor");
            return descriptor;
        }

        const WidgetDescriptor* descriptorFor(const WidgetModel& widget) const noexcept
        {
            const auto* descriptor = registry.findForWidget(widget);
            if (descriptor == nullptr)
                warnOnce(widget.type, "descriptorFor(widget)");
            return descriptor;
        }

        WidgetId createWidget(DocumentHandle& document,
                              WidgetType type,
                              juce::Point<float> origin,
                              std::optional<WidgetId> layerId = std::nullopt) const
        {
            const auto* descriptor = registry.find(type);
            if (descriptor == nullptr)
            {
                warnOnce(type, "createWidget");
                return 0;
            }

            return createWidgetFromDescriptor(document, *descriptor, origin, layerId, {});
        }

        WidgetId createWidgetByTypeKey(DocumentHandle& document,
                                       const juce::String& typeKey,
                                       juce::Point<float> origin,
                                       std::optional<WidgetId> layerId = std::nullopt) const
        {
            const auto normalizedTypeKey = typeKey.trim();
            if (normalizedTypeKey.isEmpty())
                return 0;

            const auto* descriptor = registry.findByKey(normalizedTypeKey);
            if (descriptor == nullptr)
                return 0;

            return createWidgetFromDescriptor(document,
                                              *descriptor,
                                              origin,
                                              layerId,
                                              normalizedTypeKey);
        }

        juce::Point<float> minSizeFor(WidgetType type) const noexcept
        {
            if (const auto* descriptor = registry.find(type))
                return { std::max(1.0f, descriptor->minSize.x), std::max(1.0f, descriptor->minSize.y) };

            warnOnce(type, "minSizeFor");
            return { 18.0f, 18.0f };
        }

        juce::Point<float> minSizeFor(const WidgetModel& widget) const noexcept
        {
            if (const auto* descriptor = registry.findForWidget(widget))
                return { std::max(1.0f, descriptor->minSize.x), std::max(1.0f, descriptor->minSize.y) };

            warnOnce(widget.type, "minSizeFor(widget)");
            return { 18.0f, 18.0f };
        }

        juce::String exportTargetTypeFor(WidgetType type) const
        {
            if (const auto* descriptor = registry.find(type))
                return descriptor->exportTargetType.isNotEmpty() ? descriptor->exportTargetType : descriptor->typeKey;

            warnOnce(type, "exportTargetTypeFor");
            return {};
        }

        juce::String exportTargetTypeFor(const WidgetModel& widget) const
        {
            if (const auto* descriptor = registry.findForWidget(widget))
                return descriptor->exportTargetType.isNotEmpty() ? descriptor->exportTargetType : descriptor->typeKey;

            warnOnce(widget.type, "exportTargetTypeFor(widget)");
            return {};
        }

        std::vector<ExportWidgetMapping> exportMappings() const
        {
            return registry.exportMappings();
        }

        const std::vector<WidgetPropertySpec>* propertySpecsFor(WidgetType type) const noexcept
        {
            return registry.propertySpecs(type);
        }

        const std::vector<WidgetPropertySpec>* propertySpecsFor(const WidgetModel& widget) const noexcept
        {
            return registry.propertySpecs(widget);
        }

        const WidgetPropertySpec* propertySpecFor(WidgetType type, const juce::Identifier& key) const noexcept
        {
            return registry.propertySpec(type, key);
        }

        const WidgetPropertySpec* propertySpecFor(const WidgetModel& widget,
                                                  const juce::Identifier& key) const noexcept
        {
            return registry.propertySpec(widget, key);
        }

    private:
        WidgetId createWidgetFromDescriptor(DocumentHandle& document,
                                            const WidgetDescriptor& descriptor,
                                            juce::Point<float> origin,
                                            std::optional<WidgetId> layerId,
                                            const juce::String& explicitTypeKey) const
        {
            const auto bounds = descriptor.defaultBounds.withPosition(origin);
            auto props = descriptor.defaultProperties;

            const auto normalizedTypeKey = explicitTypeKey.isNotEmpty()
                                               ? explicitTypeKey.trim()
                                               : descriptor.typeKey.trim();
            if (descriptor.isExternalPlugin || normalizedTypeKey.isNotEmpty())
                setWidgetTypeKey(props, normalizedTypeKey);

            return document.addWidget(descriptor.type, bounds, props, layerId);
        }

        static int typeOrdinal(WidgetType type) noexcept
        {
            return static_cast<int>(type);
        }

        static void warnOnce(WidgetType type, const char* context)
        {
            static std::array<std::vector<int>, 4> warnedByContext;

            int contextIndex = 0;
            if (std::strcmp(context, "descriptorFor") == 0)
                contextIndex = 0;
            else if (std::strcmp(context, "createWidget") == 0)
                contextIndex = 1;
            else if (std::strcmp(context, "minSizeFor") == 0)
                contextIndex = 2;
            else
                contextIndex = 3;

            const auto ordinal = typeOrdinal(type);
            auto& warned = warnedByContext[static_cast<size_t>(contextIndex)];
            if (std::find(warned.begin(), warned.end(), ordinal) != warned.end())
                return;

            warned.push_back(ordinal);
            DBG("[Gyeol] Unsupported widget descriptor in " + juce::String(context)
                + " (type ordinal=" + juce::String(ordinal) + ")");
        }

        const WidgetRegistry& registry;
    };

    inline WidgetRegistry makeDefaultWidgetRegistry()
    {
        WidgetRegistry registry;

        forEachRegisteredDescriptor([&registry](WidgetDescriptor descriptor)
        {
            const auto registered = registry.registerWidget(std::move(descriptor));
            if (!registered)
                DBG("[Gyeol] Widget registration skipped (duplicate or invalid descriptor).");
        });

        return registry;
    }
}





