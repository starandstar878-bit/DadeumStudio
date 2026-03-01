#include "Gyeol/Serialization/DocumentJson.h"

#include "Gyeol/Core/Document.h"
#include "Gyeol/Core/SceneValidator.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace
{
    enum class UnknownWidgetTypeLoadPolicy
    {
        reject
    };

    constexpr auto kUnknownWidgetTypeLoadPolicy = UnknownWidgetTypeLoadPolicy::reject;

    const Gyeol::Widgets::WidgetRegistry& serializationRegistry()
    {
        static const auto registry = Gyeol::Widgets::makeDefaultWidgetRegistry();
        return registry;
    }

    juce::String widgetTypeToString(Gyeol::WidgetType type)
    {
        if (const auto* descriptor = serializationRegistry().find(type))
            return descriptor->typeKey;

        DBG("[Gyeol] serialize fallback: unknown widget type ordinal="
            + juce::String(static_cast<int>(type)) + ", defaulting to 'button'");
        return "button";
    }

    std::optional<Gyeol::WidgetType> widgetTypeFromString(const juce::String& value)
    {
        if (const auto* descriptor = serializationRegistry().findByKey(value))
            return descriptor->type;
        return std::nullopt;
    }

    juce::String nodeKindToString(Gyeol::NodeKind kind)
    {
        switch (kind)
        {
            case Gyeol::NodeKind::widget: return "widget";
            case Gyeol::NodeKind::group: return "group";
            case Gyeol::NodeKind::layer: return "layer";
        }

        return {};
    }

    std::optional<Gyeol::NodeKind> nodeKindFromString(const juce::String& value)
    {
        const auto normalized = value.trim();
        if (normalized == "widget") return Gyeol::NodeKind::widget;
        if (normalized == "group") return Gyeol::NodeKind::group;
        if (normalized == "layer") return Gyeol::NodeKind::layer;
        return std::nullopt;
    }

    juce::String assetKindToString(Gyeol::AssetKind kind)
    {
        return Gyeol::assetKindToKey(kind);
    }

    std::optional<Gyeol::AssetKind> assetKindFromString(const juce::String& value)
    {
        return Gyeol::assetKindFromKey(value);
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
        {
            const auto key = props.getName(i);
            if (key == juce::Identifier("bounds"))
                continue; // Legacy compatibility: geometry is stored in widget.bounds.

            outBag.set(key, props.getValueAt(i));
        }

        return Gyeol::validatePropertyBag(outBag);
    }

    juce::Result parseOptionalBoolProperty(const juce::NamedValueSet& props,
                                           const juce::Identifier& key,
                                           const juce::String& context,
                                           bool& outValue)
    {
        if (!props.contains(key))
            return juce::Result::ok();

        const auto& value = props[key];
        if (!value.isBool())
            return juce::Result::fail(context + "." + key.toString() + " must be bool");

        outValue = static_cast<bool>(value);
        return juce::Result::ok();
    }

    juce::Result parseOptionalOpacityProperty(const juce::NamedValueSet& props,
                                              const juce::Identifier& key,
                                              const juce::String& context,
                                              float& outValue)
    {
        if (!props.contains(key))
            return juce::Result::ok();

        const auto& value = props[key];
        if (!Gyeol::isNumericVar(value))
            return juce::Result::fail(context + "." + key.toString() + " must be numeric");

        const auto parsed = static_cast<float>(static_cast<double>(value));
        if (!std::isfinite(parsed) || parsed < 0.0f || parsed > 1.0f)
            return juce::Result::fail(context + "." + key.toString() + " must be within [0, 1]");

        outValue = parsed;
        return juce::Result::ok();
    }

    juce::Result parseOptionalIntProperty(const juce::NamedValueSet& props,
                                          const juce::Identifier& key,
                                          const juce::String& context,
                                          int& outValue)
    {
        if (!props.contains(key))
            return juce::Result::ok();

        const auto& value = props[key];
        if (!Gyeol::isNumericVar(value))
            return juce::Result::fail(context + "." + key.toString() + " must be numeric");

        outValue = static_cast<int>(value);
        return juce::Result::ok();
    }

    juce::var serializeIdArray(const std::vector<Gyeol::WidgetId>& ids)
    {
        juce::Array<juce::var> array;
        for (const auto id : ids)
            array.add(Gyeol::widgetIdToJsonString(id));

        return juce::var(array);
    }

    juce::Result parseIdArray(const juce::var& value,
                              std::vector<Gyeol::WidgetId>& outIds,
                              const juce::String& context)
    {
        const auto* array = value.getArray();
        if (array == nullptr)
            return juce::Result::fail(context + " must be array");

        outIds.clear();
        outIds.reserve(static_cast<size_t>(array->size()));
        for (const auto& item : *array)
        {
            const auto id = Gyeol::widgetIdFromJsonString(item.toString());
            if (!id.has_value() || *id <= Gyeol::kRootId)
                return juce::Result::fail(context + " id must be positive int64 encoded as string");

            outIds.push_back(*id);
        }

        return juce::Result::ok();
    }

    void rebuildGroupMemberGroupIds(Gyeol::DocumentModel& document)
    {
        for (auto& group : document.groups)
            group.memberGroupIds.clear();

        for (const auto& child : document.groups)
        {
            if (!child.parentGroupId.has_value())
                continue;

            const auto parentIt = std::find_if(document.groups.begin(),
                                               document.groups.end(),
                                               [&child](const Gyeol::GroupModel& candidate)
                                               {
                                                   return candidate.id == *child.parentGroupId;
                                               });
            if (parentIt == document.groups.end())
                continue;

            auto& parentGroup = *parentIt;
            if (std::find(parentGroup.memberGroupIds.begin(),
                          parentGroup.memberGroupIds.end(),
                          child.id) == parentGroup.memberGroupIds.end())
            {
                parentGroup.memberGroupIds.push_back(child.id);
            }
        }
    }

    bool entityIdExists(const Gyeol::DocumentModel& document, Gyeol::WidgetId id) noexcept
    {
        if (id <= Gyeol::kRootId)
            return false;

        const auto hasWidget = std::find_if(document.widgets.begin(),
                                            document.widgets.end(),
                                            [id](const Gyeol::WidgetModel& widget)
                                            {
                                                return widget.id == id;
                                            }) != document.widgets.end();
        if (hasWidget)
            return true;

        const auto hasGroup = std::find_if(document.groups.begin(),
                                           document.groups.end(),
                                           [id](const Gyeol::GroupModel& group)
                                           {
                                               return group.id == id;
                                           }) != document.groups.end();
        if (hasGroup)
            return true;

        return std::find_if(document.layers.begin(),
                            document.layers.end(),
                            [id](const Gyeol::LayerModel& layer)
                            {
                                return layer.id == id;
                            }) != document.layers.end();
    }

    Gyeol::WidgetId allocateLayerId(const Gyeol::DocumentModel& document) noexcept
    {
        Gyeol::WidgetId maxId = Gyeol::kRootId;
        for (const auto& widget : document.widgets)
            maxId = std::max(maxId, widget.id);
        for (const auto& group : document.groups)
            maxId = std::max(maxId, group.id);
        for (const auto& layer : document.layers)
            maxId = std::max(maxId, layer.id);

        auto candidate = maxId < std::numeric_limits<Gyeol::WidgetId>::max()
                             ? (maxId + 1)
                             : std::numeric_limits<Gyeol::WidgetId>::max();
        if (candidate <= Gyeol::kRootId)
            candidate = 1;

        while (entityIdExists(document, candidate))
        {
            if (candidate >= std::numeric_limits<Gyeol::WidgetId>::max())
                return std::numeric_limits<Gyeol::WidgetId>::max();
            ++candidate;
        }

        return candidate;
    }

    std::unordered_map<Gyeol::WidgetId, Gyeol::WidgetId> directOwnerGroupByWidgetId(const Gyeol::DocumentModel& document)
    {
        std::unordered_map<Gyeol::WidgetId, Gyeol::WidgetId> ownerByWidgetId;
        ownerByWidgetId.reserve(document.widgets.size());

        for (const auto& group : document.groups)
        {
            for (const auto widgetId : group.memberWidgetIds)
                ownerByWidgetId[widgetId] = group.id;
        }

        return ownerByWidgetId;
    }

    bool groupCoveredByLayer(const Gyeol::DocumentModel& document,
                             Gyeol::WidgetId groupId,
                             const std::unordered_set<Gyeol::WidgetId>& directLayerGroupIds) noexcept
    {
        auto cursor = groupId;
        std::unordered_set<Gyeol::WidgetId> visited;
        while (cursor > Gyeol::kRootId && visited.insert(cursor).second)
        {
            if (directLayerGroupIds.count(cursor) > 0)
                return true;

            const auto groupIt = std::find_if(document.groups.begin(),
                                              document.groups.end(),
                                              [cursor](const Gyeol::GroupModel& group)
                                              {
                                                  return group.id == cursor;
                                              });
            if (groupIt == document.groups.end() || !groupIt->parentGroupId.has_value())
                break;

            cursor = *groupIt->parentGroupId;
        }

        return false;
    }

    bool widgetCoveredByLayer(const Gyeol::DocumentModel& document,
                              Gyeol::WidgetId widgetId,
                              const std::unordered_set<Gyeol::WidgetId>& directLayerWidgetIds,
                              const std::unordered_set<Gyeol::WidgetId>& directLayerGroupIds,
                              const std::unordered_map<Gyeol::WidgetId, Gyeol::WidgetId>& ownerByWidgetId) noexcept
    {
        if (directLayerWidgetIds.count(widgetId) > 0)
            return true;

        const auto ownerIt = ownerByWidgetId.find(widgetId);
        if (ownerIt == ownerByWidgetId.end())
            return false;

        auto cursor = ownerIt->second;
        std::unordered_set<Gyeol::WidgetId> visited;
        while (cursor > Gyeol::kRootId && visited.insert(cursor).second)
        {
            if (directLayerGroupIds.count(cursor) > 0)
                return true;

            const auto groupIt = std::find_if(document.groups.begin(),
                                              document.groups.end(),
                                              [cursor](const Gyeol::GroupModel& group)
                                              {
                                                  return group.id == cursor;
                                              });
            if (groupIt == document.groups.end() || !groupIt->parentGroupId.has_value())
                break;

            cursor = *groupIt->parentGroupId;
        }

        return false;
    }

    void appendUniqueId(std::vector<Gyeol::WidgetId>& ids, Gyeol::WidgetId id)
    {
        if (std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(id);
    }

    void ensureLayerCoverage(Gyeol::DocumentModel& document)
    {
        if (document.layers.empty())
        {
            Gyeol::LayerModel layer;
            layer.id = allocateLayerId(document);
            if (layer.id == std::numeric_limits<Gyeol::WidgetId>::max())
                layer.id = 1;
            layer.name = "Layer 1";
            layer.order = 0;
            document.layers.push_back(std::move(layer));
        }

        std::vector<Gyeol::LayerModel*> orderedLayers;
        orderedLayers.reserve(document.layers.size());
        for (auto& layer : document.layers)
            orderedLayers.push_back(&layer);

        std::sort(orderedLayers.begin(),
                  orderedLayers.end(),
                  [](const Gyeol::LayerModel* lhs, const Gyeol::LayerModel* rhs)
                  {
                      if (lhs == nullptr || rhs == nullptr)
                          return lhs != nullptr;
                      if (lhs->order != rhs->order)
                          return lhs->order < rhs->order;
                      return lhs->id < rhs->id;
                  });

        for (size_t i = 0; i < orderedLayers.size(); ++i)
            orderedLayers[i]->order = static_cast<int>(i);

        std::unordered_set<Gyeol::WidgetId> validWidgetIds;
        validWidgetIds.reserve(document.widgets.size());
        for (const auto& widget : document.widgets)
            validWidgetIds.insert(widget.id);

        std::unordered_set<Gyeol::WidgetId> validGroupIds;
        validGroupIds.reserve(document.groups.size());
        for (const auto& group : document.groups)
            validGroupIds.insert(group.id);

        std::unordered_set<Gyeol::WidgetId> seenWidgets;
        std::unordered_set<Gyeol::WidgetId> seenGroups;
        seenWidgets.reserve(document.widgets.size());
        seenGroups.reserve(document.groups.size());

        for (auto* layer : orderedLayers)
        {
            if (layer == nullptr)
                continue;

            std::vector<Gyeol::WidgetId> filteredWidgets;
            filteredWidgets.reserve(layer->memberWidgetIds.size());
            for (const auto widgetId : layer->memberWidgetIds)
            {
                if (validWidgetIds.count(widgetId) == 0)
                    continue;
                if (!seenWidgets.insert(widgetId).second)
                    continue;
                filteredWidgets.push_back(widgetId);
            }
            layer->memberWidgetIds = std::move(filteredWidgets);

            std::vector<Gyeol::WidgetId> filteredGroups;
            filteredGroups.reserve(layer->memberGroupIds.size());
            for (const auto groupId : layer->memberGroupIds)
            {
                if (validGroupIds.count(groupId) == 0)
                    continue;
                if (!seenGroups.insert(groupId).second)
                    continue;
                filteredGroups.push_back(groupId);
            }
            layer->memberGroupIds = std::move(filteredGroups);
        }

        auto& fallbackLayer = *orderedLayers.front();
        const auto ownerByWidgetId = directOwnerGroupByWidgetId(document);

        std::unordered_set<Gyeol::WidgetId> directLayerWidgetIds;
        std::unordered_set<Gyeol::WidgetId> directLayerGroupIds;
        directLayerWidgetIds.reserve(document.widgets.size());
        directLayerGroupIds.reserve(document.groups.size());
        for (const auto* layer : orderedLayers)
        {
            if (layer == nullptr)
                continue;

            directLayerWidgetIds.insert(layer->memberWidgetIds.begin(), layer->memberWidgetIds.end());
            directLayerGroupIds.insert(layer->memberGroupIds.begin(), layer->memberGroupIds.end());
        }

        for (const auto& group : document.groups)
        {
            if (groupCoveredByLayer(document, group.id, directLayerGroupIds))
                continue;

            appendUniqueId(fallbackLayer.memberGroupIds, group.id);
            directLayerGroupIds.insert(group.id);
        }

        for (const auto& widget : document.widgets)
        {
            if (widgetCoveredByLayer(document,
                                     widget.id,
                                     directLayerWidgetIds,
                                     directLayerGroupIds,
                                     ownerByWidgetId))
            {
                continue;
            }

            appendUniqueId(fallbackLayer.memberWidgetIds, widget.id);
            directLayerWidgetIds.insert(widget.id);
        }
    }

    juce::var serializeWidget(const Gyeol::WidgetModel& widget)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(widget.id));
        object->setProperty("type", widgetTypeToString(widget.type));
        object->setProperty("bounds", serializeBounds(widget.bounds));
        object->setProperty("visible", widget.visible);
        object->setProperty("locked", widget.locked);
        object->setProperty("opacity", widget.opacity);
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
        {
            if constexpr (kUnknownWidgetTypeLoadPolicy == UnknownWidgetTypeLoadPolicy::reject)
                return juce::Result::fail("widget.type is unknown (policy=reject): " + props["type"].toString());
        }

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
        outWidget.visible = true;
        outWidget.locked = false;
        outWidget.opacity = 1.0f;

        if (const auto visibleResult = parseOptionalBoolProperty(props, "visible", "widget", outWidget.visible);
            visibleResult.failed())
            return visibleResult;
        if (const auto lockedResult = parseOptionalBoolProperty(props, "locked", "widget", outWidget.locked);
            lockedResult.failed())
            return lockedResult;
        if (const auto opacityResult = parseOptionalOpacityProperty(props, "opacity", "widget", outWidget.opacity);
            opacityResult.failed())
            return opacityResult;

        outWidget.properties = std::move(parsedProperties);
        return juce::Result::ok();
    }

    juce::var serializeGroup(const Gyeol::GroupModel& group)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(group.id));
        object->setProperty("name", group.name);
        object->setProperty("visible", group.visible);
        object->setProperty("locked", group.locked);
        object->setProperty("opacity", group.opacity);
        if (group.parentGroupId.has_value())
            object->setProperty("parentGroupId", Gyeol::widgetIdToJsonString(*group.parentGroupId));

        object->setProperty("members", serializeIdArray(group.memberWidgetIds));
        if (!group.memberGroupIds.empty())
            object->setProperty("memberGroups", serializeIdArray(group.memberGroupIds));
        return juce::var(object.release());
    }

    juce::Result parseGroup(const juce::var& value, Gyeol::GroupModel& outGroup)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("group must be object");

        const auto& props = object->getProperties();
        if (!props.contains("id") || !props.contains("members"))
            return juce::Result::fail("group requires id/members");

        const auto groupId = Gyeol::widgetIdFromJsonString(props["id"].toString());
        if (!groupId.has_value() || *groupId <= Gyeol::kRootId)
            return juce::Result::fail("group.id must be positive int64 encoded as string");

        outGroup.id = *groupId;
        outGroup.name = props.contains("name") ? props["name"].toString() : juce::String("Group");
        outGroup.visible = true;
        outGroup.locked = false;
        outGroup.opacity = 1.0f;
        outGroup.memberGroupIds.clear();

        if (const auto visibleResult = parseOptionalBoolProperty(props, "visible", "group", outGroup.visible);
            visibleResult.failed())
            return visibleResult;
        if (const auto lockedResult = parseOptionalBoolProperty(props, "locked", "group", outGroup.locked);
            lockedResult.failed())
            return lockedResult;
        if (const auto opacityResult = parseOptionalOpacityProperty(props, "opacity", "group", outGroup.opacity);
            opacityResult.failed())
            return opacityResult;

        outGroup.parentGroupId.reset();
        if (props.contains("parentGroupId"))
        {
            const auto parentGroupId = Gyeol::widgetIdFromJsonString(props["parentGroupId"].toString());
            if (!parentGroupId.has_value() || *parentGroupId <= Gyeol::kRootId)
                return juce::Result::fail("group.parentGroupId must be positive int64 encoded as string");
            outGroup.parentGroupId = *parentGroupId;
        }

        const auto membersResult = parseIdArray(props["members"], outGroup.memberWidgetIds, "group.members");
        if (membersResult.failed())
            return membersResult;

        return juce::Result::ok();
    }

    juce::var serializeLayer(const Gyeol::LayerModel& layer)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(layer.id));
        object->setProperty("name", layer.name);
        object->setProperty("order", layer.order);
        object->setProperty("visible", layer.visible);
        object->setProperty("locked", layer.locked);
        object->setProperty("members", serializeIdArray(layer.memberWidgetIds));
        object->setProperty("memberGroups", serializeIdArray(layer.memberGroupIds));
        return juce::var(object.release());
    }

    juce::Result parseLayer(const juce::var& value, Gyeol::LayerModel& outLayer)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("layer must be object");

        const auto& props = object->getProperties();
        if (!props.contains("id"))
            return juce::Result::fail("layer requires id");

        const auto layerId = Gyeol::widgetIdFromJsonString(props["id"].toString());
        if (!layerId.has_value() || *layerId <= Gyeol::kRootId)
            return juce::Result::fail("layer.id must be positive int64 encoded as string");

        outLayer.id = *layerId;
        outLayer.name = props.contains("name") ? props["name"].toString() : juce::String("Layer");
        outLayer.order = 0;
        outLayer.visible = true;
        outLayer.locked = false;
        outLayer.memberWidgetIds.clear();
        outLayer.memberGroupIds.clear();

        if (const auto orderResult = parseOptionalIntProperty(props, "order", "layer", outLayer.order);
            orderResult.failed())
            return orderResult;
        if (const auto visibleResult = parseOptionalBoolProperty(props, "visible", "layer", outLayer.visible);
            visibleResult.failed())
            return visibleResult;
        if (const auto lockedResult = parseOptionalBoolProperty(props, "locked", "layer", outLayer.locked);
            lockedResult.failed())
            return lockedResult;

        if (props.contains("members"))
        {
            const auto membersResult = parseIdArray(props["members"], outLayer.memberWidgetIds, "layer.members");
            if (membersResult.failed())
                return membersResult;
        }

        if (props.contains("memberGroups"))
        {
            const auto groupsResult = parseIdArray(props["memberGroups"], outLayer.memberGroupIds, "layer.memberGroups");
            if (groupsResult.failed())
                return groupsResult;
        }

        return juce::Result::ok();
    }

    juce::var serializeAsset(const Gyeol::AssetModel& asset)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(asset.id));
        object->setProperty("name", asset.name);
        object->setProperty("kind", assetKindToString(asset.kind));
        object->setProperty("refKey", asset.refKey);
        object->setProperty("path", asset.relativePath);
        object->setProperty("mime", asset.mimeType);
        object->setProperty("meta", serializeProperties(asset.meta));
        return juce::var(object.release());
    }

    juce::Result parseAsset(const juce::var& value, Gyeol::AssetModel& outAsset)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("asset must be object");

        const auto& props = object->getProperties();
        if (!props.contains("id") || !props.contains("kind") || !props.contains("refKey"))
            return juce::Result::fail("asset requires id, kind, refKey");

        const auto parsedId = Gyeol::widgetIdFromJsonString(props["id"].toString());
        if (!parsedId.has_value() || *parsedId <= Gyeol::kRootId)
            return juce::Result::fail("asset.id must be positive int64 encoded as string");

        const auto parsedKind = assetKindFromString(props["kind"].toString());
        if (!parsedKind.has_value())
            return juce::Result::fail("asset.kind is invalid");

        Gyeol::AssetModel asset;
        asset.id = *parsedId;
        asset.kind = *parsedKind;
        asset.name = props.contains("name") ? props["name"].toString() : juce::String();
        asset.refKey = props["refKey"].toString().trim();
        asset.relativePath = props.contains("path") ? props["path"].toString().trim() : juce::String();
        asset.mimeType = props.contains("mime") ? props["mime"].toString().trim() : juce::String();

        if (props.contains("meta"))
        {
            const auto metaResult = parseProperties(props["meta"], asset.meta);
            if (metaResult.failed())
                return metaResult;
        }
        else
        {
            asset.meta.clear();
        }

        outAsset = std::move(asset);
        return juce::Result::ok();
    }

    juce::var serializeNodeRef(const Gyeol::NodeRef& ref)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("kind", nodeKindToString(ref.kind));
        object->setProperty("id", Gyeol::widgetIdToJsonString(ref.id));
        return juce::var(object.release());
    }

    juce::Result parseNodeRef(const juce::var& value, Gyeol::NodeRef& outRef, const juce::String& context)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail(context + " must be object");

        const auto& props = object->getProperties();
        if (!props.contains("kind") || !props.contains("id"))
            return juce::Result::fail(context + " requires kind and id");

        const auto parsedKind = nodeKindFromString(props["kind"].toString());
        if (!parsedKind.has_value())
            return juce::Result::fail(context + ".kind is invalid");

        const auto parsedId = Gyeol::widgetIdFromJsonString(props["id"].toString());
        if (!parsedId.has_value() || *parsedId <= Gyeol::kRootId)
            return juce::Result::fail(context + ".id must be positive int64 encoded as string");

        outRef.kind = *parsedKind;
        outRef.id = *parsedId;
        return juce::Result::ok();
    }

    juce::var serializeRuntimeAction(const Gyeol::RuntimeActionModel& action)
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
            {
                object->setProperty("target", serializeNodeRef(action.target));

                auto propsObject = std::make_unique<juce::DynamicObject>();
                if (action.visible.has_value())
                    propsObject->setProperty("visible", *action.visible);
                if (action.locked.has_value())
                    propsObject->setProperty("locked", *action.locked);
                if (action.opacity.has_value())
                    propsObject->setProperty("opacity", *action.opacity);
                if (action.patch.size() > 0)
                    propsObject->setProperty("patch", serializeProperties(action.patch));
                object->setProperty("props", juce::var(propsObject.release()));
                break;
            }
            case Gyeol::RuntimeActionKind::setNodeBounds:
                object->setProperty("targetWidgetId", Gyeol::widgetIdToJsonString(action.targetWidgetId));
                object->setProperty("bounds", serializeBounds(action.bounds));
                break;
        }

        return juce::var(object.release());
    }

    juce::Result parseRuntimeAction(const juce::var& value, Gyeol::RuntimeActionModel& outAction)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("runtimeBindings.actions[] must be object");

        const auto& props = object->getProperties();
        if (!props.contains("kind"))
            return juce::Result::fail("runtimeBindings.actions[].kind is required");

        const auto parsedKind = Gyeol::runtimeActionKindFromKey(props["kind"].toString());
        if (!parsedKind.has_value())
            return juce::Result::fail("runtimeBindings.actions[].kind is invalid");

        Gyeol::RuntimeActionModel action;
        action.kind = *parsedKind;
        action.paramKey.clear();
        action.value = juce::var();
        action.delta = 0.0;
        action.target = {};
        action.visible.reset();
        action.locked.reset();
        action.opacity.reset();
        action.patch.clear();
        action.targetWidgetId = Gyeol::kRootId;
        action.bounds = {};

        switch (action.kind)
        {
            case Gyeol::RuntimeActionKind::setRuntimeParam:
                action.paramKey = props.contains("paramKey") ? props["paramKey"].toString() : juce::String();
                if (props.contains("value"))
                    action.value = props["value"];
                break;
            case Gyeol::RuntimeActionKind::adjustRuntimeParam:
                action.paramKey = props.contains("paramKey") ? props["paramKey"].toString() : juce::String();
                if (props.contains("delta"))
                {
                    if (!Gyeol::isNumericVar(props["delta"]))
                        return juce::Result::fail("runtimeBindings.actions[].delta must be numeric");
                    action.delta = static_cast<double>(props["delta"]);
                }
                break;
            case Gyeol::RuntimeActionKind::toggleRuntimeParam:
                action.paramKey = props.contains("paramKey") ? props["paramKey"].toString() : juce::String();
                break;
            case Gyeol::RuntimeActionKind::setNodeProps:
            {
                if (!props.contains("target"))
                    return juce::Result::fail("runtimeBindings.actions[].target is required for setNodeProps");
                if (const auto targetResult = parseNodeRef(props["target"], action.target, "runtimeBindings.actions[].target");
                    targetResult.failed())
                {
                    return targetResult;
                }

                if (!props.contains("props"))
                    break;

                const auto* propsObject = props["props"].getDynamicObject();
                if (propsObject == nullptr)
                    return juce::Result::fail("runtimeBindings.actions[].props must be object");

                const auto& propsBag = propsObject->getProperties();
                if (propsBag.contains("visible"))
                {
                    if (!propsBag["visible"].isBool())
                        return juce::Result::fail("runtimeBindings.actions[].props.visible must be bool");
                    action.visible = static_cast<bool>(propsBag["visible"]);
                }
                if (propsBag.contains("locked"))
                {
                    if (!propsBag["locked"].isBool())
                        return juce::Result::fail("runtimeBindings.actions[].props.locked must be bool");
                    action.locked = static_cast<bool>(propsBag["locked"]);
                }
                if (propsBag.contains("opacity"))
                {
                    if (!Gyeol::isNumericVar(propsBag["opacity"]))
                        return juce::Result::fail("runtimeBindings.actions[].props.opacity must be numeric");
                    action.opacity = static_cast<float>(static_cast<double>(propsBag["opacity"]));
                }
                if (propsBag.contains("patch"))
                {
                    const auto parsePatchResult = parseProperties(propsBag["patch"], action.patch);
                    if (parsePatchResult.failed())
                        return parsePatchResult;
                }
                break;
            }
            case Gyeol::RuntimeActionKind::setNodeBounds:
            {
                if (!props.contains("targetWidgetId") || !props.contains("bounds"))
                    return juce::Result::fail("runtimeBindings.actions[] setNodeBounds requires targetWidgetId and bounds");

                const auto parsedWidgetId = Gyeol::widgetIdFromJsonString(props["targetWidgetId"].toString());
                if (!parsedWidgetId.has_value() || *parsedWidgetId <= Gyeol::kRootId)
                    return juce::Result::fail("runtimeBindings.actions[].targetWidgetId must be positive int64 encoded as string");

                const auto parsedBounds = parseBounds(props["bounds"]);
                if (!parsedBounds.has_value())
                    return juce::Result::fail("runtimeBindings.actions[].bounds must be rect object");

                action.targetWidgetId = *parsedWidgetId;
                action.bounds = *parsedBounds;
                break;
            }
        }

        outAction = std::move(action);
        return juce::Result::ok();
    }

    juce::var serializeRuntimeBinding(const Gyeol::RuntimeBindingModel& binding)
    {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("id", Gyeol::widgetIdToJsonString(binding.id));
        object->setProperty("name", binding.name);
        object->setProperty("enabled", binding.enabled);
        object->setProperty("sourceWidgetId", Gyeol::widgetIdToJsonString(binding.sourceWidgetId));
        object->setProperty("eventKey", binding.eventKey);

        juce::Array<juce::var> actions;
        for (const auto& action : binding.actions)
            actions.add(serializeRuntimeAction(action));
        object->setProperty("actions", juce::var(actions));

        return juce::var(object.release());
    }

    juce::Result parseRuntimeBinding(const juce::var& value, Gyeol::RuntimeBindingModel& outBinding)
    {
        const auto* object = value.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("runtimeBindings[] must be object");

        const auto& props = object->getProperties();
        if (!props.contains("id") || !props.contains("sourceWidgetId") || !props.contains("eventKey") || !props.contains("actions"))
            return juce::Result::fail("runtimeBindings[] requires id/sourceWidgetId/eventKey/actions");

        const auto parsedId = Gyeol::widgetIdFromJsonString(props["id"].toString());
        if (!parsedId.has_value() || *parsedId <= Gyeol::kRootId)
            return juce::Result::fail("runtimeBindings[].id must be positive int64 encoded as string");

        const auto parsedSource = Gyeol::widgetIdFromJsonString(props["sourceWidgetId"].toString());
        if (!parsedSource.has_value() || *parsedSource <= Gyeol::kRootId)
            return juce::Result::fail("runtimeBindings[].sourceWidgetId must be positive int64 encoded as string");

        const auto* actionArray = props["actions"].getArray();
        if (actionArray == nullptr)
            return juce::Result::fail("runtimeBindings[].actions must be array");

        Gyeol::RuntimeBindingModel binding;
        binding.id = *parsedId;
        binding.name = props.contains("name") ? props["name"].toString() : juce::String();
        binding.enabled = true;
        if (props.contains("enabled"))
        {
            if (!props["enabled"].isBool())
                return juce::Result::fail("runtimeBindings[].enabled must be bool");
            binding.enabled = static_cast<bool>(props["enabled"]);
        }
        binding.sourceWidgetId = *parsedSource;
        binding.eventKey = props["eventKey"].toString().trim();

        binding.actions.reserve(static_cast<size_t>(actionArray->size()));
        for (const auto& actionValue : *actionArray)
        {
            Gyeol::RuntimeActionModel action;
            const auto parseActionResult = parseRuntimeAction(actionValue, action);
            if (parseActionResult.failed())
                return parseActionResult;
            binding.actions.push_back(std::move(action));
        }

        outBinding = std::move(binding);
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
    juce::Result serializeDocumentToJsonString(const DocumentModel& document,
                                               const EditorStateModel& editorState,
                                               juce::String& jsonOut)
    {
        const auto sceneCheck = Core::SceneValidator::validateScene(document, &editorState);
        if (sceneCheck.failed())
            return sceneCheck;

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("version", serializeSchemaVersion(document.schemaVersion));

        juce::Array<juce::var> widgetArray;
        for (const auto& widget : document.widgets)
            widgetArray.add(serializeWidget(widget));
        root->setProperty("widgets", juce::var(widgetArray));

        juce::Array<juce::var> groupArray;
        for (const auto& group : document.groups)
            groupArray.add(serializeGroup(group));
        root->setProperty("groups", juce::var(groupArray));

        juce::Array<juce::var> layerArray;
        for (const auto& layer : document.layers)
            layerArray.add(serializeLayer(layer));
        root->setProperty("layers", juce::var(layerArray));

        juce::Array<juce::var> assetArray;
        for (const auto& asset : document.assets)
            assetArray.add(serializeAsset(asset));
        root->setProperty("assets", juce::var(assetArray));

        juce::Array<juce::var> runtimeBindingArray;
        for (const auto& binding : document.runtimeBindings)
            runtimeBindingArray.add(serializeRuntimeBinding(binding));
        root->setProperty("runtimeBindings", juce::var(runtimeBindingArray));

        root->setProperty("editor", serializeSelection(editorState));

        jsonOut = juce::JSON::toString(juce::var(root.release()), true);
        return juce::Result::ok();
    }

    juce::Result saveDocumentToFile(const juce::File& file,
                                    const DocumentModel& document,
                                    const EditorStateModel& editorState)
    {
        juce::String json;
        const auto serializeResult = serializeDocumentToJsonString(document, editorState, json);
        if (serializeResult.failed())
            return serializeResult;

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

        if (rootProps.contains("groups"))
        {
            const auto* groupsArray = rootProps["groups"].getArray();
            if (groupsArray == nullptr)
                return juce::Result::fail("groups must be array when present");

            nextDocument.groups.reserve(static_cast<size_t>(groupsArray->size()));
            for (const auto& groupValue : *groupsArray)
            {
                GroupModel group;
                const auto groupResult = parseGroup(groupValue, group);
                if (groupResult.failed())
                    return groupResult;
                nextDocument.groups.push_back(std::move(group));
            }
        }

        rebuildGroupMemberGroupIds(nextDocument);

        if (rootProps.contains("layers"))
        {
            const auto* layersArray = rootProps["layers"].getArray();
            if (layersArray == nullptr)
                return juce::Result::fail("layers must be array when present");

            nextDocument.layers.reserve(static_cast<size_t>(layersArray->size()));
            for (const auto& layerValue : *layersArray)
            {
                LayerModel layer;
                const auto layerResult = parseLayer(layerValue, layer);
                if (layerResult.failed())
                    return layerResult;
                nextDocument.layers.push_back(std::move(layer));
            }
        }

        if (rootProps.contains("assets"))
        {
            const auto* assetsArray = rootProps["assets"].getArray();
            if (assetsArray == nullptr)
                return juce::Result::fail("assets must be array when present");

            nextDocument.assets.reserve(static_cast<size_t>(assetsArray->size()));
            for (const auto& assetValue : *assetsArray)
            {
                AssetModel asset;
                const auto assetResult = parseAsset(assetValue, asset);
                if (assetResult.failed())
                    return assetResult;
                nextDocument.assets.push_back(std::move(asset));
            }
        }
        else
        {
            nextDocument.assets.clear();
        }

        if (rootProps.contains("runtimeBindings"))
        {
            const auto* bindingsArray = rootProps["runtimeBindings"].getArray();
            if (bindingsArray == nullptr)
                return juce::Result::fail("runtimeBindings must be array when present");

            nextDocument.runtimeBindings.reserve(static_cast<size_t>(bindingsArray->size()));
            for (const auto& bindingValue : *bindingsArray)
            {
                RuntimeBindingModel binding;
                const auto bindingResult = parseRuntimeBinding(bindingValue, binding);
                if (bindingResult.failed())
                    return bindingResult;
                nextDocument.runtimeBindings.push_back(std::move(binding));
            }
        }
        else
        {
            nextDocument.runtimeBindings.clear();
        }

        ensureLayerCoverage(nextDocument);

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
        return Core::SceneValidator::validateScene(documentOut, &editorStateOut);
    }
}
