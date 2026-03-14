# Teul2 (틀 2) — 통합 리팩토링 로드맵

> **역할**: 차세대 노드 기반 DSP 그래프 엔진. `Teul`의 레거시 구조를 해체하고, `Document-Editor-Runtime-Bridge` 4계층 아키텍처로 재구축하여 확장성과 안정성을 극대화한다.

---

## 🏗️ 아키텍처 현황 (Architecture Status)

Teul2는 기존의 단일 God File 구조를 탈피하여 책임이 명확히 분리된 4개의 레이어로 구성되어 있습니다.

| 레이어 | 폴더 | 상태 | 주요 역할 |
| :--- | :--- | :---: | :--- |
| **Document** | `Source/Teul2/Document` | **완료** | 그래프 데이터 진실의 원천, 히스토리, 직렬화, 마이그레이션 |
| **Editor** | `Source/Teul2/Editor` | **이전 완료** | 시각적 편집기, 전용 렌더러, 패널, 인터랙션 시스템 |
| **Runtime** | `Source/Teul2/Runtime` | **진행 중** | `AudioGraph`(실행)와 `IOControl`(제어)로 분리된 실시간 엔진 |
| **Bridge** | `Source/Teul2/Bridge` | **초기 구축** | 외부 시스템(Ieum, Gyeol, Naru 등) 연동 및 코드 생성 |

---

## 📂 폴더별 상세 상태 및 보완점

### 1. Document (데이터 레이어)
- **이전 내용**: `Model`, `History`, `Serialization` 로직 통합 흡수.
- **현재 상태**: 가장 안정적인 레이어. `TDocumentTypes`를 통해 모든 데이터 구조가 정규화됨.
- **보완/향후 과제**:
    - [ ] 대규모 그래프 저장을 위한 바이너리 직렬화 옵션 검토.
    - [ ] 협업을 위한 부분 그래프(Fragment) 비교 및 병합 로직 강화.

### 2. Editor (UI 레이어)
- **이전 내용**: `EditorHandleImpl` 해체 후 기능별 하위 서브시스템 분리.
- **현재 상태**: 구조적 이전 완료. `Canvas`, `Renderers`, `Interaction`, `Search` 등 하위 폴더 체계 확립.
- **보완/향후 과제**:
    - [ ] **Rail UX 도입**: 좌/우/하단 고정 입출력 레일 영역 구축 (현재 일반 노드와 혼용 중).
    - [ ] **Diagnostics 통합**: 런타임 성능 및 에러 정보를 캔버스 위에 히트맵/배지로 시각화.
    - [ ] **Wizard UI**: Export 및 환경 설정을 위한 단계별 마법사 UI 제공.

### 3. Runtime (실행 레이어)
- **이전 내용**: `TGraphRuntime`에서 DSP 실행부와 입출력 제어부 분리.
- **현재 상태**: 
    - **AudioGraph**: 핵심 노드(CoreNodes) 대부분 이식 완료. 컴파일러 안정화 단계.
    - **IOControl**: 현재 집중 개발 영역. 디바이스 매니저 및 이벤트 큐 최적화 중.
- **보완/향후 과제**:
    - [ ] **Hardenning**: 런타임 재빌드 시 락-프리 안전성 및 메모리 정적 할당 강화.
    - [ ] **Validation**: 노드별 개별 검증 도구(`TRuntimeValidator`)를 Teul2 전용으로 고도화.

### 4. Bridge (연동 레이어)
- **이전 내용**: `Export` 및 전역 브리지 로직 이전.
- **현재 상태**: 기초 설계 완료. `TIeumBridge` 등 핵심 연동부 구축 중.
- **보완/향후 과제**:
    - [ ] **Codegen 고도화**: C++ 코드 추출 시 최적화(Node Fusion, SIMD) 반영.
    - [ ] **Naru/TGyeol 연동**: 각 외부 프로젝트 전용 어댑터 완성.
    - [ ] **Asset Packaging**: 외부 샘플/IR 파일을 포함한 원클릭 패키징 기능.

---

## 🎯 Phase 2 & 3: 차세대 기능 목표

리팩토링 안정이 완료된 후 도입할 고급 기능들입니다. (Teul Roadmap 1, 2 계승)

### Phase 2: 성능 및 품질 고도화
- **Polyphony**: 보이스 매니저 및 보이스별 그래프 인스턴스화.
- **Oversampling**: 노드/그룹 단위 업샘플링 인프라 구축.
- **High-Quality DSP**: ZDF 필터 및 Anti-aliasing 오실레이터 추가.

### Phase 3: 상용화 준비 (Commercial Readiness)
- **CLI Tooling**: 헤드리스 환경에서의 검증 및 내보내기 도구.
- **Asset Relink**: 누락된 자산을 찾고 재연결하는 마법사 UI.
- **Release Matrix**: 플랫폼별/포맷별 자동 검증 파이프라인.

---

## 🛠️ 작업 가이드라인 (Dev Rules)

1. **Top-Down 구조 준수**: 모든 기능은 반드시 4개 레이어 중 적절한 위치에 배치한다.
2. **Editor-Runtime 분리**: Editor는 Runtime의 내부를 직접 수정하지 않고, Interface/Event를 통해 소통한다.
3. **Document Source of Truth**: 모든 문서 변경은 `TTeulDocument`를 통해 처리하며, 하위 시스템은 이를 관찰(Observe)한다.
4. **No Ellipsis**: 코드 수정 시 함수 전체를 완성된 형태로 제공한다.

---

**Last Updated**: 2026-03-14
**Status**: Architecture Refactoring (Phase 1 Final Stage)
