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
- [x] **락-프리 파라미터 교환**: UI 스레드에서 파라미터 값 변경 시, `std::atomic` 또는 FIFO 큐로 오디오 스레드에 락 없이 전달
- [x] **그래프 구조 변경 시 재평가**: 노드/연결 추가·삭제 시 오디오 스레드가 다음 블록 이전에 안전하게 새 그래프를 참조하도록 Double-Buffer 스왑
- [x] **JUCE `AudioProcessor` 래퍼**: `TGraphRuntime`을 감싸는 `TGraphProcessor`를 구현하여 플러그인 호환 및 오디오 앱 연동을 지원

#### 단계 3: MIDI 처리 경로
- [x] **MIDI 입력 노드**: `MidiInput` → 노트/CC 데이터를 포트로 방출
- [x] **MIDI-to-CV 변환 노드**: 노트 번호 → 주파수(Hz), Velocity → 진폭 변환
- [x] **MIDI Output 노드**: 그래프 결과를 외부 MIDI 로 출력

---

> **→ UI 로드맵 전환**: `Phase 3` 완료 후 **[UI Phase 4: UX 인터랙션 및 생산성](Teul%20UI%20Roadmap.md)** 으로 전환하세요.
> 런타임 엔진이 돌아가는 지금이 Quick Add, 마키 선택, Context Menu 등 인터랙션 레이어를 실제 데이터와 연결해 테스트하기 가장 좋은 시점입니다.

---

## Phase 4: Ieum 파라미터 브리지 연동 (Ieum Interface)

### 목표
- Teul의 DSP 노드 파라미터를 Ieum이 읽고 Gyeol UI 위젯과 연결할 수 있도록 파라미터 표면(Parameter Surface)을 노출한다.

### 구현 단계

- [x] **`ITeulParamProvider` 인터페이스 정의**: `listExposedParams()`, `getParam(id)`, `setParam(id, value)` 순수 가상 인터페이스
- [x] **`TNodeRegistry` → `ITeulParamProvider` 구현체 등록**: 각 노드가 자신의 파라미터를 APVTS와 유사한 형태로 Ieum에 노출
- [x] **변경 알림(Listener) 브로드캐스트**: 런타임에 파라미터 값이 오디오 스레드에서 변경되면, Ieum의 리스너로 비동기 전파

### 현재 인프라 메모 (Ieum 구현 참고)
- **구현 위치**: `ITeulParamProvider`는 `Source/Teul/Bridge/ITeulParamProvider.h`, 실제 구현체는 `TGraphRuntime`, 파라미터 표면 생성은 `TNodeRegistry`에 있음
- **파라미터 식별자 규칙**: `paramId = teul.node.<nodeId>.<paramKey>` 형태를 사용. Ieum 쪽 바인딩 키는 `node label`이 아니라 반드시 `paramId` 기준으로 잡을 것
- **노출 메타데이터**: `listExposedParams()`는 `nodeId`, `nodeTypeKey`, `nodeDisplayName`, `paramKey`, `paramLabel`, `defaultValue`, `currentValue`를 반환
- **확장 브리지 메타데이터**: `range`, `step`, `unitLabel`, `enumOptions`, `preferredWidget`, 읽기 전용/자동화/모듈레이션 플래그, Ieum 노출 힌트를 각 exposed param과 함께 전달
- **표면 재구성 시점**: `TGraphRuntime::buildGraph()`가 성공하면 현재 그래프 문서 기준으로 파라미터 표면을 다시 만들고 `teulParamSurfaceChanged()`를 보냄
- **값 변경 흐름**: `setParam()` 호출 시 런타임 표면 값을 갱신한 뒤 오디오 스레드 파라미터 큐에 넣고, 적용 완료 후 `teulParamValueChanged()`가 비동기로 전달됨
- **Ieum 쪽 권장 사용 방식**: 패널/위젯은 최초 진입 시 `listExposedParams()`로 전체 목록을 받고, 이후 `Listener` 알림으로 갱신. 그래프 재빌드 후에는 기존 캐시를 버리고 다시 조회할 것
- **프로세서 접근 경로**: 플러그인/앱 쪽에서는 `TGraphProcessor::paramProvider()`로 브리지에 접근 가능
- **현재 한계 1**: 이 표면은 런타임 스냅샷 기준이며, Ieum이 값을 바꿨을 때 편집 중인 원본 문서까지 자동 역반영하지는 않음
- **현재 한계 2**: 노드 내부 DSP 로직이 스스로 값을 바꾸는 경우, 해당 노드가 `TProcessContext::paramValueReporter`를 호출해야 Ieum 리스너까지 변경이 전달됨. 현재는 브리지 경로만 준비된 상태
- **현재 한계 3**: `AudioProcessorValueTreeState` 자동 등록/바인딩 생성은 아직 없음. 이는 `Phase 5`의 `APVTS 바인딩 코드 생성`에서 이어서 구현

## Phase 4.5: 확장 파라미터 메타데이터 (Property Schema)

### 목표
- UI Phase 5의 Property Panel, Ieum 바인딩 표시, Export/APVTS 생성이 공통으로 참조할 수 있는 풍부한 파라미터 스키마를 정의한다.

### 구현 단계

- [x] **파라미터 스키마 골격 확장**: `TParamSpec`, `TParamOptionSpec`, `valueType`, `preferredWidget`, `enumOptions`, 수치 범위/단위/설명/카테고리 필드를 추가하고 기본 정규화 경로를 마련
- [x] **레거시 노드 호환 정규화**: 기존 `key`, `label`, `defaultValue` 3필드만 가진 노드도 `TNodeRegistry` 등록 시 타입/위젯/기본 바인딩 키를 자동 추론하도록 정리
- [x] **대표 노드 1종 시범 등록**: `Oscillator`에 `enum + float` 메타데이터를 적용해 확장 스키마가 실제 노드 등록에 쓰이도록 검증
- [x] **Property Panel 소비자 구현**: `preferredWidget`, `enumOptions`, `min/max/step`, `unitLabel`, `displayPrecision`을 이용해 우측 패널이 `Slider`, `ComboBox`, `Toggle`, `TextEditor`를 자동 생성하도록 연결
- [x] **Ieum 브리지 메타데이터 확장**: `ITeulParamProvider`/`TTeulExposedParam`이 위젯 힌트, 범위, 옵션 목록 등 UI 생성에 필요한 메타데이터를 함께 전달하도록 확장
- [x] **문서/런타임 동기화 정책 확정**: Property Panel, 런타임 파라미터 큐, Ieum 변경이 동시에 있을 때 어떤 값이 source of truth 인지 정의하고 적용
- [x] **실시간 값 피드백 연결**: Run 모드에서 런타임 `currentValue`를 안전한 UI 타이머 또는 리스너 경로로 반영하도록 연결
- [x] **대표 노드 확장 적용 및 검증**: `LFO`, `VCA`, `StereoPanner`, `LowPassFilter`, `ADSR` 등에 새 스키마를 적용해 Property Panel과 Ieum이 다양한 타입을 실제로 처리하는지 검증

### 현재 적용된 동기화 정책
- **source of truth**: 편집 중 상태의 기준은 `TGraphDocument`다.
- **Property Panel 적용 순서**: 문서에 먼저 반영하고, Ieum 노출 파라미터만 `ITeulParamProvider::setParam()`으로 런타임에 즉시 미러링한다.
- **런타임 재동기화**: `runtimeRevision`이 바뀌면 `TGraphRuntime::buildGraph()`를 다시 호출해 문서 스냅샷 기준으로 런타임 표면을 재구성한다.
- **실시간 값 표시**: `ITeulParamProvider::Listener`와 UI 타이머를 함께 사용해 `currentValue`를 안전하게 반영한다.
- **현재 한계**: 외부 Ieum 변경을 편집 중 원본 문서까지 자동 역반영하는 경로는 아직 없다.

### 확장된 속성 리스트 (초안)

- `key`: 문서/런타임 내부에서 사용하는 고정 파라미터 키
- `label`: UI에 표시할 이름
- `valueType`: `float`, `int`, `bool`, `enum`, `string`
- `defaultValue`: 기본값
- `minValue`, `maxValue`: 수치형 파라미터 범위
- `step`: 슬라이더/증감 단위
- `unitLabel`: `Hz`, `dB`, `%`, `ms` 같은 단위 문자열
- `displayPrecision`: 소수점 표시 자릿수
- `group`: Property Panel 섹션 그룹명
- `description`: 툴팁/도움말에 쓸 설명
- `enumOptions[]`: Enum 파라미터 선택지 목록
- `preferredWidget`: UI 기본 위젯 타입
- `showInNodeBody`: 노드 바디에 간략 노출할지 여부
- `showInPropertyPanel`: 우측 패널에 표시할지 여부
- `isReadOnly`: 읽기 전용 여부
- `isAutomatable`: APVTS/호스트 자동화 대상 여부
- `isModulatable`: CV 또는 내부 모듈레이션 대상 여부
- `isDiscrete`: 이산값 파라미터 여부
- `exposeToIeum`: Ieum 파라미터 표면에 노출할지 여부
- `preferredBindingKey`: Gyeol/Ieum 바인딩 시 기본 제안 키
- `exportSymbol`: C++ Export 시 사용할 안전한 식별자 이름
- `categoryPath`: `Oscillator/Pitch`, `Filter/Cutoff` 같은 계층 분류

---

> **→ UI 로드맵 전환**: `Phase 4.5` 완료 후 **[UI Phase 5: 파라미터 편집 패널](Teul%20UI%20Roadmap.md)** 로 전환하세요.
> 확장된 파라미터 메타데이터가 확정되어야 Property Panel의 위젯 자동 생성, Ieum 바인딩 표시, 실시간 값 피드백을 일관된 규칙으로 구현할 수 있습니다.

---

## Phase 5: Export Foundation & Diagnostics

### 목표
- `EditableGraph Export`와 `RuntimeModule Export`를 같은 export 파이프라인 위에서 구현한다.
- export 전에 실패 사유를 명확히 진단하고, 생성 가능한 그래프만 안정적으로 내보낸다.

### 구현 단계 (우선 구현 순위)

- [x] **1. Export 모드 분리 + 공용 API 정의**: `EditableGraph`와 `RuntimeModule` 두 export 모드를 하나의 진입 API 아래에서 분기하도록 정리
- [x] **2. 노드별 export 지원 속성 추가**: `TNodeDescriptor`에 `Unsupported / JsonOnly / RuntimeModuleOnly / Both` 메타데이터를 추가하고 기본 정책 확정
- [x] **3. Export preflight validator**: 지원 불가 노드, 비호환 연결, 포트 타입 불일치, 채널 구성 문제, 순환 의존성을 export 전에 차단
- [x] **4. 노드/연결 단위 diagnostics 모델**: 어떤 노드와 연결이 왜 문제인지 위치 수준으로 식별 가능한 진단 메시지 구조 마련
- [x] **5. 공통 Export IR 구축**: `TGraphDocument`를 export 전용 중간 모델로 정규화하여 JSON export와 C++ codegen이 같은 그래프/파라미터/메타데이터를 공유
- [ ] **6. 자산/의존성 preflight**: impulse, wavetable, 외부 파일 참조가 export 패키지에 포함 가능한지 검사하고 manifest 입력으로 정리
- [ ] **7. EditableGraph JSON export/import 정리**: 버전, 노드 메타데이터, 연결, 파라미터, 자산 참조를 포함한 `.teul` 패키지 round-trip 경로 정리
- [ ] **8. RuntimeModule 클래스 스켈레톤 생성기**: 그래프를 블랙박스 DSP 클래스로 생성하고 `prepare`, `reset`, `process`, `setParam`, `getParam` API 뼈대 제공
- [ ] **9. 파라미터 표면/APVTS 생성**: `exposeToIeum`, `isAutomatable`, `enumOptions`, `range`, `step` 메타데이터를 이용해 APVTS layout 및 파라미터 접근 코드 생성
- [ ] **10. `paramId` 정책 확정 및 내부 인덱스 매핑**: 외부 공개 식별자는 기존 `paramId = teul.node.<nodeId>.<paramKey>`를 유지하고, 내부는 `paramId -> index/handle`로 최적화
- [ ] **11. 그래프 스케줄/버퍼 플래너 생성**: 위상 정렬, 임시 버퍼 계획, 처리 순서를 export IR에서 고정해 RuntimeModule codegen의 기반으로 사용
- [ ] **12. 최적화된 DSP codegen 패스**: dead path 제거, 상수 파라미터 상수화, 불필요한 dispatch 제거, 임시 버퍼 재사용 등 export 전용 최적화 도입
- [x] **13. dry-run export report**: 노출 파라미터 수, 예상 버퍼 수, 생성 파일 목록, 제한 사항을 요약 보고서로 출력
- [ ] **14. 프로젝트 편입 자동화**: 생성된 `.h/.cpp`를 Jucer/CMake 또는 현재 앱 빌드 경로에 안정적으로 포함시키는 통합 경로 제공

---

## Phase 6: Runtime Hardening & Real-time Safety

### 목표
- 런타임 그래프와 generated module이 긴 세션과 다양한 입력 조건에서도 오디오 스레드 안전성을 유지하게 한다.

### 구현 단계

- [ ] **audio thread 무할당/무락 보장**: process 경로에서 동적 할당, lock, string lookup, file IO 제거
- [ ] **파라미터 smoothing / bypass / denormal 처리**: zipper noise, bypass pop, denormal slowdown을 막는 공통 규약 정리
- [ ] **sampleRate / blockSize / channel layout 적응성**: mono/stereo/multi-channel, 작은 블록/큰 블록, sample rate 변화에 대한 prepare 정책 확정
- [ ] **runtime rebuild / state handoff 안전화**: UI 편집 중 그래프 재빌드와 런타임 상태 전환 충돌 방지
- [ ] **성능 budget 계측기**: CPU, 메모리, 임시 버퍼, rebuild cost를 측정하는 내부 디버그 카운터 추가

---

## Phase 7: Verification & Benchmark Infrastructure

### 목표
- 런타임 그래프와 export된 module의 결과 일치성과 성능 회귀를 자동으로 검출한다.

### 구현 단계

- [ ] **golden audio test**: 대표 그래프의 입력/출력 오디오 스냅샷을 저장하고 회귀 테스트 수행
- [ ] **runtime vs export parity test**: 같은 문서에서 런타임 처리 결과와 `RuntimeModule Export` 결과를 tolerance 기반으로 비교
- [ ] **export compile CI**: generated `.h/.cpp`가 headless CI에서 자동 컴파일되는 파이프라인 추가
- [ ] **stress / fuzz / soak test**: 랜덤 그래프, 긴 세션, 빠른 자동화, 반복 export/import를 장시간 검증
- [ ] **benchmark regression gate**: 대표 그래프 CPU 사용량, rebuild 시간, export 시간 상한선을 두고 회귀를 차단

---

## Phase 8: Preset, State & Compatibility

### 목표
- 버전이 달라져도 프로젝트, 프리셋, export 결과가 가능한 한 안정적으로 유지되게 한다.

### 구현 단계

- [ ] **preset/state 포맷 정의**: `paramId`, 그래프 메타데이터, 노드별 상태를 안정적으로 저장하는 preset/schema 설계
- [ ] **schema migration 체계**: 구버전 `.teul`과 preset을 최신 구조로 올리는 migration 경로 추가
- [ ] **호환성 alias 정책**: 노드 타입 이름 변경, 파라미터 키 변경, deprecated 노드 제거 시 대체 경로 제공
- [ ] **autosave / crash recovery**: 비정상 종료 후 최근 작업을 복구하는 자동 저장/복원 경로 도입
- [ ] **compatibility test matrix**: 예전 문서, 프리셋, 생성 코드를 새 버전에서 다시 여는 회귀 테스트 구축

---

## Phase 9: Asset & Dependency Packaging

### 목표
- 외부 자산과 의존성이 있는 그래프도 export/import/package 과정에서 손실 없이 이동되게 한다.

### 구현 단계

- [ ] **asset manifest**: 샘플, impulse response, wavetable, 외부 파일 참조를 한 곳에 수집하는 manifest 생성
- [ ] **bundle / relative path 정책**: 상대 경로, 복사 포함, 외부 참조 유지 중 어떤 방식을 쓸지 모드별 정책 확정
- [ ] **missing asset relink flow**: 파일 누락 시 재연결 경로를 안내하고 복구 가능한 UI/로그 제공
- [ ] **RuntimeModule package layout**: generated code, asset folder, README, build snippet을 함께 내보내는 패키지 구조 정의
- [ ] **deterministic packaging**: 같은 입력 그래프에서 같은 산출물이 나오도록 패키지 정렬/해시 규칙 정리

---

## Phase 10: Product Integration & Host Templates

### 목표
- export 결과를 실제 JUCE 제품, 플러그인, 앱 프로젝트에 바로 가져다 쓸 수 있는 수준으로 만든다.

### 구현 단계

- [ ] **host template 제공**: `AudioProcessor`, standalone app, 테스트 호스트 등 통합 예제 템플릿 제공
- [ ] **APVTS attachment 예제**: 슬라이더/버튼과 generated param surface를 연결하는 샘플 코드 제공
- [ ] **plugin wrapper 가이드**: VST3/AU/Standalone별 포함 경로, prepare/process 연결 방식 문서화
- [ ] **latency / tail / bus metadata export**: 호스트가 요구하는 메타데이터를 generated module과 함께 제공
- [ ] **integration smoke project**: 최소 샘플 프로젝트를 실제로 빌드해 가져다 쓰는 검증 경로 확보

---

## Phase 11: Tooling, CLI & Team Workflow

### 목표
- 팀 단위 개발과 CI 환경에서도 Teul export가 안정적으로 운영되게 한다.

### 구현 단계

- [ ] **headless CLI**: `validate`, `export-json`, `export-module`, `benchmark` 명령을 제공하는 CLI 추가
- [ ] **deterministic codegen**: 같은 그래프에서 diff-friendly한 동일 코드가 생성되도록 정렬/이름 규칙 고정
- [ ] **machine-readable report**: CI가 읽을 수 있는 JSON validation/export 결과 포맷 제공
- [ ] **debug trace / inspector**: 그래프 실행 순서, 파라미터 변경, export 경고를 추적할 디버그 도구 마련
- [ ] **sample project / cookbook**: 팀원이 바로 참고할 수 있는 예제 그래프와 통합 레시피 정리

---

## Phase 12: Commercial Release Readiness

### 목표
- 상용 릴리스 이후의 배포, 지원, 유지보수까지 감당 가능한 제품 수준으로 마무리한다.

### 구현 단계

- [ ] **release QA matrix**: 지원 플랫폼, sample rate, block size, channel layout, 플러그인 포맷별 검증 표준 수립
- [ ] **support bundle / crash log**: 문서, export 로그, 버전 정보, 크래시 정보를 묶어 수집하는 경로 제공
- [ ] **licensing / edition 정책**: 상용 제품화 시 기능 제한, 라이선스, 업그레이드 경로를 반영할 제품 정책 정리
- [ ] **문서 / 튜토리얼 / 온보딩**: 첫 그래프 제작부터 export 통합까지 이어지는 사용자 문서와 샘플 콘텐츠 준비
- [ ] **release checklist / LTS 정책**: 릴리스 전 체크리스트, 핫픽스 기준, 장기 지원 브랜치 운영 기준 정리

---

## 완료 기준 (DoD)
- [ ] `EditableGraph Export` 후 다시 import했을 때 노드, 연결, 파라미터, 메타데이터가 손실 없이 복구
- [ ] 기본 내장 노드 10종 이상 동작 확인
- [ ] 오디오 콜백 스레드에서 글리치(Glitch) 없이 128샘플 블록 처리
- [ ] Undo/Redo 20회 연속 후 그래프 상태 이상 없음
- [ ] `RuntimeModule Export` 생성 코드가 단독 컴파일 통과하고 `process` API로 오디오 버퍼 처리 가능
- [ ] export preflight가 지원 불가 노드, 자산, 채널 문제를 명확한 진단 메시지로 반환
- [ ] 런타임 그래프와 export module의 대표 그래프 출력이 회귀 테스트 기준 내에서 일치
- [ ] 오디오 스레드에서 동적 할당/락 없이 장시간 스트레스 테스트 통과
- [ ] preset, `.teul`, generated module 호환성 회귀 테스트 통과
- [ ] 샘플 host project와 CLI export/validate 경로가 CI에서 자동 검증됨
