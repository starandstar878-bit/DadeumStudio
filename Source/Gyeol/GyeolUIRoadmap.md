# Gyeol Editor UI 로드맵 (UI Roadmap)

## 1. 현재 UI 아키텍처

Gyeol Editor는 현재 캔버스(Canvas) 작업 공간을 중심으로 다양한 전문 패널과 툴바로 둘러싸인 형태로 구성되어 있습니다. 현재 UI는 미적인 요소보다는 기능적인 접근성에 중점을 둔 구조적인 형태를 띠고 있습니다.

### 1.1 핵심 구성 요소
*   **캔버스 컴포넌트 (CanvasComponent)**: 사용자가 시각적으로 위젯을 배치, 이동 및 상호작용하는 중앙 작업 공간입니다. 화면 확대/축소, 패닝(Pan), 스냅 맞춤, 다중 영역 선택 및 눈금자 오버레이 기능을 지원합니다.

### 1.2 서브 패널
*   **레이어 트리 패널 (LayerTreePanel)**: 그룹, 레이어, 위젯 간의 계층 구조를 관리합니다. 트리 뷰에서 순서를 변경하거나 숨김/잠금 상태를 전환할 수 있습니다.
*   **속성 패널 (PropertyPanel)**: 선택된 위젯의 속성(크기/위치, 색상, 텍스트 등)을 검사하고 편집합니다. `PropertyEditorFactory`를 통해 타입에 맞는 전문 편집기를 생성합니다.
*   **에셋 패널 (AssetsPanel)**: 이미지, 폰트, 색상 프리셋 등 프로젝트 내 에셋 리소스를 관리합니다. 에셋 드래그 앤 드롭을 지원합니다.
*   **위젯 라이브러리 패널 (WidgetLibraryPanel)**: 캔버스로 드래그하여 배치할 수 있는 사용 가능한 위젯(버튼, 슬라이더, 노브 등)의 팔레트를 제공합니다.
*   **이벤트 액션 패널 (EventActionPanel)**: 런타임 이벤트 트리거, 실행 액션, 변수(Property Paths) 바인딩 등 상호작용 로직을 설정합니다.
*   **히스토리 패널 (HistoryPanel)**: 실행 취소(Undo) / 다시 실행(Redo) 상태 스택을 표시합니다.
*   **검증 패널 (ValidationPanel)**: 누락된 에셋이나 잘못된 영역 등 씬의 잠재적 오류나 경고를 보여줍니다.
*   **성능 패널 (PerformancePanel)**: 런타임 진단 데이터 및 퍼포먼스 프로파일링 정보를 차트나 수치형으로 표시합니다.
*   **익스포트 미리보기 패널 (ExportPreviewPanel)**: 컴포넌트 익스포트 상태 및 요약 내용을 확인하는 유틸리티 패널입니다.

### 1.3 툴바 및 오버레이
*   **네비게이터 패널 (NavigatorPanel)**: 캔버스 전체 조감도(미니맵) 추적 및 줌/팬 제어 컨트롤을 제공합니다.
*   **그리드 스냅 패널 (GridSnapPanel)**: 그리드 오버레이 표시 여부와 격자 스냅 간격, 정렬 규칙 등을 설정합니다.

---

## 2. UI/UX 개선 로드맵 (프리미엄 개편)

Gyeol Editor를 단순한 기능성 툴에서 벗어나 **최첨단 프리미엄 크리에이티브 애플리케이션**으로 발전시키기 위해 포괄적인 미적, 구조적 개편이 필요합니다. 완벽한 반응형 렌더링, 세련된 다크 테마, 그리고 고급스러운 사용자 경험을 달성하기 위한 단계별 계획은 다음과 같습니다.

### Phase 1: 코어 디자인 시스템 및 LookAndFeel 개편 (V1)
*   **조화로운 컬러 팔레트 구축**: 기본 제공되는 밋밋한 색상을 배제하고, "Deep Dark" 테마를 도입합니다 (예: 캔버스 배경 `#181A1F`, 패널 배경 `#21252B`, 보더 라인 `#2C313C`, 강조색 `#5A9CFF`).
*   **"부드럽고 깔끔한(Soft & Clean)" 디자인 원칙 적용**:
    *   **둥근 모서리 (Rounded Corners)**: 날카로운 직각 대신 모든 패널 테두리, 버튼, 입력 필드에 적절한 모서리 둥글기(Radius: 6px ~ 8px)를 적용합니다.
    *   **절제된 선과 면 (Border-less & Whitespace)**: 딱딱한 경계선(Border)을 최소화하고, 배경색의 명도 차이(톤온톤)를 이용해 영역을 구분합니다. 요소 사이의 패딩(Padding)과 여백을 기존보다 1.5배 넓혀 답답함을 해소합니다.
    *   **부드러운 그림자 (Soft Drop Shadow)**: 플로팅 패널이나 스위치, 썸네일 뒤에 옅고 넓게 퍼지는 그림자 효과(Opacity 15% 이하, 넓은 Blur)를 주어 고급스러운 입체감을 줍니다.
*   **모던 타이포그래피 (Modern Typography)**: 기본 운영체제 폰트 대신 **Inter**, **Roboto** 또는 **Outfit**과 같은 선명하고 모던한 산세리프 폰트로 교체합니다. 타이틀과 본문 간의 폰트 굵기(Weight) 명도 대비를 확실히 줍니다.
*   **커스텀 JUCE LookAndFeel 제작**: `GyeolCustomLookAndFeel` 클래스를 만들어 슬라이더, 버튼, 스크롤바, 콤보박스 등 표준 JUCE 컴포넌트의 렌더링을 일관되게 덮어씁니다.

### Phase 2: 패널 레이아웃 및 심도(Depth) 적용 (V2)
*   **도킹(Docking) 및 워크스페이스 관리 개선**: 고정된 패널 구조를 유연한 도킹 시스템으로 전환합니다. 사용자는 패널을 분리, 도킹, 또는 접기/펼치기 형태로 자유롭게 뷰를 재구성할 수 있어야 합니다.
*   **글래스모피즘(Glassmorphism) 및 깊이감 부여**: 플로팅 패널이나 팝업, 컨텍스트 메뉴에 미묘한 그림자 효과와 반투명 블러(Blur) 효과를 적용하여 화면에 시각적 깊이(Depth)를 생성합니다.
*   **캔버스 렌더링 고도화**: 눈금자와 그리드가 줌인/줌아웃 비율에 맞춰 자연스럽게 페이드 되도록 업그레이드합니다. 단순한 테두리 표시에서 벗어나 선택 영역 리사이즈 핸들에 시인성 높은 하이라이트/자체 발광(Glow) 효과를 줍니다.

### Phase 3: 마이크로 애니메이션 및 상호작용성 강화 (V3)
*   **호버(Hover) 상태 트랜지션**: 속성 슬라이더나 트리 노드, 버튼 등의 상호작용 요소 위로 마우스를 올리면 색상이 즉시 바뀌는 대신 부드럽게 보간(Interpolate)되어 전환되게 합니다.
*   **팝업 애니메이션**: 컨텍스트 액션 팝업, 에스컬레이션 메뉴가 순간적으로 나타나지 않고, 약간의 확대(Scale up)와 페이드(Fade in) 효과로 부드럽게 등장하도록 처리합니다.
*   **동적 피드백 응답**: 속성이 성공적으로 변경되거나, 오류가 발생했을 때(예: 짧게 빛나는 초록색 펄스, 에러 시 은은한 붉은색 잔상 표시 등) 화면상에 즉각적 수시 피드백을 발생시킵니다.

### Phase 4: 프리미엄 폴리싱 단계
*   **에셋 썸네일 미리보기 프리뷰**: 에셋 패널과 위젯 라이브러리 패널의 구성을 텍스트 나열형 리스트뷰에서 자동으로 생성된 고품질 고해상도 시각적 썸네일로 전환합니다.
*   **빈 상태 (Empty States) 디자인**: 패널에 선택된 내역이 없거나 내용이 비어있을 때 미려한 일러스트레이션 그래픽이나 안내 아이콘을 삽입하여 시각적 황량함을 제거합니다.
*   **일체형 타이틀 바 (Custom Titlebar)**: OS(Windows) 기본 창 테두리를 제거하고 애플리케이션의 메인 테마와 부드럽게 이어지는 완벽한 커스텀 윈도우 타이틀바 영역을 구축합니다.

---

## 3. 현재 레이아웃 분석 및 개선점 (Screenshot Review)

제공된 실행 화면(Screenshot)을 바탕으로 분석한 현재 UI의 형태와 주요 개선 과제입니다.

### 3.1 상단 툴바 (Top Toolbar)
*   **분석**: "Add Button", "Add Knob", "Delete", "Group", "Dump JSON", "Run" 등 기능이 다른 버튼들이 텍스트 형태로 빽빽하게 1차원적으로 나열되어 있습니다. 이는 기능 식별에 대한 인지 부하를 높이고 시각적 피로감을 줍니다.
*   **개선점**: 
    1. 텍스트를 직관적인 **아이콘 기반 버튼**으로 교체.
    2. 기능을 논리적 그룹(예: 위젯 생성 그룹, 정렬/편집 그룹, 씬 관리/실행 그룹)으로 분리하고 여백(Separator)을 추가.

### 3.2 좌측 패널 (Left Panel - Layers, Library, Assets 등)
*   **분석**: 여러 탭(Layers, Library, Assets...)이 상단에 꽉 차 있고, 계층형 트리 뷰(Layer 구조)의 시인성이 다소 평면적(Flat)입니다. 노드 들여쓰기가 약하고 글씨가 작아 복잡한 씬에서는 구조 파악이 어려울 수 있습니다.
*   **개선점**: 
    1. 활성 탭과 비활성 탭의 명확한 시각적 대비 부여 (밑줄 하이라이트 등).
    2. 트리 뷰 인디케이터(Depth 가이드라인, 그룹 폴더 아이콘 등)를 적용하여 계층 구조 파악을 시각적으로 돕기.

### 3.3 중앙 캔버스 및 뷰포트 (Canvas)
*   **분석**: 다크 배경에 격자 모눈과 눈금자는 기능적으로 훌륭히 작동하고 있으나, 캔버스 위의 위젯 디자인(예: 슬라이더)과 선택/리사이즈 핸들이 투박한 JUCE 기본 스타일을 그대로 사용 중입니다.
*   **개선점**:
    1. 선택 바운딩 박스와 리사이즈 핸들에 은은한 Glow 발광이나 라운드 처리된 깔끔한 디자인 적용.
    2. 컴포넌트 자체(슬라이더, 콤보박스 등)에 모던한 커스텀 LookAndFeel 렌더러 적용을 목표.

### 3.4 우측 속성 검사기 (Right Panel - Inspector)
*   **분석**: Transform (X, Y, W, H), Appearance, Widget Properties 등의 데이터가 단순한 직사각형 입력 필드로 끝없이 이어져 있어, 마치 스프레드시트 폼을 보는 듯한 딱딱한 구조입니다. 
*   **개선점**: 
    1. 입력 필드의 높이와 패딩을 조절하여 숨 쉴 공간(Whitespace) 확보.
    2. 클릭해서 직접 타이핑하는 것을 넘어, 마우스 드래그로 수치를 조절할 수 있는 **스피너(Spinner/Drag Number)** 컨트롤 방식 도입.
    3. 체크박스 대신 스무스한 애니메이션이 들어간 **토글 스위치(Toggle Switch)** 형태 디자인 도입 검토.

### 3.5 하단 상태 바 (Bottom Status Bar)
*   **분석**: 하단 모서리에 "History" 및 "Undo 0 | Redo 0" 텍스트가 덩그러니 놓여 있어, 넓은 가로 공간을 제대로 활용하지 못하고 있습니다.
*   **개선점**: 
    1. 하단 바(Status Bar)의 시각적 경계를 명확히 하고, 왼쪽에는 현재 마우스 커서 위치의 좌표, 오른쪽에는 줌(Zoom) 비율 표시 및 컨트롤을 배치하여 공간 활용도를 높이기.

### 3.6 위젯 라이브러리 및 에셋 패널 (Widget Library & Assets - `page_0.png`)
*   **분석**: 두 패널 모두 기능 요소들이 단순 텍스트 박스와 회색 테두리의 버튼으로 나열되어 있어 전반적으로 무미건조(Flat)합니다. 에셋 패널의 경우 작은 이미지 뷰리어가 존재하긴 하나, 공간에 비해 정보의 가독성이나 심도(Depth)가 얕습니다.
*   **개선점**: 
    1. 둥글고 입체감 있는 카드(Card) 형태의 레이아웃을 도입하여 각 에셋/위젯 요소를 그리드 형태로 배치 (썸네일 위주 시각화).
    2. 요소 클릭 및 호버 시 색상이 변하거나 살짝 떠오르는 부드러운 애니메이션 스케일 적용.

### 3.7 그리드/스냅 및 네비게이터 패널 (Grid/Snap & Navigator - `page_1.png`)
*   **분석**: 체크박스와 텍스트 필드가 여백 없이 꽉 차게 들어가 있어 답답해 보입니다. 네비게이터 창 또한 단순한 원색 네모 모양만 표시됩니다.
*   **개선점**: 
    1. 체크박스를 macOS나 iOS 스타일의 **토글(Toggle) 스위치** 컨트롤로 변환.
    2. 네비게이터의 캔버스 썸네일에 부드러운 투명도와 함께 그라데이션, 그리고 캔버스의 현재 화면 비율(Zoom)과 포커스를 더 돋보이게 하는 Glow 효과 추가.

### 3.8 이벤트/액션 패널 (Event/Action - `page_2.png`)
*   **분석**: 바인딩된 리스트와 액션들이 마치 프로그래밍의 디버그 콘솔이나 엑셀 표를 보는 듯한 짙은 색의 콤보 박스들로 길게 이어져 있습니다. 시각적 단절선(Separator)이 없어 구조 인지가 떨어집니다.
*   **개선점**: 
    1. 액션과 이벤트 바인딩 항목을 개별적인 둥근 "블록(Block)" 디자인으로 감싸 시인성 강화.
    2. 액션 추가/삭제/이동 버튼을 콤팩트한 아이콘 그룹으로 모아 레이아웃 정돈.

### 3.9 검증, 익스포트 및 성능 패널 (Validation, Export, Performance - `page_3 ~ 5.png`)
*   **분석**: 이 세 패널은 대부분이 순수한 `텍스트 기반 로깅` 화면에 가깝습니다. 경고 표시 박스가 있긴 하지만 너무 글씨가 작고, 성능 텍스트는 완전히 평면적으로 나열된 수치 데이터일 뿐입니다.
*   **개선점**: 
    1. 검증 에러/경고 표시는 상태 메시지 옆에 느낌표 아이콘 등을 넣어 직관적으로 식별 가능하게 함.
    2. 성능 패널의 데이터들을 단순 텍스트가 아닌, 심플한 막대나 파이(Pie) 차트, 또는 실시간 꺾은선 그래프(Line Chart) 위젯으로 렌더링하도록 JUCE Paint 함수 오버라이딩 적용.

---

## 4. 최종 UI 레이아웃 설계본 (UI Design Specification)

고급형 그래픽 애플리케이션(Figma, Unreal Engine UI, Ableton 등)의 트렌드를 반영하여 각 패널의 역할에 맞는 가장 이상적인 위치와 렌더링 스타일을 설계했습니다.

### 4.1 전체 화면 프레임워크 (Macro Layout) & 반응형 크기 조절 (Responsive Resizing)
*   **유연한 스플리터(Splitter) 기반 레이아웃**: 모든 메인 도킹 영역(Left, Right, Bottom)과 캔버스 사이에는 드래그 가능한 디바이더(Resizer/Splitter)를 배치합니다. 사용자가 이를 드래그하여 각 패널의 너비와 높이 비율을 자유롭게 조절할 수 있습니다.
*   **반응형 사이즈 자동 조절 (Auto-Scaling)**: 전체 애플리케이션 창(Window)의 크기를 늘리거나 줄일 때, 설정된 비율(Flex Ratio)과 최소/최대 폭 제약(Min/Max Width) 규칙을 기반으로 패널들이 유기적으로 크로스-리사이징(Cross-Resizing) 되도록 구성합니다.
*   **Top Bar (상단 제어 패널)**: 높이 48px 고정. Gyeol 로고, 중앙에 파일명. 좌측엔 메인 툴 (선택, 이동), 우측엔 글로벌 액션 (포맷, 익스포트, 시뮬레이트 뷰, 실행). **그리드 및 스냅(Grid/Snap) 설정은 별도의 패널 창을 두지 않고, 상단 바 우측 아이콘 버튼을 클릭 시 나타나는 팝오버(Popover) 드롭다운 메뉴로 통합**하여 캔버스 공간을 확보. 기능적 그룹 간 세로선(Separator) 추가.
*   **Left Dock (좌측 통합 패널군)**: 기본폭 280px (최소 200px ~ 최대 400px 제약). 프로젝트 씬의 뼈대를 이루는 **계층 및 리소스 관리** 위주로 배치. 스플리터를 통해 내부 상/하 패널 분할 뷰어 지원 가능.
    *   `상단 탭`: [Layers] | [Assets] | [Widget Library]
*   **Right Dock (우측 통합 패널군)**: 기본폭 320px (최소 250px ~ 최대 500px 제약). 캔버스에서 **선택된 객체의 세부 정보 편집 및 반응형 동작 설정** 위주로 배치.
    *   `상단 탭`: [Inspector(Property)] | [Event/Action]
*   **Bottom Dock (하단 보조 패널 / 콘솔)**: 기본 높이 24px (확장 시 사용자가 드래그하여 ~ 전체 창의 40%까지 높이 조절 가능). 시스템 피드백 및 상태 표시 위주.
    *   기본 상태 (닫힘): 좌표계, 줌 레벨 표기 및 작은 에러 알림 뱃지 표기.
    *   확장 상태 (열림): [Validation] | [Performance] | [Export Preview] 탭 제공.
*   **Center Area (작업 캔버스)**: 뷰포트 내 남은 영역을 100% 동적으로 채움.

### 4.2 Left Dock 패널별 세부 설계
1.  **[Layers] 탭 (레이어 트리 패널)**
    *   **역할**: 씬의 z-order 및 그룹/부모-자식 구조 관리용.
    *   **스타일**: 폰트 사이즈를 소폭 키우고 라인 하이트(Line Height)를 늘려 여백 확보. 노드 옆에 계층을 잇는 희미한 선(Guide Line)과 함께 눈동자(Visible) 및 자물쇠(Lock) 아이콘을 모던한 플랫 디자인으로 교체하여 배치. 각 노드 호버 시 해당 라인 블록 전체가 하이라이트 되도록 처리.
2.  **[Assets] 탭 (에셋 패널)**
    *   **역할**: 이미지/컬러/폰트 탐색 및 관리.
    *   **스타일**: 리스트 뷰가 아닌, 최소 3열(가로로 3개)로 배치되는 **카드형 썸네일(Grid View)** 채택. 카드의 하단에 에셋 이름과 타입(Badge)이 미니멀하게 오버레이 됨.
3.  **[Widget Library] 탭 (위젯 라이브러리)**
    *   **역할**: 위젯 생성 및 드래그 앤 드롭 소스.
    *   **스타일**: 각 요소를 식별하기 쉬운 **2열 그리드의 벡터 아이콘**으로 큼직하게 렌더링. 마우스 호버 시 살짝 위로 떠오르는 스케일업(Scale-up) 모션과 파란색 Glow 테두리 적용.

### 4.3 Right Dock 패널별 세부 설계
1.  **[Inspector] 탭 (속성 검사기)**
    *   **역할**: 선택 객체의 기하학, 텍스트, 색상 속성을 편집.
    *   **스타일**: Section별로(가령 Transform, Appearance 등) 접고 펼칠 수 있는 아코디언(Accordion) 스타일 적용. 
    *   `Transform` 등의 수치 칸에 텍스트를 감추고 마우스 드래그를 통해 조절 가능한 **스피너 기능 추가**.
    *   `Visible/Locked` 등의 불리언 값은 네모 박스 대신, 스무스한 **스위치 토글(Toggle)** 로 변환.
2.  **[Event/Action] 탭 (이벤트/액션 연결)**
    *   **역할**: 인터랙티브 논리 설계.
    *   **스타일**: 노드(Node) 기반 비주얼 스크립팅의 축소판처럼 렌더링. 바인딩된 각 이벤트를 하나의 둥글고 모던한 **카드형 블록**으로 시각화. 해당 블록 내부에 세부 Action들이 하위 항목으로 리스트업 되는 트리 카드로 구성. 각 액션 추가 시 '+' 버튼에 맥동(Pulse) 애니메이션 효과.

### 4.4 Bottom Dock 및 플로팅 오버레이 설계
1.  **[Navigator] (오버레이 플로팅 패널)**
    *   **역할**: 전체 씬을 조망하고 시점을 제어.
    *   **스타일**: 캔버스 우측 하단이나 우측 상단 뷰포트 내부에 둥근 모서리의 플로팅 창(Floating Overlay)으로 배치 (투명도 20~30% 유리질감(Glassmorphism) 적용).
2.  **[Validation / Performance / Export] 탭 (하단 콘솔)**
    *   **역할**: 프로파일링, 오류, 빌드 정보 표기.
    *   **스타일**: 주로 닫아두는 탭이므로, 오류(Validation)가 생길 경우에만 하단 바에 붉은색 맥동 배지(Badge)가 발생. 열었을 땐 일반 텍스트 로그가 아닌 `Info / Warn / Error` 심볼 아이콘과 함께 표 형식 또는 리얼타임 성능 차트 그래프로 렌더링되게 개선.

---

## 5. 구현 진행 현황 및 구체적 작업 순서 (Implementation Checklist)

사용자의 요청(패널 디자인 다듬기 -> 팔레트 전역 적용 -> 고급 UX/UI 애니메이션)에 따라 최적화된 작업 순서입니다.

### ✅ 완료된 작업 (Done)
- [x] **속성 검사기(Inspector) 입력 방식 개선**: 텍스트 필드를 드래그 가능한 숫자 컨트롤(Drag Number/Spinner)로 대체하여 UX 향상.
- [x] **속성 검사기(Inspector) 토글 디자인**: Boolean 값을 조작할 때 기본 체크박스 대신 스무스한 스위치 토글(Toggle) 적용.
- [x] **실행 시 창 포커스**: 애플리케이션 실행 시 창이 강제로 최상단 및 활성 포커스(Active) 상태로 나타나도록 수정.

### 🚀 앞으로 진행할 작업 순서 (To-Do)

**Phase 1: 패널 및 컨트롤 레이아웃/디자인 기초 다듬기 (구조적 여백 및 형태 구성)**
- [x] **1. 상단 툴바(Top Toolbar) 정돈 및 아이콘화**: 텍스트로 나열된 빽빽한 버튼들을 직관적인 아이콘으로 교체, 논리적 그룹화(세로 구분선 추가).
- [x] **2. 위젯/에셋 라이브러리 카드형 UI (Grid View)**: 1차원 리스트 형태를 버리고 썸네일을 보여주는 2열 이상의 둥근 테두리 카드(Card) 형태로 변경.
- [x] **3. 패널 아코디언/섹션 구조 세분화 (Inspector 등)**: 속성 탭을 섹션별로 접고 펼칠 수 있는 아코디언(Accordion) 뷰로 재구성하고 컨트롤러 박스 간 여백 대폭 확대.
- [x] **4. 레이어 트리 패널(Layers) 가독성 향상**: 트리 행 간격(Line Height)을 늘리고 눈/자물쇠 아이콘을 심플한 벡터 그래픽으로 교체, 그리고 노드 간 뎁스(Depth) 선명화.

**Phase 2: 컬러 팔레트 및 타이포그래피 전역 적용 (Deep Dark 톤 매칭 및 모던 룩)**
- [x] **5. 글로벌 다크 테마 팔레트 하드코딩화**: 캔버스 다크(`#181A1F`), 메인 패널(`#21252B`), 보더 라인(`#2C313C`), 텍스트 하이라이트 및 강조색(`#5A9CFF`) 등 `GyeolCustomLookAndFeel`에 명확한 컬러 상수를 제정.
- [x] **6. 모던 타이포그래피 시스템 구축**: 뭉툭한 기본 시스템 폰트에서 벗어나, 'Inter' 혹은 'Roboto' 계열의 가독성 좋은 폰트를 전역으로 입히고 제목/본문의 굵기(Weight)와 투명계층 분리.
- [x] **7. 기본 렌더링 컴포넌트(스크롤바, 콤보박스 등) 드레스업**: 아직 남아있는 투박한 JUCE 기본 스크롤바 코어 및 콤보 박스 팝업을 라운디드+새로운 색상 규격으로 완전히 래핑(Wrapping).

**Phase 3: 고급 UI/UX 및 심미적 폴리싱 (Depth, 인터랙션, 마이크로 애니메이션)**
- [x] **8. 마이크로 호버(Hover) 및 컨디션 트랜지션**: 버튼 호버 시 AccentPrimary Glow 테두리, 리사이즈 핸들 호버 시 확대형 Glow, 텍스트 에디터 포커스 시 Accent Glow 구현.
- [x] **9. 그림자(Drop Shadow) 및 입체감(Depth) 부여**: 팝업 메뉴에 다중 레이어 소프트 드롭 섀도우+반투명 글래스모피즘 배경 적용. 범용 `drawSoftDropShadow` 유틸리티 함수 제작. 툴팁/알림/리스트박스 다크 테마 통일.
- [x] **10. 캔버스 렌더링 고급화**: 위젯 선택 아웃라인을 3중 레이어 네온 Glow로 교체, 리사이즈 핸들을 원형+내부 흰점 디자인으로 개편, 줌 레벨 기반 그리드 알파 페이드아웃 구현.
- [x] **11. 빈 상태(Empty State) 아트웍 및 실시간 피드백**: Inspector "No selection" 상태에 커서 아이콘 + 안내 텍스트가 포함된 미려한 Empty State 그래픽 적용. ValidationPanel "All checks passed" 상태에 체크마크 아이콘 + Glow 이펙트 적용.

**Phase 4: 나머지 패널 리팩토링 + 프리미엄 폴리싱**

> 아래 6개 패널은 아직 직접 수정되지 않았고 하드코딩 색상(`fromRGB(24,28,34)` 등)과 기본 JUCE 스타일을 그대로 사용 중입니다.
> Phase 4에서는 GyeolPalette 컬러를 적용하고, 각 패널의 특성에 맞는 프리미엄 UI를 구현합니다.

---

### 12. EventActionPanel 리팩토링 (블록형 비주얼 에디터)

**Progress Update (Code Audit: 2026-03-03)**
- [x] Stage 1 completed: display-name priority, event-label cleanup, Korean labels in key combos.
- [x] Stage 2 completed: target/property/param text inputs converted to ComboBox with data binding.
- [x] Optional filtering completed: selection-based one-way filter + 'All Widgets' option + implicit pin while text editing.
- [~] Stage 3(Visual card redesign) is partially prepared, but 12-a ~ 12-e core UI work remains.

**코드 기준 현황 (EventActionPanel.cpp)**
- [x] 바인딩/액션/런타임 파라미터/프로퍼티 바인딩 리스트의 데이터 편집 흐름은 동작 중.
- [ ] `paint()`와 ListBox/Editor 색상에 `fromRGB(...)` 하드코딩이 다수 남아 있음 (`kPanelBg`, `kPanelOutline`, 각 `paintListBoxItem`).
- [ ] 리스트 행 렌더링이 `fillRect + divider` 중심의 플랫 스타일이며 카드형 블록 레이아웃 미적용.
- [ ] Empty State가 없어 데이터 0개일 때 빈 리스트만 표시됨.
- [~] 섹션 타이틀 텍스트는 있으나(`Binding Detail`, `Runtime Params`, `Property Bindings`) HeaderBackground 기반 시각 분리 스타일은 미적용.

**갱신된 작업 순서 (권장)**
- [ ] **12-a. Palette 1차 정리 (선행 작업)**: `kPanelBg`/`kPanelOutline` 및 ListBox/Editor 하드코딩 색상을 `getGyeolColor(...)`로 치환. `EventActionPanel.cpp` 내 `fromRGB(24,28,34)`/`fromRGB(40,46,56)` 계열 우선 제거.
- [ ] **12-b. Binding ListBox 카드화**: 라운드 카드 + 좌측 이벤트 타입 컬러 바 + 제목/서브타이틀 2줄 레이아웃으로 전환. 선택 행은 AccentPrimary 보더/글로우 적용.
- [ ] **12-c. Action/RuntimeParam 카드 스타일 통일**: 액션 타입 접두 아이콘(텍스트 기반 또는 경로 아이콘) + 카드형 배경 + 선택 상태 좌측 Accent 바 적용.
- [ ] **12-d. Empty State 추가**: 바인딩이 0개일 때 "No events configured", 액션이 0개일 때 "No actions in this binding" 안내와 아이콘 표시.
- [ ] **12-e. 섹션 헤더 스타일 적용**: "Bindings", "Actions", "Runtime Params", "Property Bindings" 영역에 `HeaderBackground` + 하단 `BorderDefault` 디바이더 적용.

**12번 완료 기준 (DoD)**
- [ ] `EventActionPanel.cpp`에서 12번 범위의 하드코딩 색상 제거 및 Palette 토큰화 완료.
- [ ] Binding/Action/RuntimeParam 행 렌더링이 카드형 블록 스타일로 통일.
- [ ] 빈 데이터 상태에서 안내 UI가 노출되고, Add 버튼으로 즉시 복귀 가능.
- [ ] 섹션 헤더 시각 분리가 적용되고 선택/호버 대비가 기존 대비 명확히 개선.

---

### 13. HistoryPanel 리팩토링 (타임라인 스타일)

**현재 상태**: ListBox 기반 Undo/Redo 스택. `paintListBoxItem`에서 CURRENT/REDO/UNDO 배지(라벨) 렌더링 하드코딩. 배경 `fromRGB(24,28,34)` 직접 사용.

- [ ] **13-a. paint 배경을 GyeolPalette로 교체**: `PanelBackground`+`BorderDefault` 사용.
- [ ] **13-b. 행 항목을 타임라인 스타일로 리디자인**:
  - 좌측에 세로 타임라인 선(1px, `BorderDefault`) + 각 항목마다 원형 도트(Current=AccentPrimary 채움, Undo=보더만, Redo=흐린 보더)
  - CURRENT 행에 AccentPrimary 좌측 글로우 바 + 라벨을 `ValidSuccess` 색상으로 통일
  - REDO 행은 TextSecondary로 흐리게, UNDO 행은 TextPrimary로 일반 밝기
- [ ] **13-c. 빈 상태(Empty State)**: 히스토리가 비어있을 때 시계 아이콘 + "No history yet" 안내.
- [ ] **13-d. 접힌(collapsed) 상태의 미니 요약**: 접혀 있을 때 "Undo 3 · Redo 1" 형태의 한 줄 배지 표시.

---

### 14. NavigatorPanel 리팩토링 (글래스모피즘 미니맵)

**현재 상태**: 미니맵 캔버스를 직접 `paint`로 렌더링. 헤더에 "Navigator" 텍스트 + Zoom 수치 표시. 배경 `fromRGB(24,28,34)`, 맵 영역 `fromRGB(16,20,26)`.

- [ ] **14-a. paint 배경을 GyeolPalette로 교체**: `PanelBackground`, `CanvasBackground` 등 사용.
- [ ] **14-b. 미니맵 글래스모피즘 배경**: 맵 영역 배경에 `OverlayBackground` 색상(반투명) + 라운드(6px) + 미세 보더 적용. 현재 `fromRGB(16,20,26)` → 반투명 다크 + 라운드.
- [ ] **14-c. 뷰포트 하이라이트 개선**: 현재 파란색 사각형 → AccentPrimary의 반투명 채움(18%) + 1.5px 아웃라인 유지. 뷰포트 외곽에 미세 Glow 추가(8% alpha expanded rect).
- [ ] **14-d. 위젯 아이템 렌더링 개선**: 선택된 위젯은 `AccentPrimary` 채움, 잠금 위젯은 `ValidWarning` 반투명. 아이템 아웃라인을 `drawRect` → `drawRoundedRectangle(1.0f)`로 교체.
- [ ] **14-e. 줌 수치를 세련된 배지로**: "Zoom 1.000" → `×1.0` 형태의 라운드 배지(밝은 보더 + 작은 폰트).

---

### 15. PerformancePanel 리팩토링 (대시보드 카드 + 미니 차트)

**현재 상태**: 순수 텍스트 기반 (`g.drawText`로 12줄 이상 통계 나열). 배경 `fromRGB(24,28,34)`. 그래프/차트 없음.

- [ ] **15-a. paint 배경을 GyeolPalette로 교체**.
- [ ] **15-b. 주요 수치를 카드형 대시보드로 재배치**:
  - 상단 3열 카드: Refresh Hz / Paint Hz / SelectionSync Hz (큰 폰트 수치 + 작은 라벨)
  - 각 카드 배경 `HeaderBackground` 라운드 + 미세 보더
  - Hz 수치 색상: 정상(60Hz↑) → `ValidSuccess`, 경고(30~60) → `ValidWarning`, 위험(30↓) → `ValidError`
- [ ] **15-c. 상세 통계를 접이식 섹션으로 분류**:
  - "Timing" 섹션: last/max ms 수치
  - "Counters" 섹션: refresh/paint/selection counts
  - "Document" 섹션: widget/group/layer/asset counts
  - "Debug" 섹션: deferred refresh, dirty area
- [ ] **15-d. 미니 바 차트(선택적)**: Refresh/Paint/SelectionSync의 last ms를 max ms 대비 비율로 표시하는 0~100% 수평 바 차트. 패널 공간이 충분할 때만 표시.

---

### 16. ExportPreviewPanel 리팩토링 (코드 에디터 스타일)

**현재 상태**: TabbedComponent(Report/Header/Source/Manifest) + TextEditor 4개. 배경 `fromRGB(24,28,34)`. 탭은 JUCE 기본 스타일.

- [ ] **16-a. paint 배경을 GyeolPalette로 교체**.
- [ ] **16-b. 탭 바 커스텀**: TabbedComponent의 탭 바를 `HeaderBackground` 배경 + 선택된 탭에 AccentPrimary 하단 인디케이터(3px 라운드 바) + 미선택 탭은 TextSecondary.
- [ ] **16-c. TextEditor를 코드 에디터 풍으로**: 각 TextEditor에 monospace 폰트 적용 + 라인넘버 영역 배경색 차별화(더 어두운 배경). 읽기 전용이므로 포커스 보더 제거.
- [ ] **16-d. 상태 바 통합**: 하단에 "Generated at HH:MM:SS" + "Lines: NNN" 표시하는 미세한 1줄 상태 바.
- [ ] **16-e. 빈 상태(Empty State)**: 미리보기 생성 전 → 코드 아이콘(`< />`) + "Click Generate to preview export" 안내.

---

### 17. GridSnapPanel 리팩토링 (팝오버형 컴팩트 UI)

**현재 상태**: ToggleButton 4개(Snap/Grid View/Grid Snap/Smart Snap) + Slider 2개(Grid Size/Tolerance). 배경 `fromRGB(24,28,34)`. "Grid / Snap" 타이틀을 `g.drawText`로 직접 렌더링.

- [ ] **17-a. paint 배경을 GyeolPalette로 교체**.
- [ ] **17-b. 토글 스위치 그룹화**: 4개 토글을 2×2 그리드 또는 수평 배열로 재배치. 각 토글에 아이콘 접두사(격자 아이콘, 자석 아이콘 등) 추가.
- [ ] **17-c. 슬라이더를 DragNumber 스타일로 교체**: 기존 JUCE 슬라이더 → Phase 1에서 구현한 DragNumber 컨트롤로 교체 (Grid Size: 1~100px, Tolerance: 0~20px).
- [ ] **17-d. 섹션 구분 라인**: "Grid" 설정과 "Snap" 설정을 미세한 디바이더(`BorderDefault`)로 시각적 분리.
- [ ] **17-e. 라이브 미리보기 힌트**: Grid Size 변경 시 현재 값을 실시간으로 표시하는 작은 수치 배지.

---

### 18. 전역 폴리싱 (하드코딩 색상 제거 + 일관성)

- [ ] **18-a. 하드코딩 `fromRGB(24,28,34)` / `fromRGB(40,46,56)` 전부 제거**: 모든 패널의 paint 함수에서 하드코딩 색상을 `getGyeolColor(GyeolPalette::PanelBackground)` / `getGyeolColor(GyeolPalette::BorderDefault)`로 교체.
- [ ] **18-b. 하드코딩 `FontOptions(12.0f)` → `makeFont(12.0f)` 교체**: 패널 내부의 직접 폰트 생성을 LookAndFeel의 `makeFont` 호출로 통일.
- [ ] **18-c. 패널 공통 base class 검토**: 반복되는 paint 패턴(`fillAll` + `drawRect`)을 공통 유틸리티 또는 base class로 추출하는 것을 검토.
- [ ] **18-d. 접근성(Accessibility) 기본 속성 설정**: 각 인터랙티브 컴포넌트에 `setTitle`/`setDescription` 추가.

