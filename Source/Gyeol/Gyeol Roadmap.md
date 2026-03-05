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
> 현재 구조: 전체 씬 스캔 후 Error/Warning/Info 리스트 출력.

**✨ 현대적 UI/UX (Modern Aesthetics)**
- [ ] **Glassmorphism 플로팅 아이템**: 리스트 항목을 단조로운 박스가 아닌, 반투명한 블러(Blur) 배경의 플로팅 카드로 렌더링하여 뒷배경(캔버스 등)이 은은하게 비치도록 연출
- [ ] **상태별 펄스(Pulse) 애니메이션**: 치명적인 에러(Error) 배지나 아이콘에는 미세하게 숨쉬는 듯한(Pulse) 마이크로 애니메이션을 주어 시각적 중요도 강조
- [ ] **Fluid 트랜지션**: 항목이 삭제되거나(`Fix` 등) 필터링될 때 딱딱하게 사라지지 않고, 부드럽게 접히거나(Collapse) 페이드아웃(Fade-out)되는 자연스러운 전환 애니메이션 적용

**🔍 지연 검사 및 시각적 피드백 (Lazy Inspection)**
- [ ] **에러 연쇄 추적 (Dependency Map)**: 바인딩 에러 발견 시, 파급되는 위젯 간의 의존성(DAG) 트리를 **항목 클릭 시에만 온디맨드(On-demand) 렌더링**
- [ ] **오류 발생 영역 인라인 하이라이팅**: 리스트 항목 호버 시 캔버스 내 해당 위젯에 뷰포트 내 점선 또는 틴트 하이라이트 제공 (초당 프레임을 떨어뜨리지 않는 경량 렌더)
- [ ] **비동기 백그라운드 스캐닝**: 무거운 씬 스캔은 메인 스레드 블로킹 방지를 위해 입력 또는 편집 중지 후 300~500ms 딜레이(Debounce)를 두고 백그라운드 스레드에서 점진적 실행

**🎯 자동화된 해결 및 예방 (Auto-Resolution)**
- [ ] **인터랙티브 퀵픽스(Quick Fix) 및 Diff 프리뷰**: `Fix` 버튼 제공 및 수정될 속성의 전/후 상태를 보여주는 경량 미니 모달 모드
- [ ] **스마트 에셋 대체기 (Smart Fallback)**: 누락된 이미지/폰트 에셋 발견 시, 캐시 디렉터리나 기본 지정 폰트로 일괄 대체하는 One-Click 복구
- [ ] **Export 호환성 사전 검증기**: "JUCE C++ Native Export" 과정에서 지원되지 않는 문법이나 형태를 스캔 과정에서 함께 캐치

#### Navigator 패널
> 현재 구조: Document 전체 썸네일 표시 + 현재 Viewport 오버레이

**✨ 현대적 UI/UX (Modern Aesthetics)**
- [ ] **팬텀 보더(Phantom Border) & 라운드 오버레이**: 사각형의 딱딱한 패널을 벗어나, 모서리 곡률(Border Radius)이 큰 둥근 형태의 플로팅 뷰어로 디자인하며, 가장자리에 매우 얇은 반투명 그라데이션 보더 적용
- [ ] **시네마틱 뷰포트 아웃라인**: 현재 화면 영역을 나타내는 파란색 반투명 박스(Viewport)를 드래그할 때, 경계선에서 부드러운 네온 글로우(Glow) 효과 발생 (GyeolPalette 엑센트 컬러 기반)
- [ ] **호버형(Hover) 투명도 조절**: 마우스를 미니맵 위로 올리지 않았을 땐 투명도를 30% 수준으로 낮춰 실제 편집 영역을 방해하지 않고, 마우스를 올릴 때만 100% 진하게 스르륵 떠오르는(Fade-in) 인터랙션 

**🧭 매크로 네비게이션 및 의미론적 조작 (Semantic Navigation)**
- [ ] **시맨틱 줌(Semantic Zoom) 렌더링 최적화**: 미니맵 축소 비율이 커질수록 텍스트 렌더나 복잡한 그룹의 그림자 연산을 생략하고 단색 사각형(LOD 블록)으로 추상화하여 미니맵 페인트 성능 대폭 개선
- [ ] **더티 영역 교차 동기화 (Dirty Rect Caching)**: 오버레이 이미지 전체를 매 프레임 다시 그리지 않고 `setBufferedToImage(true)`를 활용해 캔버스에서 변경이 일어난 구역만 비동기로 캐시 갱신
- [ ] **화면 밖 객체 지시기 (Off-screen Indicators)**: 바인딩이나 퀵서치로 선택된 타겟 위젯이 현재 화면 밖에 있을 때, 네비게이터 가장자리에 화살표(방향/거리) 가이드 노출

#### Performance 패널
> 현재 구조: Paint Time / 렌더 카운트 등을 수치로 리포트.

**✨ 현대적 UI/UX (Modern Aesthetics)**
- [ ] **미니멀 스파크라인 (Sparkline Chart)**: 복잡한 눈금선이나 축이 없는, 애플 워치 스타일의 곡선형 그라데이션 라인 차트를 사용하여 프레임 타임을 세련되고 직관적으로 표시
- [ ] **동적 컬러 리본**: 차트나 수치가 임계치(예: 16ms)에 다다르면 초록색 -> 노란색 -> 빨간색으로 그라데이션 전환되는 부드러운 색상 보간(Interpolation) 연출
- [ ] **네오모피즘(Neumorphism) 계기판**: 패널 버튼(`진단 시작` 등)이나 게이지 바를 입체적이고 세련진 음각/양각 느낌으로 구현하여 대시보드(Dashboard) 같은 프리미엄 느낌 강조

**📊 시기 적절한 (Time-Sensitive) 프로파일링 및 진단**
- [ ] **Run 모드 진입 시 자동 스모크 테스트**: 에디터에서 `Run 모드`로 전환하는 순간 백그라운드에서 짧게(약 2~3초간) 바인딩 평가 소요 시간과 평균 프레임 레이트를 추산하여, 병목이 감지되면 짧은 알림(Notification)으로 경고
- [ ] **수동 심층 진단 (Deep Scan Button)**: 캔버스 작업 중에는 완전히 비활성화하다가 유저가 패널 내 `진단 시작` 버튼을 눌렀을 때만 60프레임 Paint 타임 스파크라인 및 Overdraw (과결합) 히트맵 오버레이를 화면에 시각화
- [ ] **Export 직전 메타데이터 계산 (Pre-flight Check)**: 메모리 풋프린트 차트(폰트/이미지 등 예상 RAM 사용량)는 평소 계산하지 않고, 컴파일/Export(패키징) 전이나 유저가 정보 탭을 열람할 때만 일회성으로 추산

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
