#include "Gyeol/Editor/Interaction/SnapEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Gyeol::Ui::Interaction
{
    namespace
    {
        struct AxisCandidate
        {
            bool hasValue = false;
            float snappedValue = 0.0f;
            float guideValue = 0.0f;
            float delta = std::numeric_limits<float>::max();
            SnapKind kind = SnapKind::none;
            std::optional<SmartSpacingHint> spacingHint;
        };

        float snapScalar(float value, float step)
        {
            if (step <= 0.0f)
                return value;
            return std::round(value / step) * step;
        }

        int snapKindPriority(SnapKind kind)
        {
            switch (kind)
            {
                case SnapKind::none: return 0;
                case SnapKind::grid: return 1;
                case SnapKind::smartAlign: return 2;
                case SnapKind::smartSpacing: return 3;
            }

            return 0;
        }

        void considerAxisCandidate(AxisCandidate& candidate,
                                   float currentValue,
                                   float targetValue,
                                   float guideValue,
                                   float tolerance,
                                   SnapKind kind,
                                   std::optional<SmartSpacingHint> spacingHint = std::nullopt)
        {
            constexpr float kCompareEpsilon = 0.0001f;
            const auto delta = std::abs(targetValue - currentValue);
            if (delta > tolerance)
                return;

            const auto betterByDistance = !candidate.hasValue || delta < candidate.delta - kCompareEpsilon;
            const auto tieBreakByKind = candidate.hasValue
                                     && std::abs(delta - candidate.delta) <= kCompareEpsilon
                                     && snapKindPriority(kind) > snapKindPriority(candidate.kind);
            if (betterByDistance || tieBreakByKind)
            {
                candidate.hasValue = true;
                candidate.delta = delta;
                candidate.snappedValue = targetValue;
                candidate.guideValue = guideValue;
                candidate.kind = kind;
                candidate.spacingHint = std::move(spacingHint);
            }
        }
    }

    SnapResult SnapEngine::compute(const SnapRequest& request) const
    {
        SnapResult result;
        result.snappedBounds = request.proposedBounds;

        if (!request.settings.snapEnabled)
            return result;

        const auto tolerance = std::max(0.0f, request.settings.tolerance);
        AxisCandidate xCandidate;
        AxisCandidate yCandidate;

        auto trySnapX = [&](float targetX, float guideX)
        {
            considerAxisCandidate(xCandidate,
                                  result.snappedBounds.getX(),
                                  targetX,
                                  guideX,
                                  tolerance,
                                  SnapKind::smartAlign);
        };

        auto trySnapY = [&](float targetY, float guideY)
        {
            considerAxisCandidate(yCandidate,
                                  result.snappedBounds.getY(),
                                  targetY,
                                  guideY,
                                  tolerance,
                                  SnapKind::smartAlign);
        };

        if (request.settings.enableGridSnap && request.settings.gridSize > 0.0f)
        {
            const auto snappedX = snapScalar(result.snappedBounds.getX(), request.settings.gridSize);
            const auto snappedY = snapScalar(result.snappedBounds.getY(), request.settings.gridSize);
            considerAxisCandidate(xCandidate,
                                  result.snappedBounds.getX(),
                                  snappedX,
                                  snappedX,
                                  tolerance,
                                  SnapKind::grid);
            considerAxisCandidate(yCandidate,
                                  result.snappedBounds.getY(),
                                  snappedY,
                                  snappedY,
                                  tolerance,
                                  SnapKind::grid);
        }

        if (request.settings.enableSmartSnap)
        {
            const auto width = result.snappedBounds.getWidth();
            const auto halfWidth = width * 0.5f;
            const auto height = result.snappedBounds.getHeight();
            const auto halfHeight = height * 0.5f;
            const auto movingBounds = result.snappedBounds;

            const auto overlapsOnY = [tolerance](const juce::Rectangle<float>& a,
                                                 const juce::Rectangle<float>& b)
            {
                const auto overlap = std::min(a.getBottom(), b.getBottom()) - std::max(a.getY(), b.getY());
                return overlap >= -tolerance;
            };

            const auto overlapsOnX = [tolerance](const juce::Rectangle<float>& a,
                                                 const juce::Rectangle<float>& b)
            {
                const auto overlap = std::min(a.getRight(), b.getRight()) - std::max(a.getX(), b.getX());
                return overlap >= -tolerance;
            };

            auto considerVerticalGuide = [&](float guideX)
            {
                // left / right / center alignment to one vertical guide.
                trySnapX(guideX, guideX);
                trySnapX(guideX - width, guideX);
                trySnapX(guideX - halfWidth, guideX);
            };

            auto considerHorizontalGuide = [&](float guideY)
            {
                // top / bottom / center alignment to one horizontal guide.
                trySnapY(guideY, guideY);
                trySnapY(guideY - height, guideY);
                trySnapY(guideY - halfHeight, guideY);
            };

            for (const auto& nearby : request.nearbyBounds)
            {
                considerVerticalGuide(nearby.getX());
                considerVerticalGuide(nearby.getRight());
                considerVerticalGuide(nearby.getCentreX());

                considerHorizontalGuide(nearby.getY());
                considerHorizontalGuide(nearby.getBottom());
                considerHorizontalGuide(nearby.getCentreY());
            }

            for (const auto guideX : request.verticalGuides)
                considerVerticalGuide(guideX);

            for (const auto guideY : request.horizontalGuides)
                considerHorizontalGuide(guideY);

            // Equal spacing snap (horizontal).
            for (size_t firstIndex = 0; firstIndex < request.nearbyBounds.size(); ++firstIndex)
            {
                const auto& first = request.nearbyBounds[firstIndex];
                for (size_t secondIndex = 0; secondIndex < request.nearbyBounds.size(); ++secondIndex)
                {
                    if (firstIndex == secondIndex)
                        continue;

                    const auto& second = request.nearbyBounds[secondIndex];
                    const auto* left = &first;
                    const auto* right = &second;
                    if (left->getX() > right->getX())
                        std::swap(left, right);

                    if (left->getRight() > right->getX())
                        continue;
                    if (!overlapsOnY(*left, movingBounds) || !overlapsOnY(*right, movingBounds))
                        continue;

                    const auto baseGap = right->getX() - left->getRight();
                    if (baseGap < 0.0f)
                        continue;

                    const auto overlapTop = std::max({ left->getY(), right->getY(), movingBounds.getY() });
                    const auto overlapBottom = std::min({ left->getBottom(), right->getBottom(), movingBounds.getBottom() });
                    const auto axisY = overlapBottom > overlapTop
                                         ? (overlapTop + overlapBottom) * 0.5f
                                         : movingBounds.getCentreY();

                    // Place moving widget between left and right with equal gaps on both sides.
                    if (right->getX() - left->getRight() >= width)
                    {
                        const auto desiredX = (left->getRight() + right->getX() - width) * 0.5f;
                        const auto gap = desiredX - left->getRight();
                        if (gap >= 0.0f)
                        {
                            SmartSpacingHint hint;
                            hint.horizontal = true;
                            hint.axisPosition = axisY;
                            hint.firstStart = left->getRight();
                            hint.firstEnd = desiredX;
                            hint.secondStart = desiredX + width;
                            hint.secondEnd = right->getX();
                            hint.gap = gap;

                            considerAxisCandidate(xCandidate,
                                                  movingBounds.getX(),
                                                  desiredX,
                                                  desiredX + halfWidth,
                                                  tolerance,
                                                  SnapKind::smartSpacing,
                                                  hint);
                        }
                    }

                    // Place moving widget to the left, preserving the gap rhythm.
                    {
                        const auto desiredX = left->getX() - width - baseGap;
                        SmartSpacingHint hint;
                        hint.horizontal = true;
                        hint.axisPosition = axisY;
                        hint.firstStart = desiredX + width;
                        hint.firstEnd = left->getX();
                        hint.secondStart = left->getRight();
                        hint.secondEnd = right->getX();
                        hint.gap = baseGap;

                        considerAxisCandidate(xCandidate,
                                              movingBounds.getX(),
                                              desiredX,
                                              desiredX + halfWidth,
                                              tolerance,
                                              SnapKind::smartSpacing,
                                              hint);
                    }

                    // Place moving widget to the right, preserving the gap rhythm.
                    {
                        const auto desiredX = right->getRight() + baseGap;
                        SmartSpacingHint hint;
                        hint.horizontal = true;
                        hint.axisPosition = axisY;
                        hint.firstStart = left->getRight();
                        hint.firstEnd = right->getX();
                        hint.secondStart = right->getRight();
                        hint.secondEnd = desiredX;
                        hint.gap = baseGap;

                        considerAxisCandidate(xCandidate,
                                              movingBounds.getX(),
                                              desiredX,
                                              desiredX + halfWidth,
                                              tolerance,
                                              SnapKind::smartSpacing,
                                              hint);
                    }
                }
            }

            // Equal spacing snap (vertical).
            for (size_t firstIndex = 0; firstIndex < request.nearbyBounds.size(); ++firstIndex)
            {
                const auto& first = request.nearbyBounds[firstIndex];
                for (size_t secondIndex = 0; secondIndex < request.nearbyBounds.size(); ++secondIndex)
                {
                    if (firstIndex == secondIndex)
                        continue;

                    const auto& second = request.nearbyBounds[secondIndex];
                    const auto* top = &first;
                    const auto* bottom = &second;
                    if (top->getY() > bottom->getY())
                        std::swap(top, bottom);

                    if (top->getBottom() > bottom->getY())
                        continue;
                    if (!overlapsOnX(*top, movingBounds) || !overlapsOnX(*bottom, movingBounds))
                        continue;

                    const auto baseGap = bottom->getY() - top->getBottom();
                    if (baseGap < 0.0f)
                        continue;

                    const auto overlapLeft = std::max({ top->getX(), bottom->getX(), movingBounds.getX() });
                    const auto overlapRight = std::min({ top->getRight(), bottom->getRight(), movingBounds.getRight() });
                    const auto axisX = overlapRight > overlapLeft
                                         ? (overlapLeft + overlapRight) * 0.5f
                                         : movingBounds.getCentreX();

                    // Place moving widget between top and bottom with equal gaps.
                    if (bottom->getY() - top->getBottom() >= height)
                    {
                        const auto desiredY = (top->getBottom() + bottom->getY() - height) * 0.5f;
                        const auto gap = desiredY - top->getBottom();
                        if (gap >= 0.0f)
                        {
                            SmartSpacingHint hint;
                            hint.horizontal = false;
                            hint.axisPosition = axisX;
                            hint.firstStart = top->getBottom();
                            hint.firstEnd = desiredY;
                            hint.secondStart = desiredY + height;
                            hint.secondEnd = bottom->getY();
                            hint.gap = gap;

                            considerAxisCandidate(yCandidate,
                                                  movingBounds.getY(),
                                                  desiredY,
                                                  desiredY + halfHeight,
                                                  tolerance,
                                                  SnapKind::smartSpacing,
                                                  hint);
                        }
                    }

                    // Place moving widget above, preserving the gap rhythm.
                    {
                        const auto desiredY = top->getY() - height - baseGap;
                        SmartSpacingHint hint;
                        hint.horizontal = false;
                        hint.axisPosition = axisX;
                        hint.firstStart = desiredY + height;
                        hint.firstEnd = top->getY();
                        hint.secondStart = top->getBottom();
                        hint.secondEnd = bottom->getY();
                        hint.gap = baseGap;

                        considerAxisCandidate(yCandidate,
                                              movingBounds.getY(),
                                              desiredY,
                                              desiredY + halfHeight,
                                              tolerance,
                                              SnapKind::smartSpacing,
                                              hint);
                    }

                    // Place moving widget below, preserving the gap rhythm.
                    {
                        const auto desiredY = bottom->getBottom() + baseGap;
                        SmartSpacingHint hint;
                        hint.horizontal = false;
                        hint.axisPosition = axisX;
                        hint.firstStart = top->getBottom();
                        hint.firstEnd = bottom->getY();
                        hint.secondStart = bottom->getBottom();
                        hint.secondEnd = desiredY;
                        hint.gap = baseGap;

                        considerAxisCandidate(yCandidate,
                                              movingBounds.getY(),
                                              desiredY,
                                              desiredY + halfHeight,
                                              tolerance,
                                              SnapKind::smartSpacing,
                                              hint);
                    }
                }
            }
        }

        if (xCandidate.hasValue)
        {
            result.snappedBounds.setX(xCandidate.snappedValue);
            result.snappedX = true;
            result.snapKindX = xCandidate.kind;
            if (xCandidate.kind == SnapKind::smartAlign)
                result.guideX = xCandidate.guideValue;
            if (xCandidate.spacingHint.has_value())
                result.spacingHints.push_back(*xCandidate.spacingHint);
        }

        if (yCandidate.hasValue)
        {
            result.snappedBounds.setY(yCandidate.snappedValue);
            result.snappedY = true;
            result.snapKindY = yCandidate.kind;
            if (yCandidate.kind == SnapKind::smartAlign)
                result.guideY = yCandidate.guideValue;
            if (yCandidate.spacingHint.has_value())
                result.spacingHints.push_back(*yCandidate.spacingHint);
        }

        return result;
    }
}
