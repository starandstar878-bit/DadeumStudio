# Widget Schema Reference (Phase 7 / v2)

## 문서 목적
- Phase 7 단계 0에서 정의한 위젯 등록 스키마를 필드 단위로 고정한다.
- 각 속성이 무엇을 지정하는지, 누가 채우는지(사용자/자동), 어떤 검증이 필요한지 명확히 한다.

## 분류 기준
- `Core`: 누락 시 등록 차단
- `Recommended`: 누락 시 등록 허용, Validation 경고 권장
- `Optional`: 필요 시 선택 적용
- `Generated`: 호스트/빌드 파이프라인이 생성 또는 보정

## 1) 정체성/버전 필드
- `schemaVersion` (Core): 매니페스트 스키마 버전. 예: `2.0.0`.
- `manifestVersion` (Core): 해당 위젯 선언 문서의 버전. 예: `1.3.2`.
- `widgetTypeVersion` (Optional): 위젯 타입의 내부 데이터 버전. 예: `3`.
- `migrationRules` (Optional/Generated): `widgetTypeVersion` 변경 시 데이터 이관 규칙.
- `typeKey` (Core): 위젯 타입 고유 키(문서/코드 공통 식별자). 예: `latte.imageButton`.
- `displayName` (Core): UI에 표시할 위젯 이름. 예: `Latte Image Button`.
- `category` (Core): 라이브러리 분류. 예: `Controls`.
- `tags` (Recommended): 검색/필터용 태그. 예: `button,image,latte`.
- `iconKey` (Recommended): 라이브러리 아이콘 식별자. 예: `icon_latte_button`.

## 2) 플러그인 식별/신뢰성 필드
- `pluginId` (Core): 플러그인 고유 식별자(역도메인 권장). 예: `com.dadeum.widgets.latte`.
- `pluginVersion` (Core): 플러그인 버전. 예: `1.2.0`.
- `vendor` (Recommended/Generated): 공급자 이름.
- `releaseChannel` (Recommended): 배포 채널. 예: `stable`, `beta`, `internal`.
- `publisherFingerprint` (Recommended/Generated): 발행자 인증 지문.
- `signature` (Recommended/Generated): 패키지/메타 서명값.

## 3) 호환성/플랫폼/ABI 필드
- `sdkMinVersion` (Core): 호스트 SDK 최소 버전.
- `sdkMaxVersion` (Generated): 호스트 SDK 메이저 기준 자동 보정 상한.
- `supportedHostVersions` (Recommended/Generated): 지원 호스트 버전 범위 목록.
- `platformTargets` (Recommended/Generated): 지원 플랫폼. 예: `windows,macos,linux`.
- `architectureTargets` (Recommended/Generated): 지원 아키텍처. 예: `x64,arm64`.
- `abiVersion` (Core): ABI 호환 버전.
- `abiHash` (Core/Generated): 공개 SDK 헤더 기반 ABI 해시.
- `threadingModel` (Recommended): 스레드 모델. 예: `main-thread`, `mixed`.
- `realtimeSafe` (Recommended): 실시간 컨텍스트 호출 안전 여부.
- `renderThreadOnly` (Recommended): 렌더 전용 스레드 호출 강제 여부.

## 4) 레이아웃/렌더/성능 필드
- `defaultBounds` (Core): 기본 배치 위치/크기. 예: `{x,y,w,h}`.
- `minSize` (Core): 최소 크기. 예: `{w,h}`.
- `painter` (Core): 렌더 진입점(내장/외부 위젯 등가 함수).
- `capabilities` (Recommended): 위젯 가능 기능 목록. 예: `acceptsAssetDrop`, `emitsRuntimeEvents`.
- `repaintPolicy` (Recommended): 리페인트 정책. 예: `onDemand`, `continuous`.
- `tickRateHintHz` (Recommended): 업데이트 주기 힌트(Hz).
- `supportsOffscreenCache` (Recommended): 오프스크린 캐시 사용 가능 여부.
- `estimatedPaintCost` (Recommended): 렌더 비용 추정치(상대값 또는 ms).
- `memoryBudgetKb` (Recommended): 예상 메모리 예산(KB).

## 5) 기본 상태/프로퍼티 스펙 필드
- `defaultProperties` (Core): 위젯 생성 시 기본 속성값 맵.
- `propertySpecs` (Core): 편집 가능한 속성 정의 배열.
- `propertySpecs[].key` (Core): 속성 내부 키. 예: `opacity`.
- `propertySpecs[].label` (Core): 속성 표시명. 예: `Opacity`.
- `propertySpecs[].kind` (Core): 속성 타입. 예: `bool`, `int`, `float`, `string`, `color`, `assetRef`.
- `propertySpecs[].defaultValue` (Core): 속성 기본값.
- `propertySpecs[].required` (Core): 값 필수 여부.
- `propertySpecs[].unit` (Recommended): 표시 단위. 예: `px`, `%`.
- `propertySpecs[].displayFormat` (Recommended): UI 표시 포맷. 예: `{value}%`.
- `propertySpecs[].valueCurve` (Recommended): 값 변화 곡선. 예: `linear`, `easeOut`.
- `propertySpecs[].min` (Recommended): 수치 최소값.
- `propertySpecs[].max` (Recommended): 수치 최대값.
- `propertySpecs[].step` (Recommended): 수치 증감 간격.
- `propertySpecs[].minLength` (Recommended): 문자열 최소 길이.
- `propertySpecs[].maxLength` (Recommended): 문자열 최대 길이.
- `propertySpecs[].regex` (Recommended): 문자열 정규식 제약.
- `propertySpecs[].enumOptions` (Recommended): 선택형 값 목록.
- `propertySpecs[].localeKey` (Optional): 로컬라이즈 텍스트 키.
- `propertySpecs[].hint` (Optional): 편집 힌트 문구.
- `propertySpecs[].advanced` (Optional): 고급 속성 표시 여부.
- `propertySpecs[].readOnly` (Optional): 읽기 전용 여부.
- `propertySpecs[].dependsOnKey` (Optional): 표시/활성 조건 대상 키.
- `propertySpecs[].dependsOnValue` (Optional): 표시/활성 조건 값.

## 6) 에셋 제약 필드
- `acceptedAssetKinds` (Optional): 허용 에셋 종류. 예: `image`, `font`, `audio`.
- `acceptedMimeTypes` (Optional): 허용 MIME 타입. 예: `image/png`.
- `maxAssetBytes` (Optional): 허용 최대 바이트.
- `fallbackAssetId` (Optional): 에셋 누락 시 대체 에셋 ID.
- `preloadAssets` (Optional): 사전 로딩 대상 에셋 목록.
- `supportsStreaming` (Optional): 스트리밍 로드 지원 여부.

## 7) 런타임 이벤트 필드
- `runtimeEvents` (Core): 런타임에서 발생 가능한 이벤트 정의 배열.
- `runtimeEvents[].key` (Core): 이벤트 내부 식별 키. 예: `onClick`.
- `runtimeEvents[].displayLabel` (Core): UI 표시 이벤트 이름. 예: `Click`.
- `runtimeEvents[].payloadSchema` (Core): 이벤트 페이로드 스키마(키/타입/필수항목).
- `runtimeEvents[].throttleMs` (Optional): 이벤트 최소 간격(ms).
- `runtimeEvents[].debounceMs` (Optional): 디바운스 지연(ms).
- `runtimeEvents[].reliability` (Optional): 전달 정책. 예: `bestEffort`, `guaranteed`.
- `runtimeEvents[].channel` (Optional): 이벤트 채널. 예: `ui`, `system`, `telemetry`.

## 8) 액션/상태 바인딩 필드
- `supportedActions` (Optional): 위젯이 지원하는 액션 타입 목록.
- `propertyBindings` (Optional): 런타임 상태와 속성 간 바인딩 정의.
- `stateInputs` (Optional): 위젯이 읽는 상태 키 목록.
- `stateOutputs` (Optional): 위젯이 쓰는 상태 키 목록.

## 9) 접근성/테스트/진단 필드
- `a11yRole` (Recommended): 접근성 역할. 예: `button`, `slider`.
- `a11yLabelKey` (Recommended): 접근성 라벨 로컬라이즈 키.
- `testId` (Recommended/Generated): 자동화 테스트 식별자.
- `diagnosticsContract` (Optional/Generated): 에러/경고 코드와 복구 힌트 규약.
- `telemetryTags` (Optional): 운영 관측 태그.

## 10) 상태/영속성 필드
- `statePolicy` (Optional): 상태 보존 정책. 예: `persisted`, `ephemeral`.
- `persistedKeys` (Optional): 저장 대상 상태 키 목록.
- `resetPolicy` (Optional): 리셋 동작 규칙.
- `migrationPolicy` (Optional): 버전 변경 시 상태 변환 정책.

## 11) 보안/권한 필드
- `permissions` (Core): 위젯이 요구하는 권한 목록(없으면 빈 배열).
- `sandboxLevel` (Optional): 샌드박스 수준. 예: `strict`, `balanced`, `off`.
- `fileAccess` (Optional): 파일 접근 권한 범위.
- `networkAccess` (Optional): 네트워크 접근 권한 범위.
- `midiAccess` (Optional): MIDI 접근 권한 범위.
- `scriptAccess` (Optional): 스크립트 실행 권한 범위.

## 12) Export/빌드 요구사항 필드
- `requiredJuceModules` (Recommended/Generated): 필요한 JUCE 모듈 목록.
- `requiredHeaders` (Recommended/Generated): 필요한 헤더 목록.
- `requiredLibraries` (Recommended/Generated): 필요한 링크 라이브러리 목록.
- `requiredCompileDefinitions` (Optional/Generated): 컴파일 정의 플래그.
- `requiredLinkOptions` (Optional/Generated): 링크 옵션.
- `codegenApiVersion` (Generated): Export 코드 생성 API 버전.
- `exportTargetType` (Recommended): Export 대상 타입. 예: `juce_component`.

## 등록 차단(Blocking) 검증 규칙
- `Core` 필드 누락 시 `WidgetRegistry::registerWidget`에서 즉시 실패 처리.
- `abiVersion` 또는 `abiHash` 불일치 시 로드 차단.
- `permissions` 위반 동작 감지 시 런타임 호출 거부 및 진단 이벤트 기록.
- `runtimeEvents[].payloadSchema`에서 `required` 키가 선언되면 실제 페이로드에도 동일 키 존재를 강제.

## 권장 경고(Non-blocking) 검증 규칙
- `Recommended` 누락 시 Validation 패널에 Warning 수준 리포트.
- 성능 힌트(`tickRateHintHz`, `estimatedPaintCost`, `memoryBudgetKb`) 누락 시 운영성 경고.
- 접근성 필드(`a11yRole`, `a11yLabelKey`) 누락 시 접근성 경고.

## 최소 등록 예시
```json
{
  "schemaVersion": "2.0.0",
  "manifestVersion": "1.0.0",
  "typeKey": "latte.imageButton",
  "displayName": "Latte Image Button",
  "category": "Controls",
  "pluginId": "com.dadeum.widgets.latte",
  "pluginVersion": "1.2.0",
  "sdkMinVersion": "1.0.0",
  "abiVersion": "1",
  "abiHash": "AUTO_OR_FIXED_HASH",
  "permissions": [],
  "defaultBounds": { "x": 100, "y": 100, "w": 160, "h": 48 },
  "minSize": { "w": 80, "h": 32 },
  "defaultProperties": { "text": "Buy" },
  "propertySpecs": [
    { "key": "text", "label": "Text", "kind": "string", "defaultValue": "Buy", "required": true }
  ],
  "runtimeEvents": [
    { "key": "onClick", "displayLabel": "Click", "payloadSchema": { "required": ["x", "y"] } }
  ],
  "painter": "LatteImageButtonPainter"
}
```