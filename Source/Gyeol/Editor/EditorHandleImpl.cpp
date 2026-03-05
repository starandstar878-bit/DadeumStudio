#include "Gyeol/Editor/EditorHandleImpl.h"

namespace Gyeol
{
    Shell::EditorLayoutCoordinator& EditorHandleImpl::layoutCoordinator() noexcept
    {
        return layout;
    }

    const Shell::EditorLayoutCoordinator& EditorHandleImpl::layoutCoordinator() const noexcept
    {
        return layout;
    }

    Shell::EditorModeCoordinator& EditorHandleImpl::modeCoordinator() noexcept
    {
        return mode;
    }

    const Shell::EditorModeCoordinator& EditorHandleImpl::modeCoordinator() const noexcept
    {
        return mode;
    }

    Shell::EditorHistoryBridge& EditorHandleImpl::historyBridge() noexcept
    {
        return history;
    }

    const Shell::EditorHistoryBridge& EditorHandleImpl::historyBridge() const noexcept
    {
        return history;
    }

    Shell::EditorPanelMediator& EditorHandleImpl::panelMediator() noexcept
    {
        return panels;
    }

    const Shell::EditorPanelMediator& EditorHandleImpl::panelMediator() const noexcept
    {
        return panels;
    }

    std::uint64_t EditorHandleImpl::computeDocumentDigest(
        const DocumentHandle& document) const
    {
        return panels.computeDocumentDigest(document);
    }

    bool EditorHandleImpl::updateDocumentDigest(State& state,
                                                const DocumentHandle& document) const
    {
        return panels.updateDocumentDigest(state.panels, document);
    }

    void EditorHandleImpl::rememberRecentSelection(State& state,
                                                   const DocumentHandle& document,
                                                   std::size_t maxCount) const
    {
        panels.rememberRecentSelection(document, state.recentWidgetIds, maxCount);
    }
}
