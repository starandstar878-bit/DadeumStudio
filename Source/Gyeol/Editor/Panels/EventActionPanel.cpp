#include "Gyeol/Editor/Panels/EventActionPanel.h"

#include <algorithm>
#include <cmath>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        const auto kPanelBg = juce::Colour::fromRGB(24, 28, 34);
        const auto kPanelOutline = juce::Colour::fromRGB(40, 46, 56);
        const auto kStatusInfo = juce::Colour::fromRGB(160, 170, 186);
        const auto kStatusOk = juce::Colour::fromRGB(112, 214, 156);
        const auto kStatusWarn = juce::Colour::fromRGB(255, 196, 120);
        const auto kStatusError = juce::Colour::fromRGB(255, 124, 124);

        void setupEditor(juce::TextEditor& editor, const juce::String& placeholder = {})
        {
            editor.setMultiLine(false);
            editor.setScrollbarsShown(true);
            editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(28, 34, 44));
            editor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(66, 76, 92));
            editor.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(214, 222, 234));
            editor.setTextToShowWhenEmpty(placeholder, juce::Colour::fromRGB(124, 132, 148));
        }

        bool parseWidgetId(const juce::String& text, WidgetId& outId) noexcept
        {
            const auto parsed = widgetIdFromJsonString(text.trim());
            if (!parsed.has_value() || *parsed <= kRootId)
                return false;
            outId = *parsed;
            return true;
        }

        juce::var patchToVar(const PropertyBag& patch)
        {
            auto object = std::make_unique<juce::DynamicObject>();
            for (int i = 0; i < patch.size(); ++i)
                object->setProperty(patch.getName(i), patch.getValueAt(i));
            return juce::var(object.release());
        }

        juce::String eventDisplayLabelKo(const juce::String& eventKey)
        {
            const auto key = eventKey.trim();
            if (key == "onClick") return juce::String::fromUTF8(u8"\uD074\uB9AD \uC2DC");
            if (key == "onPress") return juce::String::fromUTF8(u8"\uB204\uB97C \uB54C");
            if (key == "onRelease") return juce::String::fromUTF8(u8"\uB193\uC744 \uB54C");
            if (key == "onValueChanged") return juce::String::fromUTF8(u8"\uAC12 \uC9C0\uC815 \uC2DC");
            if (key == "onValueCommit") return juce::String::fromUTF8(u8"\uAC12 \uC81C\uCD9C \uC2DC");
            if (key == "onToggleChanged") return juce::String::fromUTF8(u8"\uD1A0\uAE00 \uBCC0\uACBD \uC2DC");
            if (key == "onTextCommit") return juce::String::fromUTF8(u8"\uD14D\uC2A4\uD2B8 \uC81C\uCD9C \uC2DC");
            if (key == "onSelectionChanged") return juce::String::fromUTF8(u8"\uC120\uD0DD \uBCC0\uACBD \uC2DC");
            return {};
        }

        int actionKindToComboId(RuntimeActionKind kind) noexcept
        {
            switch (kind)
            {
                case RuntimeActionKind::setRuntimeParam: return 1;
                case RuntimeActionKind::adjustRuntimeParam: return 2;
                case RuntimeActionKind::toggleRuntimeParam: return 3;
                case RuntimeActionKind::setNodeProps: return 4;
                case RuntimeActionKind::setNodeBounds: return 5;
            }
            return 0;
        }

        std::optional<RuntimeActionKind> actionKindFromComboId(int id) noexcept
        {
            switch (id)
            {
                case 1: return RuntimeActionKind::setRuntimeParam;
                case 2: return RuntimeActionKind::adjustRuntimeParam;
                case 3: return RuntimeActionKind::toggleRuntimeParam;
                case 4: return RuntimeActionKind::setNodeProps;
                case 5: return RuntimeActionKind::setNodeBounds;
                default: break;
            }
            return std::nullopt;
        }

        int nodeKindToComboId(NodeKind kind) noexcept
        {
            switch (kind)
            {
                case NodeKind::widget: return 1;
                case NodeKind::group: return 2;
                case NodeKind::layer: return 3;
            }
            return 0;
        }

        std::optional<NodeKind> nodeKindFromComboId(int id) noexcept
        {
            switch (id)
            {
                case 1: return NodeKind::widget;
                case 2: return NodeKind::group;
                case 3: return NodeKind::layer;
                default: break;
            }
            return std::nullopt;
        }
    }

    EventActionPanel::EventActionPanel(DocumentHandle& documentIn, const Widgets::WidgetRegistry& registryIn)
        : document(documentIn),
          registry(registryIn),
          bindingListModel(*this),
          actionListModel(*this)
    {
        titleLabel.setText("Event/Action", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(192, 200, 214));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        addAndMakeVisible(sourceCombo);
        sourceCombo.onChange = [this]
        {
            if (!suppressCallbacks)
                rebuildCreateCombos();
        };

        addAndMakeVisible(eventCombo);
        addBindingButton.onClick = [this] { createBindingFromToolbar(); };
        addAndMakeVisible(addBindingButton);

        setupEditor(searchEditor, "Search Name/Source/Event");
        searchEditor.onTextChange = [this]
        {
            if (!suppressCallbacks)
                rebuildVisibleBindings();
        };
        addAndMakeVisible(searchEditor);

        bindingList.setModel(&bindingListModel);
        bindingList.setRowHeight(34);
        bindingList.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
        bindingList.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
        addAndMakeVisible(bindingList);

        detailTitleLabel.setText("Binding Detail", juce::dontSendNotification);
        detailTitleLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        detailTitleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(194, 202, 216));
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
        actionList.setRowHeight(26);
        actionList.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
        actionList.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
        addAndMakeVisible(actionList);

        addActionButton.onClick = [this] { addAction(); };
        deleteActionButton.onClick = [this] { deleteAction(); };
        actionUpButton.onClick = [this] { moveActionUp(); };
        actionDownButton.onClick = [this] { moveActionDown(); };
        addAndMakeVisible(addActionButton);
        addAndMakeVisible(deleteActionButton);
        addAndMakeVisible(actionUpButton);
        addAndMakeVisible(actionDownButton);

        actionKindCombo.addItem(actionKindLabel(RuntimeActionKind::setRuntimeParam), 1);
        actionKindCombo.addItem(actionKindLabel(RuntimeActionKind::adjustRuntimeParam), 2);
        actionKindCombo.addItem(actionKindLabel(RuntimeActionKind::toggleRuntimeParam), 3);
        actionKindCombo.addItem(actionKindLabel(RuntimeActionKind::setNodeProps), 4);
        actionKindCombo.addItem(actionKindLabel(RuntimeActionKind::setNodeBounds), 5);
        actionKindCombo.onChange = [this] { applyActionKind(); };
        addAndMakeVisible(actionKindCombo);

        setupEditor(paramKeyEditor, "paramKey");
        setupEditor(valueEditor, "value");
        setupEditor(deltaEditor, "delta");
        setupEditor(targetIdEditor, "targetId");
        setupEditor(opacityEditor, "opacity");
        setupEditor(boundsXEditor, "x");
        setupEditor(boundsYEditor, "y");
        setupEditor(boundsWEditor, "w");
        setupEditor(boundsHEditor, "h");

        paramKeyEditor.onReturnKey = [this] { applySelectedAction(); };
        paramKeyEditor.onFocusLost = [this] { applySelectedAction(); };
        valueEditor.onReturnKey = [this] { applySelectedAction(); };
        valueEditor.onFocusLost = [this] { applySelectedAction(); };
        deltaEditor.onReturnKey = [this] { applySelectedAction(); };
        deltaEditor.onFocusLost = [this] { applySelectedAction(); };
        targetIdEditor.onReturnKey = [this] { applySelectedAction(); };
        targetIdEditor.onFocusLost = [this] { applySelectedAction(); };
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

        addAndMakeVisible(paramKeyEditor);
        addAndMakeVisible(valueEditor);
        addAndMakeVisible(deltaEditor);
        addAndMakeVisible(targetIdEditor);
        addAndMakeVisible(opacityEditor);
        addAndMakeVisible(boundsXEditor);
        addAndMakeVisible(boundsYEditor);
        addAndMakeVisible(boundsWEditor);
        addAndMakeVisible(boundsHEditor);

        targetKindCombo.addItem("widget", 1);
        targetKindCombo.addItem("group", 2);
        targetKindCombo.addItem("layer", 3);
        targetKindCombo.onChange = [this] { applySelectedAction(); };
        addAndMakeVisible(targetKindCombo);

        visibleCombo.addItem("vis:keep", 1);
        visibleCombo.addItem("vis:on", 2);
        visibleCombo.addItem("vis:off", 3);
        visibleCombo.onChange = [this] { applySelectedAction(); };
        addAndMakeVisible(visibleCombo);

        lockedCombo.addItem("lock:keep", 1);
        lockedCombo.addItem("lock:on", 2);
        lockedCombo.addItem("lock:off", 3);
        lockedCombo.onChange = [this] { applySelectedAction(); };
        addAndMakeVisible(lockedCombo);

        assetPatchKeyCombo.setTextWhenNothingSelected("asset key");
        assetPatchKeyCombo.onChange = [this]
        {
            if (!suppressCallbacks)
                syncAssetPatchValueEditor();
        };
        addAndMakeVisible(assetPatchKeyCombo);

        assetPatchValueCombo.setTextWhenNothingSelected("asset ref");
        assetPatchValueCombo.setEditableText(true);
        assetPatchValueCombo.onChange = [this]
        {
            if (!suppressCallbacks)
                applyAssetPatchValue();
        };
        addAndMakeVisible(assetPatchValueCombo);

        patchEditor.setMultiLine(true);
        patchEditor.setScrollbarsShown(true);
        patchEditor.setReturnKeyStartsNewLine(true);
        patchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(28, 34, 44));
        patchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(66, 76, 92));
        patchEditor.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(214, 222, 234));
        patchEditor.onFocusLost = [this] { applySelectedAction(); };
        addAndMakeVisible(patchEditor);

        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setColour(juce::Label::textColourId, kStatusInfo);
        addAndMakeVisible(statusLabel);

        refreshFromDocument();
    }

    EventActionPanel::~EventActionPanel()
    {
        bindingList.setModel(nullptr);
        actionList.setModel(nullptr);
    }

    void EventActionPanel::setBindingsChangedCallback(std::function<void()> callback)
    {
        onBindingsChanged = std::move(callback);
    }

    void EventActionPanel::refreshFromDocument()
    {
        bindings = document.snapshot().runtimeBindings;
        rebuildWidgetOptions();
        rebuildCreateCombos();
        rebuildVisibleBindings();
        restoreSelections();
        refreshDetailEditors();
    }

    void EventActionPanel::paint(juce::Graphics& g)
    {
        g.fillAll(kPanelBg);
        g.setColour(kPanelOutline);
        g.drawRect(getLocalBounds(), 1);
    }

    void EventActionPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);

        titleLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);

        auto topRow = area.removeFromTop(24);
        auto addBindingArea = topRow.removeFromRight(84);
        topRow.removeFromRight(4);
        sourceCombo.setBounds(topRow.removeFromLeft(juce::jmin(170, topRow.getWidth() / 2)));
        topRow.removeFromLeft(4);
        eventCombo.setBounds(topRow);
        addBindingButton.setBounds(addBindingArea);

        area.removeFromTop(4);
        searchEditor.setBounds(area.removeFromTop(24));
        area.removeFromTop(6);
        bindingList.setBounds(area.removeFromTop(148));
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
        actionList.setBounds(area.removeFromTop(96));
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
        const auto* action = selectedAction();
        const auto kind = action != nullptr ? action->kind : RuntimeActionKind::setRuntimeParam;

        const auto layoutRow = [&area](std::initializer_list<std::pair<juce::Component*, int>> fields)
        {
            std::vector<std::pair<juce::Component*, int>> visibleFields;
            visibleFields.reserve(fields.size());
            for (const auto& field : fields)
            {
                if (field.first != nullptr && field.first->isVisible())
                    visibleFields.push_back(field);
            }

            if (visibleFields.empty())
                return;

            auto row = area.removeFromTop(24);
            for (size_t i = 0; i < visibleFields.size(); ++i)
            {
                auto* component = visibleFields[i].first;
                if (component == nullptr)
                    continue;

                if (i + 1 == visibleFields.size())
                {
                    component->setBounds(row);
                }
                else
                {
                    const auto width = juce::jmin(visibleFields[i].second, row.getWidth());
                    component->setBounds(row.removeFromLeft(width));
                    row.removeFromLeft(4);
                }
            }

            area.removeFromTop(4);
        };

        switch (kind)
        {
            case RuntimeActionKind::setRuntimeParam:
                layoutRow({ { &actionKindCombo, 176 }, { &paramKeyEditor, 132 }, { &valueEditor, 200 } });
                break;
            case RuntimeActionKind::adjustRuntimeParam:
                layoutRow({ { &actionKindCombo, 176 }, { &paramKeyEditor, 154 }, { &deltaEditor, 120 } });
                break;
            case RuntimeActionKind::toggleRuntimeParam:
                layoutRow({ { &actionKindCombo, 176 }, { &paramKeyEditor, 200 } });
                break;
            case RuntimeActionKind::setNodeProps:
                layoutRow({ { &actionKindCombo, 176 }, { &targetKindCombo, 88 }, { &targetIdEditor, 120 } });
                layoutRow({ { &visibleCombo, 92 }, { &lockedCombo, 92 }, { &opacityEditor, 100 } });
                layoutRow({ { &assetPatchKeyCombo, 180 }, { &assetPatchValueCombo, 220 } });
                if (patchEditor.isVisible())
                {
                    patchEditor.setBounds(area.removeFromTop(56));
                    area.removeFromTop(4);
                }
                break;
            case RuntimeActionKind::setNodeBounds:
                layoutRow({ { &actionKindCombo, 176 }, { &targetIdEditor, 140 } });
                layoutRow({ { &boundsXEditor, 76 }, { &boundsYEditor, 76 }, { &boundsWEditor, 76 }, { &boundsHEditor, 76 } });
                break;
        }

        statusLabel.setBounds(area.removeFromTop(18));
    }

    int EventActionPanel::selectedBindingModelIndex() const
    {
        const auto row = bindingList.getSelectedRow();
        if (row < 0 || row >= static_cast<int>(visibleBindingIndices.size()))
            return -1;
        return visibleBindingIndices[static_cast<size_t>(row)];
    }

    RuntimeBindingModel* EventActionPanel::selectedBinding()
    {
        const auto index = selectedBindingModelIndex();
        if (index < 0 || index >= static_cast<int>(bindings.size()))
            return nullptr;
        return &bindings[static_cast<size_t>(index)];
    }

    const RuntimeBindingModel* EventActionPanel::selectedBinding() const
    {
        const auto index = selectedBindingModelIndex();
        if (index < 0 || index >= static_cast<int>(bindings.size()))
            return nullptr;
        return &bindings[static_cast<size_t>(index)];
    }

    RuntimeActionModel* EventActionPanel::selectedAction()
    {
        auto* binding = selectedBinding();
        const auto row = actionList.getSelectedRow();
        if (binding == nullptr || row < 0 || row >= static_cast<int>(binding->actions.size()))
            return nullptr;
        return &binding->actions[static_cast<size_t>(row)];
    }

    const RuntimeActionModel* EventActionPanel::selectedAction() const
    {
        const auto* binding = selectedBinding();
        const auto row = actionList.getSelectedRow();
        if (binding == nullptr || row < 0 || row >= static_cast<int>(binding->actions.size()))
            return nullptr;
        return &binding->actions[static_cast<size_t>(row)];
    }

    void EventActionPanel::rebuildWidgetOptions()
    {
        widgetOptions.clear();
        const auto& snapshot = document.snapshot();
        widgetOptions.reserve(snapshot.widgets.size());

        for (const auto& widget : snapshot.widgets)
        {
            WidgetOption option;
            option.id = widget.id;
            if (const auto* descriptor = registry.find(widget.type); descriptor != nullptr)
            {
                const auto name = descriptor->displayName.isNotEmpty() ? descriptor->displayName : descriptor->typeKey;
                option.label = name + " #" + juce::String(widget.id);
                option.events = descriptor->runtimeEvents;
            }
            else
            {
                option.label = "Widget #" + juce::String(widget.id);
            }
            widgetOptions.push_back(std::move(option));
        }

        std::sort(widgetOptions.begin(),
                  widgetOptions.end(),
                  [](const WidgetOption& lhs, const WidgetOption& rhs) { return lhs.id < rhs.id; });
    }

    void EventActionPanel::rebuildCreateCombos()
    {
        const auto previousSourceIndex = sourceCombo.getSelectedItemIndex();

        suppressCallbacks = true;
        sourceCombo.clear(juce::dontSendNotification);
        int sourceItemId = 1;
        for (const auto& source : widgetOptions)
            sourceCombo.addItem(source.label, sourceItemId++);

        if (!widgetOptions.empty())
        {
            const auto safeIndex = juce::jlimit(0, static_cast<int>(widgetOptions.size()) - 1, previousSourceIndex);
            sourceCombo.setSelectedItemIndex(safeIndex, juce::dontSendNotification);
        }

        eventCombo.clear(juce::dontSendNotification);
        const auto sourceIndex = sourceCombo.getSelectedItemIndex();
        if (sourceIndex >= 0 && sourceIndex < static_cast<int>(widgetOptions.size()))
        {
            int eventItemId = 1;
            for (const auto& eventSpec : widgetOptions[static_cast<size_t>(sourceIndex)].events)
                eventCombo.addItem(formatEventLabel(eventSpec), eventItemId++);
            if (eventCombo.getNumItems() > 0)
                eventCombo.setSelectedItemIndex(0, juce::dontSendNotification);
        }

        addBindingButton.setEnabled(eventCombo.getNumItems() > 0);
        suppressCallbacks = false;
    }

    void EventActionPanel::rebuildVisibleBindings()
    {
        visibleBindingIndices.clear();
        const auto filter = searchEditor.getText().trim().toLowerCase();

        for (int i = 0; i < static_cast<int>(bindings.size()); ++i)
        {
            const auto& binding = bindings[static_cast<size_t>(i)];
            if (filter.isEmpty())
            {
                visibleBindingIndices.push_back(i);
                continue;
            }

            const auto sourceText = findWidgetOption(binding.sourceWidgetId).has_value()
                                      ? findWidgetOption(binding.sourceWidgetId)->label
                                      : ("Widget #" + juce::String(binding.sourceWidgetId));
            const auto haystack = (binding.name + " " + sourceText + " "
                                   + formatEventLabel(binding.sourceWidgetId, binding.eventKey)).toLowerCase();
            if (haystack.contains(filter))
                visibleBindingIndices.push_back(i);
        }

        bindingList.updateContent();
        bindingList.repaint();
    }

    void EventActionPanel::restoreSelections()
    {
        int targetRow = -1;
        if (selectedBindingId > kRootId)
        {
            for (int row = 0; row < static_cast<int>(visibleBindingIndices.size()); ++row)
            {
                const auto index = visibleBindingIndices[static_cast<size_t>(row)];
                if (bindings[static_cast<size_t>(index)].id == selectedBindingId)
                {
                    targetRow = row;
                    break;
                }
            }
        }

        if (targetRow < 0 && !visibleBindingIndices.empty())
        {
            targetRow = 0;
            selectedBindingId = bindings[static_cast<size_t>(visibleBindingIndices.front())].id;
        }

        suppressCallbacks = true;
        if (targetRow >= 0)
            bindingList.selectRow(targetRow);
        else
            bindingList.deselectAllRows();
        suppressCallbacks = false;
    }

    void EventActionPanel::refreshDetailEditors()
    {
        const auto* binding = selectedBinding();
        const auto hasBinding = binding != nullptr;

        suppressCallbacks = true;
        detailTitleLabel.setText(hasBinding ? ("Binding Detail #" + juce::String(binding->id)) : juce::String("Binding Detail"),
                                 juce::dontSendNotification);
        bindingNameEditor.setEnabled(hasBinding);
        bindingEnabledToggle.setEnabled(hasBinding);
        duplicateBindingButton.setEnabled(hasBinding);
        deleteBindingButton.setEnabled(hasBinding);

        if (hasBinding)
        {
            bindingNameEditor.setText(binding->name, juce::dontSendNotification);
            bindingEnabledToggle.setToggleState(binding->enabled, juce::dontSendNotification);
        }
        else
        {
            bindingNameEditor.clear();
            bindingEnabledToggle.setToggleState(false, juce::dontSendNotification);
        }

        actionList.updateContent();
        actionList.repaint();
        if (hasBinding && !binding->actions.empty())
        {
            selectedActionRow = juce::jlimit(0, static_cast<int>(binding->actions.size()) - 1, selectedActionRow);
            actionList.selectRow(selectedActionRow);
        }
        else
        {
            selectedActionRow = -1;
            actionList.deselectAllRows();
        }

        auto* action = selectedAction();
        const auto hasAction = action != nullptr;
        addActionButton.setEnabled(hasBinding);
        deleteActionButton.setEnabled(hasAction);
        actionUpButton.setEnabled(hasAction && actionList.getSelectedRow() > 0);
        actionDownButton.setEnabled(hasAction && binding != nullptr
                                    && actionList.getSelectedRow() < static_cast<int>(binding->actions.size()) - 1);

        updateActionEditorVisibility(action, hasAction);
        actionKindCombo.setEnabled(hasAction && actionKindCombo.isVisible());
        paramKeyEditor.setEnabled(hasAction && paramKeyEditor.isVisible());
        valueEditor.setEnabled(hasAction && valueEditor.isVisible());
        deltaEditor.setEnabled(hasAction && deltaEditor.isVisible());
        targetKindCombo.setEnabled(hasAction && targetKindCombo.isVisible());
        targetIdEditor.setEnabled(hasAction && targetIdEditor.isVisible());
        visibleCombo.setEnabled(hasAction && visibleCombo.isVisible());
        lockedCombo.setEnabled(hasAction && lockedCombo.isVisible());
        opacityEditor.setEnabled(hasAction && opacityEditor.isVisible());
        assetPatchKeyCombo.setEnabled(hasAction && assetPatchKeyCombo.isVisible());
        assetPatchValueCombo.setEnabled(hasAction && assetPatchValueCombo.isVisible());
        patchEditor.setEnabled(hasAction && patchEditor.isVisible());
        boundsXEditor.setEnabled(hasAction && boundsXEditor.isVisible());
        boundsYEditor.setEnabled(hasAction && boundsYEditor.isVisible());
        boundsWEditor.setEnabled(hasAction && boundsWEditor.isVisible());
        boundsHEditor.setEnabled(hasAction && boundsHEditor.isVisible());

        if (hasAction)
        {
            actionKindCombo.setSelectedId(actionKindToComboId(action->kind), juce::dontSendNotification);
            paramKeyEditor.setText(action->paramKey, juce::dontSendNotification);
            valueEditor.setText(runtimeValueToString(action->value), juce::dontSendNotification);
            deltaEditor.setText(juce::String(action->delta, 6), juce::dontSendNotification);
            targetKindCombo.setSelectedId(nodeKindToComboId(action->target.kind), juce::dontSendNotification);
            targetIdEditor.setText(widgetIdToJsonString(action->target.id), juce::dontSendNotification);
            visibleCombo.setSelectedId(!action->visible.has_value() ? 1 : (*action->visible ? 2 : 3), juce::dontSendNotification);
            lockedCombo.setSelectedId(!action->locked.has_value() ? 1 : (*action->locked ? 2 : 3), juce::dontSendNotification);
            opacityEditor.setText(action->opacity.has_value() ? juce::String(*action->opacity, 4) : juce::String(),
                                  juce::dontSendNotification);
            patchEditor.setText(action->patch.size() > 0 ? juce::JSON::toString(patchToVar(action->patch), true) : juce::String(),
                                juce::dontSendNotification);
            boundsXEditor.setText(juce::String(action->bounds.getX(), 4), juce::dontSendNotification);
            boundsYEditor.setText(juce::String(action->bounds.getY(), 4), juce::dontSendNotification);
            boundsWEditor.setText(juce::String(action->bounds.getWidth(), 4), juce::dontSendNotification);
            boundsHEditor.setText(juce::String(action->bounds.getHeight(), 4), juce::dontSendNotification);
        }
        else
        {
            actionKindCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
            paramKeyEditor.clear();
            valueEditor.clear();
            deltaEditor.clear();
            targetKindCombo.setSelectedItemIndex(-1, juce::dontSendNotification);
            targetIdEditor.clear();
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
        assetPatchKeyCombo.setEnabled(hasAction && assetPatchKeyCombo.isVisible() && !assetPatchKeys.empty());
        assetPatchValueCombo.setEnabled(hasAction && assetPatchValueCombo.isVisible() && !assetPatchKeys.empty());

        resized();
        suppressCallbacks = false;
        repaint();
    }

    void EventActionPanel::updateActionEditorVisibility(const RuntimeActionModel* action, bool hasAction)
    {
        const auto setVisibility = [hasAction](juce::Component& component, bool visibleWhenActive)
        {
            component.setVisible(hasAction && visibleWhenActive);
        };

        if (!hasAction || action == nullptr)
        {
            setVisibility(actionKindCombo, false);
            setVisibility(paramKeyEditor, false);
            setVisibility(valueEditor, false);
            setVisibility(deltaEditor, false);
            setVisibility(targetKindCombo, false);
            setVisibility(targetIdEditor, false);
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

        const auto isSetRuntimeParam = action->kind == RuntimeActionKind::setRuntimeParam;
        const auto isAdjustRuntimeParam = action->kind == RuntimeActionKind::adjustRuntimeParam;
        const auto isToggleRuntimeParam = action->kind == RuntimeActionKind::toggleRuntimeParam;
        const auto isSetNodeProps = action->kind == RuntimeActionKind::setNodeProps;
        const auto isSetNodeBounds = action->kind == RuntimeActionKind::setNodeBounds;

        setVisibility(paramKeyEditor, isSetRuntimeParam || isAdjustRuntimeParam || isToggleRuntimeParam);
        setVisibility(valueEditor, isSetRuntimeParam);
        setVisibility(deltaEditor, isAdjustRuntimeParam);
        setVisibility(targetKindCombo, isSetNodeProps);
        setVisibility(targetIdEditor, isSetNodeProps || isSetNodeBounds);
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

    void EventActionPanel::rebuildAssetPatchEditors(const RuntimeActionModel* action)
    {
        assetPatchKeys.clear();
        assetPatchValues.clear();

        assetPatchKeyCombo.clear(juce::dontSendNotification);
        assetPatchValueCombo.clear(juce::dontSendNotification);
        assetPatchKeyCombo.setTextWhenNothingSelected("asset key");
        assetPatchValueCombo.setTextWhenNothingSelected("asset ref");

        if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps || action->target.kind != NodeKind::widget)
            return;

        const auto targetId = action->target.id;
        if (targetId <= kRootId)
            return;

        const auto& snapshot = document.snapshot();
        const auto widgetIt = std::find_if(snapshot.widgets.begin(),
                                           snapshot.widgets.end(),
                                           [targetId](const WidgetModel& model) noexcept
                                           {
                                               return model.id == targetId;
                                           });
        if (widgetIt == snapshot.widgets.end())
            return;

        const auto* descriptor = registry.find(widgetIt->type);
        if (descriptor == nullptr)
            return;

        int keyItemId = 1;
        for (const auto& spec : descriptor->propertySpecs)
        {
            if (spec.kind != Widgets::WidgetPropertyKind::assetRef)
                continue;

            const auto keyString = spec.key.toString();
            const auto keyLabel = spec.label.trim().isNotEmpty() ? spec.label.trim() : keyString;
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
        for (const auto& asset : snapshot.assets)
        {
            const auto refKey = asset.refKey.trim();
            if (refKey.isEmpty())
                continue;

            if (std::find(seenRefKeys.begin(), seenRefKeys.end(), refKey) != seenRefKeys.end())
                continue;

            seenRefKeys.push_back(refKey);
            const auto label = asset.name.trim().isNotEmpty() ? (asset.name + " (" + refKey + ")") : refKey;
            assetPatchValueCombo.addItem(label, valueItemId++);
            assetPatchValues.push_back(refKey);
        }

        int selectedKeyIndex = 0;
        for (int i = 0; i < action->patch.size(); ++i)
        {
            const auto candidateKey = action->patch.getName(i);
            const auto keyIt = std::find(assetPatchKeys.begin(), assetPatchKeys.end(), candidateKey);
            if (keyIt != assetPatchKeys.end())
            {
                selectedKeyIndex = static_cast<int>(std::distance(assetPatchKeys.begin(), keyIt));
                break;
            }
        }

        assetPatchKeyCombo.setSelectedItemIndex(selectedKeyIndex, juce::dontSendNotification);

        const auto selectedKey = assetPatchKeys[static_cast<size_t>(selectedKeyIndex)];
        const auto patchValue = action->patch.getVarPointer(selectedKey);
        const auto currentRef = patchValue != nullptr ? patchValue->toString().trim() : juce::String();

        int selectedValueIndex = 0;
        if (currentRef.isNotEmpty())
        {
            const auto valueIt = std::find(assetPatchValues.begin(), assetPatchValues.end(), currentRef);
            if (valueIt != assetPatchValues.end())
            {
                selectedValueIndex = static_cast<int>(std::distance(assetPatchValues.begin(), valueIt));
            }
            else
            {
                assetPatchValueCombo.addItem("[Missing] " + currentRef, valueItemId);
                assetPatchValues.push_back(currentRef);
                selectedValueIndex = static_cast<int>(assetPatchValues.size()) - 1;
            }
        }

        assetPatchValueCombo.setSelectedItemIndex(selectedValueIndex, juce::dontSendNotification);
    }

    void EventActionPanel::syncAssetPatchValueEditor()
    {
        if (suppressCallbacks)
            return;

        const auto* action = selectedAction();
        if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps || assetPatchKeys.empty())
            return;

        const auto keyIndex = assetPatchKeyCombo.getSelectedItemIndex();
        if (keyIndex < 0 || keyIndex >= static_cast<int>(assetPatchKeys.size()))
            return;

        const auto key = assetPatchKeys[static_cast<size_t>(keyIndex)];
        const auto patchValue = action->patch.getVarPointer(key);
        const auto currentRef = patchValue != nullptr ? patchValue->toString().trim() : juce::String();

        suppressCallbacks = true;
        int selectedValueIndex = 0;
        if (currentRef.isNotEmpty())
        {
            const auto valueIt = std::find(assetPatchValues.begin(), assetPatchValues.end(), currentRef);
            if (valueIt != assetPatchValues.end())
            {
                selectedValueIndex = static_cast<int>(std::distance(assetPatchValues.begin(), valueIt));
            }
            else
            {
                assetPatchValueCombo.addItem("[Missing] " + currentRef,
                                             static_cast<int>(assetPatchValues.size()) + 1);
                assetPatchValues.push_back(currentRef);
                selectedValueIndex = static_cast<int>(assetPatchValues.size()) - 1;
            }
        }

        assetPatchValueCombo.setSelectedItemIndex(selectedValueIndex, juce::dontSendNotification);
        suppressCallbacks = false;
    }

    void EventActionPanel::applyAssetPatchValue()
    {
        if (suppressCallbacks)
            return;

        auto* action = selectedAction();
        if (action == nullptr || action->kind != RuntimeActionKind::setNodeProps || assetPatchKeys.empty())
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

        patchEditor.setText(action->patch.size() > 0 ? juce::JSON::toString(patchToVar(action->patch), true)
                                                      : juce::String(),
                            juce::dontSendNotification);

        if (commitBindings("asset-patch-edit"))
            setStatus("Action updated.", kStatusOk);
    }

    void EventActionPanel::createBindingFromToolbar()
    {
        const auto sourceIndex = sourceCombo.getSelectedItemIndex();
        const auto eventIndex = eventCombo.getSelectedItemIndex();
        if (sourceIndex < 0 || sourceIndex >= static_cast<int>(widgetOptions.size()))
        {
            setStatus("No source widget is selected.", kStatusWarn);
            return;
        }
        const auto& source = widgetOptions[static_cast<size_t>(sourceIndex)];
        if (eventIndex < 0 || eventIndex >= static_cast<int>(source.events.size()))
        {
            setStatus("No supported event for selected widget.", kStatusWarn);
            return;
        }

        bindings.push_back(makeDefaultBinding(source.id, source.events[static_cast<size_t>(eventIndex)].key));
        selectedBindingId = bindings.back().id;
        selectedActionRow = -1;
        if (commitBindings("create-binding"))
            setStatus("Binding created.", kStatusOk);
    }

    void EventActionPanel::duplicateSelectedBinding()
    {
        const auto index = selectedBindingModelIndex();
        if (index < 0 || index >= static_cast<int>(bindings.size()))
            return;

        auto copy = bindings[static_cast<size_t>(index)];
        copy.id = nextBindingId();
        copy.name = copy.name.isNotEmpty() ? (copy.name + " Copy") : juce::String("Binding Copy");
        bindings.insert(bindings.begin() + index + 1, std::move(copy));
        selectedBindingId = bindings[static_cast<size_t>(index + 1)].id;
        selectedActionRow = -1;
        if (commitBindings("duplicate-binding"))
            setStatus("Binding duplicated.", kStatusOk);
    }

    void EventActionPanel::deleteSelectedBinding()
    {
        const auto index = selectedBindingModelIndex();
        if (index < 0 || index >= static_cast<int>(bindings.size()))
            return;

        bindings.erase(bindings.begin() + index);
        selectedBindingId = kRootId;
        selectedActionRow = -1;
        if (commitBindings("delete-binding"))
            setStatus("Binding deleted.", kStatusOk);
    }

    void EventActionPanel::addAction()
    {
        auto* binding = selectedBinding();
        if (binding == nullptr)
            return;

        binding->actions.push_back(makeDefaultAction(RuntimeActionKind::setRuntimeParam, binding->sourceWidgetId));
        selectedActionRow = static_cast<int>(binding->actions.size()) - 1;
        if (commitBindings("add-action"))
            setStatus("Action added.", kStatusOk);
    }

    void EventActionPanel::deleteAction()
    {
        auto* binding = selectedBinding();
        const auto row = actionList.getSelectedRow();
        if (binding == nullptr || row < 0 || row >= static_cast<int>(binding->actions.size()))
            return;

        binding->actions.erase(binding->actions.begin() + row);
        if (binding->actions.empty())
        {
            selectedActionRow = -1;
        }
        else
        {
            selectedActionRow = juce::jlimit(0, static_cast<int>(binding->actions.size()) - 1, row);
        }
        if (commitBindings("delete-action"))
            setStatus("Action deleted.", kStatusOk);
    }

    void EventActionPanel::moveActionUp()
    {
        auto* binding = selectedBinding();
        const auto row = actionList.getSelectedRow();
        if (binding == nullptr || row <= 0 || row >= static_cast<int>(binding->actions.size()))
            return;

        std::swap(binding->actions[static_cast<size_t>(row)],
                  binding->actions[static_cast<size_t>(row - 1)]);
        selectedActionRow = row - 1;
        if (commitBindings("move-action-up"))
            setStatus("Action moved.", kStatusOk);
    }

    void EventActionPanel::moveActionDown()
    {
        auto* binding = selectedBinding();
        const auto row = actionList.getSelectedRow();
        if (binding == nullptr || row < 0 || row >= static_cast<int>(binding->actions.size()) - 1)
            return;

        std::swap(binding->actions[static_cast<size_t>(row)],
                  binding->actions[static_cast<size_t>(row + 1)]);
        selectedActionRow = row + 1;
        if (commitBindings("move-action-down"))
            setStatus("Action moved.", kStatusOk);
    }

    void EventActionPanel::applyBindingMeta()
    {
        if (suppressCallbacks)
            return;

        auto* binding = selectedBinding();
        if (binding == nullptr)
            return;

        binding->name = bindingNameEditor.getText().trim();
        binding->enabled = bindingEnabledToggle.getToggleState();
        if (commitBindings("binding-meta"))
            setStatus("Binding updated.", kStatusOk);
    }

    void EventActionPanel::applyActionKind()
    {
        if (suppressCallbacks)
            return;

        auto* binding = selectedBinding();
        auto* action = selectedAction();
        if (binding == nullptr || action == nullptr)
            return;

        auto parsedKind = actionKindFromComboId(actionKindCombo.getSelectedId());
        if (!parsedKind.has_value())
            parsedKind = actionKindFromLabel(actionKindCombo.getText());
        if (!parsedKind.has_value())
            return;

        if (action->kind == *parsedKind)
        {
            setStatus("Action kind unchanged.", kStatusInfo);
            return;
        }

        const auto selectedRow = actionList.getSelectedRow();
        *action = makeDefaultAction(*parsedKind, binding->sourceWidgetId);
        selectedActionRow = selectedRow >= 0 ? selectedRow : selectedActionRow;

        // Ensure the action list/detail are redrawn immediately with the new kind.
        actionList.updateContent();
        actionList.repaint();
        bindingList.repaint();
        refreshDetailEditors();

        if (commitBindings("action-kind"))
        {
            setStatus("Action kind updated.", kStatusOk);
            return;
        }

        // Document commit failed; rollback local edits to keep UI/model consistent.
        setStatus("Failed to commit action kind update.", kStatusError);
        refreshFromDocument();
    }

    void EventActionPanel::applySelectedAction()
    {
        if (suppressCallbacks)
            return;

        auto* action = selectedAction();
        if (action == nullptr)
            return;

        switch (action->kind)
        {
            case RuntimeActionKind::setRuntimeParam:
                action->paramKey = paramKeyEditor.getText().trim();
                action->value = parseRuntimeValue(valueEditor.getText());
                break;

            case RuntimeActionKind::adjustRuntimeParam:
            {
                action->paramKey = paramKeyEditor.getText().trim();
                const auto parsed = parseNumber(deltaEditor.getText());
                if (!parsed.has_value() || !std::isfinite(*parsed))
                {
                    setStatus("Delta must be finite number.", kStatusError);
                    return;
                }
                action->delta = *parsed;
                break;
            }

            case RuntimeActionKind::toggleRuntimeParam:
                action->paramKey = paramKeyEditor.getText().trim();
                break;

            case RuntimeActionKind::setNodeProps:
            {
                auto parsedKind = nodeKindFromComboId(targetKindCombo.getSelectedId());
                if (!parsedKind.has_value())
                    parsedKind = nodeKindFromLabel(targetKindCombo.getText());
                if (!parsedKind.has_value())
                {
                    setStatus("Target kind is invalid.", kStatusError);
                    return;
                }

                WidgetId targetId = kRootId;
                if (!parseWidgetId(targetIdEditor.getText(), targetId))
                {
                    setStatus("Target id must be positive integer.", kStatusError);
                    return;
                }

                action->target.kind = *parsedKind;
                action->target.id = targetId;
                action->visible.reset();
                action->locked.reset();
                if (visibleCombo.getSelectedId() == 2) action->visible = true;
                if (visibleCombo.getSelectedId() == 3) action->visible = false;
                if (lockedCombo.getSelectedId() == 2) action->locked = true;
                if (lockedCombo.getSelectedId() == 3) action->locked = false;

                const auto opacityText = opacityEditor.getText().trim();
                if (opacityText.isEmpty())
                {
                    action->opacity.reset();
                }
                else
                {
                    const auto parsedOpacity = parseNumber(opacityText);
                    if (!parsedOpacity.has_value() || !std::isfinite(*parsedOpacity))
                    {
                        setStatus("Opacity must be numeric.", kStatusError);
                        return;
                    }
                    action->opacity = static_cast<float>(*parsedOpacity);
                }

                PropertyBag patch;
                const auto patchResult = parsePatchJson(patchEditor.getText(), patch);
                if (patchResult.failed())
                {
                    setStatus("Patch JSON error: " + patchResult.getErrorMessage(), kStatusError);
                    return;
                }
                action->patch = std::move(patch);
                break;
            }

            case RuntimeActionKind::setNodeBounds:
            {
                WidgetId targetWidgetId = kRootId;
                if (!parseWidgetId(targetIdEditor.getText(), targetWidgetId))
                {
                    setStatus("Target widget id must be positive integer.", kStatusError);
                    return;
                }

                const auto x = parseNumber(boundsXEditor.getText());
                const auto y = parseNumber(boundsYEditor.getText());
                const auto w = parseNumber(boundsWEditor.getText());
                const auto h = parseNumber(boundsHEditor.getText());
                if (!x.has_value() || !y.has_value() || !w.has_value() || !h.has_value())
                {
                    setStatus("Bounds must be numeric.", kStatusError);
                    return;
                }

                action->targetWidgetId = targetWidgetId;
                action->bounds = juce::Rectangle<float>(static_cast<float>(*x),
                                                        static_cast<float>(*y),
                                                        static_cast<float>(*w),
                                                        static_cast<float>(*h));
                break;
            }
        }

        if (commitBindings("action-edit"))
            setStatus("Action updated.", kStatusOk);
    }

    bool EventActionPanel::commitBindings(const juce::String&)
    {
        if (!document.setRuntimeBindings(bindings))
            return false;

        if (onBindingsChanged != nullptr)
            onBindingsChanged();

        refreshFromDocument();
        bindingList.updateContent();
        actionList.updateContent();
        return true;
    }

    void EventActionPanel::setStatus(const juce::String& text, juce::Colour colour)
    {
        statusLabel.setText(text, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, colour);
    }

    RuntimeBindingModel EventActionPanel::makeDefaultBinding(WidgetId sourceWidgetId, const juce::String& eventKey) const
    {
        RuntimeBindingModel binding;
        binding.id = nextBindingId();
        binding.name = "Binding " + juce::String(static_cast<int>(document.snapshot().runtimeBindings.size() + 1));
        binding.enabled = true;
        binding.sourceWidgetId = sourceWidgetId;
        binding.eventKey = eventKey;
        return binding;
    }

    RuntimeActionModel EventActionPanel::makeDefaultAction(RuntimeActionKind kind, WidgetId sourceWidgetId) const
    {
        RuntimeActionModel action;
        action.kind = kind;
        action.paramKey = "param.key";
        action.value = 0.0;
        action.delta = 0.1;
        action.target.kind = NodeKind::widget;
        action.target.id = sourceWidgetId;
        action.targetWidgetId = sourceWidgetId;
        action.bounds = { 0.0f, 0.0f, 120.0f, 28.0f };
        return action;
    }

    WidgetId EventActionPanel::nextBindingId() const
    {
        const auto& snapshot = document.snapshot();
        WidgetId maxId = kRootId;
        for (const auto& widget : snapshot.widgets) maxId = std::max(maxId, widget.id);
        for (const auto& group : snapshot.groups) maxId = std::max(maxId, group.id);
        for (const auto& layer : snapshot.layers) maxId = std::max(maxId, layer.id);
        for (const auto& binding : snapshot.runtimeBindings) maxId = std::max(maxId, binding.id);
        return maxId + 1;
    }

    std::optional<EventActionPanel::WidgetOption> EventActionPanel::findWidgetOption(WidgetId id) const
    {
        const auto it = std::find_if(widgetOptions.begin(),
                                     widgetOptions.end(),
                                     [id](const WidgetOption& option) { return option.id == id; });
        if (it == widgetOptions.end())
            return std::nullopt;
        return *it;
    }

    juce::String EventActionPanel::formatEventLabel(const Widgets::RuntimeEventSpec& eventSpec) const
    {
        const auto label = eventDisplayLabelKo(eventSpec.key).isNotEmpty()
                             ? eventDisplayLabelKo(eventSpec.key)
                             : (eventSpec.displayLabel.trim().isNotEmpty() ? eventSpec.displayLabel : eventSpec.key);
        return label + " (" + eventSpec.key + ")";
    }

    juce::String EventActionPanel::formatEventLabel(WidgetId sourceWidgetId, const juce::String& eventKey) const
    {
        if (const auto source = findWidgetOption(sourceWidgetId); source.has_value())
        {
            const auto it = std::find_if(source->events.begin(),
                                         source->events.end(),
                                         [&eventKey](const Widgets::RuntimeEventSpec& eventSpec)
                                         {
                                             return eventSpec.key == eventKey;
                                         });
            if (it != source->events.end())
                return formatEventLabel(*it);
        }
        const auto localized = eventDisplayLabelKo(eventKey);
        return localized.isNotEmpty() ? (localized + " (" + eventKey + ")") : eventKey;
    }

    juce::String EventActionPanel::actionSummary(const RuntimeActionModel& action) const
    {
        switch (action.kind)
        {
            case RuntimeActionKind::setRuntimeParam:
            {
                auto valueText = runtimeValueToString(action.value);
                if (valueText.length() > 10)
                    valueText = valueText.substring(0, 10) + "...";
                return "SetRuntimeParam " + action.paramKey + "=" + valueText;
            }
            case RuntimeActionKind::adjustRuntimeParam:
                return "AdjustRuntimeParam " + action.paramKey + " by " + juce::String(action.delta, 4);
            case RuntimeActionKind::toggleRuntimeParam:
                return "ToggleRuntimeParam " + action.paramKey;
            case RuntimeActionKind::setNodeProps:
                return "SetNodeProps " + nodeKindLabel(action.target.kind) + ":" + juce::String(action.target.id);
            case RuntimeActionKind::setNodeBounds:
                return "SetNodeBounds widget:" + juce::String(action.targetWidgetId);
        }
        return {};
    }

    juce::String EventActionPanel::actionKindLabel(RuntimeActionKind kind)
    {
        switch (kind)
        {
            case RuntimeActionKind::setRuntimeParam: return "SetRuntimeParam";
            case RuntimeActionKind::adjustRuntimeParam: return "AdjustRuntimeParam";
            case RuntimeActionKind::toggleRuntimeParam: return "ToggleRuntimeParam";
            case RuntimeActionKind::setNodeProps: return "SetNodeProps";
            case RuntimeActionKind::setNodeBounds: return "SetNodeBounds";
        }
        return {};
    }

    std::optional<RuntimeActionKind> EventActionPanel::actionKindFromLabel(const juce::String& label)
    {
        const auto normalized = label.trim();
        if (normalized == "SetRuntimeParam") return RuntimeActionKind::setRuntimeParam;
        if (normalized == "AdjustRuntimeParam") return RuntimeActionKind::adjustRuntimeParam;
        if (normalized == "ToggleRuntimeParam") return RuntimeActionKind::toggleRuntimeParam;
        if (normalized == "SetNodeProps") return RuntimeActionKind::setNodeProps;
        if (normalized == "SetNodeBounds") return RuntimeActionKind::setNodeBounds;
        return std::nullopt;
    }

    juce::String EventActionPanel::nodeKindLabel(NodeKind kind)
    {
        switch (kind)
        {
            case NodeKind::widget: return "widget";
            case NodeKind::group: return "group";
            case NodeKind::layer: return "layer";
        }
        return {};
    }

    std::optional<NodeKind> EventActionPanel::nodeKindFromLabel(const juce::String& label)
    {
        const auto normalized = label.trim();
        if (normalized == "widget") return NodeKind::widget;
        if (normalized == "group") return NodeKind::group;
        if (normalized == "layer") return NodeKind::layer;
        return std::nullopt;
    }

    juce::var EventActionPanel::parseRuntimeValue(const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.isEmpty())
            return juce::var(juce::String());

        juce::var parsed;
        const auto result = juce::JSON::parse(trimmed, parsed);
        if (result.wasOk() && (parsed.isBool() || parsed.isInt() || parsed.isInt64() || parsed.isDouble() || parsed.isString()))
            return parsed;

        if (trimmed.containsOnly("0123456789+-.eE"))
            return juce::var(trimmed.getDoubleValue());

        return juce::var(trimmed);
    }

    juce::String EventActionPanel::runtimeValueToString(const juce::var& value)
    {
        if (value.isVoid()) return {};
        if (value.isBool()) return static_cast<bool>(value) ? "true" : "false";
        if (value.isInt() || value.isInt64() || value.isDouble()) return juce::String(static_cast<double>(value), 8);
        if (value.isString()) return juce::JSON::toString(value, false);
        return juce::JSON::toString(value, true);
    }

    std::optional<double> EventActionPanel::parseNumber(const juce::String& text)
    {
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

    juce::Result EventActionPanel::parsePatchJson(const juce::String& text, PropertyBag& outPatch)
    {
        outPatch.clear();
        const auto trimmed = text.trim();
        if (trimmed.isEmpty())
            return juce::Result::ok();

        juce::var root;
        const auto parseResult = juce::JSON::parse(trimmed, root);
        if (parseResult.failed())
            return juce::Result::fail(parseResult.getErrorMessage());

        const auto* object = root.getDynamicObject();
        if (object == nullptr)
            return juce::Result::fail("Patch must be object");

        const auto& props = object->getProperties();
        for (int i = 0; i < props.size(); ++i)
            outPatch.set(props.getName(i), props.getValueAt(i));

        return validatePropertyBag(outPatch);
    }

    int EventActionPanel::BindingListModel::getNumRows()
    {
        return static_cast<int>(owner.visibleBindingIndices.size());
    }

    void EventActionPanel::BindingListModel::paintListBoxItem(int rowNumber,
                                                              juce::Graphics& g,
                                                              int width,
                                                              int height,
                                                              bool rowIsSelected)
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(owner.visibleBindingIndices.size()))
            return;

        const auto modelIndex = owner.visibleBindingIndices[static_cast<size_t>(rowNumber)];
        if (modelIndex < 0 || modelIndex >= static_cast<int>(owner.bindings.size()))
            return;

        const auto& binding = owner.bindings[static_cast<size_t>(modelIndex)];
        g.setColour(rowIsSelected ? juce::Colour::fromRGB(49, 84, 142).withAlpha(0.84f)
                                  : juce::Colour::fromRGB(23, 29, 39).withAlpha(0.62f));
        g.fillRect(0, 0, width, height);
        g.setColour(juce::Colour::fromRGB(44, 52, 66));
        g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

        auto area = juce::Rectangle<int>(0, 0, width, height).reduced(6, 3);
        auto top = area.removeFromTop(13);

        g.setColour(binding.enabled ? juce::Colour::fromRGB(112, 214, 156) : juce::Colour::fromRGB(130, 136, 148));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(binding.enabled ? "ON" : "OFF", top.removeFromLeft(24), juce::Justification::centredLeft, true);

        g.setColour(juce::Colour::fromRGB(198, 206, 220));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawFittedText(binding.name.isNotEmpty() ? binding.name : juce::String("Binding"),
                         top,
                         juce::Justification::centredLeft,
                         1);

        auto sourceText = owner.findWidgetOption(binding.sourceWidgetId).has_value()
                            ? owner.findWidgetOption(binding.sourceWidgetId)->label
                            : ("Widget #" + juce::String(binding.sourceWidgetId));

        auto eventText = owner.formatEventLabel(binding.sourceWidgetId, binding.eventKey);

        g.setColour(juce::Colour::fromRGB(160, 170, 186));
        g.setFont(juce::FontOptions(9.0f));
        g.drawFittedText(sourceText + " | " + eventText
                             + " | " + juce::String(static_cast<int>(binding.actions.size())) + " actions",
                         area,
                         juce::Justification::centredLeft,
                         1);
    }

    void EventActionPanel::BindingListModel::selectedRowsChanged(int lastRowSelected)
    {
        if (owner.suppressCallbacks)
            return;

        if (lastRowSelected < 0 || lastRowSelected >= static_cast<int>(owner.visibleBindingIndices.size()))
        {
            owner.selectedBindingId = kRootId;
            owner.selectedActionRow = -1;
        }
        else
        {
            const auto modelIndex = owner.visibleBindingIndices[static_cast<size_t>(lastRowSelected)];
            owner.selectedBindingId = owner.bindings[static_cast<size_t>(modelIndex)].id;
            owner.selectedActionRow = 0;
        }

        owner.refreshDetailEditors();
    }

    void EventActionPanel::BindingListModel::listBoxItemClicked(int row, const juce::MouseEvent& event)
    {
        if (!event.mods.isPopupMenu())
            return;

        if (row >= 0)
            owner.bindingList.selectRow(row);

        juce::PopupMenu menu;
        menu.addItem("Duplicate",
                     [safeOwner = juce::Component::SafePointer<EventActionPanel>(&owner)]
                     {
                         if (safeOwner != nullptr)
                             safeOwner->duplicateSelectedBinding();
                     });
        menu.addItem("Delete",
                     [safeOwner = juce::Component::SafePointer<EventActionPanel>(&owner)]
                     {
                         if (safeOwner != nullptr)
                             safeOwner->deleteSelectedBinding();
                     });
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&owner.bindingList));
    }

    int EventActionPanel::ActionListModel::getNumRows()
    {
        const auto* binding = owner.selectedBinding();
        return binding != nullptr ? static_cast<int>(binding->actions.size()) : 0;
    }

    void EventActionPanel::ActionListModel::paintListBoxItem(int rowNumber,
                                                             juce::Graphics& g,
                                                             int width,
                                                             int height,
                                                             bool rowIsSelected)
    {
        const auto* binding = owner.selectedBinding();
        if (binding == nullptr || rowNumber < 0 || rowNumber >= static_cast<int>(binding->actions.size()))
            return;

        g.setColour(rowIsSelected ? juce::Colour::fromRGB(56, 92, 152).withAlpha(0.82f)
                                  : juce::Colour::fromRGB(22, 28, 38).withAlpha(0.58f));
        g.fillRect(0, 0, width, height);
        g.setColour(juce::Colour::fromRGB(44, 52, 66));
        g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

        auto summary = juce::String(rowNumber + 1) + ". "
                     + owner.actionSummary(binding->actions[static_cast<size_t>(rowNumber)]);

        g.setColour(juce::Colour::fromRGB(194, 202, 216));
        g.setFont(juce::FontOptions(9.5f));
        g.drawFittedText(summary,
                         juce::Rectangle<int>(0, 0, width, height).reduced(6, 2),
                         juce::Justification::centredLeft,
                         1);
    }

    void EventActionPanel::ActionListModel::selectedRowsChanged(int lastRowSelected)
    {
        if (owner.suppressCallbacks)
            return;

        owner.selectedActionRow = lastRowSelected;
        owner.refreshDetailEditors();
    }
}
