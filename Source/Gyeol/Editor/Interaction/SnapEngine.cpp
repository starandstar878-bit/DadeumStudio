#include "Gyeol/Editor/Interaction/SnapEngine.h"

#include <cmath>

namespace Gyeol::Ui::Interaction
{
    namespace
    {
        float snapScalar(float value, float gridSize)
        {
            if (gridSize <= 0.0f)
                return value;
            return std::round(value / gridSize) * gridSize;
        }
    }

    SnapResult SnapEngine::compute(const SnapRequest& request) const
    {
        SnapResult result;
        result.snappedBounds = request.proposedBounds;

        if (request.settings.enableGrid && request.settings.gridSize > 0.0f)
        {
            const auto snappedX = snapScalar(result.snappedBounds.getX(), request.settings.gridSize);
            const auto snappedY = snapScalar(result.snappedBounds.getY(), request.settings.gridSize);

            if (std::abs(snappedX - result.snappedBounds.getX()) <= request.settings.tolerance)
            {
                result.snappedBounds.setX(snappedX);
                result.snappedX = true;
            }

            if (std::abs(snappedY - result.snappedBounds.getY()) <= request.settings.tolerance)
            {
                result.snappedBounds.setY(snappedY);
                result.snappedY = true;
            }
        }

        if (request.settings.enableWidgetGuides)
        {
            for (const auto& nearby : request.nearbyBounds)
            {
                const auto leftDelta = std::abs(nearby.getX() - result.snappedBounds.getX());
                const auto topDelta = std::abs(nearby.getY() - result.snappedBounds.getY());
                if (leftDelta <= request.settings.tolerance)
                {
                    result.snappedBounds.setX(nearby.getX());
                    result.snappedX = true;
                }

                if (topDelta <= request.settings.tolerance)
                {
                    result.snappedBounds.setY(nearby.getY());
                    result.snappedY = true;
                }
            }
        }

        return result;
    }
}
