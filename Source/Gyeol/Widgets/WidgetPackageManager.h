#pragma once

#include "Gyeol/Widgets/GyeolWidgetPluginABI.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace Gyeol::Widgets {
class WidgetPackageManager {
public:
  enum class IssueSeverity { info, warning, error };

  struct Issue {
    IssueSeverity severity = IssueSeverity::info;
    juce::String code;
    juce::String title;
    juce::String message;
    juce::String pluginPath;
    juce::String typeKey;
  };

  struct Dependency {
    juce::String pluginId;
    juce::String pluginVersion;
    juce::String binaryPath;
    juce::StringArray typeKeys;
    bool loaded = false;
    juce::String error;
  };

  void reset() {
    loadedLibraries.clear();
    dependencyByTypeKey.clear();
    dependencyRecords.clear();
    loadIssues.clear();
  }

  void scanDirectory(const juce::File &pluginDirectory,
                     WidgetRegistry &registry) {
    reset();

    if (pluginDirectory.getFullPathName().isEmpty()) {
      addIssue(IssueSeverity::warning,
               "plugin.dir.empty",
               "Plugin directory is empty",
               "Plugin scan skipped because directory path is empty.",
               {},
               {});
      return;
    }

    if (!pluginDirectory.exists() || !pluginDirectory.isDirectory()) {
      addIssue(IssueSeverity::info,
               "plugin.dir.missing",
               "Plugin directory not found",
               "No external plugin directory at: "
                   + pluginDirectory.getFullPathName(),
               pluginDirectory.getFullPathName(),
               {});
      return;
    }

    juce::Array<juce::File> files;
    pluginDirectory.findChildFiles(files,
                                   juce::File::findFiles,
                                   true,
                                   "*");

    for (const auto &file : files) {
      if (!isPluginBinaryFile(file))
        continue;

      loadPackage(file, registry);
    }

    if (dependencyRecords.empty()) {
      addIssue(IssueSeverity::info,
               "plugin.none",
               "No plugin binaries found",
               "No plugin binaries (*.dll/*.so/*.dylib) found under: "
                   + pluginDirectory.getFullPathName(),
               pluginDirectory.getFullPathName(),
               {});
    }
  }

  const std::vector<Issue> &issues() const noexcept { return loadIssues; }

  const std::vector<Dependency> &dependencies() const noexcept {
    return dependencyRecords;
  }

  std::vector<Dependency>
  dependenciesForDocument(const DocumentModel &document,
                          const WidgetRegistry &registry) const {
    std::vector<Dependency> used;
    std::set<int> usedIndices;

    for (const auto &widget : document.widgets) {
      juce::String typeKey = widgetTypeKeyForWidget(widget);
      if (typeKey.isEmpty()) {
        if (const auto *descriptor = registry.findForWidget(widget);
            descriptor != nullptr) {
          typeKey = descriptor->typeKey;
        }
      }

      if (typeKey.isEmpty())
        continue;

      const auto lowered = typeKey.toLowerCase();
      const auto found = dependencyByTypeKey.find(lowered);
      if (found == dependencyByTypeKey.end())
        continue;

      usedIndices.insert(found->second);
    }

    for (const auto index : usedIndices) {
      if (index >= 0 && index < static_cast<int>(dependencyRecords.size()))
        used.push_back(dependencyRecords[static_cast<size_t>(index)]);
    }

    return used;
  }

  std::vector<juce::File>
  binaryFilesForDocument(const DocumentModel &document,
                         const WidgetRegistry &registry) const {
    std::vector<juce::File> files;
    const auto usedDependencies = dependenciesForDocument(document, registry);
    for (const auto &dependency : usedDependencies) {
      if (!dependency.loaded)
        continue;
      const juce::File binary(dependency.binaryPath);
      if (binary.existsAsFile())
        files.push_back(binary);
    }

    return files;
  }

private:
  struct LoadedLibrary {
    juce::File binary;
    std::unique_ptr<juce::DynamicLibrary> handle;
  };

  static bool isPluginBinaryFile(const juce::File &file) {
    if (!file.existsAsFile())
      return false;

    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".dll" || extension == ".so" || extension == ".dylib";
  }

  static juce::String readString(const juce::NamedValueSet &props,
                                 const juce::Identifier &key,
                                 const juce::String &fallback = {}) {
    if (!props.contains(key))
      return fallback;
    const auto text = props[key].toString().trim();
    return text.isNotEmpty() ? text : fallback;
  }

  static juce::StringArray readStringArray(const juce::var &value) {
    juce::StringArray out;
    if (const auto *array = value.getArray(); array != nullptr) {
      for (const auto &item : *array) {
        const auto text = item.toString().trim();
        if (text.isNotEmpty())
          out.add(text);
      }
      out.removeDuplicates(false);
    }

    return out;
  }

  static std::optional<juce::Rectangle<float>> parseRect(const juce::var &value) {
    const auto *object = value.getDynamicObject();
    if (object == nullptr)
      return std::nullopt;

    const auto &props = object->getProperties();
    if (!props.contains("x") || !props.contains("y") || !props.contains("w")
        || !props.contains("h")) {
      return std::nullopt;
    }

    if (!isNumericVar(props["x"]) || !isNumericVar(props["y"])
        || !isNumericVar(props["w"]) || !isNumericVar(props["h"])) {
      return std::nullopt;
    }

    return juce::Rectangle<float>(static_cast<float>(props["x"]),
                                  static_cast<float>(props["y"]),
                                  static_cast<float>(props["w"]),
                                  static_cast<float>(props["h"]));
  }

  static std::optional<juce::Point<float>> parsePoint(const juce::var &value) {
    const auto *object = value.getDynamicObject();
    if (object == nullptr)
      return std::nullopt;

    const auto &props = object->getProperties();
    if (props.contains("w") && props.contains("h")
        && isNumericVar(props["w"]) && isNumericVar(props["h"])) {
      return juce::Point<float>(static_cast<float>(props["w"]),
                                static_cast<float>(props["h"]));
    }

    if (props.contains("x") && props.contains("y")
        && isNumericVar(props["x"]) && isNumericVar(props["y"])) {
      return juce::Point<float>(static_cast<float>(props["x"]),
                                static_cast<float>(props["y"]));
    }

    return std::nullopt;
  }

  static PropertyBag parsePropertyBag(const juce::var &value) {
    PropertyBag bag;
    const auto *object = value.getDynamicObject();
    if (object == nullptr)
      return bag;

    const auto &props = object->getProperties();
    for (int i = 0; i < props.size(); ++i)
      bag.set(props.getName(i), props.getValueAt(i));

    return bag;
  }

  static WidgetType parseWidgetType(const juce::var &value,
                                    WidgetType fallback) {
    if (value.isInt() || value.isInt64()) {
      const auto ordinal = static_cast<int>(value);
      if (ordinal >= static_cast<int>(WidgetType::button)
          && ordinal <= static_cast<int>(WidgetType::textInput)) {
        return static_cast<WidgetType>(ordinal);
      }
      return fallback;
    }

    const auto key = value.toString().trim().toLowerCase();
    if (key == "button")
      return WidgetType::button;
    if (key == "slider")
      return WidgetType::slider;
    if (key == "knob")
      return WidgetType::knob;
    if (key == "label")
      return WidgetType::label;
    if (key == "meter")
      return WidgetType::meter;
    if (key == "toggle")
      return WidgetType::toggle;
    if (key == "combobox" || key == "combo_box" || key == "combo")
      return WidgetType::comboBox;
    if (key == "textinput" || key == "text_input")
      return WidgetType::textInput;

    return fallback;
  }

  static int parseVersionMajor(const juce::String &version, int fallback = 0) {
    const auto majorText =
        version.trim().upToFirstOccurrenceOf(".", false, false).trim();
    if (majorText.isEmpty() || !majorText.containsOnly("0123456789"))
      return fallback;

    return majorText.getIntValue();
  }

  static WidgetPropertyKind parsePropertyKind(const juce::String &key) {
    const auto normalized = key.trim().toLowerCase();
    if (normalized == "text")
      return WidgetPropertyKind::text;
    if (normalized == "integer" || normalized == "int")
      return WidgetPropertyKind::integer;
    if (normalized == "number" || normalized == "float" || normalized == "double")
      return WidgetPropertyKind::number;
    if (normalized == "boolean" || normalized == "bool")
      return WidgetPropertyKind::boolean;
    if (normalized == "enumchoice" || normalized == "enum")
      return WidgetPropertyKind::enumChoice;
    if (normalized == "color" || normalized == "colour")
      return WidgetPropertyKind::color;
    if (normalized == "vec2" || normalized == "point")
      return WidgetPropertyKind::vec2;
    if (normalized == "rect")
      return WidgetPropertyKind::rect;
    if (normalized == "assetref" || normalized == "asset")
      return WidgetPropertyKind::assetRef;
    return WidgetPropertyKind::text;
  }

  static std::vector<WidgetPropertySpec>
  parsePropertySpecs(const juce::var &value, const PropertyBag &defaults) {
    std::vector<WidgetPropertySpec> specs;

    if (const auto *array = value.getArray(); array != nullptr) {
      specs.reserve(static_cast<size_t>(array->size()));
      for (const auto &item : *array) {
        const auto *object = item.getDynamicObject();
        if (object == nullptr)
          continue;

        const auto &props = object->getProperties();
        const auto keyText = readString(props, "key", {});
        if (keyText.isEmpty())
          continue;

        WidgetPropertySpec spec;
        spec.key = juce::Identifier(keyText);
        spec.label = readString(props, "label", keyText);
        spec.kind = parsePropertyKind(readString(props, "kind", "text"));
        spec.required = props.contains("required") ? static_cast<bool>(props["required"]) : false;
        if (props.contains("defaultValue"))
          spec.defaultValue = props["defaultValue"];
        else if (defaults.contains(spec.key))
          spec.defaultValue = defaults[spec.key];

        if (props.contains("min") && isNumericVar(props["min"]))
          spec.minValue = static_cast<double>(props["min"]);
        if (props.contains("max") && isNumericVar(props["max"]))
          spec.maxValue = static_cast<double>(props["max"]);
        if (props.contains("step") && isNumericVar(props["step"]))
          spec.step = static_cast<double>(props["step"]);
        if (props.contains("group"))
          spec.group = props["group"].toString();
        if (props.contains("order") && isNumericVar(props["order"]))
          spec.order = static_cast<int>(props["order"]);

        specs.push_back(std::move(spec));
      }
    }

    if (!specs.empty())
      return specs;

    // Fallback: expose all default properties as simple editable fields.
    specs.reserve(static_cast<size_t>(defaults.size()));
    for (int i = 0; i < defaults.size(); ++i) {
      WidgetPropertySpec spec;
      spec.key = defaults.getName(i);
      spec.label = spec.key.toString();
      spec.defaultValue = defaults.getValueAt(i);

      if (spec.defaultValue.isBool())
        spec.kind = WidgetPropertyKind::boolean;
      else if (spec.defaultValue.isInt() || spec.defaultValue.isInt64())
        spec.kind = WidgetPropertyKind::integer;
      else if (spec.defaultValue.isDouble())
        spec.kind = WidgetPropertyKind::number;
      else
        spec.kind = WidgetPropertyKind::text;

      specs.push_back(std::move(spec));
    }

    return specs;
  }

  static std::vector<RuntimeEventSpec> parseRuntimeEvents(const juce::var &value) {
    std::vector<RuntimeEventSpec> events;
    const auto *array = value.getArray();
    if (array == nullptr)
      return events;

    events.reserve(static_cast<size_t>(array->size()));
    for (const auto &item : *array) {
      const auto *object = item.getDynamicObject();
      if (object == nullptr)
        continue;

      const auto &props = object->getProperties();
      RuntimeEventSpec event;
      event.key = readString(props, "key", {});
      if (event.key.isEmpty())
        continue;

      event.displayLabel = readString(props, "displayLabel", event.key);
      event.description = readString(props, "description", {});
      event.continuous = props.contains("continuous")
                             ? static_cast<bool>(props["continuous"])
                             : false;
      if (props.contains("payloadSchema"))
        event.payloadSchema = props["payloadSchema"];
      if (props.contains("throttleMs") && isNumericVar(props["throttleMs"]))
        event.throttleMs = static_cast<int>(props["throttleMs"]);
      if (props.contains("debounceMs") && isNumericVar(props["debounceMs"]))
        event.debounceMs = static_cast<int>(props["debounceMs"]);
      event.reliability = readString(props, "reliability", "bestEffort");
      event.channel = readString(props, "channel", "ui");

      events.push_back(std::move(event));
    }

    return events;
  }

  static WidgetPainter makeExternalPainter(const juce::String &displayName,
                                           const juce::String &typeKey) {
    return [displayName, typeKey](juce::Graphics &g,
                                  const WidgetModel &,
                                  const juce::Rectangle<float> &bounds) {
      auto body = bounds.reduced(1.0f);
      g.setColour(juce::Colour::fromRGB(40, 45, 56));
      g.fillRoundedRectangle(body, 5.0f);
      g.setColour(juce::Colour::fromRGB(225, 118, 88));
      g.drawRoundedRectangle(body, 5.0f, 1.2f);

      g.setColour(juce::Colour::fromRGB(226, 232, 240));
      g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
      const auto name = displayName.isNotEmpty() ? displayName : juce::String("External Widget");
      g.drawFittedText(name,
                       body.reduced(6.0f).toNearestInt(),
                       juce::Justification::centredTop,
                       1);

      g.setColour(juce::Colour::fromRGB(247, 161, 131));
      g.setFont(juce::FontOptions(10.0f));
      const auto key = typeKey.isNotEmpty() ? typeKey : juce::String("external.unknown");
      g.drawFittedText(key,
                       body.reduced(6.0f).withTrimmedTop(14.0f).toNearestInt(),
                       juce::Justification::centred,
                       2);
    };
  }

  static bool hasCanonicalCreateEntrypoint(juce::DynamicLibrary &library) {
    return library.getFunction(GYEOL_WIDGET_PLUGIN_SYMBOL_CREATE_INSTANCE) !=
           nullptr;
  }

  static std::optional<juce::String>
  readManifestTextFromDynamicLibrary(juce::DynamicLibrary &library) {
    using ManifestCStringFn = GyeolGetPluginManifestJsonFn;
    using ManifestUtf8ViewFn = GyeolGetPluginManifestFn;

    static constexpr const char *manifestUtf8ViewSymbols[] = {
        GYEOL_WIDGET_PLUGIN_SYMBOL_GET_MANIFEST,
        "gyeolGetPluginManifest",
        "gyeol_widget_get_plugin_manifest"};

    for (const auto *symbol : manifestUtf8ViewSymbols) {
      if (symbol == nullptr)
        continue;

      if (void *rawFn = library.getFunction(symbol); rawFn != nullptr) {
        const auto fn = reinterpret_cast<ManifestUtf8ViewFn>(rawFn);
        const auto view = fn != nullptr ? fn() : GyeolWidgetUtf8View{nullptr, 0};
        if (view.data != nullptr && view.size > 0)
          return juce::String::fromUTF8(view.data, static_cast<int>(view.size));
      }
    }

    static constexpr const char *manifestCStringSymbols[] = {
        GYEOL_WIDGET_PLUGIN_SYMBOL_GET_MANIFEST_JSON,
        "gyeolGetPluginManifestJson",
        "GyeolWidget_GetManifestJson",
        "gyeol_widget_get_manifest_json",
        "gyeol_widget_get_plugin_manifest_json"};

    for (const auto *symbol : manifestCStringSymbols) {
      if (symbol == nullptr)
        continue;

      if (void *rawFn = library.getFunction(symbol); rawFn != nullptr) {
        const auto fn = reinterpret_cast<ManifestCStringFn>(rawFn);
        const auto *text = fn != nullptr ? fn() : nullptr;
        if (text != nullptr && text[0] != '\0')
          return juce::String::fromUTF8(text);
      }
    }

    return std::nullopt;
  }

  static std::optional<juce::String>
  readManifestTextFromSidecar(const juce::File &binaryFile) {
    const auto baseName = binaryFile.getFileNameWithoutExtension();
    const auto parent = binaryFile.getParentDirectory();

    const std::vector<juce::File> candidates = {
        binaryFile.getSiblingFile(binaryFile.getFileName() + ".manifest.json"),
        parent.getChildFile(baseName + ".manifest.json"),
        parent.getChildFile(baseName + ".gyeol.json"),
        parent.getChildFile(baseName + ".json")};

    for (const auto &candidate : candidates) {
      if (!candidate.existsAsFile())
        continue;

      const auto text = candidate.loadFileAsString();
      if (text.trim().isNotEmpty())
        return text;
    }

    return std::nullopt;
  }

  void loadPackage(const juce::File &binaryFile, WidgetRegistry &registry) {
    Dependency dependency;
    dependency.binaryPath = binaryFile.getFullPathName();

    auto library = std::make_unique<juce::DynamicLibrary>();
    if (!library->open(binaryFile.getFullPathName())) {
      dependency.loaded = false;
      dependency.error = "dynamic_library_open_failed";
      dependencyRecords.push_back(dependency);

      addIssue(IssueSeverity::error,
               "plugin.load.failed",
               "Failed to load dynamic library",
               "Could not open plugin binary: " + binaryFile.getFullPathName(),
               binaryFile.getFullPathName(),
               {});
      return;
    }

    if (!hasCanonicalCreateEntrypoint(*library)) {
      addIssue(IssueSeverity::warning,
               "plugin.abi.entrypoint.missing",
               "Canonical C ABI entrypoint missing",
               "GyeolCreateWidgetInstance() symbol is missing. Loader will continue in metadata-only mode.",
               binaryFile.getFullPathName(),
               {});
    }

    auto manifestText = readManifestTextFromDynamicLibrary(*library);
    if (!manifestText.has_value())
      manifestText = readManifestTextFromSidecar(binaryFile);

    if (!manifestText.has_value() || manifestText->trim().isEmpty()) {
      dependency.loaded = false;
      dependency.error = "manifest_missing";
      dependencyRecords.push_back(dependency);

      addIssue(IssueSeverity::error,
               "plugin.manifest.missing",
               "Plugin manifest missing",
               "No manifest JSON entrypoint or sidecar was found.",
               binaryFile.getFullPathName(),
               {});
      return;
    }

    const auto parsed = juce::JSON::parse(*manifestText);
    const auto *root = parsed.getDynamicObject();
    if (root == nullptr) {
      dependency.loaded = false;
      dependency.error = "manifest_parse_failed";
      dependencyRecords.push_back(dependency);

      addIssue(IssueSeverity::error,
               "plugin.manifest.parse",
               "Manifest parse failed",
               "Manifest is not a valid JSON object.",
               binaryFile.getFullPathName(),
               {});
      return;
    }

    const auto &rootProps = root->getProperties();
    const auto pluginIdRoot =
        readString(rootProps,
                   "pluginId",
                   "plugin." + binaryFile.getFileNameWithoutExtension());
    const auto pluginVersionRoot = readString(rootProps, "pluginVersion", "1.0.0");
    const auto sdkMinRoot = readString(rootProps, "sdkMinVersion", "1.0.0");
    const auto abiVersionRoot = readString(rootProps, "abiVersion", "1");
    const auto vendorRoot = readString(rootProps, "vendor", "Unknown");

    const auto abiMajor = parseVersionMajor(abiVersionRoot, 1);
    if (abiMajor != static_cast<int>(GYEOL_WIDGET_PLUGIN_ABI_VERSION_MAJOR)) {
      dependency.loaded = false;
      dependency.error = "abi_major_mismatch";
      dependency.pluginId = pluginIdRoot;
      dependency.pluginVersion = pluginVersionRoot;
      dependencyRecords.push_back(dependency);

      addIssue(IssueSeverity::error,
               "plugin.abi.mismatch",
               "ABI version mismatch",
               "Plugin ABI major=" + juce::String(abiMajor)
                   + ", host ABI major="
                   + juce::String(static_cast<int>(
                         GYEOL_WIDGET_PLUGIN_ABI_VERSION_MAJOR)),
               binaryFile.getFullPathName(),
               {});
      return;
    }

    const auto sdkMajor = parseVersionMajor(sdkMinRoot, 1);
    if (sdkMajor > 1) {
      dependency.loaded = false;
      dependency.error = "sdk_version_incompatible";
      dependency.pluginId = pluginIdRoot;
      dependency.pluginVersion = pluginVersionRoot;
      dependencyRecords.push_back(dependency);

      addIssue(IssueSeverity::error,
               "plugin.sdk.incompatible",
               "SDK version incompatible",
               "Plugin requires SDK " + sdkMinRoot + " but host major is 1.x.",
               binaryFile.getFullPathName(),
               {});
      return;
    }

    juce::Array<juce::var> widgetsArray;
    if (rootProps.contains("widgets") && rootProps["widgets"].isArray()) {
      widgetsArray = *rootProps["widgets"].getArray();
    } else {
      widgetsArray.add(parsed);
    }

    int registeredCount = 0;
    for (const auto &widgetVar : widgetsArray) {
      const auto *widgetObject = widgetVar.getDynamicObject();
      if (widgetObject == nullptr)
        continue;

      const auto &widgetProps = widgetObject->getProperties();
      WidgetDescriptor descriptor;
      descriptor.type = parseWidgetType(
          widgetProps.getWithDefault("widgetType", widgetProps.getWithDefault("type", juce::var())),
          WidgetType::button);
      descriptor.typeKey = readString(widgetProps, "typeKey", {});
      descriptor.displayName = readString(widgetProps,
                                          "displayName",
                                          descriptor.typeKey.isNotEmpty()
                                              ? descriptor.typeKey
                                              : juce::String("External Widget"));

      const auto categoryText = readString(widgetProps, "category", {});
      descriptor.category = categoryText.isNotEmpty()
                                ? "[External] " + categoryText
                                : juce::String("[External]");
      descriptor.defaultBounds = parseRect(widgetProps.getWithDefault("defaultBounds", juce::var()))
                                     .value_or(juce::Rectangle<float>(0.0f, 0.0f, 120.0f, 36.0f));
      descriptor.minSize = parsePoint(widgetProps.getWithDefault("minSize", juce::var()))
                               .value_or(juce::Point<float>(18.0f, 18.0f));
      descriptor.defaultProperties =
          parsePropertyBag(widgetProps.getWithDefault("defaultProperties", juce::var()));
      descriptor.propertySpecs =
          parsePropertySpecs(widgetProps.getWithDefault("propertySpecs", juce::var()),
                             descriptor.defaultProperties);
      descriptor.runtimeEvents =
          parseRuntimeEvents(widgetProps.getWithDefault("runtimeEvents", juce::var()));

      descriptor.schemaVersion = readString(widgetProps, "schemaVersion", "2.0.0");
      descriptor.manifestVersion =
          readString(widgetProps, "manifestVersion", "1.0.0");
      descriptor.widgetTypeVersion =
          juce::jmax(1,
                     static_cast<int>(widgetProps.getWithDefault("widgetTypeVersion", juce::var(1))));

      descriptor.pluginId = readString(widgetProps, "pluginId", pluginIdRoot);
      descriptor.pluginVersion =
          readString(widgetProps, "pluginVersion", pluginVersionRoot);
      descriptor.vendor = readString(widgetProps, "vendor", vendorRoot);
      descriptor.releaseChannel =
          readString(widgetProps, "releaseChannel", "stable");
      descriptor.sdkMinVersion =
          readString(widgetProps, "sdkMinVersion", sdkMinRoot);
      descriptor.sdkMaxVersion =
          readString(widgetProps, "sdkMaxVersion", {});
      descriptor.abiVersion =
          readString(widgetProps, "abiVersion", abiVersionRoot);
      descriptor.abiHash = readString(widgetProps, "abiHash", {});
      descriptor.codegenApiVersion =
          readString(widgetProps, "codegenApiVersion", "2.1");

      descriptor.supportedActions =
          readStringArray(widgetProps.getWithDefault("supportedActions", juce::var()));
      descriptor.propertyBindings =
          readStringArray(widgetProps.getWithDefault("propertyBindings", juce::var()));
      descriptor.stateInputs =
          readStringArray(widgetProps.getWithDefault("stateInputs", juce::var()));
      descriptor.stateOutputs =
          readStringArray(widgetProps.getWithDefault("stateOutputs", juce::var()));
      descriptor.permissions =
          readStringArray(widgetProps.getWithDefault("permissions", juce::var()));

      descriptor.requiredJuceModules =
          readStringArray(widgetProps.getWithDefault("requiredJuceModules", juce::var()));
      descriptor.requiredHeaders =
          readStringArray(widgetProps.getWithDefault("requiredHeaders", juce::var()));
      descriptor.requiredLibraries =
          readStringArray(widgetProps.getWithDefault("requiredLibraries", juce::var()));
      descriptor.requiredCompileDefinitions =
          readStringArray(widgetProps.getWithDefault("requiredCompileDefinitions", juce::var()));
      descriptor.requiredLinkOptions =
          readStringArray(widgetProps.getWithDefault("requiredLinkOptions", juce::var()));

      descriptor.isExternalPlugin = true;
      descriptor.pluginBinaryPath = binaryFile.getFullPathName();
      descriptor.painter =
          makeExternalPainter(descriptor.displayName, descriptor.typeKey);

      if (!registry.registerWidget(std::move(descriptor))) {
        addIssue(IssueSeverity::error,
                 "plugin.widget.register",
                 "Widget registration failed",
                 "Descriptor registration failed (duplicate key or invalid schema).",
                 binaryFile.getFullPathName(),
                 readString(widgetProps, "typeKey", {}));
        continue;
      }

      const auto registeredTypeKey = readString(widgetProps, "typeKey", {});
      if (registeredTypeKey.isNotEmpty())
        dependency.typeKeys.add(registeredTypeKey);
      ++registeredCount;
    }

    dependency.pluginId = pluginIdRoot;
    dependency.pluginVersion = pluginVersionRoot;
    dependency.loaded = registeredCount > 0;
    if (!dependency.loaded)
      dependency.error = "no_widget_registered";

    dependencyRecords.push_back(dependency);
    const auto dependencyIndex =
        static_cast<int>(dependencyRecords.size() - 1);

    for (const auto &typeKey : dependency.typeKeys)
      dependencyByTypeKey[typeKey.toLowerCase()] = dependencyIndex;

    if (!dependency.loaded) {
      addIssue(IssueSeverity::warning,
               "plugin.widget.none",
               "No widgets registered",
               "Plugin loaded but no widget descriptor passed validation.",
               binaryFile.getFullPathName(),
               {});
      return;
    }

    LoadedLibrary loaded;
    loaded.binary = binaryFile;
    loaded.handle = std::move(library);
    loadedLibraries.push_back(std::move(loaded));

    addIssue(IssueSeverity::info,
             "plugin.load.ok",
             "Plugin loaded",
             "Registered " + juce::String(registeredCount)
                 + " widget descriptor(s).",
             binaryFile.getFullPathName(),
             {});
  }

  void addIssue(IssueSeverity severity,
                const juce::String &code,
                const juce::String &title,
                const juce::String &message,
                const juce::String &pluginPath,
                const juce::String &typeKey) {
    Issue issue;
    issue.severity = severity;
    issue.code = code;
    issue.title = title;
    issue.message = message;
    issue.pluginPath = pluginPath;
    issue.typeKey = typeKey;
    loadIssues.push_back(std::move(issue));
  }

  std::vector<LoadedLibrary> loadedLibraries;
  std::map<juce::String, int> dependencyByTypeKey;
  std::vector<Dependency> dependencyRecords;
  std::vector<Issue> loadIssues;
};
} // namespace Gyeol::Widgets