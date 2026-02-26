# Gyeol 구현 로드맵 (의도 순서 고정)

## 구현 순서 원칙
아래 순서를 반드시 지킨다.
`아키텍처 코어 확정 -> 기본 Scene + Canvas -> Export MVP -> 편집 고도화 -> Style 시스템 -> 플러그인 친화 레이아웃 -> 제품화 안정화 -> 고급 확장`

## 진행판(요약)
- [ ] Phase 0. 아키텍처 코어 확정 (가장 먼저) (진행중)
- [ ] Phase 1. 기본 Scene + Canvas (진행중)
- [ ] Phase 2. Export MVP
- [ ] Phase 3. 편집 고도화 (Editor UX)
- [ ] Phase 4. Style 시스템
- [ ] Phase 5. 플러그인 친화 레이아웃
- [ ] Phase 6. 제품화 안정화
- [ ] Phase 7. 고급 확장 (선택)

## 체크 규칙
- [ ] 시작한 항목 끝에 `(진행중)`을 붙인다.
- [ ] 끝난 항목은 `[ ]`를 `[x]`로 바꾼다.
- [ ] 각 Phase의 완료 기준(DoD)을 모두 만족해야 다음 Phase로 넘어간다.
- [ ] Phase 완료 시 진행판(요약)도 즉시 업데이트한다.

---

## Phase 0. 아키텍처 코어 확정 (가장 먼저)
목표: 모든 기능의 기반이 되는 데이터 구조를 고정한다.

선행 조건:
- [ ] 없음

작업 체크리스트:
- [x] `Gyeol::Document` immutable 구조 정의
- [x] `WidgetModel + PropertyBag` 설계
- [x] `Action` 스키마 정의
- [x] `Reducer` 구현
- [x] `DocumentStore` 구현 (`apply / undoStack / redoStack`)
- [x] 기본 JSON 저장/불러오기
- [ ] Scene 검증기(최소 규칙)

결정 확정(락):
- [x] `selection`은 `EditorStateModel`로 분리하고 Export/런타임 출력에서는 무시한다.
- [x] `bounds` 타입은 `juce::Rectangle<float>`로 고정한다.
- [x] `WidgetId`는 `int64` 시퀀스를 사용하고 `rootId=0`을 고정한다.
- [x] JSON 저장 시 `widget.id`는 문자열(string)로 기록한다.
- [x] JSON 로드 직후 `nextWidgetId = max(widget.id) + 1`로 재동기화하며 최소값은 1이다.
- [x] `schemaVersion`은 `{ major, minor, patch }` 정수 필드로 분리한다.
- [x] 버전 정책은 `major 불일치 거부`, `future minor/patch 거부`, `older minor/patch 마이그레이션`이다.
- [x] 마이그레이션 전략은 `Step Registry` 체인 방식으로 고정한다.
- [x] PropertyBag은 화이트리스트 타입만 허용하고 금지 타입(포인터/핸들/함수/blob/절대경로 등)을 차단한다.
- [x] PropertyBag의 `int64`는 `Tagged String` 포맷으로 저장한다.
- [x] `selection` 변경은 Undo/Redo 스택에 포함한다.
- [x] Undo 병합은 `Scoped Transaction` 기준으로 처리한다.
- [x] `bounds`는 JSON number로 저장하고 비교/검증 시 `eps=1e-4`를 사용한다.

검증 체크리스트:
- [ ] Create/Delete/Move/Selection 테스트 통과
- [ ] Undo/Redo 20회 반복 정상
- [ ] 저장 -> 재로드 -> 상태 동일

완료 기준(DoD):
- [ ] 데이터 모델 변경 없이 기능을 계속 추가할 수 있다.

---

## Phase 1. 기본 Scene + Canvas
목표: 렌더 + 입력 루프 완성

선행 조건:
- [ ] Phase 0 완료

작업 체크리스트:
- [x] `CanvasComponent`
- [x] `CanvasRenderer(CPU)`
- [x] `WidgetComponent` 기본 타입 구현
- [x] 단일 선택
- [x] 드래그 이동
- [x] 리사이즈
- [x] 위젯 생성/삭제 UI
- [x] Undo/Redo 단축키 연결

검증 체크리스트:
- [ ] 위젯 20개 이상 배치 테스트
- [ ] 드래그 + Undo 정상

완료 기준(DoD):
- [ ] 단순 플러그인 UI 1개를 배치 가능

---

## Phase 2. Export MVP
목표: 구조가 실제 JUCE 코드로 변환 가능한지 검증

선행 조건:
- [ ] Phase 1 완료

작업 체크리스트:
- [ ] Export 데이터 계약 문서화
- [ ] 기본 JUCE Component 코드 생성
- [ ] 버튼/슬라이더 매핑
- [ ] 리소스 복사 처리
- [ ] Export 리포트

검증 체크리스트:
- [ ] 샘플 Scene export -> JUCE 프로젝트 컴파일 성공
- [ ] 런타임 UI와 에디터 미리보기 일치

완료 기준(DoD):
- [ ] "디자이너 -> 코드" 경로가 완성

---

## Phase 3. 편집 고도화 (Editor UX)
목표: 실사용 가능한 에디터 완성

선행 조건:
- [ ] Phase 2 완료

작업 체크리스트:
- [ ] 다중 선택
- [ ] 그룹/해제
- [ ] 레이어 트리
- [ ] 레이어 순서 변경
- [ ] 정렬 기능
- [ ] 그리드/스냅
- [ ] 속성 패널 고도화

검증 체크리스트:
- [ ] 50개 위젯 조작 안정성

완료 기준(DoD):
- [ ] 중간 규모 Scene 편집에서 UX 병목 없이 작업 가능

---

## Phase 4. Style 시스템
목표: 재사용 가능한 디자인 시스템 구축

선행 조건:
- [ ] Phase 3 완료

작업 체크리스트:
- [ ] `StyleToken` 모델
- [ ] 전역 스타일 테이블
- [ ] 상태 스타일(hover/pressed 등)
- [ ] 스타일 resolver
- [ ] 프리셋 저장

검증 체크리스트:
- [ ] 테마 변경 즉시 반영
- [ ] 상태 스타일 정상 동작

완료 기준(DoD):
- [ ] 스타일 재사용 흐름이 실편집/미리보기/산출물에서 일관된다.

---

## Phase 5. 플러그인 친화 레이아웃
목표: 리사이즈/DPI 안정화

선행 조건:
- [ ] Phase 4 완료

작업 체크리스트:
- [ ] Anchor 모델
- [ ] `%/px` 제약
- [ ] DPI 프리뷰 모드
- [ ] 폴백 규칙

검증 체크리스트:
- [ ] 3개 창 크기 x 4 DPI 테스트 통과

완료 기준(DoD):
- [ ] 레이아웃 규칙이 호스트 크기/스케일 변화에서 안정적으로 유지된다.

---

## Phase 6. 제품화 안정화
목표: 반복 사용 가능한 도구 수준

선행 조건:
- [ ] Phase 5 완료

작업 체크리스트:
- [ ] 자동 저장
- [ ] 성능 프로파일링(200+ 위젯)
- [ ] 렌더 최적화
- [ ] Preflight 검사 강화
- [ ] QA 문서화

검증 체크리스트:
- [ ] 대규모 Scene에서 편집/렌더 성능 저하가 허용 범위 내
- [ ] Preflight가 주요 오류를 사전에 검출

완료 기준(DoD):
- [ ] 팀 단위 반복 사용 가능한 안정성과 성능을 확보한다.

---

## Phase 7. 고급 확장 (선택)
목표: 고급 인터랙션 및 확장 생태계 기반 확보

선행 조건:
- [ ] Phase 6 완료

작업 체크리스트:
- [ ] 애니메이션
- [ ] 상태머신
- [ ] Hareum 연동 포인트
- [ ] 템플릿 패키징

검증 체크리스트:
- [ ] 고급 기능을 켜도 기존 편집/Export 흐름이 깨지지 않는다.

완료 기준(DoD):
- [ ] 확장 기능 1종 이상을 실제 프로젝트에서 재사용 가능

---

## 공통 품질 게이트(모든 Phase 공통)
- [ ] 빌드 오류 0건
- [ ] 새 compiler warning 0건
- [ ] 변경된 기능에 대한 수동 테스트 완료
- [ ] 로드맵 체크 상태 최신화 완료

## 구현 로그(갱신용)
- [ ] 2026-02-26: Phase 1 / Canvas+렌더+입력 루프 구현 / 생성·선택·이동·리사이즈·UndoRedo 단축키 동작 / 20개 배치 수동 검증 필요
- [ ] YYYY-MM-DD: Phase / 작업 / 결과 / 남은 이슈
- [ ] YYYY-MM-DD: Phase / 작업 / 결과 / 남은 이슈
