# Teul (틀) — 통합 실행 순서
> 목적: [Teul Roadmap.md](Teul%20Roadmap.md)와 [Teul UI Roadmap.md](Teul%20UI%20Roadmap.md)의 남은 단계를 실제 구현 순서로 재배열한 실행 계획.

---

## 현재 상태 요약

- 기능 로드맵은 `Phase 7: Verification & Benchmark Infrastructure` core까지 완료됨
- UI 로드맵은 `Phase 7: 검증/진단 워크스페이스` core까지 완료됨
- `마일스톤 2`까지 마감되어 verification runner, diagnostics drawer, compare/timeline/action bar가 반영됨
- 다음 메인 작업은 `마일스톤 3: Preset / State / Recovery`와 `UI Phase 8` 범위임
- 남은 확장 항목은 deferred verification, deferred diagnostics, external control/device profile UX 중심으로 정리됨

---

## 우선순위 원칙

1. **보기 좋은 것보다 먼저 안전해야 한다**
   export가 가능해진 지금부터는 오디오 스레드 안정성, 검증 가능성, 복구 가능성이 UI보다 우선이다.

2. **검증 없이 다음 단계로 가지 않는다**
   `RuntimeModule Export`가 생겼기 때문에, 이후 단계는 전부 parity test/compile test/soak test를 전제로 쌓는다.

3. **노드 백로그는 상용 경로에 필요한 최소셋만 먼저 추가한다**
   남아 있는 모든 노드를 한 번에 구현하지 않는다. 패키징, 검증, 데모, 호스트 통합에 직접 필요한 노드만 먼저 올린다.

4. **기능 Phase와 UI Phase를 번갈아 닫는다**
   기능 구현으로 기반을 만들고, 바로 대응 UI Phase를 닫아 사용자 경험까지 맞춘 뒤 다음 기능 Phase로 넘어간다.

---

## 권장 구현 순서

### 마일스톤 0: UI 잔여 1차 마감

**먼저 처리할 것**
- UI 잔여 프리뷰 3개
  - 오실레이터 파형 프리뷰
  - ADSR 커브 프리뷰
  - 레벨 미터 인라인 표시

**왜 먼저 하나**
- 이 세 항목은 기존 UI Phase 2의 미완료 잔여분이다.
- 뒤에서 만들 `Runtime Safety UX`의 라이브 프로브/상태 가시화와 방향이 같아서, 지금 정리해 두면 이후 HUD/heatmap 설계가 쉬워진다.

**완료 기준**
- 노드 바디 안에서 미니 프리뷰가 안정적으로 갱신됨
- 편집 성능과 가독성이 기존 Phase 2 스타일과 충돌하지 않음

---

### 마일스톤 1: Runtime Safety 코어 [DONE]

**기능 먼저**
- 기능 `Phase 6`
  - audio thread 무할당/무락 보장
  - parameter smoothing / bypass / denormal 처리
  - sampleRate / blockSize / channel layout 적응성
  - runtime rebuild / state handoff 안전화
  - 성능 budget 계측기

**바로 이어서 UI**
- UI `Phase 6`
  - 세션 상태 바
  - 오디오 상태 배지
  - 실행 모드 전환 피드백
  - deferred apply 배너
  - smoothing 시각화
  - 위험 동작 가드
  - 노드 비용 heatmap
  - 버퍼/스케줄 오버레이
  - 라이브 프로브

**이 단계가 중요한 이유**
- 이후의 검증, preset, packaging, host integration은 전부 “실행 중인 런타임이 안전하다”는 전제 위에서만 의미가 있다.
- 여기서 기반이 흔들리면 뒤의 테스트도 신뢰할 수 없다.

**게이트**
- 대표 그래프가 128-sample block에서 글리치 없이 동작
- process 경로에서 동적 할당/락/파일 IO가 제거됨
- 런타임 상태를 UI에서 즉시 관찰 가능

---

### 마일스톤 1.5: Runtime UX 가독성 보정 [DONE]

**UI 보정 먼저**
- `Phase 6` UI polish
  - heatmap / live probe / overlay 진입 버튼, command palette 항목, 활성 상태 표시 추가
  - 상단 런타임 HUD 정보 위계 재정렬
  - 1차 상태(sample rate / block size / CPU / I/O)와 2차 상태(gen / nodes / buffers / process) 시각 분리
  - 오디오 상태 배지, deferred apply, 위험 경고의 강조 규칙 통일
  - 좌측 Node Library, 중앙 캔버스, 우측 Inspector의 기본 폭 밸런스 재조정
  - Run 상태에서 캔버스 집중도를 해치지 않도록 패널 밀도와 기본 노출량 정리
  - Inspector에서 편집 컨트롤, 런타임 메타 정보, 진단 정보를 섹션 단위로 분리
  - 토스트/배너의 대비, 크기, 지속 시간 규칙 보정
  - Mini-map / zoom HUD / 오버레이 진입점의 시각 완성도 정리

**이 단계가 중요한 이유**
- `Phase 6`는 기능적으로 완료됐지만, 현재 화면은 "런타임 정보가 있긴 한데 한눈에 들어오지 않는" 구간이 남아 있다.
- 검증 Phase로 넘어가기 전에 정보 위계를 바로잡아야 이후 diagnostics, benchmark, export 비교 화면도 같은 패턴 위에서 안정적으로 확장된다.

**게이트**
- 100% 및 125% 배율에서 상단 HUD 1차/2차 정보가 한눈에 구분됨
- 경고/배너/토스트가 일반 편집 상태와 명확히 구분되고 캔버스 위에서 묻히지 않음
- 기본 편집 화면에서 캔버스 집중도가 유지되고, Inspector는 편집 정보와 진단 정보가 섞여 보이지 않음
- 대표 캡처 세트(기본 Run / heatmap / live probe / buffer overlay / warning state) 기준으로 정보 전달력이 일관됨

**검증 메모 (2026-03-08)**
- `build_check.bat` 기준 `EXIT_CODE=0`
- 디버그 실행창 기본 Run 캡처로 상단 HUD, 토글 상태 표시, runtime overlay 배치 확인

---

### Milestone 2: Verification Backbone [DONE]

**Function First**
- Phase `7`
  - golden audio test
  - runtime vs export parity test
  - export compile CI
  - stress / fuzz / soak test
  - benchmark regression gate

**Then UI**
- UI Phase `7`
  - Diagnostics Drawer
  - root-cause cards
  - report diff view
  - runtime vs export compare view
  - benchmark timeline
  - smoke/export artifact viewer
  - one-click validate/export/benchmark
  - representative graph set manager
  - result sharing format

**Why This Matters**
- Without verification infrastructure, later regressions will not be caught early.
- Export codegen, asset packaging, and preset migration can look correct in the UI while still producing wrong results.

**Step 1: Verification Baseline v1 [DONE]**
- Representative graph set v1
  - `G1 Tone Path`: `Oscillator -> VCA -> Audio Output`
    - Purpose: basic audio path, gain application, reset/restart consistency
  - `G2 Filter Sweep`: `Oscillator -> LowPass Filter -> Audio Output`
    - Purpose: filter automation, smoothing, cutoff/resonance parity
  - `G3 Stereo Motion`: `Oscillator -> Stereo Panner -> Audio Output`
    - Purpose: stereo routing, pan automation, left/right balance parity
  - `G4 MIDI Voice`: `MIDI Input -> MIDI to CV -> ADSR Envelope -> VCA -> Audio Output`
    - Purpose: note timing, gate/CV response, envelope shape parity
  - `G5 Time Tail`: `Oscillator -> Delay` or `Oscillator -> Reverb -> Audio Output`
    - Purpose: state retention, tail behavior, reset parity, long render stability
- Stimulus set v1
  - `S1 Static Render`: 2-second render at default parameters
  - `S2 Step Automation`: 250 ms step changes over a 2-second render
  - `S3 Sweep Automation`: 2-second linear sweep for cutoff / gain / pan style parameters
  - `S4 MIDI Phrase`: 2-second note on/off pattern for `G4 MIDI Voice`
- Render profile v1
  - primary: `48 kHz / 128 samples / stereo`
  - secondary: `48 kHz / 480 samples / stereo`
  - extended: `96 kHz / 128 samples / stereo` for `G1`, `G2`, `G3` first
- Parity tolerance v1
  - audio compare: per-channel `max abs error <= 1e-5` and `RMS error <= 1e-6`
  - gate/event timing: exact sample index match
  - `NaN` / `Inf`: immediate failure
  - denormals are normalized to `0` before comparison
- Failure artifact requirements
  - graph ID, stimulus ID, render profile, seed/document revision, first mismatch sample index, peak error, RMS error, and failing buffer dump
- This baseline is the shared contract for the future `golden audio harness`, `runtime vs export parity harness`, `benchmark baseline`, and `Diagnostics Drawer`.

**Implementation Checklist**
- [x] Lock `G1` to `G5` as actual fixture documents.
- [x] Split `S1` to `S4` into reusable headless stimulus helpers.
- [x] Add a common parity report structure for artifacts and failure summaries.
- [x] Wire `runtime vs export parity test` starting from `G1 Tone Path` as editable round-trip smoke.
- [x] Add CLI smoke entry: `--teul-phase7-parity-smoke`.
- [x] Add golden audio record runner: `Tools/TeulVerification/teul_golden_audio_record.bat`.
- [x] Add golden audio verify runner: `Tools/TeulVerification/teul_golden_audio_verify.bat`.
- [x] Add batch runner: `Tools/TeulVerification/teul_parity_smoke.bat`.
- [x] Add matrix runner: `Tools/TeulVerification/teul_parity_matrix.bat`.
- [x] Add compile runner: `Tools/TeulVerification/teul_runtime_compile_smoke.bat`.
- [x] Expand parity coverage from `G1 + S1` to the representative matrix.
- [x] Add generated RuntimeModule compile smoke on exported `.h/.cpp`.
- [x] Add CI-friendly failure artifact bundling for automated runs.
- [x] Add benchmark runner: `Tools/TeulVerification/teul_benchmark_gate.bat`.
- [x] Add representative benchmark baseline and regression gate.
- [x] Add compiled runtime parity runner: `Tools/TeulVerification/teul_compiled_runtime_parity.bat`.
- [x] Add headless workflow: `.github/workflows/teul-phase7-headless.yml`.
- [x] Diagnostics Drawer is available from the Teul toolbar and shows the latest parity, stress, benchmark, and compile artifacts.

**Verification Notes (2026-03-09)**
- [x] `DadeumStudio.exe --teul-phase7-golden-audio-record` recorded representative golden baselines.
- [x] `DadeumStudio.exe --teul-phase7-golden-audio-verify` passed on the representative golden suite.
- [x] `golden-suite-summary.txt` and `golden-output.wav` were generated for representative primary cases.
- [x] `build_check.bat` passed after fixture/stimulus/parity/CLI integration.
- [x] `DadeumStudio.exe --teul-phase7-parity-smoke` passed with `G1 / S1 / primary`.
- [x] `parity-summary.txt` recorded `passed=true`, `maxAbsoluteError=0`, `rmsError=0` for the initial smoke.
- [x] `DadeumStudio.exe --teul-phase7-parity-matrix` passed on the representative primary matrix.
- [x] `matrix-summary.txt` was generated under `Builds/TeulVerification/EditableRoundTrip/RepresentativeMatrix_primary/`.
- [x] `DadeumStudio.exe --teul-phase7-runtime-compile-smoke` passed on exported RuntimeModule code.
- [x] `compile-output.txt` was generated under `Builds/TeulCompileSmoke_*/`.
- [x] `artifact-bundle.json` is now emitted for parity smoke, parity matrix, per-case parity runs, and runtime compile smoke.
- [x] `DadeumStudio.exe --teul-phase7-stress-soak` passed on the representative stress/soak suite.
- [x] `stress-summary.txt` was generated under `Builds/TeulVerification/StressSoak/representative_stress_primary/`.
- [x] `DadeumStudio.exe --teul-phase7-benchmark-gate` passed on the representative benchmark suite.
- [x] `benchmark-summary.txt` was generated under `Builds/TeulVerification/Benchmark/representative_benchmark_primary/`.
- [x] `DadeumStudio.exe --teul-phase7-compiled-runtime-parity` passed on the stable compiled parity core suite (`G1`, `G2`, `G4`, `G5`).
- [x] `.github/workflows/teul-phase7-headless.yml` now runs `build`, `golden verify`, `parity matrix`, `compiled parity`, `runtime compile smoke`, and `benchmark gate` on `windows-2022`.
- [x] The local runner sequence matched the headless workflow command set.
- [x] The Diagnostics Drawer implementation now reads the latest verification summaries from generated artifacts.

**Gate**
- [x] parity test passes on representative graphs
- [x] generated `.h/.cpp` compiles automatically
- [x] long soak/stress runs collect logs without crashes
- [x] benchmark regression gate passes on representative graphs
- [x] headless verification workflow covers the core Phase 7 command set
---

### ???? 3: Preset / State / Recovery

**Function First**
- Functional `Phase 8`
  - patch preset storage around logical frame groups
  - state preset save/apply
  - autosave / crash recovery
  - compatibility alias handling and matrix smoke
  - migration / degraded warning propagation

**Milestone 3 Status**
- [x] frame group -> patch preset save/load/insert MVP
- [x] document-level state preset save/apply MVP
- [x] autosave / crash recovery MVP
- [x] legacy alias compatibility for document / patch preset / state preset
- [x] compatibility test matrix
- [x] migration / degraded load reports
- [x] transient recovery / compatibility warning banner
- [x] preset browser / state diff / recovery preview / conflict UX core
- [x] explicit schema migration chain
- [ ] external control sources / device profiles

**Verification Notes (2026-03-09)**
- [x] `Tools/TeulVerification/teul_patch_preset_smoke.bat` passed.
- [x] `Tools/TeulVerification/teul_state_preset_smoke.bat` passed.
- [x] `Tools/TeulVerification/teul_autosave_recovery_smoke.bat` passed.
- [x] `Tools/TeulVerification/teul_compatibility_smoke.bat` passed on legacy document / patch preset / state preset aliases.
- [x] `Tools/TeulVerification/teul_compatibility_matrix.bat` passed on the representative compatibility matrix.
- [x] compatibility smoke and matrix now verify explicit document / patch preset / state preset migration steps.
- [x] `build_check.bat` passed after the transient recovery warning banner integration.

**Then UI**
- UI `Phase 8`
  - preset browser library with favorites / recents / tags
  - state diff preview and recovery snapshot diff preview
  - dirty state, autosave status, and crash recovery preview
  - migration warning / degraded recovery guidance
  - control source rail and device profile UX

**Why This Stage Matters**
- Verification infrastructure is in place, so the next risk is losing work or restoring the wrong state.
- The current MVP path already persists presets and recovery snapshots, and the migration chain is now explicit; the remaining major work in Milestone 3 is external control/device profile UX.

**Gate**
- [x] patch preset save/load/insert smoke passes
- [x] state preset save/apply smoke passes
- [x] autosave recovery smoke passes
- [x] compatibility smoke and compatibility matrix pass
- [x] explicit schema migration chain is documented and exercised
- [x] UI Phase 8 exposes preset browsing, state diff, recovery preview, and conflict summaries clearly

---

### External Control Sources / Device Profiles

**UI Layout Draft**
- Left `Input Rail`: target width `12% - 14%` of the editor.
- Right `Output Rail`: target width `12% - 14%` of the editor.
- Bottom `Control Rail`: target height `14% - 18%` of the editor.
- Center canvas keeps the remaining space and remains the primary DSP editing area.
- Rails must be collapsible so the canvas can reclaim space during heavy graph editing.

**Visual Language**
- General node ports stay as small neutral sockets for internal graph wiring.
- Rail ports must look like hardware boundary connectors rather than normal node pins.
- Default state should show rails as `jack/socket` style endpoints; dragging should reuse the existing cable rendering language.
- Cable thickness should stay mostly consistent; differentiation should come from port silhouette, slot background, and connector cap/stub rather than heavy line-weight changes.

**Per-Rail Port Shape Rules**
- `Input Rail` / `Output Rail`: capsule or half-jack connectors with stronger frame/border treatment.
- `Control Rail`: source cards with small `pill` ports for `Value`, `Gate`, and `Trigger`.
- `Expression` sources should default to a single `Value` port.
- `Footswitch` sources should default to `Gate` + `Trigger` ports.
- Stereo audio endpoints should read as grouped `L/R` connectors rather than two unrelated dots.

**Color Direction**
- Rail backgrounds should be one tone denser than the canvas so they read as system panels.
- Audio I/O accents: teal / cyan family.
- Continuous control accents (`Value`): warm yellow / amber family.
- Gate / trigger accents: orange / red family.
- MIDI-oriented accents: mint / lime family.
- Rail connectors should keep stronger borders and slightly lower fill saturation than internal node ports.

**Planned Resource Usage**
- Prefer code-drawn rails, ports, and cables for DPI scaling, zooming, and state transitions.
- Add small reusable icon resources for `Input`, `Output`, `Expression`, `Footswitch`, `MIDI`, `Missing`, `Learn`, and `Auto` badges.
- Avoid bitmap-driven cables or large decorative panel backgrounds.
- If image assets are introduced, keep them limited to icons, badges, and mini-glyphs rather than core interaction geometry.

**Step 2 Target**
- Render the three rails with placeholder data from the new document model.
- Show at least `EXP 1`, `FS 1`, and one audio in/out endpoint in the control draft.
- Validate spacing, card density, and port readability with screenshots before device detection or learn mode is connected.

목표:
- 외부 MIDI foot controller, expression pedal, switch 입력을 그래프의 시스템 경계로 다루고, preset/state 복원 흐름과 함께 안정적으로 저장·재연결한다.

주요 작업:
- [ ] **Control Source Rail 모델 도입**: 좌측 Input rail, 우측 Output rail, 하단 Control Source rail 구조를 기준으로 시스템 I/O와 일반 DSP 노드를 분리
- [ ] **동적 장치 감지**: 연결된 외부 컨트롤 장치를 감지하고, 입력 이벤트를 바탕으로 임시 control source를 자동 생성
- [ ] **learn + confirm 등록 흐름**: 사용자가 페달/스위치를 움직여 `EXP`, `FS`, `Trigger` source를 확정하고 이름, 타입, momentary/toggle 모드를 보정
- [ ] **device profile persist**: 장치별 source 구성, 표시 이름, binding 정보를 profile로 저장하고 재연결 시 안정적으로 복원
- [ ] **preset/state 연동 복원**: 문서 로드, preset 전환, crash recovery 이후에도 control source assignment와 device mapping이 일관되게 복원되도록 상태 모델 정리
- [ ] **fallback / partial recovery 정책**: 장치가 없거나 source 일부가 누락된 경우 unassigned, degraded, relink-needed 상태를 명시적으로 유지
- [ ] **assignment inspector 계약 정리**: `Value`, `Gate`, `Trigger` 같은 source 출력 타입과 target parameter binding 규칙을 명문화
- [ ] **검증 항목 추가**: device reconnect, profile mismatch, preset reload, missing controller 상황에 대한 state recovery 테스트 추가

완료 기준:
- 대표 foot controller / expression 장치를 연결했을 때 source가 자동 감지되고, 사용자가 learn으로 의미를 확정할 수 있다.
- 저장 후 재실행하거나 장치를 재연결해도 source profile과 assignment가 일관되게 복원된다.
- 장치가 누락되거나 구성이 달라져도 문서가 깨지지 않고 degraded 상태로 복구된다.

**바로 이어서 UI**
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

**이 단계가 중요한 이유**
- 상용 툴에서 사용자가 가장 싫어하는 것은 데이터 손실이다.
- 검증 인프라가 준비된 뒤에 preset/migration을 얹어야 포맷 변경을 안전하게 운영할 수 있다.

**게이트**
- autosave 복구 경로가 실제로 동작
- 구버전 문서/프리셋을 migration 후 다시 열 수 있음
- recovery/migration 실패가 사용자에게 설명 가능한 상태로 노출됨

---

### 마일스톤 4: 상용 필수 노드 최소셋

**우선 구현할 노드**
- `SamplePlayer`
- `Compressor`
- `Limiter`
- `EnvelopeFollower`
- `Oscilloscope`
- `LevelMeter`

**선정 이유**
- `SamplePlayer`는 asset/package 경로를 실전적으로 검증하는 핵심 노드다.
- `Compressor`, `Limiter`, `EnvelopeFollower`는 믹싱/모듈레이션 실사용 그래프를 만들기 위해 필요하다.
- `Oscilloscope`, `LevelMeter`는 verification/demo/support 단계에서 관측 장비 역할을 한다.

**이 단계에서 하지 않을 것**
- EQ 계열, modulation FX, MIDI 유틸, analyzer 전부를 한 번에 구현하지 않는다.
- `ITNode` 인터페이스 분리, 동적 노드 로드는 미룬다.

**게이트**
- 데모/테스트/패키징에 쓸 대표 그래프 세트를 위 노드만으로 구성 가능
- asset 포함 그래프와 dynamics 그래프를 모두 검증할 수 있음

---

### 마일스톤 5: Asset & Packaging

**기능 먼저**
- 기능 `Phase 9`
  - asset manifest
  - bundle / relative path 정책
  - missing asset relink flow
  - RuntimeModule package layout
  - deterministic packaging

**바로 이어서 UI**
- UI `Phase 9`
  - Asset Browser
  - 의존성 맵
  - unused/missing 표시
  - Missing Asset Wizard
  - 정책 선택 UI
  - 복구 로그
  - Package Preview
  - portable health check
  - README/manifest 미리보기

**이 단계가 중요한 이유**
- 상용 사용자는 그래프만 저장하지 않는다. 자산까지 함께 옮겨야 한다.
- `SamplePlayer` 등 외부 파일 기반 노드가 들어오는 순간, packaging과 relink가 제품 완성도의 핵심이 된다.

**게이트**
- 누락 자산을 재연결할 수 있음
- export package가 다른 경로/머신에서도 재현 가능
- package 결과가 deterministic하게 유지됨

---

### 마일스톤 6: Host Integration

**기능 먼저**
- 기능 `Phase 10`
  - host template 제공
  - APVTS attachment 예제
  - plugin wrapper 가이드
  - latency / tail / bus metadata export
  - integration smoke project

**바로 이어서 UI**
- UI `Phase 10`
  - export 마법사
  - 사전 검증 요약
  - 산출물 요약 화면
  - 호스트 템플릿 런처
  - Projucer/CMake 편입 가이드
  - 파라미터 매핑 미리보기
  - post-export smoke 버튼
  - integration checklist
  - copy-ready snippet

**이 단계가 중요한 이유**
- export가 된다와 바로 제품에 붙일 수 있다는 전혀 다르다.
- 이 단계에서 “생성물”을 “통합 가능한 컴포넌트”로 끌어올린다.

**게이트**
- 샘플 host project에 generated module을 실제로 편입 가능
- 최소 하나 이상의 integration smoke가 자동 검증됨
- 사용자 입장에서 export 후 다음 행동이 문서/마법사로 바로 연결됨

---

### 마일스톤 7: Team Workflow & CLI

**기능 먼저**
- 기능 `Phase 11`
  - headless CLI
  - deterministic codegen
  - machine-readable report
  - debug trace / inspector
  - sample project / cookbook

**바로 이어서 UI**
- UI `Phase 11`
  - 작업 센터
  - 프로필 저장
  - 배치 액션
  - JSON report 뷰어
  - 실행 이력 비교
  - 재현 패키지 생성
  - cookbook 브라우저
  - 규약 체크 배지
  - 리뷰용 요약 패널

**이 단계가 중요한 이유**
- 혼자 쓰는 툴이 아니라 팀과 CI가 함께 보는 제품으로 넘어가는 구간이다.
- 이 단계가 되어야 대규모 회귀 검증, repro bundle, 팀 리뷰가 안정적으로 굴러간다.

**게이트**
- CLI만으로 validate/export/benchmark 가능
- report가 사람과 CI 둘 다 읽을 수 있음
- 버그 재현용 bundle 생성이 가능

---

### 마일스톤 8: Commercial Release Finish

**기능 먼저**
- 기능 `Phase 12`
  - release QA matrix
  - support bundle / crash log
  - licensing / edition 정책
  - 문서 / 튜토리얼 / 온보딩
  - release checklist / LTS 정책

**바로 이어서 UI**
- UI `Phase 12`
  - 키보드 완전 탐색
  - 고대비/색각 보정 테마
  - 배율/해상도 적응성
  - first-run onboarding
  - 빈 상태/실패 상태 문구 정비
  - 문서/예제 직결 링크
  - support bundle 생성기
  - 릴리스 readiness 대시보드
  - edition/license 진입점

**이 단계가 중요한 이유**
- 이제부터는 기능 추가보다 배포 후 문제를 얼마나 줄일 수 있느냐가 중요하다.
- 접근성, 문서, 지원 번들, 릴리스 운영 기준이 없으면 상용 제품으로 오래 유지하기 어렵다.

**게이트**
- 신규 사용자가 첫 실행부터 export까지 길을 잃지 않음
- 지원 요청에 필요한 정보가 에디터 내부에서 수집 가능
- 접근성/고배율/실패 상태 UX가 제품 수준으로 정리됨

---

## 1.0 이전에 미루지 말아야 할 것

- Runtime safety와 verification infrastructure
- autosave / crash recovery
- asset relink / deterministic packaging
- host integration smoke
- CLI validate/export 경로
- support bundle / crash log

이 항목들은 “있으면 좋은 기능”이 아니라 상용 배포 후 문제 비용을 줄이는 필수 기반이다.

---

## 1.0 이후로 미뤄도 되는 것

### 남은 노드 카탈로그 확장
- `Clock`
- `NotchFilter`
- `PeakEQ`
- `ShelfEQ`
- `CombFilter`
- `FormantFilter`
- `AR`
- `SampleAndHold`
- `Lag`
- `RingModulator`
- `PingPongDelay`
- `Chorus`
- `Flanger`
- `Phaser`
- `Waveshaper`
- `BitCrusher`
- `FrequencyShifter`
- `Crossfader`
- `StereoSplitter` / `StereoMerger`
- `Send` / `Return`
- `Quantizer`
- `Comparator`
- `Abs` / `Min` / `Max`
- `MidiCCToValue`
- `Arpeggiator`
- `Chord`
- `MidiFilter`
- `SpectrumAnalyzer`

### 확장 생태계 항목
- `ITNode` 인터페이스 분리
- 동적 노드 로드

이 항목들은 제품 가치를 높이지만, 지금 바로 들어가면 ABI/문서/지원 범위가 급격히 커진다.

---

## 최종 권장 순서 요약

1. UI 잔여 프리뷰 3개 마감
2. 기능 `Phase 6`
3. UI `Phase 6`
4. UI `Phase 6` polish (`마일스톤 1.5`)
5. 기능 `Phase 7`
6. UI `Phase 7`
7. 기능 `Phase 8`
8. UI `Phase 8`
9. 상용 필수 노드 최소셋 추가
10. 기능 `Phase 9`
11. UI `Phase 9`
12. 기능 `Phase 10`
13. UI `Phase 10`
14. 기능 `Phase 11`
15. UI `Phase 11`
16. 기능 `Phase 12`
17. UI `Phase 12`
18. 남은 노드 카탈로그 / 외부 플러그인 ABI는 1.0 이후 검토

---

## 실행 원칙 한 줄 요약

> `안전성 -> 검증 -> 복구 -> 필수 노드 -> 패키징 -> 통합 -> 팀 운영 -> 상용 마감` 순서로 간다.
---

### 외부 제어 소스 / 디바이스 프로필 - 포트/채널 UI 정리 (2026-03-10)

**현재까지 확정한 기준**
- 데이터 모델에서는 채널을 분리해서 다룬다.
- UI에서는 기본적으로 채널을 통합해서 보여주고, 채널별 분기가 필요할 때만 세부 채널을 드러낸다.
- 포트 스펙은 단일 enum 대신 아래 축으로 정의한다.
  - `PortDirection`: `Input`, `Output`
  - `SignalShape`: `Mono`, `Stereo`, `Bus`
  - `ConnectionPolicy`: `Single`, `Multi`
  - `SignalDomain`: `Audio`, `Midi`, `Control`
- `Stereo`는 의미 있는 `L/R`를 가지는 2채널 포트로 취급한다.
- `Bus`는 3채널 이상 또는 별도 라벨이 필요한 채널 묶음을 뜻한다.
- `Multi`는 신호 타입이 아니라 연결 정책이며, `maxConnections` 또는 `unbounded`를 함께 정의한다.
- `Audio`, `Midi`, `Control`은 서로 다른 도메인으로 구분하고 색 체계도 이 도메인 기준으로 유지한다.
- `L/R`는 도메인 구분이 아니므로 강한 색 분리는 하지 않고, 위치/형태/라벨 중심으로 구분한다.

**포트 실루엣 기준**
- `Mono`: 원형 포트
- `Stereo`: 아령형 포트(좌/우 2개의 원이 붙은 형태)
- `Multi`: 세로로 긴 캡슐형 포트
- `Stereo` 포트는 채널별 연결 상태를 따로 보여준다.
  - `L`만 연결되면 한쪽 원만 채움
  - `R`만 연결되면 다른 쪽 원만 채움
  - 둘 다 연결되면 양쪽을 모두 채움
- `Multi` 포트는 위에서부터 슬롯이 순서대로 할당된다.
  - 첫 번째 연결 = 슬롯 1
  - 두 번째 연결 = 슬롯 2
  - 이후 같은 방식으로 증가
- `Multi` 포트의 슬롯별 역할/게인/라우팅은 Inspector에서 조정한다.

**채널/라우팅 기준**
- UI 기본 상태에서는 `Stereo`를 하나의 포트/하나의 케이블처럼 보이게 한다.
- 좌우 채널이 같은 경로로 이동할 때는 stereo pair 케이블 1개로 표현한다.
- 좌우 채널이 다른 목적지로 갈라지는 순간만 `L/R` 개별 라우팅을 드러낸다.
- `Stereo In -> Stereo Out` 노드의 bypass는 기본적으로 채널 보존으로 정의한다.
  - `Out L = In L`
  - `Out R = In R`
- `Stereo In -> Mono Out`은 bypass 포함 기본 downmix 규칙이 필요하다.
- 채널 구조 자체가 의미가 되는 노드(`Stereo Mixer`, `Panner`, `Channel Mapper`, 다채널 버스 노드 등)는 명시적으로 채널 구분을 보여줄 수 있어야 한다.
- 일반적인 mono effect 노드는 기본적으로 단순 포트 UI를 유지하고, 복잡한 채널 구조를 강요하지 않는다.

**Rail UI 기준**
- 현재 구현은 rail 골격과 control rail inspector까지 들어간 상태다.
- 다음 단계에서는 `Input/Output` 카드 선택, inspector 연결, hover/selected 상태 통일, spacing/padding polish를 진행한다.
- `Input/Output` rail은 우선 표시/선택/조회 UX를 완성한 뒤, 실제 장치 감지/learn/profile 복원 흐름을 얹는다.

**다음에 더 정해야 하는 것**
- 포트 표시 상태 집합 확정
  - 예: `Idle`, `Hover`, `Selected`, `Connected`, `PartiallyConnected`, `Full`, `Invalid`, `Missing`, `Degraded`
- `Multi` 포트의 연결 수 시각화 규칙
  - 슬롯 채움형, `2/4` 카운트 배지, 최대치 도달 시 full 상태 표시 방식
- `Bus` 포트의 기본 UI 규칙
  - `3ch`, `4ch`, `5.1` 같은 다채널을 기본적으로 묶어서 보일지, 펼칠 수 있게 할지
- 자동 채널 변환 허용 범위
  - `Mono -> Stereo`, `Stereo -> Mono`, `Bus -> Mono/Stereo`를 어디까지 자동 허용할지
- Inspector 편집 범위
  - 읽기 전용으로 시작할지, 채널 모드/slot role/gain 같은 최소 편집까지 허용할지
- stereo pair 케이블이 mono 케이블 2개로 분기되는 시각 규칙
- fallback 상태(`Missing`, `Degraded`, `Relink Needed`)를 rail/card/port 어디에 어떤 강도로 표시할지

**권장 우선순위**
1. 포트 상태 집합 확정
2. `Stereo` / `Multi` 포트 렌더링 규칙 확정
3. `Input/Output` 카드 선택 + Inspector 연결
4. 자동 채널 변환 규칙 확정
5. `Bus` 포트 확장 규칙 확정
**추가 합의 사항 (포트/케이블 UI)**
- `SignalShape`는 `Mono`, `Stereo`, `Bus` 세 종류로 유지한다.
- `Multi`는 별도 포트 종류가 아니라 같은 타입 포트를 여러 개 배치하는 연결 정책으로 본다.
  - `Multi Mono` = 원형 포트 여러 개
  - `Multi Stereo` = 아령형 포트 여러 개
  - `Multi Bus` = 긴 캡슐형 포트 여러 개
- 긴 캡슐형 포트는 `Bus` 포트의 기본 실루엣으로 사용한다.
- `Stereo` 포트의 hit test는 영역별로 나눈다.
  - 좌측 원 클릭 = `L` 채널 개별 선
  - 우측 원 클릭 = `R` 채널 개별 선
  - 가운데 손잡이 클릭 = `Stereo` 통합 선
- `Bus` 포트도 같은 원칙을 따른다.
  - 채널에 대응하는 세그먼트/원 영역 클릭 = 해당 채널 개별 선
  - 나머지 바디 영역 클릭 = 전체 `Bus` 통합 선
- 통합 선은 완전히 다른 문법의 새 선으로 만들지 않고, 개별 채널 선이 묶인 bundle처럼 보이게 한다.
- 한 포트 안에서 통합 선과 개별 채널 선은 공존 가능하다.
  - 단, 해당 포트의 입력 수용량(capacity) 안에서만 허용한다.
- `MultiStereo`는 stereo 슬롯 수보다 mono channel unit 총량 기준으로 해석한다.
  - 예: `MultiStereo x4` = 총 `8 mono channel units`
  - stereo 통합 연결 1개 = `2 units`
  - mono 개별 채널 연결 1개 = `1 unit`
  - 따라서 stereo 4개, mono 8개, stereo 3개 + mono 2개 같은 혼합이 가능하다.
- 비대칭 채널 사용은 시스템 차원에서 허용한다.
  - 예: `L`만 여러 개 사용하거나 `R`만 여러 개 사용하는 구성이 가능하다.
  - 대신 UI에서 partial/asymmetric 상태가 명확히 드러나야 한다.
- `Mono -> Stereo` 자동 연결은 사용자가 꽂은 채널(`L` 또는 `R`)에만 입력한다.
- `Stereo -> Mono`는 기본 downmix(버퍼 합치기)로 처리한다.
- `Bus -> Stereo/Mono`는 사용자가 꽂은 채널만 입력으로 받는다.
  - 예: `Bus 1 -> Stereo L`, `Bus 2 -> Stereo R`
  - 사용하지 않은 다른 bus channel은 자동 연결하지 않는다.
- `Input/Output/Control` 카드 상호작용은 다음 기준을 따른다.
  - 카드 클릭 = Inspector에 내부 정보 표시
  - 포트 클릭 = 연결선 드래그 시작
- Inspector는 최소한 아래 두 섹션으로 분리한다.
  - 연결 설정부
  - 오디오 파라미터 편집부
- I/O 및 Control 포트 디자인은 일반 노드 포트 문법을 공유하되, 시스템 경계 포트처럼 약간 더 꾸민다.


**추가 합의 사항 (케이블/강조/애니메이션)**
- bundle 케이블은 개별 채널 선이 묶인 형태로 보이게 하되, 다발 구간은 비교적 얇게 유지한다.
- bundle 케이블의 포트 연결부는 포트 길이와 비슷한 폭으로 맞춘다.
  - 가능하면 포트 실루엣과 정확히 맞물리는 위치/폭으로 보이게 한다.
- `partial` / `asymmetric` 상태 강조는 약하게 유지한다.
  - 과도한 경고처럼 보이지 않도록 subtle fill, 약한 tint, 약한 보조 강조만 사용한다.
- `Selected`는 포트 내부 상태로 표현하고, `Missing`은 포트 외부 점선 링/색으로 표현한다.
  - 따라서 두 상태는 시각적으로 공존 가능하다고 본다.
- 외부 점선 링 위에 마우스를 올리면 tooltip으로 현재 문제 상태를 알려준다.
  - 예: `Missing`, `Degraded`, `Invalid Config`
- `Bus` 포트가 6채널 이상이면 포트 길이를 더 이상 늘리지 않는다.
  - 현재 길이를 유지하고, 내부 채널 세그먼트/포트 두께를 줄여서 채널 수를 수용한다.
- 애니메이션 강도는 눈에 띄게 가져간다.
  - hover, drag target, missing/degraded pulse는 사용자가 바로 인지할 수 있을 정도로 명확해야 한다.
**아직 남은 결정 사항**
- stereo/bus bundle 케이블의 실제 두께, 외곽선, 내부 결합 힌트를 어떤 수준으로 줄지
- partial/asymmetric 상태를 포트에서 어느 강도로 강조할지
- `Selected + Missing`, `Hover + InvalidConfig`, `Connected + Degraded` 같은 상태 조합의 우선순위
- `Bus` 포트가 많은 채널을 가질 때(예: 6ch 이상) 라벨과 세그먼트를 기본 노출할지, hover/expand에서만 보여줄지
- hover, drag target, missing/degraded pulse 같은 애니메이션 강도를 어디까지 허용할지
**Additional Decisions (Rendering Order / Bus Visibility / Animation Rules)**
- Port rendering order is fixed as:
  1. port interior
  2. port border
  3. outer status ring / outer error indicator
- `Bus` channels and labels stay visible by default rather than hiding behind hover-only disclosure.
- Interaction animations should be clear and noticeable, while still respecting the difference between temporary interaction feedback and longer-lived error feedback.
- `Hover`
  - Fast response.
  - Slight interior brightening and a short border glow.
  - Target feel: quick, light, immediate.
- `Selected`
  - Stable persistent emphasis.
  - Prefer a mostly static interior/border highlight rather than a repeating pulse.
  - Selection should remain readable even when combined with outer warning states.
- `Focus`
  - Lighter than `Selected`.
  - Use a subtle focus ring or border emphasis intended for keyboard/navigation clarity.
- `DragSource`
  - Stronger than hover.
  - The port should feel actively engaged as the cable origin.
  - A mild pulse is acceptable.
- `DragTarget (Valid)`
  - Clearly stronger than hover.
  - Use a readable glow/breathing highlight to show that the drop is accepted.
- `DragTarget (Invalid)`
  - Immediate warning response.
  - Use a stronger invalid tint / alert flash than the valid target state.
- `Missing`
  - Use the outer dashed ring with a slow, persistent pulse.
  - It should remain visible over time without feeling like a constant alarm.
- `Degraded`
  - Similar outer-ring treatment, but weaker than `Missing`.
  - Keep it clearly noticeable while less aggressive than a fully missing state.
- `InvalidConfig`
  - Outer warning state with the strongest error emphasis.
  - Stronger than `Degraded`, and generally more urgent than `Missing`.
- `Partial` / `Asymmetric`
  - Keep the emphasis intentionally subtle.
  - These states should read as informative routing detail, not as hard warnings.
- Tooltip rule:
  - Hovering the outer dashed warning ring should explain the current issue state such as `Missing`, `Degraded`, or `Invalid Config`.
