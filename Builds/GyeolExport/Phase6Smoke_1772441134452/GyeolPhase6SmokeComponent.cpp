#include "GyeolPhase6SmokeComponent.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>


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


void GyeolPhase6SmokeComponent::initializeRuntimeBridge()
{
    runtimeBridgeLoaded = false;
    lastRuntimeBridgeError.clear();
    runtimeParams.clear();
    runtimeParamTypes.clear();
    propertyBindings.clear();
    runtimeBindings.clear();
    runtimeWidgetById.clear();
    runtimeButtonDownStates.clear();

    runtimeWidgetById.emplace(1001, &button_1001);
    runtimeWidgetById.emplace(1002, &slider_1002);
    runtimeWidgetById.emplace(1003, &label_1003);

    runtimeButtonDownStates[1001] = false;
    button_1001.onClick = [this]()
    {
        if (runtimeBridgeMutating)
            return;
        dispatchRuntimeEvent(1001, "onClick", true);
    };
    button_1001.onStateChange = [this]()
    {
        if (runtimeBridgeMutating)
            return;
        const auto isDown = button_1001.isDown();
        const auto previous = runtimeButtonDownStates[1001];
        if (isDown == previous)
            return;
        runtimeButtonDownStates[1001] = isDown;
        dispatchRuntimeEvent(1001, isDown ? "onPress" : "onRelease", isDown);
    };
    slider_1002.onValueChange = [this]()
    {
        if (runtimeBridgeMutating)
            return;
        dispatchRuntimeEvent(1002, "onValueChanged", slider_1002.getValue());
    };
    slider_1002.onDragEnd = [this]()
    {
        if (runtimeBridgeMutating)
            return;
        dispatchRuntimeEvent(1002, "onValueCommit", slider_1002.getValue());
    };

    auto runtimeDataText = juce::String();
    const auto runtimeDataFile = resolveExportAssetFile("export-runtime.json");
    if (runtimeDataFile.existsAsFile())
        runtimeDataText = runtimeDataFile.loadFileAsString();

    if (runtimeDataText.trim().isEmpty())
        runtimeDataText = "{\"manifestVersion\": \"2.0\", \"documentSchemaVersion\": {\"major\": 0, \"minor\": 6, \"patch\": 0, \"packed\": 600}, \"runtimeParams\": [{\"key\": \"A\", \"type\": \"number\", \"defaultValue\": 0.25, \"description\": \"Smoke number\", \"exposed\": true}, {\"key\": \"B\", \"type\": \"boolean\", \"defaultValue\": false, \"description\": \"Smoke toggle\", \"exposed\": true}], \"propertyBindings\": [{\"id\": \"2001\", \"name\": \"Slider from A\", \"enabled\": true, \"targetWidgetId\": \"1002\", \"targetProperty\": \"value\", \"expression\": \"A\"}, {\"id\": \"2002\", \"name\": \"Slider min from A\", \"enabled\": true, \"targetWidgetId\": \"1002\", \"targetProperty\": \"minValue\", \"expression\": \"A * 0.5\"}], \"runtimeBindings\": [{\"id\": \"3001\", \"name\": \"Button click -> update A and label\", \"enabled\": true, \"sourceWidgetId\": \"1001\", \"eventKey\": \"onClick\", \"actions\": [{\"kind\": \"setRuntimeParam\", \"paramKey\": \"A\", \"value\": 0.9}, {\"kind\": \"setNodeProps\", \"targetKind\": \"widget\", \"targetId\": \"1003\", \"patch\": {\"text\": \"Clicked\"}}, {\"kind\": \"setNodeBounds\", \"targetWidgetId\": \"1003\", \"bounds\": {\"x\": 250.0, \"y\": 70.0, \"w\": 180.0, \"h\": 28.0}}]}, {\"id\": \"3002\", \"name\": \"Slider commit -> set A from payload\", \"enabled\": true, \"sourceWidgetId\": \"1002\", \"eventKey\": \"onValueCommit\", \"actions\": [{\"kind\": \"setRuntimeParam\", \"paramKey\": \"A\", \"value\": null}]}]}";

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

void GyeolPhase6SmokeComponent::dispatchRuntimeEvent(juce::int64 sourceWidgetId,
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

void GyeolPhase6SmokeComponent::applyPropertyBindings()
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

bool GyeolPhase6SmokeComponent::applyRuntimeAction(const juce::var& action,
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

juce::Component* GyeolPhase6SmokeComponent::findRuntimeWidget(juce::int64 widgetId) const
{
    if (const auto it = runtimeWidgetById.find(widgetId); it != runtimeWidgetById.end())
        return it->second;
    return nullptr;
}

bool GyeolPhase6SmokeComponent::setWidgetPropertyById(juce::int64 widgetId,
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



GyeolPhase6SmokeComponent::GyeolPhase6SmokeComponent()
{
    // Widget id=1001, type=button, target=juce::TextButton, codegen=juce_text_button
    button_1001.setButtonText("Apply");
    addAndMakeVisible(button_1001);

    // Widget id=1002, type=slider, target=juce::Slider::LinearHorizontal, codegen=juce_slider_dynamic
    slider_1002.setSliderStyle(juce::Slider::LinearHorizontal);
    slider_1002.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider_1002.setRange(0.00000000, 1.00000000, 0.00000000);
    slider_1002.setValue(0.25000000, juce::dontSendNotification);
    addAndMakeVisible(slider_1002);

    // Widget id=1003, type=label, target=juce::Label, codegen=juce_label
    label_1003.setText("Idle", juce::dontSendNotification);
    label_1003.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label_1003);

    initializeRuntimeBridge();
}

void GyeolPhase6SmokeComponent::resized()
{
    button_1001.setBounds(24, 20, 110, 32);
    slider_1002.setBounds(160, 20, 220, 32);
    label_1003.setBounds(24, 70, 220, 28);
}
