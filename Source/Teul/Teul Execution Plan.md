# Teul (?) ???듯빀 ?ㅽ뻾 ?쒖꽌
> 紐⑹쟻: [Teul Roadmap.md](Teul%20Roadmap.md)? [Teul UI Roadmap.md](Teul%20UI%20Roadmap.md)???⑥? ?④퀎瑜??ㅼ젣 援ы쁽 ?쒖꽌濡??щ같?댄븳 ?ㅽ뻾 怨꾪쉷.

---

## ?꾩옱 ?곹깭 ?붿빟

- 湲곕뒫 濡쒕뱶留듭? `Phase 7: Verification & Benchmark Infrastructure` core源뚯? ?꾨즺??
- UI 濡쒕뱶留듭? `Phase 7: 寃利?吏꾨떒 ?뚰겕?ㅽ럹?댁뒪` core源뚯? ?꾨즺??
- `留덉씪?ㅽ넠 2`源뚯? 留덇컧?섏뼱 verification runner, diagnostics drawer, compare/timeline/action bar媛 諛섏쁺??
- ?ㅼ쓬 硫붿씤 ?묒뾽? `留덉씪?ㅽ넠 3: Preset / State / Recovery`? `UI Phase 8` 踰붿쐞??
- ?⑥? ?뺤옣 ??ぉ? deferred verification, deferred diagnostics, external control/device profile UX 以묒떖?쇰줈 ?뺣━??

---

## ?곗꽑?쒖쐞 ?먯튃

1. **蹂닿린 醫뗭? 寃껊낫??癒쇱? ?덉쟾?댁빞 ?쒕떎**
   export媛 媛?ν빐吏?吏湲덈??곕뒗 ?ㅻ뵒???ㅻ젅???덉젙?? 寃利?媛?μ꽦, 蹂듦뎄 媛?μ꽦??UI蹂대떎 ?곗꽑?대떎.

2. **寃利??놁씠 ?ㅼ쓬 ?④퀎濡?媛吏 ?딅뒗??*
   `RuntimeModule Export`媛 ?앷꼈湲??뚮Ц?? ?댄썑 ?④퀎???꾨? parity test/compile test/soak test瑜??꾩젣濡??볥뒗??

3. **?몃뱶 諛깅줈洹몃뒗 ?곸슜 寃쎈줈???꾩슂??理쒖냼?뗫쭔 癒쇱? 異붽??쒕떎**
   ?⑥븘 ?덈뒗 紐⑤뱺 ?몃뱶瑜???踰덉뿉 援ы쁽?섏? ?딅뒗?? ?⑦궎吏? 寃利? ?곕え, ?몄뒪???듯빀??吏곸젒 ?꾩슂???몃뱶留?癒쇱? ?щ┛??

4. **湲곕뒫 Phase? UI Phase瑜?踰덇컝???ル뒗??*
   湲곕뒫 援ы쁽?쇰줈 湲곕컲??留뚮뱾怨? 諛붾줈 ???UI Phase瑜??レ븘 ?ъ슜??寃쏀뿕源뚯? 留욎텣 ???ㅼ쓬 湲곕뒫 Phase濡??섏뼱媛꾨떎.

---

## 沅뚯옣 援ы쁽 ?쒖꽌

### 留덉씪?ㅽ넠 0: UI ?붿뿬 1李?留덇컧

**癒쇱? 泥섎━??寃?*
- UI ?붿뿬 ?꾨━酉?3媛?
  - ?ㅼ떎?덉씠???뚰삎 ?꾨━酉?
  - ADSR 而ㅻ툕 ?꾨━酉?
  - ?덈꺼 誘명꽣 ?몃씪???쒖떆

**??癒쇱? ?섎굹**
- ??????ぉ? 湲곗〈 UI Phase 2??誘몄셿猷??붿뿬遺꾩씠??
- ?ㅼ뿉??留뚮뱾 `Runtime Safety UX`???쇱씠釉??꾨줈釉??곹깭 媛?쒗솕? 諛⑺뼢??媛숈븘?? 吏湲??뺣━???먮㈃ ?댄썑 HUD/heatmap ?ㅺ퀎媛 ?ъ썙吏꾨떎.

**?꾨즺 湲곗?**
- ?몃뱶 諛붾뵒 ?덉뿉??誘몃땲 ?꾨━酉곌? ?덉젙?곸쑝濡?媛깆떊??
- ?몄쭛 ?깅뒫怨?媛?낆꽦??湲곗〈 Phase 2 ?ㅽ??쇨낵 異⑸룎?섏? ?딆쓬

---

### 留덉씪?ㅽ넠 1: Runtime Safety 肄붿뼱 [DONE]

**湲곕뒫 癒쇱?**
- 湲곕뒫 `Phase 6`
  - audio thread 臾댄븷??臾대씫 蹂댁옣
  - parameter smoothing / bypass / denormal 泥섎━
  - sampleRate / blockSize / channel layout ?곸쓳??
  - runtime rebuild / state handoff ?덉쟾??
  - ?깅뒫 budget 怨꾩륫湲?

**諛붾줈 ?댁뼱??UI**
- UI `Phase 6`
  - ?몄뀡 ?곹깭 諛?
  - ?ㅻ뵒???곹깭 諛곗?
  - ?ㅽ뻾 紐⑤뱶 ?꾪솚 ?쇰뱶諛?
  - deferred apply 諛곕꼫
  - smoothing ?쒓컖??
  - ?꾪뿕 ?숈옉 媛??
  - ?몃뱶 鍮꾩슜 heatmap
  - 踰꾪띁/?ㅼ?以??ㅻ쾭?덉씠
  - ?쇱씠釉??꾨줈釉?

**???④퀎媛 以묒슂???댁쑀**
- ?댄썑??寃利? preset, packaging, host integration? ?꾨? ?쒖떎??以묒씤 ?고??꾩씠 ?덉쟾?섎떎?앸뒗 ?꾩젣 ?꾩뿉?쒕쭔 ?섎?媛 ?덈떎.
- ?ш린??湲곕컲???붾뱾由щ㈃ ?ㅼ쓽 ?뚯뒪?몃룄 ?좊ː?????녿떎.

**寃뚯씠??*
- ???洹몃옒?꾧? 128-sample block?먯꽌 湲由ъ튂 ?놁씠 ?숈옉
- process 寃쎈줈?먯꽌 ?숈쟻 ?좊떦/???뚯씪 IO媛 ?쒓굅??
- ?고????곹깭瑜?UI?먯꽌 利됱떆 愿李?媛??

---

### 留덉씪?ㅽ넠 1.5: Runtime UX 媛?낆꽦 蹂댁젙 [DONE]

**UI 蹂댁젙 癒쇱?**
- `Phase 6` UI polish
  - heatmap / live probe / overlay 吏꾩엯 踰꾪듉, command palette ??ぉ, ?쒖꽦 ?곹깭 ?쒖떆 異붽?
  - ?곷떒 ?고???HUD ?뺣낫 ?꾧퀎 ?ъ젙??
  - 1李??곹깭(sample rate / block size / CPU / I/O)? 2李??곹깭(gen / nodes / buffers / process) ?쒓컖 遺꾨━
  - ?ㅻ뵒???곹깭 諛곗?, deferred apply, ?꾪뿕 寃쎄퀬??媛뺤“ 洹쒖튃 ?듭씪
  - 醫뚯륫 Node Library, 以묒븰 罹붾쾭?? ?곗륫 Inspector??湲곕낯 ??諛몃윴???ъ“??
  - Run ?곹깭?먯꽌 罹붾쾭??吏묒쨷?꾨? ?댁튂吏 ?딅룄濡??⑤꼸 諛?꾩? 湲곕낯 ?몄텧???뺣━
  - Inspector?먯꽌 ?몄쭛 而⑦듃濡? ?고???硫뷀? ?뺣낫, 吏꾨떒 ?뺣낫瑜??뱀뀡 ?⑥쐞濡?遺꾨━
  - ?좎뒪??諛곕꼫???鍮? ?ш린, 吏???쒓컙 洹쒖튃 蹂댁젙
  - Mini-map / zoom HUD / ?ㅻ쾭?덉씠 吏꾩엯?먯쓽 ?쒓컖 ?꾩꽦???뺣━

**???④퀎媛 以묒슂???댁쑀**
- `Phase 6`??湲곕뒫?곸쑝濡??꾨즺?먯?留? ?꾩옱 ?붾㈃? "?고????뺣낫媛 ?덇릿 ?쒕뜲 ?쒕늿???ㅼ뼱?ㅼ? ?딅뒗" 援ш컙???⑥븘 ?덈떎.
- 寃利?Phase濡??섏뼱媛湲??꾩뿉 ?뺣낫 ?꾧퀎瑜?諛붾줈?≪븘???댄썑 diagnostics, benchmark, export 鍮꾧탳 ?붾㈃??媛숈? ?⑦꽩 ?꾩뿉???덉젙?곸쑝濡??뺤옣?쒕떎.

**寃뚯씠??*
- 100% 諛?125% 諛곗쑉?먯꽌 ?곷떒 HUD 1李?2李??뺣낫媛 ?쒕늿??援щ텇??
- 寃쎄퀬/諛곕꼫/?좎뒪?멸? ?쇰컲 ?몄쭛 ?곹깭? 紐낇솗??援щ텇?섍퀬 罹붾쾭???꾩뿉??臾삵엳吏 ?딆쓬
- 湲곕낯 ?몄쭛 ?붾㈃?먯꽌 罹붾쾭??吏묒쨷?꾧? ?좎??섍퀬, Inspector???몄쭛 ?뺣낫? 吏꾨떒 ?뺣낫媛 ?욎뿬 蹂댁씠吏 ?딆쓬
- ???罹≪쿂 ?명듃(湲곕낯 Run / heatmap / live probe / buffer overlay / warning state) 湲곗??쇰줈 ?뺣낫 ?꾨떖?μ씠 ?쇨???

**寃利?硫붾え (2026-03-08)**
- `build_check.bat` 湲곗? `EXIT_CODE=0`
- ?붾쾭洹??ㅽ뻾李?湲곕낯 Run 罹≪쿂濡??곷떒 HUD, ?좉? ?곹깭 ?쒖떆, runtime overlay 諛곗튂 ?뺤씤

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

### ?몃? ?쒖뼱 ?뚯뒪 / ?붾컮?댁뒪 ?꾨줈??

**UI ?덉씠?꾩썐 珥덉븞**

- ?쇱そ Input Rail: ?먮뵒???꾩껜 ??쓽 12% - 14%
- ?ㅻⅨ履?Output Rail: ?먮뵒???꾩껜 ??쓽 12% - 14%
- ?섎떒 Control Rail: ?먮뵒???꾩껜 ?믪씠??14% - 18%
- 以묒븰 罹붾쾭?ㅻ뒗 ?섎㉧吏 怨듦컙???좎??섎ŉ, 湲곕낯 DSP ?몄쭛 ?곸뿭?쇰줈 ?ъ슜
- 洹몃옒???몄쭛??蹂듭옟?댁쭏 ??罹붾쾭?ㅺ? 怨듦컙???ㅼ떆 ?뺣낫?????덈룄濡?媛?Rail? ?묎린/?쇱튂湲곌? 媛?ν빐????

**鍮꾩＜???몄뼱**

?쇰컲 ?몃뱶 ?ы듃???대? 洹몃옒??諛곗꽑???꾪븳 ?묎퀬 以묐┰?곸씤 ?뚯폆 ?뺥깭瑜??좎?
Rail ?ы듃???쇰컲 ?몃뱶 ?蹂대떎???섎뱶?⑥뼱 寃쎄퀎 而ㅻ꽖?곗쿂??蹂댁뿬????
湲곕낯 ?곹깭?먯꽌??Rail?????뚯폆 ?ㅽ??쇱쓽 ?붾뱶?ъ씤?몃줈 蹂댁뿬???섎ŉ, ?쒕옒洹??쒖뿉??湲곗〈 耳?대툝 ?뚮뜑留?諛⑹떇???ъ궗??
耳?대툝 ?먭퍡???泥대줈 ?쇨??섍쾶 ?좎??섍퀬, 李⑥씠????援듦린蹂대떎???ы듃 ?ㅻ（?? ?щ’ 諛곌꼍, 而ㅻ꽖??罹??ㅽ꽣釉??뺥깭濡??쒗쁽

**Rail蹂??ы듃 ?뺥깭 洹쒖튃**

Input Rail / Output Rail: ?꾨젅?꾧낵 ?뚮몢由??쒗쁽????媛뺥븳 罹≪뒓???먮뒗 諛???삎 而ㅻ꽖??
Control Rail: Value, Gate, Trigger???묒? pill ?ы듃瑜?媛吏??뚯뒪 移대뱶
Expression ?뚯뒪??湲곕낯?곸쑝濡??⑥씪 Value ?ы듃 ?ъ슜
Footswitch ?뚯뒪??湲곕낯?곸쑝濡?Gate + Trigger ?ы듃 ?ъ슜
?ㅽ뀒?덉삤 ?ㅻ뵒???붾뱶?ъ씤?몃뒗 ?쒕줈 臾닿???????媛쒓? ?꾨땲???섎굹濡?臾띠씤 L/R 而ㅻ꽖?곗쿂??蹂댁뿬????

**?됱긽 諛⑺뼢**

Rail 諛곌꼍? 罹붾쾭?ㅻ낫????????吏꾪븯寃??댁꽌 ?쒖뒪???⑤꼸泥섎읆 ?쏀엳寃???
?ㅻ뵒??I/O 媛뺤“?? teal / cyan 怨꾩뿴
?곗냽 ?쒖뼱(Value) 媛뺤“?? warm yellow / amber 怨꾩뿴
Gate / Trigger 媛뺤“?? orange / red 怨꾩뿴
MIDI 怨꾩뿴 媛뺤“?? mint / lime 怨꾩뿴
Rail 而ㅻ꽖?곕뒗 ?대? ?몃뱶 ?ы듃蹂대떎 ??媛뺥븳 ?뚮몢由ъ? ?쎄컙 ??? 梨꾨룄瑜??좎?

**?덉젙??由ъ냼???ъ슜 諛⑹떇**

DPI ?ㅼ??쇰쭅, 以? ?곹깭 ?꾪솚??怨좊젮??Rail, ?ы듃, 耳?대툝? 肄붾뱶 湲곕컲 ?쒕줈?됱쓣 ?곗꽑 ?ъ슜
Input, Output, Expression, Footswitch, MIDI, Missing, Learn, Auto 諛곗????뚰삎 ?ъ궗???꾩씠肄?由ъ냼?ㅻ? 異붽?
鍮꾪듃留?湲곕컲 耳?대툝?대굹 ???μ떇???⑤꼸 諛곌꼍? 吏??
?대?吏 ?먯뀑???꾩슂?섎뜑?쇰룄 ?듭떖 ?명꽣?숈뀡 ?꾪삎???꾨땲???꾩씠肄? 諛곗?, 誘몃땲 湲由ы봽 ?섏??쇰줈 ?쒗븳

**2?④퀎 紐⑺몴**

??臾몄꽌 紐⑤뜽???뚮젅?댁뒪????곗씠?곕? ?ъ슜????媛쒖쓽 Rail???뚮뜑留?
珥덉븞 ?④퀎?먯꽌 理쒖냼??EXP 1, FS 1, 洹몃━怨??섎굹???ㅻ뵒???낅젰/異쒕젰 ?붾뱶?ъ씤?몃? ?쒖떆
?붾컮?댁뒪 媛먯???Learn 紐⑤뱶瑜??곌껐?섍린 ?꾩뿉 ?ㅽ겕由곗꺑?쇰줈 媛꾧꺽, 移대뱶 諛?? ?ы듃 媛?낆꽦??寃利?

**紐⑺몴**:
- ?몃? MIDI foot controller, expression pedal, switch ?낅젰??洹몃옒?꾩쓽 ?쒖뒪??寃쎄퀎濡??ㅻ（怨? preset/state 蹂듭썝 ?먮쫫怨??④퍡 ?덉젙?곸쑝濡???Β룹옱?곌껐?쒕떎.

**二쇱슂 ?묒뾽**:
- [ ] **Control Source Rail 紐⑤뜽 ?꾩엯**: 醫뚯륫 Input rail, ?곗륫 Output rail, ?섎떒 Control Source rail 援ъ“瑜?湲곗??쇰줈 ?쒖뒪??I/O? ?쇰컲 DSP ?몃뱶瑜?遺꾨━
- [ ] **?숈쟻 ?μ튂 媛먯?**: ?곌껐???몃? 而⑦듃濡??μ튂瑜?媛먯??섍퀬, ?낅젰 ?대깽?몃? 諛뷀깢?쇰줈 ?꾩떆 control source瑜??먮룞 ?앹꽦
- [ ] **learn + confirm ?깅줉 ?먮쫫**: ?ъ슜?먭? ?섎떖/?ㅼ쐞移섎? ?吏곸뿬 `EXP`, `FS`, `Trigger` source瑜??뺤젙?섍퀬 ?대쫫, ??? momentary/toggle 紐⑤뱶瑜?蹂댁젙
- [ ] **device profile persist**: ?μ튂蹂?source 援ъ꽦, ?쒖떆 ?대쫫, binding ?뺣낫瑜?profile濡???ν븯怨??ъ뿰寃????덉젙?곸쑝濡?蹂듭썝
- [ ] **preset/state ?곕룞 蹂듭썝**: 臾몄꽌 濡쒕뱶, preset ?꾪솚, crash recovery ?댄썑?먮룄 control source assignment? device mapping???쇨??섍쾶 蹂듭썝?섎룄濡??곹깭 紐⑤뜽 ?뺣━
- [ ] **fallback / partial recovery ?뺤콉**: ?μ튂媛 ?녾굅??source ?쇰?媛 ?꾨씫??寃쎌슦 unassigned, degraded, relink-needed ?곹깭瑜?紐낆떆?곸쑝濡??좎?
- [ ] **assignment inspector 怨꾩빟 ?뺣━**: `Value`, `Gate`, `Trigger` 媛숈? source 異쒕젰 ??낃낵 target parameter binding 洹쒖튃??紐낅Ц??
- [ ] **寃利???ぉ 異붽?**: device reconnect, profile mismatch, preset reload, missing controller ?곹솴?????state recovery ?뚯뒪??異붽?

?꾨즺 湲곗?:
- ???foot controller / expression ?μ튂瑜??곌껐?덉쓣 ??source媛 ?먮룞 媛먯??섍퀬, ?ъ슜?먭? learn?쇰줈 ?섎?瑜??뺤젙?????덈떎.
- ??????ъ떎?됲븯嫄곕굹 ?μ튂瑜??ъ뿰寃고빐??source profile怨?assignment媛 ?쇨??섍쾶 蹂듭썝?쒕떎.
- ?μ튂媛 ?꾨씫?섍굅??援ъ꽦???щ씪?몃룄 臾몄꽌媛 源⑥?吏 ?딄퀬 degraded ?곹깭濡?蹂듦뎄?쒕떎.

**諛붾줈 ?댁뼱??UI**
- UI `Phase 8`
  - Preset Browser
  - ?몃뱶 ?곹깭 ?ㅻ깄??
  - 蹂寃?鍮꾧탳 蹂닿린
  - dirty state ?쒖떆
  - crash recovery ??붿긽??
  - 異⑸룎 ?닿껐 ?먮쫫
  - migration 寃쎄퀬 諛곕꼫
  - deprecated/alias ?쒖떆
  - 蹂듦뎄 媛?μ꽦 ?깃툒

**???④퀎媛 以묒슂???댁쑀**
- ?곸슜 ?댁뿉???ъ슜?먭? 媛???レ뼱?섎뒗 寃껋? ?곗씠???먯떎?대떎.
- 寃利??명봽?쇨? 以鍮꾨맂 ?ㅼ뿉 preset/migration???뱀뼱???щ㎎ 蹂寃쎌쓣 ?덉쟾?섍쾶 ?댁쁺?????덈떎.

**寃뚯씠??*
- autosave 蹂듦뎄 寃쎈줈媛 ?ㅼ젣濡??숈옉
- 援щ쾭??臾몄꽌/?꾨━?뗭쓣 migration ???ㅼ떆 ?????덉쓬
- recovery/migration ?ㅽ뙣媛 ?ъ슜?먯뿉寃??ㅻ챸 媛?ν븳 ?곹깭濡??몄텧??

---

### 留덉씪?ㅽ넠 4: ?곸슜 ?꾩닔 ?몃뱶 理쒖냼??

**?곗꽑 援ы쁽???몃뱶**
- `SamplePlayer`
- `Compressor`
- `Limiter`
- `EnvelopeFollower`
- `Oscilloscope`
- `LevelMeter`

**?좎젙 ?댁쑀**
- `SamplePlayer`??asset/package 寃쎈줈瑜??ㅼ쟾?곸쑝濡?寃利앺븯???듭떖 ?몃뱶??
- `Compressor`, `Limiter`, `EnvelopeFollower`??誘뱀떛/紐⑤뱢?덉씠???ㅼ궗??洹몃옒?꾨? 留뚮뱾湲??꾪빐 ?꾩슂?섎떎.
- `Oscilloscope`, `LevelMeter`??verification/demo/support ?④퀎?먯꽌 愿痢??λ퉬 ??븷???쒕떎.

**???④퀎?먯꽌 ?섏? ?딆쓣 寃?*
- EQ 怨꾩뿴, modulation FX, MIDI ?좏떥, analyzer ?꾨?瑜???踰덉뿉 援ы쁽?섏? ?딅뒗??
- `ITNode` ?명꽣?섏씠??遺꾨━, ?숈쟻 ?몃뱶 濡쒕뱶??誘몃，??

**寃뚯씠??*
- ?곕え/?뚯뒪???⑦궎吏뺤뿉 ?????洹몃옒???명듃瑜????몃뱶留뚯쑝濡?援ъ꽦 媛??
- asset ?ы븿 洹몃옒?꾩? dynamics 洹몃옒?꾨? 紐⑤몢 寃利앺븷 ???덉쓬

---

### 留덉씪?ㅽ넠 5: Asset & Packaging

**湲곕뒫 癒쇱?**
- 湲곕뒫 `Phase 9`
  - asset manifest
  - bundle / relative path ?뺤콉
  - missing asset relink flow
  - RuntimeModule package layout
  - deterministic packaging

**諛붾줈 ?댁뼱??UI**
- UI `Phase 9`
  - Asset Browser
  - ?섏〈??留?
  - unused/missing ?쒖떆
  - Missing Asset Wizard
  - ?뺤콉 ?좏깮 UI
  - 蹂듦뎄 濡쒓렇
  - Package Preview
  - portable health check
  - README/manifest 誘몃━蹂닿린

**???④퀎媛 以묒슂???댁쑀**
- ?곸슜 ?ъ슜?먮뒗 洹몃옒?꾨쭔 ??ν븯吏 ?딅뒗?? ?먯궛源뚯? ?④퍡 ??꺼???쒕떎.
- `SamplePlayer` ???몃? ?뚯씪 湲곕컲 ?몃뱶媛 ?ㅼ뼱?ㅻ뒗 ?쒓컙, packaging怨?relink媛 ?쒗뭹 ?꾩꽦?꾩쓽 ?듭떖???쒕떎.

**寃뚯씠??*
- ?꾨씫 ?먯궛???ъ뿰寃고븷 ???덉쓬
- export package媛 ?ㅻⅨ 寃쎈줈/癒몄떊?먯꽌???ы쁽 媛??
- package 寃곌낵媛 deterministic?섍쾶 ?좎???

---

### 留덉씪?ㅽ넠 6: Host Integration

**湲곕뒫 癒쇱?**
- 湲곕뒫 `Phase 10`
  - host template ?쒓났
  - APVTS attachment ?덉젣
  - plugin wrapper 媛?대뱶
  - latency / tail / bus metadata export
  - integration smoke project

**諛붾줈 ?댁뼱??UI**
- UI `Phase 10`
  - export 留덈쾿??
  - ?ъ쟾 寃利??붿빟
  - ?곗텧臾??붿빟 ?붾㈃
  - ?몄뒪???쒗뵆由??곗쿂
  - Projucer/CMake ?몄엯 媛?대뱶
  - ?뚮씪誘명꽣 留ㅽ븨 誘몃━蹂닿린
  - post-export smoke 踰꾪듉
  - integration checklist
  - copy-ready snippet

**???④퀎媛 以묒슂???댁쑀**
- export媛 ?쒕떎? 諛붾줈 ?쒗뭹??遺숈씪 ???덈떎???꾪? ?ㅻⅤ??
- ???④퀎?먯꽌 ?쒖깮?깅Ъ?앹쓣 ?쒗넻??媛?ν븳 而댄룷?뚰듃?앸줈 ?뚯뼱?щ┛??

**寃뚯씠??*
- ?섑뵆 host project??generated module???ㅼ젣濡??몄엯 媛??
- 理쒖냼 ?섎굹 ?댁긽??integration smoke媛 ?먮룞 寃利앸맖
- ?ъ슜???낆옣?먯꽌 export ???ㅼ쓬 ?됰룞??臾몄꽌/留덈쾿?щ줈 諛붾줈 ?곌껐??

---

### 留덉씪?ㅽ넠 7: Team Workflow & CLI

**湲곕뒫 癒쇱?**
- 湲곕뒫 `Phase 11`
  - headless CLI
  - deterministic codegen
  - machine-readable report
  - debug trace / inspector
  - sample project / cookbook

**諛붾줈 ?댁뼱??UI**
- UI `Phase 11`
  - ?묒뾽 ?쇳꽣
  - ?꾨줈?????
  - 諛곗튂 ?≪뀡
  - JSON report 酉곗뼱
  - ?ㅽ뻾 ?대젰 鍮꾧탳
  - ?ы쁽 ?⑦궎吏 ?앹꽦
  - cookbook 釉뚮씪?곗?
  - 洹쒖빟 泥댄겕 諛곗?
  - 由щ럭???붿빟 ?⑤꼸

**???④퀎媛 以묒슂???댁쑀**
- ?쇱옄 ?곕뒗 ?댁씠 ?꾨땲???怨?CI媛 ?④퍡 蹂대뒗 ?쒗뭹?쇰줈 ?섏뼱媛??援ш컙?대떎.
- ???④퀎媛 ?섏뼱???洹쒕え ?뚭? 寃利? repro bundle, ? 由щ럭媛 ?덉젙?곸쑝濡?援대윭媛꾨떎.

**寃뚯씠??*
- CLI留뚯쑝濡?validate/export/benchmark 媛??
- report媛 ?щ엺怨?CI ?????쎌쓣 ???덉쓬
- 踰꾧렇 ?ы쁽??bundle ?앹꽦??媛??

---

### 留덉씪?ㅽ넠 8: Commercial Release Finish

**湲곕뒫 癒쇱?**
- 湲곕뒫 `Phase 12`
  - release QA matrix
  - support bundle / crash log
  - licensing / edition ?뺤콉
  - 臾몄꽌 / ?쒗넗由ъ뼹 / ?⑤낫??
  - release checklist / LTS ?뺤콉

**諛붾줈 ?댁뼱??UI**
- UI `Phase 12`
  - ?ㅻ낫???꾩쟾 ?먯깋
  - 怨좊?鍮??됯컖 蹂댁젙 ?뚮쭏
  - 諛곗쑉/?댁긽???곸쓳??
  - first-run onboarding
  - 鍮??곹깭/?ㅽ뙣 ?곹깭 臾멸뎄 ?뺣퉬
  - 臾몄꽌/?덉젣 吏곴껐 留곹겕
  - support bundle ?앹꽦湲?
  - 由대━??readiness ??쒕낫??
  - edition/license 吏꾩엯??

**???④퀎媛 以묒슂???댁쑀**
- ?댁젣遺?곕뒗 湲곕뒫 異붽?蹂대떎 諛고룷 ??臾몄젣瑜??쇰쭏??以꾩씪 ???덈뒓?먭? 以묒슂?섎떎.
- ?묎렐?? 臾몄꽌, 吏??踰덈뱾, 由대━???댁쁺 湲곗????놁쑝硫??곸슜 ?쒗뭹?쇰줈 ?ㅻ옒 ?좎??섍린 ?대졄??

**寃뚯씠??*
- ?좉퇋 ?ъ슜?먭? 泥??ㅽ뻾遺??export源뚯? 湲몄쓣 ?껋? ?딆쓬
- 吏???붿껌???꾩슂???뺣낫媛 ?먮뵒???대??먯꽌 ?섏쭛 媛??
- ?묎렐??怨좊같???ㅽ뙣 ?곹깭 UX媛 ?쒗뭹 ?섏??쇰줈 ?뺣━??

---

## 1.0 ?댁쟾??誘몃（吏 留먯븘????寃?

- Runtime safety? verification infrastructure
- autosave / crash recovery
- asset relink / deterministic packaging
- host integration smoke
- CLI validate/export 寃쎈줈
- support bundle / crash log

????ぉ?ㅼ? ?쒖엳?쇰㈃ 醫뗭? 湲곕뒫?앹씠 ?꾨땲???곸슜 諛고룷 ??臾몄젣 鍮꾩슜??以꾩씠???꾩닔 湲곕컲?대떎.

---

## 1.0 ?댄썑濡?誘몃쨪???섎뒗 寃?

### ?⑥? ?몃뱶 移댄깉濡쒓렇 ?뺤옣
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

### ?뺤옣 ?앺깭怨???ぉ
- `ITNode` ?명꽣?섏씠??遺꾨━
- ?숈쟻 ?몃뱶 濡쒕뱶

????ぉ?ㅼ? ?쒗뭹 媛移섎? ?믪씠吏留? 吏湲?諛붾줈 ?ㅼ뼱媛硫?ABI/臾몄꽌/吏??踰붿쐞媛 湲됯꺽??而ㅼ쭊??

---

## 理쒖쥌 沅뚯옣 ?쒖꽌 ?붿빟

1. UI ?붿뿬 ?꾨━酉?3媛?留덇컧
2. 湲곕뒫 `Phase 6`
3. UI `Phase 6`
4. UI `Phase 6` polish (`留덉씪?ㅽ넠 1.5`)
5. 湲곕뒫 `Phase 7`
6. UI `Phase 7`
7. 湲곕뒫 `Phase 8`
8. UI `Phase 8`
9. ?곸슜 ?꾩닔 ?몃뱶 理쒖냼??異붽?
10. 湲곕뒫 `Phase 9`
11. UI `Phase 9`
12. 湲곕뒫 `Phase 10`
13. UI `Phase 10`
14. 湲곕뒫 `Phase 11`
15. UI `Phase 11`
16. 湲곕뒫 `Phase 12`
17. UI `Phase 12`
18. ?⑥? ?몃뱶 移댄깉濡쒓렇 / ?몃? ?뚮윭洹몄씤 ABI??1.0 ?댄썑 寃??

---

## ?ㅽ뻾 ?먯튃 ??以??붿빟

> `?덉쟾??-> 寃利?-> 蹂듦뎄 -> ?꾩닔 ?몃뱶 -> ?⑦궎吏?-> ?듯빀 -> ? ?댁쁺 -> ?곸슜 留덇컧` ?쒖꽌濡?媛꾨떎.

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