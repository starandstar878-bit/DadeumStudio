# Teul File Structure

This document describes the current Teul file layout after the editor refactor.
The goals are:
- keep `Teul.h` as the public umbrella include
- keep concrete editor code under `Source/Teul/Editor`
- compile Teul as normal per-file translation units
- avoid any remaining `#include "*.cpp"` aggregation

## Current Structure

```text
Source/Teul/
  Teul.h
  Teul Roadmap.md
  Teul UI Roadmap.md
  Teul Execution Plan.md
  Teul_FileStructure.md

  Public/
    EditorHandle.h

  Bridge/
    ITeulParamProvider.h

  Model/
    TTypes.h
    TPort.h
    TNode.h
    TConnection.h
    TGraphDocument.h / .cpp

  History/
    TCommand.h
    TCommands.h

  Registry/
    TNodeSDK.h
    TNodeRegistry.h / .cpp
    Nodes/
      CoreNodes.h
      Core/
        SourceNodes.h
        FilterNodes.h
        FXNodes.h
        MixerNodes.h
        MathLogicNodes.h
        ModulationNodes.h
        MidiNodes.h

  Runtime/
    TNodeInstance.h
    TGraphRuntime.h / .cpp
    TGraphProcessor.h

  Serialization/
    TSerializer.h / .cpp
    TFileIo.h / .cpp

  Export/
    TExport.h / .cpp

  Editor/
    EditorHandle.cpp
    EditorHandleImpl.h / .cpp

    Theme/
      TeulPalette.h

    Canvas/
      TGraphCanvas.h / .cpp
      CanvasRenderers.cpp

    Search/
      SearchController.h / .cpp
      QuickAddOverlay.cpp
      NodeSearchOverlay.cpp
      CommandPaletteOverlay.cpp

    Interaction/
      ConnectionInteraction.cpp
      SelectionController.cpp
      NodeFrameInteraction.cpp
      AlignmentEngine.cpp
      ContextMenuController.cpp

    Panels/
      NodeLibraryPanel.h / .cpp
      NodePropertiesPanel.h / .cpp
      Property/
        ParamEditorFactory.h / .cpp
        ParamValueFormatter.h / .cpp
        BindingSummaryPresenter.h / .cpp

    Node/
      TNodeComponent.h / .cpp
      NodePreviewRenderer.h / .cpp

    Port/
      TPortComponent.h / .cpp
```

## Ownership Rules

- `Teul.h` is the umbrella include only.
- `Public/*` is the only intended public editor-facing surface.
- `EditorHandle.cpp` stays thin and delegates to `EditorHandle::Impl`.
- `Editor/Canvas/*` owns viewport composition and rendering helpers.
- `Editor/Search/*` owns quick add, node search, and command palette UI logic.
- `Editor/Interaction/*` owns connection drag, selection, alignment, and context menu behavior.
- `Editor/Panels/*` owns library and inspector panels.
- `Editor/Node/*` and `Editor/Port/*` own node and port component visuals.
- domain modules under `Model`, `History`, `Registry`, `Runtime`, `Serialization`, and `Export` stay editor-independent.

## Migration Status

### Stage 1: Public Entry Split

- [DONE] `Source/Teul/Teul.h` reduced to the umbrella include
- [DONE] `Source/Teul/Public/EditorHandle.h` introduced as the public editor API
- [DONE] `Source/Teul/Editor/EditorHandle.cpp` separated from the old root implementation file

### Stage 2: Editor Shell Split

- [DONE] `Source/Teul/Editor/EditorHandleImpl.h`
- [DONE] `Source/Teul/Editor/EditorHandleImpl.cpp`
- [DONE] `EditorHandle::Impl` moved out of `EditorHandle.cpp`

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

- [DONE] render helpers moved from `TGraphCanvas.cpp` into `Editor/Canvas/CanvasRenderers.cpp`
- [DONE] drag, selection, alignment, and menu logic moved into `Editor/Interaction/*`
- [DONE] quick add, node search, and command palette logic moved into `Editor/Search/*`

### Stage 5: Node Rendering Split

- [DONE] `Source/Teul/Editor/Node/NodePreviewRenderer.h`
- [DONE] `Source/Teul/Editor/Node/NodePreviewRenderer.cpp`
- [DONE] `TNodeComponent.cpp` reduced to node component behavior plus preview delegation

### Stage 6: Build Finalization

- [DONE] Teul implementation files are compiled individually in the project
- [DONE] transitional `Source/Teul/Teul.cpp` removed
- [DONE] tracked editor files moved out of `Source/Teul/UI/` into `Source/Teul/Editor/`

## Stable Areas

These modules were not restructured because their responsibility boundaries were already clear.

- `Source/Teul/Model/*`
- `Source/Teul/History/*`
- `Source/Teul/Registry/*`
- `Source/Teul/Runtime/*`
- `Source/Teul/Serialization/*`
- `Source/Teul/Export/*`
- `Source/Teul/Bridge/*`

## Notes

- `Source/Teul/UI/` is no longer part of the tracked editor implementation layout.
- The remaining refactors, if any, are cleanup or further internal decomposition, not structural blockers.
- Empty legacy directories may still appear locally until the filesystem is refreshed, but no tracked editor source should depend on them.
