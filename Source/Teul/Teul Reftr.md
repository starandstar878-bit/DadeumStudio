# Teul Reftr

> Purpose: plan how the current `Source/Teul` folders and logic move into the new target structure.
> Target structure baseline: `Document`, `Editor`, `Runtime/AudioGraph`, `Runtime/IOControl`, `Bridge`.

---

## Current To Target Folder Mapping

| Current folder | Current responsibility | Target folder(s) | Migration rule |
| --- | --- | --- | --- |
| `Model/` | Core graph data, node/port/connection/control state, document state | `Document/` | Move document-owned structures into `Document`. This is the cleanest one-to-one move. |
| `History/` | Undo/redo command and history stack logic | `Document/` | Fold history behavior into `TDocumentHistory`. Keep only document-lifecycle history here. |
| `Serialization/` | `.teul` save/load, JSON, file IO, preset/state IO, migration-adjacent logic | `Document/`, `Bridge/` | Document save/load and `.teul` parsing stay in `Document`. External-system payload JSON moves to `Bridge`. |
| `Editor/` | Canvas, panels, visuals, interaction, editor orchestration | `Editor/` | Keep in `Editor`, but replace `EditorHandle/Impl` structure with `TTeulEditor` and split renderer/interaction responsibilities. |
| `Runtime/` | Graph build/process, runtime state, MIDI/control/audio routing | `Runtime/AudioGraph`, `Runtime/IOControl` | Split DSP graph execution into `AudioGraph` and device/runtime-event handling into `IOControl`. |
| `Bridge/` | Param provider bridge and external-facing adapter logic | `Bridge/` | Keep in `Bridge`, but narrow the role to external integration only. |
| `Export/` | Code/output generation for external consumption | `Bridge/` | Move into `TBridgeCodegen` because export is an external-consumption concern. |
| `Preset/` | Preset catalog and preset-facing access logic | `Document/`, `Editor/` | Preset file/data ownership goes to `Document`. Preset browsing UI support stays with `Editor`. |
| `Registry/` | Node descriptors, registry, node factories, node metadata | `Document/`, `Editor/`, `Runtime/AudioGraph` | Split by concern: document-safe node metadata to `Document`, editor-facing library/search metadata to `Editor`, runtime factory/build logic to `Runtime/AudioGraph`. |
| `Public/` | Public entry headers | `Editor/`, `Runtime/`, `Bridge/` | Remove the separate public bucket. Keep public entry files next to their owning layer (`TTeulEditor`, `TTeulRuntime`, `TTeulBridge`). |
| `Verification/` | Runtime validation, parity/golden/stress/benchmark support | `Runtime/AudioGraph`, `Editor/` | Runtime validation criteria and validator entry move to `Runtime/AudioGraph`. UI display of results stays in `Editor`. Full verification assets may remain as support data outside the minimum runtime set. |

---

## Notes On Folders With No Direct One-To-One Move

### `Registry/`
`Registry` does not cleanly map to a single new folder.

Planned split:
- Document-facing node type metadata -> `Document/`
- Editor-facing library/search/category metadata -> `Editor/`
- Runtime-facing node creation/build logic -> `Runtime/AudioGraph/`

### `Preset/`
`Preset` also does not map to a single new folder.

Planned split:
- Preset file/data ownership -> `Document/`
- Preset browser and preset interaction UI -> `Editor/`

### `Public/`
The new structure should not keep a separate `Public/` bucket.

Planned result:
- Public editor entry -> `Editor/TTeulEditor.h`
- Public runtime entry -> `Runtime/TTeulRuntime.h`
- Public bridge entry -> `Bridge/TTeulBridge.h`

### `Verification/`
Verification is partly runtime-facing and partly tooling/support.

Planned split:
- Runtime validator logic -> `Runtime/AudioGraph/TRuntimeValidator`
- Editor diagnostics presentation -> `Editor/Panels/DiagnosticsDrawer`
- Heavy verification suites/artifacts can remain as non-minimum support assets until phase 2

---

## Folder-Level Migration Order

1. `Model/` -> `Document/`
2. `History/` -> `Document/`
3. `Serialization/` -> `Document/` and `Bridge/`
4. `Editor/` -> `Editor/` reshaping around `TTeulEditor`
5. `Runtime/` -> `Runtime/AudioGraph` and `Runtime/IOControl`
6. `Export/` -> `Bridge/`
7. `Bridge/` -> narrowed `Bridge/`
8. `Preset/`, `Registry/`, `Public/`, `Verification/` -> split after the core moves are stable

---

## Next Step

After this folder table is accepted, the next refactoring document step should be a file-level mapping table.

Suggested columns:
- Current file
- Current responsibility
- Target file
- Move / split / delete / keep
- Refactor risk

---

## Registry File-Level Mapping

`Registry/` should not survive as a single folder. The file-level plan below uses the current code in `Source/Teul/Registry` as the basis.

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Registry/TNodeRegistry.h` | Mixed node descriptor types, param/port spec types, registry API, editor-facing metadata, bridge-facing export metadata, runtime factory hook | `Document/`, `Runtime/AudioGraph/`, `Bridge/` | `split` | Move document-safe descriptor/spec types into `Document`. Move runtime factory-facing pieces into `Runtime/AudioGraph`. Keep external exposure metadata only where `Bridge` needs it. |
| `Registry/TNodeRegistry.cpp` | Descriptor normalization, registry storage/lookup, exposed-param fallback generation, default registry bootstrap | `Document/`, `Runtime/AudioGraph/`, `Bridge/` | `split` | `registerNode` and descriptor lookup become the shared descriptor registry. Exposed-param fallback generation moves to `Bridge`. Default factory bootstrap moves to `Runtime/AudioGraph`. |
| `Registry/TNodeSDK.h` | Node class registration, auto-registration macro, runtime factory catalog | `Runtime/AudioGraph/` | `move` | This is runtime factory infrastructure. It does not belong in `Document` or `Editor`. |
| `Registry/Nodes/CoreNodes.h` | Aggregates core node headers so core nodes register into the factory catalog | `Runtime/AudioGraph/` | `move` | Keep as runtime bootstrap glue for built-in nodes. |
| `Registry/Nodes/Core/SourceNodes.h` | Built-in source node descriptors and DSP implementations | `Runtime/AudioGraph/` | `move now, split later` | Move into runtime first. Later split descriptor metadata from DSP implementation only if the file becomes too large. |
| `Registry/Nodes/Core/FilterNodes.h` | Built-in filter node descriptors and DSP implementations | `Runtime/AudioGraph/` | `move now, split later` | Same rule as other built-in node files. Runtime owns the executable node classes. |
| `Registry/Nodes/Core/FXNodes.h` | Built-in FX node descriptors and DSP implementations | `Runtime/AudioGraph/` | `move now, split later` | Keep node class and implementation together in phase 1, then split only if needed. |
| `Registry/Nodes/Core/MathLogicNodes.h` | Built-in math/logic node descriptors and DSP implementations | `Runtime/AudioGraph/` | `move now, split later` | Runtime-owned built-in nodes. Descriptor extraction can be a later cleanup step. |
| `Registry/Nodes/Core/MidiNodes.h` | Built-in MIDI node descriptors and runtime MIDI processing implementations | `Runtime/AudioGraph/` | `move now, split later` | Runtime owns both the executable MIDI behavior and the compile-time factory registration. |
| `Registry/Nodes/Core/MixerNodes.h` | Built-in mixer node descriptors and DSP implementations | `Runtime/AudioGraph/` | `move now, split later` | Same as other built-in runtime nodes. |
| `Registry/Nodes/Core/ModulationNodes.h` | Built-in modulation node descriptors and DSP implementations | `Runtime/AudioGraph/` | `move now, split later` | Runtime owns the executable modulation graph nodes. |

### Registry Split Principle

- `Registry` does not move as one folder.
- Shared node description data moves toward `Document`.
- Runtime node factory registration and built-in node implementations move to `Runtime/AudioGraph`.
- External exposure helpers that build bridge-facing parameter metadata move to `Bridge`.
- Editor should consume node library/category metadata from the shared descriptor layer instead of owning a separate registry implementation.

### Registry Migration Order

1. Move `TNodeSDK.h` and `Nodes/Core*.h` into `Runtime/AudioGraph` first.
2. Split `TNodeRegistry.h/.cpp` into shared descriptor storage and runtime bootstrap pieces.
3. Move bridge-only exposed-param fallback helpers out of the registry code.
4. Update `Editor` so it reads node-library/category data from the new shared descriptor layer.

---

## Serialization File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Serialization/TSerializer.h` | `.teul` document JSON schema, migration report, document JSON encode/decode API | `Document/` | `move` | This is document import/export and schema migration, not bridge JSON. |
| `Serialization/TSerializer.cpp` | Document JSON serialization, legacy alias handling, schema migration steps, field-level encode/decode | `Document/` | `move` | Split only if the migration helpers become too large; otherwise keep under the document layer. |
| `Serialization/TFileIo.h` | `.teul` file save/load facade | `Document/` | `move` | This becomes the document file store entry point. |
| `Serialization/TFileIo.cpp` | `.teul` file IO plus transient document notice side effects | `Document/` | `move now, trim later` | Move into `Document` first. If desired later, move notice formatting out so file IO stays pure. |
| `Serialization/TPatchPresetIO.h` | Patch preset file format, schema versioning, frame extraction/import API | `Document/` | `move` | Patch presets are document-level assets because they serialize graph fragments. |
| `Serialization/TPatchPresetIO.cpp` | Patch preset JSON codec, migration, graph-fragment extraction, graph insertion | `Document/` | `move` | Keep with document lifecycle code. The insertion logic is document mutation, not editor UI. |
| `Serialization/TStatePresetIO.h` | State preset format, diff preview/apply API | `Document/` | `move` | State presets are document-owned snapshots. |
| `Serialization/TStatePresetIO.cpp` | State preset JSON codec, migration, diff/preview/apply logic, document notices | `Document/` | `move now, trim later` | Move into `Document` first. Preview/apply can remain together until a dedicated preset service is needed. |

### Serialization Split Principle

- `.teul`, `.teulpatch`, and `.teulstate` parsing belongs to `Document`.
- External-system JSON does not belong here and should be handled by `Bridge` later.
- File IO can keep small document-notice hooks in phase 1, but those hooks should not spread back into `Editor`.

---

## Preset File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Preset/TPresetCatalog.h` | Preset browser entry model, provider abstraction, favorite/recent/tag state, catalog facade | `Document/`, `Editor/` | `split` | Entry/provider/state persistence fit `Document`. Browser-facing catalog presentation and filtering should be consumed by `Editor`. |
| `Preset/TPresetCatalog.cpp` | Patch/state/recovery providers, preset browser state file IO, preset summary/detail formatting, sorting | `Document/`, `Editor/` | `split` | Provider-side file discovery and persistence move to `Document`. Human-readable summary/detail formatting and browser ordering rules move to `Editor`. |

### Preset Split Principle

- Preset files and recovery snapshots are document-owned assets.
- Favorite, recent, and tag persistence can stay near the document-side preset store.
- Preset card text, warning text, and browser ordering are editor presentation concerns.
- The current single catalog file should become a document-side preset source plus an editor-side browser/view-model layer.

---

## Verification File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Verification/TVerificationFixtures.h` | Representative graph fixture declarations | `Runtime/AudioGraph/`, `Document/` | `move now, trim later` | Phase 1 can keep fixtures under runtime verification support. They still construct documents, but they exist to test runtime behavior. |
| `Verification/TVerificationFixtures.cpp` | Representative graph fixture construction | `Runtime/AudioGraph/`, `Document/` | `move now, trim later` | Keep next to runtime verification helpers first. |
| `Verification/TVerificationStimulus.h` | Render profile, automation, MIDI stimulus spec, render result, render helper API | `Runtime/AudioGraph/` | `move` | This is directly tied to runtime rendering behavior. |
| `Verification/TVerificationStimulus.cpp` | Stimulus builders and render execution helpers | `Runtime/AudioGraph/` | `move` | Runtime verification support. |
| `Verification/TVerificationParity.h` | Editable export round-trip parity test API and reports | `Runtime/AudioGraph/`, `Bridge/` | `split` | Runtime-side render/compare logic stays with runtime verification. Export round-trip hooks depend on bridge/export code. |
| `Verification/TVerificationParity.cpp` | Export parity implementation and artifact generation | `Runtime/AudioGraph/`, `Bridge/` | `split` | Separate compare/render logic from export/import round-trip helpers. |
| `Verification/TVerificationCompiledParity.h` | Compiled runtime parity report and API | `Runtime/AudioGraph/`, `Bridge/` | `split` | Runtime parity stays with runtime verification; external codegen/compile/run orchestration touches bridge/export. |
| `Verification/TVerificationCompiledParity.cpp` | Compiled runtime parity implementation | `Runtime/AudioGraph/`, `Bridge/` | `split` | Same split as above. |
| `Verification/TVerificationGoldenAudio.h` | Golden-audio record/verify API and reports | `Runtime/AudioGraph/` | `move` | This is runtime audio validation. |
| `Verification/TVerificationGoldenAudio.cpp` | Golden-audio record/verify implementation | `Runtime/AudioGraph/` | `move` | Runtime-owned verification support. |
| `Verification/TVerificationStress.h` | Stress/soak suite API and reports | `Runtime/AudioGraph/` | `move` | Runtime stability verification. |
| `Verification/TVerificationStress.cpp` | Stress/soak suite implementation | `Runtime/AudioGraph/` | `move` | Runtime stability verification. |
| `Verification/TVerificationBenchmark.h` | Benchmark thresholds, reports, benchmark suite API | `Runtime/AudioGraph/` | `move` | Runtime performance verification. |
| `Verification/TVerificationBenchmark.cpp` | Benchmark suite implementation | `Runtime/AudioGraph/` | `move` | Runtime performance verification. |
| `Verification/CompiledParity/CompiledParityCaseHost.cpp` | Helper executable host for compiled parity cases | `Bridge/`, `Runtime/AudioGraph/` | `split support` | Keep as support tooling. It depends on generated/exported runtime code and parity harness behavior. |
| `Verification/GoldenAudio/**` | Golden reference assets, metadata, bundle manifests | `Runtime/AudioGraph/` | `keep support assets` | These are test artifacts, not production runtime code. Keep them in a verification-support bucket during phase 1. |

### Verification Split Principle

- Verification code is not an end-user runtime layer, but its logic mostly validates `Runtime/AudioGraph`.
- Export/codegen round-trip verification is the one area that legitimately touches both `Runtime/AudioGraph` and `Bridge`.
- Golden audio files and compiled parity hosts are support assets/tools and do not need to become production-layer source files.

### Verification Migration Order

1. Move stimulus, golden-audio, stress, and benchmark helpers under runtime verification support.
2. Split parity and compiled-parity files into runtime compare logic and bridge/export orchestration.
3. Leave large artifact bundles and helper executables as support assets until the production layers are stable.

