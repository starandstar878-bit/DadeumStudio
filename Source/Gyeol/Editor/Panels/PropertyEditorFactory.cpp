#include "Gyeol/Editor/Panels/PropertyEditorFactory.h"

#include <cerrno>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        constexpr double kNumberEpsilon = 0.0000001;

        bool parseStrictDouble(const juce::String& text, double& outValue)
        {
            const auto trimmed = text.trim();
            if (trimmed.isEmpty())
                return false;

            auto utf8 = trimmed.toRawUTF8();
            char* end = nullptr;
            const auto parsed = std::strtod(utf8, &end);
            if (end == utf8 || *end != '\0' || !std::isfinite(parsed))
                return false;

            outValue = parsed;
            return true;
        }

        bool parseStrictInt64(const juce::String& text, int64_t& outValue)
        {
            const auto trimmed = text.trim();
            if (trimmed.isEmpty())
                return false;

            auto utf8 = trimmed.toRawUTF8();
            char* end = nullptr;
            errno = 0;
            const auto parsed = std::strtoll(utf8, &end, 10);
            if (end == utf8 || *end != '\0')
                return false;
            if (errno == ERANGE)
                return false;

            outValue = static_cast<int64_t>(parsed);
            return true;
        }

        juce::var makeVec2Var(double x, double y)
        {
            auto* object = new juce::DynamicObject();
            object->setProperty("x", x);
            object->setProperty("y", y);
            return juce::var(object);
        }

        juce::var makeRectVar(double x, double y, double w, double h)
        {
            auto* object = new juce::DynamicObject();
            object->setProperty("x", x);
            object->setProperty("y", y);
            object->setProperty("w", w);
            object->setProperty("h", h);
            return juce::var(object);
        }

        bool parseCsvDoubles(const juce::String& text, std::vector<double>& outValues, int expectedCount)
        {
            outValues.clear();

            auto normalized = text.trim();
            normalized = normalized.replaceCharacter(';', ',');
            normalized = normalized.replaceCharacter('/', ',');
            normalized = normalized.replaceCharacter('\t', ',');
            while (normalized.contains(" "))
                normalized = normalized.replace(" ", ",");

            juce::StringArray tokens;
            tokens.addTokens(normalized, ",", "\"");
            tokens.removeEmptyStrings();
            if (tokens.size() != expectedCount)
                return false;

            outValues.reserve(static_cast<size_t>(tokens.size()));
            for (const auto& token : tokens)
            {
                double parsed = 0.0;
                if (!parseStrictDouble(token, parsed))
                    return false;
                outValues.push_back(parsed);
            }

            return true;
        }

        juce::String toLowerTrimmed(juce::String text)
        {
            text = text.trim();
            text = text.toLowerCase();
            return text;
        }

        bool parseFiniteNumericVar(const juce::var& input, double& outValue)
        {
            if (!isNumericVar(input))
                return false;

            const auto parsed = static_cast<double>(input);
            if (!std::isfinite(parsed))
                return false;

            outValue = parsed;
            return true;
        }

        bool validateNumericRange(const Widgets::WidgetPropertySpec& spec, double value)
        {
            if (spec.minValue.has_value() && value < *spec.minValue)
                return false;
            if (spec.maxValue.has_value() && value > *spec.maxValue)
                return false;
            return true;
        }

        bool parseBooleanVar(const juce::var& input, bool& outValue)
        {
            if (input.isBool())
            {
                outValue = static_cast<bool>(input);
                return true;
            }

            double numeric = 0.0;
            if (parseFiniteNumericVar(input, numeric))
            {
                if (std::abs(numeric) <= kNumberEpsilon)
                {
                    outValue = false;
                    return true;
                }
                if (std::abs(numeric - 1.0) <= kNumberEpsilon)
                {
                    outValue = true;
                    return true;
                }
            }

            if (input.isString())
            {
                const auto lower = toLowerTrimmed(input.toString());
                if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
                {
                    outValue = true;
                    return true;
                }
                if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
                {
                    outValue = false;
                    return true;
                }
            }

            return false;
        }

        bool readObjectNumber(const juce::var& input, const juce::Identifier& key, double& outValue)
        {
            const auto* object = input.getDynamicObject();
            if (object == nullptr)
                return false;
            if (!object->getProperties().contains(key))
                return false;
            return parseFiniteNumericVar(object->getProperty(key), outValue);
        }

        juce::var makeRgbaVar(double r, double g, double b, double a)
        {
            auto* object = new juce::DynamicObject();
            object->setProperty("r", r);
            object->setProperty("g", g);
            object->setProperty("b", b);
            object->setProperty("a", a);
            return juce::var(object);
        }

        juce::var makeHslaVar(double h, double s, double l, double a)
        {
            auto* object = new juce::DynamicObject();
            object->setProperty("h", h);
            object->setProperty("s", s);
            object->setProperty("l", l);
            object->setProperty("a", a);
            return juce::var(object);
        }

        struct AssetDropPayload
        {
            juce::String refKey;
            juce::String mime;
            std::optional<AssetKind> assetKind;
        };

        std::optional<AssetDropPayload> extractAssetDropPayloadFromDragDescription(const juce::var& description)
        {
            const auto* object = description.getDynamicObject();
            if (object == nullptr)
                return std::nullopt;

            const auto& props = object->getProperties();
            if (!props.contains("kind") || props["kind"].toString() != "assetRef")
                return std::nullopt;

            const auto refKey = props.getWithDefault("refKey", juce::var()).toString().trim();
            if (refKey.isEmpty())
                return std::nullopt;

            AssetDropPayload payload;
            payload.refKey = refKey;
            payload.mime = props.getWithDefault("mime", juce::var()).toString().trim();
            if (const auto parsedKind = assetKindFromKey(props.getWithDefault("assetKind", juce::var()).toString().trim());
                parsedKind.has_value())
            {
                payload.assetKind = *parsedKind;
            }

            return payload;
        }

        std::optional<AssetKind> inferAssetKindFromDropPayload(const AssetDropPayload& payload)
        {
            if (payload.assetKind.has_value())
                return payload.assetKind;

            const auto mime = payload.mime.trim().toLowerCase();
            if (mime.startsWith("image/"))
                return AssetKind::image;
            if (mime.startsWith("font/"))
                return AssetKind::font;
            if (mime == "application/x-color-preset")
                return AssetKind::colorPreset;

            return std::nullopt;
        }

        bool isAssetDropPayloadAccepted(const Widgets::WidgetPropertySpec& spec, const AssetDropPayload& payload)
        {
            if (spec.kind != Widgets::WidgetPropertyKind::assetRef)
                return false;

            if (spec.acceptedAssetKinds.empty())
                return true;

            const auto inferredKind = inferAssetKindFromDropPayload(payload);
            if (!inferredKind.has_value())
                return false;

            return Widgets::isAssetKindAccepted(spec, *inferredKind);
        }

        class AssetDropComboBox final : public juce::ComboBox,
                                        public juce::DragAndDropTarget
        {
        public:
            std::function<void(const juce::String&)> onAssetDropped;
            std::function<bool(const AssetDropPayload&)> isDropAllowed;

            bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override
            {
                return extractAssetDropPayloadFromDragDescription(dragSourceDetails.description).has_value();
            }

            void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override
            {
                updateDragFeedback(dragSourceDetails);
            }

            void itemDragMove(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override
            {
                updateDragFeedback(dragSourceDetails);
            }

            void itemDragExit(const juce::DragAndDropTarget::SourceDetails&) override
            {
                dragHovering = false;
                dragAllowed = false;
                repaint();
            }

            void itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override
            {
                const auto payload = extractAssetDropPayloadFromDragDescription(dragSourceDetails.description);
                const auto accepted = payload.has_value()
                                   && (isDropAllowed == nullptr || isDropAllowed(*payload));

                dragHovering = false;
                dragAllowed = false;
                repaint();

                if (!accepted || !payload.has_value() || onAssetDropped == nullptr)
                    return;

                onAssetDropped(payload->refKey);
            }

            void paintOverChildren(juce::Graphics& g) override
            {
                if (!dragHovering)
                    return;

                const auto accent = dragAllowed ? juce::Colour::fromRGB(86, 156, 255)
                                                : juce::Colour::fromRGB(226, 92, 92);
                const auto bounds = getLocalBounds().toFloat().reduced(0.5f);

                g.setColour(accent.withAlpha(0.20f));
                g.fillRoundedRectangle(bounds, 4.0f);
                g.setColour(accent.withAlpha(0.95f));
                g.drawRoundedRectangle(bounds, 4.0f, 1.4f);
            }

        private:
            void updateDragFeedback(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
            {
                const auto payload = extractAssetDropPayloadFromDragDescription(dragSourceDetails.description);
                dragHovering = payload.has_value();
                dragAllowed = dragHovering && (isDropAllowed == nullptr || isDropAllowed(*payload));
                repaint();
            }

            bool dragHovering = false;
            bool dragAllowed = false;
        };

        std::unique_ptr<juce::Component> createTextEditor(const EditorBuildSpec& buildSpec,
                                                          const juce::String& initialText,
                                                          const juce::String& placeholder)
        {
            auto editor = std::make_unique<juce::TextEditor>();
            const auto isMultiline = buildSpec.spec.uiHint == Widgets::WidgetPropertyUiHint::multiLine;
            editor->setMultiLine(isMultiline, isMultiline);
            editor->setReturnKeyStartsNewLine(isMultiline);

            auto safeInitialText = initialText;
            if (!isMultiline)
            {
                safeInitialText = safeInitialText.replace("\r\n", " ");
                safeInitialText = safeInitialText.replace("\n", " ");
                safeInitialText = safeInitialText.replace("\r", " ");
            }

            editor->setText(safeInitialText, false);
            editor->setTextToShowWhenEmpty(placeholder, juce::Colour::fromRGB(128, 136, 148));
            editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(27, 33, 42));
            editor->setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(62, 74, 92));
            editor->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGB(78, 156, 255));
            editor->setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(222, 228, 236));
            editor->setReadOnly(buildSpec.readOnly || buildSpec.spec.readOnly);

            if (buildSpec.readOnly || buildSpec.spec.readOnly)
                return editor;

            struct EditSessionState
            {
                bool suppressNextFocusLostCommit = false;
            };
            auto state = std::make_shared<EditSessionState>();

            const auto invokePreview = [buildSpec](const juce::String& text)
            {
                juce::var parsed;
                if (!PropertyEditorFactory::parseValue(buildSpec.spec, text, parsed))
                    return;

                if (buildSpec.onPreview)
                    buildSpec.onPreview(parsed);
            };

            const auto invokeCommit = [buildSpec](const juce::String& text)
            {
                juce::var parsed;
                if (PropertyEditorFactory::parseValue(buildSpec.spec, text, parsed))
                {
                    if (buildSpec.onCommit)
                        buildSpec.onCommit(parsed);
                }
                else if (buildSpec.onCancel)
                {
                    buildSpec.onCancel();
                }
            };

            editor->onTextChange = [safe = juce::Component::SafePointer<juce::TextEditor>(editor.get()), invokePreview, state]
            {
                if (safe == nullptr)
                    return;
                state->suppressNextFocusLostCommit = false;
                invokePreview(safe->getText());
            };

            editor->onReturnKey = [safe = juce::Component::SafePointer<juce::TextEditor>(editor.get()), invokeCommit, state]
            {
                if (safe == nullptr)
                    return;
                if (safe->isMultiLine())
                    return;
                state->suppressNextFocusLostCommit = true;
                invokeCommit(safe->getText());
            };

            editor->onEscapeKey = [buildSpec, state]
            {
                state->suppressNextFocusLostCommit = true;
                if (buildSpec.onCancel)
                    buildSpec.onCancel();
            };

            editor->onFocusLost = [safe = juce::Component::SafePointer<juce::TextEditor>(editor.get()), invokeCommit, state]
            {
                if (safe == nullptr)
                    return;
                if (state->suppressNextFocusLostCommit)
                {
                    state->suppressNextFocusLostCommit = false;
                    return;
                }
                invokeCommit(safe->getText());
            };

            return editor;
        }
    }

    juce::String PropertyEditorFactory::formatValue(const Widgets::WidgetPropertySpec& spec, const juce::var& value)
    {
        switch (spec.kind)
        {
            case Widgets::WidgetPropertyKind::text:
            case Widgets::WidgetPropertyKind::assetRef:
            case Widgets::WidgetPropertyKind::enumChoice:
            case Widgets::WidgetPropertyKind::color:
                return value.toString();

            case Widgets::WidgetPropertyKind::integer:
                return juce::String(static_cast<int64_t>(value));

            case Widgets::WidgetPropertyKind::number:
            {
                if (!isNumericVar(value))
                    return {};
                const auto decimals = juce::jlimit(0, 8, spec.decimals);
                return juce::String(static_cast<double>(value), decimals);
            }

            case Widgets::WidgetPropertyKind::boolean:
                return static_cast<bool>(value) ? "true" : "false";

            case Widgets::WidgetPropertyKind::vec2:
            {
                const auto* object = value.getDynamicObject();
                if (object == nullptr)
                    return value.toString();
                return juce::String(static_cast<double>(object->getProperty("x")), 4)
                    + ", "
                    + juce::String(static_cast<double>(object->getProperty("y")), 4);
            }

            case Widgets::WidgetPropertyKind::rect:
            {
                const auto* object = value.getDynamicObject();
                if (object == nullptr)
                    return value.toString();
                return juce::String(static_cast<double>(object->getProperty("x")), 4)
                    + ", "
                    + juce::String(static_cast<double>(object->getProperty("y")), 4)
                    + ", "
                    + juce::String(static_cast<double>(object->getProperty("w")), 4)
                    + ", "
                    + juce::String(static_cast<double>(object->getProperty("h")), 4);
            }
        }

        return value.toString();
    }

    bool PropertyEditorFactory::parseValue(const Widgets::WidgetPropertySpec& spec,
                                           const juce::String& text,
                                           juce::var& valueOut)
    {
        const auto trimmed = text.trim();

        switch (spec.kind)
        {
            case Widgets::WidgetPropertyKind::text:
            case Widgets::WidgetPropertyKind::assetRef:
                valueOut = trimmed;
                return true;

            case Widgets::WidgetPropertyKind::integer:
            {
                int64_t parsed = 0;
                if (!parseStrictInt64(trimmed, parsed))
                    return false;

                if (spec.minValue.has_value() && static_cast<double>(parsed) < *spec.minValue)
                    return false;
                if (spec.maxValue.has_value() && static_cast<double>(parsed) > *spec.maxValue)
                    return false;

                valueOut = parsed;
                return true;
            }

            case Widgets::WidgetPropertyKind::number:
            {
                double parsed = 0.0;
                if (!parseStrictDouble(trimmed, parsed))
                    return false;

                if (spec.minValue.has_value() && parsed < *spec.minValue)
                    return false;
                if (spec.maxValue.has_value() && parsed > *spec.maxValue)
                    return false;

                valueOut = parsed;
                return true;
            }

            case Widgets::WidgetPropertyKind::boolean:
            {
                const auto lower = toLowerTrimmed(trimmed);
                if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
                {
                    valueOut = true;
                    return true;
                }
                if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
                {
                    valueOut = false;
                    return true;
                }
                return false;
            }

            case Widgets::WidgetPropertyKind::enumChoice:
            {
                const auto it = std::find_if(spec.enumOptions.begin(),
                                             spec.enumOptions.end(),
                                             [&trimmed](const Widgets::WidgetEnumOption& option)
                                             {
                                                 return option.value == trimmed || option.label == trimmed;
                                             });
                if (it == spec.enumOptions.end())
                    return false;
                valueOut = it->value;
                return true;
            }

            case Widgets::WidgetPropertyKind::color:
            {
                if (spec.colorStorage == Widgets::ColorStorage::argbInt)
                {
                    int64_t parsed = 0;
                    if (!parseStrictInt64(trimmed, parsed))
                        return false;
                    valueOut = parsed;
                    return true;
                }

                if (trimmed.isEmpty())
                    return false;
                valueOut = trimmed;
                return true;
            }

            case Widgets::WidgetPropertyKind::vec2:
            {
                std::vector<double> values;
                if (!parseCsvDoubles(trimmed, values, 2))
                    return false;
                valueOut = makeVec2Var(values[0], values[1]);
                return true;
            }

            case Widgets::WidgetPropertyKind::rect:
            {
                std::vector<double> values;
                if (!parseCsvDoubles(trimmed, values, 4))
                    return false;
                valueOut = makeRectVar(values[0], values[1], values[2], values[3]);
                return true;
            }
        }

        return false;
    }

    bool PropertyEditorFactory::normalizeValue(const Widgets::WidgetPropertySpec& spec,
                                               const juce::var& input,
                                               juce::var& valueOut)
    {
        if (input.isString())
            return parseValue(spec, input.toString(), valueOut);

        switch (spec.kind)
        {
            case Widgets::WidgetPropertyKind::text:
            case Widgets::WidgetPropertyKind::assetRef:
                valueOut = input.toString().trim();
                return true;

            case Widgets::WidgetPropertyKind::integer:
            {
                double parsed = 0.0;
                if (!parseFiniteNumericVar(input, parsed))
                    return false;

                const auto rounded = std::round(parsed);
                if (std::abs(parsed - rounded) > kNumberEpsilon)
                    return false;
                if (rounded < static_cast<double>(std::numeric_limits<int64_t>::lowest())
                    || rounded > static_cast<double>(std::numeric_limits<int64_t>::max()))
                {
                    return false;
                }
                if (!validateNumericRange(spec, rounded))
                    return false;

                valueOut = static_cast<int64_t>(rounded);
                return true;
            }

            case Widgets::WidgetPropertyKind::number:
            {
                double parsed = 0.0;
                if (!parseFiniteNumericVar(input, parsed))
                    return false;
                if (!validateNumericRange(spec, parsed))
                    return false;
                valueOut = parsed;
                return true;
            }

            case Widgets::WidgetPropertyKind::boolean:
            {
                bool parsed = false;
                if (!parseBooleanVar(input, parsed))
                    return false;
                valueOut = parsed;
                return true;
            }

            case Widgets::WidgetPropertyKind::enumChoice:
            {
                const auto asText = input.toString().trim();
                const auto it = std::find_if(spec.enumOptions.begin(),
                                             spec.enumOptions.end(),
                                             [&asText](const Widgets::WidgetEnumOption& option)
                                             {
                                                 return option.value == asText || option.label == asText;
                                             });
                if (it == spec.enumOptions.end())
                    return false;
                valueOut = it->value;
                return true;
            }

            case Widgets::WidgetPropertyKind::color:
            {
                if (spec.colorStorage == Widgets::ColorStorage::argbInt)
                {
                    if (input.isInt() || input.isInt64())
                    {
                        valueOut = static_cast<int64_t>(input);
                        return true;
                    }

                    double parsed = 0.0;
                    if (!parseFiniteNumericVar(input, parsed))
                        return false;
                    const auto rounded = std::round(parsed);
                    if (std::abs(parsed - rounded) > kNumberEpsilon)
                        return false;
                    if (rounded < static_cast<double>(std::numeric_limits<int64_t>::lowest())
                        || rounded > static_cast<double>(std::numeric_limits<int64_t>::max()))
                    {
                        return false;
                    }
                    valueOut = static_cast<int64_t>(rounded);
                    return true;
                }

                if (spec.colorStorage == Widgets::ColorStorage::rgbaObject255
                    || spec.colorStorage == Widgets::ColorStorage::rgbaObject01)
                {
                    double r = 0.0;
                    double g = 0.0;
                    double b = 0.0;
                    double a = spec.colorStorage == Widgets::ColorStorage::rgbaObject255 ? 255.0 : 1.0;
                    if (!readObjectNumber(input, "r", r)
                        || !readObjectNumber(input, "g", g)
                        || !readObjectNumber(input, "b", b))
                    {
                        return false;
                    }

                    const auto hasAlpha = readObjectNumber(input, "a", a);
                    juce::ignoreUnused(hasAlpha);
                    const auto min = 0.0;
                    const auto max = spec.colorStorage == Widgets::ColorStorage::rgbaObject255 ? 255.0 : 1.0;
                    if (r < min || g < min || b < min || a < min || r > max || g > max || b > max || a > max)
                        return false;

                    valueOut = makeRgbaVar(r, g, b, a);
                    return true;
                }

                if (spec.colorStorage == Widgets::ColorStorage::hslaObject)
                {
                    double h = 0.0;
                    double s = 0.0;
                    double l = 0.0;
                    double a = 1.0;
                    if (!readObjectNumber(input, "h", h)
                        || !readObjectNumber(input, "s", s)
                        || !readObjectNumber(input, "l", l))
                    {
                        return false;
                    }

                    const auto hasAlpha = readObjectNumber(input, "a", a);
                    juce::ignoreUnused(hasAlpha);
                    if (h < 0.0 || h > 360.0 || s < 0.0 || s > 1.0 || l < 0.0 || l > 1.0 || a < 0.0 || a > 1.0)
                        return false;

                    valueOut = makeHslaVar(h, s, l, a);
                    return true;
                }

                const auto asText = input.toString().trim();
                if (asText.isEmpty())
                    return false;
                valueOut = asText;
                return true;
            }

            case Widgets::WidgetPropertyKind::vec2:
            {
                double x = 0.0;
                double y = 0.0;
                if (!readObjectNumber(input, "x", x) || !readObjectNumber(input, "y", y))
                    return false;
                valueOut = makeVec2Var(x, y);
                return true;
            }

            case Widgets::WidgetPropertyKind::rect:
            {
                double x = 0.0;
                double y = 0.0;
                double w = 0.0;
                double h = 0.0;
                if (!readObjectNumber(input, "x", x)
                    || !readObjectNumber(input, "y", y)
                    || !readObjectNumber(input, "w", w)
                    || !readObjectNumber(input, "h", h))
                {
                    return false;
                }
                if (w < 0.0 || h < 0.0)
                    return false;
                valueOut = makeRectVar(x, y, w, h);
                return true;
            }
        }

        return false;
    }

    std::unique_ptr<juce::Component> PropertyEditorFactory::createEditor(const EditorBuildSpec& buildSpec)
    {
        const auto readOnly = buildSpec.readOnly || buildSpec.spec.readOnly;

        if (buildSpec.spec.kind == Widgets::WidgetPropertyKind::boolean)
        {
            auto editor = std::make_unique<juce::ToggleButton>();
            editor->setToggleState(buildSpec.mixed ? false : static_cast<bool>(buildSpec.value), juce::dontSendNotification);
            editor->setButtonText(buildSpec.mixed ? juce::String("Mixed") : juce::String());
            editor->setEnabled(!readOnly);

            if (!readOnly)
            {
                editor->onClick = [buildSpec, safe = juce::Component::SafePointer<juce::ToggleButton>(editor.get())]
                {
                    if (safe == nullptr)
                        return;
                    juce::var normalized;
                    if (!PropertyEditorFactory::normalizeValue(buildSpec.spec, juce::var(safe->getToggleState()), normalized))
                        return;
                    if (buildSpec.onPreview)
                        buildSpec.onPreview(normalized);
                    if (buildSpec.onCommit)
                        buildSpec.onCommit(normalized);
                };
            }

            return editor;
        }

        const auto shouldUseCombo = (buildSpec.spec.kind == Widgets::WidgetPropertyKind::enumChoice
                                     || (buildSpec.spec.kind == Widgets::WidgetPropertyKind::assetRef
                                         && (buildSpec.spec.uiHint == Widgets::WidgetPropertyUiHint::dropdown
                                             || buildSpec.spec.uiHint == Widgets::WidgetPropertyUiHint::assetPicker)))
                                 && !buildSpec.spec.enumOptions.empty();

        if (shouldUseCombo)
        {
            std::unique_ptr<juce::ComboBox> editorBase;
            if (buildSpec.spec.kind == Widgets::WidgetPropertyKind::assetRef)
                editorBase = std::make_unique<AssetDropComboBox>();
            else
                editorBase = std::make_unique<juce::ComboBox>();

            auto* editor = editorBase.get();
            editor->setEnabled(!readOnly);
            editor->setEditableText(buildSpec.spec.kind == Widgets::WidgetPropertyKind::assetRef);
            int itemId = 1;
            for (const auto& option : buildSpec.spec.enumOptions)
                editor->addItem(option.label.isNotEmpty() ? option.label : option.value, itemId++);

            if (!buildSpec.mixed)
            {
                const auto asText = buildSpec.value.toString();
                bool matched = false;
                itemId = 1;
                for (const auto& option : buildSpec.spec.enumOptions)
                {
                    if (option.value == asText || option.label == asText)
                    {
                        editor->setSelectedId(itemId, juce::dontSendNotification);
                        matched = true;
                        break;
                    }
                    ++itemId;
                }

                if (!matched && buildSpec.spec.kind == Widgets::WidgetPropertyKind::assetRef)
                    editor->setText(asText, juce::dontSendNotification);
            }
            else
            {
                editor->setTextWhenNothingSelected("--");
            }

            if (!readOnly)
            {
                editor->onChange = [buildSpec, safe = juce::Component::SafePointer<juce::ComboBox>(editor)]
                {
                    if (safe == nullptr)
                        return;

                    const auto index = safe->getSelectedItemIndex();
                    juce::String selectedText;

                    if (index >= 0 && index < static_cast<int>(buildSpec.spec.enumOptions.size()))
                    {
                        selectedText = buildSpec.spec.enumOptions[static_cast<size_t>(index)].value;
                    }
                    else if (buildSpec.spec.kind == Widgets::WidgetPropertyKind::assetRef)
                    {
                        selectedText = safe->getText().trim();
                    }

                    if (selectedText.isEmpty() && buildSpec.spec.kind != Widgets::WidgetPropertyKind::assetRef)
                        return;

                    juce::var normalized;
                    if (!PropertyEditorFactory::normalizeValue(buildSpec.spec, juce::var(selectedText), normalized))
                    {
                        return;
                    }
                    if (buildSpec.onPreview)
                        buildSpec.onPreview(normalized);
                    if (buildSpec.onCommit)
                        buildSpec.onCommit(normalized);
                };

                if (auto* assetDropCombo = dynamic_cast<AssetDropComboBox*>(editor); assetDropCombo != nullptr)
                {
                    assetDropCombo->isDropAllowed = [spec = buildSpec.spec](const AssetDropPayload& payload)
                    {
                        return isAssetDropPayloadAccepted(spec, payload);
                    };

                    assetDropCombo->onAssetDropped = [buildSpec, safe = juce::Component::SafePointer<juce::ComboBox>(editor)](const juce::String& refKey)
                    {
                        if (safe == nullptr)
                            return;

                        safe->setText(refKey, juce::dontSendNotification);
                        juce::var normalized;
                        if (!PropertyEditorFactory::normalizeValue(buildSpec.spec, juce::var(refKey), normalized))
                            return;

                        if (buildSpec.onPreview)
                            buildSpec.onPreview(normalized);
                        if (buildSpec.onCommit)
                            buildSpec.onCommit(normalized);
                    };
                }
            }

            return std::unique_ptr<juce::Component>(editorBase.release());
        }

        const auto useSlider = !buildSpec.mixed
                            && !readOnly
                            && (buildSpec.spec.kind == Widgets::WidgetPropertyKind::number
                                || buildSpec.spec.kind == Widgets::WidgetPropertyKind::integer)
                            && buildSpec.spec.uiHint == Widgets::WidgetPropertyUiHint::slider
                            && buildSpec.spec.minValue.has_value()
                            && buildSpec.spec.maxValue.has_value();

        if (useSlider)
        {
            auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            slider->setRange(*buildSpec.spec.minValue,
                             *buildSpec.spec.maxValue,
                             buildSpec.spec.step.value_or(buildSpec.spec.kind == Widgets::WidgetPropertyKind::integer ? 1.0 : 0.001));
            slider->setNumDecimalPlacesToDisplay(buildSpec.spec.kind == Widgets::WidgetPropertyKind::integer
                                                     ? 0
                                                     : juce::jlimit(0, 8, buildSpec.spec.decimals));
            slider->setValue(static_cast<double>(buildSpec.value), juce::dontSendNotification);
            slider->setEnabled(!readOnly);

            slider->onValueChange = [buildSpec, safe = juce::Component::SafePointer<juce::Slider>(slider.get())]
            {
                if (safe == nullptr)
                    return;
                juce::var nextValue;
                if (buildSpec.spec.kind == Widgets::WidgetPropertyKind::integer)
                    nextValue = static_cast<int64_t>(std::llround(safe->getValue()));
                else
                    nextValue = safe->getValue();
                juce::var normalized;
                if (!PropertyEditorFactory::normalizeValue(buildSpec.spec, nextValue, normalized))
                    return;

                if (buildSpec.onPreview)
                    buildSpec.onPreview(normalized);
            };

            slider->onDragEnd = [buildSpec, safe = juce::Component::SafePointer<juce::Slider>(slider.get())]
            {
                if (safe == nullptr)
                    return;
                juce::var nextValue;
                if (buildSpec.spec.kind == Widgets::WidgetPropertyKind::integer)
                    nextValue = static_cast<int64_t>(std::llround(safe->getValue()));
                else
                    nextValue = safe->getValue();
                juce::var normalized;
                if (!PropertyEditorFactory::normalizeValue(buildSpec.spec, nextValue, normalized))
                    return;

                if (buildSpec.onCommit)
                    buildSpec.onCommit(normalized);
            };

            return slider;
        }

        const auto initialText = buildSpec.mixed ? juce::String() : formatValue(buildSpec.spec, buildSpec.value);
        return createTextEditor(buildSpec, initialText, buildSpec.mixed ? juce::String("--") : juce::String());
    }
}
