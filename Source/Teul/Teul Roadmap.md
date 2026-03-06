# Teul (틀) — 기능 로드맵
> 역할: 노드 기반 DSP 그래프 에디터. 오디오 신호 흐름을 시각적인 노드 연결(Node Graph)로 설계하고, 런타임에 실제 JUCE DSP 연산으로 실행하며, Ieum을 통해 Gyeol UI 파라미터와 연동한다.

---

> [!TIP]
> **개발 사이클 전략**: 각 Phase가 완료되면 바로 다음 기능 Phase로 넘어가지 말고,
> 아래 **`→ UI 로드맵 전환`** 마커가 표시된 시점에 [Teul UI Roadmap.md](Teul%20UI%20Roadmap.md)의
> 해당 Phase로 전환하여 기능과 UI 수준을 맞춘 뒤 다음 기능 Phase로 진행하세요.

---

## Phase 1: 데이터 모델 및 직렬화 (Foundation)

### 목표
- 노드 그래프의 핵심 데이터 구조(노드, 포트, 연결선)를 정의하고, JSON 기반으로 저장/불러오기가 가능하게 한다.

### 구현 단계

#### 단계 1: 노드 데이터 모델
- [x] **`TNode` 구조체 정의**: `NodeId`, 타입 키(typeKey), 위치(x, y), 파라미터 맵(`std::map<String, var>`) 소유
- [x] **`TPort` 구조체 정의**: 포트 ID, 방향(Input/Output), 데이터 타입(Audio, CV, MIDI, Control), 이름, 소유자 NodeId
- [x] **`TConnection` 구조체 정의**: 고유 ID, 소스 NodeId+PortId, 대상 NodeId+PortId
- [x] **`TGraphDocument` 모델**: `TNode`, `TPort`, `TConnection` 컬렉션 소유하는 최상위 문서 객체

#### 단계 2: 직렬화 / 역직렬화
- [x] **JSON 스키마 확정**: `nodes[]`, `connections[]`, `graph_meta{}` 최상위 키 구조로 문서 규약 정의
- [x] **직렬화 함수 구현**: `TGraphDocument → juce::var` 변환 (`toJson()`)
- [x] **역직렬화 함수 구현**: `juce::var → TGraphDocument` 복구, 버전 불일치 시 Migration fallback 처리
- [x] **파일 저장/불러오기**: `.teul` 확장자 기반 로컬 파일 입출력 연동

#### 단계 3: Undo/Redo 히스토리
- [x] **Command 패턴 기반 히스토리 스택**: `AddNode`, `DeleteNode`, `AddConnection`, `DeleteConnection`, `MoveNode`, `SetParam` Command 객체 정의
- [x] **`TGraphDocument::undo() / redo()`** 인터페이스 구현

---

> **→ UI 로드맵 전환**: `Phase 1` 완료 후 **[UI Phase 1: 그래프 캔버스 쉘](Teul%20UI%20Roadmap.md)** 로 전환하세요.
> 데이터 모델과 직렬화가 갖춰진 지금이 캔버스 기반(Zoom/Pan, 그리드)을 붙이기 가장 적절한 타이밍입니다.

---

## Phase 2: 노드 레지스트리 및 팩토리 (Node Registry)

### 목표
- 내장(Built-in) DSP 노드 타입들을 등록하고, 타입 키로 인스턴스를 생성할 수 있는 팩토리 시스템을 구축한다.

### 구현 단계

#### 단계 1: 노드 디스크립터 시스템
- [x] **`TNodeDescriptor` 구조체**: 타입 키, 표시 이름, 카테고리, 기본 파라미터 스펙, 포트 레이아웃 정의
- [x] **`TNodeRegistry` 클래스**: 디스크립터 맵을 보유하고 `registerNode()`, `descriptorFor()` 인터페이스 제공
- [x] **`makeDefaultNodeRegistry()`**: 기본 내장 노드들을 등록하는 전역 팩토리 함수

#### 단계 2: 내장 DSP 노드 타입 구현

> 🥇 **Core** — 첫 구현 대상 / 🥈 **Advanced** — 기능이 안정된 후 / 🥉 **Optional** — 여유 있을 때

##### 🔊 Source — 신호 생성
- [x] 🥇 **`Oscillator` (VCO)**: 사인/삼각/사각/톱니/펄스 파형, 주파수·진폭·위상 파라미터. UI 파형 미리보기 연동
- [x] 🥇 **`LFO`**: 저주파 오실레이터(0.001Hz~20Hz), CV 포트로 다른 노드 파라미터 변조용
- [x] 🥇 **`NoiseGenerator`**: White / Pink / Brown 노이즈 출력
- [ ] 🥈 **`SamplePlayer`**: `.wav/.aiff` 파일 재생, Loop/One-shot 모드
- [x] 🥇 **`MidiInput`**: 노트/CC/Pitch Bend 데이터를 CV·Gate 포트로 방출
- [ ] 🥈 **`Clock`**: BPM 기반 게이트 펄스 생성, 박자 분할 설정 (1/4, 1/8, 1/16 등)
- [x] 🥇 **`Constant`**: 고정 float 값을 CV 포트로 출력 (바이어스/오프셋용)

##### 🔬 Filter / EQ — 필터링
- [x] 🥇 **`LowPassFilter`**: Biquad / Ladder / SVF 알고리즘 선택, 컷오프·Q·CV 변조 입력
- [x] 🥇 **`HighPassFilter`**: 동상
- [x] 🥇 **`BandPassFilter`**: 동상
- [ ] 🥈 **`NotchFilter`**: 특정 주파수 제거
- [ ] 🥈 **`PeakEQ`**: 중간 주파수 부스트/컷
- [ ] 🥈 **`ShelfEQ`**: Low/High Shelf 필터
- [ ] 🥉 **`CombFilter`**: 딜레이 기반 공명 필터 (플랜저·코러스 빌딩블록)
- [ ] 🥉 **`FormantFilter`**: 보컬 포먼트 시뮬레이션

##### ⚡ Dynamics — 다이나믹스
- [ ] 🥈 **`Compressor`**: Threshold/Ratio/Attack/Release/Knee, 레벨 미터 내장 표시
- [ ] 🥈 **`Limiter`**: 하드/소프트 클리핑, 출력 천장 설정
- [ ] 🥈 **`Gate`**: 임계값 이하 신호 차단, 드럼 노이즈 제거용
- [ ] 🥈 **`EnvelopeFollower`**: 입력 신호 진폭 변화를 CV로 추출해 다른 파라미터 변조

##### 🌊 Modulation — 변조
- [x] 🥇 **`ADSR`**: Attack/Decay/Sustain/Release. UI 커브 미리보기 연동
- [ ] 🥈 **`AR`**: 단순 Attack/Release 엔벨로프
- [ ] 🥈 **`SampleAndHold`**: 트리거 수신 시 현재 값 래치 → 스텝형 CV 생성
- [ ] 🥈 **`Lag`** (Slew Limiter): CV 변화 속도를 제한해 글라이드 표현
- [ ] 🥈 **`RingModulator`**: 두 신호를 곱해 사이드밴드 생성

##### 🏛️ Delay / Reverb — 공간·시간
- [x] 🥇 **`DelayLine`**: 딜레이 시간(ms/박자), Feedback, Dry/Wet Mix
- [ ] 🥈 **`PingPongDelay`**: 좌우 스테레오 교차 딜레이
- [ ] 🥈 **`Chorus`**: 피치 미세 변조 + 딜레이 혼합
- [ ] 🥈 **`Flanger`**: CombFilter + LFO 기반 짧은 딜레이
- [ ] 🥈 **`Phaser`**: All-pass 필터 체인 기반 위상 변조
- [x] 🥇 **`Reverb`**: Freeverb 알고리즘, Room Size/Damping/Width/Dry-Wet

##### 🔥 Distortion / Waveshaping — 왜곡
- [ ] 🥈 **`Waveshaper`**: Tanh / Soft Clip / Hard Clip / 커스텀 커브 선택
- [ ] 🥉 **`BitCrusher`**: 비트 깊이·샘플레이트 다운샘플링
- [ ] 🥉 **`FrequencyShifter`**: 피치 유지, 주파수 스펙트럼 이동 (Hilbert transform 기반)

##### 🎚️ Mixer / Routing — 믹싱·라우팅
- [x] 🥇 **`VCA`** (Gain): 볼륨 조절, CV 변조입력 지원
- [x] 🥇 **`StereoPanner`**: 좌/우 패닝, CV 변조 가능
- [x] 🥇 **`Mixer2`** / **`Mixer4`** / **`Mixer8`**: 다채널 오디오 믹서
- [ ] 🥈 **`Crossfader`**: A/B 두 신호를 비율 블렌딩
- [ ] 🥈 **`StereoSplitter`** / **`StereoMerger`**: Stereo↔L/R 모노 분리·병합
- [ ] 🥈 **`Send`** / **`Return`**: 그래프 내 글로벌 버스 라우팅
- [x] 🥇 **`AudioOutput`**: 최종 오디오 신호 버스 연결 (필수 출력 노드)

##### 🔢 Math / Logic — 수학·논리
- [x] 🥇 **`Add`** / **`Subtract`** / **`Multiply`** / **`Divide`**: CV 신호 기초 사칙연산
- [x] 🥇 **`Clamp`**: 신호를 Min~Max 범위로 제한
- [x] 🥇 **`ValueMap`**: 입력 범위 → 출력 범위 선형 변환
- [ ] 🥈 **`Quantizer`**: CV 신호를 음계·코드에 맞는 값으로 스냅
- [ ] 🥈 **`Comparator`**: 두 신호 비교 → Gate 신호 출력
- [ ] 🥈 **`Abs`** / **`Min`** / **`Max`**: 절댓값, 최소·최대값 선택

##### 🎹 MIDI Processing — MIDI 처리
- [x] 🥇 **`MidiToCV`**: 노트 번호→Hz 주파수, Velocity→진폭 CV 변환
- [ ] 🥈 **`MidiCCToValue`**: CC 번호를 float CV 신호로 변환
- [ ] 🥉 **`Arpeggiator`**: 코드를 받아 아르페지오 순서로 노트 출력
- [ ] 🥉 **`Chord`**: 단일 노트 입력 → 코드 보이싱 확장
- [ ] 🥉 **`MidiFilter`**: 채널/노트 범위 필터링
- [x] 🥇 **`MidiOutput`**: 그래프 결과를 외부 MIDI 포트로 출력

##### 📊 Analysis / Visualization — 분석 시각화
- [ ] 🥈 **`Oscilloscope`**: 오디오 신호 파형을 노드 바디 내 실시간 렌더링
- [ ] 🥉 **`SpectrumAnalyzer`**: FFT 기반 주파수 스펙트럼 시각화
- [ ] 🥈 **`LevelMeter`**: RMS/Peak 레벨 미터 (VU 스타일)

#### 단계 3: 외부 노드 플러그인 연동 (Phase 7 연계)
- [ ] **`ITNode` 인터페이스 분리**: 외부 DLL이 구현해야 하는 순수 가상 클래스 기반 노드 ABI
- [ ] **동적 노드 로드**: `TNodeRegistry`에 외부 플러그인 노드를 런타임에 동적 주입 허용

---

> **→ UI 로드맵 전환**: `Phase 2` 완료 후 **[UI Phase 2: 노드 컴포넌트 렌더링 + UI Phase 3: 연결선 렌더링](Teul%20UI%20Roadmap.md)** 으로 전환하세요.
> 노드 디스크립터와 포트 정보가 확정된 지금, 노드 바디·포트 색상 시스템·베지어 케이블을 붙이기에 최적입니다.

---

## Phase 3: 오디오 런타임 엔진 (DSP Runtime)

### 목표
- 그래프를 위상 정렬(Topological Sort)하여 올바른 순서로 DSP 처리하고, JUCE 오디오 스레드와 안전하게 연동한다.

### 구현 단계

#### 단계 1: 그래프 평가기 (Graph Evaluator)
- [x] **위상 정렬(Topological Sort)**: 연결을 따라 순환 참조(Cycle) 감지 후, 안전한 처리 순서 배열 계산
- [x] **오디오 버퍼 라우팅**: 각 연결(Connection)마다 `juce::AudioBuffer<float>` 중간 버퍼 할당 및 포트 간 데이터 전달
- [x] **`TGraphRuntime::process(AudioBuffer, MidiBuffer, sampleRate)`**: 정렬된 순서로 노드들의 `processSamples()` 순차 호출

#### 단계 2: 오디오 스레드 안전성
- [ ] **락-프리 파라미터 교환**: UI 스레드에서 파라미터 값 변경 시, `std::atomic` 또는 FIFO 큐로 오디오 스레드에 락 없이 전달
- [ ] **그래프 구조 변경 시 재평가**: 노드/연결 추가·삭제 시 오디오 스레드가 다음 블록 이전에 안전하게 새 그래프를 참조하도록 Double-Buffer 스왑
- [ ] **JUCE `AudioProcessor` 래퍼**: `TGraphRuntime`을 `juce::AudioProcessor` 서브클래스로 래핑하여 DAW 플러그인(VST/AU) 또는 독립 실행형 앱으로 모두 실행 가능하게 구성

#### 단계 3: MIDI 처리 경로
- [ ] **MIDI 입력 노드**: `MidiInput` → 노트/CC 데이터를 포트로 방출
- [ ] **MIDI-to-CV 변환 노드**: 노트 번호 → 주파수(Hz), Velocity → 진폭 변환
- [ ] **MIDI Output 노드**: 그래프 결과를 외부 MIDI 로 출력

---

> **→ UI 로드맵 전환**: `Phase 3` 완료 후 **[UI Phase 4: UX 인터랙션 및 생산성](Teul%20UI%20Roadmap.md)** 으로 전환하세요.
> 런타임 엔진이 돌아가는 지금이 Quick Add, 마키 선택, Context Menu 등 인터랙션 레이어를 실제 데이터와 연결해 테스트하기 가장 좋은 시점입니다.

---

## Phase 4: Ieum 파라미터 브리지 연동 (Ieum Interface)

### 목표
- Teul의 DSP 노드 파라미터를 Ieum이 읽고 Gyeol UI 위젯과 연결할 수 있도록 파라미터 표면(Parameter Surface)을 노출한다.

### 구현 단계

- [ ] **`ITeulParamProvider` 인터페이스 정의**: `listExposedParams()`, `getParam(id)`, `setParam(id, value)` 순수 가상 인터페이스
- [ ] **`TNodeRegistry` → `ITeulParamProvider` 구현체 등록**: 각 노드가 자신의 파라미터를 APVTS와 유사한 형태로 Ieum에 노출
- [ ] **변경 알림(Listener) 브로드캐스트**: 런타임에 파라미터 값이 오디오 스레드에서 변경되면, Ieum의 리스너로 비동기 전파

---

> **→ UI 로드맵 전환**: `Phase 4` 완료 후 **[UI Phase 5: 파라미터 편집 패널](Teul%20UI%20Roadmap.md)** 로 전환하세요.
> `ITeulParamProvider` 인터페이스가 완성되었으므로, Property Panel의 Ieum 연동 표시(🔗)와 실시간 값 피드백을 실제 데이터로 구동할 수 있습니다.

---

## Phase 5: Export (C++ 코드 생성)

### 목표
- 현재 그래프 상태를 독립적으로 컴파일 가능한 JUCE C++ 코드로 내보낸다.

### 구현 단계

- [ ] **정적 그래프 코드 제너레이터**: 위상 정렬된 노드 순서대로 `processSamples()` 호출 체인을 인라인 C++ 코드로 생성
- [ ] **파라미터 초기화 코드**: 각 노드의 기본 파라미터 값을 생성자에서 초기화하는 코드 자동 삽입
- [ ] **APVTS 바인딩 코드 생성**: Ieum이 정의한 노출 파라미터를 `AudioProcessorValueTreeState`의 항목으로 자동 등록
- [ ] **Jucer/CMake 업데이트**: 생성된 `.h/.cpp` 파일을 Jucer 프로젝트에 자동 추가하는 스니펫 주입

---

## 완료 기준 (DoD)
- [ ] `.teul` 파일 저장/불러오기 후 노드·연결 완벽 복구
- [ ] 기본 내장 노드 10종 이상 동작 확인
- [ ] 오디오 콜백 스레드에서 글리치(Glitch) 없이 128샘플 블록 처리
- [ ] Undo/Redo 20회 연속 후 그래프 상태 이상 없음
- [ ] Export 생성 코드 단독 컴파일 통과
