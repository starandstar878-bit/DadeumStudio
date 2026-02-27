# Gyeol Export Data Contract (Phase 2 MVP)

## Purpose
This contract defines the files emitted by `Gyeol::Export::exportToJuceComponent(...)` and the schema of `export-manifest.json`.
`WidgetDescriptor::exportCodegen` is the first-class extension point for custom widget code generation.

## Output Files
- `<ComponentClassName>.h`: generated JUCE `Component` declaration and widget members.
- `<ComponentClassName>.cpp`: generated constructor + `resized()` layout.
- `export-manifest.json`: normalized export payload for tooling/inspection.
- `ExportReport.txt`: human-readable report with warnings/errors.
- `Resources/*`: copied resource files referenced by scene properties.

## Manifest Schema (`export-manifest.json`)
- `schemaVersion` (`int`): packed scene schema version (`major*10000 + minor*100 + patch`).
- `componentClassName` (`string`): generated C++ class name.
- `generatedAtUtc` (`string`): ISO-8601 UTC timestamp.
- `groupCount` (`int`): number of logical groups in source scene.
- `groupsFlattened` (`bool`): always `true` in MVP; groups are not emitted as container components.
- `widgets` (`array<object>`):
- `id` (`string`): widget id encoded as string.
- `typeKey` (`string`): registry type key.
- `exportTargetType` (`string`): registry export mapping target.
- `codegenKind` (`string`): concrete generator kind (`juce_text_button`, `juce_slider_linear`, etc).
- `usesCustomCodegen` (`bool`): `true` when descriptor-provided `exportCodegen` generated the widget code.
- `memberName` (`string`): generated member variable name.
- `supported` (`bool`): `false` means fallback UI code was generated.
- `bounds` (`object`): `{x, y, w, h}` as float.
- `properties` (`object`): serialized widget `PropertyBag`.
- `copiedResources` (`array<object>`):
- `widgetId` (`string`)
- `propertyKey` (`string`)
- `sourcePath` (`string`)
- `destinationPath` (`string`) relative to export output directory when available.

## Resource Copy Rule (MVP)
- Only `string` properties are considered.
- Property key must look resource-like (`*image*`, `*asset*`, `*path`).
- Value must look path-like (`/`, `\`, or `.` included).
- Relative paths are resolved from `ExportOptions::projectRootDirectory`.

## Report Rule
- `ExportReport.txt` is always emitted on successful write path.
- Each issue is recorded with severity: `INFO`, `WARN`, `ERROR`.
- Export function returns failure when any hard error occurs.

## Codegen Resolution Order
1. If `WidgetDescriptor::exportCodegen` exists and succeeds, exporter uses that output.
2. Otherwise exporter falls back to built-in `exportTargetType` mapping.
3. If both are unavailable, exporter emits an unsupported fallback label and warning.

## Group Export Rule (Phase 3 step2)
- Export path keeps existing MVP behavior: widgets are emitted in flat order.
- Group metadata is not converted into parent/child JUCE components.
- Export report logs an `INFO` issue when source scene contains groups.

## Plugin ABI Alignment
- C plugin ABI is extended to `v1.1` in `GyeolWidgetPluginABI.h`.
- `GyeolWidgetDescriptor::export_codegen` is optional and mirrors `WidgetDescriptor::exportCodegen`.
- C ABI callback returns:
- `member_type`
- `codegen_kind`
- `constructor_lines_json` (`array<string>`)
- `resized_lines_json` (`array<string>`)
