# 외부 제어 소스 / 디바이스 프로필

`Teul Execution Plan.md`의 `External Control Sources / Device Profiles` 항목을 분리하고, 현재 구현 상태와 MVP까지의 수정 계획을 함께 정리한 문서다.

---

## 목표

- 외부 오디오 입력, MIDI 입력, 컨트롤 소스를 그래프의 경계로 취급한다.
- `Input Rail`, `Output Rail`, `Control Rail`이 내부 노드를 대신하는 실제 연결 종단점으로 동작해야 한다.
- preset/state/recovery 흐름에서도 device profile과 control assignment가 안정적으로 복원되어야 한다.

## UI 기본 방향

- 좌측 `Input Rail`, 우측 `Output Rail`, 하단 `Control Rail` 구조를 유지한다.
- 중앙 캔버스는 DSP 그래프 편집의 주 무대다.
- Rail은 시스템 경계, 노드는 내부 처리, 케이블은 둘을 직접 잇는 공통 계층으로 읽혀야 한다.

## 포트 체계

- `SignalShape`
  - `Mono`
  - `Stereo`
  - `Bus`
- `ConnectionPolicy`
  - `Single`
  - `Multi`
- `SignalDomain`
  - `Audio`
  - `Midi`
  - `CV`
  - `Control`

## 포트 실루엣

- `Mono`: 원형 포트
- `Stereo`: 세로 아령형 포트
  - 상단 = `L`
  - 하단 = `R`
  - 중앙 손잡이 = bundle
- `Bus`: 세로 긴 캡슐형 포트
- `Multi`: 별도 모양이 아니라 같은 타입 포트를 여러 개 반복 배치

## 상태 표현 원칙

- 내부
  - `Disconnected`
  - `Connected`
  - `PartiallyConnected`
  - `Full`
  - `Hover`
  - `Selected`
  - `DragSource`
  - `DragTarget`
  - `Focus`
- 외부 링
  - `Missing`
  - `Degraded`
  - `InvalidConfig`
- 렌더 순서
  1. 포트 내부
  2. 포트 테두리
  3. 외부 상태 링

## 케이블 원칙

- `Mono`는 단일 케이블
- `Stereo` / `Bus`는 bundle 케이블을 지원
- bundle 케이블은 개별 채널선이 묶인 것처럼 보여야 한다
- 다발 구간은 얇게 유지한다
- 포트 연결부는 포트 길이와 비슷한 폭으로 맞춘다
- `partial`, `asymmetric` 강조는 약하게 유지한다

## 히트 테스트 규칙

- `Stereo`
  - 상단 원 클릭 = `L`
  - 하단 원 클릭 = `R`
  - 중앙 손잡이 클릭 = bundle
- `Bus`
  - 채널 세그먼트 클릭 = 개별 채널
  - 나머지 바디 클릭 = 전체 bundle

## 자동 채널 변환 규칙

- `Mono -> Stereo`
  - 사용자가 꽂은 쪽 채널에만 입력
- `Stereo -> Mono`
  - downmix
- `Bus -> Stereo/Mono`
  - 사용자가 꽂은 채널만 입력
- 불가능한 경우는 `InvalidConfig`

## Multi / Bus 수용량 규칙

- `Multi`는 동일 타입 포트를 반복해서 배치한다
- `MultiStereo xN`은 `2N` mono channel unit 총량으로 해석한다
- 비대칭 채널 사용은 허용한다
- `Bus`가 6채널을 넘으면 포트 길이를 더 늘리지 않고 세그먼트 두께를 줄인다

## Inspector 구조

- `Connection Setup`
- `Audio Parameters`
- `Missing`, `Degraded`, `InvalidConfig`는 tooltip과 상세 설명으로 보강한다

## 렌더 계층 원칙

1. 캔버스 배경
2. 캔버스 그리드
3. 캔버스 내부 장식 요소
4. 노드 / 프레임 / 캔버스 위젯 본체
5. Rail 패널 본체
6. 연결선
7. 포트 강조 / 드래그 타깃 / 상태 오버레이
8. HUD 오버레이
   - runtime overlay
   - minimap
9. 최상위 UI
   - toast
   - warning banner
   - popup

핵심 관계:
- `Rail 본체 < 연결선 < 포트 상태 오버레이 < HUD < 최상위 UI`

---

## 현재 구현 상태

### 완료

- `Input Rail`, `Output Rail`, `Control Rail` 기본 배치
- Rail 카드 선택과 Inspector 연동
- `Control Rail` source inspector 기본 표시
- `Input Rail` 포트에서 node input으로 직접 드래그 가능
- node output에서 `Output Rail` 포트로 직접 드롭 가능
- `Control Rail` 포트에서 parameter assignment 드래그 가능
- rail endpoint를 backing node가 아닌 직접 연결 endpoint로 승격
- rail endpoint 직렬화 / 역직렬화
- 런타임에서 rail input / rail output을 직접 처리하는 경로 추가
- Node inspector의 `Connection Setup` 요약 표시

### 진행 중

- stereo / bus 포트 실루엣을 최종 문법대로 맞추는 작업
- bundle 케이블 렌더링
- 포트 상태 표현의 전면 적용
- rail 포트 앵커와 실제 연결선 위치 정렬

### 아직 미완료

- `Missing`, `Degraded`, `InvalidConfig`의 전면 처리
- assignment 편집부 확장
- device detect / learn / profile persist / restore
- preset / state / recovery 전체 복구 검증

---

## MVP까지 수정해야 할 사항

### 1. 렌더 순서 바로잡기

- 연결선이 rail 아래로 깔리지 않게 수정
- minimap / runtime overlay / toast / warning의 z-order 정리
- 포트 오버레이가 연결선보다 항상 위에 오게 정리

### 2. stereo / bus 포트 문법 반영

- 현재 가로 `L/R` 배치를 세로 `L/R`로 교체
- stereo 포트의 상단 `L`, 하단 `R`, 중앙 bundle hit test 반영
- bus 포트의 채널 세그먼트와 바디 hit test 반영

### 3. 연결선 앵커 정렬

- rail 포트 중심과 케이블 시작점 / 끝점이 정확히 맞도록 수정
- rail 카드 테두리에서 선이 튀어나오는 것처럼 보이는 문제 제거

### 4. bundle 케이블 구현

- stereo / bus bundle 케이블 실루엣 추가
- 개별선과 bundle 선이 시각적으로 구분되게 정리
- split / merge 지점 문법 정리

### 5. legacy 시각 흔적 제거

- rail이 있는 상태에서 중복 의미의 입력 노드가 캔버스에 보이지 않게 정리
- direct rail endpoint 구조에 맞는 canvas 표현으로 통일

### 6. 포트 상태 적용 확대

- 내부 fill, 테두리, 외부 점선 링 규칙을 rail과 node 포트 전반에 적용
- valid / invalid drag target 강조 문법 통일

### 7. Inspector 마감

- `Connection Setup`과 `Audio Parameters` 분리 정리
- assignment 읽기 / 삭제 / 상태 표시 보강

### 8. 회귀 검증

- 저장 / 불러오기
- rail selection / cable drag / invalid drop
- stereo / bus / multi routing
- asymmetric routing
- preset reload / recovery / degraded state

---

## ???

### ?? ?? ???

1. **Rail ???? ??** `[??]`
   - `Input / Output / Control` ?? ?? ?? ??
   - ?? ?? ? Inspector ??
   - ?? ?? ? ??? ??? ??
   - hover / selected / focus ?? ??

2. **?? ?? / ?? ?? ??** `[??]`
   - `SignalShape`, `ConnectionPolicy`, `SignalDomain` ? ??
   - ?? ?? ?? / ?? ?? ?? ??
   - ?? fill / ?? ? / tooltip ?? ??

3. **Direct rail endpoint ?? ??** `[??]`
   - rail endpoint? backing node? ?? ?? ?? endpoint? ??
   - ?? rail / ?? rail / control rail drag ??? direct endpoint ???? ???
   - ?? bridge node ??? ??

4. **?? ??? ??** `[??]`
   - rail / DSP ?? ?? `Mono + Bus` ?? ??
   - ?? geometry / hit test helper ??
   - `Missing / Degraded / InvalidConfig` issue ring ??
   - rail? DSP? ?? ?? ??? ???? ??? ??? ??? ??

5. **?? hit test / ??? ?? ?? ??** `[??]`
   - mono ???? bundle ??? ??
   - bus ?? ? ??? bundle body ?? ?? ??
   - rail / DSP ?? ?? hover / valid / invalid ?? ??
   - mono? ?? ??, bundle? body? ????? ?? ??

6. **??? ??? ??** `[???]`
   - direct rail endpoint ??? ??? ??
   - bundle underlay / core / highlight ?? ??
   - rail / DSP ?? ?? anchor ?? ??
   - stereo / bundle quick-add insertion ??
   - ?? ?: split / merge ?? ?? ??? ? ??? ???? ?? ??

7. **Bus / Multi ??? ?? ??** `[??]`
   - ??? `maxIncomingConnections`, `maxOutgoingConnections` ????? ??
   - input / output capacity ??? drag preview? ?? ?? ??? ??
   - `Mono Mixer (4-In)`, `Stereo Mixer (4-Bus)` ??
   - `Port Usage`, `Free / Partial / Full`, reject reason hint ??

8. **?? ?? ?? ?? ??** `[??]`
   - `Mono -> Bus`? ?? ???? ??
   - `Bus -> Mono`? ?? ???? ??
   - `Bus -> Bus` bundle ??? ?? ?? ? body-to-body? ??
   - ?? ?? ? bus? ??? ?? ??? ??
   - invalid ??? drag status hint? ?? ??

9. **Inspector 1? ??** `[??]`
   - `Connection Setup`, `Port Usage`, `Connection Rules` ?? ??
   - assignment `Clear / On / Inv / Range` ?? ??
   - issue summary? connection issue ?? ??
   - grouped port usage? channel occupancy ??

10. **Rail UI polish** `[???]`
   - rail, DSP node, inspector, library, HUD, drawers, search overlay? compact?? ??
   - minimap / runtime overlay / notice banner / toolbar footprint ??
   - collapsed rail affordance? `6ch+` dense bus ?? ?? ??
   - ?? ?: ?? ?? ???? ?? ?? ?? ?? ??? ??

11. **Control Source / Device Profile ?? ??** `[?? ?]`
   - ??? ??
     - `Learn armed` ??
     - kind / mode ??? source shape ???
     - learned binding ?? ??
     - source / profile reconcile
     - compatible profile auto-relink
     - bulk profile sync API
     - MIDI device polling
     - live MIDI learn callback ??
   - ?? ??
     - profile persist / reconnect ??
     - MIDI ? ?? ??? ???
     - preset / state / recovery ??
     - degraded / missing recovery ?? ??

12. **??** `[??]`
   - ?? / ????
   - profile persist / reconnect
   - degraded / profile mismatch / recovery
   - routing / inspector / drag-drop ?? ??
   - MIDI learn / profile sync / restore ???? ??

### ?? ?? ??

- ?? ?? ??? `11. Control Source / Device Profile ?? ??`???.
- ?? ?? ??? ???? ???.
  1. MIDI ? ?? ??? learn / profile sync API? ??
  2. profile persist / reconnect ?? ??
  3. preset / state / recovery? control profile ?? ?? ??
  4. ? ?? `12. ??`?? ????

### ?? ?? ??

- `1`?? `9`??? ?? MVP ?? ????? ??????.
- `10`? UI polish ??? ?????.
- ?? ? ??? ??? `11`? `12`???.
- ? ?? ??? `Milestone 3C ???`?? ?? ?? ?? ?????.
