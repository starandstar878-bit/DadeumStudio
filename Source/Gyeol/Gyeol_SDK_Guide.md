# 🌌 Gyeol SDK 상세 가이드: 마스터 가이드북

**업데이트:** 2026-03-09 | **버전:** SDK v2.5 (ABI 1.5 호환)

Gyeol SDK는 직관적인 **선언적 UI 정의**와 강력한 **C++ 코드 생성 시스템**을 결합하여, 전문가급 JUCE 위젯을 빠르고 표준화된 방식으로 개발할 수 있게 돕습니다.

---

## 📑 목차
1. [위젯 속성 정의 (Properties)](#1-위젯-속성-정의-widgetpropertyspec)
2. [런타임 이벤트 정의 (Events)](#2-런타임-이벤트-정의-runtimeeventspec)
3. [렌더링 가이드 (Painter)](#3-렌더링-가이드-painter--icons)
4. [완성형 예제: SuperWidget](#4-종합-예제-superwidget)
5. [개발자 경험 (IDE Support)](#5-visual-studio-intellisense-지원)
6. [속성 명명 관례 (Conventions)](#6-속성-명명-관례-naming-conventions)
7. [코드 생성 및 내보내기 (Export)](#7-코드-생성-및-export-export-pipeline)
8. [커스텀 이벤트 및 외부 API 연동](#8-커스텀-이벤트-인식-및-발생-custom-event-recognition)
9. [드래그 앤 드롭 지원 (DnD)](#9-드래그-앤-드롭-지원-drag--drop)
10. [동적 속성 제어 (Conditional UI)](#10-동적-속성-제어-conditional-properties)
11. [위젯 등록 체크리스트](#11-요약-위젯-등록-전체-체크리스트)
12. [DX 철학 및 향후 로드맵 (Roadmap)](#12-dx-철학-및-향후-개선-로드맵-roadmap)

---

## 1. 위젯 속성 정의 (WidgetPropertySpec)

`propertySpecs` 배열에 정의된 속성은 에디터의 **Inspector 패널**에 실시간으로 반영됩니다.

### 1.1 데이터 타입 (Kind)
| 타입 (Kind) | 설명 | 권장 데이터 형태 |
| :--- | :--- | :--- |
| `text` | 일반 문자열 | `juce::String` |
| `integer` | 정수형 숫자 | `int`, `int64_t` |
| `number` | 실수형 숫자 | `float`, `double` |
| `boolean` | 참/거짓 | `bool` (Toggle UI) |
| `color` | 색상 데이터 | Hex Code 또는 RGBA Object |
| `enumChoice` | 고정된 선택지 | `dropdown` UI와 함께 사용 |
| `assetRef` | 프로젝트 자산 참조 | 이미지, 폰트, 오디오 파일 ID |

### 1.2 UI 힌트 (UiHint)
속성이 인스펙터에서 어떤 **컨트롤**로 표현될지 결정합니다.
> **Tip:** `number` 타입에 `slider` 힌트를 주면 인스펙터 안에서 즉석 슬라이더 조절이 가능합니다.

---

## 2. 런타임 이벤트 정의 (RuntimeEventSpec)

사용자의 행동에 반응하는 **신호(Signal)**입니다.

*   **표준 이벤트 키:** `onClick`, `onValueChanged`, `onPress`, `onRelease`
*   **주요 설정:**
    *   `continuous`: 드래그처럼 연속적인 발생 여부 (`true` 권장)
    *   `throttleMs`: 초당 이벤트 발생 횟수 제한 (성능 최적화용)

---

## 3. 렌더링 가이드 (Painter & Icons)

### 3.1 Painter (실시간 렌더링)
위젯의 본체를 그리는 람다 함수입니다. `body` 사각형 영역 안에서 `juce::Graphics`를 사용합니다.

```cpp
descriptor.painter = [](juce::Graphics& g, const WidgetModel& widget, const juce::Rectangle<float>& body) {
    auto color = juce::Colour::fromString(widget.properties.getWithDefault("color", "#FFFFFF"));
    g.setColour(color);
    g.fillRoundedRectangle(body, 4.0f);
};
```

### 3.2 IconPainter (라이브러리용)
에디터 좌측 위젯 목록에서 보일 **심볼**을 그립니다. 속성값과 상관없이 위젯의 정체성을 보여주는 정적 그림을 권장합니다.

---

## 4. 종합 예제: SuperWidget

**Fluent API**를 사용하여 복잡한 설정을 한눈에 읽히도록 정의한 예시입니다.

```cpp
class SuperWidget final : public WidgetClass {
public:
    WidgetDescriptor makeDescriptor() const override {
        return WidgetDescriptor()
            .setType(WidgetType::button)
            .setCategory("Premium")
            .setSize(160, 60)
            .addProperty(Property::Number("volume", "Main Volume")
                .ui(WidgetPropertyUiHint::slider).range(0, 1).def(0.8))
            .addProperty(Property::Choice("theme", "Theme")
                .addOption("dark", "Dark Mode").addOption("light", "Light Mode"))
            .addEvent(Event::Action("onTrigger", "Trigger Action"))
            .setPainter([](juce::Graphics& g, const WidgetModel& w, const juce::Rectangle<float>& b) {
                // ... 상징적인 렌더링 코드 ...
            });
    }
};
GYEOL_WIDGET_AUTOREGISTER(SuperWidget);
```

---

## 5. Visual Studio IntelliSense 지원

Gyeol SDK는 개발자가 문서를 보지 않고도 코딩할 수 있는 **Self-Documenting** 환경을 지향합니다.

*   **강력한 자동 완성:** `Property::`나 `Event::` 입력 시 팩토리 메서드 즉시 제안.
*   **체이닝 가이드:** `addProperty()` 뒤에 `.`만 찍으면 설정 가능한 모든 옵션(`range`, `def`, `ui` 등) 나열.
*   **팝업 도움말:** 마우스를 올리면 각 속성/메서드의 역할과 에디터 노출 방식에 대한 한글 툴팁 제공.

---

## 6. 속성 명명 관례 (Naming Conventions)

에디터와 코드 생성기 사이의 일관성을 위해 다음 규칙을 준수하세요.

1.  **CamelCase**: 속성 키는 항상 `작은카멜케이스`를 사용합니다. (예: `mainColor`, `fontSize`)
2.  **Prefix**: 전용 속성은 `위젯명.속성명` 형식을 권장합니다. (예: `knob.sensitivity`)
3.  **Short for Common**: `visible`, `text`, `value` 처럼 보편적인 속성은 짧게 유지합니다.

---

## 7. 코드 생성 및 Export (Export Pipeline)

설계한 UI를 **실제 C++ 클래스**로 변환하는 전략입니다.

### 7.1 수준별 등록 전략
| 레벨 | 설명 | 주요 용도 |
| :--- | :--- | :--- |
| **Level 1** | 단순 매핑 (`setExportTargetType`) | `juce::TextButton` 등 표준 위젯 사용 시 |
| **Level 2** | 커스텀 초기화 (`constructorLines`) | 위젯 생성 시 특정 메서드(`setRange` 등) 호출 필요 시 |
| **Level 3** | 완전 커스텀 통합 (`addRequiredHeader`) | 자체 개발한 복잡한 가상 악기/DSP 컴포넌트 연동 시 |

---

## 8. 커스텀 이벤트 인식 및 발생 (Custom Event Recognition)

표준 마우스 입력 외의 특별한 상호작용(예: 펜 스윙, 회전 제스처, 외부 하드웨어 신호)을 처리합니다.

### 8.1 외부 API 연동
하드웨어 드라이버 SDK의 전용 콜백을 Gyeol 이벤트 시스템에 브릿지할 수 있습니다.
```cpp
// 외부 드라이버 콜백에서 직접 이벤트 방출
MyHardware::onKnobTurn = [id, api](int delta) {
    api->emit_runtime_event(id, "onExternalTurn", juce::var(delta));
};
```

---

## 9. 드래그 앤 드롭 지원 (Drag & Drop)

자산 브라우저에서 항목을 끌어다 놓을 때의 동작을 `setDropHandler`로 정의합니다.

1.  **제안 (Options)**: 드래그 중인 파일의 MIME 타입을 확인하고 가능한 액션(배경 이미지 설정, 오디오 로드 등) 반환.
2.  **적용 (Apply)**: 사용자가 선택한 옵션에 따라 위젯 속성(`PropertyBag`) 업데이트.

---

## 10. 동적 속성 제어 (Conditional Properties)

인스펙터를 깔끔하게 유지하기 위해 **조건부 노출** 기능을 활용하세요.

```cpp
.addProperty(Property::Choice("sourceType", "Source")
    .addOption("file", "File").addOption("live", "Live Input"))
.addProperty(Property::Asset("filePath", "File Path")
    .dependsOn("sourceType", "file"))  // sourceType이 'file'일 때만 보임
```

---

## 11. 요약: 위젯 등록 전체 체크리스트

1.  ✅ **Identity**: `typeKey`, `category`, `setSize`
2.  ✅ **State**: `addProperty` (단독 및 `dependsOn` 설정)
3.  ✅ **Signal**: `addEvent` (액션 바인딩용)
4.  ✅ **Visual**: `setPainter` (에디터 내 가시화)
5.  ✅ **Drop**: `setDropHandler` (자산 로드 편의성)
6.  ✅ **Code**: `setExportTargetType`, `setExportCodegen` (C++ 변환)

---

## 12. DX 철학 및 향후 개선 로드맵 (Roadmap)

### 🚀 Gyeol의 DX 철학
*   **Zero Guesswork**: 문서를 뒤지지 않아도 IDE 안에서 정의가 끝날 것.
*   **Safe by Default**: 잘못된 데이터 타입이나 범위를 정의 단계에서 방어할 것.
*   **Native-First**: 최종 결과물은 항상 최적화된 Native C++ 코드로 나올 것.

### 🛠 향후 개선 예정 (Next Steps)
1.  **Partial Refresh Inspector**: 성능 최적화를 위해 변경된 속성만 부분적으로 다시 그리는 메커니즘 도입.
2.  **Property Validation SDK**: 에디터 내에서 속성값의 유효성을 즉각 검증하는 `validate()` 메서드 추가.
3.  **Layout Engine Support**: 자율 레이아웃(Flexbox/Grid)을 속성 정의만으로 지원하는 기능.
4.  **Auto-Documentation**: 작성한 `WidgetDescriptor`를 바탕으로 한 전용 위젯 명세서 자동 생성 도구.

---
*Developed by DadeumStudio Engine Team*

