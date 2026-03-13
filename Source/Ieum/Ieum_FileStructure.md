# Ieum Planned File Structure (Teul2 Aligned)

이 문서는 `Teul2`의 아키텍처를 계승하되, `Ieum`만의 정체성을 담은 **`I` 접두사**를 사용하여 설계된 최종 파일 구조를 정의합니다.

---

## 📂 Root: `Source/Ieum/`
- `Ieum.h`: 전체 시스템 엔트리 포인트.

### 1. 📂 `Document/`
- `IeumDocument.h / .cpp`: 바인딩 시스템의 전체 데이터 및 상태 관리자.
- `IeumBindingTypes.h`: 핵심 타입(ID, ValueType 등) 및 상수.
- `IeumBindingSpec.h`: 개별 바인딩 설정 모델.
- `IeumDocumentHistory.h / .cpp`: Undo/Redo 이력 관리.
- `IeumSerializer.h / .cpp`: JSON 직렬화 및 마이그레이션.

### 2. 📂 `Editor/`
- `IeumEditor.h / .cpp`: 최상위 컴포넌트 및 UI 레이아웃.
- `Canvas/`: 바인딩 시각화 그래프/리스트 캔버스.
- `Panels/`: `SourceBrowser`, `TargetBrowser`, `BindingInspector` 등.
- `Renderers/`: 상태 배지 및 연결선 렌더링 로직.
- `Theme/`: Ieum 전용 디자인 시스템 (Palette/Style).

### 3. 📂 `Runtime/`
- `IeumRuntime.h / .cpp`: 런타임 실행 엔진 진입점.
- `IeumBindingResolver.h / .cpp`: ID to Handle 리졸빙 로직.
- `IeumTransformPipeline.h / .cpp`: 값 변환(Scale/Map) 연산.
- `IeumDispatcher.h / .cpp`: **`juce::AbstractFifo`** 기반 Lock-free 전송 레이어.
- `IeumHealthMonitor.h / .cpp`: 시스템 안정성 진단 및 수집.

### 4. 📂 `Bridge/`
- `IeumBridge.h / .cpp`: 외부 제품 연동 어댑터 관리자.
- `IeumProviderContracts.h`: `IeumSourceProvider`, `IeumTargetProvider` 인터페이스 정의.
- `IeumCodegen.h / .cpp`: C++ 소스 코드 생성 엔진.
- `Adapters/`: `GyeolAdapter`, `TeulAdapter` 등 제품별 어댑터.

---

## 🛠️ Module Mapping Summary

| 모듈 | 접두사 | 핵심 책임 | 주요 클래스 |
| :--- | :--- | :--- | :--- |
| **Document** | `Ieum` | 데이터 및 이력 | `IeumDocument` |
| **Editor** | `Ieum` | 편집 및 시뮬레이션 | `IeumEditor` |
| **Runtime** | `Ieum` | 실시간 실행 | `IeumRuntime` |
| **Bridge** | `Ieum` | 외부 연동 및 생성 | `IeumBridge` |
