#include "Gyeol/Export/JuceComponentExport.h"

#include "Gyeol/Core/SceneValidator.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace
{
    struct ExportWidgetEntry
    {
        const Gyeol::WidgetModel* model = nullptr;
        const Gyeol::Widgets::WidgetDescriptor* descriptor = nullptr;
        juce::String typeKey;
        juce::String exportTargetType;
        juce::String memberName;
        juce::String memberType;
        juce::String codegenKind;
        bool supported = true;
        bool usesCustomCodegen = false;
        juce::StringArray constructorLines;
        juce::StringArray resizedLines;
    };

    struct CopiedAssetEntry
    {
        Gyeol::WidgetId assetId = 0;
        juce::String refKey;
        juce::String kind;
        juce::String mimeType;
        juce::String sourcePath;
        juce::String destinationRelativePath;
        bool copied = false;
        bool reused = false;
    };

    juce::String severityToString(Gyeol::Export::IssueSeverity severity)
    {
        switch (severity)
        {
            case Gyeol::Export::IssueSeverity::info: return "INFO";
            case Gyeol::Export::IssueSeverity::warning: return "WARN";
            case Gyeol::Export::IssueSeverity::error: return "ERROR";
        }

        return "INFO";
    }

    juce::String nodeKindToKey(Gyeol::NodeKind kind)
    {
        switch (kind)
        {
            case Gyeol::NodeKind::widget: return "widget";
            case Gyeol::NodeKind::group: return "group";
            case Gyeol::NodeKind::layer: return "layer";
        }

        return "widget";
    }

    juce::String sanitizeIdentifier(const juce::String& raw)
    {
        juce::String sanitized;
        sanitized.preallocateBytes(raw.length() + 8);

        auto isAsciiAlpha = [](auto c) noexcept
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        };
        auto isAsciiDigit = [](auto c) noexcept
        {
            return c >= '0' && c <= '9';
        };

        for (int i = 0; i < raw.length(); ++i)
        {
            const auto c = raw[i];
            const auto keep = isAsciiAlpha(c) || isAsciiDigit(c) || c == '_';

            if (i == 0)
            {
                if (isAsciiDigit(c))
                    sanitized << "_";

                if (keep && !isAsciiDigit(c))
                    sanitized << juce::String::charToString(c);
                else if (keep)
                    sanitized << juce::String::charToString(c);
                else
                    sanitized << "_";
            }
            else
            {
                if (keep)
                    sanitized << juce::String::charToString(c);
                else
                    sanitized << "_";
            }
        }

        if (sanitized.isEmpty())
            sanitized = "_generated";

        return sanitized;
    }

    juce::String makeUniqueMemberName(const juce::String& preferredBase, std::set<juce::String>& usedNames)
    {
        const auto base = sanitizeIdentifier(preferredBase.toLowerCase());
        if (usedNames.insert(base).second)
            return base;

        for (int suffix = 2; suffix < 10000; ++suffix)
        {
            const auto candidate = base + "_" + juce::String(suffix);
            if (usedNames.insert(candidate).second)
                return candidate;
        }

        // Defensive fallback for pathological name collisions.
        const auto fallback = base + "_" + juce::String(juce::Random::getSystemRandom().nextInt(1'000'000));
        usedNames.insert(fallback);
        return fallback;
    }

    juce::String toCppStringLiteral(const juce::String& text)
    {
        return juce::JSON::toString(juce::var(text), false);
    }

    juce::Result ensureDirectory(const juce::File& directory)
    {
        if (directory.getFullPathName().isEmpty())
            return juce::Result::fail("Export output directory is empty");

        if (directory.exists())
        {
            if (!directory.isDirectory())
                return juce::Result::fail("Export output path is not a directory: " + directory.getFullPathName());
            return juce::Result::ok();
        }

        if (!directory.createDirectory())
            return juce::Result::fail("Failed to create directory: " + directory.getFullPathName());

        return juce::Result::ok();
    }

    juce::Result writeTextFile(const juce::File& file, const juce::String& text, bool overwriteExisting)
    {
        if (file.existsAsFile() && !overwriteExisting)
            return juce::Result::fail("Refusing to overwrite existing file: " + file.getFullPathName());

        if (!file.replaceWithText(text))
            return juce::Result::fail("Failed to write file: " + file.getFullPathName());

        return juce::Result::ok();
    }

    juce::String generateHeaderCode(const juce::String& className,
                                    const std::vector<ExportWidgetEntry>& widgets)
    {
        juce::StringArray lines;
        lines.add("#pragma once");
        lines.add("");
        lines.add("#include <JuceHeader.h>");
        lines.add("#include <map>");
        lines.add("");
        lines.add("class " + className + " : public juce::Component");
        lines.add("{");
        lines.add("public:");
        lines.add("    " + className + "();");
        lines.add("    ~" + className + "() override = default;");
        lines.add("");
        lines.add("    void resized() override;");
        lines.add("");
        lines.add("private:");

        if (widgets.empty())
        {
            lines.add("    // Scene is empty.");
            lines.add("    juce::Label emptySceneLabel;");
        }
        else
        {
            for (const auto& widget : widgets)
            {
                const auto memberType = widget.memberType.isNotEmpty() ? widget.memberType : juce::String("juce::Label");
                lines.add("    " + memberType + " " + widget.memberName + ";");
            }
        }

        lines.add("");
        lines.add("    // Runtime bridge (Phase 6).");
        lines.add("    void initializeRuntimeBridge();");
        lines.add("    void dispatchRuntimeEvent(juce::int64 sourceWidgetId, const juce::String& eventKey, const juce::var& payload = {});");
        lines.add("    void applyPropertyBindings();");
        lines.add("    bool applyRuntimeAction(const juce::var& action, const juce::var& payload, bool& runtimeStateChanged);");
        lines.add("    juce::Component* findRuntimeWidget(juce::int64 widgetId) const;");
        lines.add("    bool setWidgetPropertyById(juce::int64 widgetId, const juce::String& propertyKey, const juce::var& value);");
        lines.add("    std::map<juce::String, juce::var> runtimeParams;");
        lines.add("    std::map<juce::String, juce::String> runtimeParamTypes;");
        lines.add("    juce::Array<juce::var> propertyBindings;");
        lines.add("    juce::Array<juce::var> runtimeBindings;");
        lines.add("    std::map<juce::int64, juce::Component*> runtimeWidgetById;");
        lines.add("    std::map<juce::int64, bool> runtimeButtonDownStates;");
        lines.add("    bool runtimeBridgeMutating = false;");
        lines.add("    bool runtimeBridgeLoaded = false;");
        lines.add("    juce::String lastRuntimeBridgeError;");

        lines.add("");
        lines.add("    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (" + className + ")");
        lines.add("};");
        lines.add("");

        return lines.joinIntoString("\n");
    }

    double readNumericProperty(const Gyeol::PropertyBag& bag,
                               const juce::Identifier& key,
                               double fallback) noexcept
    {
        if (!bag.contains(key))
            return fallback;

        const auto& value = bag[key];
        if (!value.isInt() && !value.isInt64() && !value.isDouble())
            return fallback;

        const auto numeric = static_cast<double>(value);
        if (!std::isfinite(numeric))
            return fallback;

        return numeric;
    }

    juce::String readStringProperty(const Gyeol::PropertyBag& bag,
                                    const juce::Identifier& key,
                                    const juce::String& fallback)
    {
        if (!bag.contains(key))
            return fallback;

        const auto value = bag[key].toString();
        return value.isNotEmpty() ? value : fallback;
    }

    juce::String defaultResizedLine(const ExportWidgetEntry& widget)
    {
        const auto bounds = widget.model->bounds.getSmallestIntegerContainer();
        return "    " + widget.memberName
               + ".setBounds(" + juce::String(bounds.getX())
               + ", " + juce::String(bounds.getY())
               + ", " + juce::String(bounds.getWidth())
               + ", " + juce::String(bounds.getHeight()) + ");";
    }

    void applyBuiltinCodegen(ExportWidgetEntry& widget)
    {
        widget.memberType = "juce::Label";
        widget.codegenKind = "unsupported";
        widget.supported = false;
        widget.usesCustomCodegen = false;
        widget.constructorLines.clear();
        widget.resizedLines.clear();

        if (widget.exportTargetType == "juce::TextButton")
        {
            const auto text = readStringProperty(widget.model->properties, "text", "Button");
            widget.memberType = "juce::TextButton";
            widget.codegenKind = "juce_text_button";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setButtonText(" + toCppStringLiteral(text) + ");");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        if (widget.exportTargetType == "juce::Label")
        {
            const auto text = readStringProperty(widget.model->properties, "text", "Label");
            widget.memberType = "juce::Label";
            widget.codegenKind = "juce_label";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setText(" + toCppStringLiteral(text) + ", juce::dontSendNotification);");
            widget.constructorLines.add("    " + widget.memberName + ".setJustificationType(juce::Justification::centredLeft);");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        if (widget.exportTargetType == "juce::Slider::LinearHorizontal")
        {
            const auto value = juce::jlimit(0.0, 1.0, readNumericProperty(widget.model->properties, "value", 0.5));
            widget.memberType = "juce::Slider";
            widget.codegenKind = "juce_slider_linear";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setSliderStyle(juce::Slider::LinearHorizontal);");
            widget.constructorLines.add("    " + widget.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
            widget.constructorLines.add("    " + widget.memberName + ".setRange(0.0, 1.0, 0.0);");
            widget.constructorLines.add("    " + widget.memberName + ".setValue(" + juce::String(value, 6) + ");");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        if (widget.exportTargetType == "juce::Slider::RotaryVerticalDrag")
        {
            const auto value = juce::jlimit(0.0, 1.0, readNumericProperty(widget.model->properties, "value", 0.5));
            widget.memberType = "juce::Slider";
            widget.codegenKind = "juce_slider_rotary";
            widget.supported = true;
            widget.constructorLines.add("    " + widget.memberName + ".setSliderStyle(juce::Slider::RotaryVerticalDrag);");
            widget.constructorLines.add("    " + widget.memberName + ".setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);");
            widget.constructorLines.add("    " + widget.memberName + ".setRange(0.0, 1.0, 0.0);");
            widget.constructorLines.add("    " + widget.memberName + ".setValue(" + juce::String(value, 6) + ");");
            widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
            return;
        }

        const auto fallback = "Unsupported: " + widget.typeKey;
        widget.constructorLines.add("    " + widget.memberName + ".setText(" + toCppStringLiteral(fallback) + ", juce::dontSendNotification);");
        widget.constructorLines.add("    " + widget.memberName + ".setJustificationType(juce::Justification::centred);");
        widget.constructorLines.add("    addAndMakeVisible(" + widget.memberName + ");");
    }

    juce::Result applyCustomCodegen(const Gyeol::Widgets::WidgetDescriptor& descriptor, ExportWidgetEntry& widget)
    {
        if (!descriptor.exportCodegen)
            return juce::Result::fail("custom codegen callback is empty");

        Gyeol::Widgets::ExportCodegenContext context { *widget.model,
                                                       widget.memberName,
                                                       widget.typeKey,
                                                       widget.exportTargetType };
        Gyeol::Widgets::ExportCodegenOutput output;

        const auto result = descriptor.exportCodegen(context, output);
        if (result.failed())
            return result;

        if (output.memberType.trim().isEmpty())
            return juce::Result::fail("custom codegen returned empty memberType");

        widget.memberType = output.memberType.trim();
        widget.codegenKind = output.codegenKind.isNotEmpty() ? output.codegenKind : juce::String("custom");
        widget.constructorLines = output.constructorLines;
        widget.resizedLines = output.resizedLines;
        widget.supported = true;
        widget.usesCustomCodegen = true;
        return juce::Result::ok();
    }

    juce::String buildRuntimeBridgeSourceBlock(const juce::String& className,
                                               const std::vector<ExportWidgetEntry>& widgets,
                                               const juce::String& runtimeDataJson)
    {
        juce::StringArray widgetMapLines;
        juce::StringArray eventHookLines;
        widgetMapLines.ensureStorageAllocated(static_cast<int>(widgets.size()));
        eventHookLines.ensureStorageAllocated(static_cast<int>(widgets.size() * 8));

        for (const auto& widget : widgets)
        {
            if (widget.model == nullptr)
                continue;

            const auto widgetIdText = juce::String(widget.model->id);
            widgetMapLines.add("    runtimeWidgetById.emplace(" + widgetIdText + ", &" + widget.memberName + ");");

            switch (widget.model->type)
            {
                case Gyeol::WidgetType::button:
                {
                    eventHookLines.add("    runtimeButtonDownStates[" + widgetIdText + "] = false;");
                    eventHookLines.add("    " + widget.memberName + ".onClick = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onClick\", true);");
                    eventHookLines.add("    };");
                    eventHookLines.add("    " + widget.memberName + ".onStateChange = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        const auto isDown = " + widget.memberName + ".isDown();");
                    eventHookLines.add("        const auto previous = runtimeButtonDownStates[" + widgetIdText + "];");
                    eventHookLines.add("        if (isDown == previous)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        runtimeButtonDownStates[" + widgetIdText + "] = isDown;");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", isDown ? \"onPress\" : \"onRelease\", isDown);");
                    eventHookLines.add("    };");
                    break;
                }

                case Gyeol::WidgetType::toggle:
                {
                    eventHookLines.add("    " + widget.memberName + ".onClick = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        const auto state = " + widget.memberName + ".getToggleState();");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onClick\", state);");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onToggleChanged\", state);");
                    eventHookLines.add("    };");
                    break;
                }

                case Gyeol::WidgetType::slider:
                case Gyeol::WidgetType::knob:
                {
                    eventHookLines.add("    " + widget.memberName + ".onValueChange = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onValueChanged\", " + widget.memberName + ".getValue());");
                    eventHookLines.add("    };");
                    eventHookLines.add("    " + widget.memberName + ".onDragEnd = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onValueCommit\", " + widget.memberName + ".getValue());");
                    eventHookLines.add("    };");
                    break;
                }

                case Gyeol::WidgetType::comboBox:
                {
                    eventHookLines.add("    " + widget.memberName + ".onChange = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onSelectionChanged\", " + widget.memberName + ".getSelectedId());");
                    eventHookLines.add("    };");
                    break;
                }

                case Gyeol::WidgetType::textInput:
                {
                    eventHookLines.add("    " + widget.memberName + ".onReturnKey = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onTextCommit\", " + widget.memberName + ".getText());");
                    eventHookLines.add("    };");
                    eventHookLines.add("    " + widget.memberName + ".onFocusLost = [this]()");
                    eventHookLines.add("    {");
                    eventHookLines.add("        if (runtimeBridgeMutating)");
                    eventHookLines.add("            return;");
                    eventHookLines.add("        dispatchRuntimeEvent(" + widgetIdText + ", \"onTextCommit\", " + widget.memberName + ".getText());");
                    eventHookLines.add("    };");
                    break;
                }

                case Gyeol::WidgetType::label:
                case Gyeol::WidgetType::meter:
                    break;
            }
        }

        auto helperBlock = juce::String(R"__GYEOL__(
namespace
{
    juce::File resolveExportAssetFile(const juce::String& relativePath)
    {
        auto baseDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
        return baseDir.getChildFile(relativePath);
    }

    juce::Image preloadExportAssetImage(const juce::String& relativePath)
    {
        const auto file = resolveExportAssetFile(relativePath);
        if (!file.existsAsFile())
            return {};
        return juce::ImageFileFormat::loadFrom(file);
    }

    juce::int64 parseWidgetId(const juce::var& value)
    {
        if (value.isInt() || value.isInt64())
            return static_cast<juce::int64>(value);

        const auto text = value.toString().trim();
        if (text.isEmpty())
            return 0;

        return text.getLargeIntValue();
    }

    double readFiniteDouble(const juce::var& value, double fallback)
    {
        if (!value.isInt() && !value.isInt64() && !value.isDouble() && !value.isBool())
            return fallback;

        const auto numeric = static_cast<double>(value);
        return std::isfinite(numeric) ? numeric : fallback;
    }

    bool valueIsTruthy(const juce::var& value)
    {
        if (value.isBool())
            return static_cast<bool>(value);
        if (value.isInt() || value.isInt64() || value.isDouble())
            return std::abs(static_cast<double>(value)) > 0.000000000001;

        const auto text = value.toString().trim().toLowerCase();
        return text == "1" || text == "true" || text == "yes" || text == "on";
    }

    bool normalizeRuntimeParamValue(const juce::String& declaredType,
                                    const juce::var& inputValue,
                                    juce::var& outValue,
                                    juce::String& errorOut)
    {
        const auto type = declaredType.trim().toLowerCase();

        if (type == "boolean")
        {
            outValue = valueIsTruthy(inputValue);
            return true;
        }

        if (type == "string")
        {
            outValue = inputValue.toString();
            return true;
        }

        if (inputValue.isInt() || inputValue.isInt64() || inputValue.isDouble() || inputValue.isBool())
        {
            const auto numeric = static_cast<double>(inputValue);
            if (!std::isfinite(numeric))
            {
                errorOut = "numeric value must be finite";
                return false;
            }

            outValue = numeric;
            return true;
        }

        if (inputValue.isString())
        {
            const auto text = inputValue.toString().trim();
            if (text.isEmpty())
            {
                errorOut = "numeric value is empty";
                return false;
            }

            const auto textStd = text.toStdString();
            char* endPtr = nullptr;
            const auto parsed = std::strtod(textStd.c_str(), &endPtr);
            if (endPtr == textStd.c_str() || *endPtr != '\0' || !std::isfinite(parsed))
            {
                errorOut = "numeric value parse failed";
                return false;
            }

            outValue = parsed;
            return true;
        }

        errorOut = "unsupported numeric value type";
        return false;
    }

    juce::String resolveRuntimeParamKey(const std::map<juce::String, juce::var>& params,
                                        const juce::String& requestedKey)
    {
        const auto trimmed = requestedKey.trim();
        if (trimmed.isEmpty())
            return {};

        if (params.find(trimmed) != params.end())
            return trimmed;

        for (const auto& entry : params)
        {
            if (entry.first.equalsIgnoreCase(trimmed))
                return entry.first;
        }

        return trimmed;
    }

    bool evaluateRuntimeExpression(const juce::String& expression,
                                   const std::map<juce::String, juce::var>& runtimeParams,
                                   double& outValue,
                                   juce::String& errorOut)
    {
        class Parser
        {
        public:
            Parser(const juce::String& expressionIn,
                   const std::map<juce::String, juce::var>& runtimeParamsIn)
                : expression(expressionIn),
                  params(runtimeParamsIn)
            {
            }

            bool parse(double& resultValue, juce::String& resultError)
            {
                skipWhitespace();

                if (expression.trim().isEmpty())
                {
                    resultError = "expression is empty";
                    return false;
                }

                if (!parseExpression(resultValue))
                {
                    resultError = errorText.isNotEmpty() ? errorText : "failed to parse expression";
                    return false;
                }

                skipWhitespace();
                if (!isAtEnd())
                {
                    resultError = "unexpected token near '" + juce::String::charToString(currentChar()) + "'";
                    return false;
                }

                if (!std::isfinite(resultValue))
                {
                    resultError = "expression result is not finite";
                    return false;
                }

                return true;
            }

        private:
            const juce::String& expression;
            const std::map<juce::String, juce::var>& params;
            int position = 0;
            juce::String errorText;

            bool parseExpression(double& out)
            {
                if (!parseTerm(out))
                    return false;

                while (true)
                {
                    skipWhitespace();
                    if (match('+'))
                    {
                        double rhs = 0.0;
                        if (!parseTerm(rhs))
                            return false;
                        out += rhs;
                        continue;
                    }

                    if (match('-'))
                    {
                        double rhs = 0.0;
                        if (!parseTerm(rhs))
                            return false;
                        out -= rhs;
                        continue;
                    }

                    return true;
                }
            }

            bool parseTerm(double& out)
            {
                if (!parseFactor(out))
                    return false;

                while (true)
                {
                    skipWhitespace();
                    if (match('*'))
                    {
                        double rhs = 0.0;
                        if (!parseFactor(rhs))
                            return false;
                        out *= rhs;
                        continue;
                    }

                    if (match('/'))
                    {
                        double rhs = 0.0;
                        if (!parseFactor(rhs))
                            return false;
                        if (std::abs(rhs) <= 0.000000000001)
                        {
                            errorText = "division by zero";
                            return false;
                        }
                        out /= rhs;
                        continue;
                    }

                    return true;
                }
            }

            bool parseFactor(double& out)
            {
                skipWhitespace();

                if (match('+'))
                    return parseFactor(out);

                if (match('-'))
                {
                    if (!parseFactor(out))
                        return false;
                    out = -out;
                    return true;
                }

                if (match('('))
                {
                    if (!parseExpression(out))
                        return false;
                    skipWhitespace();
                    if (!match(')'))
                    {
                        errorText = "')' expected";
                        return false;
                    }
                    return true;
                }

                const auto ch = currentChar();
                if (isNumberStart(ch))
                    return parseNumber(out);

                if (isIdentifierStart(ch))
                {
                    juce::String identifier;
                    if (!parseIdentifier(identifier))
                        return false;
                    return parseIdentifierValue(identifier, out);
                }

                errorText = "unexpected token near '" + juce::String::charToString(ch) + "'";
                return false;
            }

            bool parseNumber(double& out)
            {
                const auto remaining = expression.substring(position).trimStart();
                const auto remainingStd = remaining.toStdString();
                if (remainingStd.empty())
                {
                    errorText = "number expected";
                    return false;
                }

                char* endPtr = nullptr;
                const auto parsed = std::strtod(remainingStd.c_str(), &endPtr);
                if (endPtr == remainingStd.c_str())
                {
                    errorText = "number expected";
                    return false;
                }
                if (!std::isfinite(parsed))
                {
                    errorText = "number is not finite";
                    return false;
                }

                const auto consumed = static_cast<int>(endPtr - remainingStd.c_str());
                skipWhitespace();
                position += consumed;
                out = parsed;
                return true;
            }

            bool parseIdentifier(juce::String& outIdentifier)
            {
                skipWhitespace();
                if (!isIdentifierStart(currentChar()))
                {
                    errorText = "identifier expected";
                    return false;
                }

                const auto start = position;
                ++position;
                while (!isAtEnd() && isIdentifierBody(currentChar()))
                    ++position;

                outIdentifier = expression.substring(start, position).trim();
                if (outIdentifier.isEmpty())
                {
                    errorText = "identifier expected";
                    return false;
                }

                return true;
            }

            bool parseIdentifierValue(const juce::String& identifier, double& out)
            {
                auto toNumeric = [this](const juce::var& value, const juce::String& key, double& converted) -> bool
                {
                    if (value.isInt() || value.isInt64() || value.isDouble() || value.isBool())
                    {
                        converted = static_cast<double>(value);
                        if (!std::isfinite(converted))
                        {
                            errorText = "param '" + key + "' is not finite";
                            return false;
                        }
                        return true;
                    }

                    if (value.isString())
                    {
                        const auto text = value.toString().trim();
                        if (text.isEmpty())
                        {
                            errorText = "param '" + key + "' cannot be converted to number";
                            return false;
                        }

                        const auto textStd = text.toStdString();
                        char* endPtr = nullptr;
                        const auto parsed = std::strtod(textStd.c_str(), &endPtr);
                        if (endPtr == textStd.c_str() || *endPtr != '\0' || !std::isfinite(parsed))
                        {
                            errorText = "param '" + key + "' cannot be converted to number";
                            return false;
                        }

                        converted = parsed;
                        return true;
                    }

                    errorText = "param '" + key + "' has unsupported type";
                    return false;
                };

                if (const auto it = params.find(identifier); it != params.end())
                    return toNumeric(it->second, identifier, out);

                for (const auto& entry : params)
                {
                    if (!entry.first.equalsIgnoreCase(identifier))
                        continue;
                    return toNumeric(entry.second, entry.first, out);
                }

                errorText = "unknown runtime param '" + identifier + "'";
                return false;
            }

            void skipWhitespace()
            {
                while (!isAtEnd() && juce::CharacterFunctions::isWhitespace(currentChar()))
                    ++position;
            }

            bool isAtEnd() const noexcept
            {
                return position >= expression.length();
            }

            juce::juce_wchar currentChar() const noexcept
            {
                if (isAtEnd())
                    return 0;
                return expression[position];
            }

            bool match(juce::juce_wchar expected)
            {
                if (currentChar() != expected)
                    return false;
                ++position;
                return true;
            }

            static bool isAsciiDigit(juce::juce_wchar ch) noexcept
            {
                return ch >= '0' && ch <= '9';
            }

            static bool isIdentifierStart(juce::juce_wchar ch) noexcept
            {
                return (ch >= 'a' && ch <= 'z')
                    || (ch >= 'A' && ch <= 'Z')
                    || ch == '_';
            }

            static bool isIdentifierBody(juce::juce_wchar ch) noexcept
            {
                return isIdentifierStart(ch) || isAsciiDigit(ch) || ch == '.';
            }

            static bool isNumberStart(juce::juce_wchar ch) noexcept
            {
                return isAsciiDigit(ch) || ch == '.';
            }
        };

        Parser parser(expression, runtimeParams);
        return parser.parse(outValue, errorOut);
    }
}
)__GYEOL__");

        auto runtimeMethods = juce::String(R"__GYEOL__(
void __CLASS__::initializeRuntimeBridge()
{
    runtimeBridgeLoaded = false;
    lastRuntimeBridgeError.clear();
    runtimeParams.clear();
    runtimeParamTypes.clear();
    propertyBindings.clear();
    runtimeBindings.clear();
    runtimeWidgetById.clear();
    runtimeButtonDownStates.clear();

__WIDGET_MAP__

__EVENT_HOOKS__

    auto runtimeDataText = juce::String();
    const auto runtimeDataFile = resolveExportAssetFile("export-runtime.json");
    if (runtimeDataFile.existsAsFile())
        runtimeDataText = runtimeDataFile.loadFileAsString();

    if (runtimeDataText.trim().isEmpty())
        runtimeDataText = __EMBEDDED_RUNTIME_JSON__;

    const auto parsed = juce::JSON::parse(runtimeDataText);
    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        lastRuntimeBridgeError = "runtime data parse failed";
        DBG("[GyeolExport] runtime bridge parse failed");
        return;
    }

    if (auto* runtimeParamArray = root->getProperty("runtimeParams").getArray(); runtimeParamArray != nullptr)
    {
        for (const auto& paramVar : *runtimeParamArray)
        {
            auto* paramObject = paramVar.getDynamicObject();
            if (paramObject == nullptr)
                continue;

            const auto key = paramObject->getProperty("key").toString().trim();
            if (key.isEmpty())
                continue;

            auto declaredType = paramObject->getProperty("type").toString().trim().toLowerCase();
            if (declaredType.isEmpty())
                declaredType = "number";
            runtimeParamTypes[key] = declaredType;

            const auto defaultValue = paramObject->getProperty("defaultValue");
            juce::var normalized;
            juce::String normalizeError;
            if (!normalizeRuntimeParamValue(declaredType, defaultValue, normalized, normalizeError))
            {
                normalized = (declaredType == "boolean") ? juce::var(false)
                           : (declaredType == "string") ? juce::var(juce::String())
                                                        : juce::var(0.0);
                DBG("[GyeolExport] runtime param normalize failed key=" + key
                    + " message=" + normalizeError);
            }

            runtimeParams[key] = normalized;
        }
    }

    if (auto* propertyBindingArray = root->getProperty("propertyBindings").getArray(); propertyBindingArray != nullptr)
    {
        for (const auto& binding : *propertyBindingArray)
            propertyBindings.add(binding);
    }

    if (auto* runtimeBindingArray = root->getProperty("runtimeBindings").getArray(); runtimeBindingArray != nullptr)
    {
        for (const auto& binding : *runtimeBindingArray)
            runtimeBindings.add(binding);
    }

    runtimeBridgeLoaded = true;
    applyPropertyBindings();
}

void __CLASS__::dispatchRuntimeEvent(juce::int64 sourceWidgetId,
                                     const juce::String& eventKey,
                                     const juce::var& payload)
{
    if (!runtimeBridgeLoaded || runtimeBridgeMutating || sourceWidgetId <= 0)
        return;

    const auto normalizedEventKey = eventKey.trim();
    if (normalizedEventKey.isEmpty())
        return;

    bool runtimeStateChanged = false;
    int executedActionCount = 0;

    for (const auto& bindingVar : runtimeBindings)
    {
        auto* bindingObject = bindingVar.getDynamicObject();
        if (bindingObject == nullptr)
            continue;
        if (bindingObject->hasProperty("enabled")
            && !valueIsTruthy(bindingObject->getProperty("enabled")))
            continue;
        if (parseWidgetId(bindingObject->getProperty("sourceWidgetId")) != sourceWidgetId)
            continue;
        if (bindingObject->getProperty("eventKey").toString().trim() != normalizedEventKey)
            continue;

        if (auto* actions = bindingObject->getProperty("actions").getArray(); actions != nullptr)
        {
            for (const auto& action : *actions)
            {
                if (++executedActionCount > 256)
                {
                    DBG("[GyeolExport] runtime action limit reached (256)");
                    break;
                }

                applyRuntimeAction(action, payload, runtimeStateChanged);
            }
        }
    }

    if (runtimeStateChanged)
        applyPropertyBindings();
}

void __CLASS__::applyPropertyBindings()
{
    if (!runtimeBridgeLoaded || runtimeBridgeMutating || propertyBindings.isEmpty())
        return;

    juce::ScopedValueSetter<bool> mutatingGuard(runtimeBridgeMutating, true);

    for (const auto& bindingVar : propertyBindings)
    {
        auto* bindingObject = bindingVar.getDynamicObject();
        if (bindingObject == nullptr)
            continue;
        if (bindingObject->hasProperty("enabled")
            && !valueIsTruthy(bindingObject->getProperty("enabled")))
            continue;

        const auto targetWidgetId = parseWidgetId(bindingObject->getProperty("targetWidgetId"));
        if (targetWidgetId <= 0)
            continue;

        const auto targetProperty = bindingObject->getProperty("targetProperty").toString().trim();
        if (targetProperty.isEmpty())
            continue;

        const auto expression = bindingObject->getProperty("expression").toString();
        double value = 0.0;
        juce::String error;
        if (!evaluateRuntimeExpression(expression, runtimeParams, value, error))
        {
            DBG("[GyeolExport] property binding eval failed target=" + juce::String(targetWidgetId)
                + " property=" + targetProperty + " error=" + error);
            continue;
        }

        setWidgetPropertyById(targetWidgetId, targetProperty, value);
    }
}

bool __CLASS__::applyRuntimeAction(const juce::var& action,
                                   const juce::var& payload,
                                   bool& runtimeStateChanged)
{
    auto* actionObject = action.getDynamicObject();
    if (actionObject == nullptr)
        return false;

    const auto kind = actionObject->getProperty("kind").toString().trim().toLowerCase();
    if (kind == "setruntimeparam")
    {
        const auto requestedKey = actionObject->getProperty("paramKey").toString().trim();
        if (requestedKey.isEmpty())
            return false;

        const auto resolvedKey = resolveRuntimeParamKey(runtimeParams, requestedKey);
        auto declaredType = juce::String("number");
        if (const auto it = runtimeParamTypes.find(resolvedKey); it != runtimeParamTypes.end())
            declaredType = it->second;
        else
            runtimeParamTypes[resolvedKey] = declaredType;

        const auto requestedValue = actionObject->hasProperty("value")
                                      ? actionObject->getProperty("value")
                                      : payload;
        juce::var normalizedValue;
        juce::String normalizeError;
        if (!normalizeRuntimeParamValue(declaredType, requestedValue, normalizedValue, normalizeError))
            return false;

        if (const auto it = runtimeParams.find(resolvedKey);
            it == runtimeParams.end() || it->second != normalizedValue)
        {
            runtimeParams[resolvedKey] = normalizedValue;
            runtimeStateChanged = true;
        }

        return true;
    }

    if (kind == "adjustruntimeparam")
    {
        const auto requestedKey = actionObject->getProperty("paramKey").toString().trim();
        if (requestedKey.isEmpty())
            return false;

        const auto resolvedKey = resolveRuntimeParamKey(runtimeParams, requestedKey);
        const auto delta = readFiniteDouble(actionObject->getProperty("delta"), 0.0);
        auto current = 0.0;
        if (const auto it = runtimeParams.find(resolvedKey); it != runtimeParams.end())
            current = readFiniteDouble(it->second, 0.0);

        const auto next = current + delta;
        if (!std::isfinite(next))
            return false;

        if (const auto it = runtimeParams.find(resolvedKey);
            it == runtimeParams.end() || it->second != juce::var(next))
        {
            runtimeParams[resolvedKey] = next;
            runtimeStateChanged = true;
        }

        return true;
    }

    if (kind == "toggleruntimeparam")
    {
        const auto requestedKey = actionObject->getProperty("paramKey").toString().trim();
        if (requestedKey.isEmpty())
            return false;

        const auto resolvedKey = resolveRuntimeParamKey(runtimeParams, requestedKey);
        const auto current = [this, &resolvedKey]()
        {
            if (const auto it = runtimeParams.find(resolvedKey); it != runtimeParams.end())
                return valueIsTruthy(it->second);
            return false;
        }();

        const auto next = !current;
        if (const auto it = runtimeParams.find(resolvedKey);
            it == runtimeParams.end() || !it->second.equalsWithSameType(next))
        {
            runtimeParams[resolvedKey] = next;
            runtimeStateChanged = true;
        }

        return true;
    }

    if (kind == "setnodeprops")
    {
        auto targetWidgetId = parseWidgetId(actionObject->getProperty("targetId"));
        if (targetWidgetId <= 0)
            targetWidgetId = parseWidgetId(actionObject->getProperty("targetWidgetId"));
        if (targetWidgetId <= 0)
            return false;

        if (actionObject->hasProperty("visible"))
            setWidgetPropertyById(targetWidgetId, "visible", actionObject->getProperty("visible"));
        if (actionObject->hasProperty("opacity"))
            setWidgetPropertyById(targetWidgetId, "opacity", actionObject->getProperty("opacity"));

        if (auto* patchObject = actionObject->getProperty("patch").getDynamicObject(); patchObject != nullptr)
        {
            const auto& properties = patchObject->getProperties();
            for (int i = 0; i < properties.size(); ++i)
                setWidgetPropertyById(targetWidgetId, properties.getName(i).toString(), properties.getValueAt(i));
        }

        return true;
    }

    if (kind == "setnodebounds")
    {
        const auto targetWidgetId = parseWidgetId(actionObject->getProperty("targetWidgetId"));
        if (targetWidgetId <= 0)
            return false;

        auto* target = findRuntimeWidget(targetWidgetId);
        if (target == nullptr)
            return false;

        auto nextBounds = target->getBounds();
        if (auto* bounds = actionObject->getProperty("bounds").getDynamicObject(); bounds != nullptr)
        {
            nextBounds.setX(static_cast<int>(std::round(readFiniteDouble(bounds->getProperty("x"), nextBounds.getX()))));
            nextBounds.setY(static_cast<int>(std::round(readFiniteDouble(bounds->getProperty("y"), nextBounds.getY()))));
            nextBounds.setWidth(std::max(1, static_cast<int>(std::round(readFiniteDouble(bounds->getProperty("w"), nextBounds.getWidth())))));
            nextBounds.setHeight(std::max(1, static_cast<int>(std::round(readFiniteDouble(bounds->getProperty("h"), nextBounds.getHeight())))));
        }

        if (nextBounds != target->getBounds())
        {
            target->setBounds(nextBounds);
            return true;
        }

        return false;
    }

    return false;
}

juce::Component* __CLASS__::findRuntimeWidget(juce::int64 widgetId) const
{
    if (const auto it = runtimeWidgetById.find(widgetId); it != runtimeWidgetById.end())
        return it->second;
    return nullptr;
}

bool __CLASS__::setWidgetPropertyById(juce::int64 widgetId,
                                      const juce::String& propertyKey,
                                      const juce::var& value)
{
    auto* component = findRuntimeWidget(widgetId);
    if (component == nullptr)
        return false;

    const auto key = propertyKey.trim().toLowerCase();
    if (key.isEmpty())
        return false;

    if (key == "visible")
    {
        component->setVisible(valueIsTruthy(value));
        return true;
    }

    if (key == "enabled")
    {
        component->setEnabled(valueIsTruthy(value));
        return true;
    }

    if (key == "opacity" || key == "alpha")
    {
        component->setAlpha(static_cast<float>(juce::jlimit(0.0, 1.0, readFiniteDouble(value, component->getAlpha()))));
        return true;
    }

    if (key == "x" || key == "y" || key == "w" || key == "h")
    {
        auto bounds = component->getBounds();
        if (key == "x")
            bounds.setX(static_cast<int>(std::round(readFiniteDouble(value, bounds.getX()))));
        else if (key == "y")
            bounds.setY(static_cast<int>(std::round(readFiniteDouble(value, bounds.getY()))));
        else if (key == "w")
            bounds.setWidth(std::max(1, static_cast<int>(std::round(readFiniteDouble(value, bounds.getWidth())))));
        else
            bounds.setHeight(std::max(1, static_cast<int>(std::round(readFiniteDouble(value, bounds.getHeight())))));

        component->setBounds(bounds);
        return true;
    }

    if (auto* slider = dynamic_cast<juce::Slider*>(component))
    {
        if (key == "value")
        {
            slider->setValue(readFiniteDouble(value, slider->getValue()), juce::dontSendNotification);
            return true;
        }
    }

    if (auto* toggle = dynamic_cast<juce::ToggleButton*>(component))
    {
        if (key == "state")
        {
            toggle->setToggleState(valueIsTruthy(value), juce::dontSendNotification);
            return true;
        }

        if (key == "text")
        {
            toggle->setButtonText(value.toString());
            return true;
        }
    }

    if (auto* button = dynamic_cast<juce::TextButton*>(component))
    {
        if (key == "text")
        {
            button->setButtonText(value.toString());
            return true;
        }
    }

    if (auto* label = dynamic_cast<juce::Label*>(component))
    {
        if (key == "text")
        {
            label->setText(value.toString(), juce::dontSendNotification);
            return true;
        }
    }

    if (auto* combo = dynamic_cast<juce::ComboBox*>(component))
    {
        if (key == "combo.selectedindex")
        {
            auto selectedId = static_cast<int>(std::llround(readFiniteDouble(value, combo->getSelectedId())));
            if (combo->getNumItems() > 0)
                selectedId = juce::jlimit(1, combo->getNumItems(), std::max(1, selectedId));
            combo->setSelectedId(selectedId, juce::dontSendNotification);
            return true;
        }
    }

    if (auto* editor = dynamic_cast<juce::TextEditor*>(component))
    {
        if (key == "text")
        {
            editor->setText(value.toString(), false);
            return true;
        }
    }

    return false;
}
)__GYEOL__");

        runtimeMethods = runtimeMethods.replace("__CLASS__", className);
        runtimeMethods = runtimeMethods.replace("__WIDGET_MAP__",
                                               widgetMapLines.isEmpty()
                                                   ? juce::String("    // no widgets in scene")
                                                   : widgetMapLines.joinIntoString("\n"));
        runtimeMethods = runtimeMethods.replace("__EVENT_HOOKS__",
                                               eventHookLines.isEmpty()
                                                   ? juce::String("    // no runtime event emitters")
                                                   : eventHookLines.joinIntoString("\n"));
        runtimeMethods = runtimeMethods.replace("__EMBEDDED_RUNTIME_JSON__",
                                               toCppStringLiteral(runtimeDataJson.isNotEmpty()
                                                                      ? runtimeDataJson
                                                                      : juce::String("{}")));

        helperBlock << "\n" << runtimeMethods << "\n";
        return helperBlock;
    }

    juce::String generateSourceCode(const juce::String& className,
                                    const std::vector<ExportWidgetEntry>& widgets,
                                    const std::vector<juce::String>& assetPreloadPaths,
                                    const juce::String& runtimeDataJson)
    {
        juce::StringArray lines;
        lines.add("#include \"" + className + ".h\"");
        lines.add("");
        lines.add("#include <algorithm>");
        lines.add("#include <cmath>");
        lines.add("#include <cstdlib>");
        lines.add("");
        lines.addLines(buildRuntimeBridgeSourceBlock(className, widgets, runtimeDataJson));
        lines.add("");
        lines.add(className + "::" + className + "()");
        lines.add("{");

        if (!assetPreloadPaths.empty())
        {
            lines.add("    // Preload exported asset binaries from Assets/.");
            for (const auto& relativePath : assetPreloadPaths)
            {
                lines.add("    juce::ignoreUnused(preloadExportAssetImage(" + toCppStringLiteral(relativePath) + "));");
            }
            lines.add("");
        }

        if (widgets.empty())
        {
            lines.add("    emptySceneLabel.setText(\"Scene is empty\", juce::dontSendNotification);");
            lines.add("    emptySceneLabel.setJustificationType(juce::Justification::centred);");
            lines.add("    addAndMakeVisible(emptySceneLabel);");
        }
        else
        {
            for (const auto& widget : widgets)
            {
                lines.add("    // Widget id=" + juce::String(widget.model->id)
                          + ", type=" + widget.typeKey
                          + ", target=" + widget.exportTargetType
                          + ", codegen=" + (widget.codegenKind.isNotEmpty() ? widget.codegenKind : juce::String("unknown")));

                for (const auto& constructorLine : widget.constructorLines)
                    lines.add(constructorLine);

                lines.add("");
            }
        }

        lines.add("    initializeRuntimeBridge();");
        lines.add("}");
        lines.add("");
        lines.add("void " + className + "::resized()");
        lines.add("{");

        if (widgets.empty())
        {
            lines.add("    emptySceneLabel.setBounds(getLocalBounds());");
        }
        else
        {
            for (const auto& widget : widgets)
            {
                for (const auto& resizedLine : widget.resizedLines)
                    lines.add(resizedLine);
            }
        }

        lines.add("}");
        lines.add("");
        return lines.joinIntoString("\n");
    }

    juce::File resolveInputFilePath(const juce::String& value, const Gyeol::Export::ExportOptions& options)
    {
        if (juce::File::isAbsolutePath(value))
            return juce::File(value);

        if (options.projectRootDirectory.getFullPathName().isNotEmpty())
            return options.projectRootDirectory.getChildFile(value);

        return juce::File::getCurrentWorkingDirectory().getChildFile(value);
    }

    juce::String normalizeRelativePath(const juce::String& value)
    {
        auto normalized = value.trim().replaceCharacter('\\', '/');
        while (normalized.startsWith("/"))
            normalized = normalized.substring(1);
        while (normalized.contains("//"))
            normalized = normalized.replace("//", "/");
        while (normalized.startsWith("../"))
            normalized = normalized.substring(3);
        while (normalized.contains("/../"))
            normalized = normalized.replace("/../", "/");
        return normalized;
    }

    juce::String buildPreferredExportAssetRelativePath(const Gyeol::AssetModel& asset, const juce::File& sourceFile)
    {
        auto normalized = normalizeRelativePath(asset.relativePath);
        if (normalized.isEmpty())
            normalized = sourceFile.getFileName();

        const auto slash = normalized.lastIndexOfChar('/');
        auto parent = slash >= 0 ? normalized.substring(0, slash) : juce::String();
        auto fileName = slash >= 0 ? normalized.substring(slash + 1) : normalized;
        fileName = fileName.trim();
        if (fileName.isEmpty())
            fileName = sourceFile.getFileName();

        if (parent.startsWithIgnoreCase("Assets/"))
            parent = parent.fromFirstOccurrenceOf("Assets/", false, false);

        auto relative = juce::String("Assets");
        if (parent.isNotEmpty())
            relative << "/" << parent;
        relative << "/" << fileName;
        return normalizeRelativePath(relative);
    }

    bool isAssetExcludedFromExport(const Gyeol::AssetModel& asset)
    {
        static constexpr const char* kExportExcludeKey = "export.exclude";
        if (!asset.meta.contains(kExportExcludeKey))
            return false;

        const auto& raw = asset.meta[kExportExcludeKey];
        if (raw.isBool())
            return static_cast<bool>(raw);
        if (raw.isInt() || raw.isInt64() || raw.isDouble())
            return static_cast<double>(raw) != 0.0;

        const auto text = raw.toString().trim().toLowerCase();
        return text == "true" || text == "1" || text == "yes" || text == "on";
    }

    juce::File makeUniqueDestinationPath(const juce::File& outputDirectory,
                                         const juce::String& preferredRelativePath)
    {
        auto normalized = normalizeRelativePath(preferredRelativePath);
        auto candidate = outputDirectory.getChildFile(normalized);
        if (!candidate.existsAsFile())
            return candidate;

        const auto fileName = candidate.getFileName();
        const auto stem = candidate.getFileNameWithoutExtension();
        const auto ext = candidate.getFileExtension();
        auto parentPath = normalizeRelativePath(normalized.upToLastOccurrenceOf(fileName, false, false));
        if (parentPath.endsWith("/"))
            parentPath = parentPath.dropLastCharacters(1);

        for (int suffix = 2; suffix < 10000; ++suffix)
        {
            const auto suffixedName = stem + "_" + juce::String(suffix) + ext;
            auto relative = parentPath.isNotEmpty() ? (parentPath + "/" + suffixedName) : suffixedName;
            relative = normalizeRelativePath(relative);
            candidate = outputDirectory.getChildFile(relative);
            if (!candidate.existsAsFile())
                return candidate;
        }

        const auto fallback = stem + "_" + juce::String(juce::Time::currentTimeMillis()) + ext;
        auto relative = parentPath.isNotEmpty() ? (parentPath + "/" + fallback) : fallback;
        relative = normalizeRelativePath(relative);
        return outputDirectory.getChildFile(relative);
    }

    juce::var serializePropertiesForManifest(const Gyeol::PropertyBag& properties)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        for (int i = 0; i < properties.size(); ++i)
            object->setProperty(properties.getName(i), properties.getValueAt(i));

        return juce::var(object.release());
    }

    juce::var serializeBoundsForManifest(const juce::Rectangle<float>& bounds)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("x", bounds.getX());
        object->setProperty("y", bounds.getY());
        object->setProperty("w", bounds.getWidth());
        object->setProperty("h", bounds.getHeight());
        return juce::var(object.release());
    }

    juce::var serializeSchemaVersionForManifest(const Gyeol::SchemaVersion& version)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("major", version.major);
        object->setProperty("minor", version.minor);
        object->setProperty("patch", version.patch);
        object->setProperty("packed", version.major * 10000 + version.minor * 100 + version.patch);
        return juce::var(object.release());
    }

    juce::var serializeRuntimeParamForManifest(const Gyeol::RuntimeParamModel& param)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("key", param.key);
        object->setProperty("type", Gyeol::runtimeParamValueTypeToKey(param.type));
        object->setProperty("defaultValue", param.defaultValue);
        object->setProperty("description", param.description);
        object->setProperty("exposed", param.exposed);
        return juce::var(object.release());
    }

    juce::var serializePropertyBindingForManifest(const Gyeol::PropertyBindingModel& binding)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(binding.id));
        object->setProperty("name", binding.name);
        object->setProperty("enabled", binding.enabled);
        object->setProperty("targetWidgetId", Gyeol::widgetIdToJsonString(binding.targetWidgetId));
        object->setProperty("targetProperty", binding.targetProperty);
        object->setProperty("expression", binding.expression);
        return juce::var(object.release());
    }

    juce::var serializeRuntimeActionForManifest(const Gyeol::RuntimeActionModel& action)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("kind", Gyeol::runtimeActionKindToKey(action.kind));

        switch (action.kind)
        {
            case Gyeol::RuntimeActionKind::setRuntimeParam:
                object->setProperty("paramKey", action.paramKey);
                object->setProperty("value", action.value);
                break;

            case Gyeol::RuntimeActionKind::adjustRuntimeParam:
                object->setProperty("paramKey", action.paramKey);
                object->setProperty("delta", action.delta);
                break;

            case Gyeol::RuntimeActionKind::toggleRuntimeParam:
                object->setProperty("paramKey", action.paramKey);
                break;

            case Gyeol::RuntimeActionKind::setNodeProps:
                object->setProperty("targetKind", nodeKindToKey(action.target.kind));
                object->setProperty("targetId", Gyeol::widgetIdToJsonString(action.target.id));
                if (action.visible.has_value())
                    object->setProperty("visible", *action.visible);
                if (action.locked.has_value())
                    object->setProperty("locked", *action.locked);
                if (action.opacity.has_value())
                    object->setProperty("opacity", *action.opacity);
                object->setProperty("patch", serializePropertiesForManifest(action.patch));
                break;

            case Gyeol::RuntimeActionKind::setNodeBounds:
                object->setProperty("targetWidgetId", Gyeol::widgetIdToJsonString(action.targetWidgetId));
                object->setProperty("bounds", serializeBoundsForManifest(action.bounds));
                break;
        }

        return juce::var(object.release());
    }

    juce::var serializeRuntimeBindingForManifest(const Gyeol::RuntimeBindingModel& binding)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(binding.id));
        object->setProperty("name", binding.name);
        object->setProperty("enabled", binding.enabled);
        object->setProperty("sourceWidgetId", Gyeol::widgetIdToJsonString(binding.sourceWidgetId));
        object->setProperty("eventKey", binding.eventKey);

        juce::Array<juce::var> actionArray;
        for (const auto& action : binding.actions)
            actionArray.add(serializeRuntimeActionForManifest(action));
        object->setProperty("actions", juce::var(actionArray));

        return juce::var(object.release());
    }

    juce::String relativePathOrAbsolute(const juce::File& target, const juce::File& baseDirectory)
    {
        if (baseDirectory.getFullPathName().isNotEmpty())
        {
            const auto relative = target.getRelativePathFrom(baseDirectory);
            if (relative.isNotEmpty())
                return relative.replaceCharacter('\\', '/');
        }

        return target.getFullPathName().replaceCharacter('\\', '/');
    }

    juce::String buildManifestJson(const Gyeol::DocumentModel& document,
                                   const juce::String& componentClassName,
                                   const std::vector<ExportWidgetEntry>& widgets,
                                   const std::vector<CopiedAssetEntry>& assets,
                                   const juce::String& runtimeDataFileName)
    {
        const auto packedSchemaVersion = document.schemaVersion.major * 10000
                                       + document.schemaVersion.minor * 100
                                       + document.schemaVersion.patch;

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("manifestVersion", "2.0");
        root->setProperty("schemaVersion", packedSchemaVersion);
        root->setProperty("documentSchemaVersion", serializeSchemaVersionForManifest(document.schemaVersion));
        root->setProperty("componentClassName", componentClassName);
        root->setProperty("generatedAtUtc", juce::Time::getCurrentTime().toISO8601(true));
        root->setProperty("groupCount", static_cast<int>(document.groups.size()));
        root->setProperty("groupsFlattened", true);
        if (runtimeDataFileName.isNotEmpty())
            root->setProperty("runtimeDataFile", runtimeDataFileName);

        std::vector<const ExportWidgetEntry*> sortedWidgets;
        sortedWidgets.reserve(widgets.size());
        for (const auto& widget : widgets)
            sortedWidgets.push_back(&widget);
        std::sort(sortedWidgets.begin(),
                  sortedWidgets.end(),
                  [](const ExportWidgetEntry* lhs, const ExportWidgetEntry* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      if (lhs->model == nullptr || rhs->model == nullptr)
                          return lhs->model < rhs->model;
                      if (lhs->model->id != rhs->model->id)
                          return lhs->model->id < rhs->model->id;
                      return lhs->memberName < rhs->memberName;
                  });

        juce::Array<juce::var> widgetArray;
        for (const auto* widget : sortedWidgets)
        {
            if (widget == nullptr || widget->model == nullptr)
                continue;

            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("id", Gyeol::widgetIdToJsonString(widget->model->id));
            item->setProperty("typeKey", widget->typeKey);
            item->setProperty("exportTargetType", widget->exportTargetType);
            item->setProperty("codegenKind", widget->codegenKind);
            item->setProperty("memberName", widget->memberName);
            item->setProperty("supported", widget->supported);
            item->setProperty("usesCustomCodegen", widget->usesCustomCodegen);
            item->setProperty("bounds", serializeBoundsForManifest(widget->model->bounds));
            item->setProperty("properties", serializePropertiesForManifest(widget->model->properties));
            widgetArray.add(juce::var(item.release()));
        }
        root->setProperty("widgets", juce::var(widgetArray));

        std::vector<const CopiedAssetEntry*> sortedAssets;
        sortedAssets.reserve(assets.size());
        for (const auto& asset : assets)
            sortedAssets.push_back(&asset);
        std::sort(sortedAssets.begin(),
                  sortedAssets.end(),
                  [](const CopiedAssetEntry* lhs, const CopiedAssetEntry* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      if (lhs->assetId != rhs->assetId)
                          return lhs->assetId < rhs->assetId;
                      return lhs->refKey.compareIgnoreCase(rhs->refKey) < 0;
                  });

        juce::Array<juce::var> assetArray;
        for (const auto* asset : sortedAssets)
        {
            if (asset == nullptr)
                continue;

            auto item = std::make_unique<juce::DynamicObject>();
            item->setProperty("assetId", Gyeol::widgetIdToJsonString(asset->assetId));
            item->setProperty("refKey", asset->refKey);
            item->setProperty("kind", asset->kind);
            item->setProperty("mime", asset->mimeType);
            item->setProperty("sourcePath", asset->sourcePath);
            item->setProperty("destinationPath", asset->destinationRelativePath);
            item->setProperty("exportPath", asset->destinationRelativePath);
            item->setProperty("copied", asset->copied);
            item->setProperty("reused", asset->reused);
            assetArray.add(juce::var(item.release()));
        }
        root->setProperty("assets", juce::var(assetArray));
        root->setProperty("exportedAssets", juce::var(assetArray));

        // Legacy key retained for one release for tooling compatibility.
        root->setProperty("copiedResources", juce::var(assetArray));

        std::vector<const Gyeol::RuntimeParamModel*> sortedParams;
        sortedParams.reserve(document.runtimeParams.size());
        for (const auto& param : document.runtimeParams)
            sortedParams.push_back(&param);
        std::sort(sortedParams.begin(),
                  sortedParams.end(),
                  [](const Gyeol::RuntimeParamModel* lhs, const Gyeol::RuntimeParamModel* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      const auto lhsKey = lhs->key.toLowerCase();
                      const auto rhsKey = rhs->key.toLowerCase();
                      if (lhsKey != rhsKey)
                          return lhsKey < rhsKey;
                      return lhs->key < rhs->key;
                  });

        juce::Array<juce::var> runtimeParamArray;
        for (const auto* param : sortedParams)
        {
            if (param == nullptr)
                continue;
            runtimeParamArray.add(serializeRuntimeParamForManifest(*param));
        }
        root->setProperty("runtimeParams", juce::var(runtimeParamArray));

        std::vector<const Gyeol::PropertyBindingModel*> sortedPropertyBindings;
        sortedPropertyBindings.reserve(document.propertyBindings.size());
        for (const auto& binding : document.propertyBindings)
            sortedPropertyBindings.push_back(&binding);
        std::sort(sortedPropertyBindings.begin(),
                  sortedPropertyBindings.end(),
                  [](const Gyeol::PropertyBindingModel* lhs, const Gyeol::PropertyBindingModel* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      if (lhs->id != rhs->id)
                          return lhs->id < rhs->id;
                      return lhs->targetWidgetId < rhs->targetWidgetId;
                  });

        juce::Array<juce::var> propertyBindingArray;
        for (const auto* binding : sortedPropertyBindings)
        {
            if (binding == nullptr)
                continue;
            propertyBindingArray.add(serializePropertyBindingForManifest(*binding));
        }
        root->setProperty("propertyBindings", juce::var(propertyBindingArray));

        std::vector<const Gyeol::RuntimeBindingModel*> sortedRuntimeBindings;
        sortedRuntimeBindings.reserve(document.runtimeBindings.size());
        for (const auto& binding : document.runtimeBindings)
            sortedRuntimeBindings.push_back(&binding);
        std::sort(sortedRuntimeBindings.begin(),
                  sortedRuntimeBindings.end(),
                  [](const Gyeol::RuntimeBindingModel* lhs, const Gyeol::RuntimeBindingModel* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      if (lhs->id != rhs->id)
                          return lhs->id < rhs->id;
                      if (lhs->sourceWidgetId != rhs->sourceWidgetId)
                          return lhs->sourceWidgetId < rhs->sourceWidgetId;
                      return lhs->eventKey.compareIgnoreCase(rhs->eventKey) < 0;
                  });

        juce::Array<juce::var> runtimeBindingArray;
        for (const auto* binding : sortedRuntimeBindings)
        {
            if (binding == nullptr)
                continue;
            runtimeBindingArray.add(serializeRuntimeBindingForManifest(*binding));
        }
        root->setProperty("runtimeBindings", juce::var(runtimeBindingArray));

        return juce::JSON::toString(juce::var(root.release()), true);
    }

    juce::String buildRuntimeDataJson(const Gyeol::DocumentModel& document)
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("manifestVersion", "2.0");
        root->setProperty("documentSchemaVersion", serializeSchemaVersionForManifest(document.schemaVersion));

        std::vector<const Gyeol::RuntimeParamModel*> sortedParams;
        sortedParams.reserve(document.runtimeParams.size());
        for (const auto& param : document.runtimeParams)
            sortedParams.push_back(&param);
        std::sort(sortedParams.begin(),
                  sortedParams.end(),
                  [](const Gyeol::RuntimeParamModel* lhs, const Gyeol::RuntimeParamModel* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      const auto lhsKey = lhs->key.toLowerCase();
                      const auto rhsKey = rhs->key.toLowerCase();
                      if (lhsKey != rhsKey)
                          return lhsKey < rhsKey;
                      return lhs->key < rhs->key;
                  });

        juce::Array<juce::var> runtimeParamArray;
        for (const auto* param : sortedParams)
        {
            if (param == nullptr)
                continue;
            runtimeParamArray.add(serializeRuntimeParamForManifest(*param));
        }
        root->setProperty("runtimeParams", juce::var(runtimeParamArray));

        std::vector<const Gyeol::PropertyBindingModel*> sortedPropertyBindings;
        sortedPropertyBindings.reserve(document.propertyBindings.size());
        for (const auto& binding : document.propertyBindings)
            sortedPropertyBindings.push_back(&binding);
        std::sort(sortedPropertyBindings.begin(),
                  sortedPropertyBindings.end(),
                  [](const Gyeol::PropertyBindingModel* lhs, const Gyeol::PropertyBindingModel* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      if (lhs->id != rhs->id)
                          return lhs->id < rhs->id;
                      return lhs->targetWidgetId < rhs->targetWidgetId;
                  });

        juce::Array<juce::var> propertyBindingArray;
        for (const auto* binding : sortedPropertyBindings)
        {
            if (binding == nullptr)
                continue;
            propertyBindingArray.add(serializePropertyBindingForManifest(*binding));
        }
        root->setProperty("propertyBindings", juce::var(propertyBindingArray));

        std::vector<const Gyeol::RuntimeBindingModel*> sortedRuntimeBindings;
        sortedRuntimeBindings.reserve(document.runtimeBindings.size());
        for (const auto& binding : document.runtimeBindings)
            sortedRuntimeBindings.push_back(&binding);
        std::sort(sortedRuntimeBindings.begin(),
                  sortedRuntimeBindings.end(),
                  [](const Gyeol::RuntimeBindingModel* lhs, const Gyeol::RuntimeBindingModel* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs < rhs;
                      if (lhs->id != rhs->id)
                          return lhs->id < rhs->id;
                      if (lhs->sourceWidgetId != rhs->sourceWidgetId)
                          return lhs->sourceWidgetId < rhs->sourceWidgetId;
                      return lhs->eventKey.compareIgnoreCase(rhs->eventKey) < 0;
                  });

        juce::Array<juce::var> runtimeBindingArray;
        for (const auto* binding : sortedRuntimeBindings)
        {
            if (binding == nullptr)
                continue;
            runtimeBindingArray.add(serializeRuntimeBindingForManifest(*binding));
        }
        root->setProperty("runtimeBindings", juce::var(runtimeBindingArray));

        return juce::JSON::toString(juce::var(root.release()), true);
    }
}

namespace Gyeol::Export
{
    void ExportReport::addIssue(IssueSeverity severity, juce::String message)
    {
        if (severity == IssueSeverity::warning)
            ++warningCount;
        else if (severity == IssueSeverity::error)
            ++errorCount;

        issues.push_back(ExportIssue { severity, std::move(message) });
    }

    bool ExportReport::hasErrors() const noexcept
    {
        return errorCount > 0;
    }

    juce::String ExportReport::toText() const
    {
        juce::StringArray lines;
        lines.add("Gyeol Export Report");
        lines.add("===================");
        lines.add("Component Class: " + componentClassName);
        lines.add("Output Directory: " + outputDirectory.getFullPathName());
        lines.add("Generated Header: " + generatedHeaderFile.getFullPathName());
        lines.add("Generated Source: " + generatedSourceFile.getFullPathName());
        lines.add("Manifest File: " + manifestFile.getFullPathName());
        lines.add("Runtime Data File: " + runtimeDataFile.getFullPathName());
        lines.add("Widgets Exported: " + juce::String(exportedWidgetCount));
        lines.add("Assets Copied: " + juce::String(copiedResourceCount));
        lines.add("Assets Total: " + juce::String(totalAssetCount));
        lines.add("Assets Reused: " + juce::String(reusedAssetCount));
        lines.add("Assets Skipped: " + juce::String(skippedAssetCount));
        lines.add("Assets Missing: " + juce::String(missingAssetCount));
        lines.add("Assets Copy Failed: " + juce::String(failedAssetCount));
        lines.add("Warnings: " + juce::String(warningCount));
        lines.add("Errors: " + juce::String(errorCount));
        lines.add("");
        lines.add("Assets Summary:");
        lines.add("- success(copied): " + juce::String(copiedResourceCount));
        lines.add("- success(reused): " + juce::String(reusedAssetCount));
        lines.add("- skipped(metadata): " + juce::String(skippedAssetCount));
        lines.add("- missing: " + juce::String(missingAssetCount));
        lines.add("- failed(copy): " + juce::String(failedAssetCount));
        lines.add("");
        lines.add("Issues:");

        if (issues.empty())
        {
            lines.add("- INFO: no issues");
        }
        else
        {
            for (const auto& issue : issues)
                lines.add("- " + severityToString(issue.severity) + ": " + issue.message);
        }

        lines.add("");
        return lines.joinIntoString("\n");
    }

    juce::Result exportToJuceComponent(const DocumentModel& document,
                                       const Widgets::WidgetRegistry& registry,
                                       const ExportOptions& options,
                                       ExportReport& reportOut)
    {
        reportOut = {};

        const auto classNameInput = options.componentClassName.isNotEmpty()
                                        ? options.componentClassName
                                        : juce::String("GyeolExportedComponent");
        reportOut.componentClassName = sanitizeIdentifier(classNameInput);
        reportOut.outputDirectory = options.outputDirectory;

        auto fail = [&](const juce::String& message)
        {
            reportOut.addIssue(IssueSeverity::error, message);
            return juce::Result::fail(message);
        };

        const auto sceneCheck = Core::SceneValidator::validateScene(document, nullptr);
        if (sceneCheck.failed())
            return fail("Scene validation failed: " + sceneCheck.getErrorMessage());

        if (!document.groups.empty())
        {
            reportOut.addIssue(IssueSeverity::info,
                               "Group metadata is flattened during export (groupCount="
                               + juce::String(static_cast<int>(document.groups.size())) + ").");
        }

        const auto outputDirCheck = ensureDirectory(options.outputDirectory);
        if (outputDirCheck.failed())
            return fail(outputDirCheck.getErrorMessage());

        const auto assetsDirectory = options.outputDirectory.getChildFile("Assets");
        if (assetsDirectory.exists() && !assetsDirectory.deleteRecursively())
            return fail("Failed to clear export Assets directory: " + assetsDirectory.getFullPathName());

        const auto assetsDirCheck = ensureDirectory(assetsDirectory);
        if (assetsDirCheck.failed())
            return fail(assetsDirCheck.getErrorMessage());

        reportOut.generatedHeaderFile = options.outputDirectory.getChildFile(reportOut.componentClassName + ".h");
        reportOut.generatedSourceFile = options.outputDirectory.getChildFile(reportOut.componentClassName + ".cpp");
        reportOut.manifestFile = options.outputDirectory.getChildFile("export-manifest.json");
        reportOut.runtimeDataFile = options.outputDirectory.getChildFile("export-runtime.json");
        reportOut.reportFile = options.outputDirectory.getChildFile("ExportReport.txt");

        std::vector<ExportWidgetEntry> exportWidgets;
        exportWidgets.reserve(document.widgets.size());
        std::set<juce::String> usedMemberNames;

        for (const auto& widget : document.widgets)
        {
            ExportWidgetEntry entry;
            entry.model = &widget;
            entry.descriptor = registry.find(widget.type);

            if (entry.descriptor != nullptr)
            {
                entry.typeKey = entry.descriptor->typeKey.isNotEmpty()
                                    ? entry.descriptor->typeKey
                                    : juce::String("widget_") + juce::String(static_cast<int>(widget.type));
                entry.exportTargetType = entry.descriptor->exportTargetType.isNotEmpty()
                                             ? entry.descriptor->exportTargetType
                                             : entry.typeKey;
            }
            else
            {
                entry.typeKey = "unknown_" + juce::String(static_cast<int>(widget.type));
                entry.exportTargetType = "unsupported";
                reportOut.addIssue(IssueSeverity::warning,
                                   "Widget descriptor missing for widget id="
                                   + juce::String(widget.id)
                                   + " (type ordinal=" + juce::String(static_cast<int>(widget.type)) + ")");
            }

            entry.memberName = makeUniqueMemberName(entry.typeKey + "_" + juce::String(widget.id), usedMemberNames);

            bool customApplied = false;
            if (entry.descriptor != nullptr && static_cast<bool>(entry.descriptor->exportCodegen))
            {
                const auto customResult = applyCustomCodegen(*entry.descriptor, entry);
                if (customResult.wasOk())
                {
                    customApplied = true;
                }
                else
                {
                    reportOut.addIssue(IssueSeverity::warning,
                                       "Custom codegen failed for widget id=" + juce::String(widget.id)
                                       + " (" + entry.typeKey + "): "
                                       + customResult.getErrorMessage()
                                       + ". Falling back to built-in mapping.");
                }
            }

            if (!customApplied)
                applyBuiltinCodegen(entry);

            if (!entry.supported && entry.descriptor != nullptr)
            {
                reportOut.addIssue(IssueSeverity::warning,
                                   "Unsupported export target '" + entry.exportTargetType
                                   + "' for widget id=" + juce::String(widget.id)
                                   + ". Fallback Label will be generated.");
            }

            if (entry.resizedLines.isEmpty())
                entry.resizedLines.add(defaultResizedLine(entry));

            exportWidgets.push_back(std::move(entry));
        }

        std::vector<CopiedAssetEntry> copiedAssets;
        copiedAssets.reserve(document.assets.size());
        reportOut.totalAssetCount = static_cast<int>(document.assets.size());
        std::map<juce::String, juce::String> copiedBySourcePath;
        std::set<juce::String> preloadImagePathsSet;

        for (const auto& asset : document.assets)
        {
            CopiedAssetEntry copied;
            copied.assetId = asset.id;
            copied.refKey = asset.refKey;
            copied.kind = Gyeol::assetKindToKey(asset.kind);
            copied.mimeType = asset.mimeType;

            if (isAssetExcludedFromExport(asset))
            {
                ++reportOut.skippedAssetCount;
                copied.copied = false;
                copied.sourcePath = asset.relativePath;
                reportOut.addIssue(IssueSeverity::info,
                                   "Asset excluded from export by flag: refKey=" + asset.refKey);
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            if (asset.kind == AssetKind::colorPreset)
            {
                ++reportOut.skippedAssetCount;
                copied.copied = false;
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            if (asset.relativePath.trim().isEmpty())
            {
                ++reportOut.missingAssetCount;
                reportOut.addIssue(IssueSeverity::warning,
                                   "Asset path is empty for refKey=" + asset.refKey);
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            const auto source = resolveInputFilePath(asset.relativePath, options);
            auto sourceKey = source.getFullPathName().replaceCharacter('\\', '/');
            copied.sourcePath = sourceKey;

            if (!source.existsAsFile())
            {
                ++reportOut.missingAssetCount;
                reportOut.addIssue(IssueSeverity::warning,
                                   "Asset file not found: refKey=" + asset.refKey
                                   + ", path=" + asset.relativePath);
                copiedAssets.push_back(std::move(copied));
                continue;
            }

            juce::String destinationRelativePath;
            const auto existing = copiedBySourcePath.find(sourceKey);
            if (existing != copiedBySourcePath.end())
            {
                destinationRelativePath = existing->second;
                copied.reused = true;
                ++reportOut.reusedAssetCount;
            }
            else
            {
                const auto preferredRelativePath = buildPreferredExportAssetRelativePath(asset, source);
                const auto destination = makeUniqueDestinationPath(options.outputDirectory,
                                                                   preferredRelativePath);
                const auto destinationParent = destination.getParentDirectory();
                if (!destinationParent.exists() && !destinationParent.createDirectory())
                {
                    ++reportOut.failedAssetCount;
                    reportOut.addIssue(IssueSeverity::warning,
                                       "Failed to create asset folder: " + destinationParent.getFullPathName());
                    copiedAssets.push_back(std::move(copied));
                    continue;
                }

                if (!source.copyFileTo(destination))
                {
                    ++reportOut.failedAssetCount;
                    reportOut.addIssue(IssueSeverity::warning,
                                       "Failed to copy asset file: " + source.getFullPathName());
                    copiedAssets.push_back(std::move(copied));
                    continue;
                }

                destinationRelativePath = relativePathOrAbsolute(destination, options.outputDirectory);
                copiedBySourcePath.emplace(sourceKey, destinationRelativePath);
                ++reportOut.copiedResourceCount;
            }

            copied.destinationRelativePath = destinationRelativePath;
            copied.copied = true;

            if (copied.destinationRelativePath.isNotEmpty()
                && !copied.destinationRelativePath.startsWithIgnoreCase("Assets/"))
            {
                reportOut.addIssue(IssueSeverity::warning,
                                   "Exported asset path is outside Assets/: refKey=" + copied.refKey
                                     + ", path=" + copied.destinationRelativePath);
            }

            copiedAssets.push_back(copied);

            if (asset.kind == AssetKind::image && copied.destinationRelativePath.isNotEmpty())
                preloadImagePathsSet.insert(copied.destinationRelativePath);
        }

        std::vector<juce::String> preloadImagePaths(preloadImagePathsSet.begin(), preloadImagePathsSet.end());

        const auto runtimeData = buildRuntimeDataJson(document);
        const auto headerCode = generateHeaderCode(reportOut.componentClassName, exportWidgets);
        const auto sourceCode = generateSourceCode(reportOut.componentClassName,
                                                   exportWidgets,
                                                   preloadImagePaths,
                                                   runtimeData);

        const auto headerWrite = writeTextFile(reportOut.generatedHeaderFile, headerCode, options.overwriteExistingFiles);
        if (headerWrite.failed())
            return fail(headerWrite.getErrorMessage());

        const auto sourceWrite = writeTextFile(reportOut.generatedSourceFile, sourceCode, options.overwriteExistingFiles);
        if (sourceWrite.failed())
            return fail(sourceWrite.getErrorMessage());

        if (options.writeRuntimeDataJson)
        {
            const auto runtimeDataWrite = writeTextFile(reportOut.runtimeDataFile,
                                                        runtimeData,
                                                        options.overwriteExistingFiles);
            if (runtimeDataWrite.failed())
                return fail(runtimeDataWrite.getErrorMessage());
        }

        if (options.writeManifestJson)
        {
            const auto runtimeDataFileName = options.writeRuntimeDataJson
                                                 ? reportOut.runtimeDataFile.getFileName()
                                                 : juce::String();
            const auto manifest = buildManifestJson(document,
                                                    reportOut.componentClassName,
                                                    exportWidgets,
                                                    copiedAssets,
                                                    runtimeDataFileName);

            const auto manifestWrite = writeTextFile(reportOut.manifestFile, manifest, options.overwriteExistingFiles);
            if (manifestWrite.failed())
                return fail(manifestWrite.getErrorMessage());
        }

        reportOut.exportedWidgetCount = static_cast<int>(exportWidgets.size());

        const auto reportWrite = writeTextFile(reportOut.reportFile, reportOut.toText(), true);
        if (reportWrite.failed())
            return fail(reportWrite.getErrorMessage());

        if (reportOut.hasErrors())
            return juce::Result::fail("Export failed. See report: " + reportOut.reportFile.getFullPathName());

        return juce::Result::ok();
    }
}
