# Teul File Structure (Target Architecture)

This document defines the final target structure for `Source/Teul` after the editor refactor is complete.
The goal is to make ownership boundaries explicit, remove root-level editor monoliths, and leave `Teul.h` as the only public umbrella include.

## Final Target Structure

```text
Source/Teul/
  Teul.h                                  // Public umbrella include only

  Teul Roadmap.md
  Teul UI Roadmap.md
  Teul Execution Plan.md
  Teul_FileStructure.md

  Public/
    EditorHandle.h                        // Public editor API
    Export.h                              // Public export API forwarding include

  Bridge/
    ITeulParamProvider.h                  // Runtime to editor parameter bridge contract

  Model/
    TTypes.h                              // Graph primitive types
    TPort.h                               // Port model
    TNode.h                               // Node model
    TConnection.h                         // Connection model
    TGraphDocument.h / .cpp               // Graph state, revisions, document mutation helpers

  History/
    TCommand.h                            // Base undo/redo command
    TCommands.h                           // Graph command set

  Registry/
    TNodeSDK.h                            // Descriptor and runtime contract
    TNodeRegistry.h / .cpp                // Registry and default factory

    Nodes/
      CoreNodes.h                         // Built-in node registration entry

      Core/
        SourceNodes.h                     // Oscillator, LFO, Noise, Constant, AudioOutput
        FilterNodes.h                     // LPF, HPF, BPF, EQ-oriented filters
        FXNodes.h                         // Delay, Reverb, distortion-style nodes
        MixerNodes.h                      // VCA, panner, mixers, routing helpers
        MathLogicNodes.h                  // Math, mapping, clamp, logic nodes
        ModulationNodes.h                 // ADSR and modulation nodes
        MidiNodes.h                       // MIDI source and utility nodes

  Runtime/
    TNodeInstance.h                       // Runtime node base class
    TGraphRuntime.h / .cpp                // DSP graph runtime
    TGraphProcessor.h                     // AudioProcessor wrapper and host bridge

  Serialization/
    TSerializer.h / .cpp                  // Graph JSON serialization
    TFileIo.h / .cpp                      // File import and export helpers

  Export/
    TExport.h / .cpp                      // Validation, diagnostics, IR, codegen, packaging

  Editor/
    EditorHandle.cpp                      // Thin orchestrator only
    EditorHandleImpl.h / .cpp             // EditorHandle::Impl body and owned services

    Theme/
      TeulPalette.h                       // Shared palette and theme constants

    Shell/
      EditorLayoutCoordinator.h / .cpp    // Panel docking and root layout policy
      EditorSessionStore.h / .cpp         // Session restore and save
      EditorRuntimeBridge.h / .cpp        // Runtime dirty, rebuild, telemetry bridge

    Commands/
      EditorCommandSystem.h / .cpp        // Keyboard shortcuts and command routing

    Search/
      SearchController.h / .cpp           // Shared search ranking and recents
      QuickAddOverlay.h / .cpp            // Quick Add overlay and list UI
      NodeSearchOverlay.h / .cpp          // Find Node overlay
      CommandPaletteOverlay.h / .cpp      // Command palette overlay

    Canvas/
      TGraphCanvas.h / .cpp               // Viewport facade only
      WireRenderer.h / .cpp               // Connection path rendering
      MiniMapRenderer.h / .cpp            // Minimap rendering
      FrameRenderer.h / .cpp              // Frame drawing and badges
      SelectionOverlayRenderer.h / .cpp   // Marquee and selection visuals
      StatusHintPresenter.h / .cpp        // Status hint and transient HUD text

    Interaction/
      ConnectionDragController.h / .cpp   // Port-to-port drag flow
      NodeDragController.h / .cpp         // Node drag lifecycle
      FrameDragController.h / .cpp        // Frame drag lifecycle
      SelectionController.h / .cpp        // Selection state and sync
      AlignmentEngine.h / .cpp            // Align and distribute operations
      ContextMenuController.h / .cpp      // Canvas, node, frame menu actions

    Panels/
      NodeLibraryPanel.h / .cpp           // Library search and insert panel
      NodePropertiesPanel.h / .cpp        // Inspector shell only
      ExportDiagnosticsPanel.h / .cpp     // Export diagnostics workspace
      RuntimeStatusPanel.h / .cpp         // Runtime telemetry and safety HUD

      Property/
        ParamEditorFactory.h / .cpp       // Per-param editor widget construction
        ParamValueFormatter.h / .cpp      // Runtime and document value formatting
        BindingSummaryPresenter.h / .cpp  // Ieum and Gyeol binding summary text

    Node/
      TNodeComponent.h / .cpp             // Single node component and hit areas
      NodePreviewRenderer.h / .cpp        // Oscillator, ADSR, meter preview drawing

    Port/
      TPortComponent.h / .cpp             // Port component and drag affordances

    CustomNodes/
      CustomNodeEditorFactory.h / .cpp    // Optional custom node editor entry point
      CustomNodePreviewFactory.h / .cpp   // Optional per-node preview extensions
```

## Ownership Rules

- `Teul.h` includes public headers only. It must not declare concrete editor implementation types.
- `Teul.cpp` does not exist in the final structure. The final build compiles each `.cpp` file individually.
- `EditorHandle.cpp` stays thin. It constructs the implementation object and forwards high-level editor API calls only.
- `EditorHandleImpl.*` owns editor-wide services, top-level components, and cross-panel coordination.
- `Panels/*` own panel UI. They do not own root layout policy or document-wide command routing.
- `Canvas/*` owns viewport rendering. It does not own menu policy, search overlays, or selection business rules directly.
- `Interaction/*` owns input-driven graph mutations and selection mechanics.
- `Search/*` owns quick add, find node, and command palette presentation and ranking.
- `Node/*` owns node visuals. Preview drawing stays out of `TNodeComponent` except for delegation.
- `Property/*` owns parameter editor creation and formatting details so `NodePropertiesPanel` remains a shell.
- `Model`, `History`, `Registry`, `Runtime`, `Serialization`, `Export`, and `Bridge` remain domain modules and should not depend on editor UI files.

## Transition Notes

The repository is not at the final structure yet. During migration, a temporary `Source/Teul/Teul.cpp` aggregator may still exist to include multiple implementation units into one translation unit.
That file is transitional only.
It should be removed after:

1. `EditorHandle::Impl`, panels, canvas helpers, and preview helpers are split into dedicated files.
2. All Teul `.cpp` files are registered individually in the project build files.
3. There are no remaining `.cpp` includes from other `.cpp` files.

## Refactor Milestones

### Stage 1: Public Entry Split

- [DONE] `Source/Teul/Teul.h` reduced to umbrella forwarding include
- [DONE] `Source/Teul/Public/EditorHandle.h` added as the public editor API
- [DONE] `Source/Teul/Editor/EditorHandle.cpp` created and moved out of root `Teul.cpp`

### Stage 2: Editor Shell Split

- [DONE] `Source/Teul/Editor/EditorHandleImpl.h`
- [DONE] `Source/Teul/Editor/EditorHandleImpl.cpp`
- [DONE] move `EditorHandle::Impl` body out of `EditorHandle.cpp`

### Stage 3: Panel Split

- [DONE] `Source/Teul/Editor/Panels/NodeLibraryPanel.h`
- [DONE] `Source/Teul/Editor/Panels/NodeLibraryPanel.cpp`
- [DONE] `Source/Teul/Editor/Panels/NodePropertiesPanel.h`
- [DONE] `Source/Teul/Editor/Panels/NodePropertiesPanel.cpp`
- [DONE] `Source/Teul/Editor/Panels/Property/ParamEditorFactory.h`
- [DONE] `Source/Teul/Editor/Panels/Property/ParamEditorFactory.cpp`
- [DONE] `Source/Teul/Editor/Panels/Property/ParamValueFormatter.h`
- [DONE] `Source/Teul/Editor/Panels/Property/ParamValueFormatter.cpp`
- [DONE] `Source/Teul/Editor/Panels/Property/BindingSummaryPresenter.h`
- [DONE] `Source/Teul/Editor/Panels/Property/BindingSummaryPresenter.cpp`

### Stage 4: Canvas and Interaction Split

- [TODO] split render helpers from `TGraphCanvas.cpp` into `Canvas/*`
- [TODO] split drag, selection, align, and menu logic into `Interaction/*`
- [TODO] split quick add, node search, and command palette overlays into `Search/*`

### Stage 5: Node Rendering Split

- [DONE] `Source/Teul/Editor/Node/NodePreviewRenderer.h`
- [DONE] `Source/Teul/Editor/Node/NodePreviewRenderer.cpp`
- [DONE] update `TNodeComponent.cpp` to delegate preview drawing only

### Stage 6: Build Finalization

- [TODO] register Teul implementation files individually in project build files
- [TODO] remove transitional `Source/Teul/Teul.cpp`
- [TODO] move any remaining editor file under `Source/Teul/UI/` into `Source/Teul/Editor/`

## Files That Should Stay Stable

These areas already have a reasonable ownership boundary and should only move if there is a strong architectural reason.

- `Source/Teul/Model/*`
- `Source/Teul/History/*`
- `Source/Teul/Registry/*`
- `Source/Teul/Runtime/*`
- `Source/Teul/Serialization/*`
- `Source/Teul/Export/*`
- `Source/Teul/Bridge/*`

## Notes

- Documentation files can remain under `Source/Teul` until the repository adopts a shared docs policy.
- `CustomNodes/` is a reserved extension point. Keep it minimal until real custom-node workflows appear.
- The final structure should prefer individually compiled `.cpp` files over umbrella translation units.
