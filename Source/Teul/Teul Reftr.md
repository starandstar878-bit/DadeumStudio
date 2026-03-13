# Teul Reftr

> Purpose: plan how the current `Source/Teul` folders and logic move into the new target structure.
> Target structure baseline: `Document`, `Editor`, `Runtime/AudioGraph`, `Runtime/IOControl`, `Bridge`.

---

## Current To Target Folder Mapping

| Current folder | Current responsibility | Target folder(s) | Migration rule |
| --- | --- | --- | --- |
| `Model/` | Core graph data, node/port/connection/control state, document state | `Document/` | Move document-owned structures into `Document`. This is the cleanest one-to-one move. |
| `History/` | Undo/redo command and history stack logic | `Document/` | Fold history behavior into `TDocumentHistory`. Keep only document-lifecycle history here. |
| `Serialization/` | `.teul` save/load, JSON, file IO, preset/state IO, migration-adjacent logic | `Document/` | The current files are document-owned serialization and preset codecs. External-system JSON belongs to `Bridge` later, but it is not what lives here today. |
| `Editor/` | Canvas, panels, visuals, interaction, editor orchestration | `Editor/` | Keep in `Editor`, but replace `EditorHandle/Impl` structure with `TTeulEditor` and split renderer/interaction responsibilities. |
| `Runtime/` | Graph build/process, runtime state, MIDI/control/audio routing | `Runtime/AudioGraph`, `Runtime/IOControl` | Split DSP graph execution into `AudioGraph` and device/runtime-event handling into `IOControl`. |
| `Bridge/` | Param provider bridge and external-facing adapter logic | `Bridge/` | Keep in `Bridge`, but narrow the role to external integration only. |
| `Export/` | Code/output generation for external consumption | `Bridge/` | Move into `TBridgeCodegen` because export is an external-consumption concern. |
| `Preset/` | Preset catalog and preset-facing access logic | `Document/`, `Editor/` | Preset file/data ownership goes to `Document`. Preset browsing UI support stays with `Editor`. |
| `Registry/` | Node descriptors, registry, node factories, node metadata | `Document/`, `Editor/`, `Runtime/AudioGraph` | Split by concern: document-safe node metadata to `Document`, editor-facing library/search metadata to `Editor`, runtime factory/build logic to `Runtime/AudioGraph`. |
| `Public/` | Public entry headers | `Editor/`, `Runtime/`, `Bridge/` | Do not refactor `Public/` as its own target. Replace `Public/EditorHandle.h` with `Editor/TTeulEditor.h`, and keep future public entry files next to their owning layer. |
| `Verification/` | Runtime validation, parity/golden/stress/benchmark support | `Runtime/AudioGraph`, `Bridge/` | Almost all verification logic falls into `Runtime/AudioGraph`, with parity/codegen verification crossing into `Bridge`. Heavy assets remain support data outside the minimum production set. |

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
`Public/` is not an active split target.

Planned result:
- `Public/EditorHandle.h` is replaced by `Editor/TTeulEditor.h`
- Future public runtime entry lives at `Runtime/TTeulRuntime.h`
- Future public bridge entry lives at `Bridge/TTeulBridge.h`

### `Verification/`
Verification is mostly runtime-facing plus export/codegen tooling support.

Planned split:
- Runtime validation logic -> `Runtime/AudioGraph/*`
- Export/codegen parity logic -> `Bridge/*` plus runtime harness support
- Heavy verification suites/artifacts remain non-minimum support assets until phase 2

---

## Folder-Level Migration Order

1. `Model/` -> `Document/`
2. `History/` -> `Document/`
3. `Serialization/` -> `Document/`
4. `Editor/` -> `Editor/` reshaping around `TTeulEditor`
5. `Runtime/` -> `Runtime/AudioGraph` and `Runtime/IOControl`
6. `Export/` -> `Bridge/`
7. `Bridge/` -> narrowed `Bridge/`
8. `Preset/`, `Registry/`, `Verification/` -> split after the core moves are stable

---

## Approval Gate

Current status:
- Folder-level mapping is complete.
- File-level mapping is complete for `Registry`, `Serialization`, `Preset`, `Verification`, `Model`, `History`, `Editor`, `Runtime`, `Bridge`, and `Export`.
- `Public/EditorHandle.h` replacement is defined, but `Public/` is not treated as its own migration stream.

Ready for:
- Refactoring approval
- Phase 1 file moves into `Source/Teul2`
- Phase 2 type and responsibility splits where a single file maps to multiple target files

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

---

## Model File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Model/TTypes.h` | Core ID types, port direction, port data type enums | `Document/TDocumentTypes.h` | `move` | These are base document types and should be shared from the document layer. |
| `Model/TPort.h` | Port data model | `Document/TDocumentTypes.h` | `merge` | Fold into the shared document type file with the rest of the graph structs. |
| `Model/TNode.h` | Node data model plus inline port lookup helpers | `Document/TDocumentTypes.h` | `merge` | Keep the struct in the document type layer. Inline helpers can remain in the header. |
| `Model/TConnection.h` | Endpoint and connection data model, rail endpoint references | `Document/TDocumentTypes.h` | `merge` | Endpoint ownership and connection shape are core document data. |
| `Model/TGraphDocument.h` | Graph document aggregate, frame/bookmark/control-source state, revisions, history hooks, transient notices | `Document/TTeulDocument.h`, `Document/TDocumentTypes.h`, `Document/TDocumentHistory.h` | `split` | Move plain structs such as frames, bookmarks, rail/control types into `TDocumentTypes.h`. Keep the owning document object in `TTeulDocument.h`. History-facing method declarations move toward `TDocumentHistory`. |
| `Model/TGraphDocument.cpp` | Document construction/copy, history execution, revision bumping, cycle detection | `Document/TTeulDocument.cpp`, `Document/TDocumentHistory.cpp` | `split` | Keep document lifecycle and validation in `TTeulDocument.cpp`. Move history stack execution glue into `TDocumentHistory.cpp`. |

### Model Split Principle

- `Model` becomes the core of `Document`.
- Plain graph/control structs go into `TDocumentTypes.h`.
- The owning mutable document object becomes `TTeulDocument`.
- Inline history ownership inside the document should be reduced so the document is not also the history subsystem.

---

## History File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `History/TCommand.h` | Abstract undo/redo command interface plus inline history stack implementation | `Document/TDocumentHistory.h`, `Document/TDocumentHistory.cpp` | `split` | The `TCommand` abstraction and history stack belong to the document history layer. Move implementation out of the header. |
| `History/TCommands.h` | Concrete graph-edit commands (add/delete node, add/delete connection, move node, set param) | `Document/TDocumentHistory.cpp` | `merge` | These are document-history implementation details. They do not need to survive as their own public file in the minimum structure. |

### History Split Principle

- History is a document concern, not a separate top-level subsystem.
- Command definitions can stay internal to `TDocumentHistory.cpp` in phase 1.
- If command count grows later, split by command domain inside `Document`, not back into a top-level `History/` folder.

---

## Editor File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Public/EditorHandle.h` | Public editor entry facade | `Editor/TTeulEditor.h` | `replace` | This is the public header being replaced. Do not keep a separate `Public/` bucket. |
| `Editor/EditorHandle.cpp` | Thin facade forwarding, paint/resized, export dry-run entry | `Editor/TTeulEditor.cpp` | `move now, trim later` | Keep only top-level editor orchestration here. Export dry-run entry may later move closer to bridge-facing tooling if desired. |
| `Editor/EditorHandleImpl.h` | Internal editor composition, runtime/document sync, device/profile queue orchestration | `Editor/TTeulEditor.h`, `Editor/TTeulEditor.cpp` | `split` | Collapse `Handle/Impl` into `TTeulEditor`. Keep orchestration only; move panel/canvas/business helpers out. |
| `Editor/EditorHandleImpl.cpp` | Mixed orchestration, panel classes, rail UI, control adapter logic, runtime status UI | `Editor/TTeulEditor.cpp`, `Editor/Canvas/*`, `Editor/Panels/*`, `Editor/Search/*`, `Editor/Interaction/*` | `split` | This is the largest mixed-responsibility file and should be dismantled by concern. |
| `Editor/TIssueState.h` | Shared issue-state enum and color/label helpers | `Editor/TIssueState.h` | `move` | Keep as-is under the editor layer. |
| `Editor/Theme/TeulPalette.h` | Editor color palette and theme helpers | `Editor/Theme/TeulPalette.h` | `move` | Direct one-to-one move. |
| `Editor/Canvas/TGraphCanvas.h` | Main canvas component, graph view state, drag/select/search/context entry points | `Editor/Canvas/TGraphCanvas.h` | `move` | This is still the right home for the canvas shell. |
| `Editor/Canvas/TGraphCanvas.cpp` | Main canvas implementation, input handling, component rebuild/layout | `Editor/Canvas/TGraphCanvas.cpp` | `move now, trim later` | Keep canvas ownership and high-level orchestration here. Renderer-heavy logic should leave the file. |
| `Editor/Canvas/CanvasRenderers.cpp` | Grid/frame/connection/minimap/runtime overlay rendering | `Editor/Canvas/TGraphCanvas.cpp`, `Editor/Renderers/TConnectionRenderer.*`, `Editor/Renderers/TNodeRenderer.*`, `Editor/Renderers/TPortRenderer.*` | `split` | Connection drawing belongs in `TConnectionRenderer`. Port/node-specific geometry and visuals should move into the dedicated renderers. Canvas-only overlays can remain in `TGraphCanvas.cpp`. |
| `Editor/Node/TNodeComponent.h` | Node view component API, port ownership, node-local layout | `Editor/Renderers/TNodeRenderer.*`, `Editor/Canvas/TGraphCanvas.cpp` | `split` | The visual/layout part becomes `TNodeRenderer`. Canvas-owned interaction hooks stay with the canvas. |
| `Editor/Node/TNodeComponent.cpp` | Node painting, port layout, node-local interaction forwarding | `Editor/Renderers/TNodeRenderer.*`, `Editor/Canvas/TGraphCanvas.cpp` | `split` | Same split as above. |
| `Editor/Node/NodePreviewRenderer.h` | Inline node preview API | `Editor/Renderers/TNodeRenderer.h` | `merge` | Fold into the node renderer surface. |
| `Editor/Node/NodePreviewRenderer.cpp` | Inline preview drawing helpers for oscillator, ADSR, meter, etc. | `Editor/Renderers/TNodeRenderer.cpp` | `merge` | Keep preview rendering as part of the node renderer implementation. |
| `Editor/Port/TPortComponent.h` | Port view component, hit testing, bundle state, issue highlighting | `Editor/Renderers/TPortRenderer.*` | `split` | The renderer owns the visuals and hit geometry; component-level state can shrink into renderer helpers. |
| `Editor/Port/TPortComponent.cpp` | Port painting and interaction forwarding | `Editor/Renderers/TPortRenderer.*` | `merge` | Move into the dedicated port renderer implementation. |
| `Editor/Port/TPortVisuals.h` | Port overlay drawing helpers | `Editor/Renderers/TPortRenderer.*` | `merge` | Keep inside the port renderer implementation instead of a separate utility header. |
| `Editor/Port/TPortShapeLayout.h` | Port geometry and hit-shape helpers | `Editor/Renderers/TPortRenderer.*` | `merge` | This is renderer-local geometry logic. |
| `Editor/Interaction/SelectionController.cpp` | Node/frame selection, duplicate/delete/disconnect/bypass actions | `Editor/Interaction/SelectionController.cpp` | `move` | Direct move. |
| `Editor/Interaction/ConnectionInteraction.cpp` | Wire drag, drop-target validation, connection creation/deletion animations | `Editor/Interaction/ConnectionInteraction.cpp` | `move` | Direct move. |
| `Editor/Interaction/ContextMenuController.cpp` | Canvas/node/frame context menus, preset save/load actions, replace-node flow | `Editor/Interaction/ContextMenuController.cpp` | `move now, trim later` | Direct move first. Preset save/load actions may later delegate to a document-side preset service. |
| `Editor/Interaction/AlignmentEngine.cpp` | Align/distribute selection helpers | `Editor/Interaction/SelectionController.cpp` | `merge` | This is selection-manipulation behavior, not a separate subsystem in the minimum structure. |
| `Editor/Interaction/NodeFrameInteraction.cpp` | Node drag, frame drag, click selection routing | `Editor/Canvas/TGraphCanvas.cpp`, `Editor/Interaction/SelectionController.cpp` | `split` | Raw mouse routing stays with the canvas. Selection consequences stay with the selection controller. |
| `Editor/Panels/NodeLibraryPanel.h` | Node library panel API | `Editor/Panels/NodeLibraryPanel.h` | `move` | Direct move. |
| `Editor/Panels/NodeLibraryPanel.cpp` | Node library panel implementation | `Editor/Panels/NodeLibraryPanel.cpp` | `move` | Direct move. |
| `Editor/Panels/NodePropertiesPanel.h` | Node properties panel API | `Editor/Panels/NodePropertiesPanel.h` | `move` | Direct move. |
| `Editor/Panels/NodePropertiesPanel.cpp` | Node/frame inspector implementation | `Editor/Panels/NodePropertiesPanel.cpp` | `move now, merge helpers` | Merge the property helper files below into this panel implementation. |
| `Editor/Panels/Property/BindingSummaryPresenter.h` | Binding summary formatting helpers | `Editor/Panels/NodePropertiesPanel.h`, `Editor/Panels/NodePropertiesPanel.cpp` | `merge` | Property-panel-only helper. |
| `Editor/Panels/Property/BindingSummaryPresenter.cpp` | Binding summary implementation | `Editor/Panels/NodePropertiesPanel.cpp` | `merge` | Same as above. |
| `Editor/Panels/Property/ParamEditorFactory.h` | Param editor creation helpers | `Editor/Panels/NodePropertiesPanel.h`, `Editor/Panels/NodePropertiesPanel.cpp` | `merge` | Property-panel-only helper. |
| `Editor/Panels/Property/ParamEditorFactory.cpp` | Param editor creation implementation | `Editor/Panels/NodePropertiesPanel.cpp` | `merge` | Same as above. |
| `Editor/Panels/Property/ParamValueFormatter.h` | Param value formatting helpers | `Editor/Panels/NodePropertiesPanel.h`, `Editor/Panels/NodePropertiesPanel.cpp` | `merge` | Property-panel-only helper. |
| `Editor/Panels/Property/ParamValueFormatter.cpp` | Param value formatting implementation | `Editor/Panels/NodePropertiesPanel.cpp` | `merge` | Same as above. |
| `Editor/Panels/DiagnosticsDrawer.h` | Diagnostics drawer API | `Editor/Panels/DiagnosticsDrawer.h` | `move` | Direct move. |
| `Editor/Panels/DiagnosticsDrawer.cpp` | Diagnostics drawer implementation | `Editor/Panels/DiagnosticsDrawer.cpp` | `move` | Direct move. |
| `Editor/Panels/PresetBrowserPanel.h` | Preset browser panel API | `Editor/TTeulEditor.cpp` | `phase1 temporary merge` | This panel is outside the current minimum scaffold. Keep it editor-owned temporarily inside `TTeulEditor` until a dedicated preset browser panel is reintroduced. |
| `Editor/Panels/PresetBrowserPanel.cpp` | Preset browser implementation | `Editor/TTeulEditor.cpp` | `phase1 temporary merge` | Same as above. |
| `Editor/Search/SearchController.h` | Search overlay API and search-entry helpers | `Editor/Search/SearchController.h` | `move` | Direct move. |
| `Editor/Search/SearchController.cpp` | Shared search overlay implementation | `Editor/Search/SearchController.cpp` | `move` | Direct move. |
| `Editor/Search/QuickAddOverlay.cpp` | Quick-add search and insert-on-wire flow | `Editor/Search/SearchController.cpp` | `merge` | This is search behavior, not a separate top-level file in the minimum structure. |
| `Editor/Search/NodeSearchOverlay.cpp` | Node search overlay flow | `Editor/Search/SearchController.cpp` | `merge` | Same as above. |
| `Editor/Search/CommandPaletteOverlay.cpp` | Command palette overlay flow | `Editor/Search/SearchController.cpp` | `merge` | Same as above. |

### Editor Split Principle

- `TTeulEditor` replaces `EditorHandle` and owns only top-level editor orchestration.
- `TGraphCanvas` owns canvas state and high-level input routing.
- Dedicated renderers own node, port, and connection drawing.
- Property-panel-only helpers should not survive as their own mini-subsystem in the minimum structure.
- The current preset browser panel can be kept editor-local temporarily even though it is not part of the minimum scaffold.

---

## Runtime File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Runtime/TNodeInstance.h` | Executable node instance interface, process context, param reporter hook | `Runtime/AudioGraph/TGraphProcessor.h`, `Runtime/TTeulRuntime.h` | `split` | `TNodeInstance` and `TProcessContext` belong to the audio-graph execution side. The param reporter hook can become an internal runtime detail. |
| `Runtime/TGraphProcessor.h` | JUCE `AudioProcessor` wrapper around the runtime engine | `Runtime/AudioGraph/TGraphProcessor.h`, `Runtime/AudioGraph/TGraphProcessor.cpp` | `move` | Direct move, with implementation split out of the header. |
| `Runtime/TGraphRuntime.h` | Monolithic runtime engine: document compile/build, audio callback, param surface, device capture, diagnostics, control routing | `Runtime/TTeulRuntime.h`, `Runtime/AudioGraph/TGraphCompiler.*`, `Runtime/AudioGraph/TRuntimeDiagnostics.*`, `Runtime/IOControl/TRuntimeDeviceManager.*`, `Runtime/IOControl/TRuntimeEvent.*`, `Runtime/IOControl/TRuntimeEventQueue.*` | `split` | This is the main runtime god-file and must be divided by responsibility. |
| `Runtime/TGraphRuntime.cpp` | Graph topological build, node instantiation, block processing, param queueing, diagnostics, device callback flow | `Runtime/TTeulRuntime.cpp`, `Runtime/AudioGraph/TGraphCompiler.cpp`, `Runtime/AudioGraph/TRuntimeDiagnostics.cpp`, `Runtime/IOControl/TRuntimeDeviceManager.cpp`, `Runtime/IOControl/TRuntimeEventQueue.cpp` | `split` | Graph compile/build logic moves to `TGraphCompiler`. Runtime stats/fault flags move to `TRuntimeDiagnostics`. Device/channel/event handling moves toward `IOControl`. `TTeulRuntime` remains the coordinator. |

### Runtime Split Principle

- `TTeulRuntime` is the runtime facade and coordinator.
- `AudioGraph` owns graph compilation, executable-node processing, and runtime diagnostics.
- `IOControl` owns runtime events, device state, channel/layout changes, and control-device presence.
- The param-provider surface should stop living under `Bridge`; it is a runtime/editor contract.

---

## Bridge File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Bridge/ITeulParamProvider.h` | Exposed-param types and runtime/editor param access interface | `Runtime/TTeulRuntime.h`, `Editor/TTeulEditor.h` | `move out of Bridge` | Despite the current folder, this is not an external bridge concern. It is a runtime surface consumed by the editor. Keep the interface next to the runtime facade and include it from the editor. |

### Bridge Split Principle

- `Bridge` should be reserved for external integration, codegen, and external JSON codecs.
- Internal runtime/editor parameter surfaces should not remain in `Bridge`.

---

## Export File-Level Mapping

| Current file | Current responsibility | Target folder(s) | Move type | Refactor note |
| --- | --- | --- | --- | --- |
| `Export/TExport.h` | Export mode/options/report types, graph IR, exporter facade, editable-graph import API | `Bridge/TBridgeCodegen.*`, `Bridge/TBridgeJsonCodec.*`, `Document/TDocumentStore.*` | `split` | Code generation and manifest/runtime JSON belong to `Bridge`. Editable graph package import belongs back in `Document`. |
| `Export/TExport.cpp` | Export validation, graph IR normalization, asset packaging, manifest/runtime JSON writing, generated code snippets, editable-graph import | `Bridge/TBridgeCodegen.cpp`, `Bridge/TBridgeJsonCodec.cpp`, `Document/TDocumentStore.cpp` | `split` | Split codegen and export packaging into `Bridge`. Keep editable `.teul` package import under `Document`. |

### Export Split Principle

- Export is an external-consumption concern and belongs under `Bridge`.
- Importing an editable graph package is document restoration logic and belongs under `Document`.
- Export report and IR types may be split between codegen and JSON-codec files, but they should stop depending directly on the old `Registry` folder.

