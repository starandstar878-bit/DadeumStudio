#include <JuceHeader.h>

#include "Gyeol/Core/DocumentStore.h"
#include "Gyeol/Core/SceneValidator.h"
#include "Gyeol/Editor/Panels/PropertyEditorFactory.h"
#include "Gyeol/Public/DocumentHandle.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    bool nearlyEqual(float lhs, float rhs, float epsilon = 1.0e-4f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
    }

    const Gyeol::WidgetModel* findWidget(const Gyeol::DocumentModel& document, Gyeol::WidgetId id)
    {
        const auto it = std::find_if(document.widgets.begin(),
                                     document.widgets.end(),
                                     [id](const Gyeol::WidgetModel& widget)
                                     {
                                         return widget.id == id;
                                     });
        return it == document.widgets.end() ? nullptr : &(*it);
    }

    const Gyeol::GroupModel* findGroup(const Gyeol::DocumentModel& document, Gyeol::WidgetId id)
    {
        const auto it = std::find_if(document.groups.begin(),
                                     document.groups.end(),
                                     [id](const Gyeol::GroupModel& group)
                                     {
                                         return group.id == id;
                                     });
        return it == document.groups.end() ? nullptr : &(*it);
    }

    const Gyeol::LayerModel* findLayer(const Gyeol::DocumentModel& document, Gyeol::WidgetId id)
    {
        const auto it = std::find_if(document.layers.begin(),
                                     document.layers.end(),
                                     [id](const Gyeol::LayerModel& layer)
                                     {
                                         return layer.id == id;
                                     });
        return it == document.layers.end() ? nullptr : &(*it);
    }

    juce::Result testActionValidationGuards()
    {
        Gyeol::Core::DocumentStore store;

        const auto rootLayerId = store.snapshot().layers.front().id;

        Gyeol::CreateWidgetPayload widgetPayload;
        widgetPayload.type = Gyeol::WidgetType::label;
        widgetPayload.parent.kind = Gyeol::ParentKind::layer;
        widgetPayload.parent.id = rootLayerId;
        widgetPayload.bounds = { 10.0f, 20.0f, 120.0f, 28.0f };

        Gyeol::CreateAction createAction;
        createAction.kind = Gyeol::NodeKind::widget;
        createAction.payload = widgetPayload;

        std::vector<Gyeol::WidgetId> createdIds;
        auto result = store.apply(createAction, &createdIds);
        if (result.failed())
            return juce::Result::fail("create widget failed: " + result.getErrorMessage());
        if (createdIds.size() != 1)
            return juce::Result::fail("create widget returned unexpected id count");

        const auto widgetId = createdIds.front();

        Gyeol::SetPropsAction invalidOpacity;
        invalidOpacity.kind = Gyeol::NodeKind::widget;
        invalidOpacity.ids = { widgetId };
        Gyeol::WidgetPropsPatch opacityPatch;
        opacityPatch.opacity = 1.5f;
        invalidOpacity.patch = opacityPatch;
        if (store.apply(invalidOpacity).wasOk())
            return juce::Result::fail("invalid opacity patch must fail");

        Gyeol::SetBoundsAction invalidBounds;
        Gyeol::SetBoundsAction::Item nanItem;
        nanItem.id = widgetId;
        nanItem.bounds = { 0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN(), 20.0f };
        invalidBounds.items = { nanItem };
        if (store.apply(invalidBounds).wasOk())
            return juce::Result::fail("nan bounds must fail");

        Gyeol::SetBoundsAction negativeBounds;
        Gyeol::SetBoundsAction::Item negativeItem;
        negativeItem.id = widgetId;
        negativeItem.bounds = { 0.0f, 0.0f, -10.0f, 20.0f };
        negativeBounds.items = { negativeItem };
        if (store.apply(negativeBounds).wasOk())
            return juce::Result::fail("negative width bounds must fail");

        return juce::Result::ok();
    }

    juce::Result testCoalescedPreviewRollbackAndCommit()
    {
        Gyeol::DocumentHandle document;
        const auto layerId = document.snapshot().layers.front().id;

        const auto widgetId = document.addWidget(Gyeol::WidgetType::knob,
                                                 { 30.0f, 30.0f, 64.0f, 64.0f },
                                                 {},
                                                 layerId);
        if (widgetId <= Gyeol::kRootId)
            return juce::Result::fail("addWidget failed");

        if (!document.beginCoalescedEdit("opacity-test"))
            return juce::Result::fail("beginCoalescedEdit failed");

        Gyeol::SetPropsAction previewAction;
        previewAction.kind = Gyeol::NodeKind::widget;
        previewAction.ids = { widgetId };
        Gyeol::WidgetPropsPatch previewPatch;
        previewPatch.opacity = 0.25f;
        previewAction.patch = previewPatch;

        if (!document.previewSetProps(previewAction))
            return juce::Result::fail("previewSetProps failed");

        const auto* previewWidget = findWidget(document.snapshot(), widgetId);
        if (previewWidget == nullptr || !nearlyEqual(previewWidget->opacity, 0.25f))
            return juce::Result::fail("preview state did not apply");

        if (!document.endCoalescedEdit("opacity-test", false))
            return juce::Result::fail("coalesced cancel failed");

        const auto* restoredWidget = findWidget(document.snapshot(), widgetId);
        if (restoredWidget == nullptr || !nearlyEqual(restoredWidget->opacity, 1.0f))
            return juce::Result::fail("cancel did not rollback preview state");

        if (!document.beginCoalescedEdit("opacity-test-commit"))
            return juce::Result::fail("beginCoalescedEdit(commit) failed");

        previewPatch.opacity = 0.40f;
        previewAction.patch = previewPatch;
        if (!document.previewSetProps(previewAction))
            return juce::Result::fail("previewSetProps(commit) failed");

        if (!document.endCoalescedEdit("opacity-test-commit", true))
            return juce::Result::fail("coalesced commit failed");

        const auto* committedWidget = findWidget(document.snapshot(), widgetId);
        if (committedWidget == nullptr || !nearlyEqual(committedWidget->opacity, 0.40f))
            return juce::Result::fail("commit state mismatch");

        if (!document.canUndo() || !document.undo())
            return juce::Result::fail("undo after coalesced commit failed");

        const auto* undoneWidget = findWidget(document.snapshot(), widgetId);
        if (undoneWidget == nullptr || !nearlyEqual(undoneWidget->opacity, 1.0f))
            return juce::Result::fail("undo did not restore baseline opacity");

        return juce::Result::ok();
    }

    juce::Result testPropertyParserConstraints()
    {
        using Gyeol::Ui::Panels::PropertyEditorFactory;

        Gyeol::Widgets::WidgetPropertySpec numberSpec;
        numberSpec.kind = Gyeol::Widgets::WidgetPropertyKind::number;
        numberSpec.minValue = 0.0;
        numberSpec.maxValue = 1.0;

        juce::var value;
        if (!PropertyEditorFactory::parseValue(numberSpec, "0.5", value))
            return juce::Result::fail("number parse valid case failed");
        if (PropertyEditorFactory::parseValue(numberSpec, "nan", value))
            return juce::Result::fail("number parse must reject nan");
        if (PropertyEditorFactory::parseValue(numberSpec, "1.2", value))
            return juce::Result::fail("number parse must reject out-of-range text");
        if (PropertyEditorFactory::normalizeValue(numberSpec, juce::var(1.2), value))
            return juce::Result::fail("number normalize must reject out-of-range numeric value");

        Gyeol::Widgets::WidgetPropertySpec intSpec;
        intSpec.kind = Gyeol::Widgets::WidgetPropertyKind::integer;
        if (PropertyEditorFactory::parseValue(intSpec, "9223372036854775808", value))
            return juce::Result::fail("integer parse must reject int64 overflow text");
        if (!PropertyEditorFactory::normalizeValue(intSpec, juce::var(42.0), value))
            return juce::Result::fail("integer normalize valid numeric failed");
        if (PropertyEditorFactory::normalizeValue(intSpec, juce::var(42.5), value))
            return juce::Result::fail("integer normalize must reject fractional value");

        Gyeol::Widgets::WidgetPropertySpec vec2Spec;
        vec2Spec.kind = Gyeol::Widgets::WidgetPropertyKind::vec2;
        if (!PropertyEditorFactory::parseValue(vec2Spec, "10, 20", value))
            return juce::Result::fail("vec2 parse failed");

        Gyeol::Widgets::WidgetPropertySpec colorSpec;
        colorSpec.kind = Gyeol::Widgets::WidgetPropertyKind::color;
        colorSpec.colorStorage = Gyeol::Widgets::ColorStorage::rgbaObject01;

        auto* rgba = new juce::DynamicObject();
        rgba->setProperty("r", 0.2);
        rgba->setProperty("g", 0.4);
        rgba->setProperty("b", 0.6);
        rgba->setProperty("a", 1.0);
        if (!PropertyEditorFactory::normalizeValue(colorSpec, juce::var(rgba), value))
            return juce::Result::fail("rgba normalize valid case failed");

        auto* invalidRgba = new juce::DynamicObject();
        invalidRgba->setProperty("r", 0.2);
        invalidRgba->setProperty("g", 0.4);
        invalidRgba->setProperty("b", 0.6);
        invalidRgba->setProperty("a", 2.0);
        if (PropertyEditorFactory::normalizeValue(colorSpec, juce::var(invalidRgba), value))
            return juce::Result::fail("rgba normalize must reject out-of-range alpha");

        return juce::Result::ok();
    }

    juce::Result testRoundTripLayerGroupWidgetModel()
    {
        Gyeol::DocumentHandle document;

        const auto layer1Id = document.snapshot().layers.front().id;

        Gyeol::CreateLayerPayload layerPayload;
        layerPayload.name = "Layer 2";
        Gyeol::CreateAction createLayer;
        createLayer.kind = Gyeol::NodeKind::layer;
        createLayer.payload = layerPayload;

        const auto layer2Id = document.createNode(createLayer);
        if (layer2Id <= Gyeol::kRootId)
            return juce::Result::fail("create layer failed");

        const auto w1 = document.addWidget(Gyeol::WidgetType::button, { 20.0f, 20.0f, 120.0f, 40.0f }, {}, layer1Id);
        const auto w2 = document.addWidget(Gyeol::WidgetType::label, { 20.0f, 80.0f, 120.0f, 28.0f }, {}, layer1Id);
        const auto w3 = document.addWidget(Gyeol::WidgetType::meter, { 220.0f, 20.0f, 36.0f, 120.0f }, {}, layer2Id);
        if (w1 <= Gyeol::kRootId || w2 <= Gyeol::kRootId || w3 <= Gyeol::kRootId)
            return juce::Result::fail("addWidget in round-trip setup failed");

        Gyeol::CreateGroupPayload groupPayload;
        groupPayload.parent.kind = Gyeol::ParentKind::layer;
        groupPayload.parent.id = layer1Id;
        groupPayload.name = "Group A";
        groupPayload.members = { { Gyeol::NodeKind::widget, w1 },
                                 { Gyeol::NodeKind::widget, w2 } };

        Gyeol::CreateAction createGroup;
        createGroup.kind = Gyeol::NodeKind::group;
        createGroup.payload = groupPayload;

        const auto groupId = document.createNode(createGroup);
        if (groupId <= Gyeol::kRootId)
            return juce::Result::fail("create group failed");

        Gyeol::SetPropsAction setGroupProps;
        setGroupProps.kind = Gyeol::NodeKind::group;
        setGroupProps.ids = { groupId };
        Gyeol::GroupPropsPatch groupPatch;
        groupPatch.opacity = 0.45f;
        groupPatch.locked = true;
        setGroupProps.patch = groupPatch;
        if (!document.setProps(setGroupProps))
            return juce::Result::fail("set group props failed");

        Gyeol::SetPropsAction setWidgetProps;
        setWidgetProps.kind = Gyeol::NodeKind::widget;
        setWidgetProps.ids = { w3 };
        Gyeol::WidgetPropsPatch widgetPatch;
        widgetPatch.visible = false;
        widgetPatch.opacity = 0.2f;
        setWidgetProps.patch = widgetPatch;
        if (!document.setProps(setWidgetProps))
            return juce::Result::fail("set widget props failed");

        Gyeol::SetPropsAction setLayerProps;
        setLayerProps.kind = Gyeol::NodeKind::layer;
        setLayerProps.ids = { layer2Id };
        Gyeol::LayerPropsPatch layerPatch;
        layerPatch.visible = false;
        setLayerProps.patch = layerPatch;
        if (!document.setProps(setLayerProps))
            return juce::Result::fail("set layer props failed");

        document.setSelection({ w2, w3 });

        const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("gyeol_phase3_inspector_roundtrip.json");
        auto ioResult = document.saveToFile(tempFile);
        if (ioResult.failed())
            return juce::Result::fail("save failed: " + ioResult.getErrorMessage());

        Gyeol::DocumentHandle loaded;
        ioResult = loaded.loadFromFile(tempFile);
        tempFile.deleteFile();
        if (ioResult.failed())
            return juce::Result::fail("load failed: " + ioResult.getErrorMessage());

        const auto sceneValidation = Gyeol::Core::SceneValidator::validateScene(loaded.snapshot(), &loaded.editorState());
        if (sceneValidation.failed())
            return juce::Result::fail("loaded scene validation failed: " + sceneValidation.getErrorMessage());

        if (loaded.editorState().selection != std::vector<Gyeol::WidgetId> { w2, w3 })
            return juce::Result::fail("selection round-trip mismatch");

        const auto* loadedGroup = findGroup(loaded.snapshot(), groupId);
        if (loadedGroup == nullptr || !nearlyEqual(loadedGroup->opacity, 0.45f) || !loadedGroup->locked)
            return juce::Result::fail("group round-trip mismatch");

        const auto* loadedLayer2 = findLayer(loaded.snapshot(), layer2Id);
        if (loadedLayer2 == nullptr || loadedLayer2->visible)
            return juce::Result::fail("layer round-trip mismatch");

        const auto* loadedW3 = findWidget(loaded.snapshot(), w3);
        if (loadedW3 == nullptr || loadedW3->visible || !nearlyEqual(loadedW3->opacity, 0.2f))
            return juce::Result::fail("widget round-trip mismatch");

        return juce::Result::ok();
    }

    juce::Result testUndoRedo100()
    {
        Gyeol::DocumentHandle document;
        const auto layerId = document.snapshot().layers.front().id;
        const auto widgetId = document.addWidget(Gyeol::WidgetType::slider,
                                                 { 100.0f, 100.0f, 180.0f, 40.0f },
                                                 {},
                                                 layerId);
        if (widgetId <= Gyeol::kRootId)
            return juce::Result::fail("addWidget for undo/redo test failed");

        for (int i = 0; i < 100; ++i)
        {
            if (!document.moveWidget(widgetId, { 1.0f, 0.0f }))
                return juce::Result::fail("moveWidget failed at step " + juce::String(i));
        }

        for (int i = 0; i < 100; ++i)
        {
            if (!document.undo())
                return juce::Result::fail("undo failed at step " + juce::String(i));
        }

        const auto* afterUndo = findWidget(document.snapshot(), widgetId);
        if (afterUndo == nullptr || !nearlyEqual(afterUndo->bounds.getX(), 100.0f))
            return juce::Result::fail("undo final position mismatch");

        for (int i = 0; i < 100; ++i)
        {
            if (!document.redo())
                return juce::Result::fail("redo failed at step " + juce::String(i));
        }

        const auto* afterRedo = findWidget(document.snapshot(), widgetId);
        if (afterRedo == nullptr || !nearlyEqual(afterRedo->bounds.getX(), 200.0f))
            return juce::Result::fail("redo final position mismatch");

        return juce::Result::ok();
    }
}

int main()
{
    const std::vector<std::pair<const char*, std::function<juce::Result()>>> tests =
    {
        { "Action validation guards", testActionValidationGuards },
        { "Coalesced preview rollback/commit", testCoalescedPreviewRollbackAndCommit },
        { "Property parser constraints", testPropertyParserConstraints },
        { "Round-trip layer/group/widget model", testRoundTripLayerGroupWidgetModel },
        { "Undo/Redo 100", testUndoRedo100 }
    };

    for (const auto& [name, run] : tests)
    {
        const auto result = run();
        if (result.failed())
        {
            std::cerr << "[FAIL] " << name << ": " << result.getErrorMessage() << std::endl;
            return 1;
        }

        std::cout << "[PASS] " << name << std::endl;
    }

    std::cout << "Phase 3 inspector regression smoke passed." << std::endl;
    return 0;
}
