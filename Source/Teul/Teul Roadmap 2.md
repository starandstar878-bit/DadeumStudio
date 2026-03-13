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

### 2. Editor
- 문서를 사람이 보고 수정하는 표현 계층이다.
- canvas, panel, inspector, rail, selection, drag, menu를 담당한다.
- 사용자가 노드를 쉽게 추가하고 배치하고 연결하도록 편의 기능을 제공한다.
- 노드, 포트, 연결선, rail 같은 시각 요소의 표현 규칙을 관리한다.
- 문서를 직접 소유하기보다 `Document` 계층의 현재 문서를 표시하고 편집한다.

### 3. Runtime
- 문서를 실행 가능한 graph로 바꾼다.
- audio, MIDI, control을 처리한다.
- 런타임 통계와 runtime-safe parameter apply를 담당한다.

### 4. Bridge
- Ieum 등 다른 프로젝트와의 연결을 담당한다.
- export, parameter surface, 외부 소비용 인터페이스를 담당한다.

---

## 1차 목표

리라이트 1차 목표는 세부 폴더를 많이 만드는 것이 아니라, 아래를 먼저 달성하는 것이다.

- `Document`, `Editor`, `Runtime`, `Bridge` 4계층 경계 고정
- `EditorHandleImpl`에서 orchestration 외 책임 제거 준비
- 현재 `Model`, `Serialization`, `History`에 흩어진 문서 책임을 `Document` 중심으로 재정리
- 현재 `EditorHandleImpl`에 몰린 editor UI와 interaction 책임을 `Editor` 폴더 구조로 재정리
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
  EditorHandle.cpp
  EditorHandleImpl.h
  EditorHandleImpl.cpp
  TIssueState.h
  Canvas/
    TGraphCanvas.h
    TGraphCanvas.cpp
    CanvasRenderers.cpp
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
  Port/
    TPortShapeLayout.h
    TPortVisuals.h
  Theme/
    TeulPalette.h
```

---

## Editor 최소 파일 설명

### EditorHandle.cpp / EditorHandleImpl.h / EditorHandleImpl.cpp
- editor 전체 orchestration
- `Document`의 현재 문서를 editor에 연결
- canvas, panel, search, diagnostics 같은 UI 객체 생성 및 배치
- refresh, layout, selection routing, rebuild scheduling 담당
- 여기에는 business logic를 쌓지 않고 coordinator 역할만 남긴다

### Canvas/TGraphCanvas.h / TGraphCanvas.cpp
- 그래프 편집의 중심 canvas
- node 배치, selection box, zoom, pan, drag, frame 기반 interaction 처리
- 문서의 node/connection을 실제 화면 객체로 보여주는 중심 컴포넌트

### Canvas/CanvasRenderers.cpp
- node, connection, background grid, overlay 같은 저수준 렌더링 코드
- 그리는 규칙을 한 곳에 모아서 시각 일관성 유지
- 나중에 최적화가 필요하면 여기서 draw pass를 다듬는다

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
- quick add, node search, command palette 같은 탐색/실행 기능의 공통 제어
- 검색과 editor action 사이를 연결

### Port/TPortShapeLayout.h
- 포트 위치 계산 규칙
- node body 내에서 포트를 어디에 배치할지 정의

### Port/TPortVisuals.h
- 포트 색상, 크기, 타입별 외형 규칙 정의
- audio/MIDI/control 차이를 시각적으로 통일

### Theme/TeulPalette.h
- editor 전반의 색상 시스템 정의
- 선택, hover, warning, accent 색상 일관성 유지

### TIssueState.h
- invalid, degraded, missing 같은 상태를 editor가 공통으로 다루도록 하는 표시용 상태 타입

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

## 현재 코드 기준 이동 방향

현재 흩어진 책임은 장기적으로 아래처럼 정리한다.

- `Model/`의 순수 문서 구조는 `Document/`로 이동
- `History/`의 undo/redo는 `DocumentHistory`로 흡수
- `Serialization/`의 문서 저장/불러오기는 `DocumentSerializer`, `DocumentStore` 중심으로 재구성
- migration 관련 책임은 `DocumentMigration`으로 통합
- `EditorHandleImpl`에 몰린 canvas/panel/interaction/renderer 책임은 `Editor/` 하위 구조로 분리

즉, 문서 관련 코드는 최종적으로 `Document` 폴더 하나에서 읽히고, editor 관련 코드는 `Editor` 폴더 하나에서 읽히게 만드는 것이 목표다.

---

## 이후 단계

이 문서 기준에서 다음 설계는 순서대로 진행한다.

1. `Document` 폴더 구조 확정
2. `Editor` 최소 구조 확정
3. `Runtime` 최소 구조 정의
4. `Bridge` 최소 구조 정의
5. 마지막에 각 계층 사이 공개 인터페이스 정리

---

## 완료 판정 기준

- 최상위 계층이 `Document`, `Editor`, `Runtime`, `Bridge` 4개로 설명 가능해야 한다.
- 문서 lifecycle 책임이 `Document` 계층 하나로 읽혀야 한다.
- editor 표현과 interaction 책임이 `Editor` 계층 하나로 읽혀야 한다.
- undo/redo, migration, serialization, autosave가 문서 계층 책임으로 정리되어야 한다.
- canvas, panel, interaction, render rule이 editor 계층 책임으로 정리되어야 한다.
- `EditorHandleImpl`은 문서 처리와 editor 세부 구현을 직접 품지 않아야 한다.