# Teul Roadmap 2

> Goal: lock a simple top-level architecture before the Teul rewrite.
> This document focuses on system boundaries and responsibilities, not detailed class decomposition.

---

## Top-Level Structure

Teul is split into four top-level layers.

```text
Source/Teul/
  Teul.h
  Document/
  Editor/
  Runtime/
  Bridge/
```

## Full Planned File Layout

```text
Source/Teul/
  Teul.h
  Document/
    TDocumentTypes.h
    TTeulDocument.h
    TTeulDocument.cpp
    TDocumentHistory.h
    TDocumentHistory.cpp
    TDocumentSerializer.h
    TDocumentSerializer.cpp
    TDocumentMigration.h
    TDocumentMigration.cpp
    TDocumentStore.h
    TDocumentStore.cpp

  Editor/
    TTeulEditor.h
    TTeulEditor.cpp
    TIssueState.h
    Canvas/
      TGraphCanvas.h
      TGraphCanvas.cpp
    Renderers/
      TNodeRenderer.h
      TNodeRenderer.cpp
      TPortRenderer.h
      TPortRenderer.cpp
      TConnectionRenderer.h
      TConnectionRenderer.cpp
    Interaction/
      SelectionController.cpp
      ConnectionInteraction.cpp
      ContextMenuController.cpp
    Panels/
      NodeLibraryPanel.h
      NodeLibraryPanel.cpp
      NodePropertiesPanel.h
      NodePropertiesPanel.cpp
      PresetBrowserPanel.h
      PresetBrowserPanel.cpp
      DiagnosticsDrawer.h
      DiagnosticsDrawer.cpp
    Search/
      SearchController.h
      SearchController.cpp
    Theme/
      TeulPalette.h

  Runtime/
    TTeulRuntime.h
    TTeulRuntime.cpp
    AudioGraph/
      TGraphCompiler.h
      TGraphCompiler.cpp
      TGraphProcessor.h
      TGraphProcessor.cpp
      TRuntimeDiagnostics.h
      TRuntimeDiagnostics.cpp
      TRuntimeValidator.h
      TRuntimeValidator.cpp
    IOControl/
      TRuntimeEvent.h
      TRuntimeEventQueue.h
      TRuntimeEventQueue.cpp
      TRuntimeDeviceManager.h
      TRuntimeDeviceManager.cpp

  Bridge/
    TTeulBridge.h
    TTeulBridge.cpp
    TBridgeJsonCodec.h
    TBridgeJsonCodec.cpp
    TBridgeCodegen.h
    TBridgeCodegen.cpp
    TIeumBridge.h
    TIeumBridge.cpp
    TGyeolBridge.h
    TGyeolBridge.cpp
    TNaruBridge.h
    TNaruBridge.cpp
```

### 1. Document
- Accept documents in a system-compatible form.
- Own and manage the current in-memory document.
- Manage history snapshots and undo/redo.
- Check versions and run migration/upgrade when needed.
- Define ValueTree shape and document serialization format.
- Handle save, load, autosave, and recovery.
- Interpret imported `.teul` files created by other users.

### 2. Editor
- Present the document visually and allow editing.
- Own canvas, panels, inspectors, rails, selection, drag, and menus.
- Provide usability features for adding, placing, and connecting nodes.
- Define visual rules for nodes, ports, connections, and rails.
- Read from `Document`; do not become the source of truth.

### 3. Runtime
- Turn the document into an executable graph.
- Own and manage the live audio graph.
- Process audio, MIDI, and control data.
- Handle runtime events and device changes.
- Detect runtime problems and keep performance stable.
- Validate new DSP nodes before admitting them into runtime.

### 4. Bridge
- Translate between Teul internal structures and external systems.
- Handle code generation, external JSON encode/decode, and external adapters.
- Support Ieum, Gyeol, Naru, and similar projects.
- `.teul` document parsing stays in `Document`, not `Bridge`.

---

## Phase 1 Rewrite Goals

- Lock the four top-level layers: `Document`, `Editor`, `Runtime`, `Bridge`.
- Remove the old `EditorHandleImpl`-centered structure and simplify around `TTeulEditor`.
- Move scattered document responsibilities into `Document/`.
- Move editor UI and interaction responsibilities into `Editor/`.
- Split runtime responsibilities into `Runtime/AudioGraph` and `Runtime/IOControl`.
- Move external integration, codegen, and external JSON responsibilities into `Bridge/`.
- Keep a single umbrella header `Teul.h` for public inclusion.
- Delay deeper subdivision until phase 2.

---

## Document Layer

`Document` owns the full document lifecycle, not just raw data storage.

Core responsibilities:
- Current in-memory document state.
- Node, port, connection, control source, and system endpoint structure.
- History snapshots.
- Undo and redo.
- Revision and version management.
- Migration of older documents.
- ValueTree representation.
- Serialization and deserialization.
- File save/load.
- Autosave and recovery.
- Import and interpretation of `.teul` files.

### Minimum Document Files

```text
Source/Teul/Document/
  TDocumentTypes.h
  TTeulDocument.h
  TTeulDocument.cpp
  TDocumentHistory.h
  TDocumentHistory.cpp
  TDocumentSerializer.h
  TDocumentSerializer.cpp
  TDocumentMigration.h
  TDocumentMigration.cpp
  TDocumentStore.h
  TDocumentStore.cpp
```

### File Roles
- `TDocumentTypes`: shared document types and keys.
- `TTeulDocument`: normalized in-memory document model.
- `TDocumentHistory`: snapshot stack and undo/redo.
- `TDocumentSerializer`: `TTeulDocument <-> ValueTree / JSON`, including `.teul` interpretation rules.
- `TDocumentMigration`: version checks and upgrade steps.
- `TDocumentStore`: top-level save/load/autosave/recovery facade.

---

## Editor Layer

`Editor` turns the document into a usable graph editing UI.

Core responsibilities:
- Visual graph presentation on canvas.
- Node add/move/select/delete.
- Port-to-port connection creation/removal.
- Control assignment drag/drop.
- Rails, panels, and inspectors.
- Quick add, search, and context menu flows.
- Visual rules for nodes, ports, and connections.
- Hover, selection, and drag feedback.
- Document mutation through user interaction.
- Stable viewport, zoom, pan, and redraw behavior.

### Minimum Editor Files

```text
Source/Teul/Editor/
  TTeulEditor.h
  TTeulEditor.cpp
  TIssueState.h
  Canvas/
    TGraphCanvas.h
    TGraphCanvas.cpp
  Renderers/
    TNodeRenderer.h
    TNodeRenderer.cpp
    TPortRenderer.h
    TPortRenderer.cpp
    TConnectionRenderer.h
    TConnectionRenderer.cpp
  Interaction/
    SelectionController.cpp
    ConnectionInteraction.cpp
    ContextMenuController.cpp
  Panels/
    NodeLibraryPanel.h
    NodeLibraryPanel.cpp
    NodePropertiesPanel.h
    NodePropertiesPanel.cpp
    DiagnosticsDrawer.h
    DiagnosticsDrawer.cpp
  Search/
    SearchController.h
    SearchController.cpp
  Theme/
    TeulPalette.h
```

### File Roles
- `Teul.h`: umbrella public header for the Teul subsystem.
- `TTeulEditor`: top-level editor orchestration entry.
- `TGraphCanvas`: live graph canvas orchestration.
- `TNodeRenderer`: node body rendering.
- `TPortRenderer`: port rendering and port geometry helpers.
- `TConnectionRenderer`: connection line rendering.
- `SelectionController`: selection rules.
- `ConnectionInteraction`: connect/preview/commit/cancel flow.
- `ContextMenuController`: contextual editing commands.
- `NodeLibraryPanel`: node browsing and insertion.
- `NodePropertiesPanel`: selected node parameter editing.
- `PresetBrowserPanel`: preset browsing and preset-triggered document actions.
- `DiagnosticsDrawer`: runtime and verification side info.
- `SearchController`: quick add, node search, command palette coordination.
- `TeulPalette`: editor color system.
- `TIssueState`: shared visual issue state.

### Editor Internal Composition

```text
TTeulEditor
  -> TGraphCanvas
       -> TNodeRenderer
            -> TPortRenderer
       -> TConnectionRenderer
```

Rules:
- `TTeulEditor` assembles the whole editor.
- `TGraphCanvas` coordinates visual graph presentation.
- `TNodeRenderer` renders nodes and uses `TPortRenderer` for ports.
- `TConnectionRenderer` renders connections in canvas coordinates.
- Port geometry helpers live inside `TPortRenderer` for now.

### Required Usability Features
- Quick add.
- Node search.
- Drag to connect.
- Drag to move.
- Marquee selection.
- Multi-select.
- Context menu.
- Zoom and pan.
- Snap or alignment assistance.
- Selected target properties display.
- Hover, selection, and drag feedback.
- Clear visual warnings for invalid or missing states.

### Visual Rules
- Node type and state must be readable at a glance.
- Port type color and placement must be consistent.
- Connection direction and active state must be readable.
- Selection, hover, and drag preview must be obvious.
- Invalid, degraded, and missing states must be distinct.
- Canvas, rail, and panel hierarchy must read naturally.

### Optimization Rules
- Avoid full rebuilds when not needed.
- Limit connection redraw scope during node move.
- Repaint only live regions for hover and meter updates.
- Separate selection refresh from document refresh.
- Separate layout work from paint work.
- Keep reusable geometry caches for ports and connections.

---

## Runtime Layer

`Runtime` turns the document into a live executable system and keeps it stable while running.

Split it into two subdomains:
- `AudioGraph`
  - Compile documents into executable graphs.
  - Own and process the live graph.
  - Handle performance, diagnostics, and validation.
- `IOControl`
  - Handle runtime I/O and control-device events.
  - Track device add/remove and stream configuration changes.
  - Notify runtime when external state requires rebuild or update.

### Minimum Runtime Files

```text
Source/Teul/Runtime/
  TTeulRuntime.h
  TTeulRuntime.cpp
  AudioGraph/
    TGraphCompiler.h
    TGraphCompiler.cpp
    TGraphProcessor.h
    TGraphProcessor.cpp
    TRuntimeDiagnostics.h
    TRuntimeDiagnostics.cpp
    TRuntimeValidator.h
    TRuntimeValidator.cpp
  IOControl/
    TRuntimeEvent.h
    TRuntimeEventQueue.h
    TRuntimeEventQueue.cpp
    TRuntimeDeviceManager.h
    TRuntimeDeviceManager.cpp
```

### File Roles
- `TTeulRuntime`: top-level runtime orchestration entry.
- `TGraphCompiler`: `Document -> executable graph` transform.
- `TGraphProcessor`: live block-based DSP processing.
- `TRuntimeDiagnostics`: runtime faults and stats.
- `TRuntimeValidator`: pre-admission validation for DSP graphs and nodes.
- `TRuntimeEvent`: runtime event types.
- `TRuntimeEventQueue`: safe event push/drain path.
- `TRuntimeDeviceManager`: audio/MIDI/control device state tracking.

### Runtime Internal Composition

```text
TTeulRuntime
  -> AudioGraph
       -> TGraphCompiler
       -> TGraphProcessor
       -> TRuntimeDiagnostics
       -> TRuntimeValidator
  -> IOControl
       -> TRuntimeEventQueue
       -> TRuntimeDeviceManager
```

Rules:
- `TTeulRuntime` assembles the whole runtime.
- `AudioGraph` owns execution graph logic.
- `IOControl` owns device and runtime-event logic.
- `IOControl` does not own the DSP graph.
- `AudioGraph` does not own device discovery logic.

---

## Bridge Layer

`Bridge` translates between Teul internals and external systems.

Core responsibilities:
- Convert Teul internal structures into external payloads.
- Convert external requests and responses into Teul-friendly structures.
- Code generation.
- External JSON encoding and decoding.
- Project-specific adapters for Ieum, Gyeol, Naru, and similar systems.
- Provide a clean external-facing facade.

Important boundary:
- `.teul` parsing stays in `Document`.
- Ieum/Gyeol/Naru JSON or external protocol parsing lives in `Bridge`.

### Minimum Bridge Files

```text
Source/Teul/Bridge/
  TTeulBridge.h
  TTeulBridge.cpp
  TBridgeJsonCodec.h
  TBridgeJsonCodec.cpp
  TBridgeCodegen.h
  TBridgeCodegen.cpp
  TIeumBridge.h
  TIeumBridge.cpp
  TGyeolBridge.h
  TGyeolBridge.cpp
  TNaruBridge.h
  TNaruBridge.cpp
```

### File Roles
- `TTeulBridge`: top-level bridge facade.
- `TBridgeJsonCodec`: external JSON encoding and decoding.
- `TBridgeCodegen`: code generation output.
- `TIeumBridge`: Ieum adapter.
- `TGyeolBridge`: Gyeol adapter.
- `TNaruBridge`: Naru adapter.

### Bridge Internal Composition

```text
TTeulBridge
  -> TBridgeJsonCodec
  -> TBridgeCodegen
  -> TIeumBridge
  -> TGyeolBridge
  -> TNaruBridge
```

Rules:
- `TTeulBridge` assembles the external integration layer.
- Each external project gets its own adapter.
- Shared JSON logic stays in `TBridgeJsonCodec`.
- Shared code generation stays in `TBridgeCodegen`.
- External-project-specific rules do not accumulate inside `TTeulBridge`.

---

## Sync Boundaries

Keep sync responsibilities at the main entry points of each layer.

```text
Document/TTeulDocument
  <-> Editor/TTeulEditor
  <-> Runtime/TTeulRuntime
         -> Runtime/AudioGraph/TGraphCompiler
         -> Runtime/IOControl/TRuntimeEventQueue
  <-> Bridge/TTeulBridge
```

### Source of Truth
- `Document/TTeulDocument`
- This is the source of truth for current document state.

### Document <-> Editor
- `Editor/TTeulEditor`
- Refresh canvas and panels on document revision changes.
- Convert user edits into document mutations.

### Document <-> Runtime
- `Runtime/TTeulRuntime`
- Rebuild when document revision or runtime revision changes.
- Actual graph conversion is handled by `Runtime/AudioGraph/TGraphCompiler`.

### Runtime Event <-> Graph
- `Runtime/IOControl/TRuntimeEventQueue`
- Queue device and control changes.
- `Runtime/TTeulRuntime` drains and applies rebuilds or state updates.

### Document <-> Bridge
- `Bridge/TTeulBridge`
- Convert documents into external payloads and codegen outputs.
- Convert external inputs into Teul-friendly structures.

### Runtime <-> Editor
- `Runtime` must not touch UI directly.
- `Runtime` exposes diagnostics, stats, and state.
- `Editor` reads and displays that state.

### Bridge <-> Document Boundary
- `.teul` import/export belongs to `Document`.
- External-project import/export belongs to `Bridge`.

---

## Migration From Current Code

- Move pure document structures from `Model/` into `Document/`.
- Fold undo/redo from `History/` into `TDocumentHistory`.
- Rebuild document save/load around `TDocumentSerializer` and `TDocumentStore`.
- Keep migration logic under `TDocumentMigration`.
- Move editor canvas/panel/interaction/renderer responsibilities out of `EditorHandleImpl` into `Editor/`.
- Remove `EditorHandle` / `EditorHandleImpl` and replace them with `TTeulEditor`.
- Split existing `TGraphRuntime` compile/process/diagnostics/validator responsibilities into `Runtime/AudioGraph/`.
- Split existing device discovery, runtime event, and control-device-state responsibilities into `Runtime/IOControl/`.
- Move Ieum/external codegen/external JSON responsibilities into `Bridge/`.
- Keep `.teul` interpretation and migration in `Document/`.

---

## Completion Criteria

- The architecture is explainable as `Document`, `Editor`, `Runtime`, and `Bridge`.
- Document lifecycle responsibility reads as one `Document` layer.
- Editor presentation and interaction responsibility reads as one `Editor` layer.
- Runtime execution-graph responsibility reads as `Runtime/AudioGraph`.
- Runtime event and device-state responsibility reads as `Runtime/IOControl`.
- External integration and codegen responsibility reads as one `Bridge` layer.
- Undo/redo, migration, serialization, and autosave live under `Document`.
- Canvas, panels, interaction, and render rules live under `Editor`.
- Compile, process, diagnostics, and validator live under `Runtime/AudioGraph`.
- Runtime event, device state, and control-device changes live under `Runtime/IOControl`.
- External JSON, codegen, and Ieum/Gyeol/Naru adapters live under `Bridge`.
- `TTeulEditor` handles editor orchestration only.
- `TTeulRuntime` handles runtime orchestration only.
- `TTeulBridge` handles bridge orchestration only.
