# Ieum Planned File Structure

이 문서는 `Ieum Commercial Roadmap`을 기반으로 설계된 `Ieum` 바인딩 플랫폼의 타겟 파일 구조를 정의합니다. `Teul2` 및 `Gyeol`의 모듈화 원칙을 계승하며, 데이터 전송의 실시간성(Real-time safety)과 상용 수준의 진단 기능을 확보하는 데 최적화되어 있습니다.

---

## 📂 Root: `Source/Ieum/`

### 1. 📂 `Common/` (Shared Foundation)
*핵심 도메인 모델과 상태 정의 (Roadmap Phase 1~2)*
- `IeumTypes.h`: `BindingId`, `ProviderId`, `ValueType` 등 핵심 ID 및 타입 정의.
- `IeumStatus.h`: `ok`, `missing`, `degraded`, `invalid` 상태 모델 및 상세 이유(StatusDetails).
- `IeumConstants.h`: 엔진 전역 상수 및 설정 값.

### 2. 📂 `Document/` (Persistence & Specs)
*바인딩 명세와 직렬화 처리 (Roadmap Phase 2, 7)*
- `TBindingDocument.h / .cpp`: 바인딩 명세(`BindingSpec`)들의 컬렉션 및 문서 최상위 모델.
- `TBindingSpec.h`: 개별 바인딩의 설정값(SourceRef, TargetRef, Mode, Enabled 등).
- `IBindingSerializer.h / .cpp`: `.ieum` (또는 프로젝트 통합형) JSON 직렬화 및 스키마 이관(Migration).
- `TPresetAdapter.h`: 프리셋 시스템과의 인터페이스.

### 3. 📂 `Runtime/` (Binding Engine)
*바인딩 실행 및 오디오 스레드 전송 (Roadmap Phase 3, 4, 5)*
- `TBindingEngine.h / .cpp`: 전체 바인딩 가동을 총괄하는 지휘자 (Orchestrator).
- `TBindingResolver.h / .cpp`: 식별자(ID)를 실제 핸들(Handle)로 연결하고 유효성 검사 수행.
- `TTransformPipeline.h / .cpp`: 변환 로직(`TransformChain`) 실행 및 결과 계산.
- `TRuntimeDispatcher.h / .cpp`: **`juce::AbstractFifo`** 기반의 오디오 스레드 값 전송 팀.
- `TTransformRegistry.h`: 확장 가능한 변환기(Transform) 등록 및 조회.

### 4. 📂 `Editor/` (Binding UX)
*에디터 GUI 및 브라우징 (Roadmap Phase 6, 8, 10)*
- `TIeumEditor.h / .cpp`: 바인딩 에디터 최상위 컴포넌트.
- `Panels/`:
    - `TBindingListPanel.h`: 현재 설정된 바인딩 목록 및 상태 표시.
    - `TSourceBrowserPanel.h`: 발행된 소스(Gyeol 위젯 등) 트리 브라우저.
    - `TTargetBrowserPanel.h`: 발행된 타겟(Teul 파라미터 등) 트리 브라우저.
    - `TBindingInspectorPanel.h`: 개별 바인딩의 상세 설정 및 트랜스폼 편집.
- `Visuals/`:
    - `TStatusBadgeRenderer.h`: 캔버스 상의 상태 배지 및 하이라이트 아이콘 렌더링.
    - `TConnectionOverlay.h`: 소스와 타겟 사이의 연결 관계 시각화 레이어.

### 5. 📂 `Providers/` (Integration Contracts)
*외부 시스템과의 연결 규약 (Roadmap Phase 1, 9)*
- `ISourceProvider.h`: 소스 발행 시스템(Gyeol)이 구현해야 할 인터페이스.
- `ITargetProvider.h`: 타겟 시스템(Teul)이 구현해야 할 인터페이스.
- `IeumRegistry.h`: 사용 가능한 프로바이더들의 등록 및 관리.

### 6. 📂 `Bridge/` (Codegen & Adapters)
*코드 생성 및 외부 제품 연동 (Roadmap Phase 6)*
- `TCodegenTemplates.h`: Export 시 생성될 C++ 브리지 코드의 템플릿.
- `TBridgeCompiler.h`: 바인딩 데이터를 런타임 최적화된 C++ 코드로 변환.
- `Adapters/`:
    - `TGyeolSourceAdapter.h`: Gyeol 위젯 이벤트를 Ieum으로 변환하는 전용 어댑터.
    - `TTeulTargetAdapter.h`: Ieum 값을 Teul 파라미터로 주입하는 전용 어댑터.

### 7. 📂 `Diagnostics/` (Operations)
*진단 및 모드 관리 (Roadmap Phase 10, 11)*
- `TBindingDiagnostics.h`: 런타임 오류 수집 및 레포팅 도구.
- `TIeumLog.h`: 구조화된 로그 기록 시스템.
- `THealthMonitor.h`: 전체 바인딩 시스템의 건강 상태 실시간 감지.

---

## 🛠️ Summary Matrix

| 폴더 | 담당 Phase | 핵심 클래스 | 비고 |
| :--- | :--- | :--- | :--- |
| **Common** | Phase 1-2 | `IeumStatus` | 모든 모듈이 의존하는 기초 |
| **Document** | Phase 2, 7 | `TBindingDocument` | 저장 및 로드 담당 |
| **Runtime** | Phase 3-5 | `TBindingEngine` | **실시간성(RT Safety)** 보장 로직 |
| **Editor** | Phase 6, 8 | `TIeumEditor` | 사용자 조작 레이어 |
| **Providers** | Phase 1, 9 | `ISourceProvider` | Gyeol/Teul 연동 규약 |
| **Bridge** | Phase 6 | `TCodegenTemplates` | C++ 코드 생성 지원 |
| **Diagnostics** | Phase 10 | `THealthMonitor` | 상용 수준의 안정성 확보 |
