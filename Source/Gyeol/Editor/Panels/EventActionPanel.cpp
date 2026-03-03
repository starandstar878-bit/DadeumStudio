#include "Gyeol/Editor/Panels/EventActionPanel.h"

#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"
#include "Gyeol/Runtime/PropertyBindingResolver.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace Gyeol::Ui::Panels {
namespace {
using Gyeol::GyeolPalette;

juce::Colour palette(GyeolPalette id, float alpha = 1.0f) {
  return Gyeol::getGyeolColor(id).withAlpha(alpha);
}

const auto kPanelBg = palette(GyeolPalette::PanelBackground);
const auto kPanelOutline = palette(GyeolPalette::BorderDefault);
const auto kStatusInfo = palette(GyeolPalette::TextSecondary);
const auto kStatusOk = palette(GyeolPalette::ValidSuccess);
const auto kStatusWarn = palette(GyeolPalette::ValidWarning);
const auto kStatusError = palette(GyeolPalette::ValidError);

juce::Colour eventAccent(const juce::String &eventKey) {
  const auto key = eventKey.toLowerCase();
  if (key.contains("click") || key.contains("press") || key.contains("release"))
    return palette(GyeolPalette::AccentPrimary);
  if (key.contains("toggle"))
    return palette(GyeolPalette::ValidWarning);
  if (key.contains("value") || key.contains("selection") ||
      key.contains("text"))
    return palette(GyeolPalette::ValidSuccess);
  return palette(GyeolPalette::AccentHover);
}

juce::Colour actionAccent(RuntimeActionKind kind) {
  switch (kind) {
  case RuntimeActionKind::setRuntimeParam:
    return palette(GyeolPalette::AccentPrimary);
  case RuntimeActionKind::adjustRuntimeParam:
    return palette(GyeolPalette::ValidSuccess);
  case RuntimeActionKind::toggleRuntimeParam:
    return palette(GyeolPalette::ValidWarning);
  case RuntimeActionKind::setNodeProps:
    return palette(GyeolPalette::AccentHover);
  case RuntimeActionKind::setNodeBounds:
    return palette(GyeolPalette::ValidError);
  }

  return palette(GyeolPalette::AccentPrimary);
}

juce::String actionPrefix(RuntimeActionKind kind) {
  switch (kind) {
  case RuntimeActionKind::setRuntimeParam:
    return "[P] ";
  case RuntimeActionKind::adjustRuntimeParam:
    return "[D] ";
  case RuntimeActionKind::toggleRuntimeParam:
    return "[T] ";
  case RuntimeActionKind::setNodeProps:
    return "[S] ";
  case RuntimeActionKind::setNodeBounds:
    return "[B] ";
  }

  return "[?] ";
}

void drawCardBackground(juce::Graphics &g, juce::Rectangle<float> bounds,
                        bool rowIsSelected, juce::Colour accent) {
  if (bounds.isEmpty())
    return;

  const auto radius = 6.0f;
  const auto fill = rowIsSelected ? palette(GyeolPalette::ControlBase, 0.96f)
                                  : palette(GyeolPalette::ControlBase, 0.78f);
  if (rowIsSelected) {
    auto glowArea = bounds.expanded(1.0f);
    g.setColour(accent.withAlpha(0.16f));
    g.fillRoundedRectangle(glowArea, radius + 1.0f);
  }

  g.setColour(fill);
  g.fillRoundedRectangle(bounds, radius);

  g.setColour(rowIsSelected ? accent.withAlpha(0.85f)
                            : palette(GyeolPalette::BorderDefault, 0.92f));
  g.drawRoundedRectangle(bounds, radius, rowIsSelected ? 1.35f : 1.0f);
}

void drawLeftAccentBar(juce::Graphics &g, juce::Rectangle<float> card,
                       juce::Colour colour, bool rowIsSelected) {
  auto bar =
      card.reduced(0.0f, 2.0f).removeFromLeft(rowIsSelected ? 4.0f : 3.0f);
  g.setColour(colour.withAlpha(rowIsSelected ? 0.95f : 0.7f));
  g.fillRoundedRectangle(bar, 2.0f);
}

void setupEditor(juce::TextEditor &editor,
                 const juce::String &placeholder = {}) {
  editor.setMultiLine(false);
  editor.setScrollbarsShown(true);
  editor.setColour(juce::TextEditor::backgroundColourId,
                   palette(GyeolPalette::ControlBase));
  editor.setColour(juce::TextEditor::outlineColourId,
                   palette(GyeolPalette::BorderDefault));
  editor.setColour(juce::TextEditor::textColourId,
                   palette(GyeolPalette::TextPrimary));
  editor.setTextToShowWhenEmpty(placeholder,
                                palette(GyeolPalette::TextSecondary));
}

bool parseWidgetId(const juce::String &text, WidgetId &outId) noexcept {
  const auto parsed = widgetIdFromJsonString(text.trim());
  if (!parsed.has_value() || *parsed <= kRootId)
    return false;
  outId = *parsed;
  return true;
}

bool isIdentifierLike(const juce::String &text) noexcept {
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return false;

  const auto isStart = [](juce::juce_wchar ch) noexcept {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
  };

  const auto isBody = [isStart](juce::juce_wchar ch) noexcept {
    return isStart(ch) || (ch >= '0' && ch <= '9') || ch == '.';
  };

  if (!isStart(trimmed[0]))
    return false;

  for (int i = 1; i < trimmed.length(); ++i) {
    if (!isBody(trimmed[i]))
      return false;
  }

  return true;
}

juce::var patchToVar(const PropertyBag &patch) {
  auto object = std::make_unique<juce::DynamicObject>();
  for (int i = 0; i < patch.size(); ++i)
    object->setProperty(patch.getName(i), patch.getValueAt(i));
  return juce::var(object.release());
}

juce::String eventDisplayLabelKo(const juce::String &eventKey) {
  const auto key = eventKey.trim();
  if (key == "onClick")
    return juce::String::fromUTF8(u8"\uD074\uB9AD \uC2DC");
  if (key == "onPress")
    return juce::String::fromUTF8(u8"\uB204\uB97C \uB54C");
  if (key == "onRelease")
    return juce::String::fromUTF8(u8"\uB193\uC744 \uB54C");
  if (key == "onValueChanged")
    return juce::String::fromUTF8(u8"\uAC12 \uBCC0\uACBD \uC2DC");
  if (key == "onValueCommit")
    return juce::String::fromUTF8(u8"\uAC12 \uD655\uC815 \uC2DC");
  if (key == "onToggleChanged")
    return juce::String::fromUTF8(u8"\uD1A0\uAE00 \uC2DC");
  if (key == "onTextCommit")
    return juce::String::fromUTF8(u8"\uD14D\uC2A4\uD2B8 \uD655\uC815 \uC2DC");
  if (key == "onSelectionChanged")
    return juce::String::fromUTF8(u8"\uC120\uD0DD \uBCC0\uACBD \uC2DC");
  return {};
}

// 위젯 표시 이름: properties["name"] 우선, 없으면 타입명 + ID
juce::String widgetDisplayName(const WidgetModel &widget,
                               const Widgets::WidgetRegistry &registry) {
  // 사용자가 name 속성을 설정한 경우 우선 사용
  if (widget.properties.contains("name")) {
    const auto name = widget.properties["name"].toString().trim();
    if (name.isNotEmpty())
      return name + " (#" + juce::String(widget.id) + ")";
  }

  // 기본값: 타입 표시명 + ID
  juce::String typeLabel = "Widget";
  if (const auto *descriptor = registry.find(widget.type);
      descriptor != nullptr) {
    if (descriptor->displayName.isNotEmpty())
      typeLabel = descriptor->displayName;
    else if (descriptor->typeKey.isNotEmpty())
      typeLabel = descriptor->typeKey;
  }
  return typeLabel + " #" + juce::String(widget.id);
}

int actionKindToComboId(RuntimeActionKind kind) noexcept {
  switch (kind) {
  case RuntimeActionKind::setRuntimeParam:
    return 1;
  case RuntimeActionKind::adjustRuntimeParam:
    return 2;
  case RuntimeActionKind::toggleRuntimeParam:
    return 3;
  case RuntimeActionKind::setNodeProps:
    return 4;
  case RuntimeActionKind::setNodeBounds:
    return 5;
  }
  return 0;
}

std::optional<RuntimeActionKind> actionKindFromComboId(int id) noexcept {
  switch (id) {
  case 1:
    return RuntimeActionKind::setRuntimeParam;
  case 2:
    return RuntimeActionKind::adjustRuntimeParam;
  case 3:
    return RuntimeActionKind::toggleRuntimeParam;
  case 4:
    return RuntimeActionKind::setNodeProps;
  case 5:
    return RuntimeActionKind::setNodeBounds;
  default:
    break;
  }
  return std::nullopt;
}

int nodeKindToComboId(NodeKind kind) noexcept {
  switch (kind) {
  case NodeKind::widget:
    return 1;
  case NodeKind::group:
    return 2;
  case NodeKind::layer:
    return 3;
  }
  return 0;
}

std::optional<NodeKind> nodeKindFromComboId(int id) noexcept {
  switch (id) {
  case 1:
    return NodeKind::widget;
  case 2:
    return NodeKind::group;
  case 3:
    return NodeKind::layer;
  default:
    break;
  }
  return std::nullopt;
}

int runtimeParamTypeToComboId(RuntimeParamValueType type) noexcept {
  switch (type) {
  case RuntimeParamValueType::number:
    return 1;
  case RuntimeParamValueType::boolean:
    return 2;
  case RuntimeParamValueType::string:
    return 3;
  }

  return 0;
}

std::optional<RuntimeParamValueType>
runtimeParamTypeFromComboId(int id) noexcept {
  switch (id) {
  case 1:
    return RuntimeParamValueType::number;
  case 2:
    return RuntimeParamValueType::boolean;
  case 3:
    return RuntimeParamValueType::string;
  default:
    break;
  }

  return std::nullopt;
}

std::optional<bool> parseLooseBool(const juce::String &text) noexcept {
  const auto normalized = text.trim().toLowerCase();
  if (normalized == "true" || normalized == "1" || normalized == "on" ||
      normalized == "yes")
    return true;
  if (normalized == "false" || normalized == "0" || normalized == "off" ||
      normalized == "no")
    return false;
  return std::nullopt;
}
} // namespace

EventActionPanel::EventActionPanel(DocumentHandle &documentIn,
                                   const Widgets::WidgetRegistry &registryIn)
    : document(documentIn), registry(registryIn), bindingListModel(*this),
      actionListModel(*this), runtimeParamListModel(*this),
      propertyBindingListModel(*this) {
  titleLabel.setText("Event/Action", juce::dontSendNotification);
  titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
  titleLabel.setColour(juce::Label::textColourId,
                       palette(GyeolPalette::TextPrimary));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(titleLabel);

  eventModeButton.setClickingTogglesState(true);
  eventModeButton.setRadioGroupId(0x47ea);
  eventModeButton.onClick = [this] {
    if (!suppressCallbacks)
      setPanelMode(PanelMode::eventAction);
  };
  addAndMakeVisible(eventModeButton);

  stateModeButton.setClickingTogglesState(true);
  stateModeButton.setRadioGroupId(0x47ea);
  stateModeButton.onClick = [this] {
    if (!suppressCallbacks)
      setPanelMode(PanelMode::stateBinding);
  };
  addAndMakeVisible(stateModeButton);

  addAndMakeVisible(sourceCombo);
  sourceCombo.onChange = [this] {
    if (!suppressCallbacks)
      rebuildCreateCombos();
  };

  addAndMakeVisible(eventCombo);
  addBindingButton.onClick = [this] { createBindingFromToolbar(); };
  addAndMakeVisible(addBindingButton);

  setupEditor(searchEditor, "Search Name/Source/Event");
  searchEditor.onTextChange = [this] {
    if (!suppressCallbacks)
      rebuildVisibleBindings();
  };
  addAndMakeVisible(searchEditor);
  showAllWidgetsToggle.setButtonText(
      juce::String::fromUTF8(u8"\uBAA8\uB4E0 \uC704\uC82F \uBCF4\uAE30"));
  showAllWidgetsToggle.onClick = [this] {
    if (suppressCallbacks)
      return;
    syncSelectionFilter();
    rebuildVisibleBindings();
    restoreSelections();
    refreshDetailEditors();
  };
  addAndMakeVisible(showAllWidgetsToggle);

  bindingSectionLabel.setText("Bindings", juce::dontSendNotification);
  bindingSectionLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
  bindingSectionLabel.setColour(juce::Label::textColourId,
                                palette(GyeolPalette::TextPrimary));
  bindingSectionLabel.setJustificationType(juce::Justification::centredLeft);
  bindingSectionLabel.setBorderSize(juce::BorderSize<int>(0, 8, 0, 8));
  addAndMakeVisible(bindingSectionLabel);

  bindingList.setModel(&bindingListModel);
  bindingList.setRowHeight(48);
  bindingList.setColour(juce::ListBox::backgroundColourId,
                        palette(GyeolPalette::CanvasBackground));
  bindingList.setColour(juce::ListBox::outlineColourId,
                        palette(GyeolPalette::BorderDefault));
  addAndMakeVisible(bindingList);

  actionSectionLabel.setText("Actions", juce::dontSendNotification);
  actionSectionLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
  actionSectionLabel.setColour(juce::Label::textColourId,
                               palette(GyeolPalette::TextPrimary));
  actionSectionLabel.setJustificationType(juce::Justification::centredLeft);
  actionSectionLabel.setBorderSize(juce::BorderSize<int>(0, 8, 0, 8));
  addAndMakeVisible(actionSectionLabel);

  detailTitleLabel.setText("Binding Detail", juce::dontSendNotification);
  detailTitleLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
  detailTitleLabel.setColour(juce::Label::textColourId,
                             palette(GyeolPalette::TextPrimary));
  detailTitleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(detailTitleLabel);

  setupEditor(bindingNameEditor, "Binding name");
  bindingNameEditor.onReturnKey = [this] { applyBindingMeta(); };
  bindingNameEditor.onFocusLost = [this] { applyBindingMeta(); };
  addAndMakeVisible(bindingNameEditor);

  bindingEnabledToggle.setClickingTogglesState(true);
  bindingEnabledToggle.onClick = [this] { applyBindingMeta(); };
  addAndMakeVisible(bindingEnabledToggle);

  duplicateBindingButton.onClick = [this] { duplicateSelectedBinding(); };
  deleteBindingButton.onClick = [this] { deleteSelectedBinding(); };
  addAndMakeVisible(duplicateBindingButton);
  addAndMakeVisible(deleteBindingButton);

  actionList.setModel(&actionListModel);
  actionList.setRowHeight(40);
  actionList.setColour(juce::ListBox::backgroundColourId,
                       palette(GyeolPalette::CanvasBackground));
  actionList.setColour(juce::ListBox::outlineColourId,
                       palette(GyeolPalette::BorderDefault));
  addAndMakeVisible(actionList);

  addActionButton.onClick = [this] { addAction(); };
  deleteActionButton.onClick = [this] { deleteAction(); };
  actionUpButton.onClick = [this] { moveActionUp(); };
  actionDownButton.onClick = [this] { moveActionDown(); };
  addAndMakeVisible(addActionButton);
  addAndMakeVisible(deleteActionButton);
  addAndMakeVisible(actionUpButton);
  addAndMakeVisible(actionDownButton);

  // 액션 종류 콤보 — 한글 레이블
  actionKindCombo.addItem(
      juce::String::fromUTF8(u8"\uD30C\uB77C\uBBF8\uD130 \uC124\uC815"),
      1); // 파라미터 설정
  actionKindCombo.addItem(
      juce::String::fromUTF8(u8"\uD30C\uB77C\uBBF8\uD130 \uC870\uC815"),
      2); // 파라미터 조정
  actionKindCombo.addItem(
      juce::String::fromUTF8(u8"\uD30C\uB77C\uBBF8\uD130 \uD1A0\uAE00"),
      3); // 파라미터 토글
  actionKindCombo.addItem(juce::String::fromUTF8(u8"\uC18D\uC131 \uBCC0\uACBD"),
                          4); // 속성 변경
  actionKindCombo.addItem(
      juce::String::fromUTF8(u8"\uC704\uCE58/\uD06C\uAE30 \uBCC0\uACBD"),
      5); // 위치/크기 변경
  actionKindCombo.onChange = [this] { applyActionKind(); };
  addAndMakeVisible(actionKindCombo);

  paramKeyCombo.setEditableText(true);
  paramKeyCombo.setTextWhenNothingSelected(
      juce::String::fromUTF8(u8"\uD30C\uB77C\uBBF8\uD130 \uD0A4"));
  setupEditor(valueEditor, juce::String::fromUTF8(u8"\uAC12"));
  setupEditor(deltaEditor, juce::String::fromUTF8(
                               u8"\uC99D\uAC10\uB7C9 (\uC608: 1.0, -0.5)"));
  targetIdCombo.setTextWhenNothingSelected(
      juce::String::fromUTF8(u8"\uB300\uC0C1 \uC704\uC82F"));
  setupEditor(opacityEditor,
              juce::String::fromUTF8(u8"\uBD88\uD22C\uBA85\uB3C4 (0.0~1.0)"));
  setupEditor(boundsXEditor, "X");
  setupEditor(boundsYEditor, "Y");
  setupEditor(boundsWEditor, juce::String::fromUTF8(u8"\uB108\uBE44"));
  setupEditor(boundsHEditor, juce::String::fromUTF8(u8"\uB192\uC774"));

  paramKeyCombo.onChange = [this] { applySelectedAction(); };
  valueEditor.onReturnKey = [this] { applySelectedAction(); };
  valueEditor.onFocusLost = [this] { applySelectedAction(); };
  deltaEditor.onReturnKey = [this] { applySelectedAction(); };
  deltaEditor.onFocusLost = [this] { applySelectedAction(); };
  targetIdCombo.onChange = [this] { applySelectedAction(); };
  opacityEditor.onReturnKey = [this] { applySelectedAction(); };
  opacityEditor.onFocusLost = [this] { applySelectedAction(); };
  boundsXEditor.onReturnKey = [this] { applySelectedAction(); };
  boundsXEditor.onFocusLost = [this] { applySelectedAction(); };
  boundsYEditor.onReturnKey = [this] { applySelectedAction(); };
  boundsYEditor.onFocusLost = [this] { applySelectedAction(); };
  boundsWEditor.onReturnKey = [this] { applySelectedAction(); };
  boundsWEditor.onFocusLost = [this] { applySelectedAction(); };
  boundsHEditor.onReturnKey = [this] { applySelectedAction(); };
  boundsHEditor.onFocusLost = [this] { applySelectedAction(); };

  addAndMakeVisible(paramKeyCombo);
  addAndMakeVisible(valueEditor);
  addAndMakeVisible(deltaEditor);
  addAndMakeVisible(targetIdCombo);
  addAndMakeVisible(opacityEditor);
  addAndMakeVisible(boundsXEditor);
  addAndMakeVisible(boundsYEditor);
  addAndMakeVisible(boundsWEditor);
  addAndMakeVisible(boundsHEditor);

  // 타겟 종류 콤보 — 한글
  targetKindCombo.addItem(juce::String::fromUTF8(u8"\uC704\uC82F"), 1); // 위젯
  targetKindCombo.addItem(juce::String::fromUTF8(u8"\uADF8\uB8F9"), 2); // 그룹
  targetKindCombo.addItem(juce::String::fromUTF8(u8"\uB808\uC774\uC5B4"),
                          3); // 레이어
  targetKindCombo.onChange = [this] { applySelectedAction(); };
  addAndMakeVisible(targetKindCombo);

  // 보이기/잠금 콤보 — 한글
  visibleCombo.addItem(juce::String::fromUTF8(u8"\uC720\uC9C0"), 1); // 유지
  visibleCombo.addItem(juce::String::fromUTF8(u8"\uBCF4\uC774\uAE30"),
                       2); // 보이기
  visibleCombo.addItem(juce::String::fromUTF8(u8"\uC228\uAE30\uAE30"),
                       3); // 숨기기
  visibleCombo.onChange = [this] { applySelectedAction(); };
  addAndMakeVisible(visibleCombo);

  lockedCombo.addItem(juce::String::fromUTF8(u8"\uC720\uC9C0"), 1); // 유지
  lockedCombo.addItem(juce::String::fromUTF8(u8"\uC7A0\uAE08"), 2); // 잠금
  lockedCombo.addItem(juce::String::fromUTF8(u8"\uC7A0\uAE08 \uD574\uC81C"),
                      3); // 잠금 해제
  lockedCombo.onChange = [this] { applySelectedAction(); };
  addAndMakeVisible(lockedCombo);

  assetPatchKeyCombo.setTextWhenNothingSelected("asset key");
  assetPatchKeyCombo.onChange = [this] {
    if (!suppressCallbacks)
      syncAssetPatchValueEditor();
  };
  addAndMakeVisible(assetPatchKeyCombo);

  assetPatchValueCombo.setTextWhenNothingSelected("asset ref");
  assetPatchValueCombo.setEditableText(true);
  assetPatchValueCombo.onChange = [this] {
    if (!suppressCallbacks)
      applyAssetPatchValue();
  };
  addAndMakeVisible(assetPatchValueCombo);

  patchEditor.setMultiLine(true);
  patchEditor.setScrollbarsShown(true);
  patchEditor.setReturnKeyStartsNewLine(true);
  patchEditor.setColour(juce::TextEditor::backgroundColourId,
                        palette(GyeolPalette::ControlBase));
  patchEditor.setColour(juce::TextEditor::outlineColourId,
                        palette(GyeolPalette::BorderDefault));
  patchEditor.setColour(juce::TextEditor::textColourId,
                        palette(GyeolPalette::TextPrimary));
  patchEditor.onFocusLost = [this] { applySelectedAction(); };
  addAndMakeVisible(patchEditor);

  statusLabel.setJustificationType(juce::Justification::centredLeft);
  statusLabel.setColour(juce::Label::textColourId, kStatusInfo);
  addAndMakeVisible(statusLabel);

  stateHintLabel.setText("Runtime Params + Property Bindings",
                         juce::dontSendNotification);
  stateHintLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  stateHintLabel.setColour(juce::Label::textColourId,
                           palette(GyeolPalette::TextSecondary));
  stateHintLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(stateHintLabel);

  runtimeParamTitleLabel.setText("Runtime Params", juce::dontSendNotification);
  runtimeParamTitleLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
  runtimeParamTitleLabel.setColour(juce::Label::textColourId,
                                   palette(GyeolPalette::TextPrimary));
  runtimeParamTitleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(runtimeParamTitleLabel);

  runtimeParamList.setModel(&runtimeParamListModel);
  runtimeParamList.setRowHeight(40);
  runtimeParamList.setColour(juce::ListBox::backgroundColourId,
                             palette(GyeolPalette::CanvasBackground));
  runtimeParamList.setColour(juce::ListBox::outlineColourId,
                             palette(GyeolPalette::BorderDefault));
  addAndMakeVisible(runtimeParamList);

  addRuntimeParamButton.onClick = [this] { addRuntimeParam(); };
  deleteRuntimeParamButton.onClick = [this] { deleteRuntimeParam(); };
  addAndMakeVisible(addRuntimeParamButton);
  addAndMakeVisible(deleteRuntimeParamButton);

  setupEditor(runtimeParamKeyEditor, "param key");
  setupEditor(runtimeParamDefaultEditor, "default");
  setupEditor(runtimeParamDescriptionEditor, "description");
  runtimeParamKeyEditor.onReturnKey = [this] { applySelectedRuntimeParam(); };
  runtimeParamKeyEditor.onFocusLost = [this] { applySelectedRuntimeParam(); };
  runtimeParamDefaultEditor.onReturnKey = [this] {
    applySelectedRuntimeParam();
  };
  runtimeParamDefaultEditor.onFocusLost = [this] {
    applySelectedRuntimeParam();
  };
  runtimeParamDescriptionEditor.onReturnKey = [this] {
    applySelectedRuntimeParam();
  };
  runtimeParamDescriptionEditor.onFocusLost = [this] {
    applySelectedRuntimeParam();
  };
  addAndMakeVisible(runtimeParamKeyEditor);
  addAndMakeVisible(runtimeParamDefaultEditor);
  addAndMakeVisible(runtimeParamDescriptionEditor);

  runtimeParamTypeCombo.addItem("number", 1);
  runtimeParamTypeCombo.addItem("boolean", 2);
  runtimeParamTypeCombo.addItem("string", 3);
  runtimeParamTypeCombo.onChange = [this] { applySelectedRuntimeParam(); };
  addAndMakeVisible(runtimeParamTypeCombo);

  runtimeParamExposedToggle.setClickingTogglesState(true);
  runtimeParamExposedToggle.onClick = [this] { applySelectedRuntimeParam(); };
  addAndMakeVisible(runtimeParamExposedToggle);

  propertyBindingTitleLabel.setText("Property Bindings",
                                    juce::dontSendNotification);
  propertyBindingTitleLabel.setFont(juce::FontOptions(10.5f, juce::Font::bold));
  propertyBindingTitleLabel.setColour(juce::Label::textColourId,
                                      palette(GyeolPalette::TextPrimary));
  propertyBindingTitleLabel.setJustificationType(
      juce::Justification::centredLeft);
  addAndMakeVisible(propertyBindingTitleLabel);

  propertyBindingList.setModel(&propertyBindingListModel);
  propertyBindingList.setRowHeight(34);
  propertyBindingList.setColour(juce::ListBox::backgroundColourId,
                                palette(GyeolPalette::CanvasBackground));
  propertyBindingList.setColour(juce::ListBox::outlineColourId,
                                palette(GyeolPalette::BorderDefault));
  addAndMakeVisible(propertyBindingList);

  addPropertyBindingButton.onClick = [this] { addPropertyBinding(); };
  deletePropertyBindingButton.onClick = [this] { deletePropertyBinding(); };
  addAndMakeVisible(addPropertyBindingButton);
  addAndMakeVisible(deletePropertyBindingButton);

  setupEditor(propertyBindingNameEditor, "link name");
  propertyBindingTargetIdCombo.setTextWhenNothingSelected("target widget");
  propertyBindingTargetPropertyCombo.setTextWhenNothingSelected(
      "target property");
  setupEditor(propertyBindingExpressionEditor, "expression (ex: A + 3*B)");
  propertyBindingNameEditor.onReturnKey = [this] {
    applySelectedPropertyBinding();
  };
  propertyBindingNameEditor.onFocusLost = [this] {
    applySelectedPropertyBinding();
  };
  propertyBindingTargetIdCombo.onChange = [this] {
    if (suppressCallbacks)
      return;

    const auto selectedTargetId =
        selectedWidgetIdFromCombo(propertyBindingTargetIdCombo);
    if (selectedTargetId.has_value()) {
      rebuildPropertyBindingTargetPropertyOptions(*selectedTargetId, {});
    } else {
      rebuildPropertyBindingTargetPropertyOptions(kRootId, {});
    }
    applySelectedPropertyBinding();
  };
  propertyBindingTargetPropertyCombo.onChange = [this] {
    applySelectedPropertyBinding();
  };
  propertyBindingExpressionEditor.onReturnKey = [this] {
    applySelectedPropertyBinding();
  };
  propertyBindingExpressionEditor.onFocusLost = [this] {
    applySelectedPropertyBinding();
  };
  addAndMakeVisible(propertyBindingNameEditor);
  addAndMakeVisible(propertyBindingTargetIdCombo);
  addAndMakeVisible(propertyBindingTargetPropertyCombo);
  addAndMakeVisible(propertyBindingExpressionEditor);

  propertyBindingEnabledToggle.setClickingTogglesState(true);
  propertyBindingEnabledToggle.onClick = [this] {
    applySelectedPropertyBinding();
  };
  addAndMakeVisible(propertyBindingEnabledToggle);

  refreshFromDocument();
  setPanelMode(PanelMode::eventAction);
}

EventActionPanel::~EventActionPanel() {
  bindingList.setModel(nullptr);
  actionList.setModel(nullptr);
  runtimeParamList.setModel(nullptr);
  propertyBindingList.setModel(nullptr);
}

void EventActionPanel::setBindingsChangedCallback(
    std::function<void()> callback) {
  onBindingsChanged = std::move(callback);
}

void EventActionPanel::refreshFromDocument() {
  const auto &snapshot = document.snapshot();
  bindings = snapshot.runtimeBindings;
  runtimeParams = snapshot.runtimeParams;
  propertyBindings = snapshot.propertyBindings;
  rebuildWidgetOptions();
  const bool filterChanged = syncSelectionFilter();
  rebuildCreateCombos(filterChanged);
  rebuildVisibleBindings();
  restoreSelections();
  refreshDetailEditors();
  refreshStateEditors();
  updatePanelModeVisibility();
}

void EventActionPanel::paint(juce::Graphics &g) {
  g.fillAll(kPanelBg);
  g.setColour(kPanelOutline);
  g.drawRect(getLocalBounds(), 1);

  const auto drawSectionHeader = [&](const juce::Label &label) {
    if (!label.isVisible())
      return;

    auto bounds = label.getBounds().toFloat();
    if (bounds.isEmpty())
      return;

    g.setColour(palette(GyeolPalette::HeaderBackground).withAlpha(0.82f));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(palette(GyeolPalette::BorderDefault).withAlpha(0.92f));
    g.drawLine(bounds.getX() + 4.0f, bounds.getBottom() - 0.5f,
               bounds.getRight() - 4.0f, bounds.getBottom() - 0.5f, 1.0f);
  };

  drawSectionHeader(bindingSectionLabel);
  drawSectionHeader(actionSectionLabel);
  drawSectionHeader(runtimeParamTitleLabel);
  drawSectionHeader(propertyBindingTitleLabel);
}

void EventActionPanel::paintOverChildren(juce::Graphics &g) {
  const auto drawEmptyState = [&](juce::Rectangle<int> bounds,
                                  const juce::String &icon,
                                  const juce::String &text) {
    if (bounds.isEmpty())
      return;

    auto area = bounds.reduced(10);
    g.setColour(palette(GyeolPalette::TextSecondary).withAlpha(0.9f));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawFittedText(icon, area.removeFromTop(14), juce::Justification::centred,
                     1);
    g.setFont(juce::FontOptions(10.0f));
    g.drawFittedText(text, area, juce::Justification::centred, 2);
  };

  if (panelMode == PanelMode::eventAction) {
    if (visibleBindingIndices.empty() && bindingList.isVisible()) {
      drawEmptyState(bindingList.getBounds(), "[ ]", "No events configured");
      return;
    }

    if (actionList.isVisible()) {
      if (const auto *binding = selectedBinding();
          binding != nullptr && binding->actions.empty()) {
        drawEmptyState(actionList.getBounds(), "+",
                       "No actions in this binding");
      }
    }
  }
}

void EventActionPanel::resized() {
  auto area = getLocalBounds().reduced(8);

  auto header = area.removeFromTop(22);
  auto modeArea = header.removeFromRight(172);
  titleLabel.setBounds(header);
  eventModeButton.setBounds(modeArea.removeFromLeft(84));
  modeArea.removeFromLeft(4);
  stateModeButton.setBounds(modeArea.removeFromLeft(84));
  area.removeFromTop(4);

  statusLabel.setBounds(area.removeFromBottom(18));
  area.removeFromBottom(4);

  if (panelMode == PanelMode::stateBinding) {

    stateHintLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);

    runtimeParamTitleLabel.setBounds(area.removeFromTop(18));
    runtimeParamList.setBounds(area.removeFromTop(122));
    area.removeFromTop(4);

    auto paramButtons = area.removeFromTop(24);
    addRuntimeParamButton.setBounds(paramButtons.removeFromLeft(94));
    paramButtons.removeFromLeft(4);
    deleteRuntimeParamButton.setBounds(paramButtons.removeFromLeft(104));
    area.removeFromTop(4);

    auto paramMetaA = area.removeFromTop(24);
    runtimeParamKeyEditor.setBounds(paramMetaA.removeFromLeft(150));
    paramMetaA.removeFromLeft(4);
    runtimeParamTypeCombo.setBounds(paramMetaA.removeFromLeft(96));
    paramMetaA.removeFromLeft(4);
    runtimeParamExposedToggle.setBounds(paramMetaA);
    area.removeFromTop(4);

    auto paramMetaB = area.removeFromTop(24);
    runtimeParamDefaultEditor.setBounds(paramMetaB.removeFromLeft(124));
    paramMetaB.removeFromLeft(4);
    runtimeParamDescriptionEditor.setBounds(paramMetaB);
    area.removeFromTop(6);

    propertyBindingTitleLabel.setBounds(area.removeFromTop(18));
    propertyBindingList.setBounds(area.removeFromTop(138));
    area.removeFromTop(4);

    auto bindingButtons = area.removeFromTop(24);
    addPropertyBindingButton.setBounds(bindingButtons.removeFromLeft(86));
    bindingButtons.removeFromLeft(4);
    deletePropertyBindingButton.setBounds(bindingButtons.removeFromLeft(96));
    area.removeFromTop(4);

    auto bindingMetaA = area.removeFromTop(24);
    propertyBindingNameEditor.setBounds(bindingMetaA.removeFromLeft(160));
    bindingMetaA.removeFromLeft(4);
    propertyBindingEnabledToggle.setBounds(bindingMetaA);
    area.removeFromTop(4);

    auto bindingMetaB = area.removeFromTop(24);
    propertyBindingTargetIdCombo.setBounds(bindingMetaB.removeFromLeft(168));
    bindingMetaB.removeFromLeft(4);
    propertyBindingTargetPropertyCombo.setBounds(bindingMetaB);
    area.removeFromTop(4);

    propertyBindingExpressionEditor.setBounds(area.removeFromTop(24));
    return;
  }

  auto topRow = area.removeFromTop(24);
  auto addBindingArea = topRow.removeFromRight(84);
  topRow.removeFromRight(4);
  sourceCombo.setBounds(
      topRow.removeFromLeft(juce::jmin(170, topRow.getWidth() / 2)));
  topRow.removeFromLeft(4);
  eventCombo.setBounds(topRow);
  addBindingButton.setBounds(addBindingArea);

  area.removeFromTop(4);
  auto searchRow = area.removeFromTop(24);
  showAllWidgetsToggle.setBounds(searchRow.removeFromRight(108));
  searchRow.removeFromRight(4);
  searchEditor.setBounds(searchRow);
  area.removeFromTop(4);
  bindingSectionLabel.setBounds(area.removeFromTop(18));
  area.removeFromTop(2);
  bindingList.setBounds(area.removeFromTop(146));
  area.removeFromTop(6);

  detailTitleLabel.setBounds(area.removeFromTop(20));
  auto metaRow = area.removeFromTop(24);
  auto deleteArea = metaRow.removeFromRight(84);
  metaRow.removeFromRight(4);
  auto duplicateArea = metaRow.removeFromRight(96);
  metaRow.removeFromRight(4);
  bindingEnabledToggle.setBounds(metaRow.removeFromRight(96));
  metaRow.removeFromRight(4);
  bindingNameEditor.setBounds(metaRow);
  duplicateBindingButton.setBounds(duplicateArea);
  deleteBindingButton.setBounds(deleteArea);

  area.removeFromTop(4);
  actionSectionLabel.setBounds(area.removeFromTop(18));
  area.removeFromTop(2);
  actionList.setBounds(area.removeFromTop(102));
  area.removeFromTop(4);

  auto actionButtons = area.removeFromTop(24);
  addActionButton.setBounds(actionButtons.removeFromLeft(100));
  actionButtons.removeFromLeft(4);
  deleteActionButton.setBounds(actionButtons.removeFromLeft(116));
  actionButtons.removeFromLeft(4);
  actionUpButton.setBounds(actionButtons.removeFromLeft(84));
  actionButtons.removeFromLeft(4);
  actionDownButton.setBounds(actionButtons.removeFromLeft(96));

  area.removeFromTop(4);
  const auto *action = selectedAction();
  const auto kind =
      action != nullptr ? action->kind : RuntimeActionKind::setRuntimeParam;

  const auto layoutRow =
      [&area](std::initializer_list<std::pair<juce::Component *, int>> fields) {
        std::vector<std::pair<juce::Component *, int>> visibleFields;
        visibleFields.reserve(fields.size());
        for (const auto &field : fields) {
          if (field.first != nullptr && field.first->isVisible())
            visibleFields.push_back(field);
        }

        if (visibleFields.empty())
          return;

        auto row = area.removeFromTop(24);
        for (size_t i = 0; i < visibleFields.size(); ++i) {
          auto *component = visibleFields[i].first;
          if (component == nullptr)
            continue;

          if (i + 1 == visibleFields.size()) {
            component->setBounds(row);
          } else {
            const auto width =
                juce::jmin(visibleFields[i].second, row.getWidth());
            component->setBounds(row.removeFromLeft(width));
            row.removeFromLeft(4);
          }
        }

        area.removeFromTop(4);
      };

  switch (kind) {
  case RuntimeActionKind::setRuntimeParam:
    layoutRow(
        {{&actionKindCombo, 168}, {&paramKeyCombo, 184}, {&valueEditor, 200}});
    break;
  case RuntimeActionKind::adjustRuntimeParam:
    layoutRow(
        {{&actionKindCombo, 168}, {&paramKeyCombo, 188}, {&deltaEditor, 120}});
    break;
  case RuntimeActionKind::toggleRuntimeParam:
    layoutRow({{&actionKindCombo, 168}, {&paramKeyCombo, 230}});
    break;
  case RuntimeActionKind::setNodeProps:
    layoutRow({{&actionKindCombo, 176},
               {&targetKindCombo, 88},
               {&targetIdCombo, 180}});
    layoutRow({{&visibleCombo, 92}, {&lockedCombo, 92}, {&opacityEditor, 100}});
    layoutRow({{&assetPatchKeyCombo, 180}, {&assetPatchValueCombo, 220}});
    if (patchEditor.isVisible()) {
      patchEditor.setBounds(area.removeFromTop(56));
      area.removeFromTop(4);
    }
    break;
  case RuntimeActionKind::setNodeBounds:
    layoutRow({{&actionKindCombo, 176}, {&targetIdCombo, 192}});
    layoutRow({{&boundsXEditor, 76},
               {&boundsYEditor, 76},
               {&boundsWEditor, 76},
               {&boundsHEditor, 76}});
    break;
  }
}

int EventActionPanel::selectedBindingModelIndex() const {
  const auto row = bindingList.getSelectedRow();
  if (row < 0 || row >= static_cast<int>(visibleBindingIndices.size()))
    return -1;
  return visibleBindingIndices[static_cast<size_t>(row)];
}

RuntimeBindingModel *EventActionPanel::selectedBinding() {
  const auto index = selectedBindingModelIndex();
  if (index < 0 || index >= static_cast<int>(bindings.size()))
    return nullptr;
  return &bindings[static_cast<size_t>(index)];
}

const RuntimeBindingModel *EventActionPanel::selectedBinding() const {
  const auto index = selectedBindingModelIndex();
  if (index < 0 || index >= static_cast<int>(bindings.size()))
    return nullptr;
  return &bindings[static_cast<size_t>(index)];
}

RuntimeActionModel *EventActionPanel::selectedAction() {
  auto *binding = selectedBinding();
  const auto row = actionList.getSelectedRow();
  if (binding == nullptr || row < 0 ||
      row >= static_cast<int>(binding->actions.size()))
    return nullptr;
  return &binding->actions[static_cast<size_t>(row)];
}

const RuntimeActionModel *EventActionPanel::selectedAction() const {
  const auto *binding = selectedBinding();
  const auto row = actionList.getSelectedRow();
  if (binding == nullptr || row < 0 ||
      row >= static_cast<int>(binding->actions.size()))
    return nullptr;
  return &binding->actions[static_cast<size_t>(row)];
}

int EventActionPanel::selectedRuntimeParamIndex() const {
  const auto row = runtimeParamList.getSelectedRow();
  if (row < 0 || row >= static_cast<int>(runtimeParams.size()))
    return -1;
  return row;
}

RuntimeParamModel *EventActionPanel::selectedRuntimeParam() {
  const auto index = selectedRuntimeParamIndex();
  if (index < 0 || index >= static_cast<int>(runtimeParams.size()))
    return nullptr;
  return &runtimeParams[static_cast<size_t>(index)];
}

const RuntimeParamModel *EventActionPanel::selectedRuntimeParam() const {
  const auto index = selectedRuntimeParamIndex();
  if (index < 0 || index >= static_cast<int>(runtimeParams.size()))
    return nullptr;
  return &runtimeParams[static_cast<size_t>(index)];
}

int EventActionPanel::selectedPropertyBindingIndex() const {
  const auto row = propertyBindingList.getSelectedRow();
  if (row < 0 || row >= static_cast<int>(propertyBindings.size()))
    return -1;
  return row;
}

PropertyBindingModel *EventActionPanel::selectedPropertyBinding() {
  const auto index = selectedPropertyBindingIndex();
  if (index < 0 || index >= static_cast<int>(propertyBindings.size()))
    return nullptr;
  return &propertyBindings[static_cast<size_t>(index)];
}

const PropertyBindingModel *EventActionPanel::selectedPropertyBinding() const {
  const auto index = selectedPropertyBindingIndex();
  if (index < 0 || index >= static_cast<int>(propertyBindings.size()))
    return nullptr;
  return &propertyBindings[static_cast<size_t>(index)];
}

void EventActionPanel::setPanelMode(PanelMode mode) {
  if (panelMode == mode)
    return;

  panelMode = mode;
  updatePanelModeVisibility();
  refreshStateEditors();
  resized();
  repaint();
}

void EventActionPanel::updatePanelModeVisibility() {
  const auto showEventAction = panelMode == PanelMode::eventAction;
  const auto showState = panelMode == PanelMode::stateBinding;

  suppressCallbacks = true;
  eventModeButton.setToggleState(showEventAction, juce::dontSendNotification);
  stateModeButton.setToggleState(showState, juce::dontSendNotification);
  suppressCallbacks = false;

  auto setEventVisibility = [showEventAction](juce::Component &component) {
    component.setVisible(showEventAction);
  };

  setEventVisibility(sourceCombo);
  setEventVisibility(eventCombo);
  setEventVisibility(addBindingButton);
  setEventVisibility(searchEditor);
  setEventVisibility(showAllWidgetsToggle);
  setEventVisibility(bindingSectionLabel);
  setEventVisibility(bindingList);
  setEventVisibility(detailTitleLabel);
  setEventVisibility(bindingNameEditor);
  setEventVisibility(bindingEnabledToggle);
  setEventVisibility(duplicateBindingButton);
  setEventVisibility(deleteBindingButton);
  setEventVisibility(actionSectionLabel);
  setEventVisibility(actionList);
  setEventVisibility(addActionButton);
  setEventVisibility(deleteActionButton);
  setEventVisibility(actionUpButton);
  setEventVisibility(actionDownButton);
  setEventVisibility(actionKindCombo);
  setEventVisibility(paramKeyCombo);
  setEventVisibility(valueEditor);
  setEventVisibility(deltaEditor);
  setEventVisibility(targetKindCombo);
  setEventVisibility(targetIdCombo);
  setEventVisibility(visibleCombo);
  setEventVisibility(lockedCombo);
  setEventVisibility(opacityEditor);
  setEventVisibility(assetPatchKeyCombo);
  setEventVisibility(assetPatchValueCombo);
  setEventVisibility(patchEditor);
  setEventVisibility(boundsXEditor);
  setEventVisibility(boundsYEditor);
  setEventVisibility(boundsWEditor);
  setEventVisibility(boundsHEditor);

  stateHintLabel.setVisible(showState);
  runtimeParamTitleLabel.setVisible(showState);
  runtimeParamList.setVisible(showState);
  addRuntimeParamButton.setVisible(showState);
  deleteRuntimeParamButton.setVisible(showState);
  runtimeParamKeyEditor.setVisible(showState);
  runtimeParamTypeCombo.setVisible(showState);
  runtimeParamDefaultEditor.setVisible(showState);
  runtimeParamDescriptionEditor.setVisible(showState);
  runtimeParamExposedToggle.setVisible(showState);
  propertyBindingTitleLabel.setVisible(showState);
  propertyBindingList.setVisible(showState);
  addPropertyBindingButton.setVisible(showState);
  deletePropertyBindingButton.setVisible(showState);
  propertyBindingNameEditor.setVisible(showState);
  propertyBindingEnabledToggle.setVisible(showState);
  propertyBindingTargetIdCombo.setVisible(showState);
  propertyBindingTargetPropertyCombo.setVisible(showState);
  propertyBindingExpressionEditor.setVisible(showState);
}

void EventActionPanel::refreshStateEditors() {
  suppressCallbacks = true;

  runtimeParamList.updateContent();
  runtimeParamList.repaint();
  if (runtimeParams.empty()) {
    selectedRuntimeParamRow = -1;
    runtimeParamList.deselectAllRows();
  } else {
    selectedRuntimeParamRow = juce::jlimit(
        0, static_cast<int>(runtimeParams.size()) - 1, selectedRuntimeParamRow);
    runtimeParamList.selectRow(selectedRuntimeParamRow);
  }

  const auto *selectedParam = selectedRuntimeParam();
  const auto hasParam = selectedParam != nullptr;
  runtimeParamKeyEditor.setEnabled(hasParam);
  runtimeParamTypeCombo.setEnabled(hasParam);
  runtimeParamDefaultEditor.setEnabled(hasParam);
  runtimeParamDescriptionEditor.setEnabled(hasParam);
  runtimeParamExposedToggle.setEnabled(hasParam);
  deleteRuntimeParamButton.setEnabled(hasParam);

  if (hasParam) {
    runtimeParamKeyEditor.setText(selectedParam->key,
                                  juce::dontSendNotification);
    runtimeParamTypeCombo.setSelectedId(
        runtimeParamTypeToComboId(selectedParam->type),
        juce::dontSendNotification);
    if (selectedParam->type == RuntimeParamValueType::number)
      runtimeParamDefaultEditor.setText(
          juce::String(static_cast<double>(selectedParam->defaultValue), 8),
          juce::dontSendNotification);
    else if (selectedParam->type == RuntimeParamValueType::boolean)
      runtimeParamDefaultEditor.setText(
          static_cast<bool>(selectedParam->defaultValue) ? "true" : "false",
          juce::dontSendNotification);
    else
      runtimeParamDefaultEditor.setText(selectedParam->defaultValue.toString(),
                                        juce::dontSendNotification);
    runtimeParamDescriptionEditor.setText(selectedParam->description,
                                          juce::dontSendNotification);
    runtimeParamExposedToggle.setToggleState(selectedParam->exposed,
                                             juce::dontSendNotification);
  } else {
    runtimeParamKeyEditor.clear();
    runtimeParamTypeCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
    runtimeParamDefaultEditor.clear();
    runtimeParamDescriptionEditor.clear();
    runtimeParamExposedToggle.setToggleState(false, juce::dontSendNotification);
  }

  propertyBindingList.updateContent();
  propertyBindingList.repaint();
  if (propertyBindings.empty()) {
    selectedPropertyBindingRow = -1;
    propertyBindingList.deselectAllRows();
  } else {
    selectedPropertyBindingRow =
        juce::jlimit(0, static_cast<int>(propertyBindings.size()) - 1,
                     selectedPropertyBindingRow);
    propertyBindingList.selectRow(selectedPropertyBindingRow);
  }

  const auto *selectedPropertyBindingModel = selectedPropertyBinding();
  const auto hasPropertyBinding = selectedPropertyBindingModel != nullptr;
  rebuildPropertyBindingTargetOptions();
  propertyBindingNameEditor.setEnabled(hasPropertyBinding);
  propertyBindingEnabledToggle.setEnabled(hasPropertyBinding);
  propertyBindingTargetIdCombo.setEnabled(hasPropertyBinding);
  propertyBindingTargetPropertyCombo.setEnabled(hasPropertyBinding);
  propertyBindingExpressionEditor.setEnabled(hasPropertyBinding);
  deletePropertyBindingButton.setEnabled(hasPropertyBinding);

  if (hasPropertyBinding) {
    propertyBindingNameEditor.setText(selectedPropertyBindingModel->name,
                                      juce::dontSendNotification);
    propertyBindingEnabledToggle.setToggleState(
        selectedPropertyBindingModel->enabled, juce::dontSendNotification);
    selectWidgetIdInCombo(propertyBindingTargetIdCombo,
                          selectedPropertyBindingModel->targetWidgetId);
    rebuildPropertyBindingTargetPropertyOptions(
        selectedPropertyBindingModel->targetWidgetId,
        selectedPropertyBindingModel->targetProperty);
    propertyBindingExpressionEditor.setText(
        selectedPropertyBindingModel->expression, juce::dontSendNotification);

    const auto validationError =
        validatePropertyBindingForUi(*selectedPropertyBindingModel);
    if (validationError.isNotEmpty())
      setStatus("Binding error: " + validationError, kStatusWarn);
  } else {
    propertyBindingNameEditor.clear();
    propertyBindingEnabledToggle.setToggleState(false,
                                                juce::dontSendNotification);
    propertyBindingTargetIdCombo.setSelectedItemIndex(
        -1, juce::dontSendNotification);
    rebuildPropertyBindingTargetPropertyOptions(kRootId, {});
    propertyBindingExpressionEditor.clear();
  }

  suppressCallbacks = false;
}

void EventActionPanel::rebuildWidgetOptions() {
  widgetOptions.clear();
  const auto &snapshot = document.snapshot();
  widgetOptions.reserve(snapshot.widgets.size());

  for (const auto &widget : snapshot.widgets) {
    WidgetOption option;
    option.id = widget.id;
    if (const auto *descriptor = registry.find(widget.type);
        descriptor != nullptr) {
      option.events = descriptor->runtimeEvents;
    }
    // 사용자 지정 이름(properties["name"]) 우선, 없으면 타입명 + ID
    option.label = widgetDisplayName(widget, registry);
    widgetOptions.push_back(std::move(option));
  }

  std::sort(widgetOptions.begin(), widgetOptions.end(),
            [](const WidgetOption &lhs, const WidgetOption &rhs) {
              return lhs.id < rhs.id;
            });
}

void EventActionPanel::rebuildRuntimeParamKeyOptions() {
  const auto previousText = paramKeyCombo.getText().trim();
  paramKeyCombo.clear(juce::dontSendNotification);

  std::vector<juce::String> keys;
  keys.reserve(runtimeParams.size());

  int itemId = 1;
  for (const auto &param : runtimeParams) {
    const auto key = param.key.trim();
    if (key.isEmpty())
      continue;
    if (std::find(keys.begin(), keys.end(), key) != keys.end())
      continue;
    keys.push_back(key);
    paramKeyCombo.addItem(key, itemId++);
  }

  if (previousText.isNotEmpty()) {
    const auto it = std::find(keys.begin(), keys.end(), previousText);
    if (it != keys.end()) {
      const auto index = static_cast<int>(std::distance(keys.begin(), it));
      paramKeyCombo.setSelectedItemIndex(index, juce::dontSendNotification);
    } else {
      paramKeyCombo.setText(previousText, juce::dontSendNotification);
    }
  } else {
    paramKeyCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
  }
}

void EventActionPanel::rebuildActionTargetOptions() {
  const auto previousTargetId = selectedWidgetIdFromCombo(targetIdCombo);
  targetIdCombo.clear(juce::dontSendNotification);

  int itemId = 1;
  for (const auto &option : widgetOptions)
    targetIdCombo.addItem(option.label, itemId++);

  if (previousTargetId.has_value())
    selectWidgetIdInCombo(targetIdCombo, *previousTargetId);
  else
    targetIdCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
}

void EventActionPanel::rebuildPropertyBindingTargetOptions() {
  const auto previousTargetId =
      selectedWidgetIdFromCombo(propertyBindingTargetIdCombo);
  propertyBindingTargetIdCombo.clear(juce::dontSendNotification);

  int itemId = 1;
  for (const auto &option : widgetOptions)
    propertyBindingTargetIdCombo.addItem(option.label, itemId++);

  if (previousTargetId.has_value())
    selectWidgetIdInCombo(propertyBindingTargetIdCombo, *previousTargetId);
  else
    propertyBindingTargetIdCombo.setSelectedItemIndex(
        -1, juce::dontSendNotification);
}

void EventActionPanel::rebuildPropertyBindingTargetPropertyOptions(
    WidgetId targetWidgetId, const juce::String &selectedProperty) {
  propertyBindingTargetPropertyCombo.clear(juce::dontSendNotification);
  propertyBindingTargetPropertyKeys.clear();

  if (targetWidgetId <= kRootId)
    return;

  const auto &snapshot = document.snapshot();
  const auto widgetIt =
      std::find_if(snapshot.widgets.begin(), snapshot.widgets.end(),
                   [targetWidgetId](const WidgetModel &widget) {
                     return widget.id == targetWidgetId;
                   });
  if (widgetIt == snapshot.widgets.end())
    return;

  const auto addPropertyItem = [this](const juce::String &key,
                                      const juce::String &label) {
    if (key.trim().isEmpty())
      return;
    if (std::find(propertyBindingTargetPropertyKeys.begin(),
                  propertyBindingTargetPropertyKeys.end(),
                  key) != propertyBindingTargetPropertyKeys.end()) {
      return;
    }

    const auto itemId =
        static_cast<int>(propertyBindingTargetPropertyKeys.size()) + 1;
    propertyBindingTargetPropertyCombo.addItem(label, itemId);
    propertyBindingTargetPropertyKeys.push_back(key);
  };

  if (const auto *specs = registry.propertySpecs(widgetIt->type);
      specs != nullptr) {
    for (const auto &spec : *specs) {
      const auto key = spec.key.toString().trim();
      if (key.isEmpty())
        continue;
      const auto label = spec.label.trim();
      const auto displayText =
          label.isNotEmpty() && label != key ? (label + " (" + key + ")") : key;
      addPropertyItem(key, displayText);
    }
  }

  for (int i = 0; i < widgetIt->properties.size(); ++i) {
    const auto key = widgetIt->properties.getName(i).toString().trim();
    if (key.isEmpty())
      continue;
    addPropertyItem(key, key);
  }

  const auto preferredProperty = selectedProperty.trim();
  if (preferredProperty.isNotEmpty() &&
      std::find(propertyBindingTargetPropertyKeys.begin(),
                propertyBindingTargetPropertyKeys.end(),
                preferredProperty) == propertyBindingTargetPropertyKeys.end()) {
    addPropertyItem(preferredProperty, preferredProperty + " (custom)");
  }

  if (preferredProperty.isNotEmpty()) {
    const auto it =
        std::find(propertyBindingTargetPropertyKeys.begin(),
                  propertyBindingTargetPropertyKeys.end(), preferredProperty);
    if (it != propertyBindingTargetPropertyKeys.end()) {
      const auto index = static_cast<int>(
          std::distance(propertyBindingTargetPropertyKeys.begin(), it));
      propertyBindingTargetPropertyCombo.setSelectedItemIndex(
          index, juce::dontSendNotification);
      return;
    }
  }

  if (propertyBindingTargetPropertyCombo.getNumItems() > 0)
    propertyBindingTargetPropertyCombo.setSelectedItemIndex(
        0, juce::dontSendNotification);
}

std::optional<WidgetId>
EventActionPanel::selectedWidgetIdFromCombo(const juce::ComboBox &combo) const {
  const auto index = combo.getSelectedItemIndex();
  if (index >= 0 && index < static_cast<int>(widgetOptions.size()))
    return widgetOptions[static_cast<size_t>(index)].id;

  WidgetId parsed = kRootId;
  if (parseWidgetId(combo.getText(), parsed))
    return parsed;

  return std::nullopt;
}

void EventActionPanel::selectWidgetIdInCombo(juce::ComboBox &combo,
                                             WidgetId id) {
  const auto it = std::find_if(
      widgetOptions.begin(), widgetOptions.end(),
      [id](const WidgetOption &option) { return option.id == id; });
  if (it == widgetOptions.end()) {
    combo.setSelectedItemIndex(-1, juce::dontSendNotification);
    return;
  }

  const auto index = static_cast<int>(std::distance(widgetOptions.begin(), it));
  combo.setSelectedItemIndex(index, juce::dontSendNotification);
}

juce::String
EventActionPanel::selectedPropertyBindingTargetPropertyKey() const {
  const auto index = propertyBindingTargetPropertyCombo.getSelectedItemIndex();
  if (index >= 0 &&
      index < static_cast<int>(propertyBindingTargetPropertyKeys.size())) {
    return propertyBindingTargetPropertyKeys[static_cast<size_t>(index)].trim();
  }

  return propertyBindingTargetPropertyCombo.getText().trim();
}
void EventActionPanel::rebuildCreateCombos(bool forceSelectionMatch) {
  const auto previousSourceIndex = sourceCombo.getSelectedItemIndex();

  suppressCallbacks = true;
  sourceCombo.clear(juce::dontSendNotification);
  int sourceItemId = 1;
  for (const auto &source : widgetOptions)
    sourceCombo.addItem(source.label, sourceItemId++);

  if (!widgetOptions.empty()) {
    if (forceSelectionMatch && selectionFilterWidgetId > kRootId) {
      selectWidgetIdInCombo(sourceCombo, selectionFilterWidgetId);
    } else {
      const auto safeIndex = juce::jlimit(
          0, static_cast<int>(widgetOptions.size()) - 1, previousSourceIndex);
      sourceCombo.setSelectedItemIndex(safeIndex, juce::dontSendNotification);
    }
  }

  eventCombo.clear(juce::dontSendNotification);
  const auto sourceIndex = sourceCombo.getSelectedItemIndex();
  if (sourceIndex >= 0 &&
      sourceIndex < static_cast<int>(widgetOptions.size())) {
    int eventItemId = 1;
    for (const auto &eventSpec :
         widgetOptions[static_cast<size_t>(sourceIndex)].events)
      eventCombo.addItem(formatEventLabel(eventSpec), eventItemId++);
    if (eventCombo.getNumItems() > 0)
      eventCombo.setSelectedItemIndex(0, juce::dontSendNotification);
  }

  addBindingButton.setEnabled(eventCombo.getNumItems() > 0);
  suppressCallbacks = false;
}

bool EventActionPanel::syncSelectionFilter() {
  const auto &selection = document.editorState().selection;
  if (selection == lastEditorSelection)
    return false;

  if (!showAllWidgetsToggle.getToggleState() && hasImplicitPinFocus())
    return false;

  lastEditorSelection = selection;
  selectionFilterWidgetId = selection.empty() ? kRootId : selection.front();
  return true;
}

bool EventActionPanel::bindingMatchesSelectionFilter(
    const RuntimeBindingModel &binding) const {
  if (showAllWidgetsToggle.getToggleState() ||
      selectionFilterWidgetId <= kRootId) {
    return true;
  }

  if (binding.sourceWidgetId == selectionFilterWidgetId)
    return true;

  return std::any_of(binding.actions.begin(), binding.actions.end(),
                     [this](const RuntimeActionModel &action) {
                       if (action.kind == RuntimeActionKind::setNodeProps) {
                         return action.target.kind == NodeKind::widget &&
                                action.target.id == selectionFilterWidgetId;
                       }
                       if (action.kind == RuntimeActionKind::setNodeBounds)
                         return action.targetWidgetId ==
                                selectionFilterWidgetId;
                       return false;
                     });
}

bool EventActionPanel::isTextEditorPendingCommit(
    const juce::TextEditor *editor) const {
  if (editor == nullptr || editor == &searchEditor)
    return false;

  if (editor == &bindingNameEditor) {
    const auto *binding = selectedBinding();
    return binding != nullptr && editor->getText().trim() != binding->name;
  }

  const auto *action = selectedAction();
  if (editor == &valueEditor) {
    if (action == nullptr || action->kind != RuntimeActionKind::setRuntimeParam)
      return false;
    return editor->getText().trim() != runtimeValueToString(action->value);
  }

  if (editor == &deltaEditor) {
    if (action == nullptr ||
        action->kind != RuntimeActionKind::adjustRuntimeParam) {
      return false;
    }
    return editor->getText().trim() != juce::String(action->delta, 6);
  }

  if (editor == &opacityEditor) {
    if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps)
      return false;
    const auto expected = action->opacity.has_value()
                              ? juce::String(*action->opacity, 4)
                              : juce::String();
    return editor->getText().trim() != expected;
  }

  if (editor == &patchEditor) {
    if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps)
      return false;
    const auto expected =
        action->patch.size() > 0
            ? juce::JSON::toString(patchToVar(action->patch), true)
            : juce::String();
    return editor->getText() != expected;
  }

  if (editor == &boundsXEditor) {
    if (action == nullptr || action->kind != RuntimeActionKind::setNodeBounds)
      return false;
    return editor->getText().trim() != juce::String(action->bounds.getX(), 4);
  }

  if (editor == &boundsYEditor) {
    if (action == nullptr || action->kind != RuntimeActionKind::setNodeBounds)
      return false;
    return editor->getText().trim() != juce::String(action->bounds.getY(), 4);
  }

  if (editor == &boundsWEditor) {
    if (action == nullptr || action->kind != RuntimeActionKind::setNodeBounds)
      return false;
    return editor->getText().trim() !=
           juce::String(action->bounds.getWidth(), 4);
  }

  if (editor == &boundsHEditor) {
    if (action == nullptr || action->kind != RuntimeActionKind::setNodeBounds)
      return false;
    return editor->getText().trim() !=
           juce::String(action->bounds.getHeight(), 4);
  }

  if (editor == &runtimeParamKeyEditor) {
    const auto *param = selectedRuntimeParam();
    return param != nullptr && editor->getText().trim() != param->key;
  }

  if (editor == &runtimeParamDefaultEditor) {
    const auto *param = selectedRuntimeParam();
    if (param == nullptr)
      return false;

    if (param->type == RuntimeParamValueType::number) {
      return editor->getText().trim() !=
             juce::String(static_cast<double>(param->defaultValue), 8);
    }
    if (param->type == RuntimeParamValueType::boolean) {
      return editor->getText().trim() !=
             (static_cast<bool>(param->defaultValue) ? "true" : "false");
    }
    return editor->getText() != param->defaultValue.toString();
  }

  if (editor == &runtimeParamDescriptionEditor) {
    const auto *param = selectedRuntimeParam();
    return param != nullptr && editor->getText().trim() != param->description;
  }

  if (editor == &propertyBindingNameEditor) {
    const auto *binding = selectedPropertyBinding();
    return binding != nullptr && editor->getText().trim() != binding->name;
  }

  if (editor == &propertyBindingExpressionEditor) {
    const auto *binding = selectedPropertyBinding();
    return binding != nullptr &&
           editor->getText().trim() != binding->expression;
  }

  return false;
}

bool EventActionPanel::isComboBoxPendingCommit(
    const juce::ComboBox *combo) const {
  if (combo == nullptr || !combo->isTextEditable())
    return false;

  if (combo == &paramKeyCombo) {
    const auto *action = selectedAction();
    if (action == nullptr)
      return false;

    const auto isParamAction =
        action->kind == RuntimeActionKind::setRuntimeParam ||
        action->kind == RuntimeActionKind::adjustRuntimeParam ||
        action->kind == RuntimeActionKind::toggleRuntimeParam;
    return isParamAction && combo->getText().trim() != action->paramKey;
  }

  if (combo == &assetPatchValueCombo) {
    const auto *action = selectedAction();
    if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps ||
        assetPatchKeys.empty()) {
      return false;
    }

    const auto keyIndex = assetPatchKeyCombo.getSelectedItemIndex();
    if (keyIndex < 0 || keyIndex >= static_cast<int>(assetPatchKeys.size()))
      return false;

    auto selectedRef = combo->getText().trim();
    const auto selectedIndex = combo->getSelectedItemIndex();
    if (selectedIndex >= 0 &&
        selectedIndex < static_cast<int>(assetPatchValues.size())) {
      selectedRef = assetPatchValues[static_cast<size_t>(selectedIndex)];
    }

    const auto key = assetPatchKeys[static_cast<size_t>(keyIndex)];
    const auto *patchValue = action->patch.getVarPointer(key);
    const auto expected =
        patchValue != nullptr ? patchValue->toString().trim() : juce::String();
    return selectedRef != expected;
  }

  return false;
}

bool EventActionPanel::hasImplicitPinFocus() const {
  auto *focused = juce::Component::getCurrentlyFocusedComponent();
  if (focused == nullptr || !isParentOf(focused))
    return false;

  if (auto *editor = dynamic_cast<juce::TextEditor *>(focused);
      editor != nullptr && isTextEditorPendingCommit(editor)) {
    return true;
  }

  if (auto *label = dynamic_cast<juce::Label *>(focused);
      label != nullptr && label->isBeingEdited()) {
    return true;
  }

  // Editable ComboBox keeps a child Label/TextEditor focused while
  // typing. Walk up the parent chain and pin only when text edits are
  // pending.
  for (auto *node = focused; node != nullptr && node != this;
       node = node->getParentComponent()) {
    if (auto *combo = dynamic_cast<juce::ComboBox *>(node);
        combo != nullptr && combo->isTextEditable() &&
        isComboBoxPendingCommit(combo)) {
      return true;
    }

    if (auto *label = dynamic_cast<juce::Label *>(node);
        label != nullptr && label->isBeingEdited()) {
      return true;
    }
  }

  return false;
}

void EventActionPanel::rebuildVisibleBindings() {
  visibleBindingIndices.clear();
  const auto filter = searchEditor.getText().trim().toLowerCase();

  for (int i = 0; i < static_cast<int>(bindings.size()); ++i) {
    const auto &binding = bindings[static_cast<size_t>(i)];
    if (!bindingMatchesSelectionFilter(binding))
      continue;

    const auto sourceText =
        findWidgetOption(binding.sourceWidgetId).has_value()
            ? findWidgetOption(binding.sourceWidgetId)->label
            : ("Widget #" + juce::String(binding.sourceWidgetId));
    const auto haystack =
        (binding.name + " " + sourceText + " " +
         formatEventLabel(binding.sourceWidgetId, binding.eventKey))
            .toLowerCase();
    if (filter.isEmpty() || haystack.contains(filter))
      visibleBindingIndices.push_back(i);
  }

  bindingList.updateContent();
  bindingList.repaint();
}

void EventActionPanel::restoreSelections() {
  int targetRow = -1;
  if (selectedBindingId > kRootId) {
    for (int row = 0; row < static_cast<int>(visibleBindingIndices.size());
         ++row) {
      const auto index = visibleBindingIndices[static_cast<size_t>(row)];
      if (bindings[static_cast<size_t>(index)].id == selectedBindingId) {
        targetRow = row;
        break;
      }
    }
  }

  if (targetRow < 0 && !visibleBindingIndices.empty()) {
    targetRow = 0;
    selectedBindingId =
        bindings[static_cast<size_t>(visibleBindingIndices.front())].id;
  }

  suppressCallbacks = true;
  if (targetRow >= 0)
    bindingList.selectRow(targetRow);
  else
    bindingList.deselectAllRows();
  suppressCallbacks = false;
}

void EventActionPanel::refreshDetailEditors() {
  const auto *binding = selectedBinding();
  const auto hasBinding = binding != nullptr;

  suppressCallbacks = true;
  detailTitleLabel.setText(
      hasBinding ? ("Binding Detail #" + juce::String(binding->id))
                 : juce::String("Binding Detail"),
      juce::dontSendNotification);
  bindingNameEditor.setEnabled(hasBinding);
  bindingEnabledToggle.setEnabled(hasBinding);
  duplicateBindingButton.setEnabled(hasBinding);
  deleteBindingButton.setEnabled(hasBinding);

  if (hasBinding) {
    bindingNameEditor.setText(binding->name, juce::dontSendNotification);
    bindingEnabledToggle.setToggleState(binding->enabled,
                                        juce::dontSendNotification);
  } else {
    bindingNameEditor.clear();
    bindingEnabledToggle.setToggleState(false, juce::dontSendNotification);
  }

  actionList.updateContent();
  actionList.repaint();
  if (hasBinding && !binding->actions.empty()) {
    selectedActionRow = juce::jlimit(
        0, static_cast<int>(binding->actions.size()) - 1, selectedActionRow);
    actionList.selectRow(selectedActionRow);
  } else {
    selectedActionRow = -1;
    actionList.deselectAllRows();
  }

  auto *action = selectedAction();
  const auto hasAction = action != nullptr;
  addActionButton.setEnabled(hasBinding);
  deleteActionButton.setEnabled(hasAction);
  actionUpButton.setEnabled(hasAction && actionList.getSelectedRow() > 0);
  actionDownButton.setEnabled(hasAction && binding != nullptr &&
                              actionList.getSelectedRow() <
                                  static_cast<int>(binding->actions.size()) -
                                      1);

  updateActionEditorVisibility(action, hasAction);
  rebuildRuntimeParamKeyOptions();
  rebuildActionTargetOptions();
  actionKindCombo.setEnabled(hasAction && actionKindCombo.isVisible());
  paramKeyCombo.setEnabled(hasAction && paramKeyCombo.isVisible());
  valueEditor.setEnabled(hasAction && valueEditor.isVisible());
  deltaEditor.setEnabled(hasAction && deltaEditor.isVisible());
  targetKindCombo.setEnabled(hasAction && targetKindCombo.isVisible());
  targetIdCombo.setEnabled(hasAction && targetIdCombo.isVisible());
  visibleCombo.setEnabled(hasAction && visibleCombo.isVisible());
  lockedCombo.setEnabled(hasAction && lockedCombo.isVisible());
  opacityEditor.setEnabled(hasAction && opacityEditor.isVisible());
  assetPatchKeyCombo.setEnabled(hasAction && assetPatchKeyCombo.isVisible());
  assetPatchValueCombo.setEnabled(hasAction &&
                                  assetPatchValueCombo.isVisible());
  patchEditor.setEnabled(hasAction && patchEditor.isVisible());
  boundsXEditor.setEnabled(hasAction && boundsXEditor.isVisible());
  boundsYEditor.setEnabled(hasAction && boundsYEditor.isVisible());
  boundsWEditor.setEnabled(hasAction && boundsWEditor.isVisible());
  boundsHEditor.setEnabled(hasAction && boundsHEditor.isVisible());

  if (hasAction) {
    actionKindCombo.setSelectedId(actionKindToComboId(action->kind),
                                  juce::dontSendNotification);
    paramKeyCombo.setText(action->paramKey, juce::dontSendNotification);
    valueEditor.setText(runtimeValueToString(action->value),
                        juce::dontSendNotification);
    deltaEditor.setText(juce::String(action->delta, 6),
                        juce::dontSendNotification);
    targetKindCombo.setSelectedId(nodeKindToComboId(action->target.kind),
                                  juce::dontSendNotification);
    const auto targetIdForEditor =
        action->kind == RuntimeActionKind::setNodeBounds
            ? action->targetWidgetId
            : action->target.id;
    selectWidgetIdInCombo(targetIdCombo, targetIdForEditor);
    visibleCombo.setSelectedId(
        !action->visible.has_value() ? 1 : (*action->visible ? 2 : 3),
        juce::dontSendNotification);
    lockedCombo.setSelectedId(
        !action->locked.has_value() ? 1 : (*action->locked ? 2 : 3),
        juce::dontSendNotification);
    opacityEditor.setText(action->opacity.has_value()
                              ? juce::String(*action->opacity, 4)
                              : juce::String(),
                          juce::dontSendNotification);
    patchEditor.setText(
        action->patch.size() > 0
            ? juce::JSON::toString(patchToVar(action->patch), true)
            : juce::String(),
        juce::dontSendNotification);
    boundsXEditor.setText(juce::String(action->bounds.getX(), 4),
                          juce::dontSendNotification);
    boundsYEditor.setText(juce::String(action->bounds.getY(), 4),
                          juce::dontSendNotification);
    boundsWEditor.setText(juce::String(action->bounds.getWidth(), 4),
                          juce::dontSendNotification);
    boundsHEditor.setText(juce::String(action->bounds.getHeight(), 4),
                          juce::dontSendNotification);
  } else {
    actionKindCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
    paramKeyCombo.setText(juce::String(), juce::dontSendNotification);
    valueEditor.clear();
    deltaEditor.clear();
    targetKindCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
    targetIdCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
    visibleCombo.setSelectedId(1, juce::dontSendNotification);
    lockedCombo.setSelectedId(1, juce::dontSendNotification);
    opacityEditor.clear();
    patchEditor.clear();
    boundsXEditor.clear();
    boundsYEditor.clear();
    boundsWEditor.clear();
    boundsHEditor.clear();
  }

  rebuildAssetPatchEditors(hasAction ? action : nullptr);
  updateActionEditorVisibility(action, hasAction);
  assetPatchKeyCombo.setEnabled(hasAction && assetPatchKeyCombo.isVisible() &&
                                !assetPatchKeys.empty());
  assetPatchValueCombo.setEnabled(
      hasAction && assetPatchValueCombo.isVisible() && !assetPatchKeys.empty());

  resized();
  suppressCallbacks = false;
  repaint();
}

void EventActionPanel::updateActionEditorVisibility(
    const RuntimeActionModel *action, bool hasAction) {
  const auto setVisibility = [hasAction](juce::Component &component,
                                         bool visibleWhenActive) {
    component.setVisible(hasAction && visibleWhenActive);
  };

  if (!hasAction || action == nullptr) {
    setVisibility(actionKindCombo, false);
    setVisibility(paramKeyCombo, false);
    setVisibility(valueEditor, false);
    setVisibility(deltaEditor, false);
    setVisibility(targetKindCombo, false);
    setVisibility(targetIdCombo, false);
    setVisibility(visibleCombo, false);
    setVisibility(lockedCombo, false);
    setVisibility(opacityEditor, false);
    setVisibility(assetPatchKeyCombo, false);
    setVisibility(assetPatchValueCombo, false);
    setVisibility(patchEditor, false);
    setVisibility(boundsXEditor, false);
    setVisibility(boundsYEditor, false);
    setVisibility(boundsWEditor, false);
    setVisibility(boundsHEditor, false);
    return;
  }

  setVisibility(actionKindCombo, true);

  const auto isSetRuntimeParam =
      action->kind == RuntimeActionKind::setRuntimeParam;
  const auto isAdjustRuntimeParam =
      action->kind == RuntimeActionKind::adjustRuntimeParam;
  const auto isToggleRuntimeParam =
      action->kind == RuntimeActionKind::toggleRuntimeParam;
  const auto isSetNodeProps = action->kind == RuntimeActionKind::setNodeProps;
  const auto isSetNodeBounds = action->kind == RuntimeActionKind::setNodeBounds;

  setVisibility(paramKeyCombo, isSetRuntimeParam || isAdjustRuntimeParam ||
                                   isToggleRuntimeParam);
  setVisibility(valueEditor, isSetRuntimeParam);
  setVisibility(deltaEditor, isAdjustRuntimeParam);
  setVisibility(targetKindCombo, isSetNodeProps);
  setVisibility(targetIdCombo, isSetNodeProps || isSetNodeBounds);
  setVisibility(visibleCombo, isSetNodeProps);
  setVisibility(lockedCombo, isSetNodeProps);
  setVisibility(opacityEditor, isSetNodeProps);
  setVisibility(assetPatchKeyCombo, isSetNodeProps);
  setVisibility(assetPatchValueCombo, isSetNodeProps);
  setVisibility(patchEditor, isSetNodeProps);
  setVisibility(boundsXEditor, isSetNodeBounds);
  setVisibility(boundsYEditor, isSetNodeBounds);
  setVisibility(boundsWEditor, isSetNodeBounds);
  setVisibility(boundsHEditor, isSetNodeBounds);
}

void EventActionPanel::rebuildAssetPatchEditors(
    const RuntimeActionModel *action) {
  assetPatchKeys.clear();
  assetPatchValues.clear();

  assetPatchKeyCombo.clear(juce::dontSendNotification);
  assetPatchValueCombo.clear(juce::dontSendNotification);
  assetPatchKeyCombo.setTextWhenNothingSelected("asset key");
  assetPatchValueCombo.setTextWhenNothingSelected("asset ref");

  if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps ||
      action->target.kind != NodeKind::widget)
    return;

  const auto targetId = action->target.id;
  if (targetId <= kRootId)
    return;

  const auto &snapshot = document.snapshot();
  const auto widgetIt =
      std::find_if(snapshot.widgets.begin(), snapshot.widgets.end(),
                   [targetId](const WidgetModel &model) noexcept {
                     return model.id == targetId;
                   });
  if (widgetIt == snapshot.widgets.end())
    return;

  const auto *descriptor = registry.find(widgetIt->type);
  if (descriptor == nullptr)
    return;

  int keyItemId = 1;
  for (const auto &spec : descriptor->propertySpecs) {
    if (spec.kind != Widgets::WidgetPropertyKind::assetRef)
      continue;

    const auto keyString = spec.key.toString();
    const auto keyLabel =
        spec.label.trim().isNotEmpty() ? spec.label.trim() : keyString;
    assetPatchKeyCombo.addItem(keyLabel + " (" + keyString + ")", keyItemId++);
    assetPatchKeys.push_back(spec.key);
  }

  if (assetPatchKeys.empty())
    return;

  int valueItemId = 1;
  assetPatchValueCombo.addItem("(None)", valueItemId++);
  assetPatchValues.emplace_back();

  std::vector<juce::String> seenRefKeys;
  seenRefKeys.reserve(snapshot.assets.size());
  for (const auto &asset : snapshot.assets) {
    const auto refKey = asset.refKey.trim();
    if (refKey.isEmpty())
      continue;

    if (std::find(seenRefKeys.begin(), seenRefKeys.end(), refKey) !=
        seenRefKeys.end())
      continue;

    seenRefKeys.push_back(refKey);
    const auto label = asset.name.trim().isNotEmpty()
                           ? (asset.name + " (" + refKey + ")")
                           : refKey;
    assetPatchValueCombo.addItem(label, valueItemId++);
    assetPatchValues.push_back(refKey);
  }

  int selectedKeyIndex = 0;
  for (int i = 0; i < action->patch.size(); ++i) {
    const auto candidateKey = action->patch.getName(i);
    const auto keyIt =
        std::find(assetPatchKeys.begin(), assetPatchKeys.end(), candidateKey);
    if (keyIt != assetPatchKeys.end()) {
      selectedKeyIndex =
          static_cast<int>(std::distance(assetPatchKeys.begin(), keyIt));
      break;
    }
  }

  assetPatchKeyCombo.setSelectedItemIndex(selectedKeyIndex,
                                          juce::dontSendNotification);

  const auto selectedKey =
      assetPatchKeys[static_cast<size_t>(selectedKeyIndex)];
  const auto patchValue = action->patch.getVarPointer(selectedKey);
  const auto currentRef =
      patchValue != nullptr ? patchValue->toString().trim() : juce::String();

  int selectedValueIndex = 0;
  if (currentRef.isNotEmpty()) {
    const auto valueIt =
        std::find(assetPatchValues.begin(), assetPatchValues.end(), currentRef);
    if (valueIt != assetPatchValues.end()) {
      selectedValueIndex =
          static_cast<int>(std::distance(assetPatchValues.begin(), valueIt));
    } else {
      assetPatchValueCombo.addItem("[Missing] " + currentRef, valueItemId);
      assetPatchValues.push_back(currentRef);
      selectedValueIndex = static_cast<int>(assetPatchValues.size()) - 1;
    }
  }

  assetPatchValueCombo.setSelectedItemIndex(selectedValueIndex,
                                            juce::dontSendNotification);
}

void EventActionPanel::syncAssetPatchValueEditor() {
  if (suppressCallbacks)
    return;

  const auto *action = selectedAction();
  if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps ||
      assetPatchKeys.empty())
    return;

  const auto keyIndex = assetPatchKeyCombo.getSelectedItemIndex();
  if (keyIndex < 0 || keyIndex >= static_cast<int>(assetPatchKeys.size()))
    return;

  const auto key = assetPatchKeys[static_cast<size_t>(keyIndex)];
  const auto patchValue = action->patch.getVarPointer(key);
  const auto currentRef =
      patchValue != nullptr ? patchValue->toString().trim() : juce::String();

  suppressCallbacks = true;
  int selectedValueIndex = 0;
  if (currentRef.isNotEmpty()) {
    const auto valueIt =
        std::find(assetPatchValues.begin(), assetPatchValues.end(), currentRef);
    if (valueIt != assetPatchValues.end()) {
      selectedValueIndex =
          static_cast<int>(std::distance(assetPatchValues.begin(), valueIt));
    } else {
      assetPatchValueCombo.addItem("[Missing] " + currentRef,
                                   static_cast<int>(assetPatchValues.size()) +
                                       1);
      assetPatchValues.push_back(currentRef);
      selectedValueIndex = static_cast<int>(assetPatchValues.size()) - 1;
    }
  }

  assetPatchValueCombo.setSelectedItemIndex(selectedValueIndex,
                                            juce::dontSendNotification);
  suppressCallbacks = false;
}

void EventActionPanel::applyAssetPatchValue() {
  if (suppressCallbacks)
    return;

  auto *action = selectedAction();
  if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps ||
      assetPatchKeys.empty())
    return;

  const auto keyIndex = assetPatchKeyCombo.getSelectedItemIndex();
  if (keyIndex < 0 || keyIndex >= static_cast<int>(assetPatchKeys.size()))
    return;

  auto selectedRef = assetPatchValueCombo.getText().trim();
  const auto valueIndex = assetPatchValueCombo.getSelectedItemIndex();
  if (valueIndex >= 0 && valueIndex < static_cast<int>(assetPatchValues.size()))
    selectedRef = assetPatchValues[static_cast<size_t>(valueIndex)];

  const auto key = assetPatchKeys[static_cast<size_t>(keyIndex)];
  if (selectedRef.isEmpty())
    action->patch.remove(key);
  else
    action->patch.set(key, selectedRef);

  patchEditor.setText(
      action->patch.size() > 0
          ? juce::JSON::toString(patchToVar(action->patch), true)
          : juce::String(),
      juce::dontSendNotification);

  if (commitBindings("asset-patch-edit"))
    setStatus("Action updated.", kStatusOk);
}

void EventActionPanel::createBindingFromToolbar() {
  const auto sourceIndex = sourceCombo.getSelectedItemIndex();
  const auto eventIndex = eventCombo.getSelectedItemIndex();
  if (sourceIndex < 0 ||
      sourceIndex >= static_cast<int>(widgetOptions.size())) {
    setStatus("No source widget is selected.", kStatusWarn);
    return;
  }
  const auto &source = widgetOptions[static_cast<size_t>(sourceIndex)];
  if (eventIndex < 0 || eventIndex >= static_cast<int>(source.events.size())) {
    setStatus("No supported event for selected widget.", kStatusWarn);
    return;
  }

  bindings.push_back(makeDefaultBinding(
      source.id, source.events[static_cast<size_t>(eventIndex)].key));
  selectedBindingId = bindings.back().id;
  selectedActionRow = -1;
  if (commitBindings("create-binding"))
    setStatus("Binding created.", kStatusOk);
}

void EventActionPanel::duplicateSelectedBinding() {
  const auto index = selectedBindingModelIndex();
  if (index < 0 || index >= static_cast<int>(bindings.size()))
    return;

  auto copy = bindings[static_cast<size_t>(index)];
  copy.id = nextBindingId();
  copy.name = copy.name.isNotEmpty() ? (copy.name + " Copy")
                                     : juce::String("Binding Copy");
  bindings.insert(bindings.begin() + index + 1, std::move(copy));
  selectedBindingId = bindings[static_cast<size_t>(index + 1)].id;
  selectedActionRow = -1;
  if (commitBindings("duplicate-binding"))
    setStatus("Binding duplicated.", kStatusOk);
}

void EventActionPanel::deleteSelectedBinding() {
  const auto index = selectedBindingModelIndex();
  if (index < 0 || index >= static_cast<int>(bindings.size()))
    return;

  bindings.erase(bindings.begin() + index);
  selectedBindingId = kRootId;
  selectedActionRow = -1;
  if (commitBindings("delete-binding"))
    setStatus("Binding deleted.", kStatusOk);
}

void EventActionPanel::addAction() {
  auto *binding = selectedBinding();
  if (binding == nullptr)
    return;

  binding->actions.push_back(makeDefaultAction(
      RuntimeActionKind::setRuntimeParam, binding->sourceWidgetId));
  selectedActionRow = static_cast<int>(binding->actions.size()) - 1;
  if (commitBindings("add-action"))
    setStatus("Action added.", kStatusOk);
}

void EventActionPanel::deleteAction() {
  auto *binding = selectedBinding();
  const auto row = actionList.getSelectedRow();
  if (binding == nullptr || row < 0 ||
      row >= static_cast<int>(binding->actions.size()))
    return;

  binding->actions.erase(binding->actions.begin() + row);
  if (binding->actions.empty()) {
    selectedActionRow = -1;
  } else {
    selectedActionRow =
        juce::jlimit(0, static_cast<int>(binding->actions.size()) - 1, row);
  }
  if (commitBindings("delete-action"))
    setStatus("Action deleted.", kStatusOk);
}

void EventActionPanel::moveActionUp() {
  auto *binding = selectedBinding();
  const auto row = actionList.getSelectedRow();
  if (binding == nullptr || row <= 0 ||
      row >= static_cast<int>(binding->actions.size()))
    return;

  std::swap(binding->actions[static_cast<size_t>(row)],
            binding->actions[static_cast<size_t>(row - 1)]);
  selectedActionRow = row - 1;
  if (commitBindings("move-action-up"))
    setStatus("Action moved.", kStatusOk);
}

void EventActionPanel::moveActionDown() {
  auto *binding = selectedBinding();
  const auto row = actionList.getSelectedRow();
  if (binding == nullptr || row < 0 ||
      row >= static_cast<int>(binding->actions.size()) - 1)
    return;

  std::swap(binding->actions[static_cast<size_t>(row)],
            binding->actions[static_cast<size_t>(row + 1)]);
  selectedActionRow = row + 1;
  if (commitBindings("move-action-down"))
    setStatus("Action moved.", kStatusOk);
}

void EventActionPanel::addRuntimeParam() {
  juce::String keyCandidate;
  int suffix = 1;
  while (true) {
    keyCandidate = "param." + juce::String(suffix++);
    const auto exists =
        std::any_of(runtimeParams.begin(), runtimeParams.end(),
                    [&keyCandidate](const RuntimeParamModel &param) {
                      return param.key.equalsIgnoreCase(keyCandidate);
                    });
    if (!exists)
      break;
  }

  RuntimeParamModel param;
  param.key = keyCandidate;
  param.type = RuntimeParamValueType::number;
  param.defaultValue = 0.0;
  param.description = {};
  param.exposed = true;
  runtimeParams.push_back(std::move(param));
  selectedRuntimeParamRow = static_cast<int>(runtimeParams.size()) - 1;

  if (commitRuntimeParams("add-runtime-param"))
    setStatus("Runtime param added.", kStatusOk);
}

void EventActionPanel::deleteRuntimeParam() {
  const auto index = selectedRuntimeParamIndex();
  if (index < 0 || index >= static_cast<int>(runtimeParams.size()))
    return;

  runtimeParams.erase(runtimeParams.begin() + index);
  if (runtimeParams.empty())
    selectedRuntimeParamRow = -1;
  else
    selectedRuntimeParamRow =
        juce::jlimit(0, static_cast<int>(runtimeParams.size()) - 1, index);

  if (commitRuntimeParams("delete-runtime-param"))
    setStatus("Runtime param deleted.", kStatusOk);
}

void EventActionPanel::applySelectedRuntimeParam() {
  if (suppressCallbacks)
    return;

  auto *param = selectedRuntimeParam();
  if (param == nullptr)
    return;

  const auto key = runtimeParamKeyEditor.getText().trim();
  if (key.isEmpty()) {
    setStatus("Param key is required.", kStatusError);
    return;
  }

  const auto keyCollides =
      std::any_of(runtimeParams.begin(), runtimeParams.end(),
                  [param, &key](const RuntimeParamModel &item) {
                    return &item != param && item.key.equalsIgnoreCase(key);
                  });
  if (keyCollides) {
    setStatus("Param key must be unique.", kStatusError);
    return;
  }

  auto parsedType =
      runtimeParamTypeFromComboId(runtimeParamTypeCombo.getSelectedId());
  if (!parsedType.has_value())
    parsedType = RuntimeParamValueType::number;

  juce::var defaultValue;
  if (*parsedType == RuntimeParamValueType::number) {
    const auto parsed = parseNumber(runtimeParamDefaultEditor.getText());
    if (!parsed.has_value() || !std::isfinite(*parsed)) {
      setStatus("Number param default must be finite number.", kStatusError);
      return;
    }

    defaultValue = *parsed;
  } else if (*parsedType == RuntimeParamValueType::boolean) {
    const auto parsed = parseLooseBool(runtimeParamDefaultEditor.getText());
    if (!parsed.has_value()) {
      setStatus("Boolean param default must be true/false.", kStatusError);
      return;
    }

    defaultValue = *parsed;
  } else {
    defaultValue = runtimeParamDefaultEditor.getText();
  }

  param->key = key;
  param->type = *parsedType;
  param->defaultValue = std::move(defaultValue);
  param->description = runtimeParamDescriptionEditor.getText().trim();
  param->exposed = runtimeParamExposedToggle.getToggleState();

  if (commitRuntimeParams("edit-runtime-param"))
    setStatus("Runtime param updated.", kStatusOk);
}

void EventActionPanel::addPropertyBinding() {
  const auto &snapshot = document.snapshot();
  if (snapshot.widgets.empty()) {
    setStatus("Add a widget first, then create property binding.", kStatusWarn);
    return;
  }

  PropertyBindingModel binding;
  binding.id = nextPropertyBindingId();
  binding.name =
      "Binding " + juce::String(static_cast<int>(propertyBindings.size() + 1));
  binding.enabled = true;
  binding.targetWidgetId = snapshot.widgets.front().id;

  juce::String defaultTargetProperty = "value";
  if (const auto *valueSpec =
          registry.propertySpec(snapshot.widgets.front().type, "value");
      valueSpec == nullptr) {
    if (const auto *specs =
            registry.propertySpecs(snapshot.widgets.front().type);
        specs != nullptr && !specs->empty())
      defaultTargetProperty = specs->front().key.toString();
    else if (snapshot.widgets.front().properties.size() > 0)
      defaultTargetProperty =
          snapshot.widgets.front().properties.getName(0).toString();
  }
  binding.targetProperty = defaultTargetProperty;

  if (!runtimeParams.empty() && runtimeParams.front().key.trim().isNotEmpty())
    binding.expression = runtimeParams.front().key.trim();
  else
    binding.expression = "1.0";

  propertyBindings.push_back(std::move(binding));
  selectedPropertyBindingRow = static_cast<int>(propertyBindings.size()) - 1;

  if (commitPropertyBindings("add-property-binding"))
    setStatus("Property binding added.", kStatusOk);
}

void EventActionPanel::deletePropertyBinding() {
  const auto index = selectedPropertyBindingIndex();
  if (index < 0 || index >= static_cast<int>(propertyBindings.size()))
    return;

  propertyBindings.erase(propertyBindings.begin() + index);
  if (propertyBindings.empty())
    selectedPropertyBindingRow = -1;
  else
    selectedPropertyBindingRow =
        juce::jlimit(0, static_cast<int>(propertyBindings.size()) - 1, index);

  if (commitPropertyBindings("delete-property-binding"))
    setStatus("Property binding deleted.", kStatusOk);
}

void EventActionPanel::applySelectedPropertyBinding() {
  if (suppressCallbacks)
    return;

  auto *binding = selectedPropertyBinding();
  if (binding == nullptr)
    return;

  const auto selectedTargetWidgetId =
      selectedWidgetIdFromCombo(propertyBindingTargetIdCombo);
  if (!selectedTargetWidgetId.has_value()) {
    setStatus("Target widget must be selected.", kStatusError);
    return;
  }
  const auto targetWidgetId = *selectedTargetWidgetId;

  const auto &snapshot = document.snapshot();
  const auto targetWidgetIt =
      std::find_if(snapshot.widgets.begin(), snapshot.widgets.end(),
                   [targetWidgetId](const WidgetModel &widget) {
                     return widget.id == targetWidgetId;
                   });
  if (targetWidgetIt == snapshot.widgets.end()) {
    setStatus("Target widget does not exist.", kStatusError);
    return;
  }

  const auto targetProperty = selectedPropertyBindingTargetPropertyKey();
  if (targetProperty.isEmpty()) {
    setStatus("Target property is required.", kStatusError);
    return;
  }
  if (!isIdentifierLike(targetProperty)) {
    setStatus("Target property must be identifier-like (letters/digits/_/.).",
              kStatusError);
    return;
  }

  const auto targetPropertyId = juce::Identifier(targetProperty);
  const auto knownBySpec =
      registry.propertySpec(targetWidgetIt->type, targetPropertyId) != nullptr;
  const auto existsInWidgetProps =
      targetWidgetIt->properties.contains(targetPropertyId);
  if (!knownBySpec && !existsInWidgetProps) {
    setStatus("Target property is not defined on the selected widget type.",
              kStatusError);
    return;
  }

  const auto expression = propertyBindingExpressionEditor.getText().trim();
  if (expression.isEmpty()) {
    setStatus("Expression is required.", kStatusError);
    return;
  }

  auto candidate = *binding;
  candidate.name = propertyBindingNameEditor.getText().trim();
  candidate.enabled = propertyBindingEnabledToggle.getToggleState();
  candidate.targetWidgetId = targetWidgetId;
  candidate.targetProperty = targetProperty;
  candidate.expression = expression;

  const auto validationError = validatePropertyBindingForUi(candidate);
  if (validationError.isNotEmpty()) {
    setStatus("Binding error: " + validationError, kStatusError);
    return;
  }

  *binding = std::move(candidate);

  if (commitPropertyBindings("edit-property-binding"))
    setStatus("Property binding updated.", kStatusOk);
  else
    setStatus("Failed to commit property binding.", kStatusError);
}

void EventActionPanel::applyBindingMeta() {
  if (suppressCallbacks)
    return;

  auto *binding = selectedBinding();
  if (binding == nullptr)
    return;

  binding->name = bindingNameEditor.getText().trim();
  binding->enabled = bindingEnabledToggle.getToggleState();
  if (commitBindings("binding-meta"))
    setStatus("Binding updated.", kStatusOk);
}

void EventActionPanel::applyActionKind() {
  if (suppressCallbacks)
    return;

  auto *binding = selectedBinding();
  auto *action = selectedAction();
  if (binding == nullptr || action == nullptr)
    return;

  auto parsedKind = actionKindFromComboId(actionKindCombo.getSelectedId());
  if (!parsedKind.has_value())
    parsedKind = actionKindFromLabel(actionKindCombo.getText());
  if (!parsedKind.has_value())
    return;

  if (action->kind == *parsedKind) {
    setStatus("Action kind unchanged.", kStatusInfo);
    return;
  }

  const auto selectedRow = actionList.getSelectedRow();
  *action = makeDefaultAction(*parsedKind, binding->sourceWidgetId);
  selectedActionRow = selectedRow >= 0 ? selectedRow : selectedActionRow;

  // Ensure the action list/detail are redrawn immediately with the new
  // kind.
  actionList.updateContent();
  actionList.repaint();
  bindingList.repaint();
  refreshDetailEditors();

  if (commitBindings("action-kind")) {
    setStatus("Action kind updated.", kStatusOk);
    return;
  }

  // Document commit failed; rollback local edits to keep UI/model
  // consistent.
  setStatus("Failed to commit action kind update.", kStatusError);
  refreshFromDocument();
}

void EventActionPanel::applySelectedAction() {
  if (suppressCallbacks)
    return;

  auto *action = selectedAction();
  if (action == nullptr)
    return;

  switch (action->kind) {
  case RuntimeActionKind::setRuntimeParam:
    action->paramKey = paramKeyCombo.getText().trim();
    action->value = parseRuntimeValue(valueEditor.getText());
    break;

  case RuntimeActionKind::adjustRuntimeParam: {
    action->paramKey = paramKeyCombo.getText().trim();
    const auto parsed = parseNumber(deltaEditor.getText());
    if (!parsed.has_value() || !std::isfinite(*parsed)) {
      setStatus("Delta must be finite number.", kStatusError);
      return;
    }
    action->delta = *parsed;
    break;
  }

  case RuntimeActionKind::toggleRuntimeParam:
    action->paramKey = paramKeyCombo.getText().trim();
    break;

  case RuntimeActionKind::setNodeProps: {
    auto parsedKind = nodeKindFromComboId(targetKindCombo.getSelectedId());
    if (!parsedKind.has_value())
      parsedKind = nodeKindFromLabel(targetKindCombo.getText());
    if (!parsedKind.has_value()) {
      setStatus("Target kind is invalid.", kStatusError);
      return;
    }

    const auto selectedTargetId = selectedWidgetIdFromCombo(targetIdCombo);
    if (!selectedTargetId.has_value()) {
      setStatus("Target widget id must be selected.", kStatusError);
      return;
    }
    const auto targetId = *selectedTargetId;

    action->target.kind = *parsedKind;
    action->target.id = targetId;
    action->targetWidgetId = targetId;
    action->visible.reset();
    action->locked.reset();
    if (visibleCombo.getSelectedId() == 2)
      action->visible = true;
    if (visibleCombo.getSelectedId() == 3)
      action->visible = false;
    if (lockedCombo.getSelectedId() == 2)
      action->locked = true;
    if (lockedCombo.getSelectedId() == 3)
      action->locked = false;

    const auto opacityText = opacityEditor.getText().trim();
    if (opacityText.isEmpty()) {
      action->opacity.reset();
    } else {
      const auto parsedOpacity = parseNumber(opacityText);
      if (!parsedOpacity.has_value() || !std::isfinite(*parsedOpacity)) {
        setStatus("Opacity must be numeric.", kStatusError);
        return;
      }
      action->opacity = static_cast<float>(*parsedOpacity);
    }

    PropertyBag patch;
    const auto patchResult = parsePatchJson(patchEditor.getText(), patch);
    if (patchResult.failed()) {
      setStatus("Patch JSON error: " + patchResult.getErrorMessage(),
                kStatusError);
      return;
    }
    action->patch = std::move(patch);
    break;
  }

  case RuntimeActionKind::setNodeBounds: {
    const auto selectedTargetId = selectedWidgetIdFromCombo(targetIdCombo);
    if (!selectedTargetId.has_value()) {
      setStatus("Target widget id must be selected.", kStatusError);
      return;
    }
    const auto targetWidgetId = *selectedTargetId;

    const auto x = parseNumber(boundsXEditor.getText());
    const auto y = parseNumber(boundsYEditor.getText());
    const auto w = parseNumber(boundsWEditor.getText());
    const auto h = parseNumber(boundsHEditor.getText());
    if (!x.has_value() || !y.has_value() || !w.has_value() || !h.has_value()) {
      setStatus("Bounds must be numeric.", kStatusError);
      return;
    }

    action->targetWidgetId = targetWidgetId;
    action->target.id = targetWidgetId;
    action->bounds =
        juce::Rectangle<float>(static_cast<float>(*x), static_cast<float>(*y),
                               static_cast<float>(*w), static_cast<float>(*h));
    break;
  }
  }

  if (commitBindings("action-edit"))
    setStatus("Action updated.", kStatusOk);
}

bool EventActionPanel::commitBindings(const juce::String &) {
  if (!document.setRuntimeBindings(bindings))
    return false;

  if (onBindingsChanged != nullptr)
    onBindingsChanged();

  refreshFromDocument();
  bindingList.updateContent();
  actionList.updateContent();
  return true;
}

bool EventActionPanel::commitRuntimeParams(const juce::String &) {
  if (!document.setRuntimeParams(runtimeParams))
    return false;

  if (onBindingsChanged != nullptr)
    onBindingsChanged();

  refreshFromDocument();
  return true;
}

bool EventActionPanel::commitPropertyBindings(const juce::String &) {
  if (!document.setPropertyBindings(propertyBindings))
    return false;

  if (onBindingsChanged != nullptr)
    onBindingsChanged();

  refreshFromDocument();
  return true;
}

void EventActionPanel::setStatus(const juce::String &text,
                                 juce::Colour colour) {
  statusLabel.setText(text, juce::dontSendNotification);
  statusLabel.setColour(juce::Label::textColourId, colour);
}

RuntimeBindingModel
EventActionPanel::makeDefaultBinding(WidgetId sourceWidgetId,
                                     const juce::String &eventKey) const {
  RuntimeBindingModel binding;
  binding.id = nextBindingId();
  binding.name =
      "Binding " + juce::String(static_cast<int>(
                       document.snapshot().runtimeBindings.size() + 1));
  binding.enabled = true;
  binding.sourceWidgetId = sourceWidgetId;
  binding.eventKey = eventKey;
  return binding;
}

RuntimeActionModel
EventActionPanel::makeDefaultAction(RuntimeActionKind kind,
                                    WidgetId sourceWidgetId) const {
  RuntimeActionModel action;
  action.kind = kind;
  action.paramKey = "param.key";
  action.value = 0.0;
  action.delta = 0.1;
  action.target.kind = NodeKind::widget;
  action.target.id = sourceWidgetId;
  action.targetWidgetId = sourceWidgetId;
  action.bounds = {0.0f, 0.0f, 120.0f, 28.0f};
  return action;
}

WidgetId EventActionPanel::nextBindingId() const {
  const auto &snapshot = document.snapshot();
  WidgetId maxId = kRootId;
  for (const auto &widget : snapshot.widgets)
    maxId = std::max(maxId, widget.id);
  for (const auto &group : snapshot.groups)
    maxId = std::max(maxId, group.id);
  for (const auto &layer : snapshot.layers)
    maxId = std::max(maxId, layer.id);
  for (const auto &binding : snapshot.runtimeBindings)
    maxId = std::max(maxId, binding.id);
  return maxId + 1;
}

WidgetId EventActionPanel::nextPropertyBindingId() const {
  const auto &snapshot = document.snapshot();
  WidgetId maxId = kRootId;
  for (const auto &widget : snapshot.widgets)
    maxId = std::max(maxId, widget.id);
  for (const auto &group : snapshot.groups)
    maxId = std::max(maxId, group.id);
  for (const auto &layer : snapshot.layers)
    maxId = std::max(maxId, layer.id);
  for (const auto &binding : snapshot.runtimeBindings)
    maxId = std::max(maxId, binding.id);
  for (const auto &binding : snapshot.propertyBindings)
    maxId = std::max(maxId, binding.id);
  for (const auto &binding : propertyBindings)
    maxId = std::max(maxId, binding.id);
  return maxId + 1;
}

std::optional<EventActionPanel::WidgetOption>
EventActionPanel::findWidgetOption(WidgetId id) const {
  const auto it = std::find_if(
      widgetOptions.begin(), widgetOptions.end(),
      [id](const WidgetOption &option) { return option.id == id; });
  if (it == widgetOptions.end())
    return std::nullopt;
  return *it;
}

juce::String EventActionPanel::formatEventLabel(
    const Widgets::RuntimeEventSpec &eventSpec) const {
  // 기술 키(onClick 등)를 괄호로 노출하지 않고 사용자 친화적 이름만 표시
  const auto koLabel = eventDisplayLabelKo(eventSpec.key);
  if (koLabel.isNotEmpty())
    return koLabel;
  if (eventSpec.displayLabel.trim().isNotEmpty())
    return eventSpec.displayLabel.trim();
  return eventSpec.key; // 번역 없는 경우만 원본 키 표시
}

juce::String
EventActionPanel::formatEventLabel(WidgetId sourceWidgetId,
                                   const juce::String &eventKey) const {
  if (const auto source = findWidgetOption(sourceWidgetId);
      source.has_value()) {
    const auto it =
        std::find_if(source->events.begin(), source->events.end(),
                     [&eventKey](const Widgets::RuntimeEventSpec &eventSpec) {
                       return eventSpec.key == eventKey;
                     });
    if (it != source->events.end())
      return formatEventLabel(*it);
  }
  const auto localized = eventDisplayLabelKo(eventKey);
  return localized.isNotEmpty() ? localized : eventKey;
}

juce::String
EventActionPanel::actionSummary(const RuntimeActionModel &action) const {
  // 위젯 ID를 사용자 이름으로 변환하는 헬퍼
  const auto widgetName = [this](WidgetId id) -> juce::String {
    const auto &widgets = document.snapshot().widgets;
    const auto it =
        std::find_if(widgets.begin(), widgets.end(),
                     [id](const WidgetModel &w) { return w.id == id; });
    if (it != widgets.end())
      return widgetDisplayName(*it, registry);
    return "#" + juce::String(id);
  };

  switch (action.kind) {
  case RuntimeActionKind::setRuntimeParam: {
    auto valueText = runtimeValueToString(action.value);
    if (valueText.length() > 12)
      valueText = valueText.substring(0, 12) + "...";
    return juce::String::fromUTF8(u8"\uD30C\uB77C\uBBF8\uD130 \uC124\uC815: ") +
           action.paramKey + " = " + valueText;
  }
  case RuntimeActionKind::adjustRuntimeParam:
    return juce::String::fromUTF8(u8"\uD30C\uB77C\uBBF8\uD130 \uC870\uC815: ") +
           action.paramKey + " (" + juce::String(action.delta, 3) + ")";
  case RuntimeActionKind::toggleRuntimeParam:
    return juce::String::fromUTF8(u8"\uD30C\uB77C\uBBF8\uD130 \uD1A0\uAE00: ") +
           action.paramKey;
  case RuntimeActionKind::setNodeProps: {
    const auto targetName = action.target.kind == NodeKind::widget
                                ? widgetName(action.target.id)
                                : (nodeKindLabel(action.target.kind) + " #" +
                                   juce::String(action.target.id));
    return juce::String::fromUTF8(u8"\uC18D\uC131 \uBCC0\uACBD: ") + targetName;
  }
  case RuntimeActionKind::setNodeBounds:
    return juce::String::fromUTF8(u8"\uC704\uCE58/\uD06C\uAE30: ") +
           widgetName(action.targetWidgetId);
  }
  return {};
}

juce::String EventActionPanel::validatePropertyBindingForUi(
    const PropertyBindingModel &binding) const {
  const auto &snapshot = document.snapshot();
  const auto targetWidgetIt =
      std::find_if(snapshot.widgets.begin(), snapshot.widgets.end(),
                   [&binding](const WidgetModel &widget) {
                     return widget.id == binding.targetWidgetId;
                   });
  if (targetWidgetIt == snapshot.widgets.end())
    return "target widget does not exist";

  const auto targetProperty = binding.targetProperty.trim();
  if (targetProperty.isEmpty())
    return "target property is required";
  if (!isIdentifierLike(targetProperty))
    return "target property format is invalid";

  const auto targetPropertyId = juce::Identifier(targetProperty);
  const auto *targetSpec =
      registry.propertySpec(targetWidgetIt->type, targetPropertyId);
  const auto hasCurrentValue =
      targetWidgetIt->properties.contains(targetPropertyId);

  if (targetSpec == nullptr && !hasCurrentValue)
    return "target property not found on target widget";

  if (targetSpec != nullptr) {
    const auto kind = targetSpec->kind;
    const auto supported = kind == Widgets::WidgetPropertyKind::number ||
                           kind == Widgets::WidgetPropertyKind::integer ||
                           kind == Widgets::WidgetPropertyKind::boolean;
    if (!supported)
      return "target property type is not bindable "
             "(number/integer/boolean "
             "only)";
  } else if (hasCurrentValue) {
    const auto &currentValue = targetWidgetIt->properties[targetPropertyId];
    if (!(currentValue.isBool() || isNumericVar(currentValue)))
      return "target property type mismatch for numeric expression";
  }

  const auto expression = binding.expression.trim();
  if (expression.isEmpty())
    return "expression is required";

  std::map<juce::String, juce::var> runtimeParamDefaults;
  for (const auto &param : runtimeParams) {
    const auto key = param.key.trim();
    if (key.isNotEmpty() &&
        runtimeParamDefaults.find(key) == runtimeParamDefaults.end())
      runtimeParamDefaults.emplace(key, param.defaultValue);
  }

  const auto evaluation = Runtime::PropertyBindingResolver::evaluateExpression(
      expression, runtimeParamDefaults);
  if (!evaluation.success)
    return "expression error: " + evaluation.error;

  return {};
}

juce::String EventActionPanel::actionKindLabel(RuntimeActionKind kind) {
  switch (kind) {
  case RuntimeActionKind::setRuntimeParam:
    return "SetRuntimeParam";
  case RuntimeActionKind::adjustRuntimeParam:
    return "AdjustRuntimeParam";
  case RuntimeActionKind::toggleRuntimeParam:
    return "ToggleRuntimeParam";
  case RuntimeActionKind::setNodeProps:
    return "SetNodeProps";
  case RuntimeActionKind::setNodeBounds:
    return "SetNodeBounds";
  }
  return {};
}

std::optional<RuntimeActionKind>
EventActionPanel::actionKindFromLabel(const juce::String &label) {
  const auto normalized = label.trim();
  if (normalized == "SetRuntimeParam")
    return RuntimeActionKind::setRuntimeParam;
  if (normalized == "AdjustRuntimeParam")
    return RuntimeActionKind::adjustRuntimeParam;
  if (normalized == "ToggleRuntimeParam")
    return RuntimeActionKind::toggleRuntimeParam;
  if (normalized == "SetNodeProps")
    return RuntimeActionKind::setNodeProps;
  if (normalized == "SetNodeBounds")
    return RuntimeActionKind::setNodeBounds;
  return std::nullopt;
}

juce::String EventActionPanel::nodeKindLabel(NodeKind kind) {
  switch (kind) {
  case NodeKind::widget:
    return "widget";
  case NodeKind::group:
    return "group";
  case NodeKind::layer:
    return "layer";
  }
  return {};
}

std::optional<NodeKind>
EventActionPanel::nodeKindFromLabel(const juce::String &label) {
  const auto normalized = label.trim();
  if (normalized == "widget")
    return NodeKind::widget;
  if (normalized == "group")
    return NodeKind::group;
  if (normalized == "layer")
    return NodeKind::layer;
  return std::nullopt;
}

juce::var EventActionPanel::parseRuntimeValue(const juce::String &text) {
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return juce::var(juce::String());

  juce::var parsed;
  const auto result = juce::JSON::parse(trimmed, parsed);
  if (result.wasOk() &&
      (parsed.isBool() || parsed.isInt() || parsed.isInt64() ||
       parsed.isDouble() || parsed.isString()))
    return parsed;

  if (trimmed.containsOnly("0123456789+-.eE"))
    return juce::var(trimmed.getDoubleValue());

  return juce::var(trimmed);
}

juce::String EventActionPanel::runtimeValueToString(const juce::var &value) {
  if (value.isVoid())
    return {};
  if (value.isBool())
    return static_cast<bool>(value) ? "true" : "false";
  if (value.isInt() || value.isInt64() || value.isDouble())
    return juce::String(static_cast<double>(value), 8);
  if (value.isString())
    return juce::JSON::toString(value, false);
  return juce::JSON::toString(value, true);
}

std::optional<double> EventActionPanel::parseNumber(const juce::String &text) {
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return std::nullopt;

  juce::var parsed;
  const auto result = juce::JSON::parse(trimmed, parsed);
  if (result.wasOk() && isNumericVar(parsed))
    return static_cast<double>(parsed);

  if (!trimmed.containsOnly("0123456789+-.eE"))
    return std::nullopt;
  return trimmed.getDoubleValue();
}

juce::Result EventActionPanel::parsePatchJson(const juce::String &text,
                                              PropertyBag &outPatch) {
  outPatch.clear();
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return juce::Result::ok();

  juce::var root;
  const auto parseResult = juce::JSON::parse(trimmed, root);
  if (parseResult.failed())
    return juce::Result::fail(parseResult.getErrorMessage());

  const auto *object = root.getDynamicObject();
  if (object == nullptr)
    return juce::Result::fail("Patch must be object");

  const auto &props = object->getProperties();
  for (int i = 0; i < props.size(); ++i)
    outPatch.set(props.getName(i), props.getValueAt(i));

  return validatePropertyBag(outPatch);
}

int EventActionPanel::BindingListModel::getNumRows() {
  return static_cast<int>(owner.visibleBindingIndices.size());
}

void EventActionPanel::BindingListModel::paintListBoxItem(int rowNumber,
                                                          juce::Graphics &g,
                                                          int width, int height,
                                                          bool rowIsSelected) {
  if (rowNumber < 0 ||
      rowNumber >= static_cast<int>(owner.visibleBindingIndices.size()))
    return;

  const auto modelIndex =
      owner.visibleBindingIndices[static_cast<size_t>(rowNumber)];
  if (modelIndex < 0 || modelIndex >= static_cast<int>(owner.bindings.size()))
    return;

  const auto &binding = owner.bindings[static_cast<size_t>(modelIndex)];
  const auto accent = eventAccent(binding.eventKey);

  auto card =
      juce::Rectangle<float>(1.0f, 1.0f, static_cast<float>(width) - 2.0f,
                             static_cast<float>(height) - 2.0f);
  drawCardBackground(g, card, rowIsSelected, accent);
  drawLeftAccentBar(g, card, accent, rowIsSelected);

  auto area = card.toNearestInt().reduced(10, 6);
  auto titleRow = area.removeFromTop(14);

  g.setColour(binding.enabled ? palette(GyeolPalette::ValidSuccess)
                              : palette(GyeolPalette::TextSecondary));
  g.setFont(juce::FontOptions(8.8f, juce::Font::bold));
  g.drawText(binding.enabled ? "ON" : "OFF", titleRow.removeFromLeft(30),
             juce::Justification::centredLeft, true);

  g.setColour(palette(GyeolPalette::TextPrimary));
  g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
  const auto title =
      binding.name.isNotEmpty() ? binding.name : juce::String("Binding");
  g.drawFittedText(title, titleRow, juce::Justification::centredLeft, 1);

  const auto sourceText =
      owner.findWidgetOption(binding.sourceWidgetId).has_value()
          ? owner.findWidgetOption(binding.sourceWidgetId)->label
          : ("Widget #" + juce::String(binding.sourceWidgetId));
  const auto eventText =
      owner.formatEventLabel(binding.sourceWidgetId, binding.eventKey);
  const auto subtitle = sourceText + " | " + eventText + " | " +
                        juce::String(static_cast<int>(binding.actions.size())) +
                        " actions";

  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(juce::FontOptions(8.6f));
  g.drawFittedText(subtitle, area, juce::Justification::centredLeft, 1);
}

void EventActionPanel::BindingListModel::selectedRowsChanged(
    int lastRowSelected) {
  if (owner.suppressCallbacks)
    return;

  if (lastRowSelected < 0 ||
      lastRowSelected >= static_cast<int>(owner.visibleBindingIndices.size())) {
    owner.selectedBindingId = kRootId;
    owner.selectedActionRow = -1;
  } else {
    const auto modelIndex =
        owner.visibleBindingIndices[static_cast<size_t>(lastRowSelected)];
    owner.selectedBindingId =
        owner.bindings[static_cast<size_t>(modelIndex)].id;
    owner.selectedActionRow = 0;
  }

  owner.refreshDetailEditors();
}

void EventActionPanel::BindingListModel::listBoxItemClicked(
    int row, const juce::MouseEvent &event) {
  if (!event.mods.isPopupMenu())
    return;

  if (row >= 0)
    owner.bindingList.selectRow(row);

  juce::PopupMenu menu;
  menu.addItem(
      "Duplicate",
      [safeOwner = juce::Component::SafePointer<EventActionPanel>(&owner)] {
        if (safeOwner != nullptr)
          safeOwner->duplicateSelectedBinding();
      });
  menu.addItem(
      "Delete",
      [safeOwner = juce::Component::SafePointer<EventActionPanel>(&owner)] {
        if (safeOwner != nullptr)
          safeOwner->deleteSelectedBinding();
      });
  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(&owner.bindingList));
}

int EventActionPanel::ActionListModel::getNumRows() {
  const auto *binding = owner.selectedBinding();
  return binding != nullptr ? static_cast<int>(binding->actions.size()) : 0;
}

void EventActionPanel::ActionListModel::paintListBoxItem(int rowNumber,
                                                         juce::Graphics &g,
                                                         int width, int height,
                                                         bool rowIsSelected) {
  const auto *binding = owner.selectedBinding();
  if (binding == nullptr || rowNumber < 0 ||
      rowNumber >= static_cast<int>(binding->actions.size()))
    return;

  const auto &action = binding->actions[static_cast<size_t>(rowNumber)];
  const auto accent = actionAccent(action.kind);

  auto card =
      juce::Rectangle<float>(1.0f, 1.0f, static_cast<float>(width) - 2.0f,
                             static_cast<float>(height) - 2.0f);
  drawCardBackground(g, card, rowIsSelected, accent);
  drawLeftAccentBar(g, card, accent, rowIsSelected);

  auto area = card.toNearestInt().reduced(10, 6);
  auto titleRow = area.removeFromTop(14);

  const auto title = juce::String(rowNumber + 1) + ". " +
                     actionPrefix(action.kind) +
                     EventActionPanel::actionKindLabel(action.kind);
  g.setColour(palette(GyeolPalette::TextPrimary));
  g.setFont(juce::FontOptions(9.6f, juce::Font::bold));
  g.drawFittedText(title, titleRow, juce::Justification::centredLeft, 1);

  auto summary = owner.actionSummary(action);
  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(juce::FontOptions(8.6f));
  g.drawFittedText(summary, area, juce::Justification::centredLeft, 1);
}

void EventActionPanel::ActionListModel::selectedRowsChanged(
    int lastRowSelected) {
  if (owner.suppressCallbacks)
    return;

  owner.selectedActionRow = lastRowSelected;
  owner.refreshDetailEditors();
}

int EventActionPanel::RuntimeParamListModel::getNumRows() {
  return static_cast<int>(owner.runtimeParams.size());
}

void EventActionPanel::RuntimeParamListModel::paintListBoxItem(
    int rowNumber, juce::Graphics &g, int width, int height,
    bool rowIsSelected) {
  if (rowNumber < 0 ||
      rowNumber >= static_cast<int>(owner.runtimeParams.size()))
    return;

  const auto &param = owner.runtimeParams[static_cast<size_t>(rowNumber)];

  juce::Colour accent = palette(GyeolPalette::AccentPrimary);
  switch (param.type) {
  case RuntimeParamValueType::number:
    accent = palette(GyeolPalette::AccentPrimary);
    break;
  case RuntimeParamValueType::boolean:
    accent = palette(GyeolPalette::ValidWarning);
    break;
  case RuntimeParamValueType::string:
    accent = palette(GyeolPalette::ValidSuccess);
    break;
  }

  auto card =
      juce::Rectangle<float>(1.0f, 1.0f, static_cast<float>(width) - 2.0f,
                             static_cast<float>(height) - 2.0f);
  drawCardBackground(g, card, rowIsSelected, accent);
  drawLeftAccentBar(g, card, accent, rowIsSelected);

  auto area = card.toNearestInt().reduced(10, 6);
  auto titleRow = area.removeFromTop(14);

  g.setColour(palette(GyeolPalette::TextPrimary));
  g.setFont(juce::FontOptions(9.6f, juce::Font::bold));
  g.drawFittedText(param.key, titleRow, juce::Justification::centredLeft, 1);

  auto subtitle = "type: " + runtimeParamValueTypeToKey(param.type);
  if (!param.defaultValue.isVoid())
    subtitle += " | default: " + param.defaultValue.toString();
  if (param.exposed)
    subtitle += " | exposed";

  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(juce::FontOptions(8.6f));
  g.drawFittedText(subtitle, area, juce::Justification::centredLeft, 1);
}

void EventActionPanel::RuntimeParamListModel::selectedRowsChanged(
    int lastRowSelected) {
  if (owner.suppressCallbacks)
    return;

  owner.selectedRuntimeParamRow = lastRowSelected;
  owner.refreshStateEditors();
}

int EventActionPanel::PropertyBindingListModel::getNumRows() {
  return static_cast<int>(owner.propertyBindings.size());
}

void EventActionPanel::PropertyBindingListModel::paintListBoxItem(
    int rowNumber, juce::Graphics &g, int width, int height,
    bool rowIsSelected) {
  if (rowNumber < 0 ||
      rowNumber >= static_cast<int>(owner.propertyBindings.size()))
    return;

  const auto &binding = owner.propertyBindings[static_cast<size_t>(rowNumber)];
  const auto validationError = owner.validatePropertyBindingForUi(binding);
  const auto hasError = validationError.isNotEmpty();
  g.setColour(rowIsSelected
                  ? palette(GyeolPalette::SelectionBackground).withAlpha(0.84f)
                  : palette(GyeolPalette::ControlBase).withAlpha(0.62f));
  g.fillRect(0, 0, width, height);
  g.setColour(palette(GyeolPalette::BorderDefault));
  g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

  auto area = juce::Rectangle<int>(0, 0, width, height).reduced(6, 3);
  auto top = area.removeFromTop(12);

  const auto statusText =
      !binding.enabled ? juce::String("OFF")
                       : (hasError ? juce::String("ERR") : juce::String("ON"));
  const auto statusColor =
      !binding.enabled ? palette(GyeolPalette::TextSecondary)
                       : (hasError ? palette(GyeolPalette::ValidError)
                                   : palette(GyeolPalette::ValidSuccess));

  g.setColour(statusColor);
  g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
  g.drawText(statusText, top.removeFromLeft(26),
             juce::Justification::centredLeft, true);

  g.setColour(palette(GyeolPalette::TextPrimary));
  g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
  const auto name = binding.name.isNotEmpty()
                        ? binding.name
                        : juce::String("Property Binding");
  g.drawFittedText(name, top, juce::Justification::centredLeft, 1);

  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(juce::FontOptions(8.8f));
  auto detail = "widget:" + juce::String(binding.targetWidgetId) + "  " +
                binding.targetProperty + " <- " + binding.expression;
  if (hasError)
    detail += " | " + validationError;

  g.drawFittedText(detail, area, juce::Justification::centredLeft, 1);
}

void EventActionPanel::PropertyBindingListModel::selectedRowsChanged(
    int lastRowSelected) {
  if (owner.suppressCallbacks)
    return;

  owner.selectedPropertyBindingRow = lastRowSelected;
  owner.refreshStateEditors();
}
} // namespace Gyeol::Ui::Panels
