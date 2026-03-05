# Gyeol File Structure (Refactor Draft)

This document defines a cleaned target structure for `Source/Gyeol`.
The goal is to keep `EditorHandle` as a thin orchestrator and move detailed logic into role-specific modules.

## Recommended Structure

```text
Source/Gyeol/
  Gyeol.h                                  // Public umbrella include

  Public/
    Action.h                               // Public action API
    Types.h                                // Public model/type definitions
    DocumentHandle.h                       // Public document handle
    EditorHandle.h                         // Public editor handle
    LayerModel.h                           // Compatibility include (redirect to Types.h)

  Core/
    Document.h / Document.cpp              // Document domain model/state
    DocumentHandle.cpp                     // DocumentHandle implementation
    DocumentStore.h                        // Undo/redo store
    Reducer.h                              // Action reducer
    SceneValidator.h                       // Scene validation rules

  Serialization/
    DocumentJson.h / DocumentJson.cpp      // JSON serialize/deserialize

  Runtime/
    PropertyBindingResolver.h              // Binding expression evaluator
    RuntimeActionExecutor.h / .cpp         // Runtime action execution
    RuntimeBindingEngine.h / .cpp          // Event -> action dispatch
    RuntimeParamBridge.h / .cpp            // Runtime parameter bridge
    RuntimeDiagnostics.h / .cpp            // Runtime logging/diagnostics

  Export/
    JuceComponentExport.h / .cpp           // JUCE export pipeline
    ExportDataContract.md                  // Export data contract

  Widgets/
    WidgetSDK.h                            // Widget SDK contract
    WidgetRegistry.h                       // Widget registry
    WidgetLibraryExchange.h / .cpp         // Widget exchange format
    GyeolWidgetPluginABI.h                 // Plugin ABI contract
    ButtonWidget.h
    ToggleWidget.h
    SliderWidget.h
    KnobWidget.h
    ComboBoxWidget.h
    LabelWidget.h
    TextInputWidget.h
    MeterWidget.h                          // Widget descriptors/painter/export metadata

  Editor/
    EditorHandle.cpp                       // Thin orchestrator only
    EditorHandleImpl.h / .cpp              // EditorHandle::Impl body

    Theme/
      GyeolCustomLookAndFeel.h / .cpp      // LAF, palette, and theme drawing

    Canvas/
      CanvasComponent.h / .cpp             // Canvas facade + external API
      CanvasRenderer.h / .cpp              // Canvas/widget rendering
      WidgetComponent.h / .cpp             // Single widget view + event forwarding
      MarqueeSelectionOverlay.h / .cpp     // Marquee overlay
      SnapGuideOverlay.h / .cpp            // Snap guide overlay

    Interaction/
      AlignDistributeEngine.h / .cpp       // Align/distribute calculations
      LayerOrderEngine.h / .cpp            // Layer ordering
      SelectionEngine.h / .cpp             // Selection utility logic
      SnapEngine.h / .cpp                  // Snap calculations
      EditorInteractionEngine.h / .cpp     // Shared interaction calculations

    Panels/
      AssetsPanel.h / .cpp                 // Assets panel
      EventActionPanel.h / .cpp            // Event-action panel
      ExportPreviewPanel.h / .cpp          // Export preview panel
      GridSnapPanel.h / .cpp               // Grid/snap settings panel
      HistoryPanel.h / .cpp                // History panel
      LayerTreePanel.h / .cpp              // Layer tree panel
      NavigatorPanel.h / .cpp              // Navigator panel
      PerformancePanel.h / .cpp            // Performance panel
      PropertyEditorFactory.h / .cpp       // Property editor factory
      PropertyPanel.h / .cpp               // Property panel
      ValidationPanel.h / .cpp             // Validation panel
      WidgetLibraryPanel.h / .cpp          // Widget library panel

    Shell/
      EditorLayoutCoordinator.h / .cpp     // Dock/layout coordination
      EditorPanelMediator.h / .cpp         // Panel-to-panel mediation
      EditorSettingsStore.h / .cpp         // UI settings load/save
      EditorModeCoordinator.h / .cpp       // Preview/run/sim mode transitions
      EditorHistoryBridge.h / .cpp         // Canvas mutation <-> history sync
      EditorCommandSystem.h / .cpp         // Shortcut/command system

    Perf/
      EditorPerfTracker.h / .cpp           // Editor performance tracking
      HitTestGrid.h / .cpp                 // Hit test acceleration
      PerfReportWriter.h / .cpp            // Perf report text writer
```

## Remove From Target Structure

These items are considered temporary, redundant, or unnecessary in the target layout.

- `Source/Gyeol/Render/` (`.gitkeep` only)
- [DONE] `Source/Gyeol/EditorHandle_Init_Snippet.txt` (temporary snippet)
- `Source/Gyeol/Gyeol_Roadmap_Dump.txt` (roadmap dump copy, if present)
- `Source/Gyeol/Editor/Panels/ValidationPanel_Test.h` (duplicate stub header)

## Refactor Change Set

Status Legend: [DONE] completed in current migration history.


This section tracks which files are expected to be deleted, added, or modified
when the structure above is applied.

### Files To Delete

- `Source/Gyeol/Render/.gitkeep`
- [DONE] `Source/Gyeol/EditorHandle_Init_Snippet.txt`
- `Source/Gyeol/Gyeol_Roadmap_Dump.txt` (if present)
- `Source/Gyeol/Editor/Panels/ValidationPanel_Test.h`
- `Source/Gyeol/Public/LayerModel.h` (if compatibility layer is no longer needed)
- `Source/Gyeol/Editor/GyeolCustomLookAndFeel.h` (moved)
- `Source/Gyeol/Editor/GyeolCustomLookAndFeel.cpp` (moved)

### Files To Add

- [DONE] `Source/Gyeol/Editor/EditorHandleImpl.h`
- [DONE] `Source/Gyeol/Editor/EditorHandleImpl.cpp`
- [DONE] `Source/Gyeol/Editor/Theme/GyeolCustomLookAndFeel.h`
- [DONE] `Source/Gyeol/Editor/Theme/GyeolCustomLookAndFeel.cpp`
- [DONE] `Source/Gyeol/Editor/Shell/EditorLayoutCoordinator.h`
- [DONE] `Source/Gyeol/Editor/Shell/EditorLayoutCoordinator.cpp`
- [DONE] `Source/Gyeol/Editor/Shell/EditorPanelMediator.h`
- [DONE] `Source/Gyeol/Editor/Shell/EditorPanelMediator.cpp`
- [DONE] `Source/Gyeol/Editor/Shell/EditorSettingsStore.h`
- [DONE] `Source/Gyeol/Editor/Shell/EditorSettingsStore.cpp`
- [DONE] `Source/Gyeol/Editor/Shell/EditorModeCoordinator.h`
- [DONE] `Source/Gyeol/Editor/Shell/EditorModeCoordinator.cpp`
- [DONE] `Source/Gyeol/Editor/Shell/EditorHistoryBridge.h`
- [DONE] `Source/Gyeol/Editor/Shell/EditorHistoryBridge.cpp`
- [DONE] `Source/Gyeol/Editor/Shell/EditorCommandSystem.h`
- [DONE] `Source/Gyeol/Editor/Shell/EditorCommandSystem.cpp`

### Files To Modify

- [DONE] `Source/Gyeol/Gyeol.h` (umbrella include path updates)
- `Source/Gyeol/Public/EditorHandle.h` (Impl split integration)
- `Source/Gyeol/Editor/EditorHandle.cpp` (reduced to thin orchestrator)
- [DONE] `Source/Gyeol/Editor/Canvas/CanvasComponent.h`
- [DONE] `Source/Gyeol/Editor/Canvas/CanvasComponent.cpp`
- [DONE] `Source/Gyeol/Editor/Canvas/CanvasRenderer.h`
- [DONE] `Source/Gyeol/Editor/Canvas/CanvasRenderer.cpp`
- [DONE] `Source/Gyeol/Editor/Canvas/WidgetComponent.h`
- [DONE] `Source/Gyeol/Editor/Canvas/WidgetComponent.cpp`
- [DONE] `Source/Gyeol/Editor/Panels/AssetsPanel.h` / `.cpp` (mediator callback integration)
- [DONE] `Source/Gyeol/Editor/Panels/EventActionPanel.h` / `.cpp` (mediator callback integration)
- [DONE] `Source/Gyeol/Editor/Panels/PropertyPanel.h` / `.cpp` (mediator callback integration)
- [DONE] `Source/Gyeol/Editor/Panels/LayerTreePanel.h` / `.cpp` (mediator callback integration)
- [DONE] `Source/Gyeol/Editor/Panels/HistoryPanel.h` / `.cpp` (history bridge integration)
- [DONE] `Source/Gyeol/Editor/Panels/NavigatorPanel.h` / `.cpp` (viewport bridge integration)
- [DONE] `Source/Gyeol/Editor/Panels/GridSnapPanel.h` / `.cpp` (snap bridge integration)
- [DONE] `Source/Gyeol/Editor/Panels/ValidationPanel.h` / `.cpp` (mediator callback integration)
- [DONE] `Source/Gyeol/Editor/Panels/PerformancePanel.h` / `.cpp` (perf bridge integration)
- [DONE] `Source/Gyeol/Editor/Panels/ExportPreviewPanel.h` / `.cpp` (export bridge integration)
- [DONE] `Source/Gyeol/Editor/Panels/WidgetLibraryPanel.h` / `.cpp` (canvas drop integration)
- [DONE] Project build files (`.jucer`/CMake/etc.) for new file registration
## Notes

- Move documentation files out of `Source` to `docs/` when possible.
- Move test files to `tests/` or a dedicated test target directory.
- This document defines the target architecture only; migration should be done incrementally in separate commits.
