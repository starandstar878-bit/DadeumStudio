# Teul Roadmap 2

> 목적: `Teul` 리라이트 전에 최상위 구조를 단순하게 다시 고정한다.
> 이 문서는 세부 클래스 분해안보다, 시스템을 어떤 계층으로 나누고 각 계층이 무엇을 책임지는지에 집중한다.

---

## 최상위 구조

`Teul`은 우선 아래 4개 계층으로 나눈다.

```text
Source/Teul/
  Document/
  Editor/
  Runtime/
  Bridge/
```

### 1. Document
- 문서를 시스템에 맞는 형태로 받아들인다.
- 문서를 런타임 동안 관리한다.
- history snapshot을 관리하고 undo/redo를 수행한다.
- 버전을 검사하고 필요하면 upgrade/migration 한다.
- ValueTree 양식과 외부 직렬화 형식을 관리한다.
- 저장, 불러오기, autosave, recovery를 담당한다.
- 다른 유저가 만든 `.teul` 파일을 해석해 현재 시스템 문서로 받아들이는 책임도 여기에 둔다.

### 2. Editor
- 문서를 사람이 보고 수정하는 표현 계층이다.
- canvas, panel, inspector, rail, selection, drag, menu를 담당한다.
- 사용자가 노드를 쉽게 추가하고 배치하고 연결하도록 편의 기능을 제공한다.
- 노드, 포트, 연결선, rail 같은 시각 요소의 표현 규칙을 관리한다.
- 문서를 직접 소유하기보다 `Document` 계층의 현재 문서를 표시하고 편집한다.

### 3. Runtime
- 문서를 실행 가능한 graph로 바꾼다.
- 실행 중인 오디오 그래프를 직접 소유하고 관리한다.
- audio, MIDI, control을 처리한다.
- 런타임 이벤트와 장치 변화를 관리한다.
- 성능 최적화와 이상 상태 감지를 담당한다.
- 새 DSP node 도입 전 기본 검증과 호환성 확인을 담당한다.

### 4. Bridge
- Teul 내부 구조를 외부 시스템이 이해하는 형식으로 바꾼다.
- 외부 시스템의 요청과 응답을 Teul 내부 구조로 번역한다.
- code generation, 외부용 JSON 생성/해석, Ieum/Gyeol/Naru 같은 외부 프로젝트와의 소통을 담당한다.
- 단, `.teul` 파일 자체의 해석은 `Bridge`가 아니라 `Document` 책임으로 둔다.

---

## 1차 목표

리라이트 1차 목표는 세부 폴더를 많이 만드는 것이 아니라, 아래를 먼저 달성하는 것이다.

- `Document`, `Editor`, `Runtime`, `Bridge` 4계층 경계 고정
- 기존 `EditorHandleImpl` 구조를 제거하고 `TTeulEditor` 중심으로 단순화
- 현재 `Model`, `Serialization`, `History`에 흩어진 문서 책임을 `Document` 중심으로 재정리
- 현재 `EditorHandleImpl`에 몰린 editor UI와 interaction 책임을 `Editor` 폴더 구조로 재정리
- 현재 `TGraphRuntime` 주변에 몰린 실행 graph, 런타임 이벤트, 장치, 검증 책임을 `Runtime/AudioGraph`, `Runtime/IOControl` 구조로 재정리
- 외부 연동, codegen, 외부용 JSON 책임을 `Bridge` 폴더 구조로 재정리
- 나머지 세분화는 2차 작업으로 미룸

---

## Document 계층 역할

`Document`는 단순 데이터 보관소가 아니라 문서 lifecycle 전체를 책임지는 계층으로 본다.

핵심 책임은 아래와 같다.

- 현재 문서의 in-memory 상태 보유
- node/port/connection/control source/system endpoint 구조 정의
- history snapshot 저장
- undo/redo 수행
- revision/version 관리
- 구버전 문서 migration
- ValueTree 양식 관리
- 외부 직렬화/역직렬화
- 파일 저장/불러오기
- autosave/recovery 관리
- 다른 유저가 만든 `.teul` 파일 import 및 해석

즉, 문서가 들어오면 시스템이 이해할 수 있는 현재 포맷으로 바꾸고, 실행 중에는 그 문서를 관리하고, 필요할 때 다시 저장 가능한 형태로 내보내는 계층이다.

---

## Document 폴더 최소 파일 구조

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

### TDocumentTypes.h
- 문서 공통 타입 정의
- `Node`, `Port`, `Connection`, `ControlSource`, `SystemEndpoint`
- `DocumentVersion`, `DocumentId`, `RevisionId`
- ValueTree property key 상수

### TTeulDocument.h / TTeulDocument.cpp
- 정규화된 in-memory 문서 모델
- 현재 시스템이 실제로 사용하는 문서 객체
- revision 관리
- 문서 무결성 유지
- 기본 mutation API 제공

### TDocumentHistory.h / TDocumentHistory.cpp
- 문서 snapshot stack 관리
- undo / redo
- history depth 제한
- snapshot push/pop 정책 관리

### TDocumentSerializer.h / TDocumentSerializer.cpp
- `TTeulDocument <-> ValueTree`
- `TTeulDocument <-> JSON`
- 외부 저장 포맷과 내부 문서 포맷 간 변환
- `.teul` 파일의 실질적인 해석 규칙 담당

### TDocumentMigration.h / TDocumentMigration.cpp
- 문서 버전 검사
- 구버전 문서 upgrade
- migration step 적용
- load 시 현재 버전으로 정규화

### TDocumentStore.h / TDocumentStore.cpp
- 파일 입출력 facade
- 저장 / 불러오기 / autosave / recovery
- serializer, migration, history를 묶는 상위 진입점

---

## Editor 계층 역할

`Editor`는 문서를 시각적으로 보여주고 편집하는 계층이다. 단순히 그리는 것만이 아니라, 사용자가 오디오 그래프를 빠르고 실수 적게 조작할 수 있도록 편의 기능을 제공해야 한다.

핵심 책임은 아래와 같다.

- 현재 문서를 canvas 위에 시각적으로 표현
- node 추가, 이동, 선택, 삭제
- port 간 연결 생성/삭제
- control assignment drag/drop
- rail, panel, inspector 표시
- quick add, search, context menu 같은 편의 기능
- node, port, connection의 시각 규칙 관리
- hover, selection, drag feedback 제공
- 문서 변경을 editor interaction으로 연결
- viewport, zoom, pan, redraw를 안정적으로 처리

즉, `Editor`는 문서를 사람이 다루기 쉬운 그래프 편집기로 바꾸는 계층이다.

---

## Editor 폴더 최소 파일 구조

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

---

## Editor 최소 파일 설명

### TTeulEditor.h / TTeulEditor.cpp
- editor 전체 orchestration 진입점
- `Document`의 현재 문서를 editor에 연결
- canvas, panel, search, diagnostics 같은 UI 객체 생성 및 배치
- refresh, layout, selection routing, rebuild scheduling 담당
- 기존 `EditorHandle` / `EditorHandleImpl` 이중 구조를 없애고 단일 editor entry로 유지한다

### Canvas/TGraphCanvas.h / TGraphCanvas.cpp
- 그래프 편집의 중심 canvas
- node view와 connection view를 관리
- zoom, pan, selection box, drag 상태, viewport redraw를 조정
- 즉, canvas는 개별 요소를 직접 다 그리는 곳이라기보다 화면 전체 그래프 표현을 orchestration 하는 곳으로 둔다

### Renderers/TNodeRenderer.h / .cpp
- node body 렌더링 담당
- node 제목, 본문, 상태 강조, 선택 상태 표시
- 내부에서 `TPortRenderer`를 사용해 포트 외형을 함께 그린다

### Renderers/TPortRenderer.h / .cpp
- port 외형 렌더링 담당
- port 타입별 색상, 크기, 상태, hover/active 표시 규칙 관리
- hit area와 포트 표시 규칙을 일관되게 유지
- 포트 위치 계산과 포트 외형 계산은 우선 이 파일 내부 책임으로 둔다
- bus channel bounds, mono/bundle hit test 같은 geometry helper도 별도 파일로 빼지 않고 여기 포함한다

### Renderers/TConnectionRenderer.h / .cpp
- connection line 렌더링 담당
- 방향성, hover, selected, invalid, active 상태 표현
- spline 또는 curve 규칙을 한 곳에서 관리

### Interaction/SelectionController.cpp
- 단일 선택, 다중 선택, marquee 선택, 선택 변경 정책
- keyboard modifier를 포함한 selection 동작 규칙 담당

### Interaction/ConnectionInteraction.cpp
- 포트 간 연결 시작, 미리보기, 연결 확정, 연결 취소
- control assignment drag/drop 같은 연결 계열 interaction 담당

### Interaction/ContextMenuController.cpp
- 우클릭 메뉴, quick action, node 생성/삭제/변환 메뉴
- 선택 상태에 따라 적절한 편집 명령 제공

### Panels/NodeLibraryPanel.h / .cpp
- node 목록 탐색 및 추가 UI
- 카테고리별 node 삽입

### Panels/NodePropertiesPanel.h / .cpp
- 선택된 node의 파라미터 편집
- binding summary, assignment drop target, param editor 표시

### Panels/DiagnosticsDrawer.h / .cpp
- runtime/verification 관련 보조 정보 표시
- editor 본문을 가리지 않는 보조 도구 역할

### Search/SearchController.h / .cpp
- quick add, node search, command palette 같은 탐색 기반 기능의 공통 조정자
- 검색 입력, 결과 선택, editor action 연결을 담당

### Theme/TeulPalette.h
- editor 전반의 색상 시스템 정의
- 선택, hover, warning, accent 색상 일관성 유지

### TIssueState.h
- invalid, degraded, missing 같은 상태를 editor가 공통으로 다루도록 하는 표시용 상태 타입
- node, port, connection, rail, endpoint가 같은 문제 상태 규칙을 공유하게 한다

---

## Editor 내부 포함 관계

editor 내부 관계는 우선 아래처럼 단순하게 잡는다.

```text
TTeulEditor
  -> TGraphCanvas
       -> TNodeRenderer
            -> TPortRenderer
       -> TConnectionRenderer
```

원칙은 아래와 같다.

- `TTeulEditor`는 editor 전체를 조립한다.
- `TGraphCanvas`는 node와 connection을 화면에 배치하고 그리도록 조정한다.
- `TNodeRenderer`는 node를 그리고 내부 포트 표현에 `TPortRenderer`를 사용한다.
- `TConnectionRenderer`는 canvas 좌표계 기준으로 연결선을 그린다.
- 포트 위치/형상 geometry는 우선 `TPortRenderer` 내부 helper로 처리한다.

즉, `Canvas -> Node, Connection`, 그리고 `Node -> PortRenderer` 구조를 기본으로 한다.

---

## Editor가 제공해야 하는 편의 기능

최소 editor라도 아래 기능은 있어야 한다.

- quick add
- node search
- drag to connect
- drag to move
- marquee selection
- multi-select
- context menu
- zoom / pan
- snap 또는 정렬 보조
- 선택 대상 properties 표시
- hover/selection/drag feedback
- 잘못된 연결이나 누락 상태에 대한 시각적 경고

이 기능들은 전부 고급 기능이 아니라, 그래프 편집기를 usable하게 만드는 기본 기능으로 본다.

---

## Editor의 시각 규칙

Editor는 단순히 데이터를 그리는 것이 아니라, 사용자가 빨리 읽고 빨리 연결할 수 있도록 시각 규칙을 강하게 가져야 한다.

최소 원칙은 아래와 같다.

- node는 타입과 상태가 한눈에 구분되어야 한다.
- port는 데이터 타입별 색과 위치 규칙이 일관되어야 한다.
- connection은 방향과 활성 상태가 읽혀야 한다.
- selection, hover, drag preview는 즉시 눈에 띄어야 한다.
- invalid, degraded, missing 상태는 일반 상태와 확실히 구분되어야 한다.
- rail, canvas, panel의 위계가 화면에서 자연스럽게 읽혀야 한다.

---

## Editor 최적화 기준

최소 구조라도 editor는 어느 정도 최적화가 되어 있어야 한다.

- 전체 rebuild를 남발하지 않는다.
- node 이동 시 connection redraw 범위를 제한한다.
- hover나 meter 같은 실시간 요소는 필요한 부분만 repaint 한다.
- selection 변경과 document 변경을 분리해서 refresh한다.
- layout 계산과 paint 계산을 가능한 한 분리한다.
- 포트/연결선 geometry는 재사용 가능한 형태로 캐시할 수 있게 설계한다.

즉, 1차 editor는 화려함보다 반응성과 안정성을 우선한다.

---

## Runtime 계층 역할

`Runtime`은 문서를 실행 가능한 오디오 그래프로 바꾸고, 실행 중인 그래프와 장치 상태를 안전하게 유지하는 계층이다.

핵심 책임은 크게 두 덩어리로 나눈다.

- `AudioGraph`
  - 문서를 읽어 실행 가능한 graph로 컴파일
  - 실행 중인 graph를 직접 소유하고 처리
  - 성능 최적화, diagnostics, validator 관리
- `IOControl`
  - 런타임 중 발생하는 I/O 및 control device 이벤트 관리
  - 장치 연결/제거, 채널 변화, sample rate/block size 변화 반영
  - runtime graph rebuild가 필요한 외부 상태 변화를 runtime에 전달

즉, `Runtime`은 단순 DSP 처리기 하나가 아니라, 문서를 실행 상태로 바꾸는 `AudioGraph`와 실행 중 외부 변화를 관리하는 `IOControl`을 함께 가진 계층이다.

---

## Runtime 폴더 최소 파일 구조

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

---

## Runtime 최소 파일 설명

### TTeulRuntime.h / TTeulRuntime.cpp
- runtime 전체 진입점
- 현재 실행 graph와 runtime 상태 소유
- `AudioGraph`와 `IOControl`을 묶는 orchestration 담당
- document revision/runtime revision 기준으로 rebuild 판단
- event 반영, diagnostics 갱신, processor와 compiler orchestration 담당

### AudioGraph/TGraphCompiler.h / TGraphCompiler.cpp
- `Document -> 실행 가능한 graph` 변환
- node, connection, control route를 runtime용 구조로 해석
- compile 결과물을 `TTeulRuntime`에 전달

### AudioGraph/TGraphProcessor.h / TGraphProcessor.cpp
- 실제 audio / MIDI / control 처리
- block 단위 DSP 실행
- runtime-safe parameter apply
- compile된 graph를 따라 process 수행

### AudioGraph/TRuntimeDiagnostics.h / TRuntimeDiagnostics.cpp
- clip, overload, invalid route, fallback 같은 문제 감지
- runtime stats와 경고 상태 정리
- editor나 bridge가 읽을 수 있는 runtime 상태 요약 제공

### AudioGraph/TRuntimeValidator.h / TRuntimeValidator.cpp
- 새 DSP node나 graph가 runtime에 들어오기 전 기본 검증
- 필수 포트, 지원 타입, 기본 처리 가능 여부, 최소 안전성 검사
- verification 기준의 runtime 측 진입점 역할

### IOControl/TRuntimeEvent.h
- 런타임 이벤트 타입 정의
- 예: device added/removed, channel layout changed, sample rate changed, rebuild requested, control device changed

### IOControl/TRuntimeEventQueue.h / TRuntimeEventQueue.cpp
- 런타임 이벤트 push / drain 통로
- processing 중 직접 구조를 흔들지 않고 안전하게 상태를 반영하도록 돕는다

### IOControl/TRuntimeDeviceManager.h / TRuntimeDeviceManager.cpp
- audio/MIDI/control 장치 상태 관리
- 장치 연결/제거, 활성 I/O, 채널 수, 기본 device 정보 추적
- 장치 변화 결과를 runtime event 또는 runtime state로 반영

---

## Runtime 내부 포함 관계

runtime 내부 관계는 우선 아래처럼 단순하게 잡는다.

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

원칙은 아래와 같다.

- `TTeulRuntime`는 runtime 전체를 조립한다.
- `AudioGraph`는 document를 실행 graph로 만들고 실제 DSP 실행을 담당한다.
- `IOControl`은 장치와 런타임 이벤트를 관리하고, 필요한 변화만 runtime에 전달한다.
- `IOControl`이 직접 DSP graph를 소유하지는 않는다.
- `AudioGraph`가 장치 감지 로직을 직접 품지 않는다.

즉, graph 실행은 `AudioGraph`, 실행 중 외부 상태 관리는 `IOControl`로 나눈다.

---

## Bridge 계층 역할

`Bridge`는 Teul 내부 구조와 외부 시스템 사이를 번역하는 계층이다.

핵심 책임은 아래와 같다.

- Teul 내부 구조를 외부용 데이터로 변환
- 외부 요청과 응답을 Teul 내부 구조로 변환
- code generation
- 외부용 JSON 생성과 해석
- Ieum, Gyeol, Naru 같은 외부 프로젝트별 adapter 제공
- 외부에서 읽기 쉬운 facade 제공

중요한 경계는 아래와 같다.

- `.teul` 파일 해석은 `Document`
- Ieum/Gyeol/Naru용 JSON 또는 외부 protocol 해석은 `Bridge`

즉, `Bridge`는 문서 저장 포맷이 아니라 외부 연동 포맷을 담당한다.

---

## Bridge 폴더 최소 파일 구조

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

---

## Bridge 최소 파일 설명

### TTeulBridge.h / TTeulBridge.cpp
- bridge 전체 진입점
- 외부 시스템별 bridge와 json/codegen 기능을 묶는 facade
- 외부에서 Teul을 호출할 때 가장 먼저 들어오는 통합 진입점

### TBridgeJsonCodec.h / TBridgeJsonCodec.cpp
- 외부 연동용 JSON 생성과 해석
- 내부 구조와 외부 payload 간 변환
- `.teul` 문서 포맷이 아니라 bridge payload 포맷 담당

### TBridgeCodegen.h / TBridgeCodegen.cpp
- code generation 담당
- document/runtime metadata를 외부 코드나 설정 조각으로 변환
- 외부 프로젝트에서 재사용할 산출물 생성

### TIeumBridge.h / TIeumBridge.cpp
- Ieum 전용 adapter
- Ieum이 필요로 하는 payload, param surface, 요청/응답 형식 제공

### TGyeolBridge.h / TGyeolBridge.cpp
- Gyeol 전용 adapter
- Gyeol과 Teul 사이의 번역 규칙 담당

### TNaruBridge.h / TNaruBridge.cpp
- Naru 전용 adapter
- Naru와 Teul 사이의 번역 규칙 담당

---

## Bridge 내부 포함 관계

bridge 내부 관계는 우선 아래처럼 단순하게 잡는다.

```text
TTeulBridge
  -> TBridgeJsonCodec
  -> TBridgeCodegen
  -> TIeumBridge
  -> TGyeolBridge
  -> TNaruBridge
```

원칙은 아래와 같다.

- `TTeulBridge`는 외부 연동 계층 전체를 조립한다.
- 각 외부 프로젝트는 전용 bridge로 분리한다.
- 공통 JSON 해석과 생성은 `TBridgeJsonCodec`이 맡는다.
- 공통 code generation은 `TBridgeCodegen`이 맡는다.
- 외부 프로젝트별 규칙을 `TTeulBridge` 안에 섞지 않는다.

즉, bridge 공통 기능과 외부 프로젝트별 adapter를 나눠 유지한다.

---

## Document / Editor / Runtime / Bridge sync 기준

네 계층 sync는 한 파일에 몰지 않고, 각 계층의 대표 진입점끼리 맞물리는 구조로 둔다.

```text
Document/TTeulDocument
  <-> Editor/TTeulEditor
  <-> Runtime/TTeulRuntime
         -> Runtime/AudioGraph/TGraphCompiler
         -> Runtime/IOControl/TRuntimeEventQueue
  <-> Bridge/TTeulBridge
```

### 문서 기준점
- `Document/TTeulDocument`
- 현재 문서 상태의 source of truth

### 문서-에디터 sync
- `Editor/TTeulEditor`
- 문서 revision 변화를 보고 canvas/panel refresh
- 사용자 편집 결과를 문서 mutation으로 반영

### 문서-런타임 sync
- `Runtime/TTeulRuntime`
- 문서 revision/runtime revision 변화를 보고 rebuild 요청
- 실제 graph 변환은 `Runtime/AudioGraph/TGraphCompiler`가 담당

### 런타임 이벤트-그래프 sync
- `Runtime/IOControl/TRuntimeEventQueue`
- 장치 변화나 control device 변화 같은 런타임 이벤트를 queue로 전달
- `Runtime/TTeulRuntime`가 drain 후 graph rebuild 또는 상태 갱신 반영

### 문서-브리지 sync
- `Bridge/TTeulBridge`
- document를 외부 payload, codegen 결과, 외부 프로젝트용 구조로 변환
- 외부 응답이나 요청을 내부에서 처리 가능한 구조로 변환

### 런타임-에디터 연동
- `Runtime`이 UI를 직접 건드리면 안 된다
- `Runtime`은 diagnostics/stats/state를 노출한다
- `Editor`는 그 상태를 읽어 표시한다

### 브리지-문서 경계
- `.teul` import/export는 `Document`
- 외부 프로젝트용 import/export는 `Bridge`

즉, `Document`가 중심 상태이고 `Editor`, `Runtime`, `Bridge`는 각각 `Document`를 기준으로 sync 한다. `Runtime` 내부에서는 `AudioGraph`와 `IOControl`이 역할을 나눠 sync 한다.

---

## 현재 코드 기준 이동 방향

현재 흩어진 책임은 장기적으로 아래처럼 정리한다.

- `Model/`의 순수 문서 구조는 `Document/`로 이동
- `History/`의 undo/redo는 `DocumentHistory`로 흡수
- `Serialization/`의 문서 저장/불러오기는 `DocumentSerializer`, `DocumentStore` 중심으로 재구성
- migration 관련 책임은 `DocumentMigration`으로 통합
- `EditorHandleImpl`에 몰린 canvas/panel/interaction/renderer 책임은 `Editor/` 하위 구조로 분리
- `EditorHandle` / `EditorHandleImpl`은 제거하고 `TTeulEditor` 단일 진입점으로 정리
- 기존 `TGraphRuntime`에 몰린 compile/process/diagnostics/validator 책임은 `Runtime/AudioGraph/`로 분리
- 기존 장치 감지, 런타임 이벤트, control device state 책임은 `Runtime/IOControl/`로 분리
- Ieum/외부 codegen/외부 JSON 관련 책임은 `Bridge/`로 이동
- `.teul` 파일 해석과 migration은 `Document/`에 남긴다

즉, 문서 관련 코드는 최종적으로 `Document` 폴더 하나에서 읽히고, editor 관련 코드는 `Editor` 폴더 하나에서 읽히고, runtime 관련 코드는 `Runtime/AudioGraph`, `Runtime/IOControl` 경계로 읽히고, 외부 연동 관련 코드는 `Bridge` 폴더 하나에서 읽히게 만드는 것이 목표다.

---

## 이후 단계

이 문서 기준에서 다음 설계는 순서대로 진행한다.

1. `Document` 폴더 구조 확정
2. `Editor` 최소 구조 확정
3. `Runtime` 최소 구조 확정
4. `Bridge` 최소 구조 확정
5. 마지막에 각 계층 사이 공개 인터페이스 정리

---

## 완료 판정 기준

- 최상위 계층이 `Document`, `Editor`, `Runtime`, `Bridge` 4개로 설명 가능해야 한다.
- 문서 lifecycle 책임이 `Document` 계층 하나로 읽혀야 한다.
- editor 표현과 interaction 책임이 `Editor` 계층 하나로 읽혀야 한다.
- runtime 실행 graph 책임이 `Runtime/AudioGraph`로 읽혀야 한다.
- 런타임 이벤트와 장치 상태 책임이 `Runtime/IOControl`로 읽혀야 한다.
- 외부 연동과 codegen 책임이 `Bridge` 계층 하나로 읽혀야 한다.
- undo/redo, migration, serialization, autosave가 문서 계층 책임으로 정리되어야 한다.
- canvas, panel, interaction, render rule이 editor 계층 책임으로 정리되어야 한다.
- compile, process, diagnostics, validator가 `Runtime/AudioGraph` 책임으로 정리되어야 한다.
- runtime event, device state, control device change가 `Runtime/IOControl` 책임으로 정리되어야 한다.
- 외부 JSON, codegen, Ieum/Gyeol/Naru adapter가 `Bridge` 책임으로 정리되어야 한다.
- `TTeulEditor`는 editor orchestration만 담당해야 한다.
- `TTeulRuntime`는 runtime orchestration만 담당해야 한다.
- `TTeulBridge`는 bridge orchestration만 담당해야 한다.