#include "Gyeol/Editor/Interaction/SelectionEngine.h"

#include <algorithm>

namespace Gyeol::Ui::Interaction
{
    juce::Result SelectionEngine::selectSingle(DocumentHandle& document, WidgetId id)
    {
        if (id <= kRootId)
            return juce::Result::fail("selectSingle requires id > rootId");

        document.selectSingle(id);
        return juce::Result::ok();
    }

    juce::Result SelectionEngine::setSelection(DocumentHandle& document, std::vector<WidgetId> ids)
    {
        document.setSelection(std::move(ids));
        return juce::Result::ok();
    }

    juce::Result SelectionEngine::toggleSelection(DocumentHandle& document, WidgetId id)
    {
        if (id <= kRootId)
            return juce::Result::fail("toggleSelection requires id > rootId");

        auto current = document.editorState().selection;
        if (const auto it = std::find(current.begin(), current.end(), id); it != current.end())
            current.erase(it);
        else
            current.push_back(id);

        document.setSelection(std::move(current));
        return juce::Result::ok();
    }

    juce::Result SelectionEngine::applyMarquee(DocumentHandle& document, const MarqueeSelectionRequest& request)
    {
        (void) request.mode;

        std::vector<WidgetId> nextSelection = request.additive ? document.editorState().selection : std::vector<WidgetId> {};
        for (const auto id : request.hitIds)
        {
            if (id <= kRootId)
                continue;

            const auto it = std::find(nextSelection.begin(), nextSelection.end(), id);
            if (request.toggle)
            {
                if (it != nextSelection.end())
                    nextSelection.erase(it);
                else
                    nextSelection.push_back(id);
            }
            else if (it == nextSelection.end())
            {
                nextSelection.push_back(id);
            }
        }

        document.setSelection(std::move(nextSelection));
        return juce::Result::ok();
    }

    juce::Result SelectionEngine::clear(DocumentHandle& document)
    {
        document.clearSelection();
        return juce::Result::ok();
    }
}
