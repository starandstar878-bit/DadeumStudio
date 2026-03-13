#include "Teul/Editor/Panels/Property/ParamEditorFactory.h"
#include "Teul/Editor/Theme/TeulPalette.h"

#include "Teul/Editor/Panels/Property/ParamValueFormatter.h"

namespace Teul {

std::unique_ptr<juce::Component> createParamEditor(const TParamSpec &spec,
                                                   const juce::var &value) {
  if (spec.preferredWidget == TParamWidgetHint::Combo &&
      !spec.enumOptions.empty()) {
    auto combo = std::make_unique<juce::ComboBox>();
    int id = 1;
    int selectedId = 0;
    for (const auto &option : spec.enumOptions) {
      combo->addItem(option.label.isNotEmpty() ? option.label : option.id, id);
      if (selectedId == 0 && varEquals(option.value, value))
        selectedId = id;
      ++id;
    }
    combo->setSelectedId(selectedId > 0 ? selectedId : 1,
                         juce::dontSendNotification);
    combo->setEnabled(!spec.isReadOnly);
    return combo;
  }

  if (spec.preferredWidget == TParamWidgetHint::Toggle ||
      spec.valueType == TParamValueType::Bool) {
    auto toggle = std::make_unique<juce::ToggleButton>();
    toggle->setToggleState((bool)value, juce::dontSendNotification);
    toggle->setEnabled(!spec.isReadOnly);
    return toggle;
  }

  if (spec.preferredWidget == TParamWidgetHint::Slider &&
      hasNumericRange(spec)) {
    auto slider = std::make_unique<juce::Slider>(
        juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    slider->setRange(numericValue(spec.minValue), numericValue(spec.maxValue),
                     sliderIntervalFor(spec));
    slider->setValue(numericValue(value), juce::dontSendNotification);
    slider->setNumDecimalPlacesToDisplay(
        spec.valueType == TParamValueType::Int || spec.isDiscrete
            ? 0
            : juce::jlimit(0, 6, spec.displayPrecision));
    slider->setTextValueSuffix(spec.unitLabel.isNotEmpty()
                                   ? " " + spec.unitLabel
                                   : juce::String());
    slider->setDoubleClickReturnValue(true, numericValue(spec.defaultValue));
    slider->setEnabled(!spec.isReadOnly);
    return slider;
  }

  auto textEditor = std::make_unique<juce::TextEditor>();
  textEditor->setText(formatValueForDisplay(value, spec),
                      juce::dontSendNotification);
  textEditor->setColour(juce::TextEditor::backgroundColourId,
                        TeulPalette::InputBackground());
  textEditor->setColour(juce::TextEditor::textColourId, TeulPalette::PanelTextStrong());
  textEditor->setColour(juce::TextEditor::outlineColourId,
                        TeulPalette::InputOutline());
  textEditor->setReadOnly(spec.isReadOnly);
  return textEditor;
}

int editorHeightFor(const juce::Component &editor) {
  if (dynamic_cast<const juce::Slider *>(&editor) != nullptr)
    return 32;
  return 24;
}

juce::var readEditorValue(const juce::Component &editor,
                         const TParamSpec &spec,
                         const juce::var &originalValue) {
  if (const auto *combo = dynamic_cast<const juce::ComboBox *>(&editor)) {
    const int index = combo->getSelectedItemIndex();
    if (index >= 0 && index < (int)spec.enumOptions.size())
      return spec.enumOptions[(size_t)index].value;
    return originalValue;
  }

  if (const auto *toggle = dynamic_cast<const juce::ToggleButton *>(&editor))
    return toggle->getToggleState();

  if (const auto *slider = dynamic_cast<const juce::Slider *>(&editor)) {
    if (spec.valueType == TParamValueType::Int || spec.isDiscrete)
      return juce::roundToInt(slider->getValue());
    return slider->getValue();
  }

  if (const auto *textEditor = dynamic_cast<const juce::TextEditor *>(&editor))
    return parseTextValue(textEditor->getText(), spec, originalValue);

  return originalValue;
}

} // namespace Teul
