#pragma once

#include "Gyeol/Editor/Theme/GyeolCustomLookAndFeel.h"
#include "Gyeol/Widgets/WidgetSDK.h"

namespace Gyeol::Widgets {
class TextInputWidget final : public WidgetClass {
public:
  WidgetDescriptor makeDescriptor() const override {
    WidgetDescriptor descriptor;
    descriptor.type = WidgetType::textInput;
    descriptor.typeKey = "textInput";
    descriptor.displayName = "Text Input";
    descriptor.category = "Input";
    descriptor.tags = {"text", "input", "editor"};
    descriptor.iconKey = "textInput";
    descriptor.exportTargetType = "juce::TextEditor";
    descriptor.defaultBounds = {0.0f, 0.0f, 180.0f, 32.0f};
    descriptor.minSize = {90.0f, 24.0f};
    applyBuiltinDescriptorDefaults(descriptor, "textbox", "widget.textInput.label");
    descriptor.capabilities = {"emitsRuntimeEvents", "customCodegen"};
    descriptor.supportsOffscreenCache = true;
    descriptor.estimatedPaintCost = 0.16;
    descriptor.memoryBudgetKb = 96;
    descriptor.supportedActions = {"setRuntimeParam", "setNodeProps",
                                   "setNodeBounds"};
    descriptor.stateInputs = {"text", "textInput.placeholder",
                              "textInput.multiline", "textInput.readOnly",
                              "textInput.passwordChar"};
    descriptor.stateOutputs = {"text"};
    descriptor.runtimeEvents = {{"onTextCommit", "Text Commit",
                                 "Fires when text input is committed", false}};
    for (auto &eventSpec : descriptor.runtimeEvents)
      eventSpec.payloadSchema = makePayloadSchema();

    descriptor.defaultProperties.set("text", juce::String());
    descriptor.defaultProperties.set("textInput.placeholder",
                                     juce::String("Type..."));
    descriptor.defaultProperties.set("textInput.multiline", false);
    descriptor.defaultProperties.set("textInput.readOnly", false);
    descriptor.defaultProperties.set("textInput.passwordChar", juce::String());

    {
      WidgetPropertySpec textSpec;
      textSpec.key = juce::Identifier("text");
      textSpec.label = "Text";
      textSpec.kind = WidgetPropertyKind::text;
      textSpec.uiHint = WidgetPropertyUiHint::multiLine;
      textSpec.group = "Content";
      textSpec.order = 10;
      textSpec.hint = "Initial text";
      textSpec.defaultValue = juce::var(juce::String());
      textSpec.required = true;
      textSpec.maxLength = 4096;
      descriptor.propertySpecs.push_back(std::move(textSpec));

      WidgetPropertySpec placeholderSpec;
      placeholderSpec.key = juce::Identifier("textInput.placeholder");
      placeholderSpec.label = "Placeholder";
      placeholderSpec.kind = WidgetPropertyKind::text;
      placeholderSpec.uiHint = WidgetPropertyUiHint::lineEdit;
      placeholderSpec.group = "Content";
      placeholderSpec.order = 20;
      placeholderSpec.hint = "Shown when text is empty";
      placeholderSpec.defaultValue = juce::var("Type...");
      placeholderSpec.maxLength = 256;
      descriptor.propertySpecs.push_back(std::move(placeholderSpec));

      WidgetPropertySpec multilineSpec;
      multilineSpec.key = juce::Identifier("textInput.multiline");
      multilineSpec.label = "Multiline";
      multilineSpec.kind = WidgetPropertyKind::boolean;
      multilineSpec.uiHint = WidgetPropertyUiHint::toggle;
      multilineSpec.group = "Behavior";
      multilineSpec.order = 30;
      multilineSpec.hint = "Enable multi-line editing";
      multilineSpec.defaultValue = juce::var(false);
      multilineSpec.required = true;
      descriptor.propertySpecs.push_back(std::move(multilineSpec));

      WidgetPropertySpec readOnlySpec;
      readOnlySpec.key = juce::Identifier("textInput.readOnly");
      readOnlySpec.label = "Read Only";
      readOnlySpec.kind = WidgetPropertyKind::boolean;
      readOnlySpec.uiHint = WidgetPropertyUiHint::toggle;
      readOnlySpec.group = "Behavior";
      readOnlySpec.order = 40;
      readOnlySpec.hint = "Disable user editing";
      readOnlySpec.defaultValue = juce::var(false);
      readOnlySpec.required = true;
      descriptor.propertySpecs.push_back(std::move(readOnlySpec));

      WidgetPropertySpec passwordSpec;
      passwordSpec.key = juce::Identifier("textInput.passwordChar");
      passwordSpec.label = "Password Char";
      passwordSpec.kind = WidgetPropertyKind::text;
      passwordSpec.uiHint = WidgetPropertyUiHint::lineEdit;
      passwordSpec.group = "Behavior";
      passwordSpec.order = 100;
      passwordSpec.hint = "Single character mask (empty = none)";
      passwordSpec.defaultValue = juce::var(juce::String());
      passwordSpec.maxLength = 1;
      passwordSpec.advanced = true;
      descriptor.propertySpecs.push_back(std::move(passwordSpec));
    }

    descriptor.painter = [](juce::Graphics &g, const WidgetModel &widget,
                            const juce::Rectangle<float> &body) {
      const auto text =
          widget.properties.getWithDefault("text", juce::var(juce::String()))
              .toString();
      const auto placeholder =
          widget.properties
              .getWithDefault("textInput.placeholder", juce::var("Type..."))
              .toString();
      const auto readOnly = static_cast<bool>(widget.properties.getWithDefault(
          "textInput.readOnly", juce::var(false)));

      g.setColour(Gyeol::getGyeolColor(Gyeol::GyeolPalette::ControlBase));
      g.fillRoundedRectangle(body, 3.0f);
      g.setColour(readOnly ? Gyeol::getGyeolColor(Gyeol::GyeolPalette::BorderDefault)
                           : Gyeol::getGyeolColor(Gyeol::GyeolPalette::BorderActive));
      g.drawRoundedRectangle(body, 3.0f, 1.0f);

      const auto display = text.isNotEmpty() ? text : placeholder;
      g.setColour(text.isNotEmpty() ? Gyeol::getGyeolColor(Gyeol::GyeolPalette::TextPrimary)
                                    : Gyeol::getGyeolColor(Gyeol::GyeolPalette::TextSecondary));
      g.setFont(juce::FontOptions(12.0f));
      g.drawFittedText(display, body.reduced(8.0f, 5.0f).toNearestInt(),
                       juce::Justification::centredLeft, 2);
    };

    descriptor.iconPainter = [](juce::Graphics &g,
                                const juce::Rectangle<float> &r) {
      g.setColour(Gyeol::getGyeolColor(Gyeol::GyeolPalette::TextSecondary));
      g.drawRoundedRectangle(r.reduced(4.0f, 10.0f), 2.0f, 1.5f);
      g.setColour(Gyeol::getGyeolColor(Gyeol::GyeolPalette::AccentPrimary));
      g.drawLine(r.getX() + 8.0f, r.getCentreY() - 4.0f, r.getX() + 8.0f,
                 r.getCentreY() + 4.0f, 2.0f);
    };

    descriptor.exportCodegen = [](const ExportCodegenContext &context,
                                  ExportCodegenOutput &out) {
      const auto text = context.widget.properties
                            .getWithDefault("text", juce::var(juce::String()))
                            .toString();
      const auto placeholder =
          context.widget.properties
              .getWithDefault("textInput.placeholder", juce::var("Type..."))
              .toString();
      const auto multiline =
          static_cast<bool>(context.widget.properties.getWithDefault(
              "textInput.multiline", juce::var(false)));
      const auto readOnly =
          static_cast<bool>(context.widget.properties.getWithDefault(
              "textInput.readOnly", juce::var(false)));
      const auto passwordChar = context.widget.properties
                                    .getWithDefault("textInput.passwordChar",
                                                    juce::var(juce::String()))
                                    .toString();
      const auto toLiteral = [](const juce::String &value) {
        return juce::JSON::toString(juce::var(value), false);
      };

      out.memberType = "juce::TextEditor";
      out.codegenKind = "juce_text_editor";
      out.constructorLines.clear();
      out.resizedLines.clear();

      out.constructorLines.add("    " + context.memberName + ".setMultiLine(" +
                               juce::String(multiline ? "true" : "false") +
                               ");");
      out.constructorLines.add(
          "    " + context.memberName + ".setReturnKeyStartsNewLine(" +
          juce::String(multiline ? "true" : "false") + ");");
      out.constructorLines.add("    " + context.memberName + ".setReadOnly(" +
                               juce::String(readOnly ? "true" : "false") +
                               ");");
      out.constructorLines.add(
          "    " + context.memberName + ".setTextToShowWhenEmpty(" +
          toLiteral(placeholder) + ", juce::Colours::grey);");
      out.constructorLines.add("    " + context.memberName + ".setText(" +
                               toLiteral(text) + ", false);");

      if (passwordChar.isNotEmpty()) {
        const auto codepoint = static_cast<int>(passwordChar[0]);
        out.constructorLines.add(
            "    " + context.memberName +
            ".setPasswordCharacter(static_cast<juce_wchar>(" +
            juce::String(codepoint) + "));");
      } else {
        out.constructorLines.add("    " + context.memberName +
                                 ".setPasswordCharacter(0);");
      }

      out.constructorLines.add("    addAndMakeVisible(" + context.memberName +
                               ");");
      return juce::Result::ok();
    };

    descriptor.cursorProvider = [](const WidgetModel &, juce::Point<float>) {
      return juce::MouseCursor(juce::MouseCursor::IBeamCursor);
    };
    return descriptor;
  }
};

GYEOL_WIDGET_AUTOREGISTER(TextInputWidget);
} // namespace Gyeol::Widgets
