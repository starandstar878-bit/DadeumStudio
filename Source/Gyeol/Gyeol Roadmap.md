# Gyeol Roadmap

기준일: 2026-02-27

## 진행판 (요약)
- [x] Phase 0. 아키텍처 코어 확정
- [x] Phase 1. 기본 Scene + Canvas
- [ ] Phase 1.5. 위젯 레지스트리 구조 확립 (진행중)
- [x] Phase 2. Export MVP
- [ ] Phase 3. 편집 고도화 (Editor UX) (진행중)
- [ ] Phase 4. Style 시스템
- [ ] Phase 5. 플러그인 친화 레이아웃
- [ ] Phase 6. 제품화 안정화
- [ ] Phase 7. 고급 확장 (선택)

## 운영 규칙
- [x] 시작한 항목 끝에 `(진행중)` 표기
- [x] 완료 항목은 `[x]`로 즉시 갱신
- [x] 각 Phase의 DoD 충족 전 다음 Phase 진입 금지
- [x] Phase 상태 변경 시 진행판/변경 이력 동시 업데이트

---

## Phase 0. 아키텍처 코어 확정 (완료)

### 목표
- 편집/저장/Undo/Redo의 기준 모델을 안정화한다.

### 체크리스트
- [x] `Gyeol::Document` immutable 구조 정의
- [x] `WidgetModel + PropertyBag` 설계
- [x] `Action` 스키마 정의
- [x] `Reducer` 구현
- [x] `DocumentStore` 구현 (`apply/undo/redo`)
- [x] 기본 JSON 저장/불러오기
- [x] Scene 검증기 최소 규칙 정리

### 결정 사항
- [x] `selection`은 `EditorStateModel`로 분리 (Export/런타임 출력에서 제외)
- [x] `WidgetId`는 `int64`, `rootId=0`
- [x] JSON에서 id는 string으로 저장
- [x] `schemaVersion = { major, minor, patch }`
- [x] 버전 정책: major 불일치 거부, future minor/patch 거부
- [x] PropertyBag 화이트리스트 타입만 허용
- [x] selection 변경도 Undo/Redo 포함

### DoD
- [x] Create/Delete/Move/Selection 테스트 통과
- [x] Undo/Redo 반복 안정성 확보
- [x] 저장/재로드 동등성 확보

---

## Phase 1. 기본 Scene + Canvas (완료)

### 체크리스트
- [x] `CanvasComponent`
- [x] `CanvasRenderer(CPU)`
- [x] 기본 `WidgetComponent` 타입
- [x] 단일 선택/드래그 이동/리사이즈
- [x] 위젯 생성/삭제 UI
- [x] Undo/Redo 단축키 연결

### DoD
- [x] 20개 이상 배치 시 기본 편집 안정
- [x] 드래그 + Undo 정상 동작

---

## Phase 1.5. 위젯 레지스트리 구조 확립 (진행중)

### 체크리스트
- [x] `WidgetDescriptor` 모델 정리
- [x] `WidgetRegistry` 인터페이스/기본 구현
- [x] Canvas 생성 경로를 Registry 기반으로 전환
- [x] 위젯 메타데이터 기반 렌더/생성 정리
- [x] Export 매핑 키(`exportTargetType`) 도입
- [x] 미지원 타입 fallback/경고 정책 정리
- [x] JSON 로드 시 미지원 타입 거부 정책 정리
- [ ] 전 타입 생성/저장/재로드 스모크 테스트 완료

### DoD
- [ ] 새 위젯 타입 추가 시 변경 지점이 Registry 중심으로 수렴

---

## Phase 2. Export MVP (완료)

### 체크리스트
- [x] Export 데이터 계약 문서화
- [x] JUCE `Component` 코드 생성기 구현
- [x] 기본 위젯(Button/Slider 등) 매핑
- [x] 리소스 복사 처리
- [x] Export 리포트 생성
- [x] Descriptor `exportCodegen` 콜백 경로 도입
- [x] 플러그인 ABI 확장(`export_codegen`)
- [x] Export 출력 폴더 실행별 분리(`.../<ComponentClass>_YYYYMMDD_HHMMSS`)

### DoD
- [x] 샘플 Scene export/컴파일 경로 동작
- [x] 에디터 미리보기와 산출물 UI 큰 차이 없음

---

## Phase 3. 편집 고도화 (Editor UX) (진행중)

### 상위 항목
- [x] 0) 구현 전략/엔진 분리 게이트
- [x] 1) 다중 선택
- [x] 2) 그룹/해제
- [x] 3) 레이어 트리
- [x] 4) 레이어 순서 변경
- [ ] 5) 정렬 기능
- [ ] 6) 그리드/스냅
- [ ] 7) 속성 패널 고도화
- [ ] 8) 성능 최적화/계측

### 0) 구현 전략/엔진 분리 게이트 (완료)
- [x] 역할 경계 확정: `DocumentStore`(상태/Undo), `Interaction`(입력 해석), `View`(렌더)
- [x] 서브 엔진 경계 확정: `SelectionEngine`, `LayerOrderEngine`, `SnapEngine`, `PropertyEditEngine`
- [x] 모델 변경은 Action 경유, UI 임시 상태는 Editor 상태로 한정
- [x] 성능 SLO 및 폴백 규칙 정의

### 1) 다중 선택 (완료)
- [x] Shift/Ctrl(Cmd) 추가/토글 선택
- [x] 마키 선택(교차 기반)
- [x] 다중 이동/다중 리사이즈 공통 변환
- [x] 화살표 nudge/큰 이동
- [x] 선택 + 배치 bounds 트랜잭션 정리

### 2) 그룹/해제 (완료)
- [x] 그룹 생성/해제 액션 + Undo/Redo
- [x] 그룹 경계 박스 계산/갱신
- [x] 그룹 내부 편집 UX(Alt 기반 진입) 정리
- [x] 중첩 그룹 모델 반영
- [x] Export flatten 정책 유지

### 3) 레이어 트리 (완료)
- [x] 트리 어댑터(`DocumentModel -> Tree`)
- [x] Layer Panel UI(선택 동기화, 펼침/접힘 유지)
- [x] 검색/필터(이름/타입 substring)
- [ ] 잠금/가시성 토글 (deferred)
- [ ] 대량 노드 가상화/부분 렌더 (deferred)

### 4) 레이어 순서 변경 (완료)
- [x] Tree DnD reorder/reparent
- [x] Bring Forward / Send Backward / To Front / To Back
- [x] 부모-자식 제약 및 사이클 방지 검증
- [x] Undo/Redo 트랜잭션 정책 적용
- [x] Canvas z-order와 Layer Tree 동기화

### 5) 정렬 기능 (예정)
- [ ] 기준점 정책 확정(선택/부모/캔버스)
- [ ] 좌/우/상/하/중앙 정렬
- [ ] 간격 균등 분배
- [ ] 정렬 프리뷰 가이드

### 6) 그리드/스냅 (예정)
- [ ] 그리드 표시/간격/원점 모델
- [ ] 스냅 우선순위 규칙
- [ ] 드래그/리사이즈 스냅 파이프라인
- [ ] 임시 스냅 해제 modifier 규칙

### 7) 속성 패널 고도화 (예정)
- [ ] Descriptor 기반 동적 속성 편집기
- [ ] 다중 선택 공통 속성 일괄 편집
- [ ] 입력 검증/클램프/단위 처리
- [ ] 변경 반영 debounce/병합 규칙

### 8) 성능 최적화/계측 (예정)
- [ ] 핵심 상호작용 계측 포인트 추가
- [ ] repaint/layout 최소화
- [ ] hit-test 가속 구조 검토
- [ ] 이벤트 스로틀링/코얼레싱 적용
- [ ] 성능 리포트 포맷(`p50/p95`) 고정

### Phase 3 DoD
- [ ] 50~100 위젯 씬에서 선택/이동/그룹/레이어 편집 안정
- [ ] Layer Tree-Canvas 상태 불일치 0건
- [ ] Undo/Redo 100회 반복 불일치 0건
- [ ] 핵심 상호작용 p95 목표 충족

---

## Phase 4. Style 시스템 (예정)
- [ ] `StyleToken` 모델
- [ ] 전역 스타일 테이블
- [ ] 상태 스타일(hover/pressed)
- [ ] 스타일 resolver
- [ ] 프리셋 저장/적용

## Phase 5. 플러그인 친화 레이아웃 (예정)
- [ ] Anchor 모델
- [ ] `%/px` 제약 모델
- [ ] DPI 프리뷰 모드
- [ ] 폴백 규칙

## Phase 6. 제품화 안정화 (예정)
- [ ] 자동 저장
- [ ] 대규모 Scene 성능 프로파일링
- [ ] 렌더 최적화
- [ ] Preflight 검사 강화
- [ ] QA 체크리스트 문서화

## Phase 7. 고급 확장 (선택)
- [ ] 애니메이션
- [ ] 상태머신
- [ ] 연동 포인트 확장
- [ ] 템플릿 패키징

---

## 공통 완료 조건
- [ ] 빌드 오류 0건
- [ ] 새 warning 0건(기존 warning 제외)
- [ ] 변경 기능 수동 테스트 완료
- [ ] 로드맵 체크 상태 최신화

## 변경 이력
- [x] 2026-02-26: Phase 1 기본 Canvas/입력 루프 구현 완료
- [x] 2026-02-27: Phase 1.5 레지스트리 구조/메타데이터 전환 진행
- [x] 2026-02-27: Phase 2 Export MVP + codegen 콜백/ABI 확장 완료
- [x] 2026-02-27: Phase 3 0~4 단계(전략/다중선택/그룹/레이어트리/레이어순서) 구현 완료
- [ ] YYYY-MM-DD: Phase / 작업 / 결과 / 남은 이슈

