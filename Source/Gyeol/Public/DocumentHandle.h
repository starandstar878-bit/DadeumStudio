#pragma once

#include "Gyeol/Public/Types.h"
#include <memory>

namespace Gyeol
{
    class DocumentHandle
    {
    public:
        DocumentHandle();
        ~DocumentHandle();
        DocumentHandle(DocumentHandle&&) noexcept;
        DocumentHandle& operator=(DocumentHandle&&) noexcept;

        DocumentHandle(const DocumentHandle&) = delete;
        DocumentHandle& operator=(const DocumentHandle&) = delete;

        const DocumentModel& snapshot() const noexcept;
        const EditorStateModel& editorState() const noexcept;

        WidgetId addWidget(WidgetType type, juce::Rectangle<float> bounds, const PropertyBag& properties = {});
        bool removeWidget(const WidgetId& id);
        bool moveWidget(const WidgetId& id, juce::Point<float> delta);

        void selectSingle(const WidgetId& id);
        void setSelection(std::vector<WidgetId> selection);
        void clearSelection();

        bool canUndo() const noexcept;
        bool canRedo() const noexcept;
        bool undo();
        bool redo();

        juce::Result saveToFile(const juce::File& file) const;
        juce::Result loadFromFile(const juce::File& file);

    private:
        class Impl;
        std::unique_ptr<Impl> impl;
    };
}
