# Ieum 로드맵

`Ieum`은 `Gyeol`의 위젯 출력과 `Teul`의 오디오 파라미터를 연결하는 바인딩 레이어다. 목표는 "위젯을 움직였을 때 어떤 오디오 파라미터가 어떤 규칙으로 반응하는가"를 안정적으로 정의하고, 저장하고, 복구하고, 시각적으로 편집할 수 있게 만드는 것이다.

---

## 프로젝트 목표

- `Gyeol` 위젯 출력을 `Teul` 파라미터에 안정적으로 연결한다.
- 위젯 하나가 여러 파라미터를 제어하거나, 여러 위젯이 하나의 파라미터에 기여하는 구조를 지원한다.
- 바인딩 상태를 문서와 프리셋에 저장하고, 재실행 시 정확히 복원한다.
- `Missing`, `Degraded`, `Invalid` 상태를 UI에서 명확히 보여준다.
- 이후 `Naru`에서 들어오는 펜/제스처 입력도 같은 바인딩 소스로 연결할 수 있게 준비한다.

## 시스템 역할

- `Gyeol`
  - 위젯, 페이지, 장면, 인터랙션을 제공한다.
  - 위젯 값 변화와 제스처 이벤트를 바인딩 소스로 제공한다.
- `Teul`
  - DSP 노드와 파라미터를 보유한다.
  - Ieum이 계산한 최종 값을 실제 파라미터에 적용한다.
- `Ieum`
  - 소스와 타깃을 연결한다.
  - 변환 규칙, 범위, 우선순위, 상태, 저장/복원을 담당한다.

## 핵심 개념

- `Binding Source`
  - `Gyeol` 위젯의 출력 채널 또는 이벤트
  - 예: knob value, slider value, XY pad X/Y, toggle state, sequencer lane, button trigger
- `Binding Target`
  - `Teul` 노드 파라미터
  - 예: gain, mix, cutoff, feedback, pan, bypass, wet/dry, modulation depth
- `Binding`
  - source와 target을 연결하는 단위
- `Binding Mode`
  - `absolute`, `relative`, `toggle`, `trigger`
- `Transform`
  - range remap, invert, curve, smoothing, quantize
- `Binding Group`
  - 하나의 위젯이 여러 파라미터를 제어하거나, 여러 소스를 묶어서 관리하는 단위

## 데이터 모델 초안

### 1. Gyeol 쪽 식별자
- `WidgetId`
- `WidgetType`
- `PageId` 또는 `SceneId`
- `WidgetOutputId`
- `WidgetOutputKind`

### 2. Teul 쪽 식별자
- `NodeId`
- `ParamKey`
- `ParamType`
- `ParamRange`
- `ParamDefault`

### 3. 바인딩 엔트리
- `BindingId`
- `WidgetId`
- `WidgetOutputId`
- `NodeId`
- `ParamKey`
- `BindingMode`
- `RangeMin`
- `RangeMax`
- `DefaultValue`
- `Inverted`
- `Enabled`
- `CurveType`
- `SmoothingMs`
- `QuantizeStep`
- `LastResolvedStatus`
  - `ok`
  - `missing-widget`
  - `missing-target`
  - `degraded`
  - `invalid`

### 4. 문서 저장 상태
- 위젯 구조 변경에 따른 복구 정보
- 프리셋/씬 별 override
- 마지막 정상 resolve 결과와 경고 상태

## 편집 UI 목표

- `Gyeol` 위젯을 선택하면 바인딩 가능한 `Teul` 파라미터를 찾을 수 있어야 한다.
- `Teul` 파라미터를 선택하면 어떤 `Gyeol` 위젯이 연결되어 있는지 보여줘야 한다.
- 바인딩 추가는 drag-and-drop과 inspector 편집 둘 다 지원하는 것이 이상적이다.
- `Missing`, `Degraded`, `Invalid`는 inspector, 목록, badge, tooltip에서 일관되게 보여야 한다.
- 위젯 종류가 많아져도 검색과 필터가 가능해야 한다.

## 개발 원칙

- 런타임 적용 경로와 편집기 표시 경로를 분리한다.
- 바인딩 모델을 먼저 고정하고 UI는 그 위에 얹는다.
- resolve 실패는 조용히 무시하지 않고 상태로 남긴다.
- `Gyeol`, `Teul`, `Ieum` 사이에 직접 참조를 최소화하고 식별자 기반 해석 계층을 둔다.

---

## 구현 순서

### 1단계. 바인딩 모델 고정
목표: 문서, 런타임, UI가 같이 쓸 최소 바인딩 구조를 확정한다.

작업:
- source/target 식별자 구조 정의
- binding entry 필드 확정
- mode, range, invert, enabled, curve, smoothing 정의
- 상태값(`ok`, `missing`, `degraded`, `invalid`) 정의
- 기본 직렬화 포맷 설계

완료 기준:
- 코드 없이도 JSON 예시를 만들 수 있다.
- 위젯 삭제/노드 삭제/파라미터 변경 시 어떤 상태가 되는지 설명 가능하다.

### 2단계. resolve/검증 엔진 구현
목표: 저장된 binding이 현재 문서에서 유효한지 판정한다.

작업:
- widget lookup
- target param lookup
- mode/타입 호환성 검사
- status 계산
- `Missing`, `Degraded`, `Invalid` 판정 로직 추가

완료 기준:
- 문서를 열었을 때 모든 binding에 상태를 붙일 수 있다.
- invalid 원인을 최소 문자열로 설명할 수 있다.

### 3단계. 런타임 적용 엔진 구현
목표: Gyeol 위젯 변화가 실제로 Teul 파라미터를 바꾼다.

작업:
- source event 수집
- binding lookup
- transform 적용
- final value push
- 같은 target에 여러 source가 연결될 때 합성 정책 정의

완료 기준:
- knob, slider, toggle 같은 기본 widget이 Teul 파라미터를 안정적으로 제어한다.
- runtime update가 UI 쓰레드와 오디오 쓰레드를 불안정하게 만들지 않는다.

### 4단계. Ieum 편집기 1차 구현
목표: 바인딩을 만들고 삭제하고 상태를 읽을 수 있는 UI를 만든다.

작업:
- widget -> param binding 생성 UI
- binding 목록 UI
- inspector에서 binding 상세 표시
- binding enable/disable, invert, range 조절
- 상태 badge / tooltip 표시

완료 기준:
- 사용자가 코드 수정 없이 binding을 만들고 수정할 수 있다.
- missing/invalid 상태가 어디서도 숨지 않는다.

### 5단계. 고급 바인딩 규칙 구현
목표: 실사용에 필요한 변환 규칙을 확장한다.

작업:
- relative mode
- trigger/toggle mode
- curve presets
- smoothing
- quantize
- multi-target group
- macro binding

완료 기준:
- 실시간 퍼포먼스용 바인딩과 정밀 조정용 바인딩을 모두 만들 수 있다.

### 6단계. 저장/복구/프리셋 연동
목표: binding이 문서와 프리셋 생명주기를 따라간다.

작업:
- 문서 저장/로드
- autosave/recovery
- preset override 구조 정의
- widget 이름 변경 / 노드 교체 / 파라미터 교체 시 복구 전략 추가

완료 기준:
- 앱 재실행 후 binding이 유지된다.
- 일부 대상을 못 찾는 경우에도 상태가 손실되지 않는다.

### 7단계. Naru 입력 연동 준비
목표: 추후 펜 입력도 같은 binding source로 쓸 수 있게 한다.

작업:
- widget source와 derived source를 같은 인터페이스로 추상화
- Naru source placeholder 추가
- gesture trigger / continuous source 타입 확장

완료 기준:
- Naru에서 들어올 source를 기존 binding 엔진에 바로 꽂을 수 있는 구조가 된다.

### 8단계. 검증 및 회귀 테스트
목표: 바인딩이 장기적으로 깨지지 않게 한다.

작업:
- widget rename / delete
- node replace / param rename
- preset reload
- recovery restore
- missing/degraded/invalid scenario 테스트

완료 기준:
- 대표적인 문서 변경과 복구 상황에서 binding 상태가 예측 가능하게 유지된다.

---

## 마일스톤 정리

### I1. 바인딩 기반 마련
- 1단계 완료
- 2단계 완료
- 3단계 기본 구현

### I2. 편집 가능 상태 도달
- 3단계 완료
- 4단계 완료

### I3. 실사용 기능 확보
- 5단계 완료
- 6단계 완료

### I4. Naru 연동 준비 완료
- 7단계 완료
- 8단계 완료

## 최종 완료 기준

- `Gyeol` 위젯을 `Teul` 파라미터에 안정적으로 바인딩할 수 있다.
- 바인딩 상태를 편집기에서 명확히 볼 수 있다.
- 저장/로드/recovery 후에도 binding이 예측 가능하게 유지된다.
- 이후 `Naru` 입력을 같은 binding 소스로 자연스럽게 붙일 수 있다.
