# Gyeol 로드맵
업데이트: 2026-02-28

## 목적
- 에디터/문서/익스포트/런타임을 분리된 계층으로 유지한다.
- Public API는 단순하게 유지하고, 내부 구현은 노드 종류별 분기 처리한다.
- Phase 0~3은 체크리스트 중심으로 운영하고, 이후 Phase는 구현 계획 + 파일 목록 중심으로 운영한다.

## 공통 원칙
- 문서 구조: `root > layer > group > widget`
- 저장 포맷: JSON 단일 소스 오브 트루스
- Undo 정책: 사용자 1회 의도 동작 = Undo 1회
- 빌드 기준: LLVM Debug x64 우선 검증
- 라인 엔딩: CRLF 유지

---

## Phase 0~3 개발자 체크리스트

### Phase 0: 코어 모델/액션 안정화
- [x] `DocumentModel`, `EditorStateModel`, 기본 Validator 구조 확정
- [x] 액션 최소 집합 정리: `Create/Delete/SetProps/SetBounds/Reparent/Reorder`
- [x] Public API 단순화, 내부 kind 분기 유지
- [x] JSON round-trip 안정화(누락 필드 기본값 포함)
- [x] include 경로 정리(`Gyeol/...`)
- [x] LLVM Debug x64 컴파일 통과

### Phase 1: 상호작용 엔진/선택/리사이즈
- [x] `EditorInteractionEngine` 중심 구조 확정
- [x] 다중 선택 union bounds 리사이즈/이동 동작
- [x] 다중 리사이즈 시 빈 공간 비율 유지
- [x] 선택 표시/핸들 표시 분리
- [x] Alt 기반 그룹 내부 선택 UX(최상위/하위 선택 규칙)
- [x] 리사이즈/드래그 중 잔상/리페인트 이슈 수정

### Phase 2: Export MVP
- [x] Export 결과물 생성 경로 정리(타임스탬프 폴더)
- [x] manifest/report/header/source 출력
- [x] 외부 등록 위젯 codegen 확장 포인트 반영
- [x] ABI 업데이트 반영
- [x] flatten 정책 유지(편집 그룹 구조는 export 코드에 직접 노출하지 않음)

### Phase 3: Layer/Group/Inspector/스냅 기반 편집 UX
- [x] Layer/Group/Widget 트리 DnD(reorder/reparent)
- [x] Layer 패널 선택/캔버스 선택 동기화
- [x] Layer 추가/삭제 및 기본 ActiveLayer 정책
- [x] Grid View / Grid Snap / Smart Snap 분리
- [x] Guide 생성/삭제(룰러 드래그, 더블클릭 삭제)
- [x] 캔버스 줌/패닝/중앙 정렬 정책 반영
- [x] Inspector 패널 기본 편집(공통/Transform/Appearance/Widget Properties)
- [x] WidgetRegistry 기반 위젯 목록 사용(하드코딩 제거)

---

## Phase 3.5: 고급 Editor UX (진행 중)

### 패널 구현 현황
- [x] LayerTree Panel
- [x] Widget Library Panel
- [x] Assets Panel(1차)
- [x] Inspector Panel
- [x] Event/Action Panel(MVP, 편집/저장/검증)
- [x] Validation Panel
- [x] Export Preview Panel
- [x] History Panel
- [ ] Performance Panel
- [ ] Navigator Panel

### Event/Action Panel 검사 항목
- [x] `DocumentModel.runtimeBindings` 저장/로드
- [x] `WidgetDescriptor.runtimeEvents` 기반 이벤트 표시
- [x] 바인딩/액션 추가/삭제/복제/순서변경
- [x] 액션 kind별 필드 표시 분기
- [x] Validation 연동(잘못된 source/event/target 경고)
- [x] 저장 후 재로드 round-trip 일치

### Event/Action Panel 개선 백로그 (UX)
- [ ] `AdjustRuntimeParam` 수치 입력창 상시 가시화(최소 너비/행 높이 고정/클리핑 방지)
- [ ] `paramKey`를 수동 입력 대신 검색/자동완성 리스트로 제공
- [ ] 편집 가능한 노드(`Layer/Group/Widget`)를 이름+ID 리스트로 선택 제공
- [ ] 내부 액션명(`SetNodeProps`) 대신 사용자 친화 라벨 사용, 내부 키는 괄호 보조 표기
- [ ] 액션 payload 라벨/플레이스홀더 개선(`value`, `delta`, `target`, `property`)
- [ ] 키보드 흐름 개선(Tab 순서, Enter 커밋, Esc 취소)

### Validation Panel 검사 항목
- [x] 문서 구조 검증(고아 노드, 잘못된 부모, 순환 참조)
- [x] 속성 검증(불허 속성/타입 불일치/범위 경고)
- [x] runtimeBindings 검증(source/event/action payload)
- [x] 패널 갱신 시 stale 상태 없이 즉시 반영

### Validation Panel 개선 백로그
- [ ] 경고 클릭 시 대상 노드 자동 포커스/선택 강화
- [ ] 카테고리 필터(구조/속성/런타임/에셋)
- [ ] 자동 수정 가능한 항목 Quick Fix

### Export Preview Panel 검사 항목
- [x] Export 결과 텍스트 미리보기(Report/Header/Source/Manifest)
- [x] Export 직후 자동 갱신
- [x] preview와 실제 생성 파일 불일치 0건 유지

### History Panel 검사 항목
- [x] Undo/Redo 스택 표시
- [x] 현재 포인터 표시
- [x] Undo/Redo 시 기록 반영
- [x] 기본 접힘 + 타이틀바 토글

### Assets Panel 검사 항목 (1차)
- [x] 에셋 목록/검색/필터
- [x] 파일/컬러 자산 등록
- [x] 참조 키 복사/삭제
- [x] 문서 저장 연동

### Assets Panel 개선 백로그 (2차)
- [x] `assetRef` 타입 에디터 Inspector 연동(에셋 선택 드롭다운)
- [x] `assetRef` 타입 에디터 Event/Action 연동
- [x] rename/rekey 시 참조 일괄 치환
- [x] import 패키징(UI/스키마 검증) + export 패키지 ZIP화
- [x] 외부 파일 드래그앤드롭으로 Assets Panel 임포트
- [x] Assets Panel -> Canvas/Inspector 드래그앤드롭 적용(유효 위젯만 적용)

### Assets Panel 개선 백로그 (3차, 내일 구현 후보)
- [x] 드롭 옵션 선택 UI: `dropOptions`가 여러 개인 위젯에 대해 드롭 시 선택 팝업 제공
- [x] 썸네일/미리듣기: 이미지 썸네일 + 오디오 Play/Stop 미리듣기
- [x] 사용처 카운트 배지: 에셋별 `사용중 N개 위젯` 표시
- [x] 사용처 역추적 리스트: 선택 에셋의 연결 위젯/프로퍼티 목록 + 클릭 시 포커스 이동
- [x] 미사용 에셋 필터 + 일괄 정리 버튼
- [x] 재임포트(Reimport): 원본 파일 변경 시 같은 refKey 유지 갱신
- [x] Replace Asset: 참조 유지한 채 파일만 교체
- [x] 중복 에셋 탐지(해시/경로/이름) + 병합 도구
- [x] 임포트 충돌 정책 선택: 덮어쓰기/이름변경/건너뛰기
- [x] Missing 경로 재연결 도우미(배치 리링크)
- [x] Export 포함/제외 토글(에셋 단위) + Preview 반영
- [x] 에셋 품질/용량 경고(해상도, 파일 크기, MIME 불일치) Validation 연동

### Assets Panel 2차 개발 단계 (Export 연계)

#### 목표
- Export 결과물 폴더에 `Assets/` 디렉터리를 생성한다.
- 문서 `assets[].path`가 가리키는 원본 파일을 `Assets/`로 복사한다.
- export된 GUI 구현 코드가 원본 경로가 아니라 export 폴더 내부 `Assets/` 경로를 사용하도록 전환한다.

#### 체크리스트
- [x] Export 시작 시 출력 루트에 `Assets/` 생성
- [x] `DocumentModel.assets` 순회 후 파일 존재/상대경로 정규화
- [x] 파일명 충돌 처리 규칙 적용(예: `name`, `name_2`, `name_3`)
- [x] 복사 결과를 manifest에 기록(`sourcePath`, `exportPath`, `mime`, `copied`)
- [x] 누락 파일은 export 중단 대신 warning으로 리포트에 누적
- [x] 생성 코드에서 에셋 참조를 `Assets/...` 기준으로 사용
- [x] 런타임 로딩 helper(이미지/폰트)에서 export 상대 경로 해석
- [x] Export Preview에 assets 복사 요약(성공/실패/누락) 표시

#### 수정/추가 파일
- 수정: `Source/Gyeol/Export/JuceComponentExport.h`
- 수정: `Source/Gyeol/Export/JuceComponentExport.cpp`
- 수정: `Source/Gyeol/Export/ExportDataContract.md`
- 수정: `Source/Gyeol/Editor/EditorHandle.cpp` (Export Preview 요약 표기)

---

## Phase 4: 런타임 Event/Action 실행 엔진

### 목표
- 에디터에서 저장한 `runtimeBindings`를 런타임에서 실제 실행한다.
- UI 스레드/오디오 스레드 경계를 지키는 실행 정책을 확정한다.

### 세부 계획
- [ ] Runtime 이벤트 디스패처 도입(위젯 이벤트 -> 바인딩 조회)
- [ ] Runtime 액션 실행기 도입(MVP 5종 액션)
- [ ] 파라미터 라우팅 계층(DAW/플러그인 파라미터 브리지)
- [ ] 실행 실패/무효 타깃 로깅 정책
- [ ] 성능 가드(프레임당 실행량 제한/중복 억제)

### 수정/추가 파일
- 수정: `Source/Gyeol/Public/Types.h`
- 수정: `Source/Gyeol/Serialization/DocumentJson.cpp`
- 수정: `Source/Gyeol/Core/SceneValidator.h`
- 추가: `Source/Gyeol/Runtime/RuntimeBindingEngine.h`
- 추가: `Source/Gyeol/Runtime/RuntimeBindingEngine.cpp`
- 추가: `Source/Gyeol/Runtime/RuntimeActionExecutor.h`
- 추가: `Source/Gyeol/Runtime/RuntimeActionExecutor.cpp`
- 수정: `Source/Gyeol/Export/*` (필요 최소 범위)

---

## Phase 5: 데이터 바인딩/상태 시스템

### 목표
- 위젯 속성과 런타임 상태(파라미터/메타 값)를 선언적으로 연결한다.

### 세부 계획
- [ ] Binding 표현식 스키마 설계(단순 경로/연산자 제한형)
- [ ] 단방향/양방향 바인딩 정책
- [ ] 충돌 정책(수동 편집 vs 바인딩 업데이트)
- [ ] Inspector에서 바인딩 표시/해제 UI
- [ ] Validation 규칙 확장(끊긴 참조, 타입 불일치)

### 수정/추가 파일
- 수정: `Source/Gyeol/Public/Types.h`
- 추가: `Source/Gyeol/Public/BindingModel.h`
- 추가: `Source/Gyeol/Core/BindingResolver.h`
- 추가: `Source/Gyeol/Core/BindingResolver.cpp`
- 수정: `Source/Gyeol/Editor/Panels/PropertyPanel.*`
- 수정: `Source/Gyeol/Core/SceneValidator.h`

---

## Phase 6: Export 런타임 브리지

### 목표
- 에디터 기능이 export 코드에서도 동일하게 동작하도록 런타임 브리지를 만든다.

### 세부 계획
- [x] export 출력물에 `Assets/` 폴더 생성 + 자산 파일 복사 파이프라인
- [x] export GUI 코드의 자산 경로를 `Assets/...` 기준으로 고정
- [ ] export 코드에 runtimeBindings 포함
- [ ] 위젯 이벤트 emit 코드 생성
- [ ] 액션 실행기 연결 코드 생성
- [ ] assets/bindings 포함한 manifest 규약 확정
- [ ] 샘플 프로젝트로 회귀 테스트

### 수정/추가 파일
- 수정: `Source/Gyeol/Export/ExportDataContract.md`
- 수정: `Source/Gyeol/Export/JuceComponentExport.*`
- 수정: `Source/Gyeol/Serialization/DocumentJson.*`
- 추가: `Source/Gyeol/Export/RuntimeBridgeTemplates/*`

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
