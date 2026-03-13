# Ieum Planned File Structure (Teul2 Aligned)

이 문서는 `Teul2`의 4대 모듈 아키텍처(`Document`, `Editor`, `Runtime`, `Bridge`)를 계승하여 설계된 `Ieum` 바인딩 플랫폼의 최종 파일 구조를 정의합니다.

---

## 📂 Root: `Source/Ieum/`
- `Ieum.h`: 전체 시스템을 사용하는 엔트리 포인트 (Umbrella Header).

### 1. 📂 `Document/` (Single Source of Truth)
*데이터 모델, 상태 관리, 직렬화 담당*
- `TIeumDocument.h / .cpp`: 바인딩 시스템의 전체 상태를 소유하는 오케스트레이터.
- `TBindingTypes.h`: `BindingId`, `ProviderId`, `ValueType` 등 핵심 타입과 전역 상수.
- `TBindingSpec.h`: 개별 바인딩의 설정 데이터 구조.
- `TIeumDocumentHistory.h / .cpp`: 바인딩 생성/삭제/수정에 대한 Undo/Redo 이력 관리.
- `TIeumSerializer.h / .cpp`: `.ieum` JSON 직렬화 및 스키마 이관(Migration) 로직.

### 2. 📂 `Editor/` (User Experience)
*사용자 인터페이스, 시각화, 상호작용 담당*
- `TIeumEditor.h / .cpp`: 에디터 최상위 컴포넌트 및 레이아웃 제어.
- `Canvas/`: 바인딩 관계를 시각적으로 보여주는 그래프/리스트 캔버스.
- `Panels/`: `SourceBrowser`, `TargetBrowser`, `BindingInspector` 등 사이드바 패널.
- `Renderers/`: 바인딩 상태 배지 및 연결선 전용 렌더링 로직.
- `Theme/`: Ieum만의 고유한 컬러 팔레트 및 디자인 시스템.

### 3. 📂 `Runtime/` (Live Execution)
*실시간 바인딩 실행 및 데이터 전송 담당*
- `TIeumRuntime.h / .cpp`: 런타임 엔진의 진입점.
- `TBindingResolver.h / .cpp`: 저장된 ID를 실시간 핸들로 리졸빙 및 유효성 검사.
- `TTransformPipeline.h / .cpp`: 변환 로직(Scale, Map 등) 연산 처리.
- `TDispatcher.h / .cpp`: **`juce::AbstractFifo`** 기반의 Lock-free 오디오 스레드 전송 레이어.
- `THealthMonitor.h / .cpp`: 런타임 안정성 감지 및 진단 데이터 수집.

### 4. 📂 `Bridge/` (External Connectivity)
*외부 프로토콜 계약 및 코드 생성 담당*
- `TIeumBridge.h / .cpp`: 외부 제품(`Gyeol`, `Teul2`)과의 연동 및 어댑터 관리.
- `IProviderContracts.h`: `ISourceProvider`, `ITargetProvider` 인터페이스 정의.
- `TCodegen.h / .cpp`: 바인딩 구성을 C++ 소스 코드로 변환하는 엔진 (Export 지원).
- `Adapters/`: 각 제품별 전용 어댑터 (`TGyeolAdapter`, `TTeulAdapter` 등).

---

## 🛠️ Module Mapping Summary

| 모듈 | Teul2 대응 | 핵심 책임 | 주요 클래스 |
| :--- | :--- | :--- | :--- |
| **Document** | `Document/` | 데이터 및 이력 | `TIeumDocument` |
| **Editor** | `Editor/` | 시뮬레이션 및 편집 | `TIeumEditor` |
| **Runtime** | `Runtime/` | 실시간 바인딩 실행 | `TIeumRuntime` |
| **Bridge** | `Bridge/` | 외부 연동 및 Codegen | `TIeumBridge` |
