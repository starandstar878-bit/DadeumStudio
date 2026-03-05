# Gyeol 로드맵
업데이트: 2026-03-05

## Phase 개요
- [x] Phase 0: 코어 모델/액션 안정화
- [x] Phase 1: 상호작용 엔진/선택/리사이즈
- [x] Phase 2: Export MVP
- [x] Phase 3: Layer/Group/Inspector/스냅 기반 편집 UX
- [x] Phase 3.5: 고급 Editor UX 패널/보완
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

#### Event/Action 패널
> 현재 구조: 바인딩 리스트 → 액션 리스트 → 상세 에디터 (PanelMode: eventAction / stateBinding)

**🧭 탐색 및 발견성 (Discoverability)**
- [x] **바인딩 카드에 요약 정보 추가**: 현재 리스트 행에는 이름만 표시됨. 이벤트 소스 위젯명 + 이벤트 키 + 액션 수를 한 행에 2줄 형태(카드形)로 노출해 클릭 없이도 내용 파악 가능하게 개선
- [x] **액션 리스트 행 요약화**: 액션 종류(아이콘) + 대상 위젯명 + 핵심 파라미터 값을 한 줄에 표시. 현재 `paintListBoxItem`에서 텍스트만 출력하는 구조 개선
- [x] **검색 필터 기능 강화**: `searchEditor`는 있으나 필터링 대상이 명확하지 않음. 바인딩 이름/소스 위젯명/이벤트 키 전체를 대상으로 검색되도록 `rebuildVisibleBindings` 로직 확장
- [x] **이벤트 종류별 색상 태그**: `onClick` / `onValueChanged` / `onToggleChanged` 등 이벤트 종류에 따라 바인딩 카드 좌측 컬러 바를 구분해 한눈에 식별 가능하게 처리

**⌨️ 키보드 편집 흐름**
- [x] **Tab 순서 통일**: 현재 `sourceCombo → eventCombo → add버튼` 간 Tab 포커스가 자연스럽지 않음. `sourceCombo → eventCombo → bindingNameEditor → enabledToggle` 순서로 Tab 흐름 정규화
- [x] **Enter로 커밋, Escape로 취소**: `valueEditor`, `deltaEditor`, `boundsXYWH` 등 텍스트 에디터 전체에 Enter = 적용, Escape = 원복 동작 일관 적용 (현재 포커스 해제 시에만 적용되는 경우 있음)
- [x] **액션 리스트 방향키 네비게이션**: Up/Down 키로 액션 선택, Ctrl+Up/Down으로 순서 변경 지원 (현재 버튼 클릭만 가능)
- [x] **Del / Backspace로 선택 항목 삭제**: 바인딩/액션 리스트에서 행 선택 후 Delete 키 지원

**🔁 데이터 편집 효율**
- [x] **paramKey 콤보 실시간 필터**: `paramKeyCombo`에서 키를 입력하면 일치하는 파라미터 이름을 필터링해 보여주는 검색형 콤보박스로 전환 (위젯이 많은 문서에서 스크롤 없이 찾기 위함)
- [x] **바인딩 활성화 토글(Enabled)을 카드에 직접 노출**: 현재 상세 편집 후에야 boolen 상태 확인 가능. 바인딩 리스트 행 우측에 미니 토글로 즉시 on/off 제어
- [x] **액션 복제(Duplicate) 단축키**: `duplicateBindingButton`은 있으나 단축키 없음. Ctrl+D로 선택된 바인딩 복제 지원
- [x] **바인딩/액션 일괄 복사 (Binding → 다른 위젯)**: 선택한 바인딩을 다른 소스 위젯으로 복사하는 "Clone to…" 기능 추가

**⚡ 자원/성능 측면**
- [x] **`refreshFromDocument` 호출 최소화**: 현재 문서 갱신 시마다 전체 리빌드 (`rebuildWidgetOptions`, `rebuildCreateCombos` 등). 선택 변경 없이도 무조건 호출되는 케이스를 dirty flag로 막아 불필요한 콤보 재구성 방지
- [x] **`rebuildVisibleBindings` 지연 실행**: 검색 텍스트 입력 시 매 키 입력마다 호출. 타이핑 50ms 디바운스 후 실행으로 변경
- [x] **ListBox 가상화**: `BindingListModel`, `ActionListModel` 등 행 수가 많아지면 모든 행을 paint. 100건 이상 바인딩에서도 빠르게 동작하도록 `ListBox::setRowHeight` + 가상화 확인 및 최적화
- [x] **`dynamicPropEditor` 재생성 억제**: 액션 선택 시 `rebuildDynamicPropEditor()`가 매번 `std::unique_ptr` 재할당. 동일한 `WidgetPropertySpec`이면 에디터를 재사용하도록 캐시 조건 추가

**✅ 검증 및 피드백**
- [x] **인라인 유효성 오류 표시**: 현재 하단 `statusLabel`에만 오류를 출력. 오류가 있는 필드 옆에 작은 경고 아이콘(⚠)을 표시해 어느 필드가 문제인지 즉시 파악 가능하게 개선
- [x] **`paramKey` 미존재 경고**: `setRuntimeParam` 액션에서 선택한 `paramKey`가 State 탭에 정의된 파라미터 목록에 없을 경우, 해당 콤보에 노란색 테두리 + 툴팁 경고 표시
- [x] **속성 바인딩 식(expression) 실시간 프리뷰**: `propertyBindingExpressionEditor`에 식 입력 시 현재 파라미터 기본값 기준으로 계산 결과를 에디터 아래 작은 라벨로 즉시 표시 (예: `param_volume * 100` → "= 75.0")
- [x] **무효 바인딩 시각적 구분**: 참조 위젯이 삭제되거나 이벤트 키가 없어진 경우 해당 바인딩 카드를 흐리게 처리 + 우측에 삭제 유도 버튼 노출

#### Validation 패널
> 구현 목표: 블로킹 없는 에러 검출과 직관적인 해결 UI 제공

**단계 1: 비동기 스캐닝 기반(Core Async Foundation)**
- [x] `SceneValidator`에 백그라운드 스레드 평가 큐 연동. 편집 이벤트 발생 시 500ms Debounce 후 `startValidationThread()` 호출
- [x] 검사 완료 시 `MessageManager::callAsync`를 통해 패널에 결과(에러 트리) 전달 및 캐싱
- [x] 상단 탭/칩스 UI 구현: `[Errors: 2] [Warnings: 5] [Infos: 1]` 형식 컴포넌트 추가 및 칩스 클릭 시 ListBox 필터링 연동

**단계 2: 시각적 피드백 및 딥 인스펙션(UI & UX)**
- [x] ListBox 아이템 렌더러 교체: `GyeolCustomLookAndFeel::drawValidationItem` 구현 (에러 유형별 아이콘 및 펄스(Pulse) 애니메이션 타이머 부착)
- [x] 호버 및 크로스 하이라이팅: 항목 호버 시 `CanvasComponent`로 메시지를 쏴 해당 타겟 위젯에 인라인 점선 하이라이트 레이어 오버레이
- [x] 의존성 트리(Dependency Map) 렌더링: 리스트 아이템 확장(Expand) 시 하단에 연관된 바인딩 DAG 다이어그램 그리기 로직 추가

**단계 3: 이슈 해결 엔진(Actionable Resolutions)**
- [x] 더블클릭 이벤트 처리: 항목 더블클릭 시 캔버스 `SelectionEngine` 포커스 및 PropertyPanel 전환
- [x] Quick Fix 모달: "누락된 폰트/이미지" 검출 시 항목 옆 `[Fix]` 버튼 노출. 클릭 시 팝업을 띄워 로컬 에셋 브라우저 연결 또는 캐시 폰트로 대체(Command 전송)
- [x] 무시(Mute) 기능: 특정 항목에 대한 UUID를 저장해 다음 스캔부터 해당 경고 무시되록 필터 목록에 추가

#### Navigator 패널
> 구현 목표: 거대 씬 렌더링 최적화와 화면 조종 쾌감 상승

**단계 1: 모던 플로팅 쉘 구축(Modern Shell)**
- [x] 패널 자체를 사각형 컴포넌트에서 벗어나 모서리가 둥근 `DropShadow` 플로팅 윈도우(아크릴 룩)로 컨테이너화
- [x] 호버 반응형 투명도 상태머신: 마우스 진입 시 투명도 30% -> 100% `Fade-in` 애니메이터 연동
- [x] 플로팅 드래그 핸들: 패널 최상단이나 코너 드래그 시 캔버스 내 절대좌표 `setBounds()` 갱신으로 사용자 패널 커스텀 위치 이동 지원

**단계 2: 캐시 기반 시맨틱 렌더링(Semantic Render Cache)**
- [x] `CanvasComponent` 변경점 구독: 렌더 완료 후 `DirtyRect` 발생 시 Navigator에만 비동기 통보
- [x] 문서 복제 렌더러 분리: `paint()` 시 캔버스 트리를 순회하되, 배율이 낮을 경우(예: 스케일 0.1 이하) Text와 Shadow 렌더를 `ScopedSaveState`로 스킵하고 단색 `fillRect`로 치환(LOD)
- [x] `Image` 캐시 버퍼(`setBufferedToImage(true)`) 구성하여 프레임당 재렌더 방지

**단계 3: 고급 뷰포트 인터랙션(Advanced Viewport)**
- [x] Viewport 사각형(아웃라인) 부드러운 네온 보더 그리기 (`GyeolPalette` 엑센트 색상 적용)
- [x] 뷰포트 크기 조절: 미니맵 안에서 사각형 테두리를 드래그하면 실제 메인 `Canvas`의 줌 배율(`setZoom()`) 양방향 동기화
- [x] 오프스크린 지시기: 선택된 타겟 위젯이 현재 범위 밖일 경우, 네비게이터 외곽선 쪽에 방향 화살표(Angle 계산) 및 거리 `drawText` 표기

#### Performance 패널
> 구현 목표: 옵트인 방식의 안전한 진단 툴링

**단계 1: 진단 데이터 모델 및 수집기(Metrics Pipeline)**
- [x] `PerformanceMetricsStore` 싱글톤 또는 세션 객체 생성. 프레임당 소요 시간(Paint / Action / Layout) Rolling Buffer 기록 배열(길이 60) 마련
- [x] Run 모드 전환 시점 훅: `EditorModeCoordinator::setMode(Run)` 진입 시 3초간 `PerformanceMetricsStore` 강제 수집 후, 임계치 위반 시 알림(Notification) 컴포넌트 팝업
- [x] 진단 패널 UI 골격: 상단 "진단 시작/중지" 토글 버튼 및 3가지 탭(프레임 주기, 과결합 히트맵, 메모리) 배치

**단계 2: UI/UX 고도화(Modern Dashboard)**
- [x] 스파크라인(Sparkline) 컴포넌트: `Path` 객체를 활용해 Rolling Buffer 값을 부드러운 곡선 차트로 렌더링. Y축 높이에 따라 초록-노랑-빨강 색상 보간(`Colour::interpolatedWith`) 적용
- [x] 네오모피즘(Neumorphism) 버튼 렌더 함수 추가 (`GyeolCustomLookAndFeel::drawPerformanceDashboardButton`)
- [x] 파이차트 컴포넌트: `std::map<String, int>` 형식으로 폰트 크기, 텍스처 메모리 크기 합산 후 호(Arc)로 렌더링 (Export 탭 확인 시 호출)

**단계 3: 오버드로 렌더 모드(Overdraw Heatmap)**
- [x] "히트맵 보기" On 시 상태 트리거: `CanvasRenderer`에 `setHeatmapMode(true)` 파라미터 전달
- [x] 히트맵 렌더러 처리: 활성화된 경우 컴포넌트 고유 색상을 무시하고, 모든 위젯에 투명도 30%의 단일 붉은색을 덮어 칠함(교집합 영역이 진해지도록 연출).
- [x] 결과 레포트: 히트맵 교차 비용이 큰 Top 5 위젯 리스트업 및 `[해결 제안: 렌더 캐시 켜기]` 버튼 노출

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

## Phase 7: 확장 SDK (외부 위젯/패키지 연동 체계)

### 목표
- 제공된 런타임 위젯 외에도, 사용자가 직접 만든 커스텀 위젯(C++ 클래스)이나 제3자가 배포하는 동적 라이브러리(DLL/SO/DYLIB)를 에디터 내에서 안전하게 로드하고, 네이티브 위젯과 동일하게 조작 및 Export 할 수 있도록 아키텍처를 확장한다.

### 구현 단계 (구체화된 Step-by-Step)

#### 단계 0: 위젯 속성 스키마 확장 (Schema Expansion)
- [ ] **속성 확장 스펙 고정**: 외부 위젯 메타데이터/프로퍼티/이벤트 payload 스키마를 `WidgetSDK.h`와 `GyeolWidgetPluginABI.h`에 동기화한다.
- [ ] **레지스트리 검증 규칙 확장**: 신규 속성의 필수/선택/자동 필드를 구분해 `WidgetRegistry::registerWidget` 검증 규칙에 반영한다.
- [ ] **매니페스트/Export 연계**: 새 속성이 `WidgetLibraryManifest`, `ExportManifest`, 코드 생성 경로에 누락 없이 전달되는지 검증한다.

##### 확장된 위젯 속성 리스트 (v1)
- **플러그인 식별/호환성**: `pluginId`, `pluginVersion`, `vendor`, `sdkMinVersion`, `sdkMaxVersion`
- **위젯 능력/성능 힌트**: `capabilities`, `repaintPolicy`, `tickRateHintHz`, `supportsOffscreenCache`
- **프로퍼티 표현 메타**: `unit`, `displayFormat`, `valueCurve`, `required`, `minLength`, `maxLength`, `regex`
- **에셋 제약 메타**: `acceptedMimeTypes`, `maxAssetBytes`, `fallbackAssetId`
- **런타임 이벤트 메타**: `payloadSchema` (키/타입/필수 여부)
- **접근성/테스트 메타**: `a11yRole`, `a11yLabelKey`, `testId`
- **Export 요구사항 메타**: `requiredJuceModules`, `requiredHeaders`, `requiredLibraries`, `codegenApiVersion`

##### 분류: 사용자가 최소한으로 정해야 하는 속성 (필수)
- `typeKey`, `displayName`, `category`
- `defaultBounds`, `minSize`, `defaultProperties`
- `propertySpecs`의 `key`, `label`, `kind`, `defaultValue`
- `runtimeEvents`의 `key`, `displayLabel`
- `pluginId`, `pluginVersion`, `sdkMinVersion`
- `painter`(또는 외부 위젯의 동등 렌더 진입점)

##### 분류: 사용자가 사용할 만한 속성 (선택)
- `tags`, `iconKey`, `exportTargetType`
- `unit`, `displayFormat`, `valueCurve`
- `hint`, `advanced`, `readOnly`, `dependsOnKey`, `dependsOnValue`
- `acceptedAssetKinds`, `acceptedMimeTypes`, `maxAssetBytes`, `fallbackAssetId`
- `capabilities`, `repaintPolicy`, `tickRateHintHz`
- `payloadSchema`, `a11yRole`, `a11yLabelKey`, `testId`
- `requiredJuceModules`, `requiredHeaders`, `requiredLibraries`

##### 분류: 자동화할 속성 (호스트/빌드 파이프라인 산출)
- `sdkMaxVersion` 기본값 보정 (`Host SDK Major`)
- `vendor` 기본값 채우기 (플러그인 매니페스트/서명 정보 기반)
- `testId` 자동 생성 (`pluginId.typeKey.propertyKey`)
- `requiredJuceModules`/`requiredHeaders` 추론 (codegen, include scan)
- `supportedMimeTypes` 파생값 생성 (`acceptedAssetKinds` 기반)
- `codegenApiVersion` 자동 주입 (Export 시점)
#### 단계 1: SDK ABI 및 플러그인 인터페이스 확립 (Foundation)
- [ ] **SDK 헤더 분리**: `WidgetSDK.h`를 외부 개발자가 단독으로 Include할 수 있는 순수 가상 클래스(Interface) 레벨로 고도화 (예: `IGyeolCustomWidget`, `IPropertyProvider`)
- [ ] **ABI 안정성 보장**: C++ Name Mangling과 메모리 할당자 이슈를 피하기 위해, 플러그인 진입점은 순수 C ABI(`extern "C"`) 함수인 `GyeolCreateWidgetInstance()`, `GyeolGetPluginManifest()` 형태로 규격화
- [ ] **Manifest 구조체 정의**: 외부 DLL 내부에서 반환할 `PluginManifest` 정보(위젯명, 버전, 작성자, 사용하는 프로퍼티 스펙 리스트 등)를 JSON 기반 데이터 모델로 확정

#### 단계 2: 에디터 내 동적 라이브러리 로더 구현 (Loader & Registry)
- [ ] **DLL/플러그인 로드 매니저 구현**: `juce::DynamicLibrary`를 래핑한 `WidgetPackageManager`를 구현하여, 특정 디렉터리(`DadeumStudio/Plugins`) 내의 외부 파일을 스캔 및 런타임 로드
- [ ] **레지스트리 런타임 바인딩**: 로드 성공한 커스텀 위젯을 기존 `WidgetRegistry`의 `factoryMap` 및 목록에 동적으로 주입. "Widget Library Panel"에 [External] 카테고리로 자동 분리 표시
- [ ] **버전 호환성 및 충돌 검사**: 새로 로드되는 DLL의 SDK 버전이 에디터의 버전과 다를 경우 로드를 차단하거나, 기존 동일 UUID를 가진 위젯과 충돌할 때 경고를 발생시키는 로직 포함

#### 단계 3: 안전한 렌더링 및 플레이스홀더 샌드박싱 (Error Handling)
- [ ] **예외 포착 (Exception Boundary)**: 외부 DLL 위젯의 `paint()`나 속성 변경 함수 호출 중 발생할 수 있는 크래시(Access Violation 등)를 최소화시키기 위한 SEH/Try-Catch 보호 래퍼(Wrapper) 적용
- [ ] **플레이스홀더 전환 (Graceful Fallback)**: DLL 파일을 찾을 수 없거나 로드에 실패한(버전 불일치, 오류 발생 등) 커스텀 위젯은 캔버스에서 투명해지는 대신, 붉은 테두리의 "Missing Plugin: [위젯명]" 형태의 **Placeholder 위젯으로 즉시 변환**
- [ ] **Validation 패널 연동**: 로드 실패나 속성 불일치가 발생할 경우 Phase 3.5에서 만든 `Validation Panel`에 딥 링크가 연결된 치명 에러(Error)로 즉각 리포팅

#### 단계 4: 완전한 Export 파이프라인 통합 (Export Bridge)
- [ ] **Export Manifest 확장**: Document를 Export 할 때, 현재 씬에서 사용된 외부 플러그인의 리스트(버전 및 소스 DLL 포함)를 `ExportManifest.json`의 `dependencies` 항목에 자동 기록
- [ ] **에셋 파일 패키징**: 구워진 Export 폴더 내부의 `Plugins/` 폴더로 사용된 원본 DLL/정적 라이브러리 파일들을 함께 자동 복사(Packaging)
- [ ] **JuceProject (Jucer/CMake) 자동 갱신**: 외부 C++ 코드를 사용하는 패키지인 경우, 런타임 결과물(Jucer 프로젝트)에서 외부 소스나 Include 경로를 자동으로 주입해 주는 CMake/Jucer 스니펫(Snippet) 생성 로직 추가

### 수정/추가 레퍼런스
- 수정 대상: `Source/Gyeol/Widgets/WidgetSDK.h`, `Source/Gyeol/Widgets/GyeolWidgetPluginABI.h`
- 내부 개편: `Source/Gyeol/Widgets/WidgetRegistry.*` (동적 주입 허용)
- 신규 작성: `Source/Gyeol/Widgets/WidgetPackageManager.h / .cpp` (DynamicLibrary 로딩 관리)
- 신규 작성: `Source/Gyeol/Widgets/PlaceholderWidget.h / .cpp` (Fallback 처리반)
- 융합 로직: `Source/Gyeol/Core/SceneValidator.h` (DLL 누락/에러 리포팅 검수)
- 융합 로직: `Source/Gyeol/Export/JuceComponentExport.*` (DLL 파일 에셋 복사 및 CMake/Jucer 링커 주입)

---

## Phase 8: 재사용 가능한 프리팹/컴포넌트 시스템 (Component & Prefab System)

### 목표
- 복잡한 UI 화면에서 반복되는 위젯 그룹(예: 리스트 아이템, 헤더, 공통 버튼 형태)을 재사용 가능한 `Prefab` 템플릿으로 캡슐화하고, 인스턴스(Instance)별 오버라이드를 지원하여 대규모 프로젝트 관리를 용이하게 한다.

### 구현 단계 (Step-by-Step)

#### 단계 1: 프리팹 데이터 모델 및 직렬화 정의 (Foundation)
- [ ] **Document 모델 확장**: `DocumentModel` 내에 `prefabs` 컬렉션(사전) 추가. 각 프리팹은 마스터 노드 트리와 기본 파라미터를 소유한 가상 레이어로 기능
- [ ] **인스턴스 모델 설계 (`PrefabWidget`)**: 특정 프리팹 ID를 참조하는 전용 위젯 생태계 정의. 원본 트리 구조는 복제하되, 오버라이드 델타(Delta) 맵만 별도 저장하는 경량 객체로 설계
- [ ] **버전 해시 및 동기화 메커니즘**: 마스터 프리팹의 구조가 변경되었을 때 캔버스에 배치된 모든 인스턴스에 강제(또는 지연) 전파되는 로직 구현

#### 단계 2: 에디터 툴링 및 분리된 작업 흐름 (Editor UX)
- [ ] **Make Component 매니저**: 다수의 선택된 위젯을 우클릭하여 "프리팹 만들기(Make Component)" 시, 원본 트리 노드 삭제 후 즉시 해당 위치에 `PrefabWidget` 인스턴스로 감싸 교체
- [ ] **마스터 편집 격리 뷰 (Isolation Mode)**: 마스터 프리팹 템플릿을 더블클릭할 때, 기존 캔버스를 블러 처리하고(또는 새 창 열기) 마스터 트리에 대한 독립적인 편집 환경 진입 (Figma 스타일 격리 편집)
- [ ] **Assets 패널(Components 탭) 연동**: 생성된 프리팹 목록을 Assets(또는 별도의 Components) 패널에 등록하고 캔버스로 언제든지 드래그 앤 드롭해서 인스턴스 소환

#### 단계 3: 컴포넌트 간 오버라이드 시스템 (Property Overrides)
- [ ] **속성 차분(Delta) 검출기**: 인스턴스의 특정 노드나 파라미터를 수정하려 할 때, 마스터 값과 다른 차분(Override Map)만 인스턴스 내부에 기록하는 Getter/Setter 래퍼 구현
- [ ] **UI 시각적 피드백**: `PropertyPanel`에서 마스터 프리팹 값이 아닌, 오버라이드(특성화)된 속성은 굵은 글씨(Bold)나 푸른색 하이라이트/Dot으로 시각적 구별 제시
- [ ] **Reset Override 리셋 버튼**: 오버라이드된 개별 속성이나 위젯을 다시 마스터의 원본 상태로 복귀(Revert)시키는 퀵 액션 연동

#### 단계 4: 런타임 바인딩 연결 관리 및 코드 Export (Export Pipeline)
- [ ] **스코프 제어 (Event Isolation)**: 프리팹 내부에서 정의된 버튼 클릭 이벤트나 파라미터가, 부모(메인) 캔버스의 전역 범위 이름과 충돌하지 않도록 ID 네임스페이스 격리 기술(Sub-scope) 적용
- [ ] **Export 시 C++ 클래스 분리**: Export 시, 반복 사용되는 프리팹 마스터는 독자적인 C++ `.h/.cpp` 클래스 파일로 추출(Sub-Component)하여 생성.
- [ ] **자식 인스턴스 멤버 변수화**: 메인 캔버스 뷰 생성 시, 해당 프리팹 C++ 클래스를 멤버 변수나 `std::vector<std::unique_ptr>` 배열로 동적 로드하도록 코드 제너레이터(JuceComponentExport) 분기 확장

---

## Phase 9: 다중 페이지 시스템 및 라우팅 (Multi-Page & Routing System)

### 목표
- 단일 캔버스(기존 Root) 한계에서 벗어나, 앱 전체의 여러 화면(Page)을 하나의 문서(Document) 안에서 설계하고 이동할 수 있는 멀티-스크린 아키텍처를 도입한다.
- 최상위 컨테이너 명칭을 `App`(또는 `Application`)으로 격상하고, 기존의 단일 Root 트리를 개별 `Page` 단위로 전환한다.

### 구현 단계 (Step-by-Step)

#### 단계 1: App-Page 데이터 모델 재편성 (Foundation)
- [ ] **최상위 App 모델 도입**: `DocumentModel`의 메인 진입점을 `AppNode`(또는 `ApplicationModel`)로 명칭 변경 및 승격 (기존 `RootNode`는 단일 페이지를 의미)
- [ ] **Page 리스트 컬렉션**: App 내부 상태에 `std::vector<PageNode>`(또는 Dictionary)를 두어 여러 페이지 구조체를 소유하도록 JSON 스키마 및 직렬화/역직렬화 업데이트
- [ ] **전역(Global) vs 지역(Local) 상태 분리**: Phase 5에서 만든 파라미터들(`runtimeParams`)을 앱 전반에서 공유할 수 있는 **전역 파라미터**와 탭/페이지 종속적인 **페이지 파라미터**로 스코프 분리

#### 단계 2: 에디터 캔버스 및 페이지 네비게이션 (Editor UX)
- [ ] **페이지 브라우저 패널/탭**: 캔버스 상단 혹은 `Navigator` 패널 측면에 현재 활성화된 페이지를 전환할 수 있는 [Home] [Settings] [Login] 형태의 페이지 탭 브라우저 바 추가
- [ ] **페이지 생성/삭제 관리**: 새 페이지 추가 템플릿 제공 및 캔버스 클리어(전환), Undo/Redo 이력의 페이징 뎁스(Context) 연동 처리
- [ ] **Figma형 인피니트 캔버스 (선택적)**: 활성 페이지만 1:1로 보는 것이 아닌, 줌-아웃 시 모든 페이지 보드가 캔버스 평면 위에 펼쳐져 시각적으로 연결되도록 뷰포트 업그레이드 검토

#### 단계 3: 런타임 라우팅 엔진 및 패널 연동 (Runtime Navigation & Event Panel)
- [ ] **네비게이션 액션(Navigation Actions) 추가**: 버튼 클릭 등 이벤트 발생 시 실행될 `RuntimeAction` 종류에 라우팅 전용 액션(`changePage`, `pushPage`, `popPage`) 추가
- [ ] **Event/Action 패널 UI 연동**: 위젯 속성 패널에서 액션 종류로 "Navigate To Page"를 선택하면 대상 페이지를 고를 수 있는 콤보박스(자동 완성 기능 포함) 노출 지원
- [ ] **트랜지션 애니메이션 속성**: 페이지 이동 동작 시 페이드(Fade), 슬라이드(Slide X/Y) 같은 기본 화면 전환 효과(Transition) 파라미터 지원 및 해석기 탑재
- [ ] **App 진입점 설정**: 에디터 및 런타임 시 앱을 시작했을 때 가장 먼저 보여줄 `Initial Page(Index)` 메타데이터 전역 속성으로 지정

#### 단계 4: 복수 페이지 Export 파이프라인 (Export Pipeline)
- [ ] **페이지별 C++ Component 분할**: 기존에 `DadeumStudio_RootComponent` 하나로 구워내던 방식을 `DadeumStudio_MainApp` 1개 + 종속되는 `LoginPage_Component`, `SettingsPage_Component` 다수로 쪼개어 파일(.cpp)별 코드로 분리 생성
- [ ] **메인 App 라우터 컨트롤러 주입**: 분할된 C++ 클래스들을 교체해 주는 `juce::ComponentAnimator` 또는 `juce::View` 기반 컨테이너 스태킹(Stacking) 매니저를 런타임 브리지 코드로 자동 삽입
- [ ] **전역 바인딩 연결**: 여러 페이지의 컴포넌트가 하나의 전역 `RuntimeParamStore` 변수 인스턴스를 공유(참조)하도록 의존성 주입(DI) 코드 생성 분기

---

## Phase 10: 고급 툴링 시스템 확장 (Advanced Tooling Framework)

### 목표
- 단순했던 기본 선택(Selection) 및 박스 생성 툴을 넘어서, UI 제작에 필수적인 다목적 제작 툴(Text, Shape, Pen, Component Tool 등)을 캔버스 툴바에 다수 이식하고 그 행동 상태(State Machine)를 정교하게 분리한다.

### 구현 단계 (Step-by-Step)

#### 단계 1: 툴 상태머신(Tool State Machine) 아키텍처 (Foundation)
- [ ] **에디터 모드 관리자 분리**: 기존 `EditorInteractionEngine`의 하드코딩된 마우스 이벤트를 `IEditorTool` 가상 클래스를 기반으로 한 **상태머신(State Pattern)** 구조로 분리 (예: `SelectionTool`, `TextTool`, `ShapeTool`)
- [ ] **툴 전환 파이프라인**: 캔버스 좌측 상단 또는 사이드의 메인 툴바 컴포넌트(`ToolbarPanel`)를 생성하고, 버튼 선택/단축키(T, V, P 등) 입력 시 `CanvasComponent`의 현재 활성화 툴이 안전하게 교체되는(Switched) 브리지 로직 연동
- [ ] **초기화 및 클린업**: 새로운 툴로 전환될 때 기존 마우스 캡처나 진행 중인 생성 트랜잭션(드래그 중인 미완성 렌더 등)을 깔끔하게 취소(Abort)하고 기본 상태로 되돌리는 롤백 구현

#### 단계 2: 신규 툴셋 구현(Core Toolset)
- [ ] **텍스트 툴 (T 단축키)**: 선택 시 캔버스를 클릭/드래그하여 `TextWidget`의 Bounds를 생성함과 동시에, 그 자리에서 즉시 커서를 깜박이며 글씨를 입력할 수 있는 인라인 텍스트 에디터 모드 진입
- [ ] **도형 툴 (R, O 단축키)**: `Rectangle`, `Ellipse` 툴. 드래그 중 `Shift` 홀드 시 1:1 비율(정사각형/원) 고정 렌더 트리거 추가
- [ ] **벡터 펜 툴 (P 단축키)**: `juce::Path` 기반의 베지어 곡선(Bézier Curve)을 캔버스에 직접 찍어 다각형/자유형 곡선 형태의 커스텀 위젯을 실시간 생성하는 시스템 탑재
- [ ] **FX 스타일 브러시 툴 (B 단축키)**: 캔버스 위에 배치된 위젯을 이 툴로 '문지르면(Drag or Click)', 브러시가 머금고 있는 특수 질감(Metallic 그라데이션, Drop Shadow, Neon Glow)을 위젯 속성에 실시간으로 버퍼링해 덧칠(Layering) 해주는 다이렉트 스타일링 모드 적용
- [ ] **프리팹/인스턴스 툴 (C 단축키)**: 클릭하여 툴이 활성화된 상태에서 캔버스를 `클릭`하면, 이전에 Assets 패널 등에서 선택해놓은 마스터 컴포넌트(`PrefabWidget`)가 1:1 사이즈로 스냅되어 드롭됨

#### 단계 3: 맥락적(Contextual) 툴 커서 및 피드백 (UX)
- [ ] **마우스 커서 동기화**: 활성화된 툴의 종류에 따라 마우스 커서 아이콘을 동적으로 다르게 지정 (정밀 십자 커서(Crosshair), 텍스트 커서(I-Beam), 셀렉트 화살표 등)
- [ ] **툴 힌트 HUD (Heads-up Display)**: 새 툴이 선택되면 캔버스 중앙 하단에 작게 "Shift를 눌러 정비율 생성" 같은 일시적/조건적 단축키 안내 툴팁 오버레이 제공
- [ ] **포커스 복귀 자동화**: 신규 위젯을 `TextTool` 등으로 생성 완료 및 편집 종료 시점(엔터 등)을 감지하면 자동으로 다시 사용자에게 가장 친숙한 `SelectionTool(V)`로 모드를 복구시켜 작업 흐름 방해 최소화

---

## Phase 11: 태블릿/아이패드 네이티브 연동 시스템 (Remote Companion App)

### 목표
- 벡터 펜 툴 및 FX 브러시 작업 등 미세한 압력(Pressure)과 드로잉 제어가 필요한 작업을 위해, iPad/Stylus 전용 네이티브 앱(Companion App)과 데스크톱 에디터를 양방향으로 실시간 연동한다.

### 구현 단계 (Step-by-Step)

#### 단계 1: 실시간 동기화 네트워크 프로토콜 (Sync Protocol)
- [ ] **소켓/OSC 통신 레이어**: 데스크톱의 `juce::InterprocessConnection` 또는 UDP/OSC (`juce::OSCSender/OSCReceiver`) 기반의 로컬 네트워크 브리지 구축. 
- [ ] **양방향 상태 동기화**: 데스크톱의 캔버스 뷰포트 상태(Zoom/Pan)를 아이패드로 1:1 전송하고, 반대로 아이패드의 펜슬 터치/압력 데이터를 실시간 스트리밍으로 데스크톱 에디터 큐에 Push

#### 단계 2: Apple Pencil 압력 및 기울기 맵핑 (Stylus Dynamics)
- [ ] **데이터 수신 및 보간 (Interpolation)**: 아이패드에서 전송되는 터치 좌표, Pencil 압력(Pressure), 기울기(Tilt) 데이터를 수신하여 `juce::Path` 두께 조절이나 FX 브러시의 Alpha(불투명도) 값으로 변환(Mapping)하는 동적 해석기 추가
- [ ] **지연 보상 렌더링**: 네트워크 핑(Ping)으로 인한 드로잉 지연을 덮기 위해, 데스크톱 화면에서 수신된 좌표 벡터 사이를 부드러운 스플라인(Spline) 곡선으로 잇는 브러시 스무딩(Smoothing) 구현

#### 단계 3: 원격 캔버스 격리 모드 (Remote Canvas Mode)
- [ ] **듀얼 스크린 UI 전환**: 아이패드가 연결되면 데스크톱 에디터 패널에 `[Tablet Connected]` 인디케이터 활성화. 캔버스는 순수하게 디스플레이 출력(Viewer) 역할을 담당하고, 조작(Draw) 권한과 브러시 툴바 UI는 아이패드로 일시 위임
- [ ] **전용 에셋 전송**: 아이패드 컴패니언 앱 내에서 툴(펜, 브러시, 지우개)을 전환했을 때, 해당 ToolState와 컬러픽커 등 핵심 디자인 도구 패널을 아이패드 네이티브 UI로 렌더링(또는 WebRTC 미러링)

---

## Phase 12: 모션 및 키프레임 애니메이션 (Motion & Timelines)

### 목표
- 단순한 상태 전환(Hover/Click)이나 페이지 슬라이딩을 넘어서, Lottie나 After Effects처럼 **시간 축(Timeline)에 기반한 미세한 트위닝(Tweening)과 키프레임 애니메이션**을 UI 컴포넌트에 부여해 생동감 있는 인터랙션을 구축한다.

### 구현 단계 (Step-by-Step)

#### 단계 1: 키프레임 데이터 모델 (Foundation)
- [ ] **Animation Track 모델링**: 위젯의 특정 속성(예: `x`, `y`, `opacity`, `rotation`) 변화를 시간에 따라 추적할 수 있는 `AnimationTrack` 데이터 구조체 추가
- [ ] **보간기(Interpolator) 엔진**: 선형(Linear) 애니메이션의 한계를 극복하기 위해 곡선형 이징 함수(Easing Functions - `ease-in`, `ease-out`, `spring`, `bounce`)를 JUCE 애니메이터와 연동해 수학적으로 계산하는 엔진 탑재
- [ ] **이벤트 트리거 맵핑**: 캔버스나 버튼 이벤트(onClick, onMount) 발생 시 특정 애니메이션 타임라인을 재생(Play), 정지(Stop), 역재생(Reverse) 시킬 수 있는 신규 `RuntimeAction` 연결

#### 단계 2: 모션 타임라인 편집 패널 (Editor UX)
- [ ] **타임라인 하단 도크(Dock) 패널**: 에디터 하단에 숨길 수 있는 영상 편집기 스타일의 `Timeline Panel` 구축. 위젯의 트랙을 레이어 형태로 펼치고 눈금자(Ruler) 위에서 Playhead를 드래그하도록 뷰어 구성
- [ ] **직관적 키프레임 삽입**: 캔버스에서 특정 시간(프레임)으로 Playhead를 맞추고 위젯의 위치나 투명도를 바꾸면, 타임라인에 마름모(◆) 형태의 **자동 키프레임(Auto-Keyframe)**이 찍히는 토글 모드 지원
- [ ] **커스텀 곡선 에디터 (Curve Editor)**: 두 키프레임 사이의 선을 클릭하면 베지어 핸들이 노출되어, 모션의 가속/감속 곡선을 사용자가 직접 시각적으로 꺾고 조절할 수 있는 미니 팝업 렌더링

#### 단계 3: 모션 C++ Export 컴파일 (Export Pipeline)
- [ ] **C++ 런타임 제너레이터**: 정의된 프레임 타겟과 Easing 타입을 순수 `juce::Desktop::getInstance().getAnimator()` 또는 별도의 경량 Delta-time 루프로 컴파일 시간에 풀어써주는 Export 로직 분기 
- [ ] **Lottie/JSON 스니펫 호환성 검토**: 완전히 독자적인 C++ 애니메이션 루프 말고도, 타협안으로 벡터 애니메이션이 Lottie 데이터 규격과 부분 호환되도록 내보낼 수 있을지 실험

---

## Phase 13: 안정화/성능/릴리스

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
