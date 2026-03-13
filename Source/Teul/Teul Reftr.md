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

