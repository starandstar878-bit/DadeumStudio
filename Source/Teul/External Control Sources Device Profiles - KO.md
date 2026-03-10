# 외부 제어 소스 / 디바이스 프로필

`Teul Execution Plan.md`의 `External Control Sources / Device Profiles` 항목을 한글로 분리 정리한 문서입니다.

---

## UI 레이아웃 초안

- 왼쪽 `Input Rail`: 에디터 전체 폭의 `12% - 14%`
- 오른쪽 `Output Rail`: 에디터 전체 폭의 `12% - 14%`
- 하단 `Control Rail`: 에디터 전체 높이의 `14% - 18%`
- 중앙 캔버스는 나머지 공간을 유지하며, 기본 DSP 편집 영역으로 사용
- 그래프 편집이 복잡해질 때 캔버스가 공간을 다시 확보할 수 있도록 각 Rail은 접기/펼치기가 가능해야 함

## 비주얼 언어

- 일반 노드 포트는 내부 그래프 배선을 위한 작고 중립적인 소켓 형태를 유지
- Rail 포트는 일반 노드 핀보다는 하드웨어 경계 커넥터처럼 보여야 함
- 기본 상태에서는 Rail이 `jack/socket` 스타일의 엔드포인트로 보여야 하며, 드래그 시에는 기존 케이블 렌더링 방식을 재사용
- 케이블 두께는 대체로 일관되게 유지하고, 차이는 선 굵기보다는 포트 실루엣, 슬롯 배경, 커넥터 캡/스터브 형태로 표현

## Rail별 포트 형태 규칙

- `Input Rail` / `Output Rail`: 프레임과 테두리 표현이 더 강한 캡슐형 또는 반-잭형 커넥터
- `Control Rail`: `Value`, `Gate`, `Trigger`용 작은 `pill` 포트를 가진 소스 카드
- `Expression` 소스는 기본적으로 단일 `Value` 포트 사용
- `Footswitch` 소스는 기본적으로 `Gate` + `Trigger` 포트 사용
- 스테레오 오디오 엔드포인트는 서로 무관한 점 두 개가 아니라 하나로 묶인 `L/R` 커넥터처럼 보여야 함

## 색상 방향

- Rail 배경은 캔버스보다 한 톤 더 진하게 해서 시스템 패널처럼 읽히게 함
- 오디오 I/O 강조색: teal / cyan 계열
- 연속 제어(`Value`) 강조색: warm yellow / amber 계열
- Gate / Trigger 강조색: orange / red 계열
- MIDI 계열 강조색: mint / lime 계열
- Rail 커넥터는 내부 노드 포트보다 더 강한 테두리와 약간 낮은 채도를 유지

## 예정된 리소스 사용 방식

- DPI 스케일링, 줌, 상태 전환을 고려해 Rail, 포트, 케이블은 코드 기반 드로잉을 우선 사용
- `Input`, `Output`, `Expression`, `Footswitch`, `MIDI`, `Missing`, `Learn`, `Auto` 배지용 소형 재사용 아이콘 리소스를 추가
- 비트맵 기반 케이블이나 큰 장식용 패널 배경은 지양
- 이미지 에셋이 필요하더라도 핵심 인터랙션 도형이 아니라 아이콘, 배지, 미니 글리프 수준으로 제한

## 2단계 목표

- 새 문서 모델의 placeholder 데이터를 사용해 세 개의 Rail을 렌더링
- 초안 단계에서 최소한 `EXP 1`, `FS 1`, 그리고 하나의 오디오 입력/출력 엔드포인트를 표시
- 디바이스 감지나 Learn 모드를 연결하기 전에 스크린샷으로 간격, 카드 밀도, 포트 가독성을 검증

## 목표

- 외부 MIDI foot controller, expression pedal, switch 입력을 그래프의 시스템 경계로 다루고, preset/state 복원 흐름과 함께 안정적으로 저장하고 재연결한다.

## 주요 작업

- `Control Source Rail` 모델 도입
  - 좌측 `Input Rail`, 우측 `Output Rail`, 하단 `Control Rail` 구조를 기준으로 시스템 I/O와 일반 DSP 노드를 분리
- 동적 장치 감지
  - 연결된 외부 컨트롤 장치를 감지하고, 입력 이벤트를 바탕으로 임시 control source를 자동 생성
- `learn + confirm` 등록 흐름
  - 사용자가 페달/스위치를 움직여 `EXP`, `FS`, `Trigger` source를 확정하고 이름, 타입, momentary/toggle 모드를 보정
- device profile persist
  - 장치별 source 구성, 표시 이름, binding 정보를 profile로 저장하고 재연결 시 안정적으로 복원
- preset/state 연동 복원
  - 문서 로드, preset 전환, crash recovery 이후에도 control source assignment와 device mapping이 일관되게 복원되도록 상태 모델 정리
- fallback / partial recovery 정책
  - 장치가 없거나 source 일부가 누락된 경우 `unassigned`, `degraded`, `relink-needed` 상태를 명시적으로 유지
- assignment inspector 계약 정리
  - `Value`, `Gate`, `Trigger` 같은 source 출력 타입과 target parameter binding 규칙을 명문화
- 검증 항목 추가
  - device reconnect, profile mismatch, preset reload, missing controller 상황에 대한 state recovery 테스트 추가

## 완료 기준

- 대표 foot controller / expression 장치를 연결했을 때 source가 자동 감지되고, 사용자가 learn으로 의미를 확정할 수 있음
- 저장 후 재실행하거나 장치를 재연결해도 source profile과 assignment가 일관되게 복원됨
- 장치가 누락되거나 구성이 달라져도 문서가 깨지지 않고 degraded 상태로 복구됨

## 바로 이어서 UI

- UI `Phase 8`
  - Preset Browser
  - 노드 상태 스냅샷
  - 변경 비교 보기
  - dirty state 표시
  - crash recovery 대화상자
  - 충돌 해결 흐름
  - migration 경고 배너
  - deprecated/alias 표시
  - 복구 가능성 등급

## 이 단계가 중요한 이유

- 상용 툴에서 사용자가 가장 싫어하는 것은 데이터 손실임
- 검증 인프라가 준비된 뒤에 preset/migration을 얹어야 포맷 변경을 안전하게 운영할 수 있음

## 게이트

- autosave 복구 경로가 실제로 동작
- 구버전 문서/프리셋을 migration 후 다시 열 수 있음
- recovery/migration 실패가 사용자에게 설명 가능한 상태로 노출됨