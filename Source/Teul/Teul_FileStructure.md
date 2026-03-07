# Teul File Structure (Refactor Draft)

This document defines a cleaned target structure for `Source/Teul`.
The goal is to restore `Teul.h` as a public umbrella include, keep `Teul.cpp` as a single-translation-unit aggregator only while the current build requires it, and move editor logic into role-specific modules.

## Recommended Structure

```text
Source/Teul/
  Teul.h                                  // Public umbrella include only
  Teul.cpp                                // Single-TU aggregator only (temporary while project build requires it)

  Teul Roadmap.md
  Teul UI Roadmap.md
  Teul Execution Plan.md
  Teul_FileStructure.md

  Public/
    EditorHandle.h                        // Public editor API
    Export.h                              // Optional compatibility include for export API

  Bridge/
    ITeulParamProvider.h                  // Runtime/UI parameter bridge contract

  Model/
    TTypes.h                              // Graph primitive types
    TPort.h                               // Port model
    TNode.h                               // Node model
    TConnection.h                         // Connection model
    TGraphDocument.h / .cpp               // Graph state, revisions, undo-facing mutations

  History/
    TCommand.h                            // Base undo/redo command
    TCommands.h                           // Graph command set

  Registry/
    TNodeSDK.h                            // Descriptor/runtime contract
    TNodeRegistry.h / .cpp                // Registry and default factory

    Nodes/
      CoreNodes.h                         // Built-in node registration entry

      Core/
        SourceNodes.h                     // Oscillator, LFO, Noise, Constant, AudioOutput
        FilterNodes.h                     // LPF/HPF/BPF and related filters
        FXNodes.h                         // Delay, Reverb, distortion-style nodes
        MixerNodes.h                      // VCA, panner, mixers, routing helpers
        MathLogicNodes.h                  // Math, mapping, clamp, logic nodes
        ModulationNodes.h                 // ADSR/LFO-style modulation nodes
        MidiNodes.h                       // MIDI source/util nodes

  Runtime/
    TNodeInstance.h                       // Runtime node base class
    TGraphRuntime.h / .cpp                // DSP graph runtime
    TGraphProcessor.h                     // AudioProcessor wrapper / host bridge

  Serialization/
    TSerializer.h / .cpp                  // Graph JSON serialization
    TFileIo.h / .cpp                      // File import/export helpers

  Export/
    TExport.h / .cpp                      // Validation, diagnostics, IR, codegen, packaging

  Editor/
    EditorHandle.cpp                      // Thin orchestrator only
    EditorHandleImpl.h / .cpp             // EditorHandle::Impl body

    Theme/
      TeulPalette.h                       // Shared palette/theme constants

    Canvas/
      TGraphCanvas.h / .cpp               // Graph viewport, pan/zoom, wire interactions
      WireRenderer.h / .cpp               // Future dedicated wire/path rendering split
      SelectionOverlay.h / .cpp           // Future marquee/selection overlay split

    Node/
      TNodeComponent.h / .cpp             // Single node component
      NodePreviewRenderer.h / .cpp        // Oscillator/ADSR/meter preview rendering split

    Port/
      TPortComponent.h / .cpp             // Port component and drag affordances

    Panels/
      NodeLibraryPanel.h / .cpp           // Library/search/filter panel
      NodePropertiesPanel.h / .cpp        // Inspector/property panel
      ExportDiagnosticsPanel.h / .cpp     // Export diagnostics workspace
      RuntimeStatusPanel.h / .cpp         // Runtime telemetry / status HUD

    Commands/
      EditorCommandSystem.h / .cpp        // Shortcuts and command palette wiring
      SearchController.h / .cpp           // Quick add / find node shared search logic

    Shell/
      EditorLayoutCoordinator.h / .cpp    // Panel docking/layout policy
      EditorSessionStore.h / .cpp         // Session restore/save
      EditorRuntimeBridge.h / .cpp        // Runtime dirty/rebuild coordination
```

## Remove From Target Structure

These items are considered temporary or structurally misplaced in the target layout.

- `Source/Teul/Teul.h` holding the full `EditorHandle` public declaration directly
- `Source/Teul/Teul.cpp` containing concrete editor UI classes such as `NodeLibraryPanel` and `NodePropertiesPanel`
- Inline `EditorHandle::Impl` body living in the same file as the umbrella single-TU aggregator
- New editor-facing files being added under `Source/Teul/` root instead of a dedicated `Editor/` or `Public/` subtree

## Refactor Change Set

Status Legend: `[TODO]` planned, `[DONE]` completed during migration.

This section tracks the main file moves and edits expected when the target structure is applied.

### Files To Add

- [TODO] `Source/Teul/Public/EditorHandle.h`
- [TODO] `Source/Teul/Public/Export.h` (only if a stable public export include is needed)
- [TODO] `Source/Teul/Editor/EditorHandle.cpp`
- [TODO] `Source/Teul/Editor/EditorHandleImpl.h`
- [TODO] `Source/Teul/Editor/EditorHandleImpl.cpp`
- [TODO] `Source/Teul/Editor/Panels/NodeLibraryPanel.h`
- [TODO] `Source/Teul/Editor/Panels/NodeLibraryPanel.cpp`
- [TODO] `Source/Teul/Editor/Panels/NodePropertiesPanel.h`
- [TODO] `Source/Teul/Editor/Panels/NodePropertiesPanel.cpp`
- [TODO] `Source/Teul/Editor/Commands/EditorCommandSystem.h`
- [TODO] `Source/Teul/Editor/Commands/EditorCommandSystem.cpp`
- [TODO] `Source/Teul/Editor/Shell/EditorLayoutCoordinator.h`
- [TODO] `Source/Teul/Editor/Shell/EditorLayoutCoordinator.cpp`
- [TODO] `Source/Teul/Editor/Shell/EditorSessionStore.h`
- [TODO] `Source/Teul/Editor/Shell/EditorSessionStore.cpp`
- [TODO] `Source/Teul/Editor/Node/NodePreviewRenderer.h`
- [TODO] `Source/Teul/Editor/Node/NodePreviewRenderer.cpp`

### Files To Modify

- [TODO] `Source/Teul/Teul.h` (reduce to umbrella include path forwarding)
- [TODO] `Source/Teul/Teul.cpp` (reduce to single-TU aggregator only)
- [TODO] `Source/Teul/UI/TeulPalette.h` or move to `Source/Teul/Editor/Theme/TeulPalette.h`
- [TODO] `Source/Teul/UI/Canvas/TGraphCanvas.h`
- [TODO] `Source/Teul/UI/Canvas/TGraphCanvas.cpp`
- [TODO] `Source/Teul/UI/Node/TNodeComponent.h`
- [TODO] `Source/Teul/UI/Node/TNodeComponent.cpp`
- [TODO] `Source/Teul/UI/Port/TPortComponent.h`
- [TODO] `Source/Teul/UI/Port/TPortComponent.cpp`
- [TODO] `Source/MainComponent.h` (include path update if `Teul.h` forwarding changes)
- [TODO] Project build files (`.vcxproj`, `.filters`, `.jucer`, CMake if added later)

### Files To Keep As-Is Unless There Is a Strong Reason

- `Source/Teul/Model/*`
- `Source/Teul/History/*`
- `Source/Teul/Registry/*`
- `Source/Teul/Runtime/*`
- `Source/Teul/Serialization/*`
- `Source/Teul/Export/*`
- `Source/Teul/Bridge/*`

These are already grouped by domain and do not have the same structural problem as the current root-level editor files.

## Migration Notes

- Keep the migration incremental. `Teul.cpp` can continue to aggregate `.cpp` files during the transition, but it should stop containing concrete editor implementation code.
- The first refactor slice should be: `Public/EditorHandle.h`, `Editor/EditorHandle.cpp`, `EditorHandleImpl.*`, `Panels/NodeLibraryPanel.*`, and `Panels/NodePropertiesPanel.*`.
- After that split, move theme, command, and shell helpers only when they provide a real reduction in `EditorHandle::Impl` size.
- `UI/CustomNodes/` can remain reserved for future specialized node renderers/editors; do not invent structure there until a real custom-node workflow exists.
- Documentation files currently live under `Source/Teul`. That is acceptable for now; move them to a dedicated `docs/` tree only when the repository adopts a shared documentation policy.
