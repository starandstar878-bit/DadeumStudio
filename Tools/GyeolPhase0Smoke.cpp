#include <JuceHeader.h>
#include "Gyeol/Core/DocumentStore.h"
#include "Gyeol/Core/SceneValidator.h"
#include "Gyeol/Serialization/DocumentJson.h"
#include <cmath>
#include <iostream>
#include <optional>

namespace
{
    bool nearlyEqual(float lhs, float rhs, float epsilon = 1.0e-4f)
    {
        return std::fabs(lhs - rhs) <= epsilon;
    }

    bool equalBounds(const juce::Rectangle<float>& lhs, const juce::Rectangle<float>& rhs)
    {
        return nearlyEqual(lhs.getX(), rhs.getX())
            && nearlyEqual(lhs.getY(), rhs.getY())
            && nearlyEqual(lhs.getWidth(), rhs.getWidth())
            && nearlyEqual(lhs.getHeight(), rhs.getHeight());
    }

    bool equalPropertyBag(const Gyeol::PropertyBag& lhs, const Gyeol::PropertyBag& rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (int i = 0; i < lhs.size(); ++i)
        {
            const auto key = lhs.getName(i);
            if (!rhs.contains(key))
                return false;
            if (rhs[key] != lhs.getValueAt(i))
                return false;
        }

        return true;
    }

    bool equalDocument(const Gyeol::DocumentModel& lhs, const Gyeol::DocumentModel& rhs)
    {
        if (lhs.schemaVersion.major != rhs.schemaVersion.major
            || lhs.schemaVersion.minor != rhs.schemaVersion.minor
            || lhs.schemaVersion.patch != rhs.schemaVersion.patch)
        {
            return false;
        }

        if (lhs.widgets.size() != rhs.widgets.size())
            return false;

        for (size_t i = 0; i < lhs.widgets.size(); ++i)
        {
            const auto& lw = lhs.widgets[i];
            const auto& rw = rhs.widgets[i];

            if (lw.id != rw.id || lw.type != rw.type)
                return false;
            if (!equalBounds(lw.bounds, rw.bounds))
                return false;
            if (!equalPropertyBag(lw.properties, rw.properties))
                return false;
        }

        return true;
    }

    bool equalEditorState(const Gyeol::EditorStateModel& lhs, const Gyeol::EditorStateModel& rhs)
    {
        return lhs.selection == rhs.selection;
    }

    juce::Result testCreateDeleteMoveSelection()
    {
        Gyeol::Core::DocumentStore store;

        Gyeol::CreateWidgetAction createAction;
        createAction.type = Gyeol::WidgetType::button;
        createAction.bounds = { 10.0f, 20.0f, 100.0f, 40.0f };
        createAction.properties.set("name", "btnA");

        std::vector<Gyeol::WidgetId> createdIds;
        auto result = store.apply(createAction, &createdIds);
        if (result.failed())
            return juce::Result::fail("create failed: " + result.getErrorMessage());
        if (createdIds.size() != 1 || store.snapshot().widgets.size() != 1)
            return juce::Result::fail("create result mismatch");

        const auto id = createdIds.front();

        Gyeol::SetWidgetPropsAction moveAction;
        moveAction.ids = { id };
        moveAction.bounds = juce::Rectangle<float>(33.0f, 55.0f, 100.0f, 40.0f);

        result = store.apply(moveAction, nullptr);
        if (result.failed())
            return juce::Result::fail("move failed: " + result.getErrorMessage());

        const auto& moved = store.snapshot().widgets.front();
        if (!equalBounds(moved.bounds, { 33.0f, 55.0f, 100.0f, 40.0f }))
            return juce::Result::fail("move bounds mismatch");

        Gyeol::EditorStateModel editor;
        editor.selection = { id };
        result = Gyeol::Core::SceneValidator::validateScene(store.snapshot(), &editor);
        if (result.failed())
            return juce::Result::fail("selection validation failed: " + result.getErrorMessage());

        Gyeol::DeleteWidgetsAction deleteAction;
        deleteAction.ids = { id };
        result = store.apply(deleteAction, nullptr);
        if (result.failed())
            return juce::Result::fail("delete failed: " + result.getErrorMessage());
        if (!store.snapshot().widgets.empty())
            return juce::Result::fail("delete result mismatch");

        return juce::Result::ok();
    }

    juce::Result testUndoRedo20()
    {
        Gyeol::Core::DocumentStore store;

        for (int i = 0; i < 20; ++i)
        {
            Gyeol::CreateWidgetAction action;
            action.type = Gyeol::WidgetType::label;
            action.bounds = { static_cast<float>(i), 0.0f, 10.0f, 10.0f };

            const auto result = store.apply(action, nullptr);
            if (result.failed())
                return juce::Result::fail("create in undo test failed at step " + juce::String(i));
        }

        if (store.snapshot().widgets.size() != 20)
            return juce::Result::fail("undo test setup size mismatch");

        for (int i = 0; i < 20; ++i)
        {
            if (!store.undo())
                return juce::Result::fail("undo failed at step " + juce::String(i));
        }

        if (!store.snapshot().widgets.empty())
            return juce::Result::fail("undo final state mismatch");

        for (int i = 0; i < 20; ++i)
        {
            if (!store.redo())
                return juce::Result::fail("redo failed at step " + juce::String(i));
        }

        if (store.snapshot().widgets.size() != 20)
            return juce::Result::fail("redo final state mismatch");

        return juce::Result::ok();
    }

    juce::Result testSaveReloadRoundTrip()
    {
        Gyeol::DocumentModel sourceDocument;
        sourceDocument.schemaVersion = Gyeol::currentSchemaVersion();

        Gyeol::WidgetModel widget;
        widget.id = 10;
        widget.type = Gyeol::WidgetType::slider;
        widget.bounds = { 5.0f, 6.0f, 123.0f, 33.0f };
        widget.properties.set("title", "gain");
        sourceDocument.widgets.push_back(widget);

        Gyeol::EditorStateModel sourceEditor;
        sourceEditor.selection = { 10 };

        const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("gyeol_phase0_smoke.json");

        auto result = Gyeol::Serialization::saveDocumentToFile(tempFile, sourceDocument, sourceEditor);
        if (result.failed())
            return juce::Result::fail("save failed: " + result.getErrorMessage());

        Gyeol::DocumentModel loadedDocument;
        Gyeol::EditorStateModel loadedEditor;
        result = Gyeol::Serialization::loadDocumentFromFile(tempFile, loadedDocument, loadedEditor);
        tempFile.deleteFile();

        if (result.failed())
            return juce::Result::fail("load failed: " + result.getErrorMessage());
        if (!equalDocument(sourceDocument, loadedDocument))
            return juce::Result::fail("round-trip document mismatch");
        if (!equalEditorState(sourceEditor, loadedEditor))
            return juce::Result::fail("round-trip editor mismatch");

        return juce::Result::ok();
    }
}

int main()
{
    const auto createDeleteMoveSelection = testCreateDeleteMoveSelection();
    if (createDeleteMoveSelection.failed())
    {
        std::cerr << "[FAIL] Create/Delete/Move/Selection: "
                  << createDeleteMoveSelection.getErrorMessage() << std::endl;
        return 1;
    }
    std::cout << "[PASS] Create/Delete/Move/Selection" << std::endl;

    const auto undoRedo = testUndoRedo20();
    if (undoRedo.failed())
    {
        std::cerr << "[FAIL] Undo/Redo 20x: "
                  << undoRedo.getErrorMessage() << std::endl;
        return 1;
    }
    std::cout << "[PASS] Undo/Redo 20x" << std::endl;

    const auto roundTrip = testSaveReloadRoundTrip();
    if (roundTrip.failed())
    {
        std::cerr << "[FAIL] Save/Load round-trip: "
                  << roundTrip.getErrorMessage() << std::endl;
        return 1;
    }
    std::cout << "[PASS] Save/Load round-trip" << std::endl;

    std::cout << "Phase 0 smoke tests passed." << std::endl;
    return 0;
}
