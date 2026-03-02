# Gyeol Export Data Contract (Phase 6)

## Purpose
This contract defines files emitted by `Gyeol::Export::exportToJuceComponent(...)` and the schema of `export-manifest.json`.
Phase 6 extends export payloads so runtime behavior from editor Phase 4/5 can be reconstructed in exported outputs.

## Output Files
- `<ComponentClassName>.h`: generated JUCE `Component` declaration and widget members.
- `<ComponentClassName>.cpp`: generated constructor + `resized()` layout.
- `export-manifest.json`: normalized export payload for tooling/inspection/runtime bootstrap.
- `export-runtime.json`: runtime-only payload (`runtimeParams/propertyBindings/runtimeBindings`).
- `ExportReport.txt`: human-readable export report (`INFO/WARN/ERROR`).
- `Assets/*`: copied asset binaries resolved from `DocumentModel.assets`.

## Manifest Schema (`export-manifest.json`)

### Root Fields
- `manifestVersion` (`string`): export contract version (`"2.0"` in Phase 6).
- `schemaVersion` (`int`): packed scene schema version (`major*10000 + minor*100 + patch`) for compatibility.
- `documentSchemaVersion` (`object`): `{ major, minor, patch, packed }`.
- `componentClassName` (`string`): generated C++ class name.
- `generatedAtUtc` (`string`): ISO-8601 UTC timestamp.
- `groupCount` (`int`): number of groups in source scene.
- `groupsFlattened` (`bool`): currently `true`.
- `runtimeDataFile` (`string`, optional): filename of runtime payload (`export-runtime.json`).
- `widgets` (`array<object>`): exported widget descriptors.
- `assets` (`array<object>`): exported asset copy results (canonical key).
- `runtimeParams` (`array<object>`): runtime parameter definitions.
- `propertyBindings` (`array<object>`): property-binding definitions.
- `runtimeBindings` (`array<object>`): event-action binding definitions.
- `exportedAssets` (`array<object>`): legacy mirror of `assets`.
- `copiedResources` (`array<object>`): legacy mirror of `assets`.

### `widgets[]`
- `id` (`string`)
- `typeKey` (`string`)
- `exportTargetType` (`string`)
- `codegenKind` (`string`)
- `usesCustomCodegen` (`bool`)
- `memberName` (`string`)
- `supported` (`bool`)
- `bounds` (`object`): `{ x, y, w, h }` float
- `properties` (`object`): serialized widget `PropertyBag`

### `assets[]`
- `assetId` (`string`)
- `refKey` (`string`)
- `kind` (`string`)
- `mime` (`string`)
- `sourcePath` (`string`)
- `destinationPath` (`string`)
- `exportPath` (`string`): canonical export-relative path (`Assets/...`)
- `copied` (`bool`)
- `reused` (`bool`)

### `runtimeParams[]`
- `key` (`string`)
- `type` (`string`): `number | boolean | string`
- `defaultValue` (`number | bool | string`)
- `description` (`string`)
- `exposed` (`bool`)

### `propertyBindings[]`
- `id` (`string`)
- `name` (`string`)
- `enabled` (`bool`)
- `targetWidgetId` (`string`)
- `targetProperty` (`string`)
- `expression` (`string`)

### `runtimeBindings[]`
- `id` (`string`)
- `name` (`string`)
- `enabled` (`bool`)
- `sourceWidgetId` (`string`)
- `eventKey` (`string`)
- `actions` (`array<object>`)

### `runtimeBindings[].actions[]`
- `kind` (`string`)
- `setRuntimeParam`: `paramKey`, `value`
- `adjustRuntimeParam`: `paramKey`, `delta`
- `toggleRuntimeParam`: `paramKey`
- `setNodeProps`: `targetKind`, `targetId`, optional `visible/locked/opacity`, `patch`
- `setNodeBounds`: `targetWidgetId`, `bounds`

## Deterministic Output Rules
- `widgets` are sorted by `id` (then `memberName`) when written to manifest.
- `assets` are sorted by `assetId` (then case-insensitive `refKey`) when written to manifest.
- `runtimeParams` are sorted by case-insensitive `key` (then original `key`).
- `propertyBindings` are sorted by `id` (then `targetWidgetId`).
- `runtimeBindings` are sorted by `id` (then `sourceWidgetId`, then case-insensitive `eventKey`).
- `runtimeBindings[].actions` preserve original authoring order.

## Asset Copy Rules
- Exporter creates/refreshes output `Assets/` directory per run.
- Copy source is `DocumentModel.assets` (`image/font/file` kinds).
- `colorPreset` is metadata-only and skipped.
- Relative source paths resolve against `ExportOptions::projectRootDirectory`.
- Destination tries to preserve relative subpath under `Assets/`.
- Filename collisions are resolved with suffix (`name`, `name_2`, ...).
- Missing files are recorded as `WARN` and do not hard-fail export.

## Report Rules
- `ExportReport.txt` is always emitted on successful write path.
- Each issue uses severity `INFO`, `WARN`, or `ERROR`.
- Report includes assets summary (`copied/reused/skipped/missing/failed`).
- Export returns failure when hard errors occur.

## Codegen Resolution Order
1. If `WidgetDescriptor::exportCodegen` exists and succeeds, use it.
2. Otherwise use built-in `exportTargetType` mapping.
3. If both unavailable, emit unsupported fallback label + warning.

## Group Export Rule
- Export remains flattened (widgets only); groups are metadata.
- Export report emits an `INFO` message when groups exist.

## ABI Alignment
- C plugin ABI remains compatible with widget export codegen extension points.
- `export_codegen` callback output maps to:
  - `member_type`
  - `codegen_kind`
  - `constructor_lines_json`
  - `resized_lines_json`
