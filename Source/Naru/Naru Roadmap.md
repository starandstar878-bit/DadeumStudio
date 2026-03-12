# Naru 로드맵

`Naru`는 iPad의 Apple Pencil 입력을 서버를 통해 받아서 `Gyeol`, `Teul`, `Ieum`에서 공통으로 사용할 수 있는 입력으로 정규화하는 프로젝트다. 목표는 "펜 입력을 단순 좌표 스트림이 아니라, 여러 프로젝트에서 재사용 가능한 제어 소스와 제스처 소스로 바꾸는 것"이다.

---

## 프로젝트 목표

- iPad의 펜 입력을 낮은 지연으로 수신한다.
- 입력을 공통 좌표계와 공통 이벤트 모델로 정규화한다.
- `Gyeol`에서는 드로잉/위젯 인터랙션 입력으로 사용한다.
- `Teul`에서는 modulation / automation / performance control source로 사용한다.
- `Ieum`에서는 binding source로 연결할 수 있게 한다.

## 시스템 역할

- `iPad Client`
  - Apple Pencil sample을 수집한다.
  - 위치, pressure, tilt, azimuth, phase를 전송한다.
- `Naru Server / Bridge`
  - 세션 관리
  - 입력 정규화
  - 지연 보정
  - 대상 앱별 분배
- `Gyeol`
  - stroke, brush, widget gesture 입력 소비자
- `Teul`
  - pressure, tilt, speed, gesture-derived control source 소비자
- `Ieum`
  - pen-derived source를 widget/parameter binding source로 사용

## 핵심 개념

- `Pen Session`
  - 하나의 연결/사용 단위
- `Pen Sample`
  - 시간축 위의 단일 입력 샘플
- `Pen Stroke`
  - sample들의 연속 구간
- `Derived Control Source`
  - pressure, speed, tilt, normalized x/y, stroke length, direction
- `Gesture Event`
  - tap, hold, flick, scrub, trigger, region enter/exit

## 데이터 모델 초안

### 1. Raw Sample
- `SessionId`
- `SampleId`
- `Timestamp`
- `X`, `Y`
- `Pressure`
- `TiltX`, `TiltY`
- `Azimuth`
- `Phase`
  - `began`, `moved`, `ended`, `cancelled`

### 2. Normalized Sample
- viewport-normalized `X/Y`
- smoothed pressure
- stroke-relative time
- velocity
- acceleration
- path direction

### 3. Derived Source
- `PositionX`
- `PositionY`
- `Pressure`
- `TiltX`
- `TiltY`
- `Velocity`
- `StrokeProgress`
- `StrokeLength`
- `Direction`

### 4. Gesture Event
- `Tap`
- `DoubleTap`
- `Hold`
- `Flick`
- `Scrub`
- `StrokeStart`
- `StrokeEnd`

## 아키텍처 원칙

- transport, normalization, routing, consumer integration을 분리한다.
- raw input과 derived input을 구분한다.
- 네트워크 지연과 끊김을 상태로 관리한다.
- 입력 손실이 생겨도 세션이 완전히 붕괴하지 않도록 fallback을 둔다.
- 이후 여러 입력 장치가 추가돼도 같은 인터페이스를 재사용할 수 있게 한다.

---

## 구현 순서

### 1단계. 전송 규약 정의
목표: iPad와 서버 사이에 오갈 최소 프로토콜을 확정한다.

작업:
- sample packet 구조 정의
- session start/end/heartbeat 정의
- timestamp 기준 확정
- reconnect 정책 정의
- message versioning 추가

완료 기준:
- wire format 문서가 있다.
- 클라이언트와 서버가 같은 샘플 구조를 본다.

### 2단계. 서버 브리지 기본 구현
목표: iPad 입력을 서버에서 받을 수 있다.

작업:
- transport 선택 및 구현
- session manager 추가
- heartbeat/timeout 처리
- 연결 상태 노출

완료 기준:
- iPad 입력을 서버에서 안정적으로 수신한다.
- 연결 끊김과 재연결이 상태로 보인다.

### 3단계. 입력 정규화 계층 구현
목표: raw sample을 공통 입력 모델로 바꾼다.

작업:
- 좌표 정규화
- pressure smoothing
- tilt normalization
- velocity/acceleration 계산
- stroke segmentation

완료 기준:
- raw input과 normalized input을 모두 확인할 수 있다.
- consumer는 공통 모델만 읽어도 된다.

### 4단계. Gyeol 연동
목표: 펜 입력을 `Gyeol`에서 쓸 수 있게 한다.

작업:
- canvas stroke 입력
- widget interaction bridge
- hover/drag/region hit test 통합
- pressure를 brush 또는 widget modulation에 반영

완료 기준:
- Gyeol에서 펜 입력이 마우스/터치와 별개로 동작한다.

### 5단계. Teul 연동
목표: 펜 입력을 `Teul` control source처럼 사용할 수 있게 한다.

작업:
- pressure/tilt/speed를 control source로 노출
- gesture trigger를 discrete source로 노출
- rail 또는 source inspector에 Naru source 표시

완료 기준:
- Teul 파라미터가 펜 derived source에 반응한다.

### 6단계. Ieum 연동
목표: Naru 입력을 `Ieum` binding source로 통합한다.

작업:
- Naru source 식별자 정의
- Ieum source resolver와 연결
- pen-derived source를 widget source와 같은 추상 계층에 올림

완료 기준:
- pressure, tilt, gesture가 Ieum 바인딩 소스로 선택 가능하다.

### 7단계. 제스처 엔진 구현
목표: 단순 sample 스트림을 고수준 입력으로 변환한다.

작업:
- tap / hold / flick 판정
- scrub / stroke direction 판정
- region enter/exit 판정
- gesture confidence 모델 추가

완료 기준:
- 단순 좌표/pressure 외에도 이벤트형 입력을 제공한다.

### 8단계. 장치/프로필/복구 구현
목표: 입력 장치 특성과 세션 상태를 저장하고 복구한다.

작업:
- device profile
- sensitivity / calibration profile
- reconnect restore
- missing/degraded 상태 표시

완료 기준:
- 연결이 잠깐 끊겨도 세션과 설정을 예측 가능하게 복구한다.

### 9단계. 캘리브레이션 및 사용자 UX 구현
목표: 실제 사용자가 입력 품질을 조정할 수 있게 한다.

작업:
- sensitivity 조절
- dead zone / smoothing 조절
- pen source preview UI
- latency / connection quality 표시

완료 기준:
- 사용자가 환경에 맞게 펜 입력을 보정할 수 있다.

### 10단계. 검증 및 회귀 테스트
목표: 실제 네트워크/장치 조건에서도 안정성을 확보한다.

작업:
- network jitter
- reconnect
- packet drop
- long stroke
- rapid gesture switching
- multi-consumer routing 테스트

완료 기준:
- Gyeol, Teul, Ieum 어느 쪽에 연결해도 입력 의미가 예측 가능하게 유지된다.

---

## 마일스톤 정리

### N1. 입력 수신 기반 마련
- 1단계 완료
- 2단계 완료
- 3단계 기본 구현

### N2. 제품 연동 시작
- 3단계 완료
- 4단계 완료
- 5단계 완료

### N3. 제어 소스 완성도 확보
- 6단계 완료
- 7단계 완료

### N4. 실사용 안정화
- 8단계 완료
- 9단계 완료
- 10단계 완료

## 최종 완료 기준

- iPad Apple Pencil 입력이 안정적으로 수신된다.
- 입력이 공통 정규화 모델로 변환된다.
- `Gyeol`, `Teul`, `Ieum`이 같은 입력을 서로 다른 방식으로 재사용할 수 있다.
- 네트워크 문제나 재연결 상황에서도 상태가 예측 가능하게 유지된다.
