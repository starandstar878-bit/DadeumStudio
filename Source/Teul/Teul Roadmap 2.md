# Teul Roadmap 2

> 목적: `Teul` 프로젝트가 최종적으로 안정화되었을 때의 예상 파일 구조를 먼저 고정한다.  
> 이 문서는 "현재 파일을 어디서 빼야 하는가"보다 "완성본에서 어떤 모듈이 어떤 책임을 가져야 하는가"를 기준으로 작성한다.

---

## 설계 원칙

- `EditorHandle` 계층은 orchestration만 담당한다.
- 장치 I/O, Control sync, rail sync, learn, assignment mutation은 editor coordinator 밖으로 분리한다.
- UI 컴포넌트는 `EditorHandleImpl.cpp` 내부 중첩 클래스로 두지 않는다.
- 상태 계산, badge/summary/message 포맷팅은 presenter/helper 계층으로 이동한다.
- 문서 모델, 런타임, UI, 직렬화, 검증은 디렉터리 경계를 명확히 유지한다.

---

## 최종 예상 루트 구조

```text
Source/Teul/
  Teul.h
  Teul Execution Plan.md
  Teul Roadmap.md
  Teul Roadmap 2.md
  Teul UI Roadmap.md
  Teul_FileStructure.md
  External Control Sources Device Profiles - KO.md

  Public/
    EditorHandle.h
    TeulFacade.h

  Model/
    TTypes.h
    TNode.h
    TPort.h
    TConnection.h
    TGraphDocument.h
    TGraphDocument.cpp
    Control/
      TControlTypes.h
      TControlStateSnapshot.h
      TControlStateSnapshot.cpp
      TControlAssignmentMutations.h
      TControlAssignmentMutations.cpp
      TControlProfileDiff.h
      TControlProfileDiff.cpp
    System/
      TSystemEndpointSnapshot.h
      TSystemEndpointSnapshot.cpp
      TSystemEndpointDiff.h
      TSystemEndpointDiff.cpp

  History/
    TCommand.h
    TCommands.h
    TDocumentHistory.cpp
    TDocumentHistory.h

  Registry/
    TNodeSDK.h
    TNodeRegistry.h
    TNodeRegistry.cpp
    Nodes/
      CoreNodes.h
      Core/
        SourceNodes.h
        FilterNodes.h
        FXNodes.h
        MathLogicNodes.h
        MidiNodes.h
        MixerNodes.h
        ModulationNodes.h
      External/
        ExternalNodeLoader.h
        ExternalNodeLoader.cpp

  Runtime/
    TGraphProcessor.h
    TGraphRuntime.h
    TGraphRuntime.cpp
    TNodeInstance.h
    Compiled/
      TCompiledGraph.h
      TCompiledGraph.cpp
      TCompiledNode.h
      TCompiledRoute.h
      TCompiledControlRoute.h
    Control/
      TRuntimeControlRouter.h
      TRuntimeControlRouter.cpp
      TRuntimeControlValueMap.h
      TRuntimeControlValueMap.cpp
    Metrics/
      TRuntimeStatsPresenter.h
      TRuntimeStatsPresenter.cpp

  Bridge/
    ITeulParamProvider.h
    TeulParamSurfaceBuilder.h
    TeulParamSurfaceBuilder.cpp
    TeulParamDispatch.h
    TeulParamDispatch.cpp

  IOControl/
    ControlDeviceService.h
    ControlDeviceService.cpp
    ControlBindingMatcher.h
    ControlBindingMatcher.cpp
    ControlInputAdapter.h
    MidiControlInputAdapter.h
    MidiControlInputAdapter.cpp
    MidiOutputDeviceManager.h
    MidiOutputDeviceManager.cpp
    RailSyncService.h
    RailSyncService.cpp
    SystemEndpointSnapshotBuilder.h
    SystemEndpointSnapshotBuilder.cpp

  Editor/
    EditorHandle.cpp
    EditorHandleImpl.h
    EditorHandleImpl.cpp
    TIssueState.h
    Canvas/
      TGraphCanvas.h
      TGraphCanvas.cpp
      CanvasRenderers.cpp
      CanvasOverlayLayer.h
      CanvasOverlayLayer.cpp
    Interaction/
      AlignmentEngine.cpp
      ConnectionInteraction.cpp
      ContextMenuController.cpp
      NodeFrameInteraction.cpp
      SelectionController.cpp
      EditorSelectionRouter.h
      EditorSelectionRouter.cpp
    Node/
      TNodeComponent.h
      TNodeComponent.cpp
      NodePreviewRenderer.h
      NodePreviewRenderer.cpp
    Port/
      TPortComponent.h
      TPortComponent.cpp
      TPortShapeLayout.h
      TPortVisuals.h
    Rails/
      RailPanel.h
      RailPanel.cpp
      RailViewModel.h
      RailViewModelBuilder.h
      RailViewModelBuilder.cpp
      RailInteractionHelpers.h
      RailInteractionHelpers.cpp
      RailBadgeFormatter.h
      RailBadgeFormatter.cpp
    Panels/
      DiagnosticsDrawer.h
      DiagnosticsDrawer.cpp
      NodeLibraryPanel.h
      NodeLibraryPanel.cpp
      NodePropertiesPanel.h
      NodePropertiesPanel.cpp
      PresetBrowserPanel.h
      PresetBrowserPanel.cpp
      Inspectors/
        PlaceholderInspectorPanel.h
        PlaceholderInspectorPanel.cpp
        ControlSourceInspectorPanel.h
        ControlSourceInspectorPanel.cpp
        SystemEndpointInspectorPanel.h
        SystemEndpointInspectorPanel.cpp
      Property/
        BindingSummaryPresenter.h
        BindingSummaryPresenter.cpp
        ParamEditorFactory.h
        ParamEditorFactory.cpp
        ParamValueFormatter.h
        ParamValueFormatter.cpp
    Search/
      SearchController.h
      SearchController.cpp
      CommandPaletteOverlay.cpp
      NodeSearchOverlay.cpp
      QuickAddOverlay.cpp
    Status/
      RuntimeStatusStrip.h
      RuntimeStatusStrip.cpp
      DocumentNoticeBanner.h
      DocumentNoticeBanner.cpp
      RuntimeMessagePresenter.h
      RuntimeMessagePresenter.cpp
    Theme/
      TeulPalette.h

  Preset/
    TPresetCatalog.h
    TPresetCatalog.cpp
    Preview/
      RecoveryPreviewBuilder.h
      RecoveryPreviewBuilder.cpp
      StatePresetPreviewBuilder.h
      StatePresetPreviewBuilder.cpp

  Serialization/
    TSerializer.h
    TSerializer.cpp
    TFileIo.h
    TFileIo.cpp
    TPatchPresetIO.h
    TPatchPresetIO.cpp
    TStatePresetIO.h
    TStatePresetIO.cpp
    Schema/
      TSchemaMigrationReport.h
      TSchemaMigrationRules.cpp

  Export/
    TExport.h
    TExport.cpp
    TExportBuilder.h
    TExportBuilder.cpp

  Verification/
    TVerificationFixtures.h
    TVerificationFixtures.cpp
    TVerificationStimulus.h
    TVerificationStimulus.cpp
    TVerificationParity.h
    TVerificationParity.cpp
    TVerificationCompiledParity.h
    TVerificationCompiledParity.cpp
    TVerificationGoldenAudio.h
    TVerificationGoldenAudio.cpp
    TVerificationStress.h
    TVerificationStress.cpp
    TVerificationBenchmark.h
    TVerificationBenchmark.cpp
    CompiledParity/
      CompiledParityCaseHost.cpp
    GoldenAudio/
      RepresentativePrimary/
        ...
```

---

## 핵심 경계

### 1. Public

- 외부에서 접근 가능한 진입점만 둔다.
- `EditorHandle.h`는 UI editor entry로 유지한다.
- 장기적으로 editor 외부 orchestration 진입점이 필요하면 `TeulFacade.h`를 둔다.

### 2. Model

- 순수 데이터와 문서 mutation 규칙만 둔다.
- UI 컴포넌트, JUCE device callback, runtime sink는 두지 않는다.
- `Control/`, `System/` 하위로 나눠 control source/device profile/system endpoint 구조를 정리한다.

### 3. IOControl

- 현재 `EditorHandleImpl`에 섞여 있는 장치 감지, learn, profile sync, MIDI input callback, MIDI output open/close, rail sync를 전부 받는다.
- editor는 이 계층을 호출만 하고 직접 장치 상태를 관리하지 않는다.

### 4. Editor/Rails

- rail card 생성, badge/issue/subtitle 계산, rail drag token 규칙, rail component 렌더링을 전부 여기서 처리한다.
- `EditorHandleImpl`은 `RailPanel`에 데이터 공급과 selection routing만 한다.

### 5. Editor/Panels/Inspectors

- `PlaceholderInspectorPanel`
- `ControlSourceInspectorPanel`
- `SystemEndpointInspectorPanel`
- inspector는 독립 component로 유지하고, 문서 변경은 service/mutation helper를 통해서만 수행한다.

### 6. Editor/Status

- runtime strip, document notice, runtime message formatting을 분리한다.
- 상태 문구 계산과 색상 정책은 presenter가 담당한다.

### 7. Preset/Preview

- recovery/state preset preview 로직을 editor coordinator 밖으로 분리한다.
- preset browser panel은 preview builder를 호출만 한다.

### 8. Runtime

- compiled graph, control route, runtime stats presenter를 하위 디렉터리로 분리한다.
- `TGraphRuntime.cpp`가 단일 거대 파일이 되지 않도록 compile/runtime-control/metrics를 쪼갠다.

---

## EditorHandle 최종 역할

완성 시점의 `EditorHandleImpl`은 아래만 남는 것을 목표로 한다.

- `doc`, `runtime`, 주요 panel/component 소유
- constructor/destructor에서 wiring
- `layout()`
- `timerCallback()`
- `rebuildAll()`
- selection / inspection routing
- `refreshFromDocument()` 같은 top-level refresh orchestration

다음 책임은 남기지 않는다.

- Control learn/profile sync
- MIDI input callback
- MIDI output device open/close
- rail badge/subtitle/helper 계산
- inspector component 구현
- runtime/document status strip 구현
- preset/recovery preview 계산

---

## 1차 리팩터 우선순위

1. `IOControl/` 신설
2. `Editor/Rails/` 신설
3. `Editor/Panels/Inspectors/` 신설
4. `Editor/Status/` 신설
5. `Preset/Preview/` 신설
6. `Runtime/Compiled/`, `Runtime/Control/`, `Runtime/Metrics/` 세분화
7. 마지막에 `EditorHandleImpl` 얇게 정리

---

## 완료 판정 기준

- `EditorHandleImpl.cpp`는 orchestration 외 로직이 거의 없는 coordinator 수준이어야 한다.
- 중첩 UI 클래스가 없어야 한다.
- 장치 I/O 관련 코드는 `IOControl/` 밖에 없어야 한다.
- rail/helper/presenter 계층이 `Editor/Rails`, `Editor/Status`로 분리되어야 한다.
- preset preview 계산이 editor panel 바깥으로 이동해야 한다.
- 런타임/모델/UI/직렬화/검증 경계가 디렉터리 기준으로 읽혀야 한다.