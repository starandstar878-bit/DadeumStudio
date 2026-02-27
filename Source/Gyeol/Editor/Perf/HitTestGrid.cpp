#include "Gyeol/Editor/Perf/HitTestGrid.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Gyeol::Ui::Perf
{
    void HitTestGrid::setCellSize(float size) noexcept
    {
        if (std::isfinite(size) && size > 1.0f)
            gridCellSize = size;
    }

    float HitTestGrid::cellSize() const noexcept
    {
        return gridCellSize;
    }

    void HitTestGrid::rebuild(const std::vector<HitTestItem>& items)
    {
        allItems = items;
        cellToIds.clear();

        for (const auto& item : allItems)
        {
            if (item.id <= kRootId)
                continue;

            for (const auto key : cellsForBounds(item.bounds))
                cellToIds[key].push_back(item.id);
        }
    }

    std::vector<WidgetId> HitTestGrid::query(juce::Point<float> point) const
    {
        return query(juce::Rectangle<float>(point.x, point.y, 1.0f, 1.0f));
    }

    std::vector<WidgetId> HitTestGrid::query(juce::Rectangle<float> area) const
    {
        auto candidates = queryCandidates(cellsForBounds(area));
        std::vector<WidgetId> hits;
        hits.reserve(candidates.size());

        for (const auto id : candidates)
        {
            const auto it = std::find_if(allItems.begin(),
                                         allItems.end(),
                                         [id](const HitTestItem& item)
                                         {
                                             return item.id == id;
                                         });
            if (it == allItems.end())
                continue;

            if (area.getWidth() <= 1.0f && area.getHeight() <= 1.0f)
            {
                if (it->bounds.contains(area.getPosition()))
                    hits.push_back(id);
            }
            else if (it->bounds.intersects(area))
            {
                hits.push_back(id);
            }
        }

        return hits;
    }

    std::int64_t HitTestGrid::makeCellKey(int x, int y) noexcept
    {
        return (static_cast<std::int64_t>(x) << 32) | (static_cast<std::uint32_t>(y));
    }

    std::vector<std::int64_t> HitTestGrid::cellsForBounds(juce::Rectangle<float> bounds) const
    {
        std::vector<std::int64_t> keys;

        const int left = static_cast<int>(std::floor(bounds.getX() / gridCellSize));
        const int right = static_cast<int>(std::floor(bounds.getRight() / gridCellSize));
        const int top = static_cast<int>(std::floor(bounds.getY() / gridCellSize));
        const int bottom = static_cast<int>(std::floor(bounds.getBottom() / gridCellSize));

        keys.reserve(static_cast<size_t>((right - left + 1) * (bottom - top + 1)));
        for (int y = top; y <= bottom; ++y)
        {
            for (int x = left; x <= right; ++x)
                keys.push_back(makeCellKey(x, y));
        }

        return keys;
    }

    std::vector<WidgetId> HitTestGrid::queryCandidates(const std::vector<std::int64_t>& cellKeys) const
    {
        std::unordered_set<WidgetId> unique;
        for (const auto key : cellKeys)
        {
            const auto it = cellToIds.find(key);
            if (it == cellToIds.end())
                continue;

            for (const auto id : it->second)
                unique.insert(id);
        }

        std::vector<WidgetId> ids;
        ids.reserve(unique.size());
        for (const auto id : unique)
            ids.push_back(id);
        return ids;
    }
}
