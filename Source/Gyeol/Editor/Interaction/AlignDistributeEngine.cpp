#include "Gyeol/Editor/Interaction/AlignDistributeEngine.h"

#include <algorithm>
#include <cmath>

namespace Gyeol::Ui::Interaction
{
    namespace
    {
        juce::Rectangle<float> unionBounds(const std::vector<WidgetModel>& widgets)
        {
            if (widgets.empty())
                return {};

            auto result = widgets.front().bounds;
            for (size_t i = 1; i < widgets.size(); ++i)
                result = result.getUnion(widgets[i].bounds);
            return result;
        }

        bool sameBounds(const juce::Rectangle<float>& lhs, const juce::Rectangle<float>& rhs) noexcept
        {
            constexpr float eps = 0.0001f;
            return std::abs(lhs.getX() - rhs.getX()) <= eps
                && std::abs(lhs.getY() - rhs.getY()) <= eps
                && std::abs(lhs.getWidth() - rhs.getWidth()) <= eps
                && std::abs(lhs.getHeight() - rhs.getHeight()) <= eps;
        }
    }

    std::vector<BoundsPatch> AlignDistributeEngine::computeAlignPatches(const std::vector<WidgetModel>& widgets,
                                                                        AlignEdge edge,
                                                                        const AlignOptions& options) const
    {
        std::vector<BoundsPatch> patches;
        if (widgets.empty())
            return patches;

        const auto reference = options.target == AlignTarget::externalBounds
                                   ? options.externalBounds
                                   : unionBounds(widgets);

        for (const auto& widget : widgets)
        {
            auto next = widget.bounds;
            switch (edge)
            {
                case AlignEdge::left:    next.setX(reference.getX()); break;
                case AlignEdge::right:   next.setX(reference.getRight() - next.getWidth()); break;
                case AlignEdge::top:     next.setY(reference.getY()); break;
                case AlignEdge::bottom:  next.setY(reference.getBottom() - next.getHeight()); break;
                case AlignEdge::hCenter: next.setX(reference.getCentreX() - next.getWidth() * 0.5f); break;
                case AlignEdge::vCenter: next.setY(reference.getCentreY() - next.getHeight() * 0.5f); break;
            }

            if (!sameBounds(next, widget.bounds))
                patches.push_back({ widget.id, next });
        }

        return patches;
    }

    std::vector<BoundsPatch> AlignDistributeEngine::computeDistributePatches(const std::vector<WidgetModel>& widgets,
                                                                             DistributeAxis axis) const
    {
        std::vector<BoundsPatch> patches;
        if (widgets.size() < 3)
            return patches;

        std::vector<const WidgetModel*> sorted;
        sorted.reserve(widgets.size());
        for (const auto& widget : widgets)
            sorted.push_back(&widget);

        std::sort(sorted.begin(),
                  sorted.end(),
                  [axis](const WidgetModel* lhs, const WidgetModel* rhs)
                  {
                      return axis == DistributeAxis::horizontal
                                 ? lhs->bounds.getX() < rhs->bounds.getX()
                                 : lhs->bounds.getY() < rhs->bounds.getY();
                  });

        if (axis == DistributeAxis::horizontal)
        {
            const float start = sorted.front()->bounds.getX();
            const float end = sorted.back()->bounds.getRight();

            float totalWidth = 0.0f;
            for (const auto* widget : sorted)
                totalWidth += widget->bounds.getWidth();

            const float gap = (end - start - totalWidth) / static_cast<float>(sorted.size() - 1);
            float cursor = start;
            for (const auto* widget : sorted)
            {
                auto next = widget->bounds;
                next.setX(cursor);
                if (!sameBounds(next, widget->bounds))
                    patches.push_back({ widget->id, next });
                cursor += widget->bounds.getWidth() + gap;
            }
        }
        else
        {
            const float start = sorted.front()->bounds.getY();
            const float end = sorted.back()->bounds.getBottom();

            float totalHeight = 0.0f;
            for (const auto* widget : sorted)
                totalHeight += widget->bounds.getHeight();

            const float gap = (end - start - totalHeight) / static_cast<float>(sorted.size() - 1);
            float cursor = start;
            for (const auto* widget : sorted)
            {
                auto next = widget->bounds;
                next.setY(cursor);
                if (!sameBounds(next, widget->bounds))
                    patches.push_back({ widget->id, next });
                cursor += widget->bounds.getHeight() + gap;
            }
        }

        return patches;
    }

    juce::Result AlignDistributeEngine::applyBoundsPatches(DocumentHandle& document,
                                                           const std::vector<BoundsPatch>& patches) const
    {
        if (patches.empty())
            return juce::Result::ok();

        std::vector<WidgetBoundsUpdate> updates;
        updates.reserve(patches.size());
        for (const auto& patch : patches)
            updates.push_back({ patch.id, patch.bounds });

        if (!document.setWidgetsBounds(updates))
            return juce::Result::fail("Failed to apply bounds patches");

        return juce::Result::ok();
    }
}
