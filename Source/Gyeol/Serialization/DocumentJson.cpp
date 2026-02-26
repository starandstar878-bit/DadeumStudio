#include "Gyeol/Serialization/DocumentJson.h"

#include "Gyeol/Core/Document.h"
#include <algorithm>

namespace
{
    juce::String widgetTypeToString(Gyeol::WidgetType type)
    {
        switch (type)
        {
            case Gyeol::WidgetType::button: return "button";
            case Gyeol::WidgetType::slider: return "slider";
            case Gyeol::WidgetType::knob:   return "knob";
            case Gyeol::WidgetType::label:  return "label";
            case Gyeol::WidgetType::meter:  return "meter";
            default: return "button";
        }
    }

    std::optional<Gyeol::WidgetType> widgetTypeFromString(const juce::String& value)
    {
        if (value == "button") return Gyeol::WidgetType::button;
        if (value == "slider") return Gyeol::WidgetType::slider;
        if (value == "knob")   return Gyeol::WidgetType::knob;
        if (value == "label")  return Gyeol::WidgetType::label;
        if (value == "meter")  return Gyeol::WidgetType::meter;
        return std::nullopt;
    }

    juce::var serializeSchemaVersion(const Gyeol::SchemaVersion& version)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("major", version.major);
        object->setProperty("minor", version.minor);
        object->setProperty("patch", version.patch);
        return juce::var(object.release());
    }

    std::optional<Gyeol::SchemaVersion> parseSchemaVersion(const juce::var& value)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return std::nullopt;

        const auto& props = object->getProperties();
        if (!props.contains("major") || !props.contains("minor") || !props.contains("patch"))
            return std::nullopt;
        if (!Gyeol::isNumericVar(props["major"]) || !Gyeol::isNumericVar(props["minor"]) || !Gyeol::isNumericVar(props["patch"]))
            return std::nullopt;

        Gyeol::SchemaVersion version;
        version.major = static_cast<int>(props["major"]);
        version.minor = static_cast<int>(props["minor"]);
        version.patch = static_cast<int>(props["patch"]);
        return version;
    }

    juce::var serializeBounds(const juce::Rectangle<float>& bounds)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("x", bounds.getX());
        object->setProperty("y", bounds.getY());
        object->setProperty("w", bounds.getWidth());
        object->setProperty("h", bounds.getHeight());
        return juce::var(object.release());
    }

    std::optional<juce::Rectangle<float>> parseBounds(const juce::var& value)
    {
        if (!Gyeol::isRectFVar(value))
            return std::nullopt;

        const auto& props = value.getDynamicObject()->getProperties();
        return juce::Rectangle<float>(static_cast<float>(props["x"]),
                                      static_cast<float>(props["y"]),
                                      static_cast<float>(props["w"]),
                                      static_cast<float>(props["h"]));
    }

    juce::var serializeProperties(const Gyeol::PropertyBag& bag)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        for (int i = 0; i < bag.size(); ++i)
            object->setProperty(bag.getName(i), bag.getValueAt(i));

        return juce::var(object.release());
    }

    juce::Result parseProperties(const juce::var& value, Gyeol::PropertyBag& outBag)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("widget.properties must be object");

        outBag.clear();
        const auto& props = object->getProperties();
        for (int i = 0; i < props.size(); ++i)
            outBag.set(props.getName(i), props.getValueAt(i));

        return Gyeol::validatePropertyBag(outBag);
    }

    juce::var serializeWidget(const Gyeol::WidgetModel& widget)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(widget.id));
        object->setProperty("type", widgetTypeToString(widget.type));
        object->setProperty("bounds", serializeBounds(widget.bounds));
        object->setProperty("properties", serializeProperties(widget.properties));
        return juce::var(object.release());
    }

    juce::Result parseWidget(const juce::var& value, Gyeol::WidgetModel& outWidget)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("widget must be object");

        const auto& props = object->getProperties();
        if (!props.contains("id") || !props.contains("type") || !props.contains("bounds") || !props.contains("properties"))
            return juce::Result::fail("widget requires id/type/bounds/properties");

        const auto id = Gyeol::widgetIdFromJsonString(props["id"].toString());
        if (!id.has_value() || *id <= Gyeol::kRootId)
            return juce::Result::fail("widget.id must be positive int64 encoded as string");

        const auto type = widgetTypeFromString(props["type"].toString());
        if (!type.has_value())
            return juce::Result::fail("widget.type is unknown");

        const auto bounds = parseBounds(props["bounds"]);
        if (!bounds.has_value())
            return juce::Result::fail("widget.bounds is invalid");

        Gyeol::PropertyBag parsedProperties;
        const auto propResult = parseProperties(props["properties"], parsedProperties);
        if (propResult.failed())
            return propResult;

        outWidget.id = *id;
        outWidget.type = *type;
        outWidget.bounds = *bounds;
        outWidget.properties = std::move(parsedProperties);
        return juce::Result::ok();
    }

    juce::var serializeSelection(const Gyeol::EditorStateModel& editorState)
    {
        juce::Array<juce::var> selectionArray;
        for (const auto id : editorState.selection)
            selectionArray.add(Gyeol::widgetIdToJsonString(id));

        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("selection", juce::var(selectionArray));
        return juce::var(object.release());
    }

    juce::Result parseSelection(const juce::var& value,
                                const Gyeol::DocumentModel& document,
                                Gyeol::EditorStateModel& outEditorState)
    {
        outEditorState.selection.clear();

        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::ok();

        const auto& props = object->getProperties();
        if (!props.contains("selection"))
            return juce::Result::ok();

        const auto* array = props["selection"].getArray();
        if (array == nullptr)
            return juce::Result::fail("editor.selection must be an array");

        std::vector<Gyeol::WidgetId> parsedSelection;
        parsedSelection.reserve(static_cast<size_t>(array->size()));

        for (const auto& valueInArray : *array)
        {
            const auto id = Gyeol::widgetIdFromJsonString(valueInArray.toString());
            if (!id.has_value())
                continue;

            const auto exists = std::find_if(document.widgets.begin(),
                                             document.widgets.end(),
                                             [id](const Gyeol::WidgetModel& widget)
                                             {
                                                 return widget.id == *id;
                                             }) != document.widgets.end();

            if (!exists)
                continue;

            if (std::find(parsedSelection.begin(), parsedSelection.end(), *id) == parsedSelection.end())
                parsedSelection.push_back(*id);
        }

        outEditorState.selection = std::move(parsedSelection);
        return juce::Result::ok();
    }

    juce::Result verifySchemaCompatibility(const Gyeol::SchemaVersion& loaded)
    {
        const auto current = Gyeol::currentSchemaVersion();

        if (loaded.major != current.major)
            return juce::Result::fail("Unsupported schema major version");

        if (loaded.minor > current.minor || (loaded.minor == current.minor && loaded.patch > current.patch))
            return juce::Result::fail("Document schema is newer than runtime");

        return juce::Result::ok();
    }
}

namespace Gyeol::Serialization
{
    juce::Result saveDocumentToFile(const juce::File& file,
                                    const DocumentModel& document,
                                    const EditorStateModel& editorState)
    {
        const auto bagCheck = [&document]()
        {
            for (const auto& widget : document.widgets)
            {
                const auto result = validatePropertyBag(widget.properties);
                if (result.failed())
                    return result;
            }
            return juce::Result::ok();
        }();

        if (bagCheck.failed())
            return bagCheck;

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("version", serializeSchemaVersion(document.schemaVersion));

        juce::Array<juce::var> widgetArray;
        for (const auto& widget : document.widgets)
            widgetArray.add(serializeWidget(widget));
        root->setProperty("widgets", juce::var(widgetArray));
        root->setProperty("editor", serializeSelection(editorState));

        const auto json = juce::JSON::toString(juce::var(root.release()), true);
        if (!file.replaceWithText(json))
            return juce::Result::fail("Failed to write JSON file: " + file.getFullPathName());

        return juce::Result::ok();
    }

    juce::Result loadDocumentFromFile(const juce::File& file,
                                      DocumentModel& documentOut,
                                      EditorStateModel& editorStateOut)
    {
        if (!file.existsAsFile())
            return juce::Result::fail("File not found: " + file.getFullPathName());

        const auto fileText = file.loadFileAsString();
        juce::var rootVar;
        const auto parseResult = juce::JSON::parse(fileText, rootVar);
        if (parseResult.failed())
            return juce::Result::fail("JSON parse error: " + parseResult.getErrorMessage());

        const auto* rootObject = rootVar.getDynamicObject();
        if (rootObject == nullptr)
            return juce::Result::fail("Root must be object");

        const auto& rootProps = rootObject->getProperties();
        if (!rootProps.contains("version") || !rootProps.contains("widgets"))
            return juce::Result::fail("Document requires version and widgets");

        const auto parsedVersion = parseSchemaVersion(rootProps["version"]);
        if (!parsedVersion.has_value())
            return juce::Result::fail("Invalid version field");

        const auto versionCheck = verifySchemaCompatibility(*parsedVersion);
        if (versionCheck.failed())
            return versionCheck;

        const auto* widgetsArray = rootProps["widgets"].getArray();
        if (widgetsArray == nullptr)
            return juce::Result::fail("widgets must be array");

        DocumentModel nextDocument;
        nextDocument.schemaVersion = currentSchemaVersion();
        nextDocument.widgets.reserve(static_cast<size_t>(widgetsArray->size()));

        for (const auto& widgetValue : *widgetsArray)
        {
            WidgetModel widget;
            const auto result = parseWidget(widgetValue, widget);
            if (result.failed())
                return result;
            nextDocument.widgets.push_back(std::move(widget));
        }

        // JSON load rule: nextWidgetId = max(widgetId) + 1 (minimum 1, rootId fixed to 0).
        Core::Document::syncNextWidgetIdAfterLoad(nextDocument);

        EditorStateModel nextEditor;
        if (rootProps.contains("editor"))
        {
            const auto selectionResult = parseSelection(rootProps["editor"], nextDocument, nextEditor);
            if (selectionResult.failed())
                return selectionResult;
        }

        documentOut = std::move(nextDocument);
        editorStateOut = std::move(nextEditor);
        return juce::Result::ok();
    }
}
