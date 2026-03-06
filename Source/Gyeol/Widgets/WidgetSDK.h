#pragma once

#include "Gyeol/Public/Types.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace Gyeol::Widgets {
struct AssetRef {
  juce::String assetId;
  juce::String displayName;
  juce::String mime;
};

struct DropOption {
  juce::String label;
  juce::Identifier propKey;
  juce::String hint;
};

enum class WidgetPropertyKind {
  text,
  integer,
  number,
  boolean,
  enumChoice,
  color,
  vec2,
  rect,
  assetRef
};

enum class WidgetPropertyUiHint {
  autoHint,
  lineEdit,
  multiLine,
  spinBox,
  slider,
  toggle,
  dropdown,
  segmented,
  colorPicker,
  vec2Editor,
  rectEditor,
  assetPicker
};

enum class ColorStorage {
  hexString,     // "#RRGGBB" / "#RRGGBBAA"
  rgbaObject255, // { r,g,b,a } in 0..255
  rgbaObject01,  // { r,g,b,a } in 0..1
  hslaObject,    // { h,s,l,a } with h:0..360, s/l/a:0..1
  argbInt,       // 0xAARRGGBB
  token          // design token id/string
};

struct WidgetEnumOption {
  juce::String value;
  juce::String label;
};

struct WidgetPropertySpec {
  juce::Identifier key;
  juce::String label;
  WidgetPropertyKind kind = WidgetPropertyKind::text;
  WidgetPropertyUiHint uiHint = WidgetPropertyUiHint::autoHint;
  bool required = false;
  juce::String group = "Widget";
  int order = 0;
  juce::String hint;
  juce::String unit;
  juce::String displayFormat;
  juce::String valueCurve;
  juce::var defaultValue;
  std::optional<double> minValue;
  std::optional<double> maxValue;
  std::optional<double> step;
  std::optional<int> minLength;
  std::optional<int> maxLength;
  juce::String regex;
  juce::String localeKey;
  int decimals = 3;
  std::vector<WidgetEnumOption> enumOptions;
  std::vector<AssetKind> acceptedAssetKinds;
  juce::StringArray acceptedMimeTypes;
  std::optional<std::int64_t> maxAssetBytes;
  juce::String fallbackAssetId;
  juce::StringArray preloadAssets;
  bool supportsStreaming = false;
  ColorStorage colorStorage = ColorStorage::hexString;
  bool colorAllowAlpha = true;
  bool colorAllowHdr = false;
  std::optional<juce::Identifier> dependsOnKey;
  std::optional<juce::var> dependsOnValue;
  bool advanced = false;
  bool readOnly = false;
};

struct RuntimeEventSpec {
  juce::String key;
  juce::String displayLabel;
  juce::String description;
  bool continuous = false;
  juce::var payloadSchema;
  std::optional<int> throttleMs;
  std::optional<int> debounceMs;
  juce::String reliability = "bestEffort";
  juce::String channel = "ui";
};

using DropOptionsProvider = std::function<std::vector<DropOption>(
    const WidgetModel &, const AssetRef &)>;
using ApplyDrop =
    std::function<juce::Result(PropertyBag &patchOut, const WidgetModel &,
                               const AssetRef &, const DropOption &)>;

enum class ConsumeEvent { no, yes };

using CursorProvider = std::function<juce::MouseCursor(
    const WidgetModel &, juce::Point<float> local)>;
using HitTest =
    std::function<bool(const WidgetModel &, juce::Point<float> local)>;

struct InteractionHandlers {
  std::function<ConsumeEvent(const WidgetModel &, const juce::MouseEvent &,
                             PropertyBag &patchPreview)>
      onMouseDown;
  std::function<ConsumeEvent(const WidgetModel &, const juce::MouseEvent &,
                             PropertyBag &patchPreview)>
      onMouseDrag;
  std::function<ConsumeEvent(const WidgetModel &, const juce::MouseEvent &,
                             PropertyBag &patchCommit)>
      onMouseUp;
};

using WidgetPainter = std::function<void(juce::Graphics &, const WidgetModel &,
                                         const juce::Rectangle<float> &)>;

class IPropertyProvider {
public:
  virtual ~IPropertyProvider() = default;
  virtual const std::vector<WidgetPropertySpec> &propertySpecs() const = 0;
  virtual const PropertyBag &defaultProperties() const = 0;
};

class IGyeolCustomWidget : public IPropertyProvider {
public:
  ~IGyeolCustomWidget() override = default;

  virtual juce::String typeKey() const = 0;
  virtual juce::String displayName() const = 0;
  virtual juce::Rectangle<float> defaultBounds() const = 0;
  virtual juce::Point<float> minSize() const = 0;

  virtual void paint(juce::Graphics &g, const WidgetModel &widget,
                     const juce::Rectangle<float> &bounds) = 0;

  virtual bool hitTest(const WidgetModel &, juce::Point<float>) const {
    return true;
  }

  virtual juce::MouseCursor cursorFor(const WidgetModel &,
                                      juce::Point<float>) const {
    return juce::MouseCursor::NormalCursor;
  }

  virtual ConsumeEvent onMouseDown(const WidgetModel &, const juce::MouseEvent &,
                                   PropertyBag &) {
    return ConsumeEvent::no;
  }

  virtual ConsumeEvent onMouseDrag(const WidgetModel &, const juce::MouseEvent &,
                                   PropertyBag &) {
    return ConsumeEvent::no;
  }

  virtual ConsumeEvent onMouseUp(const WidgetModel &, const juce::MouseEvent &,
                                 PropertyBag &) {
    return ConsumeEvent::no;
  }
};

struct ExportCodegenContext {
  const WidgetModel &widget;
  juce::String memberName;
  juce::String typeKey;
  juce::String exportTargetType;
};

struct ExportCodegenOutput {
  juce::String memberType;
  juce::String codegenKind;
  juce::StringArray constructorLines;
  juce::StringArray resizedLines;
};

using ExportCodegen = std::function<juce::Result(const ExportCodegenContext &,
                                                 ExportCodegenOutput &)>;

struct WidgetDescriptor {
  WidgetType type = WidgetType::button;
  juce::String schemaVersion = "2.0.0";
  juce::String manifestVersion = "1.0.0";
  int widgetTypeVersion = 1;
  juce::var migrationRules;

  juce::String abiVersion = "1";
  juce::String abiHash;
  juce::String sdkMinVersion = "1.0.0";
  juce::String sdkMaxVersion;
  juce::StringArray supportedHostVersions;
  juce::StringArray platformTargets;
  juce::StringArray architectureTargets;

  juce::String pluginId;
  juce::String pluginVersion = "1.0.0";
  juce::String vendor;
  juce::String releaseChannel = "stable";
  bool isExternalPlugin = false;
  juce::String pluginBinaryPath;
  juce::String publisherFingerprint;
  juce::String signature;

  juce::StringArray capabilities;
  juce::String repaintPolicy = "onDemand";
  std::optional<double> tickRateHintHz;
  bool supportsOffscreenCache = false;
  std::optional<double> estimatedPaintCost;
  std::optional<int> memoryBudgetKb;

  juce::String threadingModel = "main-thread";
  bool realtimeSafe = false;
  bool renderThreadOnly = false;

  juce::String typeKey;
  juce::String displayName;
  juce::String category;
  juce::StringArray tags;
  juce::String iconKey;
  juce::String exportTargetType;
  juce::Rectangle<float> defaultBounds;
  juce::Point<float> minSize{18.0f, 18.0f};
  PropertyBag defaultProperties;
  std::vector<WidgetPropertySpec> propertySpecs;
  std::vector<RuntimeEventSpec> runtimeEvents;

  juce::StringArray supportedActions;
  juce::StringArray propertyBindings;
  juce::StringArray stateInputs;
  juce::StringArray stateOutputs;

  juce::String a11yRole;
  juce::String a11yLabelKey;
  juce::String testId;
  juce::var diagnosticsContract;
  juce::StringArray telemetryTags;

  juce::String statePolicy;
  juce::StringArray persistedKeys;
  juce::String resetPolicy;
  juce::String migrationPolicy;

  juce::StringArray permissions;
  juce::String sandboxLevel = "strict";
  juce::StringArray fileAccess;
  juce::StringArray networkAccess;
  juce::StringArray midiAccess;
  juce::StringArray scriptAccess;

  juce::StringArray requiredJuceModules;
  juce::StringArray requiredHeaders;
  juce::StringArray requiredLibraries;
  juce::StringArray requiredCompileDefinitions;
  juce::StringArray requiredLinkOptions;
  juce::String codegenApiVersion;

  WidgetPainter painter;
  std::function<void(juce::Graphics &, const juce::Rectangle<float> &)>
      iconPainter;
  ExportCodegen exportCodegen;

  HitTest hitTest;
  CursorProvider cursorProvider;
  InteractionHandlers interaction;
  DropOptionsProvider dropOptions;
  ApplyDrop applyDrop;
};

inline constexpr const char *kWidgetTypeKeyPropertyName =
    "__gyeol.widgetTypeKey";

inline const juce::Identifier &widgetTypeKeyPropertyId() {
  static const juce::Identifier propertyId(kWidgetTypeKeyPropertyName);
  return propertyId;
}

inline juce::String widgetTypeKeyFromProperties(const PropertyBag &properties) {
  const auto &propertyId = widgetTypeKeyPropertyId();
  if (!properties.contains(propertyId))
    return {};

  return properties[propertyId].toString().trim();
}

inline juce::String widgetTypeKeyForWidget(const WidgetModel &widget) {
  return widgetTypeKeyFromProperties(widget.properties);
}

inline bool isExternalWidgetModel(const WidgetModel &widget) {
  return widgetTypeKeyForWidget(widget).isNotEmpty();
}

inline void setWidgetTypeKey(PropertyBag &properties,
                             const juce::String &typeKey) {
  const auto normalizedTypeKey = typeKey.trim();
  const auto &propertyId = widgetTypeKeyPropertyId();
  if (normalizedTypeKey.isEmpty()) {
    properties.remove(propertyId);
    return;
  }

  properties.set(propertyId, normalizedTypeKey);
}

inline const WidgetPropertySpec *
findPropertySpec(const std::vector<WidgetPropertySpec> &specs,
                 const juce::Identifier &key) noexcept {
  const auto it = std::find_if(
      specs.begin(), specs.end(),
      [&key](const WidgetPropertySpec &spec) { return spec.key == key; });
  return it == specs.end() ? nullptr : &(*it);
}

inline const WidgetPropertySpec *
findPropertySpec(const WidgetDescriptor &descriptor,
                 const juce::Identifier &key) noexcept {
  return findPropertySpec(descriptor.propertySpecs, key);
}

inline bool isAssetKindAccepted(const WidgetPropertySpec &spec,
                                AssetKind kind) noexcept {
  if (spec.acceptedAssetKinds.empty())
    return true;

  return std::find(spec.acceptedAssetKinds.begin(),
                   spec.acceptedAssetKinds.end(),
                   kind) != spec.acceptedAssetKinds.end();
}

inline juce::var makePayloadSchema(const juce::StringArray &requiredKeys = {}) {
  auto payload = std::make_unique<juce::DynamicObject>();
  juce::Array<juce::var> required;
  required.ensureStorageAllocated(requiredKeys.size());
  for (const auto &key : requiredKeys) {
    const auto normalized = key.trim();
    if (normalized.isNotEmpty())
      required.add(normalized);
  }
  payload->setProperty("required", juce::var(required));
  return juce::var(payload.release());
}

inline juce::var emptyRequiredPayloadSchema() {
  return makePayloadSchema();
}

inline juce::String widgetPropertyKindToKey(WidgetPropertyKind kind) {
  switch (kind) {
  case WidgetPropertyKind::text:
    return "text";
  case WidgetPropertyKind::integer:
    return "integer";
  case WidgetPropertyKind::number:
    return "number";
  case WidgetPropertyKind::boolean:
    return "boolean";
  case WidgetPropertyKind::enumChoice:
    return "enumChoice";
  case WidgetPropertyKind::color:
    return "color";
  case WidgetPropertyKind::vec2:
    return "vec2";
  case WidgetPropertyKind::rect:
    return "rect";
  case WidgetPropertyKind::assetRef:
    return "assetRef";
  }

  return "text";
}

inline juce::String widgetPropertyUiHintToKey(WidgetPropertyUiHint hint) {
  switch (hint) {
  case WidgetPropertyUiHint::autoHint:
    return "auto";
  case WidgetPropertyUiHint::lineEdit:
    return "lineEdit";
  case WidgetPropertyUiHint::multiLine:
    return "multiLine";
  case WidgetPropertyUiHint::spinBox:
    return "spinBox";
  case WidgetPropertyUiHint::slider:
    return "slider";
  case WidgetPropertyUiHint::toggle:
    return "toggle";
  case WidgetPropertyUiHint::dropdown:
    return "dropdown";
  case WidgetPropertyUiHint::segmented:
    return "segmented";
  case WidgetPropertyUiHint::colorPicker:
    return "colorPicker";
  case WidgetPropertyUiHint::vec2Editor:
    return "vec2Editor";
  case WidgetPropertyUiHint::rectEditor:
    return "rectEditor";
  case WidgetPropertyUiHint::assetPicker:
    return "assetPicker";
  }

  return "auto";
}

inline juce::String colorStorageToKey(ColorStorage storage) {
  switch (storage) {
  case ColorStorage::hexString:
    return "hexString";
  case ColorStorage::rgbaObject255:
    return "rgbaObject255";
  case ColorStorage::rgbaObject01:
    return "rgbaObject01";
  case ColorStorage::hslaObject:
    return "hslaObject";
  case ColorStorage::argbInt:
    return "argbInt";
  case ColorStorage::token:
    return "token";
  }

  return "hexString";
}

inline juce::var stringArrayToVar(const juce::StringArray &values) {
  juce::Array<juce::var> array;
  array.ensureStorageAllocated(values.size());
  for (const auto &value : values) {
    const auto normalized = value.trim();
    if (normalized.isNotEmpty())
      array.add(normalized);
  }
  return juce::var(array);
}

inline juce::var assetKindsToVar(const std::vector<AssetKind> &values) {
  juce::Array<juce::var> array;
  array.ensureStorageAllocated(static_cast<int>(values.size()));
  for (const auto kind : values)
    array.add(assetKindToKey(kind));
  return juce::var(array);
}
inline void applyBuiltinDescriptorDefaults(WidgetDescriptor &descriptor,
                                           const juce::String &a11yRole,
                                           const juce::String &a11yLabelKey) {
  const auto normalizedTypeKey = descriptor.typeKey.trim();
  if (normalizedTypeKey.isNotEmpty()) {
    descriptor.typeKey = normalizedTypeKey;
    descriptor.pluginId = "com.dadeum.gyeol.builtin." + normalizedTypeKey;
  }

  descriptor.schemaVersion = "2.0.0";
  descriptor.manifestVersion = "1.0.0";
  descriptor.widgetTypeVersion = juce::jmax(1, descriptor.widgetTypeVersion);
  descriptor.pluginVersion = "1.0.0";
  descriptor.vendor = "Dadeum";
  descriptor.releaseChannel = "stable";
  descriptor.isExternalPlugin = false;
  descriptor.pluginBinaryPath.clear();
  descriptor.sdkMinVersion = "1.0.0";
  descriptor.abiVersion = "1";
  descriptor.supportedHostVersions = {"1.0.0"};
  descriptor.permissions.clear();
  descriptor.sandboxLevel = "strict";

  descriptor.requiredJuceModules = {"juce_gui_basics"};
  descriptor.requiredHeaders = {"<JuceHeader.h>"};
  descriptor.requiredLibraries.clear();
  descriptor.requiredCompileDefinitions.clear();
  descriptor.requiredLinkOptions.clear();
  descriptor.codegenApiVersion = "2.1";

  descriptor.a11yRole = a11yRole;
  descriptor.a11yLabelKey = a11yLabelKey;
}

inline juce::var serializeWidgetPropertySpecV2(const WidgetPropertySpec &spec) {
  auto object = std::make_unique<juce::DynamicObject>();
  object->setProperty("key", spec.key.toString());
  object->setProperty("label", spec.label);
  object->setProperty("kind", widgetPropertyKindToKey(spec.kind));
  object->setProperty("uiHint", widgetPropertyUiHintToKey(spec.uiHint));
  object->setProperty("required", spec.required);
  object->setProperty("group", spec.group);
  object->setProperty("order", spec.order);
  object->setProperty("hint", spec.hint);
  object->setProperty("unit", spec.unit);
  object->setProperty("displayFormat", spec.displayFormat);
  object->setProperty("valueCurve", spec.valueCurve);
  object->setProperty("defaultValue", spec.defaultValue);
  if (spec.minValue.has_value())
    object->setProperty("min", *spec.minValue);
  if (spec.maxValue.has_value())
    object->setProperty("max", *spec.maxValue);
  if (spec.step.has_value())
    object->setProperty("step", *spec.step);
  if (spec.minLength.has_value())
    object->setProperty("minLength", *spec.minLength);
  if (spec.maxLength.has_value())
    object->setProperty("maxLength", *spec.maxLength);
  object->setProperty("regex", spec.regex);
  object->setProperty("localeKey", spec.localeKey);
  object->setProperty("decimals", spec.decimals);
  object->setProperty("acceptedAssetKinds", assetKindsToVar(spec.acceptedAssetKinds));
  object->setProperty("acceptedMimeTypes", stringArrayToVar(spec.acceptedMimeTypes));
  if (spec.maxAssetBytes.has_value())
    object->setProperty("maxAssetBytes", static_cast<double>(*spec.maxAssetBytes));
  object->setProperty("fallbackAssetId", spec.fallbackAssetId);
  object->setProperty("preloadAssets", stringArrayToVar(spec.preloadAssets));
  object->setProperty("supportsStreaming", spec.supportsStreaming);
  object->setProperty("colorStorage", colorStorageToKey(spec.colorStorage));
  object->setProperty("colorAllowAlpha", spec.colorAllowAlpha);
  object->setProperty("colorAllowHdr", spec.colorAllowHdr);
  if (spec.dependsOnKey.has_value())
    object->setProperty("dependsOnKey", spec.dependsOnKey->toString());
  if (spec.dependsOnValue.has_value())
    object->setProperty("dependsOnValue", *spec.dependsOnValue);
  object->setProperty("advanced", spec.advanced);
  object->setProperty("readOnly", spec.readOnly);

  juce::Array<juce::var> enumOptions;
  enumOptions.ensureStorageAllocated(static_cast<int>(spec.enumOptions.size()));
  for (const auto &option : spec.enumOptions) {
    auto optionObject = std::make_unique<juce::DynamicObject>();
    optionObject->setProperty("value", option.value);
    optionObject->setProperty("label", option.label);
    enumOptions.add(juce::var(optionObject.release()));
  }
  object->setProperty("enumOptions", juce::var(enumOptions));

  return juce::var(object.release());
}

inline juce::var serializeRuntimeEventSpecV2(const RuntimeEventSpec &spec) {
  auto object = std::make_unique<juce::DynamicObject>();
  object->setProperty("key", spec.key);
  object->setProperty("displayLabel", spec.displayLabel);
  object->setProperty("description", spec.description);
  object->setProperty("continuous", spec.continuous);
  object->setProperty("payloadSchema",
                      spec.payloadSchema.isVoid() ? emptyRequiredPayloadSchema()
                                                  : spec.payloadSchema);
  if (spec.throttleMs.has_value())
    object->setProperty("throttleMs", *spec.throttleMs);
  if (spec.debounceMs.has_value())
    object->setProperty("debounceMs", *spec.debounceMs);
  object->setProperty("reliability", spec.reliability);
  object->setProperty("channel", spec.channel);
  return juce::var(object.release());
}

inline juce::var serializeWidgetDescriptorSchemaV2(
    const WidgetDescriptor &descriptor,
    const juce::String &codegenApiVersionOverride = {}) {
  auto object = std::make_unique<juce::DynamicObject>();

  object->setProperty("schemaVersion", descriptor.schemaVersion);
  object->setProperty("manifestVersion", descriptor.manifestVersion);
  object->setProperty("widgetTypeVersion", descriptor.widgetTypeVersion);
  if (!descriptor.migrationRules.isVoid())
    object->setProperty("migrationRules", descriptor.migrationRules);

  object->setProperty("abiVersion", descriptor.abiVersion);
  object->setProperty("abiHash", descriptor.abiHash);
  object->setProperty("sdkMinVersion", descriptor.sdkMinVersion);
  object->setProperty("sdkMaxVersion", descriptor.sdkMaxVersion);
  object->setProperty("supportedHostVersions",
                      stringArrayToVar(descriptor.supportedHostVersions));
  object->setProperty("platformTargets",
                      stringArrayToVar(descriptor.platformTargets));
  object->setProperty("architectureTargets",
                      stringArrayToVar(descriptor.architectureTargets));

  object->setProperty("pluginId", descriptor.pluginId);
  object->setProperty("pluginVersion", descriptor.pluginVersion);
  object->setProperty("vendor", descriptor.vendor);
  object->setProperty("releaseChannel", descriptor.releaseChannel);
  object->setProperty("isExternalPlugin", descriptor.isExternalPlugin);
  object->setProperty("pluginBinaryPath", descriptor.pluginBinaryPath);
  object->setProperty("publisherFingerprint", descriptor.publisherFingerprint);
  object->setProperty("signature", descriptor.signature);

  object->setProperty("typeKey", descriptor.typeKey);
  object->setProperty("displayName", descriptor.displayName);
  object->setProperty("category", descriptor.category);
  object->setProperty("tags", stringArrayToVar(descriptor.tags));
  object->setProperty("iconKey", descriptor.iconKey);

  auto defaultBounds = std::make_unique<juce::DynamicObject>();
  defaultBounds->setProperty("x", descriptor.defaultBounds.getX());
  defaultBounds->setProperty("y", descriptor.defaultBounds.getY());
  defaultBounds->setProperty("w", descriptor.defaultBounds.getWidth());
  defaultBounds->setProperty("h", descriptor.defaultBounds.getHeight());
  object->setProperty("defaultBounds", juce::var(defaultBounds.release()));

  auto minSize = std::make_unique<juce::DynamicObject>();
  minSize->setProperty("w", descriptor.minSize.x);
  minSize->setProperty("h", descriptor.minSize.y);
  object->setProperty("minSize", juce::var(minSize.release()));

  auto defaultProperties = std::make_unique<juce::DynamicObject>();
  for (int i = 0; i < descriptor.defaultProperties.size(); ++i)
    defaultProperties->setProperty(descriptor.defaultProperties.getName(i),
                                   descriptor.defaultProperties.getValueAt(i));
  object->setProperty("defaultProperties", juce::var(defaultProperties.release()));

  juce::Array<juce::var> propertySpecsArray;
  propertySpecsArray.ensureStorageAllocated(
      static_cast<int>(descriptor.propertySpecs.size()));
  for (const auto &propertySpec : descriptor.propertySpecs)
    propertySpecsArray.add(serializeWidgetPropertySpecV2(propertySpec));
  object->setProperty("propertySpecs", juce::var(propertySpecsArray));

  juce::Array<juce::var> runtimeEventsArray;
  runtimeEventsArray.ensureStorageAllocated(
      static_cast<int>(descriptor.runtimeEvents.size()));
  for (const auto &runtimeEvent : descriptor.runtimeEvents)
    runtimeEventsArray.add(serializeRuntimeEventSpecV2(runtimeEvent));
  object->setProperty("runtimeEvents", juce::var(runtimeEventsArray));

  object->setProperty("capabilities", stringArrayToVar(descriptor.capabilities));
  object->setProperty("repaintPolicy", descriptor.repaintPolicy);
  if (descriptor.tickRateHintHz.has_value())
    object->setProperty("tickRateHintHz", *descriptor.tickRateHintHz);
  object->setProperty("supportsOffscreenCache",
                      descriptor.supportsOffscreenCache);
  if (descriptor.estimatedPaintCost.has_value())
    object->setProperty("estimatedPaintCost", *descriptor.estimatedPaintCost);
  if (descriptor.memoryBudgetKb.has_value())
    object->setProperty("memoryBudgetKb", *descriptor.memoryBudgetKb);

  object->setProperty("threadingModel", descriptor.threadingModel);
  object->setProperty("realtimeSafe", descriptor.realtimeSafe);
  object->setProperty("renderThreadOnly", descriptor.renderThreadOnly);

  object->setProperty("supportedActions",
                      stringArrayToVar(descriptor.supportedActions));
  object->setProperty("propertyBindings",
                      stringArrayToVar(descriptor.propertyBindings));
  object->setProperty("stateInputs", stringArrayToVar(descriptor.stateInputs));
  object->setProperty("stateOutputs", stringArrayToVar(descriptor.stateOutputs));

  object->setProperty("a11yRole", descriptor.a11yRole);
  object->setProperty("a11yLabelKey", descriptor.a11yLabelKey);
  object->setProperty("testId", descriptor.testId);
  if (!descriptor.diagnosticsContract.isVoid())
    object->setProperty("diagnosticsContract", descriptor.diagnosticsContract);
  object->setProperty("telemetryTags", stringArrayToVar(descriptor.telemetryTags));

  object->setProperty("statePolicy", descriptor.statePolicy);
  object->setProperty("persistedKeys", stringArrayToVar(descriptor.persistedKeys));
  object->setProperty("resetPolicy", descriptor.resetPolicy);
  object->setProperty("migrationPolicy", descriptor.migrationPolicy);

  object->setProperty("permissions", stringArrayToVar(descriptor.permissions));
  object->setProperty("sandboxLevel", descriptor.sandboxLevel);
  object->setProperty("fileAccess", stringArrayToVar(descriptor.fileAccess));
  object->setProperty("networkAccess", stringArrayToVar(descriptor.networkAccess));
  object->setProperty("midiAccess", stringArrayToVar(descriptor.midiAccess));
  object->setProperty("scriptAccess", stringArrayToVar(descriptor.scriptAccess));

  object->setProperty("requiredJuceModules",
                      stringArrayToVar(descriptor.requiredJuceModules));
  object->setProperty("requiredHeaders",
                      stringArrayToVar(descriptor.requiredHeaders));
  object->setProperty("requiredLibraries",
                      stringArrayToVar(descriptor.requiredLibraries));
  object->setProperty("requiredCompileDefinitions",
                      stringArrayToVar(descriptor.requiredCompileDefinitions));
  object->setProperty("requiredLinkOptions",
                      stringArrayToVar(descriptor.requiredLinkOptions));
  object->setProperty("codegenApiVersion",
                      codegenApiVersionOverride.isNotEmpty()
                          ? codegenApiVersionOverride
                          : descriptor.codegenApiVersion);
  object->setProperty("exportTargetType", descriptor.exportTargetType);

  return juce::var(object.release());
}

class WidgetClass {
public:
  virtual ~WidgetClass() = default;
  virtual WidgetDescriptor makeDescriptor() const = 0;
};

using WidgetClassFactory = std::function<std::unique_ptr<WidgetClass>()>;

class WidgetClassCatalog {
public:
  static void registerFactory(WidgetClassFactory factory) {
    factories().push_back(std::move(factory));
  }

  static const std::vector<WidgetClassFactory> &allFactories() {
    return factories();
  }

private:
  static std::vector<WidgetClassFactory> &factories() {
    static std::vector<WidgetClassFactory> registered;
    return registered;
  }
};

template <typename WidgetClassType> class AutoWidgetClassRegistration {
public:
  explicit AutoWidgetClassRegistration(const char *debugName = nullptr) {
    WidgetClassCatalog::registerFactory([]() -> std::unique_ptr<WidgetClass> {
      return std::make_unique<WidgetClassType>();
    });

#if JUCE_DEBUG
    if (debugName != nullptr && debugName[0] != '\0')
      DBG("[Gyeol][WidgetSDK] Auto-registered widget class: " << debugName);
    else
      DBG("[Gyeol][WidgetSDK] Auto-registered unnamed widget class.");
#endif
  }
};

template <typename DescriptorVisitor>
void forEachRegisteredDescriptor(DescriptorVisitor &&visitor) {
  for (const auto &widgetFactory : WidgetClassCatalog::allFactories()) {
    if (!widgetFactory)
      continue;

    auto widgetClass = widgetFactory();
    if (widgetClass == nullptr)
      continue;

    visitor(widgetClass->makeDescriptor());
  }
}
} // namespace Gyeol::Widgets

#define GYEOL_WIDGET_INTERNAL_JOIN2(a, b) a##b
#define GYEOL_WIDGET_INTERNAL_JOIN(a, b) GYEOL_WIDGET_INTERNAL_JOIN2(a, b)

#define GYEOL_WIDGET_AUTOREGISTER(WidgetClassType)                             \
  inline const ::Gyeol::Widgets::AutoWidgetClassRegistration<WidgetClassType>  \
  GYEOL_WIDGET_INTERNAL_JOIN(gGyeolAutoRegister_, WidgetClassType) {           \
    #WidgetClassType                                                           \
  }
