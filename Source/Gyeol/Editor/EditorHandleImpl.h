#pragma once

#include "Gyeol/Editor/Shell/EditorHistoryBridge.h"
#include "Gyeol/Editor/Shell/EditorLayoutCoordinator.h"
#include "Gyeol/Editor/Shell/EditorModeCoordinator.h"
#include "Gyeol/Editor/Shell/EditorPanelMediator.h"
#include "Gyeol/Public/DocumentHandle.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Gyeol
{
    class EditorHandleImpl
    {
    public:
        struct State
        {
            Shell::EditorLayoutCoordinator::State layout;
            Shell::EditorModeCoordinator::State mode;
            Shell::EditorHistoryBridge::State history;
            Shell::EditorPanelMediator::State panels;
            std::vector<WidgetId> recentWidgetIds;
        };

        EditorHandleImpl() = default;

        Shell::EditorLayoutCoordinator& layoutCoordinator() noexcept;
        const Shell::EditorLayoutCoordinator& layoutCoordinator() const noexcept;

        Shell::EditorModeCoordinator& modeCoordinator() noexcept;
        const Shell::EditorModeCoordinator& modeCoordinator() const noexcept;

        Shell::EditorHistoryBridge& historyBridge() noexcept;
        const Shell::EditorHistoryBridge& historyBridge() const noexcept;

        Shell::EditorPanelMediator& panelMediator() noexcept;
        const Shell::EditorPanelMediator& panelMediator() const noexcept;

        std::uint64_t computeDocumentDigest(const DocumentHandle& document) const;
        bool updateDocumentDigest(State& state, const DocumentHandle& document) const;

        void rememberRecentSelection(State& state,
                                     const DocumentHandle& document,
                                     std::size_t maxCount = 20) const;

    private:
        Shell::EditorLayoutCoordinator layout;
        Shell::EditorModeCoordinator mode;
        Shell::EditorHistoryBridge history;
        Shell::EditorPanelMediator panels;
    };
}
