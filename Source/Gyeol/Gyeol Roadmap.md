# Gyeol 로드맵
업데이트: 2026-03-01

## Phase 개요
- [x] Phase 0: 코어 모델/액션 안정화
- [x] Phase 1: 상호작용 엔진/선택/리사이즈
- [x] Phase 2: Export MVP
- [x] Phase 3: Layer/Group/Inspector/스냅 기반 편집 UX
- [~] Phase 3.5: 고급 Editor UX 패널/보완
- [~] Phase 3.8: Preview/Run 모드 분리(MVP)
- [x] Phase 4: 런타임 Event/Action 실행 엔진
- [x] Phase 5: 데이터 바인딩/상태 시스템
- [~] Phase 6: Export 런타임 브리지
- [ ] Phase 7: 확장 SDK(외부 위젯/패키지)
- [~] Phase 8: 안정화/성능/릴리스

---

## Phase 0: 코어 모델/액션 안정화
### 구현 완료
- [x] `DocumentModel`, `EditorStateModel`, Validator 기본 구조 확정
- [x] 액션 최소 집합 정리(`Create/Delete/SetProps/SetBounds/Reparent/Reorder`)
- [x] Public API 단순화 + 내부 kind 분기 유지
- [x] JSON round-trip 안정화(기본값 포함)
- [x] include 경로 정리(`Gyeol/...`)

### 남은 항목
- [ ] 없음(유지보수만)

---

## Phase 1: 상호작용 엔진/선택/리사이즈
### 구현 완료
- [x] `EditorInteractionEngine` 중심 구조 확정
- [x] 다중 선택 union bounds 이동/리사이즈
- [x] 다중 리사이즈 비율 유지
- [x] 선택 표시/핸들 표시 분리
- [x] Alt 기반 그룹 내부 선택 UX
- [x] 드래그/리사이즈 중 리페인트 잔상 이슈 수정

### 남은 항목
- [ ] 없음(유지보수만)

---

## Phase 2: Export MVP
### 구현 완료
- [x] Export 출력 경로 정리(타임스탬프 폴더)
- [x] manifest/report/header/source 출력
- [x] 외부 등록 위젯 codegen 확장 포인트 반영
- [x] ABI 업데이트 반영
- [x] flatten 정책 유지

### 남은 항목
- [ ] 없음(유지보수만)

---

## Phase 3: Layer/Group/Inspector/스냅 기반 편집 UX
### 구현 완료
- [x] Layer/Group/Widget 트리 DnD(reorder/reparent)
- [x] Layer 패널 선택/캔버스 선택 동기화
- [x] Layer 추가/삭제 + ActiveLayer 정책
- [x] Grid View / Grid Snap / Smart Snap 분리
- [x] Guide 생성/삭제(룰러 드래그, 더블클릭 삭제)
- [x] 캔버스 줌/패닝/정렬 정책 반영
- [x] Inspector 기본 편집(공통/Transform/Appearance/Widget)
- [x] WidgetRegistry 기반 위젯 목록 사용

### 남은 항목
- [ ] 없음(유지보수만)

---

## Phase 3.5: 고급 Editor UX 패널/보완
### 구현 완료
- [x] LayerTree Panel
- [x] Widget Library Panel
- [x] Assets Panel
- [x] Inspector Panel
- [x] Event/Action Panel(MVP)
- [x] Validation Panel
- [x] Export Preview Panel
- [x] History Panel
- [x] Performance Panel
- [x] Navigator Panel

### 패널별 남은 보완
- [ ] Event/Action: `paramKey` 자동완성/검색, 키보드 편집 흐름 개선
- [ ] Event/Action: 사용자 친화 라벨/입력 UI 정리
- [ ] Validation: 카테고리 필터, 경고 클릭 포커스 강화, Quick Fix
- [ ] Navigator: 오버레이 배치(캔버스 우상단/우하단) 여부 결정
- [ ] Performance: 표시 주기/지표 UX 튜닝(이벤트+저주기 타이머 하이브리드 검토)

---

## Phase 3.8: Preview/Run 모드 분리(MVP)
### 목표
- Preview 모드: 기존 편집 플로우(위치/크기/속성 수정) 유지
- Run 모드: 실제 사용자 상호작용 중심으로 위젯 이벤트와 runtimeBindings를 시험

### 구현 범위(MVP)
- [x] 툴바에 `Preview` / `Run` 모드 토글 추가
- [x] Run 모드에서 편집 단축키/편집 드래그 입력 차단
- [x] Run 모드 위젯 상호작용 기본 지원
- 버튼: `onPress`, `onRelease`, `onClick`
- 토글: `state` 변경 + `onClick`, `onToggleChanged`
- 슬라이더: `value` 변경 + `onValueChanged`, `onValueCommit`
- 노브: `value` 드래그 변경 + `onValueChanged`, `onValueCommit`
- 콤보박스: 선택 인덱스 순환 + `onSelectionChanged`
- [x] 이벤트 발생 시 `runtimeBindings(sourceWidgetId, eventKey)` 매칭 후 액션 실행
- [~] `setRuntimeParam/adjustRuntimeParam/toggleRuntimeParam`는 런세션 파라미터 저장소에 반영
- [x] `setNodeProps/setNodeBounds`는 Run 세션 preview 상태로 반영

### 남은 보완
- [x] Run 세션 종료 시 상태 롤백 기본정책 적용(Preview 복귀 시 rollback)
- [~] 텍스트 입력/콤보박스/노브 등 고급 런타임 입력 확장(콤보/노브 완료, 텍스트 입력 미완)
- [x] 런타임 이벤트/액션 실행 로그 패널 연동(History 패널)

---

## Phase 4: 런타임 Event/Action 실행 엔진

### 목표
- 에디터에서 저장한 `runtimeBindings`를 런타임에서 실제 실행한다.
- UI 스레드/오디오 스레드 경계를 지키는 실행 정책을 확정한다.

### 구현 범위(MVP)
- [x] 이벤트 입력: `WidgetId + eventKey + payload`를 런타임 이벤트로 정규화
- [x] 바인딩 조회: `runtimeBindings`에서 `enabled/sourceWidgetId/eventKey` 매칭
- [x] 액션 실행: `setRuntimeParam`, `adjustRuntimeParam`, `toggleRuntimeParam`, `setNodeProps`, `setNodeBounds`
- [x] 실행 결과 수집: 성공/실패/스킵 카운트 + 에러 메시지

### 단계별 계획
#### 4-1) Runtime 이벤트 디스패처
- [x] `RuntimeBindingEngine::dispatchEvent(...)` 진입점 구현
- [x] source/event 기준으로 실행 대상 바인딩 목록 계산
- [x] 비활성 바인딩(`enabled=false`) 및 잘못된 입력 필터

#### 4-2) Runtime 액션 실행기
- [x] `RuntimeActionExecutor::execute(...)` 구현
- [x] MVP 5종 액션별 실행 함수 분리
- [x] 액션 실패 시 중단/계속 정책 명시(기본: 실패해도 다음 액션 계속)

#### 4-3) 파라미터 라우팅 브리지
- [x] `set/adjust/toggleRuntimeParam`를 파라미터 저장소 인터페이스로 연결
- [x] 값 타입 변환 규칙(`bool/int/double/string`) 고정
- [x] 파라미터 미존재/타입 불일치 에러 코드화

#### 4-4) 스레드/실행 정책
- [x] UI 스레드 전용 액션(`setNodeProps/setNodeBounds`) 분리
- [x] 오디오 콜백 경로에서 금지 동작(할당/락/컴포넌트 접근) 차단
- [x] 필요 시 lock-free 큐 또는 메시지 스레드 포워딩 훅 준비

#### 4-5) 로깅/성능 가드
- [x] 실행 로그 레벨(`off/error/info/trace`) 지원
- [x] 프레임당 최대 실행량 제한(예: maxActionsPerTick)
- [x] 동일 이벤트 폭주 억제(디바운스/스로틀 최소 정책)

### 완료 기준(Definition of Done)
- [x] 저장된 `runtimeBindings`를 로드 후 이벤트 발생 시 동일하게 실행된다.
- [x] MVP 5종 액션이 문서 상태/파라미터 상태에 반영된다.
- [x] 무효 source/event/target에서 크래시 없이 에러 리포트만 남긴다.
- [x] 실행량 제한이 동작하고, 제한 초과 시 경고 로그가 남는다.
- [x] Debug 빌드에서 누수/assert 없이 기본 시나리오 통과한다.

### 테스트 체크리스트
- [ ] Unit: 바인딩 매칭(정확/부분/미스매치)
- [ ] Unit: 액션별 정상/실패 케이스(타입 불일치, 대상 없음)
- [ ] Integration: 버튼 이벤트 -> 파라미터 변경 -> UI 반영
- [ ] Integration: 이벤트 체인 다중 액션 순서 보장
- [ ] Regression: 빈 바인딩/대량 바인딩(1000+)에서 성능 저하/크래시 없음

### 수정/추가 파일
- 수정: `Source/Gyeol/Public/Types.h`
- 수정: `Source/Gyeol/Serialization/DocumentJson.cpp`
- 수정: `Source/Gyeol/Core/SceneValidator.h`
- 추가: `Source/Gyeol/Runtime/RuntimeBindingEngine.h`
- 추가: `Source/Gyeol/Runtime/RuntimeBindingEngine.cpp`
- 추가: `Source/Gyeol/Runtime/RuntimeActionExecutor.h`
- 추가: `Source/Gyeol/Runtime/RuntimeActionExecutor.cpp`
- 추가: `Source/Gyeol/Runtime/RuntimeParamBridge.h`
- 추가: `Source/Gyeol/Runtime/RuntimeParamBridge.cpp`
- 추가: `Source/Gyeol/Runtime/RuntimeDiagnostics.h`
- 추가: `Source/Gyeol/Runtime/RuntimeDiagnostics.cpp`
- 수정: `Source/Gyeol/Editor/EditorHandle.cpp`
- 수정: `Source/Gyeol/Export/*` (필요 최소 범위)

---

## Phase 5: 데이터 바인딩/상태 시스템

### 목표
- 위젯 속성과 런타임 상태(파라미터/메타 값)를 선언적으로 연결한다.

### 구현 단계(구체)
#### 5-1) 데이터 모델/스키마 확장
- [x] `runtimeParams` 모델 추가(키/타입/기본값/설명)
- [x] `propertyBindings` 모델 추가(대상 위젯/속성/표현식/활성화)
- [x] JSON 직렬화/역직렬화 및 Schema minor 업데이트
- [x] DocumentHandle setter + Undo/Redo 이력 반영

#### 5-2) 바인딩 엔진(MVP)
- [x] 식 문법: 숫자/식별자/`+ - * /`/괄호
- [x] 식별자 해석: 런타임 파라미터 스냅샷 조회
- [x] 대상 속성 반영: `setNodeProps` 경로 재사용
- [x] 동작 범위: 단방향 + Run 모드 우선 적용

#### 5-3) 패널/UI(MVP)
- [x] PropertyPanel에 바인딩 읽기 전용 표시(배지/요약)
- [x] Event/Action 패널에 Param/Binding 편집 섹션 추가
- [x] 바인딩 On/Off 및 에러 상태 표시

#### 5-4) 검증/진단
- [x] 끊긴 참조(대상 위젯 없음) 검사
- [x] 타입/식 오류 검사(파라미터 타입 불일치, 파싱 실패)
- [x] 실행 로그에 바인딩 평가 실패 원인 출력

#### 5-5) 적용 정책
- [x] Preview 기본: 바인딩 비활성(편집 우선)
- [x] Run 기본: 바인딩 활성(실사용 시뮬레이션)
- [x] 옵션: Preview에서 임시 시뮬레이션 토글(후순위)

### 1차 구현 범위(이번 착수)
- [x] 5-1 데이터 모델/직렬화/검증 골격
- [x] 5-2 바인딩 엔진 Run 모드 연결(단방향, 숫자식)
- [x] 5-3 패널 편집 UI

### 완료 기준(DoD)
- [ ] 문서에 파라미터/바인딩을 저장/로드해도 round-trip 일치
- [ ] Run 모드에서 `setRuntimeParam` 계열 액션 후 바인딩 결과가 즉시 반영
- [ ] 잘못된 식/참조에서도 크래시 없이 경고/로그만 출력
- [ ] Preview 복귀 시 기존 Run 롤백 정책 유지

### 수정/추가 파일(예상)
- 수정: `Source/Gyeol/Public/Types.h`
- 수정: `Source/Gyeol/Core/DocumentHandle.cpp`
- 수정: `Source/Gyeol/Public/DocumentHandle.h`
- 수정: `Source/Gyeol/Serialization/DocumentJson.cpp`
- 수정: `Source/Gyeol/Core/SceneValidator.h`
- 추가: `Source/Gyeol/Runtime/PropertyBindingResolver.h`
- 수정: `Source/Gyeol/Editor/EditorHandle.cpp`

---

## Phase 6: Export 런타임 브리지

### 목표
- 에디터 기능이 export 코드에서도 동일하게 동작하도록 런타임 브리지를 만든다.

### 구현 단계(구체)
#### 6-0) 기존 export 기반 정리
- [x] export 출력물에 `Assets/` 폴더 생성 + 자산 파일 복사 파이프라인
- [x] export GUI 코드의 자산 경로를 `Assets/...` 기준으로 고정

#### 6-1) Export Data Contract/Manifest 고정
- [x] manifest 스키마 확정(`schemaVersion`, `assets`, `runtimeParams`, `propertyBindings`, `runtimeBindings`)
- [x] 필수/선택 필드 규약 문서화(누락 시 fallback 포함)
- [x] 정렬/키 안정화 규칙 확정(같은 문서면 동일 출력)

#### 6-2) Export 직렬화 파이프라인
- [x] export 산출물에 runtime state/binding 데이터 파일 생성
- [x] `DocumentModel` -> export runtime 데이터 변환기 구현
- [x] 상대 경로/자산 참조 일관성 검증(`Assets/...`)

#### 6-3) Runtime Bridge 템플릿 생성
- [x] `RuntimeParamStore` 생성 코드(키 조회/타입 검증/기본값 시드)
- [x] `PropertyBindingResolver` 생성 코드(Phase 5 문법과 동등성 유지)
- [x] `PropertyBindingApplier` 생성 코드(타겟 위젯 속성 반영)

#### 6-4) 위젯 이벤트 emit 코드 생성
- [x] 버튼/슬라이더/노브/토글/콤보/텍스트입력 이벤트 emit 코드 생성
- [x] 이벤트 payload 정규화 규칙 적용(`WidgetId + eventKey + payload`)
- [x] unsupported 위젯/이벤트 graceful fallback 로그 처리

#### 6-5) 액션 실행기 연결 코드 생성
- [x] `setRuntimeParam/adjust/toggle` 실행 경로 연결
- [x] `setNodeProps/setNodeBounds` 실행 경로 연결
- [x] 실패/스킵 정책 및 로그 레벨(`error/info`) 반영

#### 6-6) 부트스트랩/통합
- [x] export 컴포넌트 생성 시 runtime bridge 초기화 코드 주입
- [x] 초기 파라미터 시드 -> 1회 바인딩 적용 -> 위젯 상태 반영 순서 고정
- [x] 런타임 이벤트 발생 시 action -> param -> property binding 재평가 루프 연결

#### 6-7) 회귀/검증
- [x] 샘플 프로젝트 export 후 단독 컴파일 통과
- [x] 기본 시나리오 테스트(버튼 클릭 -> 파라미터 변경 -> UI 반영)
- [x] 오류 시나리오 테스트(없는 대상/잘못된 식)에서 크래시 없이 로그만 출력

### 완료 기준(DoD)
- [x] export 결과물에서 runtimeBindings 이벤트-액션 체인이 동작한다.
- [x] export 결과물에서 runtimeParams/propertyBindings가 Phase 5와 동일하게 반영된다.
- [x] assets + runtime manifest 규약이 문서화되고, 생성물과 일치한다.
- [x] 샘플 회귀 시나리오를 자동 또는 반자동으로 재현 가능하다.
- [x] Export 생성물 컴파일/실행 시 치명 오류 없이 로그로 진단 가능하다.

### 수정/추가 파일
- 수정: `Source/Gyeol/Export/ExportDataContract.md`
- 수정: `Source/Gyeol/Export/JuceComponentExport.*`
- 수정: `Source/Gyeol/Serialization/DocumentJson.*` (export용 변환 경로 재사용 시)
- 수정: `Source/Main.cpp` (`--phase6-export-smoke` CLI smoke mode)
- 추가: `Tools/Phase6ExportSmoke.ps1` (반자동 회귀/컴파일 스모크)
- 추가: `Source/Gyeol/Export/RuntimeBridgeTemplates/*`
- 추가: `Source/Gyeol/Export/RuntimeBridgeRuntime/*` (생성 코드 대상 또는 템플릿 소스)

---

## Phase 7: 확장 SDK(외부 위젯/패키지)

### 목표
- 외부 위젯 패키지를 안전하게 등록/조회/내보내기 가능하게 만든다.

### 세부 계획
- [ ] WidgetSDK ABI 버전 정책 확정
- [ ] 외부 위젯 manifest + 자산 규약 확정
- [ ] Plugin/Package 등록 관리자
- [ ] 미설치 위젯 placeholder 정책
- [ ] 충돌/호환성 검증 도구

### 수정/추가 파일
- 수정: `Source/Gyeol/Widgets/WidgetSDK.h`
- 수정: `Source/Gyeol/Widgets/WidgetRegistry.*`
- 추가: `Source/Gyeol/Widgets/WidgetPackageManager.*`
- 추가: `Source/Gyeol/Widgets/WidgetCompatibility.*`
- 수정: `Source/Gyeol/Core/SceneValidator.h`

---

## Phase 8: 안정화/성능/릴리스

### 목표
- 대형 문서에서도 안정적인 편집 성능과 예측 가능한 결과를 보장한다.

### 세부 계획
- [ ] Canvas 부분 리페인트 최적화 고도화
- [ ] LayerTree 부분 갱신/가상화
- [ ] Undo/Redo 1,000회 스트레스 테스트
- [ ] 메모리 누수/객체 수명 점검(`TreeViewItem` 포함)
- [ ] 성능 패널(프레임타임/리페인트 카운트) 제공
- [ ] 릴리스 체크리스트 자동화

### 수정/추가 파일
- 수정: `Source/Gyeol/Editor/EditorHandle.cpp`
- 수정: `Source/Gyeol/Editor/Panels/LayerTreePanel.*`
- 수정: `Source/Gyeol/Editor/Interaction/*`
- 추가: `Source/Gyeol/Editor/Panels/PerformancePanel.*`
- 추가: `Tools/GyeolPerfSmoke.cpp`

---

## 최종 품질 게이트
- [ ] LLVM Debug x64 빌드 통과
- [ ] include 회귀 0건
- [ ] JSON round-trip 회귀 0건
- [ ] Undo/Redo 불일치 0건
- [ ] Export 생성물 컴파일 통과
- [ ] CRLF 라인 엔딩 유지
