#include "Gyeol/Editor/Canvas/CanvasRenderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Gyeol::Ui::Canvas
{
    namespace
    {
        constexpr float kResizeHandleSize = 10.0f;

        void warnUnsupportedWidgetOnce(WidgetType type, const char* context)
        {
            static std::vector<int> warnedTypeOrdinals;
            const auto ordinal = static_cast<int>(type);
            if (std::find(warnedTypeOrdinals.begin(), warnedTypeOrdinals.end(), ordinal) != warnedTypeOrdinals.end())
                return;

            warnedTypeOrdinals.push_back(ordinal);
            DBG("[Gyeol][Canvas] Unsupported widget painter in " + juce::String(context)
                + " (type=" + juce::String(ordinal) + ")");
        }

        bool isNearMajorGrid(float value, float majorStep, float minorStep) noexcept
        {
            if (majorStep <= 0.0f)
                return false;

            const auto mod = std::fmod(std::abs(value), majorStep);
            return std::abs(mod) <= (minorStep * 0.2f)
                || std::abs(majorStep - mod) <= (minorStep * 0.2f);
        }
    }

    CanvasRenderer::CanvasRenderer(const Widgets::WidgetFactory& widgetFactoryIn) noexcept
        : widgetFactory(widgetFactoryIn)
    {
    }

    void CanvasRenderer::paintCanvas(juce::Graphics& g,
                                     juce::Rectangle<int> viewportBounds,
                                     juce::Rectangle<float> visibleWorldBounds,
                                     juce::Rectangle<float> canvasWorldBounds,
                                     float zoomLevel,
                                     const Interaction::SnapSettings& snapSettings) const
    {
        g.fillAll(juce::Colour::fromRGB(16, 18, 24));

        if (viewportBounds.isEmpty())
            return;

        g.setColour(juce::Colour::fromRGB(18, 20, 25));
        g.fillRect(viewportBounds);

        g.saveState();
        g.reduceClipRegion(viewportBounds);

        const auto toViewX = [x0 = static_cast<float>(viewportBounds.getX()),
                              worldX = visibleWorldBounds.getX(),
                              zoomLevel](float world) noexcept
            {
                return x0 + (world - worldX) * zoomLevel;
            };
        const auto toViewY = [y0 = static_cast<float>(viewportBounds.getY()),
                              worldY = visibleWorldBounds.getY(),
                              zoomLevel](float world) noexcept
            {
                return y0 + (world - worldY) * zoomLevel;
            };

        const auto canvasViewBounds = juce::Rectangle<float>(toViewX(canvasWorldBounds.getX()),
                                                             toViewY(canvasWorldBounds.getY()),
                                                             canvasWorldBounds.getWidth() * zoomLevel,
                                                             canvasWorldBounds.getHeight() * zoomLevel);
        const auto visibleCanvasBounds = canvasViewBounds.getIntersection(viewportBounds.toFloat());

        if (snapSettings.enableGrid && !visibleCanvasBounds.isEmpty())
        {
            const auto gridSize = std::max(1.0f, snapSettings.gridSize);
            const auto majorStep = gridSize * 4.0f;
            const auto gridWorldBounds = visibleWorldBounds.getIntersection(canvasWorldBounds);
            const auto worldMinX = gridWorldBounds.getX();
            const auto worldMaxX = gridWorldBounds.getRight();
            const auto worldMinY = gridWorldBounds.getY();
            const auto worldMaxY = gridWorldBounds.getBottom();

            const auto gridFade = juce::jlimit(0.0f, 1.0f, (zoomLevel - 0.3f) / 0.6f);
            const auto minorAlpha = static_cast<juce::uint8>(juce::jlimit(0.0f, 255.0f, 14.0f * gridFade));
            const auto majorAlpha = static_cast<juce::uint8>(juce::jlimit(0.0f, 255.0f, 28.0f * gridFade));

            g.saveState();
            g.reduceClipRegion(visibleCanvasBounds.toNearestInt());

            if (minorAlpha > 0)
            {
                g.setColour(juce::Colour::fromRGBA(255, 255, 255, minorAlpha));
                auto startX = std::floor(worldMinX / gridSize) * gridSize;
                for (float x = startX; x <= worldMaxX + gridSize; x += gridSize)
                    g.drawVerticalLine(juce::roundToInt(toViewX(x)),
                                       visibleCanvasBounds.getY(),
                                       visibleCanvasBounds.getBottom());

                auto startY = std::floor(worldMinY / gridSize) * gridSize;
                for (float y = startY; y <= worldMaxY + gridSize; y += gridSize)
                    g.drawHorizontalLine(juce::roundToInt(toViewY(y)),
                                         visibleCanvasBounds.getX(),
                                         visibleCanvasBounds.getRight());
            }

            if (majorAlpha > 0)
            {
                g.setColour(juce::Colour::fromRGBA(255, 255, 255, majorAlpha));
                auto startX = std::floor(worldMinX / majorStep) * majorStep;
                for (float x = startX; x <= worldMaxX + majorStep; x += majorStep)
                    g.drawVerticalLine(juce::roundToInt(toViewX(x)),
                                       visibleCanvasBounds.getY(),
                                       visibleCanvasBounds.getBottom());

                auto startY = std::floor(worldMinY / majorStep) * majorStep;
                for (float y = startY; y <= worldMaxY + majorStep; y += majorStep)
                    g.drawHorizontalLine(juce::roundToInt(toViewY(y)),
                                         visibleCanvasBounds.getX(),
                                         visibleCanvasBounds.getRight());
            }

            g.restoreState();
        }

        g.setColour(juce::Colour::fromRGBA(82, 140, 220, 90));
        g.drawRect(canvasViewBounds.toNearestInt(), 1);

        g.restoreState();

        const auto topRuler = juce::Rectangle<int>(viewportBounds.getX(), 0, viewportBounds.getWidth(), viewportBounds.getY());
        const auto leftRuler = juce::Rectangle<int>(0, viewportBounds.getY(), viewportBounds.getX(), viewportBounds.getHeight());
        if (!topRuler.isEmpty() || !leftRuler.isEmpty())
        {
            g.setColour(juce::Colour::fromRGB(26, 30, 38));
            if (!topRuler.isEmpty())
                g.fillRect(topRuler);
            if (!leftRuler.isEmpty())
                g.fillRect(leftRuler);

            g.setColour(juce::Colour::fromRGB(90, 98, 112));
            if (!topRuler.isEmpty())
                g.drawHorizontalLine(topRuler.getBottom() - 1,
                                     static_cast<float>(topRuler.getX()),
                                     static_cast<float>(topRuler.getRight()));
            if (!leftRuler.isEmpty())
                g.drawVerticalLine(leftRuler.getRight() - 1,
                                   static_cast<float>(leftRuler.getY()),
                                   static_cast<float>(leftRuler.getBottom()));

            const auto rawMajorStep = std::max(2.0f, 80.0f / std::max(0.0001f, zoomLevel));
            const auto magnitude = std::pow(10.0f, std::floor(std::log10(rawMajorStep)));
            float majorStep = magnitude;
            for (const auto candidate : { 1.0f, 2.0f, 5.0f, 10.0f })
            {
                majorStep = magnitude * candidate;
                if (majorStep >= rawMajorStep)
                    break;
            }
            const auto minorStep = majorStep / 5.0f;

            if (!topRuler.isEmpty())
            {
                auto start = std::floor(visibleWorldBounds.getX() / minorStep) * minorStep;
                for (float x = start; x <= visibleWorldBounds.getRight() + minorStep; x += minorStep)
                {
                    const auto viewX = toViewX(x);
                    const auto isMajor = isNearMajorGrid(x, majorStep, minorStep);
                    const auto tick = isMajor ? 10.0f : 6.0f;
                    g.setColour(juce::Colour::fromRGBA(175, 183, 196, isMajor ? 210 : 120));
                    g.drawLine(viewX, static_cast<float>(topRuler.getBottom()) - tick,
                               viewX, static_cast<float>(topRuler.getBottom()));
                }
            }

            if (!leftRuler.isEmpty())
            {
                auto start = std::floor(visibleWorldBounds.getY() / minorStep) * minorStep;
                for (float y = start; y <= visibleWorldBounds.getBottom() + minorStep; y += minorStep)
                {
                    const auto viewY = toViewY(y);
                    const auto isMajor = isNearMajorGrid(y, majorStep, minorStep);
                    const auto tick = isMajor ? 10.0f : 6.0f;
                    g.setColour(juce::Colour::fromRGBA(175, 183, 196, isMajor ? 210 : 120));
                    g.drawLine(static_cast<float>(leftRuler.getRight()) - tick, viewY,
                               static_cast<float>(leftRuler.getRight()), viewY);
                }
            }
        }
    }

    void CanvasRenderer::paintWidget(juce::Graphics& g,
                                     const WidgetModel& widget,
                                     juce::Rectangle<float> localBounds,
                                     bool selected,
                                     float effectiveOpacity,
                                     bool showResizeHandle,
                                     bool resizeHandleHot) const
    {
        const auto body = localBounds.reduced(1.0f);
        const auto clampedOpacity = juce::jlimit(0.0f, 1.0f, effectiveOpacity);

        g.saveState();
        if (clampedOpacity < 0.999f)
            g.beginTransparencyLayer(clampedOpacity);

        if (const auto* descriptor = widgetFactory.descriptorFor(widget.type);
            descriptor != nullptr && static_cast<bool>(descriptor->painter))
        {
            descriptor->painter(g, widget, body);
        }
        else
        {
            warnUnsupportedWidgetOnce(widget.type, "CanvasRenderer::paintWidget");
            g.setColour(juce::Colour::fromRGB(44, 49, 60));
            g.fillRoundedRectangle(body, 4.0f);
            g.setColour(juce::Colour::fromRGB(228, 110, 110));
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawFittedText("Unsupported",
                             body.reduced(4.0f).toNearestInt(),
                             juce::Justification::centred,
                             1);
        }

        g.setColour(juce::Colour::fromRGB(95, 101, 114));
        g.drawRoundedRectangle(body, 5.0f, 1.0f);

        if (clampedOpacity < 0.999f)
            g.endTransparencyLayer();
        g.restoreState();

        if (!selected)
            return;

        const auto selectionOutline = juce::Colour::fromRGB(78, 156, 255);
        g.setColour(selectionOutline.withAlpha(0.08f));
        g.drawRoundedRectangle(body.expanded(3.0f), 7.0f, 4.0f);
        g.setColour(selectionOutline.withAlpha(0.15f));
        g.drawRoundedRectangle(body.expanded(1.5f), 6.0f, 2.5f);
        g.setColour(selectionOutline);
        g.drawRoundedRectangle(body, 5.0f, 1.8f);

        if (!showResizeHandle)
            return;

        const auto handle = resizeHandleBounds(localBounds);
        const auto center = handle.getCentre();
        const auto radius = std::min(handle.getWidth(), handle.getHeight()) * 0.5f;
        if (resizeHandleHot)
        {
            g.setColour(selectionOutline.withAlpha(0.15f));
            g.fillEllipse(center.x - radius - 3.0f,
                          center.y - radius - 3.0f,
                          (radius + 3.0f) * 2.0f,
                          (radius + 3.0f) * 2.0f);
        }

        g.setColour(resizeHandleHot ? selectionOutline.brighter(0.25f) : selectionOutline);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(center.x - radius * 0.35f,
                      center.y - radius * 0.35f,
                      radius * 0.7f,
                      radius * 0.7f);
    }

    juce::Rectangle<float> CanvasRenderer::resizeHandleBounds(const juce::Rectangle<float>& localBounds) const noexcept
    {
        const auto handleSize = std::min({ kResizeHandleSize,
                                           localBounds.getWidth(),
                                           localBounds.getHeight() });
        return { localBounds.getRight() - handleSize - 1.0f,
                 localBounds.getBottom() - handleSize - 1.0f,
                 handleSize,
                 handleSize };
    }

    bool CanvasRenderer::hitResizeHandle(const juce::Rectangle<float>& localBounds,
                                         juce::Point<float> point) const noexcept
    {
        return resizeHandleBounds(localBounds).contains(point);
    }
}
