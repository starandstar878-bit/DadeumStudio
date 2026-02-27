#pragma once

#include "Gyeol/Public/Types.h"
#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

namespace Gyeol::Ui::Perf
{
    struct HitTestItem
    {
        WidgetId id = kRootId;
        juce::Rectangle<float> bounds;
    };

    class HitTestGrid
    {
    public:
        void setCellSize(float size) noexcept;
        float cellSize() const noexcept;

        void rebuild(const std::vector<HitTestItem>& items);

        std::vector<WidgetId> query(juce::Point<float> point) const;
        std::vector<WidgetId> query(juce::Rectangle<float> area) const;

    private:
        static std::int64_t makeCellKey(int x, int y) noexcept;
        std::vector<std::int64_t> cellsForBounds(juce::Rectangle<float> bounds) const;
        std::vector<WidgetId> queryCandidates(const std::vector<std::int64_t>& cellKeys) const;

        float gridCellSize = 64.0f;
        std::vector<HitTestItem> allItems;
        std::unordered_map<std::int64_t, std::vector<WidgetId>> cellToIds;
    };
}
