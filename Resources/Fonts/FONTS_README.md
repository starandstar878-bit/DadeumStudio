# Gyeol Editor 내장 폰트 리소스

## 폴더 구조

```
Source/Resources/Fonts/
├── Nunito/               ← 기본 UI 폰트 (영문, 숫자)
│   ├── Nunito-Regular.ttf
│   ├── Nunito-Medium.ttf
│   ├── Nunito-SemiBold.ttf
│   └── Nunito-Bold.ttf
├── NanumGothic/          ← 한글 호환 폰트
│   ├── NanumGothic.ttf
│   └── NanumGothicBold.ttf
└── FONTS_README.md       ← 이 파일
```

## 폰트 파일 배치 방법

1. 위 폴더 구조에 맞게 `.ttf` 파일을 복사해 넣으세요.
2. `.jucer` 파일에 리소스로 등록 (resource="1") 합니다.
3. Projucer 재생성 또는 수동 BinaryData 업데이트가 필요합니다.

## 사용 중인 폰트 라이선스

- **Nunito** - SIL Open Font License (OFL) - 상업적 사용 가능
  - 출처: https://fonts.google.com/specimen/Nunito
- **나눔고딕 / NanumGothic** - SIL Open Font License (OFL) - 상업적 사용 가능
  - 출처: https://fonts.google.com/specimen/Nanum+Gothic

## 코드에서 사용하는 방법

폰트가 BinaryData에 등록되면 아래와 같이 사용합니다:

```cpp
// GyeolCustomLookAndFeel.cpp 에서:
auto typeface = juce::Typeface::createSystemTypefaceFor(
    BinaryData::Nunito_Regular_ttf,
    BinaryData::Nunito_Regular_ttfSize
);

// 폰트 객체 생성:
juce::Font nunitoFont(juce::FontOptions(typeface).withHeight(13.0f));
```

## 현재 등록 상태

| 파일 | BinaryData 변수명 | 상태 |
|------|-------------------|------|
| Nunito-Regular.ttf | `Nunito_Regular_ttf` | 🔲 파일 대기 중 |
| Nunito-SemiBold.ttf | `Nunito_SemiBold_ttf` | 🔲 파일 대기 중 |
| Nunito-Bold.ttf | `Nunito_Bold_ttf` | 🔲 파일 대기 중 |
| NanumGothic.ttf | `NanumGothic_ttf` | 🔲 파일 대기 중 |
| NanumGothicBold.ttf | `NanumGothicBold_ttf` | 🔲 파일 대기 중 |
