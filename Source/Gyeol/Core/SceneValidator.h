#pragma once

#include "Gyeol/Public/Types.h"
#include "Gyeol/Runtime/PropertyBindingResolver.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Gyeol::Core::SceneValidator
{
    enum class RuntimeBindingIssueSeverity
    {
        warning,
        error
    };

    struct RuntimeBindingIssue
    {
        RuntimeBindingIssueSeverity severity = RuntimeBindingIssueSeverity::warning;
        juce::String title;
        juce::String message;
    };

    inline bool isFiniteOpacity(float value) noexcept
    {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    }

    inline bool isFiniteBounds(const juce::Rectangle<float>& bounds) noexcept
    {
        return std::isfinite(bounds.getX())
            && std::isfinite(bounds.getY())
            && std::isfinite(bounds.getWidth())
            && std::isfinite(bounds.getHeight());
    }

    inline bool isFiniteRectNonNegative(const juce::Rectangle<float>& bounds) noexcept
    {
        return isFiniteBounds(bounds) && bounds.getWidth() >= 0.0f && bounds.getHeight() >= 0.0f;
    }

    inline juce::Result validateSchemaVersion(const SchemaVersion& version)
    {
        const auto current = currentSchemaVersion();
        if (version.major != current.major)
            return juce::Result::fail("schema.major mismatch");

        if (compareSchemaVersion(version, current) > 0)
            return juce::Result::fail("schema is newer than runtime");

        return juce::Result::ok();
    }

    inline juce::Result validateDocument(const DocumentModel& document)
    {
        const auto schemaCheck = validateSchemaVersion(document.schemaVersion);
        if (schemaCheck.failed())
            return schemaCheck;

        if (document.layers.empty())
            return juce::Result::fail("document must contain at least one layer");

        std::vector<WidgetId> sortedIds;
        sortedIds.reserve(document.widgets.size());

        for (const auto& widget : document.widgets)
        {
            if (widget.id <= kRootId)
                return juce::Result::fail("widget.id must be > rootId");

            if (!isFiniteBounds(widget.bounds))
                return juce::Result::fail("widget.bounds must be finite");
            if (widget.bounds.getWidth() < 0.0f || widget.bounds.getHeight() < 0.0f)
                return juce::Result::fail("widget.bounds width/height must be >= 0");
            if (!isFiniteOpacity(widget.opacity))
                return juce::Result::fail("widget.opacity must be within [0, 1]");

            const auto propertyResult = validatePropertyBag(widget.properties);
            if (propertyResult.failed())
                return juce::Result::fail("invalid widget.properties for widget id " + juce::String(widget.id)
                                          + ": " + propertyResult.getErrorMessage());

            sortedIds.push_back(widget.id);
        }

        std::sort(sortedIds.begin(), sortedIds.end());
        if (std::adjacent_find(sortedIds.begin(), sortedIds.end()) != sortedIds.end())
            return juce::Result::fail("widget ids must be unique");

        std::unordered_set<WidgetId> widgetIdSet(sortedIds.begin(), sortedIds.end());
        std::unordered_set<WidgetId> groupIds;
        std::unordered_set<WidgetId> groupedMembers;
        std::unordered_map<WidgetId, const GroupModel*> groupById;
        std::unordered_map<WidgetId, int> childCounts;
        std::unordered_map<WidgetId, WidgetId> ownerByWidgetId;
        groupById.reserve(document.groups.size());
        childCounts.reserve(document.groups.size());
        ownerByWidgetId.reserve(document.widgets.size());

        for (const auto& group : document.groups)
        {
            if (group.id <= kRootId)
                return juce::Result::fail("group.id must be > rootId");
            if (widgetIdSet.count(group.id) > 0)
                return juce::Result::fail("group.id must not collide with widget ids");
            if (!groupIds.insert(group.id).second)
                return juce::Result::fail("group ids must be unique");
            if (!isFiniteOpacity(group.opacity))
                return juce::Result::fail("group.opacity must be within [0, 1]");
            groupById.emplace(group.id, &group);
            childCounts[group.id] = 0;

            std::unordered_set<WidgetId> membersInGroup;
            for (const auto memberId : group.memberWidgetIds)
            {
                if (memberId <= kRootId)
                    return juce::Result::fail("group member id must be > rootId");
                if (widgetIdSet.count(memberId) == 0)
                    return juce::Result::fail("group member id not found in document widgets");
                if (!membersInGroup.insert(memberId).second)
                    return juce::Result::fail("group member ids must be unique inside each group");
                if (!groupedMembers.insert(memberId).second)
                    return juce::Result::fail("widget must belong to at most one group");
                ownerByWidgetId[memberId] = group.id;
            }
        }

        for (const auto& group : document.groups)
        {
            if (!group.parentGroupId.has_value())
                continue;

            if (*group.parentGroupId <= kRootId)
                return juce::Result::fail("group.parentGroupId must be > rootId");
            if (*group.parentGroupId == group.id)
                return juce::Result::fail("group.parentGroupId must not equal group.id");
            if (groupById.find(*group.parentGroupId) == groupById.end())
                return juce::Result::fail("group.parentGroupId not found in document groups");

            ++childCounts[*group.parentGroupId];
        }

        std::unordered_map<WidgetId, std::unordered_set<WidgetId>> expectedChildGroupIdsByParent;
        expectedChildGroupIdsByParent.reserve(document.groups.size());
        for (const auto& group : document.groups)
            expectedChildGroupIdsByParent[group.id] = {};

        for (const auto& group : document.groups)
        {
            if (group.parentGroupId.has_value())
                expectedChildGroupIdsByParent[*group.parentGroupId].insert(group.id);
        }

        for (const auto& group : document.groups)
        {
            std::unordered_set<WidgetId> explicitChildGroupIds;
            explicitChildGroupIds.reserve(group.memberGroupIds.size());
            for (const auto childGroupId : group.memberGroupIds)
            {
                if (childGroupId <= kRootId)
                    return juce::Result::fail("group.memberGroupIds id must be > rootId");
                if (groupIds.count(childGroupId) == 0)
                    return juce::Result::fail("group.memberGroupIds id not found in document groups");
                if (!explicitChildGroupIds.insert(childGroupId).second)
                    return juce::Result::fail("group.memberGroupIds must be unique inside each group");
            }

            const auto& expectedChildIds = expectedChildGroupIdsByParent.at(group.id);

            if (explicitChildGroupIds != expectedChildIds)
                return juce::Result::fail("group.memberGroupIds must match parentGroupId hierarchy");
        }

        for (const auto& group : document.groups)
        {
            std::unordered_set<WidgetId> chain;
            chain.insert(group.id);

            auto parent = group.parentGroupId;
            while (parent.has_value())
            {
                if (!chain.insert(*parent).second)
                    return juce::Result::fail("group hierarchy must not contain cycles");

                const auto it = groupById.find(*parent);
                if (it == groupById.end())
                    return juce::Result::fail("group hierarchy references missing parent");

                parent = it->second->parentGroupId;
            }
        }

        for (const auto& group : document.groups)
        {
            const auto childCountIt = childCounts.find(group.id);
            const auto childCount = childCountIt != childCounts.end() ? childCountIt->second : 0;

            // Keep leaf-group invariants stable: a leaf group should have at least two widgets.
            if (childCount == 0 && group.memberWidgetIds.size() < 2)
                return juce::Result::fail("leaf group must contain at least two widget ids");
        }

        std::unordered_set<WidgetId> layerIds;
        std::unordered_set<int> layerOrders;
        std::unordered_set<WidgetId> layeredWidgetMembers;
        std::unordered_set<WidgetId> layeredGroupMembers;
        layerIds.reserve(document.layers.size());
        layerOrders.reserve(document.layers.size());

        for (const auto& layer : document.layers)
        {
            if (layer.id <= kRootId)
                return juce::Result::fail("layer.id must be > rootId");
            if (!layerIds.insert(layer.id).second)
                return juce::Result::fail("layer ids must be unique");
            if (widgetIdSet.count(layer.id) > 0 || groupIds.count(layer.id) > 0)
                return juce::Result::fail("layer.id must not collide with widget/group ids");
            if (!layerOrders.insert(layer.order).second)
                return juce::Result::fail("layer.order must be unique");

            std::unordered_set<WidgetId> membersInLayer;
            membersInLayer.reserve(layer.memberWidgetIds.size());
            for (const auto memberId : layer.memberWidgetIds)
            {
                if (memberId <= kRootId)
                    return juce::Result::fail("layer.members id must be > rootId");
                if (widgetIdSet.count(memberId) == 0)
                    return juce::Result::fail("layer.members id not found in document widgets");
                if (!membersInLayer.insert(memberId).second)
                    return juce::Result::fail("layer.members ids must be unique inside each layer");
                if (!layeredWidgetMembers.insert(memberId).second)
                    return juce::Result::fail("widget must belong to at most one layer");
            }

            std::unordered_set<WidgetId> groupsInLayer;
            groupsInLayer.reserve(layer.memberGroupIds.size());
            for (const auto groupId : layer.memberGroupIds)
            {
                if (groupId <= kRootId)
                    return juce::Result::fail("layer.memberGroups id must be > rootId");
                if (groupIds.count(groupId) == 0)
                    return juce::Result::fail("layer.memberGroups id not found in document groups");
                if (!groupsInLayer.insert(groupId).second)
                    return juce::Result::fail("layer.memberGroups ids must be unique inside each layer");
                if (!layeredGroupMembers.insert(groupId).second)
                    return juce::Result::fail("group must belong to at most one layer");
            }
        }

        const auto groupCoveredByLayer = [&](WidgetId groupId) -> bool
        {
            WidgetId cursor = groupId;
            std::unordered_set<WidgetId> visited;
            while (cursor > kRootId && visited.insert(cursor).second)
            {
                if (layeredGroupMembers.count(cursor) > 0)
                    return true;

                const auto it = groupById.find(cursor);
                if (it == groupById.end() || !it->second->parentGroupId.has_value())
                    break;

                cursor = *it->second->parentGroupId;
            }

            return false;
        };

        for (const auto& group : document.groups)
        {
            if (!groupCoveredByLayer(group.id))
                return juce::Result::fail("group must belong to at least one layer");
        }

        for (const auto& widget : document.widgets)
        {
            if (layeredWidgetMembers.count(widget.id) > 0)
                continue;

            bool coveredByGroup = false;
            auto ownerIt = ownerByWidgetId.find(widget.id);
            if (ownerIt != ownerByWidgetId.end())
            {
                WidgetId cursor = ownerIt->second;
                std::unordered_set<WidgetId> visited;
                while (cursor > kRootId && visited.insert(cursor).second)
                {
                    if (layeredGroupMembers.count(cursor) > 0)
                    {
                        coveredByGroup = true;
                        break;
                    }

                    const auto groupIt = groupById.find(cursor);
                    if (groupIt == groupById.end() || !groupIt->second->parentGroupId.has_value())
                        break;

                    cursor = *groupIt->second->parentGroupId;
                }
            }

            if (!coveredByGroup)
                return juce::Result::fail("widget must belong to at least one layer");
        }

        std::unordered_set<WidgetId> assetIds;
        std::unordered_set<juce::String> assetRefKeys;
        assetIds.reserve(document.assets.size());
        assetRefKeys.reserve(document.assets.size());

        for (const auto& asset : document.assets)
        {
            if (asset.id <= kRootId)
                return juce::Result::fail("asset.id must be > rootId");
            if (!assetIds.insert(asset.id).second)
                return juce::Result::fail("asset ids must be unique");

            const auto normalizedRefKey = asset.refKey.trim().toLowerCase();
            if (normalizedRefKey.isEmpty())
                return juce::Result::fail("asset.refKey must not be empty");
            if (!assetRefKeys.insert(normalizedRefKey).second)
                return juce::Result::fail("asset.refKey must be unique");

            if (asset.relativePath.isNotEmpty() && juce::File::isAbsolutePath(asset.relativePath))
                return juce::Result::fail("asset.relativePath must be relative");

            const auto metaResult = validatePropertyBag(asset.meta);
            if (metaResult.failed())
                return juce::Result::fail("invalid asset.meta for asset id " + juce::String(asset.id)
                                          + ": " + metaResult.getErrorMessage());
        }

        return juce::Result::ok();
    }

    inline juce::Result validateEditorState(const DocumentModel& document,
                                            const EditorStateModel& editorState)
    {
        std::unordered_set<WidgetId> widgetIds;
        widgetIds.reserve(document.widgets.size());
        for (const auto& widget : document.widgets)
            widgetIds.insert(widget.id);

        std::vector<WidgetId> sortedSelection;
        sortedSelection.reserve(editorState.selection.size());

        for (const auto id : editorState.selection)
        {
            if (id <= kRootId)
                return juce::Result::fail("selection ids must be > rootId");
            if (widgetIds.find(id) == widgetIds.end())
                return juce::Result::fail("selection id not found in document");

            sortedSelection.push_back(id);
        }

        std::sort(sortedSelection.begin(), sortedSelection.end());
        if (std::adjacent_find(sortedSelection.begin(), sortedSelection.end()) != sortedSelection.end())
            return juce::Result::fail("selection ids must be unique");

        return juce::Result::ok();
    }

    inline juce::Result validateRuntimeParams(const DocumentModel& document)
    {
        std::unordered_set<juce::String> paramKeys;
        paramKeys.reserve(document.runtimeParams.size());

        for (const auto& param : document.runtimeParams)
        {
            const auto key = param.key.trim();
            if (key.isEmpty())
                return juce::Result::fail("runtimeParams.key must not be empty");

            const auto normalizedKey = key.toLowerCase();
            if (!paramKeys.insert(normalizedKey).second)
                return juce::Result::fail("runtimeParams.key must be unique");

            switch (param.type)
            {
                case RuntimeParamValueType::number:
                {
                    if (!isNumericVar(param.defaultValue))
                        return juce::Result::fail("runtimeParams.defaultValue must be numeric for number type");
                    const auto value = static_cast<double>(param.defaultValue);
                    if (!std::isfinite(value))
                        return juce::Result::fail("runtimeParams.defaultValue must be finite for number type");
                    break;
                }
                case RuntimeParamValueType::boolean:
                    if (!param.defaultValue.isBool())
                        return juce::Result::fail("runtimeParams.defaultValue must be bool for boolean type");
                    break;
                case RuntimeParamValueType::string:
                    if (!param.defaultValue.isString())
                        return juce::Result::fail("runtimeParams.defaultValue must be string for string type");
                    break;
            }
        }

        return juce::Result::ok();
    }

    inline juce::Result validatePropertyBindings(const DocumentModel& document)
    {
        const auto isIdentifierLike = [](const juce::String& text) noexcept
        {
            const auto trimmed = text.trim();
            if (trimmed.isEmpty())
                return false;

            const auto isStart = [](juce::juce_wchar ch) noexcept
            {
                return (ch >= 'a' && ch <= 'z')
                    || (ch >= 'A' && ch <= 'Z')
                    || ch == '_';
            };

            const auto isBody = [isStart](juce::juce_wchar ch) noexcept
            {
                return isStart(ch)
                    || (ch >= '0' && ch <= '9')
                    || ch == '.';
            };

            if (!isStart(trimmed[0]))
                return false;

            for (int i = 1; i < trimmed.length(); ++i)
            {
                if (!isBody(trimmed[i]))
                    return false;
            }

            return true;
        };

        std::unordered_set<WidgetId> widgetIds;
        widgetIds.reserve(document.widgets.size());
        std::unordered_map<WidgetId, const WidgetModel*> widgetsById;
        widgetsById.reserve(document.widgets.size());
        for (const auto& widget : document.widgets)
        {
            widgetIds.insert(widget.id);
            widgetsById.emplace(widget.id, &widget);
        }

        std::map<juce::String, juce::var> runtimeParamValues;
        for (const auto& param : document.runtimeParams)
        {
            const auto key = param.key.trim();
            if (key.isNotEmpty() && runtimeParamValues.find(key) == runtimeParamValues.end())
                runtimeParamValues.emplace(key, param.defaultValue);
        }

        std::unordered_set<WidgetId> bindingIds;
        bindingIds.reserve(document.propertyBindings.size());

        for (const auto& binding : document.propertyBindings)
        {
            if (binding.id <= kRootId)
                return juce::Result::fail("propertyBindings.id must be > rootId");
            if (!bindingIds.insert(binding.id).second)
                return juce::Result::fail("propertyBindings.id must be unique");
            if (binding.targetWidgetId <= kRootId)
                return juce::Result::fail("propertyBindings.targetWidgetId must be > rootId");
            if (widgetIds.count(binding.targetWidgetId) == 0)
                return juce::Result::fail("propertyBindings.targetWidgetId not found");

            const auto widgetIt = widgetsById.find(binding.targetWidgetId);
            if (widgetIt == widgetsById.end() || widgetIt->second == nullptr)
                return juce::Result::fail("propertyBindings.targetWidgetId not found");
            const auto& targetWidget = *widgetIt->second;

            const auto targetProperty = binding.targetProperty.trim();
            if (targetProperty.isEmpty())
                return juce::Result::fail("propertyBindings.targetProperty must not be empty");
            if (!isIdentifierLike(targetProperty))
            {
                return juce::Result::fail("propertyBindings.targetProperty has invalid identifier for binding id "
                                          + juce::String(binding.id));
            }

            const auto expression = binding.expression.trim();
            if (expression.isEmpty())
                return juce::Result::fail("propertyBindings.expression must not be empty");

            const auto targetPropertyId = juce::Identifier(targetProperty);
            if (!targetWidget.properties.contains(targetPropertyId))
            {
                return juce::Result::fail("propertyBindings.targetProperty not found on target widget for binding id "
                                          + juce::String(binding.id));
            }

            const auto& currentValue = targetWidget.properties[targetPropertyId];
            if (!(currentValue.isBool() || isNumericVar(currentValue)))
            {
                return juce::Result::fail("propertyBindings.targetProperty type mismatch for binding id "
                                          + juce::String(binding.id)
                                          + " (number/integer/boolean only)");
            }

            const auto evaluation = Runtime::PropertyBindingResolver::evaluateExpression(expression, runtimeParamValues);
            if (!evaluation.success)
            {
                return juce::Result::fail("propertyBindings.expression invalid for binding id "
                                          + juce::String(binding.id)
                                          + ": " + evaluation.error);
            }
        }

        return juce::Result::ok();
    }

    inline std::vector<RuntimeBindingIssue> validateRuntimeBindings(const DocumentModel& document)
    {
        std::vector<RuntimeBindingIssue> issues;

        std::unordered_set<WidgetId> widgetIds;
        widgetIds.reserve(document.widgets.size());
        for (const auto& widget : document.widgets)
            widgetIds.insert(widget.id);

        std::unordered_set<WidgetId> groupIds;
        groupIds.reserve(document.groups.size());
        for (const auto& group : document.groups)
            groupIds.insert(group.id);

        std::unordered_set<WidgetId> layerIds;
        layerIds.reserve(document.layers.size());
        for (const auto& layer : document.layers)
            layerIds.insert(layer.id);

        std::unordered_set<WidgetId> bindingIds;
        bindingIds.reserve(document.runtimeBindings.size());

        auto pushIssue = [&issues](RuntimeBindingIssueSeverity severity,
                                   const juce::String& title,
                                   const juce::String& message)
        {
            RuntimeBindingIssue issue;
            issue.severity = severity;
            issue.title = title;
            issue.message = message;
            issues.push_back(std::move(issue));
        };

        auto hasNode = [&widgetIds, &groupIds, &layerIds](const NodeRef& node) noexcept
        {
            if (node.kind == NodeKind::widget)
                return widgetIds.count(node.id) > 0;
            if (node.kind == NodeKind::group)
                return groupIds.count(node.id) > 0;
            return layerIds.count(node.id) > 0;
        };

        for (const auto& binding : document.runtimeBindings)
        {
            const auto bindingLabel = "Binding id=" + juce::String(binding.id);
            if (binding.id <= kRootId)
            {
                pushIssue(RuntimeBindingIssueSeverity::error,
                          "Invalid binding id",
                          bindingLabel + " has invalid id");
            }
            else if (!bindingIds.insert(binding.id).second)
            {
                pushIssue(RuntimeBindingIssueSeverity::error,
                          "Duplicate binding id",
                          bindingLabel + " is duplicated");
            }

            if (binding.sourceWidgetId <= kRootId)
            {
                pushIssue(RuntimeBindingIssueSeverity::error,
                          "Invalid source widget id",
                          bindingLabel + " has invalid sourceWidgetId");
            }
            else if (widgetIds.count(binding.sourceWidgetId) == 0)
            {
                pushIssue(RuntimeBindingIssueSeverity::warning,
                          "Missing source widget",
                          bindingLabel + " references sourceWidgetId=" + juce::String(binding.sourceWidgetId)
                              + " which does not exist");
            }

            if (binding.eventKey.trim().isEmpty())
            {
                pushIssue(RuntimeBindingIssueSeverity::error,
                          "Missing event key",
                          bindingLabel + " has empty eventKey");
            }

            if (binding.actions.empty())
            {
                pushIssue(RuntimeBindingIssueSeverity::warning,
                          "Empty action chain",
                          bindingLabel + " has no actions");
            }

            for (size_t actionIndex = 0; actionIndex < binding.actions.size(); ++actionIndex)
            {
                const auto& action = binding.actions[actionIndex];
                const auto actionLabel = bindingLabel + " action#" + juce::String(static_cast<int>(actionIndex + 1));

                switch (action.kind)
                {
                    case RuntimeActionKind::setRuntimeParam:
                        if (action.paramKey.trim().isEmpty())
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " setRuntimeParam requires paramKey");
                        }
                        break;

                    case RuntimeActionKind::adjustRuntimeParam:
                        if (action.paramKey.trim().isEmpty())
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " adjustRuntimeParam requires paramKey");
                        }
                        if (!std::isfinite(action.delta))
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " adjustRuntimeParam.delta must be finite");
                        }
                        break;

                    case RuntimeActionKind::toggleRuntimeParam:
                        if (action.paramKey.trim().isEmpty())
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " toggleRuntimeParam requires paramKey");
                        }
                        break;

                    case RuntimeActionKind::setNodeProps:
                    {
                        if (action.target.id <= kRootId)
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " setNodeProps target.id must be > rootId");
                        }
                        else if (!hasNode(action.target))
                        {
                            pushIssue(RuntimeBindingIssueSeverity::warning,
                                      "Missing target node",
                                      actionLabel + " setNodeProps target does not exist");
                        }

                        if (action.opacity.has_value())
                        {
                            if (action.target.kind == NodeKind::layer)
                            {
                                pushIssue(RuntimeBindingIssueSeverity::error,
                                          "Invalid action payload",
                                          actionLabel + " setNodeProps.opacity is not allowed for layer");
                            }
                            else if (!isFiniteOpacity(*action.opacity))
                            {
                                pushIssue(RuntimeBindingIssueSeverity::error,
                                          "Invalid action payload",
                                          actionLabel + " setNodeProps.opacity must be within [0, 1]");
                            }
                        }

                        const auto patchResult = validatePropertyBag(action.patch);
                        if (patchResult.failed())
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " patch is invalid: " + patchResult.getErrorMessage());
                        }
                        break;
                    }

                    case RuntimeActionKind::setNodeBounds:
                    {
                        if (action.targetWidgetId <= kRootId)
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " setNodeBounds.targetWidgetId must be > rootId");
                        }
                        else if (widgetIds.count(action.targetWidgetId) == 0)
                        {
                            if (groupIds.count(action.targetWidgetId) > 0 || layerIds.count(action.targetWidgetId) > 0)
                            {
                                pushIssue(RuntimeBindingIssueSeverity::error,
                                          "Invalid action payload",
                                          actionLabel + " setNodeBounds target must be widget");
                            }
                            else
                            {
                                pushIssue(RuntimeBindingIssueSeverity::warning,
                                          "Missing target widget",
                                          actionLabel + " setNodeBounds target does not exist");
                            }
                        }

                        if (!isFiniteRectNonNegative(action.bounds))
                        {
                            pushIssue(RuntimeBindingIssueSeverity::error,
                                      "Invalid action payload",
                                      actionLabel + " setNodeBounds.bounds must be finite and non-negative");
                        }
                        break;
                    }
                }
            }
        }

        return issues;
    }

    inline juce::Result validateScene(const DocumentModel& document,
                                      const EditorStateModel* editorState = nullptr)
    {
        const auto documentResult = validateDocument(document);
        if (documentResult.failed())
            return documentResult;

        if (editorState != nullptr)
        {
            const auto editorStateResult = validateEditorState(document, *editorState);
            if (editorStateResult.failed())
                return editorStateResult;
        }

        const auto runtimeParamsResult = validateRuntimeParams(document);
        if (runtimeParamsResult.failed())
            return runtimeParamsResult;

        const auto propertyBindingsResult = validatePropertyBindings(document);
        if (propertyBindingsResult.failed())
            return propertyBindingsResult;

        const auto runtimeIssues = validateRuntimeBindings(document);
        for (const auto& issue : runtimeIssues)
        {
            if (issue.severity == RuntimeBindingIssueSeverity::error)
                return juce::Result::fail(issue.title + ": " + issue.message);
        }

        return juce::Result::ok();
    }
}
