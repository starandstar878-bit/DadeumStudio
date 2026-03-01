
#include "Gyeol/Editor/Panels/PropertyPanel.h"
#include "Gyeol/Editor/Panels/PropertyEditorFactory.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        constexpr float kValueEpsilon = 0.0001f;

        juce::String widgetTypeLabel(const Widgets::WidgetFactory& widgetFactory, WidgetType type)
        {
            if (const auto* descriptor = widgetFactory.descriptorFor(type); descriptor != nullptr)
            {
                if (descriptor->displayName.isNotEmpty())
                    return descriptor->displayName;
                if (descriptor->typeKey.isNotEmpty())
                    return descriptor->typeKey;
            }

            return "Widget";
        }

        bool floatNearlyEqual(float lhs, float rhs) noexcept
        {
            return std::abs(lhs - rhs) <= kValueEpsilon;
        }

        juce::Rectangle<float> unionRect(const juce::Rectangle<float>& lhs, const juce::Rectangle<float>& rhs)
        {
            const auto left = std::min(lhs.getX(), rhs.getX());
            const auto top = std::min(lhs.getY(), rhs.getY());
            const auto right = std::max(lhs.getRight(), rhs.getRight());
            const auto bottom = std::max(lhs.getBottom(), rhs.getBottom());
            return { left, top, right - left, bottom - top };
        }

        std::optional<float> toFloat(const juce::var& value)
        {
            if (value.isInt() || value.isInt64() || value.isDouble())
            {
                const auto numeric = static_cast<float>(static_cast<double>(value));
                if (std::isfinite(numeric))
                    return numeric;
            }

            return std::nullopt;
        }
    }

    PropertyPanel::PropertyPanel(DocumentHandle& documentIn, const Widgets::WidgetFactory& widgetFactoryIn)
        : document(documentIn),
          widgetFactory(widgetFactoryIn)
    {
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(220, 228, 236));
        addAndMakeVisible(titleLabel);

        subtitleLabel.setJustificationType(juce::Justification::centredLeft);
        subtitleLabel.setFont(juce::FontOptions(11.0f));
        subtitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 170, 184));
        addAndMakeVisible(subtitleLabel);

        showAdvancedToggle.setClickingTogglesState(true);
        showAdvancedToggle.onClick = [this]
        {
            showAdvancedProperties = showAdvancedToggle.getToggleState();
            rebuildContent();
        };
        showAdvancedToggle.setVisible(false);
        addAndMakeVisible(showAdvancedToggle);

        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        rebuildContent();
    }

    PropertyPanel::~PropertyPanel()
    {
        if (activeEditKey.isNotEmpty())
            document.endCoalescedEdit(activeEditKey, true);
        viewport.setViewedComponent(nullptr, false);
    }

    void PropertyPanel::setInspectorTarget(InspectorTarget target)
    {
        std::sort(target.widgetIds.begin(), target.widgetIds.end());
        target.widgetIds.erase(std::unique(target.widgetIds.begin(), target.widgetIds.end()), target.widgetIds.end());

        const auto same = inspectorTarget.kind == target.kind
                       && inspectorTarget.nodeId == target.nodeId
                       && inspectorTarget.widgetIds == target.widgetIds;
        if (same)
            return;

        if (activeEditKey.isNotEmpty())
            commitEditSession(activeEditKey);

        showAdvancedProperties = false;
        showAdvancedToggle.setToggleState(false, juce::dontSendNotification);
        inspectorTarget = std::move(target);
        rebuildContent();
    }

    void PropertyPanel::setCommitCallbacks(CommitCallbacks callbacks)
    {
        commitCallbacks = std::move(callbacks);
    }

    void PropertyPanel::refreshFromDocument()
    {
        // Keep active text editors alive during coalesced preview.
        if (activeEditKey.isNotEmpty())
            return;

        rebuildContent();
    }

    void PropertyPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(38, 45, 56));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);
    }

    void PropertyPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(6);
        titleLabel.setBounds(bounds.removeFromTop(20));
        subtitleLabel.setBounds(bounds.removeFromTop(18));

        if (showAdvancedToggle.isVisible())
        {
            showAdvancedToggle.setBounds(bounds.removeFromTop(22));
            bounds.removeFromTop(4);
        }
        else
        {
            showAdvancedToggle.setBounds({});
        }

        viewport.setBounds(bounds);
        layoutContent();
    }

    void PropertyPanel::resetContent()
    {
        layoutEntries.clear();
        ownedEditors.clear();
        ownedLabels.clear();
        content.removeAllChildren();
    }

    void PropertyPanel::layoutContent()
    {
        const auto width = std::max(40, viewport.getWidth() - 8);
        int y = 4;

        for (const auto& entry : layoutEntries)
        {
            if (entry.fullWidth)
            {
                if (entry.left != nullptr)
                    entry.left->setBounds(6, y, width - 12, entry.height);
            }
            else
            {
                constexpr int labelWidth = 80;
                constexpr int gap = 6;
                if (entry.left != nullptr)
                    entry.left->setBounds(6, y + 2, labelWidth - 8, entry.height - 4);
                if (entry.right != nullptr)
                    entry.right->setBounds(6 + labelWidth + gap, y, std::max(20, width - labelWidth - gap - 12), entry.height);
            }

            y += entry.height + 4;
        }

        content.setSize(width, std::max(y + 4, viewport.getHeight()));
    }
    void PropertyPanel::addSectionHeader(const juce::String& text)
    {
        auto header = std::make_unique<juce::Label>();
        header->setText(text, juce::dontSendNotification);
        header->setFont(juce::FontOptions(11.5f, juce::Font::bold));
        header->setColour(juce::Label::textColourId, juce::Colour::fromRGB(186, 198, 214));
        header->setJustificationType(juce::Justification::centredLeft);
        content.addAndMakeVisible(*header);

        LayoutEntry entry;
        entry.left = header.get();
        entry.height = 20;
        entry.fullWidth = true;
        layoutEntries.push_back(entry);
        ownedLabels.push_back(std::move(header));
    }

    void PropertyPanel::addInfoRow(const juce::String& label, const juce::String& value)
    {
        Widgets::WidgetPropertySpec spec;
        spec.key = juce::Identifier("info." + label);
        spec.label = label;
        spec.kind = Widgets::WidgetPropertyKind::text;
        spec.uiHint = Widgets::WidgetPropertyUiHint::lineEdit;
        spec.readOnly = true;

        ValueState state;
        state.valid = true;
        state.value = value;
        addEditorRow(spec, state, {}, {}, {});
    }

    void PropertyPanel::addExpanderRow(const juce::String& label,
                                       bool expanded,
                                       std::function<void()> onToggle)
    {
        auto button = std::make_unique<juce::TextButton>();
        button->setButtonText((expanded ? "v " : "> ") + label);
        button->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(28, 34, 44));
        button->setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(28, 34, 44));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(185, 195, 210));
        button->setColour(juce::TextButton::textColourOnId, juce::Colour::fromRGB(185, 195, 210));
        button->setTriggeredOnMouseDown(false);
        button->onClick = std::move(onToggle);
        content.addAndMakeVisible(*button);

        LayoutEntry entry;
        entry.left = button.get();
        entry.height = 24;
        entry.fullWidth = true;
        layoutEntries.push_back(entry);
        ownedEditors.push_back(std::move(button));
    }

    void PropertyPanel::addEditorRow(const Widgets::WidgetPropertySpec& spec,
                                     const ValueState& valueState,
                                     std::function<void(const juce::var&)> onPreview,
                                     std::function<void(const juce::var&)> onCommit,
                                     std::function<void()> onCancel)
    {
        auto label = std::make_unique<juce::Label>();
        label->setText(spec.label, juce::dontSendNotification);
        label->setFont(juce::FontOptions(11.0f));
        label->setColour(juce::Label::textColourId, juce::Colour::fromRGB(185, 195, 210));
        label->setJustificationType(juce::Justification::centredLeft);
        content.addAndMakeVisible(*label);

        EditorBuildSpec buildSpec;
        buildSpec.spec = spec;
        buildSpec.value = valueState.valid ? valueState.value : spec.defaultValue;
        buildSpec.mixed = valueState.mixed;
        buildSpec.readOnly = spec.readOnly;
        buildSpec.onPreview = std::move(onPreview);
        buildSpec.onCommit = std::move(onCommit);
        buildSpec.onCancel = std::move(onCancel);

        auto editor = PropertyEditorFactory::createEditor(buildSpec);
        content.addAndMakeVisible(*editor);

        LayoutEntry entry;
        entry.left = label.get();
        entry.right = editor.get();
        entry.height = 24;
        entry.fullWidth = false;
        layoutEntries.push_back(entry);

        ownedLabels.push_back(std::move(label));
        ownedEditors.push_back(std::move(editor));
    }

    PropertyPanel::ValueState PropertyPanel::makeBooleanState(const std::vector<bool>& values) const
    {
        ValueState state;
        if (values.empty())
            return state;

        state.valid = true;
        state.value = values.front();
        state.mixed = std::any_of(values.begin(), values.end(), [first = values.front()](bool value) { return value != first; });
        return state;
    }

    PropertyPanel::ValueState PropertyPanel::makeFloatState(const std::vector<float>& values) const
    {
        ValueState state;
        if (values.empty())
            return state;

        state.valid = true;
        state.value = values.front();
        state.mixed = std::any_of(values.begin(), values.end(), [first = values.front()](float value) { return !floatNearlyEqual(value, first); });
        return state;
    }

    PropertyPanel::ValueState PropertyPanel::makeStringState(const std::vector<juce::String>& values) const
    {
        ValueState state;
        if (values.empty())
            return state;

        state.valid = true;
        state.value = values.front();
        state.mixed = std::any_of(values.begin(), values.end(), [first = values.front()](const juce::String& value) { return value != first; });
        return state;
    }

    std::optional<PropertyPanel::WidgetRef> PropertyPanel::findWidgetRef(WidgetId id) const
    {
        const auto& widgets = document.snapshot().widgets;
        const auto it = std::find_if(widgets.begin(), widgets.end(), [id](const WidgetModel& widget) { return widget.id == id; });
        if (it == widgets.end())
            return std::nullopt;

        WidgetRef ref;
        ref.id = it->id;
        ref.type = it->type;
        ref.bounds = it->bounds;
        ref.properties = it->properties;
        ref.visible = it->visible;
        ref.locked = it->locked;
        ref.opacity = it->opacity;
        return ref;
    }

    const GroupModel* PropertyPanel::findGroup(WidgetId id) const
    {
        const auto& groups = document.snapshot().groups;
        const auto it = std::find_if(groups.begin(), groups.end(), [id](const GroupModel& group) { return group.id == id; });
        return it == groups.end() ? nullptr : &(*it);
    }

    const LayerModel* PropertyPanel::findLayer(WidgetId id) const
    {
        const auto& layers = document.snapshot().layers;
        const auto it = std::find_if(layers.begin(), layers.end(), [id](const LayerModel& layer) { return layer.id == id; });
        return it == layers.end() ? nullptr : &(*it);
    }

    bool PropertyPanel::varEquals(const juce::var& lhs, const juce::var& rhs) const
    {
        if (isNumericVar(lhs) && isNumericVar(rhs))
            return std::abs(static_cast<double>(lhs) - static_cast<double>(rhs)) <= 0.000001;

        if (lhs.isObject() || rhs.isObject())
            return juce::JSON::toString(lhs) == juce::JSON::toString(rhs);

        return lhs.equalsWithSameType(rhs);
    }

    bool PropertyPanel::beginEditSession(const juce::String& editKey)
    {
        if (editKey.isEmpty())
            return false;
        if (activeEditKey == editKey)
            return true;

        if (activeEditKey.isNotEmpty())
            commitEditSession(activeEditKey);

        if (!document.beginCoalescedEdit(editKey))
            return false;

        activeEditKey = editKey;
        return true;
    }
    void PropertyPanel::commitEditSession(const juce::String& editKey)
    {
        if (editKey.isEmpty() || activeEditKey != editKey)
            return;

        document.endCoalescedEdit(editKey, true);
        activeEditKey.clear();

        if (commitCallbacks.onCommitted)
            commitCallbacks.onCommitted();
    }

    void PropertyPanel::cancelEditSession(const juce::String& editKey)
    {
        if (editKey.isEmpty() || activeEditKey != editKey)
            return;

        const auto canceled = document.endCoalescedEdit(editKey, false);
        activeEditKey.clear();

        if (canceled && commitCallbacks.onPreviewApplied)
            commitCallbacks.onPreviewApplied();

        if (commitCallbacks.onCommitted)
            commitCallbacks.onCommitted();
    }

    bool PropertyPanel::applySetPropsPreview(const SetPropsAction& action, const juce::String& editKey)
    {
        if (!beginEditSession(editKey))
            return false;

        const auto ok = commitCallbacks.onSetPropsPreview ? commitCallbacks.onSetPropsPreview(action).wasOk()
                                                          : document.previewSetProps(action);
        if (!ok)
            return false;

        if (commitCallbacks.onPreviewApplied)
            commitCallbacks.onPreviewApplied();

        return true;
    }

    bool PropertyPanel::applySetBoundsPreview(const SetBoundsAction& action, const juce::String& editKey)
    {
        if (!beginEditSession(editKey))
            return false;

        const auto ok = commitCallbacks.onSetBoundsPreview ? commitCallbacks.onSetBoundsPreview(action).wasOk()
                                                           : document.previewSetBounds(action);
        if (!ok)
            return false;

        if (commitCallbacks.onPreviewApplied)
            commitCallbacks.onPreviewApplied();

        return true;
    }

    std::vector<PropertyPanel::WidgetRef> PropertyPanel::resolveTargetWidgets() const
    {
        std::vector<WidgetRef> refs;

        if (inspectorTarget.kind == InspectorTargetKind::widgetSingle)
        {
            if (const auto ref = findWidgetRef(inspectorTarget.nodeId); ref.has_value())
                refs.push_back(*ref);
            return refs;
        }

        if (inspectorTarget.kind == InspectorTargetKind::widgetMulti)
        {
            for (const auto id : inspectorTarget.widgetIds)
            {
                if (const auto ref = findWidgetRef(id); ref.has_value())
                    refs.push_back(*ref);
            }
            return refs;
        }

        if (inspectorTarget.kind == InspectorTargetKind::group)
        {
            const auto widgetIds = collectGroupWidgetIdsRecursive(inspectorTarget.nodeId);
            for (const auto id : widgetIds)
            {
                if (const auto ref = findWidgetRef(id); ref.has_value())
                    refs.push_back(*ref);
            }
        }

        return refs;
    }

    std::vector<WidgetId> PropertyPanel::collectGroupWidgetIdsRecursive(WidgetId groupId) const
    {
        std::vector<WidgetId> outIds;
        std::unordered_set<WidgetId> visitedGroups;
        std::unordered_set<WidgetId> seenWidgets;

        const std::function<void(WidgetId)> collect = [&](WidgetId id)
        {
            if (!visitedGroups.insert(id).second)
                return;

            const auto* group = findGroup(id);
            if (group == nullptr)
                return;

            for (const auto widgetId : group->memberWidgetIds)
            {
                if (seenWidgets.insert(widgetId).second)
                    outIds.push_back(widgetId);
            }

            for (const auto& child : document.snapshot().groups)
            {
                if (child.parentGroupId.has_value() && *child.parentGroupId == id)
                    collect(child.id);
            }
        };

        collect(groupId);
        return outIds;
    }

    std::optional<juce::Rectangle<float>> PropertyPanel::computeUnionBounds(const std::vector<WidgetId>& widgetIds) const
    {
        std::optional<juce::Rectangle<float>> result;
        for (const auto id : widgetIds)
        {
            const auto ref = findWidgetRef(id);
            if (!ref.has_value())
                continue;

            result = result.has_value() ? unionRect(*result, ref->bounds) : ref->bounds;
        }

        return result;
    }

    std::optional<juce::Rectangle<float>> PropertyPanel::resolveCurrentTransformBounds() const
    {
        if (inspectorTarget.kind == InspectorTargetKind::widgetSingle)
        {
            const auto ref = findWidgetRef(inspectorTarget.nodeId);
            return ref.has_value() ? std::optional<juce::Rectangle<float>> { ref->bounds } : std::nullopt;
        }

        if (inspectorTarget.kind == InspectorTargetKind::widgetMulti)
            return computeUnionBounds(inspectorTarget.widgetIds);

        if (inspectorTarget.kind == InspectorTargetKind::group)
            return computeUnionBounds(collectGroupWidgetIdsRecursive(inspectorTarget.nodeId));

        return std::nullopt;
    }

    juce::Rectangle<float> PropertyPanel::clampWidgetBounds(const WidgetRef& widget, juce::Rectangle<float> bounds) const
    {
        const auto minSize = widgetFactory.minSizeFor(widget.type);
        bounds.setWidth(std::max(minSize.x, bounds.getWidth()));
        bounds.setHeight(std::max(minSize.y, bounds.getHeight()));
        bounds.setWidth(std::min(bounds.getWidth(), canvasWidth));
        bounds.setHeight(std::min(bounds.getHeight(), canvasHeight));
        bounds.setX(juce::jlimit(0.0f, canvasWidth - bounds.getWidth(), bounds.getX()));
        bounds.setY(juce::jlimit(0.0f, canvasHeight - bounds.getHeight(), bounds.getY()));
        return bounds;
    }
    bool PropertyPanel::buildScaledBoundsUpdates(const std::vector<WidgetRef>& widgets,
                                                 const juce::Rectangle<float>& sourceUnion,
                                                 const juce::Rectangle<float>& targetUnion,
                                                 std::vector<SetBoundsAction::Item>& updates) const
    {
        constexpr float epsilon = 0.0001f;
        const auto sourceW = sourceUnion.getWidth();
        const auto sourceH = sourceUnion.getHeight();
        const auto targetW = std::max(1.0f, targetUnion.getWidth());
        const auto targetH = std::max(1.0f, targetUnion.getHeight());
        const auto canScaleX = sourceW > epsilon;
        const auto canScaleY = sourceH > epsilon;

        updates.clear();
        updates.reserve(widgets.size());

        for (const auto& widget : widgets)
        {
            auto next = widget.bounds;

            if (canScaleX)
            {
                const auto normLeft = (widget.bounds.getX() - sourceUnion.getX()) / sourceW;
                const auto normWidth = widget.bounds.getWidth() / sourceW;
                next.setX(targetUnion.getX() + normLeft * targetW);
                next.setWidth(normWidth * targetW);
            }
            else
            {
                next.setX(widget.bounds.getX() + (targetUnion.getX() - sourceUnion.getX()));
            }

            if (canScaleY)
            {
                const auto normTop = (widget.bounds.getY() - sourceUnion.getY()) / sourceH;
                const auto normHeight = widget.bounds.getHeight() / sourceH;
                next.setY(targetUnion.getY() + normTop * targetH);
                next.setHeight(normHeight * targetH);
            }
            else
            {
                next.setY(widget.bounds.getY() + (targetUnion.getY() - sourceUnion.getY()));
            }

            next = clampWidgetBounds(widget, next);
            if (std::abs(next.getX() - widget.bounds.getX()) <= kValueEpsilon
                && std::abs(next.getY() - widget.bounds.getY()) <= kValueEpsilon
                && std::abs(next.getWidth() - widget.bounds.getWidth()) <= kValueEpsilon
                && std::abs(next.getHeight() - widget.bounds.getHeight()) <= kValueEpsilon)
            {
                continue;
            }

            SetBoundsAction::Item item;
            item.id = widget.id;
            item.bounds = next;
            updates.push_back(item);
        }

        return true;
    }

    bool PropertyPanel::applyTransformPreview(const juce::Rectangle<float>& targetBounds, const juce::String& editKey)
    {
        if (inspectorTarget.kind == InspectorTargetKind::layer)
            return false;

        const auto widgets = resolveTargetWidgets();
        const auto sourceUnion = resolveCurrentTransformBounds();
        if (widgets.empty() || !sourceUnion.has_value())
            return false;

        if (inspectorTarget.kind == InspectorTargetKind::group && commitCallbacks.onGroupTransformPreview)
        {
            if (!beginEditSession(editKey))
                return false;

            const auto result = commitCallbacks.onGroupTransformPreview(inspectorTarget.nodeId, targetBounds);
            if (result.failed())
                return false;

            if (commitCallbacks.onPreviewApplied)
                commitCallbacks.onPreviewApplied();
            return true;
        }

        std::vector<SetBoundsAction::Item> updates;
        if (!buildScaledBoundsUpdates(widgets, *sourceUnion, targetBounds, updates))
            return false;
        if (updates.empty())
            return true;

        SetBoundsAction action;
        action.items = std::move(updates);
        return applySetBoundsPreview(action, editKey);
    }

    PropertyPanel::ValueState PropertyPanel::makeWidgetPropertyState(const std::vector<WidgetRef>& widgets,
                                                                     const juce::Identifier& key) const
    {
        ValueState state;
        if (widgets.empty())
            return state;

        bool hasFirst = false;
        juce::var firstValue;

        for (const auto& widget : widgets)
        {
            juce::var value;
            if (widget.properties.contains(key))
            {
                value = widget.properties[key];
            }
            else if (const auto* descriptor = widgetFactory.descriptorFor(widget.type);
                     descriptor != nullptr && descriptor->defaultProperties.contains(key))
            {
                value = descriptor->defaultProperties[key];
            }
            else
            {
                return state;
            }

            if (!hasFirst)
            {
                firstValue = value;
                hasFirst = true;
            }
            else if (!varEquals(firstValue, value))
            {
                state.mixed = true;
            }
        }

        state.valid = hasFirst;
        state.value = firstValue;
        return state;
    }

    std::vector<Widgets::WidgetPropertySpec> PropertyPanel::commonPropertySpecs(const std::vector<WidgetRef>& widgets) const
    {
        if (widgets.empty())
            return {};

        const auto* firstDescriptor = widgetFactory.descriptorFor(widgets.front().type);
        if (firstDescriptor == nullptr)
            return {};

        std::unordered_map<juce::String, Widgets::WidgetPropertySpec> common;
        for (const auto& spec : firstDescriptor->propertySpecs)
            common.emplace(spec.key.toString(), spec);

        for (size_t i = 1; i < widgets.size(); ++i)
        {
            const auto* descriptor = widgetFactory.descriptorFor(widgets[i].type);
            if (descriptor == nullptr)
            {
                common.clear();
                break;
            }

            std::unordered_map<juce::String, Widgets::WidgetPropertyKind> kinds;
            for (const auto& spec : descriptor->propertySpecs)
                kinds.emplace(spec.key.toString(), spec.kind);

            for (auto it = common.begin(); it != common.end();)
            {
                const auto found = kinds.find(it->first);
                if (found == kinds.end() || found->second != it->second.kind)
                    it = common.erase(it);
                else
                    ++it;
            }
        }

        std::vector<Widgets::WidgetPropertySpec> specs;
        specs.reserve(common.size());
        for (const auto& [_, spec] : common)
            specs.push_back(spec);

        std::sort(specs.begin(), specs.end(), [](const Widgets::WidgetPropertySpec& lhs, const Widgets::WidgetPropertySpec& rhs)
        {
            if (lhs.group != rhs.group)
                return lhs.group < rhs.group;
            if (lhs.order != rhs.order)
                return lhs.order < rhs.order;
            return lhs.label < rhs.label;
        });

        return specs;
    }
    void PropertyPanel::buildNoneContent()
    {
        addSectionHeader("Inspector");
        addInfoRow("Target", "No selection");
        showAdvancedToggle.setVisible(false);
    }

    void PropertyPanel::buildLayerContent(const LayerModel& layer)
    {
        addSectionHeader("Common");

        const auto addLayerText = [this, layerId = layer.id](const juce::String& label,
                                                              const juce::String& value,
                                                              const juce::String& keyPrefix,
                                                              std::function<void(LayerPropsPatch&, const juce::var&)> assign)
        {
            Widgets::WidgetPropertySpec spec;
            spec.key = juce::Identifier(keyPrefix);
            spec.label = label;
            spec.kind = Widgets::WidgetPropertyKind::text;
            spec.uiHint = Widgets::WidgetPropertyUiHint::lineEdit;

            ValueState state;
            state.valid = true;
            state.value = value;

            const auto editKey = keyPrefix + ":" + juce::String(layerId);
            auto apply = [this, layerId, assign, editKey](const juce::var& nextValue) -> bool
            {
                SetPropsAction action;
                action.kind = NodeKind::layer;
                action.ids = { layerId };
                LayerPropsPatch patch;
                assign(patch, nextValue);
                action.patch = std::move(patch);
                return applySetPropsPreview(action, editKey);
            };

            addEditorRow(spec,
                         state,
                         [apply](const juce::var& nextValue) { apply(nextValue); },
                         [this, apply, editKey](const juce::var& nextValue)
                         {
                             if (apply(nextValue))
                                 commitEditSession(editKey);
                         },
                         [this, editKey]
                         {
                             cancelEditSession(editKey);
                             rebuildContent();
                         });
        };

        addLayerText("Name", layer.name, "layer.name", [](LayerPropsPatch& patch, const juce::var& value) { patch.name = value.toString(); });

        const auto addLayerBool = [this, layerId = layer.id](const juce::String& label,
                                                              bool current,
                                                              const juce::String& keyPrefix,
                                                              std::function<void(LayerPropsPatch&, bool)> assign)
        {
            Widgets::WidgetPropertySpec spec;
            spec.key = juce::Identifier(keyPrefix);
            spec.label = label;
            spec.kind = Widgets::WidgetPropertyKind::boolean;
            spec.uiHint = Widgets::WidgetPropertyUiHint::toggle;

            ValueState state;
            state.valid = true;
            state.value = current;

            const auto editKey = keyPrefix + ":" + juce::String(layerId);
            auto apply = [this, layerId, assign, editKey](const juce::var& nextValue) -> bool
            {
                SetPropsAction action;
                action.kind = NodeKind::layer;
                action.ids = { layerId };
                LayerPropsPatch patch;
                assign(patch, static_cast<bool>(nextValue));
                action.patch = std::move(patch);
                return applySetPropsPreview(action, editKey);
            };

            addEditorRow(spec,
                         state,
                         [apply](const juce::var& nextValue) { apply(nextValue); },
                         [this, apply, editKey](const juce::var& nextValue)
                         {
                             if (apply(nextValue))
                                 commitEditSession(editKey);
                         },
                         [this, editKey]
                         {
                             cancelEditSession(editKey);
                             rebuildContent();
                         });
        };

        addLayerBool("Visible", layer.visible, "layer.visible", [](LayerPropsPatch& patch, bool value) { patch.visible = value; });
        addLayerBool("Locked", layer.locked, "layer.locked", [](LayerPropsPatch& patch, bool value) { patch.locked = value; });

        showAdvancedToggle.setVisible(false);
    }

    void PropertyPanel::buildGroupContent(const GroupModel& group)
    {
        addSectionHeader("Common");

        const auto addGroupBool = [this, groupId = group.id](const juce::String& label,
                                                              bool current,
                                                              const juce::String& keyPrefix,
                                                              std::function<void(GroupPropsPatch&, bool)> assign)
        {
            Widgets::WidgetPropertySpec spec;
            spec.key = juce::Identifier(keyPrefix);
            spec.label = label;
            spec.kind = Widgets::WidgetPropertyKind::boolean;
            spec.uiHint = Widgets::WidgetPropertyUiHint::toggle;

            ValueState state;
            state.valid = true;
            state.value = current;

            const auto editKey = keyPrefix + ":" + juce::String(groupId);
            auto apply = [this, groupId, assign, editKey](const juce::var& nextValue) -> bool
            {
                SetPropsAction action;
                action.kind = NodeKind::group;
                action.ids = { groupId };
                GroupPropsPatch patch;
                assign(patch, static_cast<bool>(nextValue));
                action.patch = std::move(patch);
                return applySetPropsPreview(action, editKey);
            };

            addEditorRow(spec,
                         state,
                         [apply](const juce::var& nextValue) { apply(nextValue); },
                         [this, apply, editKey](const juce::var& nextValue)
                         {
                             if (apply(nextValue))
                                 commitEditSession(editKey);
                         },
                         [this, editKey]
                         {
                             cancelEditSession(editKey);
                             rebuildContent();
                         });
        };

        Widgets::WidgetPropertySpec nameSpec;
        nameSpec.key = juce::Identifier("group.name");
        nameSpec.label = "Name";
        nameSpec.kind = Widgets::WidgetPropertyKind::text;
        nameSpec.uiHint = Widgets::WidgetPropertyUiHint::lineEdit;

        ValueState nameState;
        nameState.valid = true;
        nameState.value = group.name;

        const auto groupNameKey = "group.name:" + juce::String(group.id);
        auto applyGroupName = [this, groupId = group.id, groupNameKey](const juce::var& nextValue) -> bool
        {
            SetPropsAction action;
            action.kind = NodeKind::group;
            action.ids = { groupId };
            GroupPropsPatch patch;
            patch.name = nextValue.toString();
            action.patch = std::move(patch);
            return applySetPropsPreview(action, groupNameKey);
        };

        addEditorRow(nameSpec,
                     nameState,
                     [applyGroupName](const juce::var& nextValue) { applyGroupName(nextValue); },
                     [this, applyGroupName, groupNameKey](const juce::var& nextValue)
                     {
                         if (applyGroupName(nextValue))
                             commitEditSession(groupNameKey);
                     },
                     [this, groupNameKey]
                     {
                         cancelEditSession(groupNameKey);
                         rebuildContent();
                     });

        addGroupBool("Visible", group.visible, "group.visible", [](GroupPropsPatch& patch, bool value) { patch.visible = value; });
        addGroupBool("Locked", group.locked, "group.locked", [](GroupPropsPatch& patch, bool value) { patch.locked = value; });
        addSectionHeader("Transform");
        if (const auto transform = resolveCurrentTransformBounds(); transform.has_value())
        {
            const auto addTransformField = [this, groupId = group.id](const juce::String& label, int axisIndex, float current)
            {
                Widgets::WidgetPropertySpec spec;
                spec.key = juce::Identifier("group.transform." + label.toLowerCase());
                spec.label = label;
                spec.kind = Widgets::WidgetPropertyKind::number;
                spec.uiHint = Widgets::WidgetPropertyUiHint::spinBox;
                spec.decimals = 2;
                if (axisIndex >= 2)
                    spec.minValue = 1.0;

                ValueState state;
                state.valid = true;
                state.value = current;

                const auto editKey = "group.transform." + juce::String(axisIndex) + ":" + juce::String(groupId);
                auto apply = [this, axisIndex, editKey](const juce::var& nextValue) -> bool
                {
                    const auto numeric = toFloat(nextValue);
                    const auto currentBounds = resolveCurrentTransformBounds();
                    if (!numeric.has_value() || !currentBounds.has_value())
                        return false;

                    auto target = *currentBounds;
                    const auto applied = axisIndex >= 2 ? std::max(1.0f, *numeric) : *numeric;
                    if (axisIndex == 0) target.setX(applied);
                    else if (axisIndex == 1) target.setY(applied);
                    else if (axisIndex == 2) target.setWidth(applied);
                    else target.setHeight(applied);
                    return applyTransformPreview(target, editKey);
                };

                addEditorRow(spec,
                             state,
                             [apply](const juce::var& nextValue) { apply(nextValue); },
                             [this, apply, editKey](const juce::var& nextValue)
                             {
                                 if (apply(nextValue))
                                     commitEditSession(editKey);
                             },
                             [this, editKey]
                             {
                                 cancelEditSession(editKey);
                                 rebuildContent();
                             });
            };

            addTransformField("X", 0, transform->getX());
            addTransformField("Y", 1, transform->getY());
            addTransformField("W", 2, transform->getWidth());
            addTransformField("H", 3, transform->getHeight());
        }
        else
        {
            addInfoRow("Transform", "No members");
        }

        addSectionHeader("Appearance");
        Widgets::WidgetPropertySpec opacitySpec;
        opacitySpec.key = juce::Identifier("group.opacity");
        opacitySpec.label = "Opacity";
        opacitySpec.kind = Widgets::WidgetPropertyKind::number;
        opacitySpec.uiHint = Widgets::WidgetPropertyUiHint::slider;
        opacitySpec.minValue = 0.0;
        opacitySpec.maxValue = 1.0;
        opacitySpec.step = 0.01;
        opacitySpec.decimals = 3;

        ValueState opacityState;
        opacityState.valid = true;
        opacityState.value = group.opacity;

        const auto groupOpacityKey = "group.opacity:" + juce::String(group.id);
        auto applyGroupOpacity = [this, groupId = group.id, groupOpacityKey](const juce::var& nextValue) -> bool
        {
            const auto numeric = toFloat(nextValue);
            if (!numeric.has_value())
                return false;

            SetPropsAction action;
            action.kind = NodeKind::group;
            action.ids = { groupId };
            GroupPropsPatch patch;
            patch.opacity = juce::jlimit(0.0f, 1.0f, *numeric);
            action.patch = std::move(patch);
            return applySetPropsPreview(action, groupOpacityKey);
        };

        addEditorRow(opacitySpec,
                     opacityState,
                     [applyGroupOpacity](const juce::var& nextValue) { applyGroupOpacity(nextValue); },
                     [this, applyGroupOpacity, groupOpacityKey](const juce::var& nextValue)
                     {
                         if (applyGroupOpacity(nextValue))
                             commitEditSession(groupOpacityKey);
                     },
                     [this, groupOpacityKey]
                     {
                         cancelEditSession(groupOpacityKey);
                         rebuildContent();
                     });

        showAdvancedToggle.setVisible(false);
    }

    void PropertyPanel::buildWidgetContent(const std::vector<WidgetRef>& widgets, bool multiSelection)
    {
        if (widgets.empty())
        {
            buildNoneContent();
            return;
        }

        const auto ids = inspectorTarget.widgetIds.empty() ? std::vector<WidgetId> { widgets.front().id } : inspectorTarget.widgetIds;

        addSectionHeader("Common");
        std::vector<bool> visibleValues;
        std::vector<bool> lockedValues;
        std::vector<float> opacityValues;
        for (const auto& widget : widgets)
        {
            visibleValues.push_back(widget.visible);
            lockedValues.push_back(widget.locked);
            opacityValues.push_back(widget.opacity);
        }

        const auto addWidgetBool = [this, ids](const juce::String& label,
                                               const ValueState& state,
                                               const juce::String& keyPrefix,
                                               std::function<void(WidgetPropsPatch&, bool)> assign)
        {
            Widgets::WidgetPropertySpec spec;
            spec.key = juce::Identifier(keyPrefix);
            spec.label = label;
            spec.kind = Widgets::WidgetPropertyKind::boolean;
            spec.uiHint = Widgets::WidgetPropertyUiHint::toggle;

            const auto editKey = keyPrefix + ":" + juce::String(ids.front());
            auto apply = [this, ids, assign, editKey](const juce::var& nextValue) -> bool
            {
                SetPropsAction action;
                action.kind = NodeKind::widget;
                action.ids = ids;
                WidgetPropsPatch patch;
                assign(patch, static_cast<bool>(nextValue));
                action.patch = std::move(patch);
                return applySetPropsPreview(action, editKey);
            };

            addEditorRow(spec,
                         state,
                         [apply](const juce::var& nextValue) { apply(nextValue); },
                         [this, apply, editKey](const juce::var& nextValue)
                         {
                             if (apply(nextValue))
                                 commitEditSession(editKey);
                         },
                         [this, editKey]
                         {
                             cancelEditSession(editKey);
                             rebuildContent();
                         });
        };

        addWidgetBool("Visible", makeBooleanState(visibleValues), "widget.visible", [](WidgetPropsPatch& patch, bool value) { patch.visible = value; });
        addWidgetBool("Locked", makeBooleanState(lockedValues), "widget.locked", [](WidgetPropsPatch& patch, bool value) { patch.locked = value; });
        addSectionHeader("Transform");
        if (const auto transform = resolveCurrentTransformBounds(); transform.has_value())
        {
            const auto addTransformField = [this, ids](const juce::String& label, int axisIndex, float current)
            {
                Widgets::WidgetPropertySpec spec;
                spec.key = juce::Identifier("widget.transform." + label.toLowerCase());
                spec.label = label;
                spec.kind = Widgets::WidgetPropertyKind::number;
                spec.uiHint = Widgets::WidgetPropertyUiHint::spinBox;
                spec.decimals = 2;
                if (axisIndex >= 2)
                    spec.minValue = 1.0;

                ValueState state;
                state.valid = true;
                state.value = current;

                const auto editKey = "widget.transform." + juce::String(axisIndex) + ":" + juce::String(ids.front());
                auto apply = [this, axisIndex, editKey](const juce::var& nextValue) -> bool
                {
                    const auto numeric = toFloat(nextValue);
                    const auto currentBounds = resolveCurrentTransformBounds();
                    if (!numeric.has_value() || !currentBounds.has_value())
                        return false;

                    auto target = *currentBounds;
                    const auto applied = axisIndex >= 2 ? std::max(1.0f, *numeric) : *numeric;
                    if (axisIndex == 0) target.setX(applied);
                    else if (axisIndex == 1) target.setY(applied);
                    else if (axisIndex == 2) target.setWidth(applied);
                    else target.setHeight(applied);
                    return applyTransformPreview(target, editKey);
                };

                addEditorRow(spec,
                             state,
                             [apply](const juce::var& nextValue) { apply(nextValue); },
                             [this, apply, editKey](const juce::var& nextValue)
                             {
                                 if (apply(nextValue))
                                     commitEditSession(editKey);
                             },
                             [this, editKey]
                             {
                                 cancelEditSession(editKey);
                                 rebuildContent();
                             });
            };

            addTransformField("X", 0, transform->getX());
            addTransformField("Y", 1, transform->getY());
            addTransformField("W", 2, transform->getWidth());
            addTransformField("H", 3, transform->getHeight());
        }

        addSectionHeader("Appearance");
        Widgets::WidgetPropertySpec opacitySpec;
        opacitySpec.key = juce::Identifier("widget.opacity");
        opacitySpec.label = "Opacity";
        opacitySpec.kind = Widgets::WidgetPropertyKind::number;
        opacitySpec.uiHint = Widgets::WidgetPropertyUiHint::slider;
        opacitySpec.minValue = 0.0;
        opacitySpec.maxValue = 1.0;
        opacitySpec.step = 0.01;
        opacitySpec.decimals = 3;

        const auto widgetOpacityKey = "widget.opacity:" + juce::String(ids.front());
        auto applyWidgetOpacity = [this, ids, widgetOpacityKey](const juce::var& nextValue) -> bool
        {
            const auto numeric = toFloat(nextValue);
            if (!numeric.has_value())
                return false;

            SetPropsAction action;
            action.kind = NodeKind::widget;
            action.ids = ids;
            WidgetPropsPatch patch;
            patch.opacity = juce::jlimit(0.0f, 1.0f, *numeric);
            action.patch = std::move(patch);
            return applySetPropsPreview(action, widgetOpacityKey);
        };

        addEditorRow(opacitySpec,
                     makeFloatState(opacityValues),
                     [applyWidgetOpacity](const juce::var& nextValue) { applyWidgetOpacity(nextValue); },
                     [this, applyWidgetOpacity, widgetOpacityKey](const juce::var& nextValue)
                     {
                         if (applyWidgetOpacity(nextValue))
                             commitEditSession(widgetOpacityKey);
                     },
                     [this, widgetOpacityKey]
                     {
                         cancelEditSession(widgetOpacityKey);
                         rebuildContent();
                     });

        buildCommonWidgetProperties(widgets);

        if (!multiSelection)
            addInfoRow("Type", widgetTypeLabel(widgetFactory, widgets.front().type));
    }

    void PropertyPanel::buildCommonWidgetProperties(const std::vector<WidgetRef>& widgets)
    {
        const auto specs = commonPropertySpecs(widgets);
        if (specs.empty())
        {
            showAdvancedToggle.setVisible(false);
            return;
        }

        const auto hasAdvanced = std::any_of(specs.begin(), specs.end(), [](const Widgets::WidgetPropertySpec& spec) { return spec.advanced; });
        showAdvancedToggle.setVisible(false);

        addSectionHeader("Widget Properties");

        const auto ids = inspectorTarget.widgetIds.empty() ? std::vector<WidgetId> { widgets.front().id } : inspectorTarget.widgetIds;

        enum class SliderValueLayout
        {
            notSlider,
            single,
            range,
            three,
            mixed
        };

        const auto resolveSliderLayout = [&widgets]() -> SliderValueLayout
        {
            if (widgets.empty())
                return SliderValueLayout::notSlider;

            for (const auto& widget : widgets)
            {
                if (widget.type != WidgetType::slider)
                    return SliderValueLayout::notSlider;
            }

            const auto styleToLayout = [](const juce::String& styleKey) -> SliderValueLayout
            {
                if (styleKey == "twoValueHorizontal" || styleKey == "twoValueVertical")
                    return SliderValueLayout::range;
                if (styleKey == "threeValueHorizontal" || styleKey == "threeValueVertical")
                    return SliderValueLayout::three;
                return SliderValueLayout::single;
            };

            auto layout = SliderValueLayout::single;
            bool hasLayout = false;
            for (const auto& widget : widgets)
            {
                const auto style = widget.properties.getWithDefault("slider.style", juce::var("linearHorizontal")).toString();
                const auto nextLayout = styleToLayout(style);
                if (!hasLayout)
                {
                    layout = nextLayout;
                    hasLayout = true;
                }
                else if (layout != nextLayout)
                {
                    return SliderValueLayout::mixed;
                }
            }

            return hasLayout ? layout : SliderValueLayout::single;
        };

        const auto sliderLayout = resolveSliderLayout();
        const auto renderSpec = [this, &widgets, &ids](const Widgets::WidgetPropertySpec& specForRendering)
        {
            auto specForEditor = specForRendering;
            const auto valueState = makeWidgetPropertyState(widgets, specForRendering.key);

            if (specForEditor.kind == Widgets::WidgetPropertyKind::assetRef)
            {
                specForEditor.uiHint = Widgets::WidgetPropertyUiHint::assetPicker;
                specForEditor.enumOptions.clear();

                Widgets::WidgetEnumOption noneOption;
                noneOption.value = {};
                noneOption.label = "(None)";
                specForEditor.enumOptions.push_back(std::move(noneOption));

                std::unordered_set<juce::String> seenRefKeys;
                const auto currentRef = valueState.valid ? valueState.value.toString().trim() : juce::String();
                const auto currentRefNormalized = currentRef.toLowerCase();
                bool currentRefExists = false;
                bool currentRefRejectedByType = false;
                const auto& assetList = document.snapshot().assets;
                for (const auto& asset : assetList)
                {
                    const auto refKey = asset.refKey.trim();
                    if (refKey.isEmpty())
                        continue;

                    if (currentRef.isNotEmpty() && refKey.equalsIgnoreCase(currentRef))
                        currentRefExists = true;

                    if (!Widgets::isAssetKindAccepted(specForEditor, asset.kind))
                    {
                        if (currentRef.isNotEmpty() && refKey.equalsIgnoreCase(currentRef))
                            currentRefRejectedByType = true;
                        continue;
                    }

                    if (!seenRefKeys.insert(refKey.toLowerCase()).second)
                        continue;

                    Widgets::WidgetEnumOption option;
                    option.value = refKey;
                    option.label = asset.name.isNotEmpty() ? asset.name + " (" + refKey + ")" : refKey;
                    specForEditor.enumOptions.push_back(std::move(option));
                }

                if (currentRef.isNotEmpty() && seenRefKeys.count(currentRefNormalized) == 0)
                {
                    Widgets::WidgetEnumOption missingOption;
                    missingOption.value = currentRef;
                    if (currentRefRejectedByType)
                        missingOption.label = "[Incompatible Type] " + currentRef;
                    else if (currentRefExists)
                        missingOption.label = "[Filtered] " + currentRef;
                    else
                        missingOption.label = "[Missing] " + currentRef;
                    specForEditor.enumOptions.push_back(std::move(missingOption));
                }
            }

            const auto editKey = "widget.prop." + specForRendering.key.toString().replaceCharacter('.', '_') + ":" + juce::String(ids.front());

            auto applyProp = [this, ids, specForEditor, propertyKey = specForRendering.key, editKey](const juce::var& nextValue) -> bool
            {
                juce::var normalized;
                if (!PropertyEditorFactory::normalizeValue(specForEditor, nextValue, normalized))
                    return false;

                SetPropsAction action;
                action.kind = NodeKind::widget;
                action.ids = ids;
                WidgetPropsPatch patch;
                patch.patch.set(propertyKey, normalized);
                action.patch = std::move(patch);
                return applySetPropsPreview(action, editKey);
            };

            addEditorRow(specForEditor,
                         valueState,
                         [applyProp](const juce::var& nextValue) { applyProp(nextValue); },
                         [this, applyProp, editKey](const juce::var& nextValue)
                         {
                             if (applyProp(nextValue))
                                 commitEditSession(editKey);
                         },
                         [this, editKey]
                         {
                             cancelEditSession(editKey);
                             rebuildContent();
                         });
        };

        std::vector<Widgets::WidgetPropertySpec> basicSpecs;
        std::vector<Widgets::WidgetPropertySpec> advancedSpecs;
        basicSpecs.reserve(specs.size());
        advancedSpecs.reserve(specs.size());

        for (const auto& spec : specs)
        {
            if (sliderLayout != SliderValueLayout::notSlider && sliderLayout != SliderValueLayout::mixed)
            {
                const auto key = spec.key.toString();
                if (sliderLayout == SliderValueLayout::single)
                {
                    if (key == "minValue" || key == "maxValue")
                        continue;
                }
                else if (sliderLayout == SliderValueLayout::range)
                {
                    if (key == "value")
                        continue;
                }
            }

            if (spec.advanced)
                advancedSpecs.push_back(spec);
            else
                basicSpecs.push_back(spec);
        }

        for (const auto& spec : basicSpecs)
            renderSpec(spec);

        if (showAdvancedProperties)
        {
            for (const auto& spec : advancedSpecs)
                renderSpec(spec);
        }

        if (hasAdvanced)
        {
            addExpanderRow("Advanced (" + juce::String(static_cast<int>(advancedSpecs.size())) + ")",
                           showAdvancedProperties,
                           [this]
                           {
                               showAdvancedProperties = !showAdvancedProperties;
                               rebuildContent();
                           });
        }
    }

    void PropertyPanel::rebuildContent()
    {
        resetContent();
        titleLabel.setText("Inspector", juce::dontSendNotification);
        subtitleLabel.setText({}, juce::dontSendNotification);

        switch (inspectorTarget.kind)
        {
            case InspectorTargetKind::none:
                subtitleLabel.setText("No target", juce::dontSendNotification);
                buildNoneContent();
                break;

            case InspectorTargetKind::layer:
                if (const auto* layer = findLayer(inspectorTarget.nodeId); layer != nullptr)
                {
                    titleLabel.setText("Layer", juce::dontSendNotification);
                    subtitleLabel.setText(layer->name + "  #" + juce::String(layer->id), juce::dontSendNotification);
                    buildLayerContent(*layer);
                }
                else
                {
                    subtitleLabel.setText("Layer not found", juce::dontSendNotification);
                    buildNoneContent();
                }
                break;

            case InspectorTargetKind::group:
                if (const auto* group = findGroup(inspectorTarget.nodeId); group != nullptr)
                {
                    titleLabel.setText("Group", juce::dontSendNotification);
                    subtitleLabel.setText(group->name + "  #" + juce::String(group->id), juce::dontSendNotification);
                    buildGroupContent(*group);
                }
                else
                {
                    subtitleLabel.setText("Group not found", juce::dontSendNotification);
                    buildNoneContent();
                }
                break;

            case InspectorTargetKind::widgetSingle:
            case InspectorTargetKind::widgetMulti:
            {
                const auto widgets = resolveTargetWidgets();
                if (widgets.empty())
                {
                    subtitleLabel.setText("Widget not found", juce::dontSendNotification);
                    buildNoneContent();
                    break;
                }

                const auto multi = inspectorTarget.kind == InspectorTargetKind::widgetMulti || widgets.size() > 1;
                titleLabel.setText(multi ? "Widgets" : "Widget", juce::dontSendNotification);
                subtitleLabel.setText(multi
                                          ? juce::String(static_cast<int>(widgets.size())) + " selected"
                                          : (widgetTypeLabel(widgetFactory, widgets.front().type) + "  #" + juce::String(widgets.front().id)),
                                      juce::dontSendNotification);
                buildWidgetContent(widgets, multi);
                break;
            }
        }

        layoutContent();
        repaint();
    }
}
