# 🌌 Gyeol UI Script (GUS) v0.4: Human-Friendly Design

**"개발자가 대충 말해도, 해석기가 찰떡같이 알아먹는 설계"**

GUS v0.4의 핵심 철학은 **'코드의 노이즈 제거'**입니다. 중괄호(`{}`), 세미콜론(`;`), 불필요한 객체 참조(`g.`, `this.`)를 최소화하여, 마치 디자인 명세서를 적듯 위젯을 정의합니다.

---

## 1. 극도로 단순해진 문법 (Ultra-Simple Syntax)

*   **암시적 변수**: `Paint`와 `Interaction` 블록 안에서는 `width`, `height`, `center`, `mouse` 변수가 자동으로 제공됩니다. 계산할 필요가 없습니다.
*   **타입 추론**: `prop volume = 0.8` 이라고 적으면 해석기가 자동으로 `float` 타입과 기본값을 인식합니다.
*   **블록 최소화**: 단순한 위젯은 중괄호 없이 탭(Tab) 또는 들여쓰기만으로 구조를 파악합니다.

---

## 2. 프로퍼티 및 이벤트 (Shorthand)

마치 변수를 선언하듯 적습니다.
```javascript
Widget RetroKnob

prop value = 0.5           // 숫자형 자동 인식
prop theme = "Dark"        // 문자열 자동 인식
prop activeColor = #FF3300 // 색상 자동 인식

@dependsOn(theme == "Light")
prop shadow = true

event onClick              // 단순 이벤트 선언
```

---

## 3. 강력한 페인트 블록 (Natural Paint)

`g.` 접두사가 없고, 자주 쓰이는 수식들이 내장되어 있습니다.

```javascript
Paint ->
    // 'center'는 위젯의 중앙 점, 'width'는 전체 너비임 (자동 제공)
    fill(activeColor)
    circle(center, radius: width * 0.4)
    
    // 조건문도 자연스럽게
    if (value > 0.8) 
        set(#FF0000) // 위험 수위 시 빨간색으로 변경
    
    text(value * 100 + "%", font: 20, bold: true)
```

---

## 4. 함수 선언 (Drawing & Logic Helpers)

함수 또한 화살표(`->`)를 사용하여 간결하게 선언합니다.

```javascript
// 바늘을 그리는 헬퍼 (draw 키워드로 그리기 옵션 활성화)
draw needle(angle, len) ->
    rotate(angle, pivot: center)
    line(center, [center.x, center.y - len], thickness: 3)

Paint ->
    needle(value * 270, 50) // 선언한 함수 호출
```

---

## 5. 지능형 상호작용 (Advanced Interaction)

`Interaction` 블록은 단순 클릭 외에도 드래그 앤 드롭, 팝업 메뉴 등 복잡한 UX를 한 줄로 선언합니다.

```javascript
Interaction ->
    // [DnD] 특정 타입의 파일이 드롭될 때의 동작
    onDrop(image/*) : bgImage = asset.id
    onDrop(audio/*) : loadSample(asset.path)

    // [Context Menu] 우클릭 시 메뉴 표시
    onMouseDown where mouse.isRight :
        menu("Reset", "Copy", "Settings")

    // [Gesture] 더블 클릭 등 특수 동작
    onDoubleClick : val = 0.5
```

---

## 6. 종합 완성형 예제: `ProGauge.gus`

모든 요소가 결합된 전문가용 위젯의 모습입니다.

```javascript
Widget ProGauge

prop val = 0.7
prop color = #00FFCC
prop bg = ""

draw tick(i) ->
    rotate(i * 30, pivot: center)
    line([center.x, 10], [center.x, 20])

draw needle(ang) ->
    rotate(ang, pivot: center)
    line(center, [center.x, center.y - 40], thickness: 2)

Paint ->
    if bg != "" : image(bg, bounds) // 드롭된 이미지가 있으면 배경으로 출력
    
    set(color)
    for i in 0..12 : tick(i) // 눈금 그리기
    needle(val * 240 - 120)  // 바늘 그리기
    text(val * 100 + "%", y: height - 20)

Interaction ->
    onMouseDrag : val = clamp(mouse.x / width, 0, 1)
    onDrop(image/*) : bg = asset.id
    onMouseDown where mouse.isRight : 
        menu("Clear BG") : bg = ""
```

---

## 7. 해석기(Interpreter)의 지능형 동작

Gyeol 해석기는 사용자의 의도를 다음과 같이 스마트하게 추측합니다:

1.  **Unit Guessing**: `text(24)` 라고 적으면 `fontSize: 24` 로 이해합니다.
2.  **Color Context**: 단독으로 쓰인 `#Hex` 코드는 `setColour()`의 인자로 이해합니다.
3.  **Coordinate Magic**: `[x, y]` 형태의 배열은 자동으로 `juce::Point` 객체로 변환합니다.
4.  **Auto-Action**: `Interaction`에서 `val = ...` 처럼 속성을 직접 대입하면, 자동으로 `patch.set()` 명령을 생성하고 화면 리프레시를 예약합니다.

---
*Inspired by Developer Happiness | DadeumStudio*
