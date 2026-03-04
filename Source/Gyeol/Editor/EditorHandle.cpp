#include "Gyeol/Public/EditorHandle.h"
#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

#include "Gyeol/Editor/Interaction/AlignDistributeEngine.h"
#include "Gyeol/Editor/Interaction/LayerOrderEngine.h"
#include "Gyeol/Editor/Interaction/SnapEngine.h"
#include "Gyeol/Editor/Panels/AssetsPanel.h"
#include "Gyeol/Editor/Panels/EventActionPanel.h"
#include "Gyeol/Editor/Panels/ExportPreviewPanel.h"
#include "Gyeol/Editor/Panels/GridSnapPanel.h"
#include "Gyeol/Editor/Panels/HistoryPanel.h"
#include "Gyeol/Editor/Panels/LayerTreePanel.h"
#include "Gyeol/Editor/Panels/NavigatorPanel.h"
#include "Gyeol/Editor/Panels/PerformancePanel.h"
#include "Gyeol/Editor/Panels/PropertyPanel.h"
#include "Gyeol/Editor/Panels/ValidationPanel.h"
#include "Gyeol/Editor/Panels/WidgetLibraryPanel.h"
#include "Gyeol/Export/JuceComponentExport.h"
#include "Gyeol/Runtime/PropertyBindingResolver.h"
#include "Gyeol/Runtime/RuntimeActionExecutor.h"
#include "Gyeol/Runtime/RuntimeBindingEngine.h"
#include "Gyeol/Runtime/RuntimeDiagnostics.h"
#include "Gyeol/Runtime/RuntimeParamBridge.h"
#include "Gyeol/Serialization/DocumentJson.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
using Gyeol::GyeolPalette;

juce::Colour palette(GyeolPalette id, float alpha = 1.0f) {
  return Gyeol::getGyeolColor(id).withAlpha(alpha);
}

juce::Font makePanelFont(const juce::Component &component, float height,
                         bool bold = false) {
  if (const auto *lf = dynamic_cast<const Gyeol::GyeolCustomLookAndFeel *>(
          &component.getLookAndFeel());
      lf != nullptr) {
    return lf->makeFont(height, bold);
  }

  auto fallback = juce::Font(juce::FontOptions(height));
  return bold ? fallback.boldened() : fallback;
}

juce::String shortcutText(const juce::KeyPress &key) {
  if (!key.isValid())
    return "(none)";

  auto text = key.getTextDescription();
  text = text.replace("command", "Cmd/Ctrl");
  text = text.replace("ctrl", "Ctrl");
  return text;
}

double channelToLinear(double channel) {
  if (channel <= 0.04045)
    return channel / 12.92;
  return std::pow((channel + 0.055) / 1.055, 2.4);
}

double relativeLuminance(juce::Colour colour) {
  const auto r = channelToLinear(colour.getFloatRed());
  const auto g = channelToLinear(colour.getFloatGreen());
  const auto b = channelToLinear(colour.getFloatBlue());
  return (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
}

double contrastRatio(juce::Colour a, juce::Colour b) {
  const auto l1 = relativeLuminance(a);
  const auto l2 = relativeLuminance(b);
  const auto brighter = std::max(l1, l2);
  const auto darker = std::min(l1, l2);
  return (brighter + 0.05) / (darker + 0.05);
}

bool isTextInputComponent(const juce::Component *component) {
  return dynamic_cast<const juce::TextEditor *>(component) != nullptr ||
         dynamic_cast<const juce::Label *>(component) != nullptr;
}
} // namespace

namespace Gyeol::Ui {
inline constexpr float kResizeHandleSize = 10.0f;
inline constexpr float kBoundsEpsilon = 0.001f;
inline constexpr int kRulerThicknessPx = 20;
inline constexpr float kMinCanvasZoom = 0.2f;
inline constexpr float kMaxCanvasZoom = 4.0f;
inline constexpr float kCanvasWorldWidth = 1600.0f;
inline constexpr float kCanvasWorldHeight = 1000.0f;
inline constexpr float kCanvasWorldPadding = 120.0f;
inline constexpr float kGuideRemoveThresholdPx = 8.0f;

inline bool containsWidgetId(const std::vector<WidgetId> &ids, WidgetId id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

inline bool areClose(float lhs, float rhs) noexcept {
  return std::abs(lhs - rhs) <= kBoundsEpsilon;
}

inline bool areRectsEqual(const juce::Rectangle<float> &lhs,
                          const juce::Rectangle<float> &rhs) noexcept {
  return areClose(lhs.getX(), rhs.getX()) && areClose(lhs.getY(), rhs.getY()) &&
         areClose(lhs.getWidth(), rhs.getWidth()) &&
         areClose(lhs.getHeight(), rhs.getHeight());
}

inline juce::Rectangle<float>
unionRect(const juce::Rectangle<float> &lhs,
          const juce::Rectangle<float> &rhs) noexcept {
  const auto left = std::min(lhs.getX(), rhs.getX());
  const auto top = std::min(lhs.getY(), rhs.getY());
  const auto right = std::max(lhs.getRight(), rhs.getRight());
  const auto bottom = std::max(lhs.getBottom(), rhs.getBottom());
  return {left, top, right - left, bottom - top};
}

inline juce::Rectangle<float>
makeNormalizedRect(juce::Point<float> a, juce::Point<float> b) noexcept {
  const auto left = std::min(a.x, b.x);
  const auto top = std::min(a.y, b.y);
  const auto right = std::max(a.x, b.x);
  const auto bottom = std::max(a.y, b.y);
  return {left, top, right - left, bottom - top};
}

inline void drawDashedRect(juce::Graphics &g,
                           const juce::Rectangle<float> &rect,
                           float dashLength = 5.0f, float gapLength = 3.0f,
                           float thickness = 1.0f) {
  const float dashPattern[]{dashLength, gapLength};
  g.drawDashedLine(
      juce::Line<float>(rect.getX(), rect.getY(), rect.getRight(), rect.getY()),
      dashPattern, 2, thickness);
  g.drawDashedLine(juce::Line<float>(rect.getRight(), rect.getY(),
                                     rect.getRight(), rect.getBottom()),
                   dashPattern, 2, thickness);
  g.drawDashedLine(juce::Line<float>(rect.getRight(), rect.getBottom(),
                                     rect.getX(), rect.getBottom()),
                   dashPattern, 2, thickness);
  g.drawDashedLine(juce::Line<float>(rect.getX(), rect.getBottom(), rect.getX(),
                                     rect.getY()),
                   dashPattern, 2, thickness);
}

inline void warnUnsupportedWidgetOnce(WidgetType type, const char *context) {
  static std::vector<juce::String> warnedKeys;
  const auto key =
      juce::String(context) + ":" + juce::String(static_cast<int>(type));
  if (std::find(warnedKeys.begin(), warnedKeys.end(), key) != warnedKeys.end())
    return;

  warnedKeys.push_back(key);
  DBG("[Gyeol] Unsupported widget fallback in " + juce::String(context) +
      " (type ordinal=" + juce::String(static_cast<int>(type)) + ")");
}

class CanvasRenderer {
public:
  explicit CanvasRenderer(
      const Widgets::WidgetFactory &widgetFactoryIn) noexcept
      : widgetFactory(widgetFactoryIn) {}

  void paintCanvas(juce::Graphics &g, juce::Rectangle<int> bounds) const {
    g.fillAll(juce::Colour::fromRGB(18, 20, 25));

    g.setColour(juce::Colour::fromRGBA(255, 255, 255, 12));
    constexpr int kMajorGrid = 48;
    constexpr int kMinorGrid = 12;

    for (int x = bounds.getX(); x < bounds.getRight(); x += kMinorGrid)
      g.drawVerticalLine(x, static_cast<float>(bounds.getY()),
                         static_cast<float>(bounds.getBottom()));

    for (int y = bounds.getY(); y < bounds.getBottom(); y += kMinorGrid)
      g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                           static_cast<float>(bounds.getRight()));

    g.setColour(juce::Colour::fromRGBA(255, 255, 255, 24));
    for (int x = bounds.getX(); x < bounds.getRight(); x += kMajorGrid)
      g.drawVerticalLine(x, static_cast<float>(bounds.getY()),
                         static_cast<float>(bounds.getBottom()));

    for (int y = bounds.getY(); y < bounds.getBottom(); y += kMajorGrid)
      g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                           static_cast<float>(bounds.getRight()));
  }

  juce::Rectangle<float>
  resizeHandleBounds(const juce::Rectangle<float> &localBounds) const noexcept {
    const auto handleSize = std::min(
        {kResizeHandleSize, localBounds.getWidth(), localBounds.getHeight()});
    return {localBounds.getRight() - handleSize - 1.0f,
            localBounds.getBottom() - handleSize - 1.0f, handleSize,
            handleSize};
  }

  bool hitResizeHandle(const juce::Rectangle<float> &localBounds,
                       juce::Point<float> point) const noexcept {
    return resizeHandleBounds(localBounds).contains(point);
  }

  void paintWidget(juce::Graphics &g, const WidgetModel &widget,
                   const juce::Rectangle<float> &localBounds,
                   float effectiveOpacity, bool selected, bool showResizeHandle,
                   bool resizeHandleHot) const {
    const auto body = localBounds.reduced(1.0f);
    const auto clampedOpacity = juce::jlimit(0.0f, 1.0f, effectiveOpacity);
    const auto baseOutline = juce::Colour::fromRGB(95, 101, 114);
    const auto selectionOutline = juce::Colour::fromRGB(78, 156, 255);

    g.saveState();
    const auto useTransparencyLayer = clampedOpacity < 0.999f;
    if (useTransparencyLayer)
      g.beginTransparencyLayer(clampedOpacity);

    if (const auto *descriptor = widgetFactory.descriptorFor(widget.type);
        descriptor != nullptr && static_cast<bool>(descriptor->painter)) {
      descriptor->painter(g, widget, body);
    } else {
      warnUnsupportedWidgetOnce(widget.type, "CanvasRenderer::paintWidget");
      g.setColour(juce::Colour::fromRGB(44, 49, 60));
      g.fillRoundedRectangle(body, 4.0f);
      g.setColour(juce::Colour::fromRGB(228, 110, 110));
      g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
      g.drawFittedText("Unsupported", body.reduced(4.0f).toNearestInt(),
                       juce::Justification::centred, 1);
    }
    g.setColour(baseOutline);
    g.drawRoundedRectangle(body, 5.0f, 1.0f);

    if (useTransparencyLayer)
      g.endTransparencyLayer();

    g.restoreState();

    if (selected) {
      // Phase 3: �׿� Glow �ܰ��� (�ٱ��������
      // ���� ���̾�)
      g.setColour(selectionOutline.withAlpha(0.08f));
      g.drawRoundedRectangle(body.expanded(3.0f), 7.0f, 4.0f);
      g.setColour(selectionOutline.withAlpha(0.15f));
      g.drawRoundedRectangle(body.expanded(1.5f), 6.0f, 2.5f);
      g.setColour(selectionOutline);
      g.drawRoundedRectangle(body, 5.0f, 1.8f);

      if (showResizeHandle) {
        const auto handle = resizeHandleBounds(localBounds);
        const auto handleCenter = handle.getCentre();
        const auto handleRadius =
            std::min(handle.getWidth(), handle.getHeight()) * 0.5f;

        // Phase 3: �������� �ڵ��� ���� + Glow�� ��ü
        if (resizeHandleHot) {
          g.setColour(selectionOutline.withAlpha(0.15f));
          g.fillEllipse(handleCenter.x - handleRadius - 3.0f,
                        handleCenter.y - handleRadius - 3.0f,
                        (handleRadius + 3.0f) * 2.0f,
                        (handleRadius + 3.0f) * 2.0f);
        }
        g.setColour(resizeHandleHot ? selectionOutline.brighter(0.3f)
                                    : selectionOutline);
        g.fillEllipse(handleCenter.x - handleRadius,
                      handleCenter.y - handleRadius, handleRadius * 2.0f,
                      handleRadius * 2.0f);
        // ��� ���� ��
        g.setColour(juce::Colours::white);
        g.fillEllipse(handleCenter.x - handleRadius * 0.35f,
                      handleCenter.y - handleRadius * 0.35f,
                      handleRadius * 0.7f, handleRadius * 0.7f);
      }
    }
  }

  void paintGroupBadge(juce::Graphics &g,
                       const juce::Rectangle<float> &localBounds, bool selected,
                       bool groupedInActiveEdit) const {
    if (localBounds.getWidth() < 28.0f || localBounds.getHeight() < 20.0f)
      return;

    const auto badgeBounds = juce::Rectangle<float>(4.0f, 4.0f, 16.0f, 12.0f);
    const auto badgeFill = groupedInActiveEdit
                               ? juce::Colour::fromRGB(255, 196, 112)
                               : juce::Colour::fromRGB(120, 170, 235);
    const auto badgeStroke = selected ? juce::Colour::fromRGB(78, 156, 255)
                                      : juce::Colour::fromRGB(56, 72, 96);

    g.setColour(badgeFill.withAlpha(0.92f));
    g.fillRoundedRectangle(badgeBounds, 3.0f);
    g.setColour(badgeStroke.withAlpha(0.95f));
    g.drawRoundedRectangle(badgeBounds, 3.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawFittedText("G", badgeBounds.toNearestInt(),
                     juce::Justification::centred, 1);
  }

private:
  const Widgets::WidgetFactory &widgetFactory;
};

class CanvasComponent;

class WidgetComponent : public juce::Component {
public:
  WidgetComponent(CanvasComponent &ownerIn, const CanvasRenderer &rendererIn,
                  const WidgetModel &widgetIn, float effectiveOpacityIn,
                  bool isSelected, bool showResizeHandleIn, bool isGroupedIn,
                  bool groupedInActiveEditIn)
      : owner(ownerIn), renderer(rendererIn) {
    updateFromModel(widgetIn, effectiveOpacityIn, isSelected,
                    showResizeHandleIn, isGroupedIn, groupedInActiveEditIn);
    setRepaintsOnMouseActivity(true);
  }

  void updateFromModel(const WidgetModel &widgetIn, float effectiveOpacityIn,
                       bool isSelected, bool showResizeHandleIn,
                       bool isGroupedIn, bool groupedInActiveEditIn) {
    const auto opacityChanged = !areClose(effectiveOpacity, effectiveOpacityIn);
    const auto modelChanged = widget.type != widgetIn.type ||
                              widget.properties != widgetIn.properties ||
                              !areClose(widget.opacity, widgetIn.opacity);

    widget = widgetIn;
    effectiveOpacity = effectiveOpacityIn;
    resizeHandleHot = false;
    setSelected(isSelected);
    showResizeHandle = showResizeHandleIn;
    grouped = isGroupedIn;
    groupedInActiveEdit = groupedInActiveEditIn;

    if (opacityChanged || modelChanged)
      repaint();
  }

  void setSelected(bool isSelected) {
    if (selected == isSelected)
      return;

    selected = isSelected;
    repaint();
  }

  bool setSelectionVisual(bool isSelected, bool showResizeHandleIn,
                          bool isGroupedIn, bool groupedInActiveEditIn) {
    if (selected == isSelected && showResizeHandle == showResizeHandleIn &&
        grouped == isGroupedIn && groupedInActiveEdit == groupedInActiveEditIn)
      return false;

    selected = isSelected;
    showResizeHandle = showResizeHandleIn;
    grouped = isGroupedIn;
    groupedInActiveEdit = groupedInActiveEditIn;
    if (!showResizeHandle)
      resizeHandleHot = false;
    repaint();
    return true;
  }

  void setViewBounds(const juce::Rectangle<float> &bounds) {
    const auto nextBounds = bounds.getSmallestIntegerContainer();
    if (getBounds() == nextBounds)
      return;
    setBounds(nextBounds);
  }

  WidgetId widgetId() const noexcept { return widget.id; }
  bool isResizeHandleHit(juce::Point<float> localPoint) const noexcept {
    return selected && showResizeHandle &&
           renderer.hitResizeHandle(getLocalBounds().toFloat(), localPoint);
  }

  void paint(juce::Graphics &g) override {
    renderer.paintWidget(g, widget, getLocalBounds().toFloat(),
                         effectiveOpacity, selected, showResizeHandle,
                         resizeHandleHot);

    if (grouped)
      renderer.paintGroupBadge(g, getLocalBounds().toFloat(), selected,
                               groupedInActiveEdit);
  }

  void mouseMove(const juce::MouseEvent &event) override;
  void mouseExit(const juce::MouseEvent &event) override;

  void mouseDown(const juce::MouseEvent &event) override;
  void mouseDrag(const juce::MouseEvent &event) override;
  void mouseUp(const juce::MouseEvent &event) override;
  void mouseDoubleClick(const juce::MouseEvent &event) override;
  void mouseWheelMove(const juce::MouseEvent &event,
                      const juce::MouseWheelDetails &wheel) override;

private:
  CanvasComponent &owner;
  const CanvasRenderer &renderer;
  WidgetModel widget;
  bool selected = false;
  bool showResizeHandle = false;
  bool resizeHandleHot = false;
  bool grouped = false;
  bool groupedInActiveEdit = false;
  float effectiveOpacity = 1.0f;
};

class CanvasComponent : public juce::Component, public juce::DragAndDropTarget {
public:
  enum class InteractionMode { preview, run };

  CanvasComponent(DocumentHandle &documentIn,
                  const Widgets::WidgetFactory &widgetFactoryIn)
      : document(documentIn), widgetFactory(widgetFactoryIn),
        renderer(widgetFactoryIn) {
    setWantsKeyboardFocus(true);

    Runtime::RuntimeDiagnostics::Settings diagnosticsSettings;
    diagnosticsSettings.logLevel = Runtime::RuntimeLogLevel::info;
    diagnosticsSettings.valueChangedLogStride = 12;
    runtimeDiagnostics.setSettings(diagnosticsSettings);

    Runtime::RuntimeBindingEngine::DispatchOptions dispatchOptions;
    dispatchOptions.maxActionsPerEvent = 128;
    dispatchOptions.continueOnActionFailure = true;
    runtimeBindingEngine.setDispatchOptions(dispatchOptions);

    refreshFromDocument();
  }

  void setInteractionMode(InteractionMode nextMode) {
    if (interactionModeValue == nextMode)
      return;

    interactionModeValue = nextMode;
    dragState = {};
    marqueeState = {};
    panState = {};
    guideDragState = {};
    runtimeInteraction = {};
    runtimeParamBridge.clear();
    runtimeDiagnostics.resetSession();
    clearTransientSnapGuides();
    clearWidgetLibraryDropPreview();
    clearAssetDropPreview();
    lastRuntimeBindingFailureDigest.clear();

    bool bindingAppliedOnModeSwitch = false;
    if (interactionModeValue == InteractionMode::run) {
      activeGroupEditId.reset();
      seedRuntimeParamsFromDocument();
      bindingAppliedOnModeSwitch = applyRuntimePropertyBindings();
    } else if (interactionModeValue == InteractionMode::preview &&
               previewBindingSimulationEnabled) {
      seedRuntimeParamsFromDocument();
      bindingAppliedOnModeSwitch = applyRuntimePropertyBindings();
    }

    if (bindingAppliedOnModeSwitch)
      refreshFromDocument();
    else {
      syncSelectionToViews();
      repaint();
    }
  }

  InteractionMode interactionMode() const noexcept {
    return interactionModeValue;
  }

  bool isRunMode() const noexcept {
    return interactionModeValue == InteractionMode::run;
  }

  bool isPreviewBindingSimulationEnabled() const noexcept {
    return previewBindingSimulationEnabled;
  }

  void setPreviewBindingSimulationEnabled(bool enabled) {
    if (previewBindingSimulationEnabled == enabled)
      return;

    previewBindingSimulationEnabled = enabled;
    runtimeParamBridge.clear();
    runtimeDiagnostics.resetSession();
    lastRuntimeBindingFailureDigest.clear();

    bool bindingApplied = false;
    if (interactionModeValue == InteractionMode::preview &&
        previewBindingSimulationEnabled) {
      seedRuntimeParamsFromDocument();
      bindingApplied = applyRuntimePropertyBindings();
    }

    if (bindingApplied)
      refreshFromDocument();
    else {
      syncSelectionToViews();
      repaint();
    }
  }

  void setStateChangedCallback(std::function<void()> callback) {
    onStateChanged = std::move(callback);
  }

  void setViewportChangedCallback(std::function<void()> callback) {
    onViewportChanged = std::move(callback);
  }

  void setRuntimeLogCallback(
      std::function<void(const juce::String &, const juce::String &)>
          callback) {
    onRuntimeLog = std::move(callback);
  }

  void
  setActiveLayerResolver(std::function<std::optional<WidgetId>()> resolver) {
    activeLayerResolver = std::move(resolver);
  }

  void setWidgetLibraryDropCallback(
      std::function<void(const juce::String &, juce::Point<float>)> callback) {
    onWidgetLibraryDrop = std::move(callback);
  }

  juce::Point<float> snapCreateOrigin(WidgetType type,
                                      juce::Point<float> worldOrigin) const {
    const auto *descriptor = widgetFactory.descriptorFor(type);
    if (descriptor == nullptr)
      return worldOrigin;

    const auto proposedBounds =
        descriptor->defaultBounds.withPosition(worldOrigin);
    const auto snapResult =
        snapEngine.compute(makeSnapRequest(proposedBounds, {}));
    return snapResult.snappedBounds.getPosition();
  }

  void setSnapSettings(const Interaction::SnapSettings &settingsIn) {
    snapSettings = settingsIn;
    repaint();
  }

  const Interaction::SnapSettings &currentSnapSettings() const noexcept {
    return snapSettings;
  }

  float currentZoomLevel() const noexcept { return zoomLevel; }

  juce::Point<float> currentViewOriginWorld() const noexcept {
    return viewOriginWorld;
  }

  juce::Rectangle<float> visibleWorldBounds() const noexcept {
    const auto viewport = viewportBounds();
    if (viewport.isEmpty())
      return {};
    return viewToWorldRect(viewport.toFloat());
  }

  void resized() override {
    recenterAndClampViewport();
    updateAllWidgetViewBounds();
    repaint();
    notifyViewportChanged();
  }

  juce::Rectangle<int> viewportBounds() const noexcept {
    auto area = getLocalBounds();
    area.removeFromTop(kRulerThicknessPx);
    area.removeFromLeft(kRulerThicknessPx);
    return area;
  }

  juce::Rectangle<int> topRulerBounds() const noexcept {
    return {kRulerThicknessPx, 0, juce::jmax(0, getWidth() - kRulerThicknessPx),
            kRulerThicknessPx};
  }

  juce::Rectangle<int> leftRulerBounds() const noexcept {
    return {0, kRulerThicknessPx, kRulerThicknessPx,
            juce::jmax(0, getHeight() - kRulerThicknessPx)};
  }

  bool isPointInViewport(juce::Point<float> localPoint) const noexcept {
    return viewportBounds().toFloat().contains(localPoint);
  }

  juce::Rectangle<float> canvasViewBounds() const noexcept {
    return worldToViewRect(canvasWorldBounds());
  }

  juce::Rectangle<float> visibleCanvasViewBounds() const noexcept {
    return canvasViewBounds().getIntersection(viewportBounds().toFloat());
  }

  bool isPointInCanvasView(juce::Point<float> localPoint) const noexcept {
    return canvasViewBounds().contains(localPoint);
  }

  bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails
                                    &dragSourceDetails) override {
    if (isRunMode())
      return false;

    return extractWidgetLibraryTypeKey(dragSourceDetails.description)
               .has_value() ||
           extractAssetDragPayload(dragSourceDetails.description).has_value();
  }

  void itemDragEnter(const juce::DragAndDropTarget::SourceDetails
                         &dragSourceDetails) override {
    itemDragMove(dragSourceDetails);
  }

  void itemDragMove(const juce::DragAndDropTarget::SourceDetails
                        &dragSourceDetails) override {
    if (isRunMode()) {
      clearWidgetLibraryDropPreview();
      clearAssetDropPreview();
      return;
    }

    if (extractWidgetLibraryTypeKey(dragSourceDetails.description)
            .has_value()) {
      clearAssetDropPreview();
      const auto viewPoint = dragSourceDetails.localPosition.toFloat();
      if (!isPointInCanvasView(viewPoint)) {
        clearWidgetLibraryDropPreview();
        return;
      }

      if (!widgetLibraryDropPreviewActive ||
          viewPoint.getDistanceFrom(widgetLibraryDropPreviewView) > 0.5f) {
        widgetLibraryDropPreviewActive = true;
        widgetLibraryDropPreviewView = viewPoint;
        repaint();
      }
      return;
    }

    const auto assetPayload =
        extractAssetDragPayload(dragSourceDetails.description);
    if (!assetPayload.has_value()) {
      clearWidgetLibraryDropPreview();
      clearAssetDropPreview();
      return;
    }

    clearWidgetLibraryDropPreview();
    const auto viewPoint = dragSourceDetails.localPosition.toFloat();
    if (!isPointInCanvasView(viewPoint)) {
      clearAssetDropPreview();
      return;
    }

    const auto targetWidgetId = hitTestWidgetIdAtViewPoint(viewPoint);
    bool canDrop = false;
    if (targetWidgetId.has_value() &&
        !isWidgetEffectivelyLocked(*targetWidgetId)) {
      if (const auto options =
              resolveAssetDropOptions(*targetWidgetId, *assetPayload);
          options.has_value())
        canDrop = !options->empty();
    }

    updateAssetDropPreview(viewPoint, targetWidgetId, canDrop,
                           assetPayload->refKey);
  }

  void itemDragExit(const juce::DragAndDropTarget::SourceDetails &) override {
    clearWidgetLibraryDropPreview();
    clearAssetDropPreview();
  }

  void itemDropped(const juce::DragAndDropTarget::SourceDetails
                       &dragSourceDetails) override {
    if (isRunMode())
      return;

    const auto typeKey =
        extractWidgetLibraryTypeKey(dragSourceDetails.description);
    const auto viewPoint = dragSourceDetails.localPosition.toFloat();
    clearWidgetLibraryDropPreview();
    clearAssetDropPreview();

    if (typeKey.has_value()) {
      if (!isPointInCanvasView(viewPoint))
        return;

      if (onWidgetLibraryDrop != nullptr)
        onWidgetLibraryDrop(*typeKey, viewToWorld(viewPoint));
      return;
    }

    const auto assetPayload =
        extractAssetDragPayload(dragSourceDetails.description);
    if (!assetPayload.has_value() || !isPointInCanvasView(viewPoint))
      return;

    const auto targetWidgetId = hitTestWidgetIdAtViewPoint(viewPoint);
    if (!targetWidgetId.has_value())
      return;
    if (isWidgetEffectivelyLocked(*targetWidgetId))
      return;

    auto options = resolveAssetDropOptions(*targetWidgetId, *assetPayload);
    if (!options.has_value() || options->empty())
      return;

    applyAssetDropWithSelection(*targetWidgetId, *assetPayload,
                                std::move(*options),
                                dragSourceDetails.localPosition.toInt());
  }

  bool isPointInTopRuler(juce::Point<float> localPoint) const noexcept {
    if (localPoint.y < 0.0f ||
        localPoint.y >= static_cast<float>(kRulerThicknessPx))
      return false;

    return localPoint.x >= static_cast<float>(kRulerThicknessPx) &&
           localPoint.x < static_cast<float>(getWidth());
  }

  bool isPointInLeftRuler(juce::Point<float> localPoint) const noexcept {
    if (localPoint.x < 0.0f ||
        localPoint.x >= static_cast<float>(kRulerThicknessPx))
      return false;

    return localPoint.y >= static_cast<float>(kRulerThicknessPx) &&
           localPoint.y < static_cast<float>(getHeight());
  }

  juce::Point<float> worldToView(juce::Point<float> worldPoint) const noexcept {
    const auto viewport = viewportBounds();
    return {static_cast<float>(viewport.getX()) +
                (worldPoint.x - viewOriginWorld.x) * zoomLevel,
            static_cast<float>(viewport.getY()) +
                (worldPoint.y - viewOriginWorld.y) * zoomLevel};
  }

  juce::Point<float> viewToWorld(juce::Point<float> viewPoint) const noexcept {
    const auto viewport = viewportBounds();
    return {viewOriginWorld.x +
                (viewPoint.x - static_cast<float>(viewport.getX())) / zoomLevel,
            viewOriginWorld.y +
                (viewPoint.y - static_cast<float>(viewport.getY())) /
                    zoomLevel};
  }

  juce::Rectangle<float>
  worldToViewRect(const juce::Rectangle<float> &worldRect) const noexcept {
    const auto topLeft = worldToView(worldRect.getTopLeft());
    return {topLeft.x, topLeft.y, worldRect.getWidth() * zoomLevel,
            worldRect.getHeight() * zoomLevel};
  }

  juce::Rectangle<float>
  viewToWorldRect(const juce::Rectangle<float> &viewRect) const noexcept {
    const auto topLeft = viewToWorld(viewRect.getTopLeft());
    return {topLeft.x, topLeft.y, viewRect.getWidth() / zoomLevel,
            viewRect.getHeight() / zoomLevel};
  }

  bool focusWidget(WidgetId widgetId) {
    if (widgetId <= kRootId)
      return false;

    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return false;

    const auto viewport = viewportBounds();
    if (viewport.getWidth() <= 0 || viewport.getHeight() <= 0)
      return false;

    const auto visibleWorldW =
        static_cast<float>(viewport.getWidth()) / zoomLevel;
    const auto visibleWorldH =
        static_cast<float>(viewport.getHeight()) / zoomLevel;
    viewOriginWorld.x = widget->bounds.getCentreX() - visibleWorldW * 0.5f;
    viewOriginWorld.y = widget->bounds.getCentreY() - visibleWorldH * 0.5f;

    clampViewOriginToCanvas();
    updateAllWidgetViewBounds();
    repaint();
    notifyViewportChanged();
    return true;
  }

  bool focusWorldPoint(juce::Point<float> worldPoint) {
    const auto viewport = viewportBounds();
    if (viewport.getWidth() <= 0 || viewport.getHeight() <= 0)
      return false;

    const auto visibleWorldW =
        static_cast<float>(viewport.getWidth()) / zoomLevel;
    const auto visibleWorldH =
        static_cast<float>(viewport.getHeight()) / zoomLevel;
    const auto previousOrigin = viewOriginWorld;

    viewOriginWorld.x = worldPoint.x - visibleWorldW * 0.5f;
    viewOriginWorld.y = worldPoint.y - visibleWorldH * 0.5f;
    clampViewOriginToCanvas();

    if (areClose(previousOrigin.x, viewOriginWorld.x) &&
        areClose(previousOrigin.y, viewOriginWorld.y)) {
      return false;
    }

    updateAllWidgetViewBounds();
    repaint();
    notifyViewportChanged();
    return true;
  }

  juce::Rectangle<float> canvasWorldBounds() const {
    return {0.0f, 0.0f, kCanvasWorldWidth, kCanvasWorldHeight};
  }

  juce::Rectangle<float>
  clampBoundsToCanvas(juce::Rectangle<float> bounds,
                      const juce::Rectangle<float> &fallback) const noexcept {
    const auto canvas = canvasWorldBounds();

    auto clampRect = [&canvas](juce::Rectangle<float> value) noexcept {
      value.setWidth(std::min(value.getWidth(), canvas.getWidth()));
      value.setHeight(std::min(value.getHeight(), canvas.getHeight()));
      value.setX(juce::jlimit(
          canvas.getX(), canvas.getRight() - value.getWidth(), value.getX()));
      value.setY(juce::jlimit(
          canvas.getY(), canvas.getBottom() - value.getHeight(), value.getY()));
      return value;
    };

    bounds = clampRect(bounds);
    if (canvas.contains(bounds))
      return bounds;

    return clampRect(fallback);
  }

  void clampViewOriginToCanvas() {
    const auto viewport = viewportBounds();
    if (viewport.getWidth() <= 0 || viewport.getHeight() <= 0)
      return;

    const auto canvas = canvasWorldBounds();
    const auto visibleWorldW =
        static_cast<float>(viewport.getWidth()) / zoomLevel;
    const auto visibleWorldH =
        static_cast<float>(viewport.getHeight()) / zoomLevel;

    const auto canvasW = canvas.getWidth();
    const auto canvasH = canvas.getHeight();

    if (canvasW * zoomLevel <= static_cast<float>(viewport.getWidth())) {
      viewOriginWorld.x = canvas.getCentreX() - visibleWorldW * 0.5f;
    } else {
      const auto minX = canvas.getX();
      const auto maxX = canvas.getRight() - visibleWorldW;
      viewOriginWorld.x = juce::jlimit(minX, maxX, viewOriginWorld.x);
    }

    if (canvasH * zoomLevel <= static_cast<float>(viewport.getHeight())) {
      viewOriginWorld.y = canvas.getCentreY() - visibleWorldH * 0.5f;
    } else {
      const auto minY = canvas.getY();
      const auto maxY = canvas.getBottom() - visibleWorldH;
      viewOriginWorld.y = juce::jlimit(minY, maxY, viewOriginWorld.y);
    }
  }

  void recenterAndClampViewport() { clampViewOriginToCanvas(); }

  void updateAllWidgetViewBounds() {
    for (auto &view : widgetViews) {
      if (view == nullptr)
        continue;
      const auto *widget = findWidgetModel(view->widgetId());
      if (widget == nullptr)
        continue;
      view->setViewBounds(worldToViewRect(widget->bounds));
    }
  }

  void setZoomAtPoint(float nextZoom, juce::Point<float> localAnchor) {
    nextZoom = juce::jlimit(kMinCanvasZoom, kMaxCanvasZoom, nextZoom);
    if (std::abs(nextZoom - zoomLevel) <= 0.0001f)
      return;

    const auto worldAnchor = viewToWorld(localAnchor);
    zoomLevel = nextZoom;

    const auto viewport = viewportBounds();
    const auto viewportX = localAnchor.x - static_cast<float>(viewport.getX());
    const auto viewportY = localAnchor.y - static_cast<float>(viewport.getY());
    viewOriginWorld.x = worldAnchor.x - viewportX / zoomLevel;
    viewOriginWorld.y = worldAnchor.y - viewportY / zoomLevel;

    clampViewOriginToCanvas();
    updateAllWidgetViewBounds();
    repaint();
    notifyViewportChanged();
  }

  void repaintRulerTrackerDelta(std::optional<juce::Point<float>> previousPoint,
                                std::optional<juce::Point<float>> nextPoint) {
    const auto topRuler = topRulerBounds();
    const auto leftRuler = leftRulerBounds();
    if (topRuler.isEmpty() && leftRuler.isEmpty())
      return;

    juce::Rectangle<int> dirty;
    const auto includeRect = [&dirty](juce::Rectangle<int> rect) {
      if (rect.isEmpty())
        return;
      dirty = dirty.isEmpty() ? rect : dirty.getUnion(rect);
    };

    const auto includePoint = [&](juce::Point<float> point) {
      if (!topRuler.isEmpty()) {
        const auto clampedX =
            juce::jlimit(static_cast<float>(topRuler.getX()),
                         static_cast<float>(topRuler.getRight() - 1), point.x);
        includeRect({juce::roundToInt(clampedX) - 2, topRuler.getY(), 5,
                     topRuler.getHeight()});
      }

      if (!leftRuler.isEmpty()) {
        const auto clampedY = juce::jlimit(
            static_cast<float>(leftRuler.getY()),
            static_cast<float>(leftRuler.getBottom() - 1), point.y);
        includeRect({leftRuler.getX(), juce::roundToInt(clampedY) - 2,
                     leftRuler.getWidth(), 5});
      }
    };

    if (previousPoint.has_value())
      includePoint(*previousPoint);
    if (nextPoint.has_value())
      includePoint(*nextPoint);

    if (!dirty.isEmpty())
      repaint(dirty.expanded(1));
  }

  void setMouseTrackerPoint(juce::Point<float> localPoint) {
    if (hasMouseLocalPoint &&
        std::abs(lastMouseLocalPoint.x - localPoint.x) < 0.001f &&
        std::abs(lastMouseLocalPoint.y - localPoint.y) < 0.001f) {
      return;
    }

    const auto previousPoint =
        hasMouseLocalPoint
            ? std::optional<juce::Point<float>>(lastMouseLocalPoint)
            : std::nullopt;
    hasMouseLocalPoint = true;
    lastMouseLocalPoint = localPoint;
    repaintRulerTrackerDelta(previousPoint,
                             std::optional<juce::Point<float>>(localPoint));
  }

  void clearMouseTrackerPoint() {
    if (!hasMouseLocalPoint)
      return;

    const auto previousPoint =
        std::optional<juce::Point<float>>(lastMouseLocalPoint);
    hasMouseLocalPoint = false;
    repaintRulerTrackerDelta(previousPoint, std::nullopt);
  }

  void repaintGuideOverlayForLine(bool vertical, float worldPosition) {
    juce::Rectangle<int> dirty;
    const auto includeRect = [&dirty](juce::Rectangle<int> rect) {
      if (rect.isEmpty())
        return;
      dirty = dirty.isEmpty() ? rect : dirty.getUnion(rect);
    };

    const auto visibleCanvas = visibleCanvasViewBounds();
    if (visibleCanvas.getWidth() > 0.0f && visibleCanvas.getHeight() > 0.0f) {
      if (vertical) {
        const auto x = worldToView({worldPosition, 0.0f}).x;
        includeRect({juce::roundToInt(x) - 3,
                     juce::roundToInt(visibleCanvas.getY()), 7,
                     juce::roundToInt(visibleCanvas.getHeight())});
      } else {
        const auto y = worldToView({0.0f, worldPosition}).y;
        includeRect({juce::roundToInt(visibleCanvas.getX()),
                     juce::roundToInt(y) - 3,
                     juce::roundToInt(visibleCanvas.getWidth()), 7});
      }
    }

    const auto topRuler = topRulerBounds();
    const auto leftRuler = leftRulerBounds();
    if (vertical && !topRuler.isEmpty()) {
      const auto x = worldToView({worldPosition, 0.0f}).x;
      if (x >= static_cast<float>(topRuler.getX() - 4) &&
          x <= static_cast<float>(topRuler.getRight() + 4))
        includeRect({juce::roundToInt(x) - 3, topRuler.getY(), 7,
                     topRuler.getHeight()});
    } else if (!vertical && !leftRuler.isEmpty()) {
      const auto y = worldToView({0.0f, worldPosition}).y;
      if (y >= static_cast<float>(leftRuler.getY() - 4) &&
          y <= static_cast<float>(leftRuler.getBottom() + 4))
        includeRect({leftRuler.getX(), juce::roundToInt(y) - 3,
                     leftRuler.getWidth(), 7});
    }

    if (!dirty.isEmpty())
      repaint(dirty.expanded(2));
  }

  void updateMouseTrackerFromChild(juce::Point<float> localPoint) {
    setMouseTrackerPoint(localPoint);
  }

  bool removeGuideNearPoint(juce::Point<float> localPoint,
                            std::optional<bool> verticalOnly) {
    if (guides.empty())
      return false;

    auto bestIt = guides.end();
    auto bestDistance = std::numeric_limits<float>::max();

    for (auto it = guides.begin(); it != guides.end(); ++it) {
      if (verticalOnly.has_value() && it->vertical != *verticalOnly)
        continue;

      const auto linePos = it->vertical
                               ? worldToView({it->worldPosition, 0.0f}).x
                               : worldToView({0.0f, it->worldPosition}).y;
      const auto distance =
          std::abs((it->vertical ? localPoint.x : localPoint.y) - linePos);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestIt = it;
      }
    }

    if (bestIt == guides.end() || bestDistance > kGuideRemoveThresholdPx)
      return false;

    const auto removedGuide = *bestIt;
    guides.erase(bestIt);
    repaintGuideOverlayForLine(removedGuide.vertical,
                               removedGuide.worldPosition);
    return true;
  }

  void handleCanvasDoubleClick(juce::Point<float> localPoint) {
    const auto hadPreviewGuide =
        guideDragState.active && guideDragState.previewInViewport;
    const auto previewGuideVertical = guideDragState.vertical;
    const auto previewGuideWorldPosition = guideDragState.worldPosition;
    if (guideDragState.active)
      guideDragState = {};
    if (hadPreviewGuide)
      repaintGuideOverlayForLine(previewGuideVertical,
                                 previewGuideWorldPosition);

    if (isPointInTopRuler(localPoint)) {
      removeGuideNearPoint(localPoint, true);
      return;
    }

    if (isPointInLeftRuler(localPoint)) {
      removeGuideNearPoint(localPoint, false);
      return;
    }

    if (isPointInCanvasView(localPoint))
      removeGuideNearPoint(localPoint, std::nullopt);
  }

  std::vector<juce::Rectangle<float>> collectNearbyBoundsForSnap(
      const std::vector<WidgetId> &excludedWidgetIds) const {
    std::vector<juce::Rectangle<float>> bounds;
    bounds.reserve(document.snapshot().widgets.size());

    for (const auto &widget : document.snapshot().widgets) {
      if (containsWidgetId(excludedWidgetIds, widget.id))
        continue;
      if (!isWidgetEffectivelyVisible(widget.id))
        continue;

      bounds.push_back(widget.bounds);
    }

    return bounds;
  }

  std::vector<float> collectRulerGuidePositions(bool vertical) const {
    std::vector<float> positions;
    positions.reserve(guides.size());
    for (const auto &guide : guides) {
      if (guide.vertical == vertical)
        positions.push_back(guide.worldPosition);
    }
    return positions;
  }

  Interaction::SnapRequest
  makeSnapRequest(const juce::Rectangle<float> &proposedBounds,
                  const std::vector<WidgetId> &excludedWidgetIds) const {
    Interaction::SnapRequest request;
    request.proposedBounds = proposedBounds;
    request.nearbyBounds = collectNearbyBoundsForSnap(excludedWidgetIds);
    request.verticalGuides = collectRulerGuidePositions(true);
    request.horizontalGuides = collectRulerGuidePositions(false);
    request.settings = snapSettings;
    return request;
  }

  void clearTransientSnapGuides() {
    if (transientSnapGuides.empty() && transientSmartSpacingHints.empty())
      return;

    transientSnapGuides.clear();
    transientSmartSpacingHints.clear();
    repaint();
  }

  void updateTransientSnapGuides(const Interaction::SnapResult &snapResult) {
    transientSnapGuides.clear();
    transientSmartSpacingHints = snapResult.spacingHints;

    if (snapResult.snapKindX == Interaction::SnapKind::smartAlign &&
        snapResult.guideX.has_value())
      transientSnapGuides.push_back({true, *snapResult.guideX});

    if (snapResult.snapKindY == Interaction::SnapKind::smartAlign &&
        snapResult.guideY.has_value())
      transientSnapGuides.push_back({false, *snapResult.guideY});
  }

  void clearMouseTrackerFromChild() { clearMouseTrackerPoint(); }

  void applyWheelZoomAtPoint(juce::Point<float> localPoint,
                             const juce::MouseWheelDetails &wheel) {
    if (!isPointInCanvasView(localPoint))
      return;

    const auto delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    if (std::abs(delta) <= 0.0001f)
      return;

    const auto factor = std::pow(1.1f, delta * 4.0f);
    setZoomAtPoint(zoomLevel * static_cast<float>(factor), localPoint);
  }

  WidgetId createWidget(WidgetType type) {
    const auto newWidgetId = widgetFactory.createWidget(
        document, type, createDefaultOrigin(), resolveActiveLayerId());
    if (newWidgetId <= kRootId)
      return 0;

    document.selectSingle(newWidgetId);
    refreshFromDocument();
    grabKeyboardFocus();
    return newWidgetId;
  }

  bool deleteSelection() {
    if (document.ungroupSelection()) {
      activeGroupEditId.reset();
      refreshFromDocument();
      grabKeyboardFocus();
      return true;
    }

    const auto selection = document.editorState().selection;
    if (selection.empty())
      return false;

    DeleteAction action;
    action.kind = NodeKind::widget;
    action.ids = selection;
    std::sort(action.ids.begin(), action.ids.end());
    action.ids.erase(std::unique(action.ids.begin(), action.ids.end()),
                     action.ids.end());

    const auto changed = document.deleteNodes(action);

    if (changed) {
      refreshFromDocument();
      grabKeyboardFocus();
    }

    return changed;
  }

  bool performUndo() {
    if (!document.undo())
      return false;

    refreshFromDocument();
    grabKeyboardFocus();
    return true;
  }

  bool performRedo() {
    if (!document.redo())
      return false;

    refreshFromDocument();
    grabKeyboardFocus();
    return true;
  }

  bool groupSelection() {
    if (!document.groupSelection(resolveActiveLayerId()))
      return false;

    activeGroupEditId.reset();
    refreshFromDocument();
    grabKeyboardFocus();
    return true;
  }

  bool ungroupSelection() {
    if (!document.ungroupSelection())
      return false;

    activeGroupEditId.reset();
    refreshFromDocument();
    grabKeyboardFocus();
    return true;
  }

  bool canGroupSelection() const noexcept {
    return document.editorState().selection.size() >= 2;
  }

  bool canUngroupSelection() const noexcept {
    const auto &selection = document.editorState().selection;
    if (selection.empty())
      return false;

    for (const auto id : selection) {
      if (findGroupByMember(id) != nullptr)
        return true;
    }

    return false;
  }

  bool enterGroupEditMode() {
    const auto groupId = selectedWholeGroupId();
    if (!groupId.has_value())
      return false;

    activeGroupEditId = *groupId;
    repaint();
    notifyStateChanged();
    return true;
  }

  bool exitGroupEditMode(bool restoreWholeGroupSelection) {
    if (!activeGroupEditId.has_value())
      return false;

    const auto previousGroupId = *activeGroupEditId;
    activeGroupEditId.reset();

    if (restoreWholeGroupSelection) {
      if (const auto *group = findGroupById(previousGroupId))
        document.setSelection(group->memberWidgetIds);
    }

    syncSelectionToViews();
    return true;
  }

  void refreshFromDocument() {
    clearWidgetLibraryDropPreview();
    clearAssetDropPreview();
    const auto refreshStart = std::chrono::steady_clock::now();
    dragState = {};
    marqueeState = {};

    bool groupEditReset = false;
    if (activeGroupEditId.has_value() &&
        findGroupById(*activeGroupEditId) == nullptr) {
      activeGroupEditId.reset();
      groupEditReset = true;
    }

    std::unordered_map<WidgetId, std::unique_ptr<WidgetComponent>>
        previousViewsById;
    previousViewsById.reserve(widgetViews.size());

    std::unordered_map<WidgetId, juce::Rectangle<float>> previousBoundsById;
    previousBoundsById.reserve(widgetViews.size());
    for (auto &view : widgetViews) {
      if (view == nullptr)
        continue;

      const auto id = view->widgetId();
      previousBoundsById[id] = view->getBounds().toFloat();
      previousViewsById[id] = std::move(view);
    }

    const auto previousSelection = lastSelectionSnapshot;
    widgetViews.clear();
    widgetViews.reserve(document.snapshot().widgets.size());

    const auto &selection = document.editorState().selection;
    const auto showSelectionVisuals = !isRunMode();
    const auto showWidgetHandles =
        showSelectionVisuals && selection.size() == 1;
    const auto orderedIds = orderedWidgetIdsForCanvas();
    clampViewOriginToCanvas();

    std::unordered_map<WidgetId, juce::Rectangle<float>> nextBoundsById;
    nextBoundsById.reserve(orderedIds.size());

    bool hasDirtyBounds = false;
    juce::Rectangle<float> dirtyBounds;
    const auto addDirtyBounds =
        [&hasDirtyBounds, &dirtyBounds](const juce::Rectangle<float> &bounds) {
          if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f)
            return;
          if (!hasDirtyBounds) {
            dirtyBounds = bounds;
            hasDirtyBounds = true;
            return;
          }

          dirtyBounds = unionRect(dirtyBounds, bounds);
        };

    for (const auto widgetId : orderedIds) {
      const auto *widget = findWidgetModel(widgetId);
      if (widget == nullptr)
        continue;
      if (!isWidgetEffectivelyVisible(widget->id))
        continue;

      const auto isSelected =
          showSelectionVisuals && containsWidgetId(selection, widget->id);
      const auto *group = findGroupByMember(widget->id);
      const auto isGrouped = group != nullptr;
      const auto groupedInActiveEdit =
          isGrouped && activeGroupEditId.has_value() &&
          isWidgetInGroup(widget->id, *activeGroupEditId);
      const auto effectiveOpacity = effectiveOpacityForWidget(widget->id);

      const auto widgetViewBounds = worldToViewRect(widget->bounds);
      nextBoundsById[widget->id] = widgetViewBounds;
      std::unique_ptr<WidgetComponent> view;
      if (const auto previousIt = previousViewsById.find(widget->id);
          previousIt != previousViewsById.end()) {
        view = std::move(previousIt->second);
        previousViewsById.erase(previousIt);
        if (const auto oldBoundsIt = previousBoundsById.find(widget->id);
            oldBoundsIt != previousBoundsById.end())
          addDirtyBounds(unionRect(oldBoundsIt->second, widgetViewBounds));
      } else {
        view = std::make_unique<WidgetComponent>(
            *this, renderer, *widget, effectiveOpacity, isSelected,
            isSelected && showWidgetHandles, isGrouped, groupedInActiveEdit);
        addDirtyBounds(widgetViewBounds);
      }

      view->updateFromModel(*widget, effectiveOpacity, isSelected,
                            isSelected && showWidgetHandles, isGrouped,
                            groupedInActiveEdit);
      view->setViewBounds(widgetViewBounds);
      addAndMakeVisible(*view);
      view->toFront(false);
      widgetViews.push_back(std::move(view));
    }

    for (auto &[widgetId, orphanedView] : previousViewsById) {
      juce::ignoreUnused(widgetId);
      if (orphanedView == nullptr)
        continue;
      addDirtyBounds(orphanedView->getBounds().toFloat());
      removeChildComponent(orphanedView.get());
    }

    const auto appendSelectionDirty =
        [&addDirtyBounds](
            const std::vector<WidgetId> &ids,
            const std::unordered_map<WidgetId, juce::Rectangle<float>>
                &boundsById) {
          bool hasSelectionBounds = false;
          juce::Rectangle<float> selectionBounds;
          for (const auto id : ids) {
            const auto it = boundsById.find(id);
            if (it == boundsById.end())
              continue;

            if (!hasSelectionBounds) {
              selectionBounds = it->second;
              hasSelectionBounds = true;
            } else {
              selectionBounds = unionRect(selectionBounds, it->second);
            }
          }

          if (hasSelectionBounds)
            addDirtyBounds(selectionBounds);
        };

    appendSelectionDirty(previousSelection, previousBoundsById);
    appendSelectionDirty(selection, nextBoundsById);
    lastSelectionSnapshot = selection;

    bool requestedFullRepaint = false;
    bool requestedPartialRepaint = false;
    float dirtyAreaPx = 0.0f;
    if (groupEditReset) {
      repaint();
      requestedFullRepaint = true;
    } else if (hasDirtyBounds) {
      repaint(dirtyBounds.expanded(6.0f, 6.0f).getSmallestIntegerContainer());
      requestedPartialRepaint = true;
      dirtyAreaPx =
          std::max(0.0f, dirtyBounds.getWidth() * dirtyBounds.getHeight());
    }

    const auto refreshEnd = std::chrono::steady_clock::now();
    const auto refreshMs =
        std::chrono::duration<double, std::milli>(refreshEnd - refreshStart)
            .count();
    perf.refreshCount += 1;
    perf.lastRefreshMs = refreshMs;
    perf.maxRefreshMs = std::max(perf.maxRefreshMs, refreshMs);
    perf.lastWidgetViewCount = static_cast<int>(widgetViews.size());
    perf.lastSelectionCount = static_cast<int>(selection.size());
    perf.lastDirtyAreaPx = dirtyAreaPx;
    if (requestedFullRepaint)
      perf.refreshRequestedFullRepaintCount += 1;
    if (requestedPartialRepaint)
      perf.refreshRequestedPartialRepaintCount += 1;

    const auto shouldLogRefresh =
        refreshMs >= slowCanvasRefreshLogThresholdMs ||
        (periodicCanvasPerfLogInterval > 0 &&
         perf.refreshCount % periodicCanvasPerfLogInterval == 0);
    if (shouldLogRefresh) {
      DBG(juce::String("[Gyeol][Canvas][Perf] refresh#") +
          juce::String(static_cast<int64_t>(perf.refreshCount)) +
          " ms=" + juce::String(refreshMs, 3) +
          " widgets=" + juce::String(perf.lastWidgetViewCount) +
          " selection=" + juce::String(perf.lastSelectionCount) +
          " dirtyPx=" + juce::String(static_cast<int>(perf.lastDirtyAreaPx)) +
          " partialReq=" + juce::String(requestedPartialRepaint ? 1 : 0) +
          " fullReq=" + juce::String(requestedFullRepaint ? 1 : 0) +
          " dragPreviewUpdates=" +
          juce::String(static_cast<int64_t>(perf.dragPreviewUpdateCount)) +
          " maxRefreshMs=" + juce::String(perf.maxRefreshMs, 3));
    }

    notifyStateChanged();
    notifyViewportChanged();
  }

  void syncSelectionFromDocument() { syncSelectionToViews(); }

  void paint(juce::Graphics &g) override {
    const auto paintStart = std::chrono::steady_clock::now();
    const auto finishPaint = [this, &paintStart]() {
      const auto paintEnd = std::chrono::steady_clock::now();
      const auto paintMs =
          std::chrono::duration<double, std::milli>(paintEnd - paintStart)
              .count();
      perf.paintCount += 1;
      perf.lastPaintMs = paintMs;
      perf.maxPaintMs = std::max(perf.maxPaintMs, paintMs);

      const auto shouldLogPaint =
          paintMs >= slowCanvasPaintLogThresholdMs ||
          (periodicCanvasPerfLogInterval > 0 &&
           perf.paintCount % periodicCanvasPerfLogInterval == 0);
      if (shouldLogPaint) {
        DBG(juce::String("[Gyeol][Canvas][Perf] paint#") +
            juce::String(static_cast<int64_t>(perf.paintCount)) + " ms=" +
            juce::String(paintMs, 3) + " zoom=" + juce::String(zoomLevel, 3) +
            " widgets=" + juce::String(perf.lastWidgetViewCount) +
            " selection=" + juce::String(perf.lastSelectionCount) +
            " maxPaintMs=" + juce::String(perf.maxPaintMs, 3));
      }
    };

    g.fillAll(juce::Colour::fromRGB(16, 18, 24));

    const auto viewport = viewportBounds();
    if (viewport.getWidth() <= 0 || viewport.getHeight() <= 0) {
      finishPaint();
      return;
    }

    g.setColour(juce::Colour::fromRGB(18, 20, 25));
    g.fillRect(viewport);

    // Draw world-aligned grid only inside the visible canvas area.
    const auto canvasViewBounds = worldToViewRect(canvasWorldBounds());
    const auto visibleCanvasBounds =
        canvasViewBounds.getIntersection(viewport.toFloat());
    if (snapSettings.enableGrid && visibleCanvasBounds.getWidth() > 0.0f &&
        visibleCanvasBounds.getHeight() > 0.0f) {
      g.saveState();
      g.reduceClipRegion(visibleCanvasBounds.toNearestInt());

      const auto worldStart = viewToWorld(visibleCanvasBounds.getTopLeft());
      const auto worldEnd = viewToWorld(visibleCanvasBounds.getBottomRight());

      const auto drawGrid = [&](float worldStep, juce::Colour colour) {
        if (worldStep <= 0.0f)
          return;

        g.setColour(colour);

        auto startX = std::floor(worldStart.x / worldStep) * worldStep;
        for (float worldX = startX; worldX <= worldEnd.x + worldStep;
             worldX += worldStep) {
          const auto x = worldToView({worldX, 0.0f}).x;
          g.drawVerticalLine(juce::roundToInt(x), visibleCanvasBounds.getY(),
                             visibleCanvasBounds.getBottom());
        }

        auto startY = std::floor(worldStart.y / worldStep) * worldStep;
        for (float worldY = startY; worldY <= worldEnd.y + worldStep;
             worldY += worldStep) {
          const auto y = worldToView({0.0f, worldY}).y;
          g.drawHorizontalLine(juce::roundToInt(y), visibleCanvasBounds.getX(),
                               visibleCanvasBounds.getRight());
        }
      };

      const auto minorStep = std::max(1.0f, snapSettings.gridSize);
      const auto majorStep = minorStep * 4.0f;

      // Phase 3: �� ������ ���� �׸���
      // ���̵�ƿ�
      // ���� �۾������� �׸��尡
      // �������� �ð��� ����� ����
      const auto gridFade =
          juce::jlimit(0.0f, 1.0f,
                       (zoomLevel - 0.3f) / 0.5f); // 0.3 ���Ͽ��� ���� ����
      const auto minorAlpha = static_cast<juce::uint8>(
          juce::jlimit(0.0f, 255.0f, 12.0f * gridFade));
      const auto majorAlpha = static_cast<juce::uint8>(
          juce::jlimit(0.0f, 255.0f, 24.0f * gridFade));

      if (minorAlpha > 0)
        drawGrid(minorStep, juce::Colour::fromRGBA(255, 255, 255, minorAlpha));
      if (majorAlpha > 0)
        drawGrid(majorStep, juce::Colour::fromRGBA(255, 255, 255, majorAlpha));
      g.restoreState();
    }

    g.saveState();
    g.reduceClipRegion(viewport);
    g.setColour(juce::Colour::fromRGBA(82, 140, 220, 90));
    g.drawRect(canvasViewBounds.toNearestInt(), 1);
    g.restoreState();

    // Ruler background.
    const auto topRuler = topRulerBounds();
    const auto leftRuler = leftRulerBounds();
    const auto corner =
        juce::Rectangle<int>(0, 0, kRulerThicknessPx, kRulerThicknessPx);
    g.setColour(juce::Colour::fromRGB(26, 30, 38));
    g.fillRect(topRuler);
    g.fillRect(leftRuler);
    g.setColour(juce::Colour::fromRGB(30, 34, 42));
    g.fillRect(corner);

    finishPaint();
  }

  void paintOverChildren(juce::Graphics &g) override {
    const auto viewport = viewportBounds();
    if (viewport.getWidth() <= 0 || viewport.getHeight() <= 0)
      return;

    g.saveState();
    g.reduceClipRegion(viewport);

    const auto visibleCanvasBounds = visibleCanvasViewBounds();
    if (visibleCanvasBounds.getWidth() > 0.0f &&
        visibleCanvasBounds.getHeight() > 0.0f) {
      g.saveState();
      g.reduceClipRegion(visibleCanvasBounds.toNearestInt());

      for (const auto &guide : guides) {
        g.setColour(juce::Colour::fromRGBA(255, 160, 75, 210));
        if (guide.vertical) {
          const auto x = worldToView({guide.worldPosition, 0.0f}).x;
          g.drawVerticalLine(juce::roundToInt(x), visibleCanvasBounds.getY(),
                             visibleCanvasBounds.getBottom());
        } else {
          const auto y = worldToView({0.0f, guide.worldPosition}).y;
          g.drawHorizontalLine(juce::roundToInt(y), visibleCanvasBounds.getX(),
                               visibleCanvasBounds.getRight());
        }
      }

      if (guideDragState.active && guideDragState.previewInViewport) {
        g.setColour(juce::Colour::fromRGBA(255, 212, 140, 225));
        if (guideDragState.vertical) {
          const auto x = worldToView({guideDragState.worldPosition, 0.0f}).x;
          g.drawVerticalLine(juce::roundToInt(x), visibleCanvasBounds.getY(),
                             visibleCanvasBounds.getBottom());
        } else {
          const auto y = worldToView({0.0f, guideDragState.worldPosition}).y;
          g.drawHorizontalLine(juce::roundToInt(y), visibleCanvasBounds.getX(),
                               visibleCanvasBounds.getRight());
        }
      }

      if (!transientSnapGuides.empty()) {
        static constexpr float kDashPattern[]{6.0f, 4.0f};
        g.setColour(juce::Colour::fromRGBA(84, 212, 255, 230));
        for (const auto &guide : transientSnapGuides) {
          if (guide.vertical) {
            const auto x = worldToView({guide.worldPosition, 0.0f}).x;
            g.drawDashedLine({x, visibleCanvasBounds.getY(), x,
                              visibleCanvasBounds.getBottom()},
                             kDashPattern, 2, 1.3f);
          } else {
            const auto y = worldToView({0.0f, guide.worldPosition}).y;
            g.drawDashedLine({visibleCanvasBounds.getX(), y,
                              visibleCanvasBounds.getRight(), y},
                             kDashPattern, 2, 1.3f);
          }
        }
      }

      if (!transientSmartSpacingHints.empty()) {
        static constexpr float kDashPattern[]{5.0f, 4.0f};
        g.setColour(juce::Colour::fromRGBA(84, 212, 255, 220));
        g.setFont(juce::FontOptions(11.0f));
        const auto drawGapLabel = [&g](float centerX, float centerY,
                                       const juce::String &text) {
          auto box =
              juce::Rectangle<int>(juce::roundToInt(centerX - 24.0f),
                                   juce::roundToInt(centerY - 7.0f), 48, 14);
          g.setColour(juce::Colour::fromRGBA(14, 22, 32, 220));
          g.fillRoundedRectangle(box.toFloat(), 3.0f);
          g.setColour(juce::Colour::fromRGBA(84, 212, 255, 230));
          g.drawRoundedRectangle(box.toFloat(), 3.0f, 1.0f);
          g.setColour(juce::Colour::fromRGBA(176, 230, 255, 245));
          g.drawText(text, box, juce::Justification::centred, false);
        };

        for (const auto &hint : transientSmartSpacingHints) {
          const auto text = juce::String(hint.gap, 1);
          if (hint.horizontal) {
            const auto y = worldToView({0.0f, hint.axisPosition}).y;
            const auto x1a = worldToView({hint.firstStart, 0.0f}).x;
            const auto x1b = worldToView({hint.firstEnd, 0.0f}).x;
            const auto x2a = worldToView({hint.secondStart, 0.0f}).x;
            const auto x2b = worldToView({hint.secondEnd, 0.0f}).x;

            g.drawDashedLine({x1a, y, x1b, y}, kDashPattern, 2, 1.2f);
            g.drawDashedLine({x2a, y, x2b, y}, kDashPattern, 2, 1.2f);

            drawGapLabel((x1a + x1b) * 0.5f, y - 18.0f, text);
            drawGapLabel((x2a + x2b) * 0.5f, y - 18.0f, text);
          } else {
            const auto x = worldToView({hint.axisPosition, 0.0f}).x;
            const auto y1a = worldToView({0.0f, hint.firstStart}).y;
            const auto y1b = worldToView({0.0f, hint.firstEnd}).y;
            const auto y2a = worldToView({0.0f, hint.secondStart}).y;
            const auto y2b = worldToView({0.0f, hint.secondEnd}).y;

            g.drawDashedLine({x, y1a, x, y1b}, kDashPattern, 2, 1.2f);
            g.drawDashedLine({x, y2a, x, y2b}, kDashPattern, 2, 1.2f);

            drawGapLabel(x + 30.0f, (y1a + y1b) * 0.5f, text);
            drawGapLabel(x + 30.0f, (y2a + y2b) * 0.5f, text);
          }
        }
      }

      g.restoreState();
    }

    if (marqueeState.active) {
      const auto rect = marqueeState.bounds.toNearestInt();
      g.setColour(juce::Colour::fromRGBA(78, 156, 255, 34));
      g.fillRect(rect);
      g.setColour(juce::Colour::fromRGBA(78, 156, 255, 200));
      g.drawRect(rect, 1);
    }

    if (widgetLibraryDropPreviewActive) {
      const auto p = widgetLibraryDropPreviewView;
      g.setColour(juce::Colour::fromRGBA(84, 212, 255, 220));
      g.drawLine(p.x - 10.0f, p.y, p.x + 10.0f, p.y, 1.4f);
      g.drawLine(p.x, p.y - 10.0f, p.x, p.y + 10.0f, 1.4f);
      g.setColour(juce::Colour::fromRGBA(84, 212, 255, 80));
      g.fillEllipse(p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
      g.setColour(juce::Colour::fromRGBA(84, 212, 255, 220));
      g.drawEllipse(p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, 1.2f);
    }

    if (assetDropPreviewActive) {
      const auto previewColor =
          assetDropPreviewValid ? juce::Colour::fromRGBA(112, 214, 156, 230)
                                : juce::Colour::fromRGBA(255, 124, 124, 230);

      if (assetDropPreviewWidgetId > kRootId) {
        if (const auto *widget = findWidgetModel(assetDropPreviewWidgetId);
            widget != nullptr) {
          const auto bounds = worldToViewRect(widget->bounds).expanded(2.0f);
          g.setColour(previewColor.withAlpha(0.22f));
          g.fillRoundedRectangle(bounds, 5.0f);
          g.setColour(previewColor);
          g.drawRoundedRectangle(bounds, 5.0f, 2.0f);

          if (assetDropPreviewRefKey.isNotEmpty()) {
            auto labelBounds =
                bounds
                    .withSizeKeepingCentre(std::min(180.0f, bounds.getWidth()),
                                           18.0f)
                    .withY(bounds.getY() - 22.0f);
            if (labelBounds.getY() < visibleCanvasBounds.getY())
              labelBounds.setY(bounds.getBottom() + 4.0f);

            g.setColour(juce::Colour::fromRGBA(14, 22, 32, 220));
            g.fillRoundedRectangle(labelBounds, 4.0f);
            g.setColour(previewColor);
            g.drawRoundedRectangle(labelBounds, 4.0f, 1.0f);
            g.setColour(juce::Colour::fromRGB(228, 236, 246));
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawFittedText(assetDropPreviewRefKey, labelBounds.toNearestInt(),
                             juce::Justification::centred, 1);
          }
        }
      } else {
        const auto p = assetDropPreviewView;
        g.setColour(previewColor);
        g.drawLine(p.x - 8.0f, p.y - 8.0f, p.x + 8.0f, p.y + 8.0f, 1.4f);
        g.drawLine(p.x - 8.0f, p.y + 8.0f, p.x + 8.0f, p.y - 8.0f, 1.4f);
      }
    }

    paintGroupOverlays(g);

    juce::Rectangle<float> selectionBounds;
    if (computeCurrentSelectionUnionBounds(selectionBounds)) {
      const auto outline = juce::Colour::fromRGB(78, 156, 255);

      // Phase 3: �׿� Glow �ܰ��� (��Ƽ ������)
      g.setColour(outline.withAlpha(0.06f));
      g.drawRoundedRectangle(selectionBounds.expanded(2.5f), 7.0f, 4.0f);
      g.setColour(outline.withAlpha(0.12f));
      g.drawRoundedRectangle(selectionBounds.expanded(1.0f), 6.0f, 2.0f);
      g.setColour(outline);
      g.drawRoundedRectangle(selectionBounds.reduced(0.5f), 5.0f, 1.5f);

      // Phase 3: �������� �ڵ� ���� + Glow
      const auto handle = selectionResizeHandleBounds(selectionBounds);
      const auto handleCenter = handle.getCentre();
      const auto handleRadius =
          std::min(handle.getWidth(), handle.getHeight()) * 0.5f;

      g.setColour(outline.withAlpha(0.12f));
      g.fillEllipse(handleCenter.x - handleRadius - 2.5f,
                    handleCenter.y - handleRadius - 2.5f,
                    (handleRadius + 2.5f) * 2.0f, (handleRadius + 2.5f) * 2.0f);
      g.setColour(outline);
      g.fillEllipse(handleCenter.x - handleRadius,
                    handleCenter.y - handleRadius, handleRadius * 2.0f,
                    handleRadius * 2.0f);
      g.setColour(juce::Colours::white);
      g.fillEllipse(handleCenter.x - handleRadius * 0.35f,
                    handleCenter.y - handleRadius * 0.35f, handleRadius * 0.7f,
                    handleRadius * 0.7f);
    }

    g.restoreState();

    const auto topRuler =
        juce::Rectangle<int>(kRulerThicknessPx, 0,
                             getWidth() - kRulerThicknessPx, kRulerThicknessPx);
    const auto leftRuler =
        juce::Rectangle<int>(0, kRulerThicknessPx, kRulerThicknessPx,
                             getHeight() - kRulerThicknessPx);
    const auto worldStart = viewToWorld({static_cast<float>(viewport.getX()),
                                         static_cast<float>(viewport.getY())});
    const auto worldEnd =
        viewToWorld({static_cast<float>(viewport.getRight()),
                     static_cast<float>(viewport.getBottom())});

    const auto rawMajorStep = 80.0f / zoomLevel;
    const auto magnitude = std::pow(
        10.0f, std::floor(std::log10(std::max(0.0001f, rawMajorStep))));
    float majorStep = magnitude;
    for (const auto candidate : {1.0f, 2.0f, 5.0f, 10.0f}) {
      majorStep = magnitude * candidate;
      if (majorStep >= rawMajorStep)
        break;
    }
    const auto minorStep = majorStep / 5.0f;

    g.setColour(juce::Colour::fromRGB(90, 98, 112));
    g.drawLine(static_cast<float>(topRuler.getX()),
               static_cast<float>(topRuler.getBottom() - 1),
               static_cast<float>(topRuler.getRight()),
               static_cast<float>(topRuler.getBottom() - 1));
    g.drawLine(static_cast<float>(leftRuler.getRight() - 1),
               static_cast<float>(leftRuler.getY()),
               static_cast<float>(leftRuler.getRight() - 1),
               static_cast<float>(leftRuler.getBottom()));

    const auto formatCoord = [](float value) -> juce::String {
      if (std::abs(value) >= 100.0f ||
          std::abs(value - std::round(value)) <= 0.001f)
        return juce::String(static_cast<int>(std::round(value)));
      return juce::String(value, 1);
    };

    auto startXMinor = std::floor(worldStart.x / minorStep) * minorStep;
    for (float worldX = startXMinor; worldX <= worldEnd.x + minorStep;
         worldX += minorStep) {
      const auto x = worldToView({worldX, 0.0f}).x;
      const bool isMajor =
          std::abs(std::fmod(std::abs(worldX), majorStep)) <=
              (minorStep * 0.2f) ||
          std::abs(majorStep - std::fmod(std::abs(worldX), majorStep)) <=
              (minorStep * 0.2f);
      const auto tick = isMajor ? 10.0f : 6.0f;
      g.setColour(juce::Colour::fromRGBA(175, 183, 196, isMajor ? 210 : 120));
      g.drawLine(x, static_cast<float>(topRuler.getBottom()) - tick, x,
                 static_cast<float>(topRuler.getBottom()));
      if (isMajor) {
        g.setColour(juce::Colour::fromRGB(185, 192, 205));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(formatCoord(worldX),
                   juce::Rectangle<int>(juce::roundToInt(x) + 2, 2, 56,
                                        kRulerThicknessPx - 2),
                   juce::Justification::topLeft, false);
      }
    }

    auto startYMinor = std::floor(worldStart.y / minorStep) * minorStep;
    for (float worldY = startYMinor; worldY <= worldEnd.y + minorStep;
         worldY += minorStep) {
      const auto y = worldToView({0.0f, worldY}).y;
      const bool isMajor =
          std::abs(std::fmod(std::abs(worldY), majorStep)) <=
              (minorStep * 0.2f) ||
          std::abs(majorStep - std::fmod(std::abs(worldY), majorStep)) <=
              (minorStep * 0.2f);
      const auto tick = isMajor ? 10.0f : 6.0f;
      g.setColour(juce::Colour::fromRGBA(175, 183, 196, isMajor ? 210 : 120));
      g.drawLine(static_cast<float>(leftRuler.getRight()) - tick, y,
                 static_cast<float>(leftRuler.getRight()), y);
      if (isMajor) {
        g.setColour(juce::Colour::fromRGB(185, 192, 205));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText(formatCoord(worldY),
                   juce::Rectangle<int>(1, juce::roundToInt(y) - 8,
                                        kRulerThicknessPx - 2, 16),
                   juce::Justification::centredRight, false);
      }
    }

    if (hasMouseLocalPoint) {
      g.setColour(juce::Colour::fromRGBA(120, 170, 245, 220));
      const auto clampedX = juce::jlimit(
          static_cast<float>(topRuler.getX()),
          static_cast<float>(topRuler.getRight()), lastMouseLocalPoint.x);
      const auto clampedY = juce::jlimit(
          static_cast<float>(leftRuler.getY()),
          static_cast<float>(leftRuler.getBottom()), lastMouseLocalPoint.y);

      g.drawLine(clampedX, static_cast<float>(topRuler.getY()), clampedX,
                 static_cast<float>(topRuler.getBottom()));
      g.drawLine(static_cast<float>(leftRuler.getX()), clampedY,
                 static_cast<float>(leftRuler.getRight()), clampedY);
    }

    // Draw permanent guide markers on rulers.
    g.setColour(juce::Colour::fromRGBA(255, 160, 75, 220));
    for (const auto &guide : guides) {
      if (guide.vertical) {
        const auto x = worldToView({guide.worldPosition, 0.0f}).x;
        if (x >= static_cast<float>(topRuler.getX()) &&
            x <= static_cast<float>(topRuler.getRight()))
          g.drawLine(x, static_cast<float>(topRuler.getY()), x,
                     static_cast<float>(topRuler.getBottom()), 1.5f);
      } else {
        const auto y = worldToView({0.0f, guide.worldPosition}).y;
        if (y >= static_cast<float>(leftRuler.getY()) &&
            y <= static_cast<float>(leftRuler.getBottom()))
          g.drawLine(static_cast<float>(leftRuler.getX()), y,
                     static_cast<float>(leftRuler.getRight()), y, 1.5f);
      }
    }

    // Draw guide-preview marker on rulers while dragging from ruler.
    if (guideDragState.active && guideDragState.previewInViewport) {
      g.setColour(juce::Colour::fromRGBA(255, 212, 140, 230));
      if (guideDragState.vertical) {
        const auto x = worldToView({guideDragState.worldPosition, 0.0f}).x;
        if (x >= static_cast<float>(topRuler.getX()) &&
            x <= static_cast<float>(topRuler.getRight()))
          g.drawLine(x, static_cast<float>(topRuler.getY()), x,
                     static_cast<float>(topRuler.getBottom()), 1.5f);
      } else {
        const auto y = worldToView({0.0f, guideDragState.worldPosition}).y;
        if (y >= static_cast<float>(leftRuler.getY()) &&
            y <= static_cast<float>(leftRuler.getBottom()))
          g.drawLine(static_cast<float>(leftRuler.getX()), y,
                     static_cast<float>(leftRuler.getRight()), y, 1.5f);
      }
    }
  }

  void mouseDown(const juce::MouseEvent &event) override {
    setMouseTrackerPoint(event.position);
    clearTransientSnapGuides();

    if (!event.mods.isLeftButtonDown())
      return;

    if (event.getNumberOfClicks() > 1)
      return;

    grabKeyboardFocus();

    if (isRunMode()) {
      if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)) {
        panState.active = true;
        panState.startMouse = event.position;
        panState.startViewOriginWorld = viewOriginWorld;
      }
      return;
    }

    if (isPointInTopRuler(event.position) ||
        isPointInLeftRuler(event.position)) {
      guideDragState = {};
      guideDragState.active = true;
      // Top ruler -> vertical guide, Left ruler -> horizontal guide.
      guideDragState.vertical = isPointInTopRuler(event.position);
      guideDragState.startMouse = event.position;
      guideDragState.previewInViewport = false;
      return;
    }

    if (!isPointInCanvasView(event.position))
      return;

    if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)) {
      panState.active = true;
      panState.startMouse = event.position;
      panState.startViewOriginWorld = viewOriginWorld;
      marqueeState = {};
      return;
    }

    if (modifiersAllowResizeDrag(event.mods) &&
        isMultiSelectionResizeHandleHit(event.position)) {
      const auto &selection = document.editorState().selection;
      if (!selection.empty()) {
        marqueeState = {};
        beginDragForSelection(selection.front(), DragMode::resize,
                              event.position);
        return;
      }
    }

    marqueeState.active = true;
    marqueeState.additive = event.mods.isShiftDown();
    marqueeState.toggle = event.mods.isCommandDown();
    marqueeState.startMouse = event.position;
    marqueeState.bounds =
        makeNormalizedRect(marqueeState.startMouse, marqueeState.startMouse);

    if (!marqueeState.additive && !marqueeState.toggle) {
      activeGroupEditId.reset();
      document.clearSelection();
      syncSelectionToViews();
    }
  }

  void mouseDrag(const juce::MouseEvent &event) override {
    setMouseTrackerPoint(event.position);

    if (isRunMode() && !panState.active)
      return;

    if (guideDragState.active) {
      const auto previousPreviewInViewport = guideDragState.previewInViewport;
      const auto previousWorldPosition = guideDragState.worldPosition;
      guideDragState.previewInViewport = isPointInCanvasView(event.position);
      if (guideDragState.previewInViewport) {
        const auto worldPoint = viewToWorld(event.position);
        guideDragState.worldPosition =
            guideDragState.vertical ? worldPoint.x : worldPoint.y;
      }

      if (previousPreviewInViewport)
        repaintGuideOverlayForLine(guideDragState.vertical,
                                   previousWorldPosition);
      if (guideDragState.previewInViewport)
        repaintGuideOverlayForLine(guideDragState.vertical,
                                   guideDragState.worldPosition);
      return;
    }

    if (panState.active) {
      const auto previousOrigin = viewOriginWorld;
      const auto delta = event.position - panState.startMouse;
      viewOriginWorld =
          panState.startViewOriginWorld -
          juce::Point<float>(delta.x / zoomLevel, delta.y / zoomLevel);
      clampViewOriginToCanvas();
      updateAllWidgetViewBounds();
      repaint();
      if (!areClose(previousOrigin.x, viewOriginWorld.x) ||
          !areClose(previousOrigin.y, viewOriginWorld.y))
        notifyViewportChanged();
      return;
    }

    if (dragState.active) {
      handleWidgetMouseDrag(dragState.anchorWidgetId, event);
      return;
    }

    if (!marqueeState.active)
      return;

    const auto previousBounds = marqueeState.bounds;
    marqueeState.bounds =
        makeNormalizedRect(marqueeState.startMouse, event.position);
    auto dirty = unionRect(previousBounds, marqueeState.bounds);
    dirty = dirty.expanded(2.0f, 2.0f);
    repaint(dirty.getSmallestIntegerContainer());
  }

  void mouseUp(const juce::MouseEvent &event) override {
    setMouseTrackerPoint(event.position);

    if (isRunMode() && !panState.active)
      return;

    if (guideDragState.active) {
      const auto hadPreviewGuide = guideDragState.previewInViewport;
      const auto previewGuideVertical = guideDragState.vertical;
      const auto previewGuideWorldPosition = guideDragState.worldPosition;
      if (guideDragState.previewInViewport)
        guides.push_back(
            {guideDragState.vertical, guideDragState.worldPosition});
      guideDragState = {};
      if (hadPreviewGuide)
        repaintGuideOverlayForLine(previewGuideVertical,
                                   previewGuideWorldPosition);
      return;
    }

    if (panState.active) {
      panState = {};
      return;
    }

    if (dragState.active) {
      handleWidgetMouseUp(dragState.anchorWidgetId);
      return;
    }

    if (!marqueeState.active)
      return;

    marqueeState.bounds =
        makeNormalizedRect(marqueeState.startMouse, event.position);
    const auto marqueeDirty =
        marqueeState.bounds.expanded(2.0f, 2.0f).getSmallestIntegerContainer();
    applyMarqueeSelection();
    marqueeState = {};

    if (normalizeSelectionAfterAltReleasePending && !altPreviewEnabled) {
      normalizeSelectionAfterAltReleasePending = false;
      normalizeSelectionForCurrentModifierState();
    }

    repaint(marqueeDirty);
  }

  void mouseDoubleClick(const juce::MouseEvent &event) override {
    if (isRunMode())
      return;

    setMouseTrackerPoint(event.position);

    if (!event.mods.isLeftButtonDown())
      return;

    handleCanvasDoubleClick(event.position);
  }

  void mouseMove(const juce::MouseEvent &event) override {
    setMouseTrackerPoint(event.position);
  }

  void mouseExit(const juce::MouseEvent &) override {
    clearMouseTrackerPoint();
  }

  void mouseWheelMove(const juce::MouseEvent &event,
                      const juce::MouseWheelDetails &wheel) override {
    applyWheelZoomAtPoint(event.position, wheel);
  }

  bool keyStateChanged(bool isKeyDown) override {
    juce::ignoreUnused(isKeyDown);
    refreshAltPreviewState();
    return juce::Component::keyStateChanged(isKeyDown);
  }

  void modifierKeysChanged(const juce::ModifierKeys &) override {
    refreshAltPreviewState();
  }

  bool keyPressed(const juce::KeyPress &key) override {
    if (isRunMode())
      return false;

    const auto mods = key.getModifiers();
    const auto keyCode = key.getKeyCode();
    const auto isZ = keyCode == 'z' || keyCode == 'Z';
    const auto isY = keyCode == 'y' || keyCode == 'Y';
    const auto isG = keyCode == 'g' || keyCode == 'G';

    if (mods.isCommandDown() && isZ) {
      if (mods.isShiftDown())
        return performRedo();
      return performUndo();
    }

    if (mods.isCommandDown() && isY)
      return performRedo();

    if (mods.isCommandDown() && isG) {
      if (mods.isShiftDown())
        return ungroupSelection();
      return groupSelection();
    }

    if (!mods.isAnyModifierKeyDown() && keyCode == juce::KeyPress::returnKey)
      return enterGroupEditMode();

    if (!mods.isAnyModifierKeyDown() && keyCode == juce::KeyPress::escapeKey)
      return exitGroupEditMode(true);

    if (!mods.isCommandDown()) {
      const auto step = mods.isShiftDown() ? 10.0f : 1.0f;
      juce::Point<float> delta;

      if (keyCode == juce::KeyPress::leftKey)
        delta.x = -step;
      else if (keyCode == juce::KeyPress::rightKey)
        delta.x = step;
      else if (keyCode == juce::KeyPress::upKey)
        delta.y = -step;
      else if (keyCode == juce::KeyPress::downKey)
        delta.y = step;

      if (!areClose(delta.x, 0.0f) || !areClose(delta.y, 0.0f))
        return nudgeSelection(delta);
    }

    if (!mods.isAnyModifierKeyDown() &&
        (key.getKeyCode() == juce::KeyPress::deleteKey ||
         key.getKeyCode() == juce::KeyPress::backspaceKey)) {
      return deleteSelection();
    }

    return false;
  }

private:
  enum class DragMode { move, resize };

  struct DragItemState {
    WidgetId widgetId = 0;
    WidgetType widgetType = WidgetType::button;
    juce::Point<float> minSize{1.0f, 1.0f};
    juce::Rectangle<float> startBounds;
    juce::Rectangle<float> currentBounds;
  };

  struct DragState {
    bool active = false;
    WidgetId anchorWidgetId = 0;
    DragMode mode = DragMode::move;
    juce::Point<float> startMouse;
    juce::Rectangle<float> startSelectionBounds;
    float minScaleX = 0.0f;
    float minScaleY = 0.0f;
    std::vector<DragItemState> items;
  };

  struct MarqueeState {
    bool active = false;
    bool additive = false;
    bool toggle = false;
    juce::Point<float> startMouse;
    juce::Rectangle<float> bounds;
  };

  struct PanState {
    bool active = false;
    juce::Point<float> startMouse;
    juce::Point<float> startViewOriginWorld;
  };

  struct Guide {
    bool vertical = false;
    float worldPosition = 0.0f;
  };

  struct GuideDragState {
    bool active = false;
    bool vertical = false;
    bool previewInViewport = false;
    juce::Point<float> startMouse;
    float worldPosition = 0.0f;
  };

  struct AssetDragPayload {
    WidgetId assetId = kRootId;
    juce::String refKey;
    juce::String displayName;
    juce::String mime;
    AssetKind kind = AssetKind::file;
  };

  struct RuntimeInteractionState {
    bool active = false;
    WidgetId widgetId = kRootId;
    WidgetType widgetType = WidgetType::button;
    juce::Point<float> startMouse;
    double startValue = 0.0;
  };

  std::optional<juce::String>
  extractWidgetLibraryTypeKey(const juce::var &description) const {
    const auto *object = description.getDynamicObject();
    if (object == nullptr)
      return std::nullopt;

    const auto &props = object->getProperties();
    if (!props.contains("kind") || !props.contains("typeKey"))
      return std::nullopt;
    if (props["kind"].toString() != "widgetLibraryType")
      return std::nullopt;
    if (!props["typeKey"].isString())
      return std::nullopt;

    const auto typeKey = props["typeKey"].toString().trim();
    if (typeKey.isEmpty())
      return std::nullopt;
    return typeKey;
  }

  std::optional<AssetDragPayload>
  extractAssetDragPayload(const juce::var &description) const {
    const auto *object = description.getDynamicObject();
    if (object == nullptr)
      return std::nullopt;

    const auto &props = object->getProperties();
    if (!props.contains("kind") || props["kind"].toString() != "assetRef")
      return std::nullopt;

    if (!props.contains("refKey"))
      return std::nullopt;

    AssetDragPayload payload;
    payload.refKey = props["refKey"].toString().trim();
    if (payload.refKey.isEmpty())
      return std::nullopt;

    payload.displayName = props.getWithDefault("name", juce::var()).toString();
    payload.mime = props.getWithDefault("mime", juce::var()).toString();

    const auto kindKey =
        props.getWithDefault("assetKind", juce::var()).toString();
    if (const auto parsedKind = assetKindFromKey(kindKey);
        parsedKind.has_value())
      payload.kind = *parsedKind;

    const auto assetIdText =
        props.getWithDefault("assetId", juce::var()).toString().trim();
    if (const auto parsedId = widgetIdFromJsonString(assetIdText);
        parsedId.has_value())
      payload.assetId = *parsedId;

    return payload;
  }

  std::optional<WidgetId>
  hitTestWidgetIdAtViewPoint(juce::Point<float> viewPoint) {
    auto *component = getComponentAt(viewPoint.toInt());
    while (component != nullptr) {
      if (const auto *widgetComponent =
              dynamic_cast<WidgetComponent *>(component);
          widgetComponent != nullptr)
        return widgetComponent->widgetId();
      component = component->getParentComponent();
    }

    return std::nullopt;
  }

  std::optional<std::vector<Widgets::DropOption>>
  resolveAssetDropOptions(WidgetId widgetId,
                          const AssetDragPayload &payload) const {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return std::nullopt;

    const auto *descriptor = widgetFactory.descriptorFor(widget->type);
    if (descriptor == nullptr || !static_cast<bool>(descriptor->dropOptions))
      return std::nullopt;

    Widgets::AssetRef assetRef;
    assetRef.assetId = payload.refKey;
    assetRef.displayName = payload.displayName;
    assetRef.mime = payload.mime;

    auto options = descriptor->dropOptions(*widget, assetRef);
    options.erase(
        std::remove_if(options.begin(), options.end(),
                       [](const Widgets::DropOption &option) {
                         return option.propKey.toString().trim().isEmpty();
                       }),
        options.end());
    return options;
  }

  bool applyAssetDropToWidget(WidgetId widgetId,
                              const AssetDragPayload &payload,
                              const Widgets::DropOption &option) {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return false;

    const auto *descriptor = widgetFactory.descriptorFor(widget->type);
    if (descriptor == nullptr)
      return false;

    Widgets::AssetRef assetRef;
    assetRef.assetId = payload.refKey;
    assetRef.displayName = payload.displayName;
    assetRef.mime = payload.mime;

    PropertyBag patch;
    if (static_cast<bool>(descriptor->applyDrop)) {
      const auto result =
          descriptor->applyDrop(patch, *widget, assetRef, option);
      if (result.failed())
        return false;
    } else {
      if (option.propKey.toString().trim().isEmpty())
        return false;
      patch.set(option.propKey, payload.refKey);
    }

    if (patch.size() == 0)
      return false;

    SetPropsAction action;
    action.kind = NodeKind::widget;
    action.ids = {widgetId};
    WidgetPropsPatch widgetPatch;
    for (int i = 0; i < patch.size(); ++i)
      widgetPatch.patch.set(patch.getName(i), patch.getValueAt(i));
    action.patch = std::move(widgetPatch);
    return document.setProps(action);
  }

  void applyAssetDropWithSelection(WidgetId widgetId, AssetDragPayload payload,
                                   std::vector<Widgets::DropOption> options,
                                   juce::Point<int> localDropPoint) {
    if (options.empty())
      return;

    if (options.size() == 1) {
      if (!applyAssetDropToWidget(widgetId, payload, options.front()))
        return;

      document.selectSingle(widgetId);
      refreshFromDocument();
      grabKeyboardFocus();
      return;
    }

    juce::PopupMenu menu;
    for (size_t i = 0; i < options.size(); ++i) {
      const auto &option = options[i];
      auto label = option.label.trim();
      if (label.isEmpty())
        label = option.propKey.toString();

      menu.addItem(static_cast<int>(i + 1), label);
    }

    const auto screenPoint = localPointToGlobal(localDropPoint);
    const auto targetArea =
        juce::Rectangle<int>(screenPoint.x, screenPoint.y, 1, 1);
    const juce::Component::SafePointer<CanvasComponent> safeThis(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(targetArea),
        [safeThis, widgetId, payload = std::move(payload),
         options = std::move(options)](int result) mutable {
          if (safeThis == nullptr || result <= 0)
            return;

          const auto optionIndex = static_cast<size_t>(result - 1);
          if (optionIndex >= options.size())
            return;

          if (!safeThis->applyAssetDropToWidget(widgetId, payload,
                                                options[optionIndex]))
            return;

          safeThis->document.selectSingle(widgetId);
          safeThis->refreshFromDocument();
          safeThis->grabKeyboardFocus();
        });
  }

  void clearWidgetLibraryDropPreview() {
    if (!widgetLibraryDropPreviewActive)
      return;
    widgetLibraryDropPreviewActive = false;
    repaint();
  }

  void clearAssetDropPreview() {
    if (!assetDropPreviewActive)
      return;

    assetDropPreviewActive = false;
    assetDropPreviewWidgetId = kRootId;
    assetDropPreviewValid = false;
    assetDropPreviewRefKey.clear();
    repaint();
  }

  void updateAssetDropPreview(juce::Point<float> viewPoint,
                              std::optional<WidgetId> targetWidgetId,
                              bool valid, const juce::String &refKey) {
    const auto nextWidgetId = targetWidgetId.value_or(kRootId);
    const auto changed =
        !assetDropPreviewActive ||
        viewPoint.getDistanceFrom(assetDropPreviewView) > 0.5f ||
        assetDropPreviewWidgetId != nextWidgetId ||
        assetDropPreviewValid != valid || assetDropPreviewRefKey != refKey;

    if (!changed)
      return;

    assetDropPreviewActive = true;
    assetDropPreviewView = viewPoint;
    assetDropPreviewWidgetId = nextWidgetId;
    assetDropPreviewValid = valid;
    assetDropPreviewRefKey = refKey;
    repaint();
  }

  juce::Point<float> createDefaultOrigin() const noexcept {
    const auto index = static_cast<int>(document.snapshot().widgets.size());
    const auto x = 24.0f + static_cast<float>((index % 10) * 20);
    const auto y = 24.0f + static_cast<float>(((index / 10) % 6) * 20);
    return {x, y};
  }

  std::optional<WidgetId> resolveActiveLayerId() const {
    return activeLayerResolver != nullptr ? activeLayerResolver()
                                          : std::optional<WidgetId>{};
  }

  WidgetComponent *findWidgetView(WidgetId id) noexcept {
    const auto it =
        std::find_if(widgetViews.begin(), widgetViews.end(),
                     [id](const auto &view) { return view->widgetId() == id; });
    return it == widgetViews.end() ? nullptr : it->get();
  }

  const WidgetComponent *findWidgetView(WidgetId id) const noexcept {
    const auto it =
        std::find_if(widgetViews.begin(), widgetViews.end(),
                     [id](const auto &view) { return view->widgetId() == id; });
    return it == widgetViews.end() ? nullptr : it->get();
  }

  const WidgetModel *findWidgetModel(WidgetId id) const noexcept {
    const auto &widgets = document.snapshot().widgets;
    const auto it = std::find_if(
        widgets.begin(), widgets.end(),
        [id](const WidgetModel &widget) { return widget.id == id; });
    return it == widgets.end() ? nullptr : &(*it);
  }

  const GroupModel *findGroupById(WidgetId id) const noexcept {
    const auto &groups = document.snapshot().groups;
    const auto it =
        std::find_if(groups.begin(), groups.end(),
                     [id](const GroupModel &group) { return group.id == id; });
    return it == groups.end() ? nullptr : &(*it);
  }

  const GroupModel *findGroupByMember(WidgetId memberId) const noexcept {
    const auto &groups = document.snapshot().groups;
    const auto it = std::find_if(
        groups.begin(), groups.end(), [memberId](const GroupModel &group) {
          return containsWidgetId(group.memberWidgetIds, memberId);
        });
    return it == groups.end() ? nullptr : &(*it);
  }

  const LayerModel *findLayerById(WidgetId id) const noexcept {
    const auto &layers = document.snapshot().layers;
    const auto it =
        std::find_if(layers.begin(), layers.end(),
                     [id](const LayerModel &layer) { return layer.id == id; });
    return it == layers.end() ? nullptr : &(*it);
  }

  std::optional<WidgetId>
  directLayerForWidget(WidgetId widgetId) const noexcept {
    for (const auto &layer : document.snapshot().layers) {
      if (containsWidgetId(layer.memberWidgetIds, widgetId))
        return layer.id;
    }
    return std::nullopt;
  }

  std::optional<WidgetId> directLayerForGroup(WidgetId groupId) const noexcept {
    for (const auto &layer : document.snapshot().layers) {
      if (containsWidgetId(layer.memberGroupIds, groupId))
        return layer.id;
    }
    return std::nullopt;
  }

  std::optional<WidgetId>
  effectiveLayerForGroup(WidgetId groupId) const noexcept {
    if (const auto direct = directLayerForGroup(groupId); direct.has_value())
      return direct;

    auto *group = findGroupById(groupId);
    std::unordered_set<WidgetId> visited;
    while (group != nullptr && visited.insert(group->id).second) {
      if (const auto direct = directLayerForGroup(group->id);
          direct.has_value())
        return direct;
      if (!group->parentGroupId.has_value())
        break;
      group = findGroupById(*group->parentGroupId);
    }

    return std::nullopt;
  }

  std::optional<WidgetId>
  effectiveLayerForWidget(WidgetId widgetId) const noexcept {
    if (const auto direct = directLayerForWidget(widgetId); direct.has_value())
      return direct;

    if (const auto *owner = findGroupByMember(widgetId); owner != nullptr)
      return effectiveLayerForGroup(owner->id);

    return std::nullopt;
  }

  bool isGroupChainVisible(WidgetId groupId) const noexcept {
    const auto *group = findGroupById(groupId);
    std::unordered_set<WidgetId> visited;
    while (group != nullptr && visited.insert(group->id).second) {
      if (!group->visible)
        return false;
      if (!group->parentGroupId.has_value())
        break;
      group = findGroupById(*group->parentGroupId);
    }

    return true;
  }

  bool isGroupChainLocked(WidgetId groupId) const noexcept {
    const auto *group = findGroupById(groupId);
    std::unordered_set<WidgetId> visited;
    while (group != nullptr && visited.insert(group->id).second) {
      if (group->locked)
        return true;
      if (!group->parentGroupId.has_value())
        break;
      group = findGroupById(*group->parentGroupId);
    }

    return false;
  }

  float groupChainOpacity(WidgetId groupId) const noexcept {
    float opacity = 1.0f;
    const auto *group = findGroupById(groupId);
    std::unordered_set<WidgetId> visited;
    while (group != nullptr && visited.insert(group->id).second) {
      opacity *= juce::jlimit(0.0f, 1.0f, group->opacity);
      if (!group->parentGroupId.has_value())
        break;
      group = findGroupById(*group->parentGroupId);
    }

    return juce::jlimit(0.0f, 1.0f, opacity);
  }

  float effectiveOpacityForWidget(WidgetId widgetId) const noexcept {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return 1.0f;

    auto opacity = juce::jlimit(0.0f, 1.0f, widget->opacity);
    if (const auto *owner = findGroupByMember(widgetId); owner != nullptr)
      opacity *= groupChainOpacity(owner->id);

    return juce::jlimit(0.0f, 1.0f, opacity);
  }

  bool isWidgetEffectivelyVisible(WidgetId widgetId) const noexcept {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr || !widget->visible)
      return false;

    if (const auto *owner = findGroupByMember(widgetId); owner != nullptr) {
      if (!isGroupChainVisible(owner->id))
        return false;
    }

    if (const auto layerId = effectiveLayerForWidget(widgetId);
        layerId.has_value()) {
      if (const auto *layer = findLayerById(*layerId); layer != nullptr)
        return layer->visible;
    }

    return true;
  }

  bool isWidgetEffectivelyLocked(WidgetId widgetId) const noexcept {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return true;
    if (widget->locked)
      return true;

    if (const auto *owner = findGroupByMember(widgetId); owner != nullptr) {
      if (isGroupChainLocked(owner->id))
        return true;
    }

    if (const auto layerId = effectiveLayerForWidget(widgetId);
        layerId.has_value()) {
      if (const auto *layer = findLayerById(*layerId); layer != nullptr)
        return layer->locked;
    }

    return false;
  }

  std::vector<WidgetId> childGroupIds(WidgetId parentGroupId) const {
    std::vector<WidgetId> children;
    for (const auto &group : document.snapshot().groups) {
      if (group.parentGroupId.has_value() &&
          *group.parentGroupId == parentGroupId)
        children.push_back(group.id);
    }

    return children;
  }

  void collectGroupWidgetIdsRecursive(
      WidgetId groupId, std::vector<WidgetId> &outWidgetIds,
      std::unordered_set<WidgetId> &visitedGroupIds) const {
    if (!visitedGroupIds.insert(groupId).second)
      return;

    const auto *group = findGroupById(groupId);
    if (group == nullptr)
      return;

    for (const auto widgetId : group->memberWidgetIds) {
      if (!containsWidgetId(outWidgetIds, widgetId))
        outWidgetIds.push_back(widgetId);
    }

    for (const auto childGroupId : childGroupIds(groupId))
      collectGroupWidgetIdsRecursive(childGroupId, outWidgetIds,
                                     visitedGroupIds);
  }

  std::vector<WidgetId> collectGroupWidgetIdsRecursive(WidgetId groupId) const {
    std::vector<WidgetId> widgetIds;
    std::unordered_set<WidgetId> visitedGroupIds;
    collectGroupWidgetIdsRecursive(groupId, widgetIds, visitedGroupIds);
    return widgetIds;
  }

  bool isWidgetInGroup(WidgetId memberId, WidgetId groupId) const noexcept {
    const auto groupWidgets = collectGroupWidgetIdsRecursive(groupId);
    return containsWidgetId(groupWidgets, memberId);
  }

  bool selectionEqualsGroup(const std::vector<WidgetId> &selection,
                            const GroupModel &group) const noexcept {
    const auto groupWidgetIds = collectGroupWidgetIdsRecursive(group.id);
    if (selection.size() != groupWidgetIds.size())
      return false;

    auto selectionSorted = selection;
    auto groupSorted = groupWidgetIds;
    std::sort(selectionSorted.begin(), selectionSorted.end());
    std::sort(groupSorted.begin(), groupSorted.end());
    return selectionSorted == groupSorted;
  }

  std::vector<WidgetId> groupAncestryForWidget(WidgetId widgetId) const {
    std::vector<WidgetId> ancestry;
    const auto *group = findGroupByMember(widgetId);
    if (group == nullptr)
      return ancestry;

    std::unordered_set<WidgetId> visited;
    while (group != nullptr && visited.insert(group->id).second) {
      ancestry.push_back(group->id); // leaf -> ... -> top-level
      if (!group->parentGroupId.has_value())
        break;

      group = findGroupById(*group->parentGroupId);
    }

    return ancestry;
  }

  std::optional<WidgetId> topLevelGroupForWidget(WidgetId widgetId) const {
    const auto ancestry = groupAncestryForWidget(widgetId);
    if (ancestry.empty())
      return std::nullopt;

    return ancestry.back();
  }

  std::optional<WidgetId> altSelectableGroupForWidget(WidgetId widgetId) const {
    const auto ancestry = groupAncestryForWidget(widgetId);
    if (ancestry.empty())
      return std::nullopt;
    if (ancestry.size() == 1)
      return std::nullopt;

    // Alt preview exposes only one level below top-level.
    return ancestry[ancestry.size() - 2];
  }

  std::vector<WidgetId> altSelectionUnitForWidget(WidgetId id) const {
    if (activeGroupEditId.has_value() &&
        isWidgetInGroup(id, *activeGroupEditId))
      return {id};

    if (const auto groupId = altSelectableGroupForWidget(id);
        groupId.has_value())
      return collectGroupWidgetIdsRecursive(*groupId);

    // Widgets directly under a top-level group stay selectable as single
    // widgets in Alt mode.
    return {id};
  }

  bool modifiersAllowResizeDrag(const juce::ModifierKeys &mods) const noexcept {
    return !mods.isCommandDown() && !mods.isShiftDown();
  }

  void paintSingleGroupOverlay(juce::Graphics &g, const GroupModel &group,
                               const std::vector<WidgetId> &selection,
                               float alphaScale = 1.0f) const {
    const auto isActiveEdit =
        activeGroupEditId.has_value() && *activeGroupEditId == group.id;

    juce::Rectangle<float> bounds;
    if (!computeGroupBounds(group.id, bounds, true))
      return;

    const auto boundsInset = bounds.reduced(0.5f);
    const auto isWholeSelected = selectionEqualsGroup(selection, group);
    const auto overlayAlpha = juce::jlimit(0.0f, 1.0f, alphaScale);

    if (isActiveEdit) {
      g.setColour(juce::Colour::fromRGB(255, 196, 112)
                      .withAlpha(0.12f)
                      .withMultipliedAlpha(overlayAlpha));
      g.fillRoundedRectangle(boundsInset, 4.0f);
    }

    const auto outline =
        isActiveEdit
            ? juce::Colour::fromRGB(255, 196, 112)
            : (isWholeSelected ? juce::Colour::fromRGB(78, 156, 255)
                               : juce::Colour::fromRGBA(150, 190, 235, 145));
    g.setColour(outline.withMultipliedAlpha(overlayAlpha));
    drawDashedRect(g, boundsInset, isActiveEdit ? 6.0f : 4.0f, 3.0f,
                   isActiveEdit ? 1.6f : 1.1f);
  }

  std::optional<WidgetId> selectedWholeGroupId() const noexcept {
    const auto &selection = document.editorState().selection;
    if (selection.size() < 2)
      return std::nullopt;

    const auto &groups = document.snapshot().groups;
    for (const auto &group : groups) {
      if (selectionEqualsGroup(selection, group))
        return group.id;
    }

    return std::nullopt;
  }

  bool computeGroupBounds(WidgetId groupId, juce::Rectangle<float> &boundsOut,
                          bool useViewBounds) const noexcept {
    const auto widgetIds = collectGroupWidgetIdsRecursive(groupId);
    if (widgetIds.empty())
      return false;

    bool hasBounds = false;
    for (const auto memberId : widgetIds) {
      juce::Rectangle<float> memberBounds;
      bool hasMemberBounds = false;

      if (useViewBounds) {
        if (const auto *view = findWidgetView(memberId); view != nullptr) {
          memberBounds = view->getBounds().toFloat();
          hasMemberBounds = true;
        }
      }

      if (!hasMemberBounds) {
        if (const auto *widget = findWidgetModel(memberId); widget != nullptr) {
          memberBounds = widget->bounds;
          hasMemberBounds = true;
        }
      }

      if (!hasMemberBounds)
        continue;

      if (!hasBounds) {
        boundsOut = memberBounds;
        hasBounds = true;
      } else {
        boundsOut = unionRect(boundsOut, memberBounds);
      }
    }

    return hasBounds;
  }

  void paintGroupOverlays(juce::Graphics &g) const {
    const auto &groups = document.snapshot().groups;
    const auto &selection = document.editorState().selection;

    std::vector<const GroupModel *> topLevelGroups;
    topLevelGroups.reserve(groups.size());

    for (const auto &group : groups) {
      if (!group.parentGroupId.has_value()) {
        topLevelGroups.push_back(&group);
        paintSingleGroupOverlay(g, group, selection,
                                altPreviewEnabled ? 0.55f : 1.0f);
      }
    }

    if (!altPreviewEnabled)
      return;

    for (const auto *topLevelGroup : topLevelGroups) {
      if (topLevelGroup == nullptr)
        continue;

      for (const auto childGroupId : childGroupIds(topLevelGroup->id)) {
        if (const auto *childGroup = findGroupById(childGroupId);
            childGroup != nullptr)
          paintSingleGroupOverlay(g, *childGroup, selection);
      }
    }
  }

  void addUniqueSelectionIds(std::vector<WidgetId> &target,
                             const std::vector<WidgetId> &ids) const {
    for (const auto id : ids) {
      if (!containsWidgetId(target, id))
        target.push_back(id);
    }
  }

  bool selectionSetsEqual(const std::vector<WidgetId> &lhs,
                          const std::vector<WidgetId> &rhs) const {
    if (lhs.size() != rhs.size())
      return false;

    auto lhsSorted = lhs;
    auto rhsSorted = rhs;
    std::sort(lhsSorted.begin(), lhsSorted.end());
    std::sort(rhsSorted.begin(), rhsSorted.end());
    return lhsSorted == rhsSorted;
  }

  void normalizeSelectionForCurrentModifierState() {
    if (altPreviewEnabled || activeGroupEditId.has_value())
      return;

    const auto &currentSelection = document.editorState().selection;
    if (currentSelection.empty())
      return;

    auto normalizedSelection = expandToSelectionUnits(currentSelection);
    if (selectionSetsEqual(currentSelection, normalizedSelection))
      return;

    document.setSelection(std::move(normalizedSelection));
    syncSelectionToViews();
  }

  void removeSelectionIds(std::vector<WidgetId> &target,
                          const std::vector<WidgetId> &ids) const {
    target.erase(std::remove_if(target.begin(), target.end(),
                                [&ids](WidgetId selectedId) {
                                  return containsWidgetId(ids, selectedId);
                                }),
                 target.end());
  }

  std::vector<WidgetId> selectionUnitForWidget(WidgetId id) const {
    if (activeGroupEditId.has_value() &&
        isWidgetInGroup(id, *activeGroupEditId))
      return {id};

    if (const auto groupId = topLevelGroupForWidget(id); groupId.has_value())
      return collectGroupWidgetIdsRecursive(*groupId);

    return {id};
  }

  std::vector<WidgetId>
  expandToSelectionUnits(const std::vector<WidgetId> &ids) const {
    std::vector<WidgetId> expanded;
    expanded.reserve(ids.size());
    for (const auto id : ids) {
      const auto unit = selectionUnitForWidget(id);
      addUniqueSelectionIds(expanded, unit);
    }

    return expanded;
  }

  std::vector<WidgetId> orderedWidgetIdsForCanvas() const {
    const auto &snapshot = document.snapshot();
    std::vector<WidgetId> orderedIds;
    orderedIds.reserve(snapshot.widgets.size());

    if (snapshot.widgets.empty())
      return orderedIds;

    if (snapshot.layers.empty()) {
      for (const auto &widget : snapshot.widgets)
        orderedIds.push_back(widget.id);
      return orderedIds;
    }

    std::vector<const LayerModel *> layers;
    layers.reserve(snapshot.layers.size());
    for (const auto &layer : snapshot.layers)
      layers.push_back(&layer);

    std::sort(layers.begin(), layers.end(),
              [](const LayerModel *lhs, const LayerModel *rhs) {
                if (lhs == nullptr || rhs == nullptr)
                  return lhs != nullptr;

                if (lhs->order != rhs->order)
                  return lhs->order < rhs->order; // back -> front
                return lhs->id < rhs->id;
              });

    std::unordered_set<WidgetId> emittedIds;
    emittedIds.reserve(snapshot.widgets.size());

    for (const auto *layer : layers) {
      if (layer == nullptr)
        continue;

      std::unordered_set<WidgetId> layerWidgetIds;
      layerWidgetIds.reserve(layer->memberWidgetIds.size() +
                             layer->memberGroupIds.size() * 2);

      for (const auto widgetId : layer->memberWidgetIds)
        layerWidgetIds.insert(widgetId);

      for (const auto groupId : layer->memberGroupIds) {
        const auto groupWidgetIds = collectGroupWidgetIdsRecursive(groupId);
        for (const auto widgetId : groupWidgetIds)
          layerWidgetIds.insert(widgetId);
      }

      // Keep stable depth inside one layer by existing document z-order.
      for (const auto &widget : snapshot.widgets) {
        if (layerWidgetIds.count(widget.id) == 0)
          continue;
        if (!emittedIds.insert(widget.id).second)
          continue;

        orderedIds.push_back(widget.id);
      }
    }

    // Compatibility fallback for documents that still keep root-level widgets.
    for (const auto &widget : snapshot.widgets) {
      if (!emittedIds.insert(widget.id).second)
        continue;
      orderedIds.push_back(widget.id);
    }

    return orderedIds;
  }

  bool computeCurrentSelectionUnionBounds(
      juce::Rectangle<float> &boundsOut) const noexcept {
    const auto &selection = document.editorState().selection;
    if (selection.size() <= 1)
      return false;

    bool hasBounds = false;
    for (const auto id : selection) {
      const auto *view = findWidgetView(id);
      if (view == nullptr)
        continue;

      const auto viewBounds = view->getBounds().toFloat();
      if (!hasBounds) {
        boundsOut = viewBounds;
        hasBounds = true;
      } else {
        boundsOut = unionRect(boundsOut, viewBounds);
      }
    }

    return hasBounds;
  }

  juce::Rectangle<float> selectionResizeHandleBounds(
      const juce::Rectangle<float> &selectionBounds) const noexcept {
    const auto handleSize =
        std::min({kResizeHandleSize, selectionBounds.getWidth(),
                  selectionBounds.getHeight()});
    return {selectionBounds.getRight() - handleSize - 1.0f,
            selectionBounds.getBottom() - handleSize - 1.0f, handleSize,
            handleSize};
  }

  bool isMultiSelectionResizeHandleHit(
      juce::Point<float> canvasPoint) const noexcept {
    juce::Rectangle<float> selectionBounds;
    if (!computeCurrentSelectionUnionBounds(selectionBounds))
      return false;

    return selectionResizeHandleBounds(selectionBounds).contains(canvasPoint);
  }

  static double readFiniteNumericProperty(const PropertyBag &props,
                                          const juce::Identifier &key,
                                          double fallback) noexcept {
    const auto raw = props.getWithDefault(key, juce::var(fallback));
    if (!raw.isInt() && !raw.isInt64() && !raw.isDouble())
      return fallback;

    const auto value = static_cast<double>(raw);
    return std::isfinite(value) ? value : fallback;
  }

  bool runtimeApplySetProps(const SetPropsAction &action) {
    if (isRunMode() || (interactionModeValue == InteractionMode::preview &&
                        previewBindingSimulationEnabled))
      return document.previewSetProps(action);
    return document.setProps(action);
  }

  bool runtimeApplySetBounds(const SetBoundsAction &action) {
    if (isRunMode() || (interactionModeValue == InteractionMode::preview &&
                        previewBindingSimulationEnabled))
      return document.previewSetBounds(action);
    return document.setBounds(action);
  }

  bool runtimeSetWidgetProperty(WidgetId widgetId, const juce::Identifier &key,
                                const juce::var &value) {
    if (widgetId <= kRootId)
      return false;

    SetPropsAction action;
    action.kind = NodeKind::widget;
    action.ids = {widgetId};
    WidgetPropsPatch patch;
    patch.patch.set(key, value);
    action.patch = std::move(patch);
    return runtimeApplySetProps(action);
  }

  void seedRuntimeParamsFromDocument() {
    const auto &snapshot = document.snapshot();
    for (const auto &param : snapshot.runtimeParams) {
      const auto result =
          runtimeParamBridge.set(param.key, param.defaultValue, {});
      if (!result.wasOk())
        emitRuntimeLog("Run Param", "seed failed key=" + param.key.trim() +
                                        " message=" + result.message);
    }
  }

  bool applyRuntimePropertyBindings() {
    if (!(isRunMode() || (interactionModeValue == InteractionMode::preview &&
                          previewBindingSimulationEnabled)))
      return false;

    const auto &snapshot = document.snapshot();
    if (snapshot.propertyBindings.empty())
      return false;

    bool changed = false;
    int failureCount = 0;
    juce::String firstFailure;
    const auto isIdentifierLike = [](const juce::String &text) noexcept {
      const auto trimmed = text.trim();
      if (trimmed.isEmpty())
        return false;

      auto isStart = [](juce::juce_wchar ch) noexcept {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
               ch == '_';
      };

      auto isBody = [isStart](juce::juce_wchar ch) noexcept {
        return isStart(ch) || (ch >= '0' && ch <= '9') || ch == '.';
      };

      if (!isStart(trimmed[0]))
        return false;

      for (int i = 1; i < trimmed.length(); ++i) {
        if (!isBody(trimmed[i]))
          return false;
      }

      return true;
    };

    for (const auto &binding : snapshot.propertyBindings) {
      if (!binding.enabled)
        continue;

      const auto propertyKeyText = binding.targetProperty.trim();
      if (propertyKeyText.isEmpty())
        continue;

      if (!isIdentifierLike(propertyKeyText)) {
        ++failureCount;
        if (firstFailure.isEmpty())
          firstFailure =
              "binding#" + juce::String(binding.id) + " invalid targetProperty";
        continue;
      }

      const auto *targetWidget = findWidgetModel(binding.targetWidgetId);
      if (targetWidget == nullptr) {
        ++failureCount;
        if (firstFailure.isEmpty())
          firstFailure =
              "binding#" + juce::String(binding.id) + " missing target widget";
        continue;
      }

      const auto expression = binding.expression.trim();
      const auto &params = runtimeParamBridge.values();
      juce::var finalValue;

      if (const auto it = params.find(expression); it != params.end()) {
        finalValue = it->second;
      } else {
        const auto evaluation =
            Runtime::PropertyBindingResolver::evaluateExpression(expression,
                                                                 params);
        if (!evaluation.success) {
          ++failureCount;
          if (firstFailure.isEmpty()) {
            firstFailure = "binding#" + juce::String(binding.id) +
                           " eval failed: " + evaluation.error;
          }
          continue;
        }
        finalValue = juce::var(evaluation.value);
      }

      changed = runtimeSetWidgetProperty(binding.targetWidgetId,
                                         juce::Identifier(propertyKeyText),
                                         finalValue) ||
                changed;
    }

    if (failureCount > 0) {
      const auto digest =
          "failed=" + juce::String(failureCount) + " first=" + firstFailure;
      if (digest != lastRuntimeBindingFailureDigest) {
        emitRuntimeLog("Run Binding", digest);
        lastRuntimeBindingFailureDigest = digest;
      }
    } else {
      lastRuntimeBindingFailureDigest.clear();
    }

    return changed;
  }

  bool runtimeSetSliderValueFromPointer(WidgetId widgetId,
                                        juce::Point<float> localPoint,
                                        double *valueOut = nullptr) {
    const auto *widget = findWidgetModel(widgetId);
    const auto *view = findWidgetView(widgetId);
    if (widget == nullptr || view == nullptr)
      return false;

    auto rangeMin =
        readFiniteNumericProperty(widget->properties, "slider.rangeMin", 0.0);
    auto rangeMax =
        readFiniteNumericProperty(widget->properties, "slider.rangeMax", 1.0);
    if (rangeMax <= rangeMin)
      rangeMax = rangeMin + 1.0;

    auto step =
        readFiniteNumericProperty(widget->properties, "slider.step", 0.0);
    if (step < 0.0 || !std::isfinite(step))
      step = 0.0;

    const auto style =
        widget->properties
            .getWithDefault("slider.style", juce::var("linearHorizontal"))
            .toString()
            .trim()
            .toLowerCase();
    const auto vertical = style.contains("vertical");

    const auto localBounds = view->getLocalBounds().toFloat();
    if (localBounds.getWidth() <= 0.0f || localBounds.getHeight() <= 0.0f)
      return false;

    const auto xNorm = juce::jlimit(
        0.0, 1.0, static_cast<double>(localPoint.x / localBounds.getWidth()));
    const auto yNorm = juce::jlimit(
        0.0, 1.0, static_cast<double>(localPoint.y / localBounds.getHeight()));
    auto normalized = vertical ? (1.0 - yNorm) : xNorm;

    auto nextValue = rangeMin + (rangeMax - rangeMin) * normalized;
    if (step > 0.0)
      nextValue = rangeMin + std::round((nextValue - rangeMin) / step) * step;
    nextValue = juce::jlimit(rangeMin, rangeMax, nextValue);

    if (valueOut != nullptr)
      *valueOut = nextValue;

    const auto currentValue = readFiniteNumericProperty(
        widget->properties, "value", rangeMin + (rangeMax - rangeMin) * 0.5);
    if (std::abs(currentValue - nextValue) <= 0.000001)
      return false;

    return runtimeSetWidgetProperty(widgetId, "value", juce::var(nextValue));
  }

  double runtimeCurrentSliderValue(WidgetId widgetId) const noexcept {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return 0.0;

    auto rangeMin =
        readFiniteNumericProperty(widget->properties, "slider.rangeMin", 0.0);
    auto rangeMax =
        readFiniteNumericProperty(widget->properties, "slider.rangeMax", 1.0);
    if (rangeMax <= rangeMin)
      rangeMax = rangeMin + 1.0;
    return juce::jlimit(
        rangeMin, rangeMax,
        readFiniteNumericProperty(widget->properties, "value",
                                  rangeMin + (rangeMax - rangeMin) * 0.5));
  }

  double runtimeCurrentKnobValue(WidgetId widgetId) const noexcept {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return 0.0;

    auto rangeMin =
        readFiniteNumericProperty(widget->properties, "knob.rangeMin", 0.0);
    auto rangeMax =
        readFiniteNumericProperty(widget->properties, "knob.rangeMax", 1.0);
    if (rangeMax <= rangeMin)
      rangeMax = rangeMin + 1.0;
    return juce::jlimit(
        rangeMin, rangeMax,
        readFiniteNumericProperty(widget->properties, "value",
                                  rangeMin + (rangeMax - rangeMin) * 0.5));
  }

  bool runtimeSetKnobValueFromDrag(WidgetId widgetId,
                                   juce::Point<float> startMouse,
                                   juce::Point<float> currentMouse,
                                   double startValue,
                                   double *valueOut = nullptr) {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return false;

    auto rangeMin =
        readFiniteNumericProperty(widget->properties, "knob.rangeMin", 0.0);
    auto rangeMax =
        readFiniteNumericProperty(widget->properties, "knob.rangeMax", 1.0);
    if (rangeMax <= rangeMin)
      rangeMax = rangeMin + 1.0;

    auto step = readFiniteNumericProperty(widget->properties, "knob.step", 0.0);
    if (step < 0.0 || !std::isfinite(step))
      step = 0.0;

    const auto deltaY = static_cast<double>(startMouse.y - currentMouse.y);
    const auto range = rangeMax - rangeMin;
    auto nextValue = startValue + (range * deltaY / 140.0);
    if (step > 0.0)
      nextValue = rangeMin + std::round((nextValue - rangeMin) / step) * step;
    nextValue = juce::jlimit(rangeMin, rangeMax, nextValue);

    if (valueOut != nullptr)
      *valueOut = nextValue;

    if (std::abs(runtimeCurrentKnobValue(widgetId) - nextValue) <= 0.000001)
      return false;

    return runtimeSetWidgetProperty(widgetId, "value", juce::var(nextValue));
  }

  static int comboItemCountFromText(const juce::String &source) {
    juce::StringArray items;
    items.addLines(source);
    for (int i = items.size() - 1; i >= 0; --i) {
      const auto trimmed = items[i].trim();
      if (trimmed.isEmpty())
        items.remove(i);
      else
        items.set(i, trimmed);
    }
    return items.size();
  }

  bool runtimeAdvanceComboSelection(WidgetId widgetId,
                                    int *selectedIndexOut = nullptr) {
    const auto *widget = findWidgetModel(widgetId);
    if (widget == nullptr)
      return false;

    const auto count = comboItemCountFromText(
        widget->properties
            .getWithDefault("combo.items", juce::var("Item 1\nItem 2\nItem 3"))
            .toString());
    if (count <= 0)
      return false;

    auto selectedIndex = static_cast<int>(
        widget->properties.getWithDefault("combo.selectedIndex", juce::var(1)));
    if (selectedIndex < 1 || selectedIndex > count)
      selectedIndex = 1;
    else
      selectedIndex = (selectedIndex % count) + 1;

    if (selectedIndexOut != nullptr)
      *selectedIndexOut = selectedIndex;

    return runtimeSetWidgetProperty(widgetId, "combo.selectedIndex",
                                    selectedIndex);
  }

  bool dispatchRuntimeEvent(WidgetId sourceWidgetId,
                            const juce::String &eventKey,
                            const juce::var &payload = {}) {
    Runtime::RuntimeActionExecutor::Context context;
    context.paramBridge = &runtimeParamBridge;
    context.applySetProps = [this](const SetPropsAction &action) {
      return runtimeApplySetProps(action);
    };
    context.applySetBounds = [this](const SetBoundsAction &action) {
      return runtimeApplySetBounds(action);
    };
    if (const auto *messageManager =
            juce::MessageManager::getInstanceWithoutCreating();
        messageManager != nullptr && messageManager->isThisTheMessageThread()) {
      context.dispatchThread = Runtime::RuntimeDispatchThread::uiMessage;
    } else {
      // Default non-message dispatch path to realtime-safe policy (UI mutation
      // blocked).
      context.dispatchThread = Runtime::RuntimeDispatchThread::realtimeAudio;
    }

    const auto previousParamRevision = runtimeParamBridge.revision();
    const auto report = runtimeBindingEngine.dispatchEvent(
        document.snapshot(), sourceWidgetId, eventKey, payload,
        runtimeActionExecutor, context);

    if (runtimeDiagnostics.shouldLogEvent(report))
      emitRuntimeLog("Run Event",
                     runtimeDiagnostics.formatEventSummary(report, payload));

    auto documentChanged = report.documentChanged;
    if (runtimeParamBridge.revision() != previousParamRevision)
      documentChanged = applyRuntimePropertyBindings() || documentChanged;

    return documentChanged;
  }

  bool isEventInsideWidgetBounds(WidgetId widgetId,
                                 const juce::MouseEvent *event) const {
    if (event == nullptr)
      return false;

    const auto *view = findWidgetView(widgetId);
    if (view == nullptr)
      return false;

    const auto ownerPoint =
        event->getEventRelativeTo(const_cast<CanvasComponent *>(this)).position;
    return view->getBounds().toFloat().contains(ownerPoint);
  }

  void handleWidgetRuntimeMouseDown(WidgetId id,
                                    const juce::MouseEvent &event) {
    runtimeInteraction = {};

    const auto *widget = findWidgetModel(id);
    if (widget == nullptr)
      return;
    if (!isWidgetEffectivelyVisible(id) || isWidgetEffectivelyLocked(id))
      return;

    runtimeInteraction.active = true;
    runtimeInteraction.widgetId = id;
    runtimeInteraction.widgetType = widget->type;
    runtimeInteraction.startMouse = event.position;
    runtimeInteraction.startValue = 0.0;

    auto changed = false;
    if (widget->type == WidgetType::button) {
      changed = dispatchRuntimeEvent(id, "onPress", true) || changed;
    } else if (widget->type == WidgetType::slider) {
      double value = runtimeCurrentSliderValue(id);
      changed = runtimeSetSliderValueFromPointer(id, event.position, &value) ||
                changed;
      changed = dispatchRuntimeEvent(id, "onValueChanged", value) || changed;
    } else if (widget->type == WidgetType::knob) {
      runtimeInteraction.startValue = runtimeCurrentKnobValue(id);
    }

    if (changed)
      refreshFromDocument();
  }

  void handleWidgetRuntimeMouseDrag(WidgetId id,
                                    const juce::MouseEvent &event) {
    if (!runtimeInteraction.active || runtimeInteraction.widgetId != id)
      return;

    auto changed = false;
    if (runtimeInteraction.widgetType == WidgetType::slider) {
      double value = runtimeCurrentSliderValue(id);
      changed = runtimeSetSliderValueFromPointer(id, event.position, &value) ||
                changed;
      changed = dispatchRuntimeEvent(id, "onValueChanged", value) || changed;
    } else if (runtimeInteraction.widgetType == WidgetType::knob) {
      double value = runtimeCurrentKnobValue(id);
      changed = runtimeSetKnobValueFromDrag(
                    id, runtimeInteraction.startMouse, event.position,
                    runtimeInteraction.startValue, &value) ||
                changed;
      changed = dispatchRuntimeEvent(id, "onValueChanged", value) || changed;
    }

    if (changed)
      refreshFromDocument();
  }

  void handleWidgetRuntimeMouseUp(WidgetId id, const juce::MouseEvent *event) {
    if (!runtimeInteraction.active || runtimeInteraction.widgetId != id)
      return;

    const auto releasedInside = isEventInsideWidgetBounds(id, event);
    auto changed = false;

    if (runtimeInteraction.widgetType == WidgetType::button) {
      changed =
          dispatchRuntimeEvent(id, "onRelease", releasedInside) || changed;
      if (releasedInside)
        changed = dispatchRuntimeEvent(id, "onClick", true) || changed;
    } else if (runtimeInteraction.widgetType == WidgetType::toggle) {
      if (releasedInside) {
        const auto *widget = findWidgetModel(id);
        const auto currentState =
            widget != nullptr
                ? static_cast<bool>(widget->properties.getWithDefault(
                      "state", juce::var(false)))
                : false;
        const auto nextState = !currentState;
        changed = runtimeSetWidgetProperty(id, "state", nextState) || changed;
        changed = dispatchRuntimeEvent(id, "onClick", nextState) || changed;
        changed =
            dispatchRuntimeEvent(id, "onToggleChanged", nextState) || changed;
      }
    } else if (runtimeInteraction.widgetType == WidgetType::slider) {
      double value = runtimeCurrentSliderValue(id);
      if (event != nullptr)
        changed =
            runtimeSetSliderValueFromPointer(id, event->position, &value) ||
            changed;
      changed = dispatchRuntimeEvent(id, "onValueCommit", value) || changed;
    } else if (runtimeInteraction.widgetType == WidgetType::knob) {
      double value = runtimeCurrentKnobValue(id);
      if (event != nullptr)
        changed = runtimeSetKnobValueFromDrag(
                      id, runtimeInteraction.startMouse, event->position,
                      runtimeInteraction.startValue, &value) ||
                  changed;
      changed = dispatchRuntimeEvent(id, "onValueCommit", value) || changed;
    } else if (runtimeInteraction.widgetType == WidgetType::comboBox) {
      if (releasedInside) {
        int selectedIndex = 0;
        changed = runtimeAdvanceComboSelection(id, &selectedIndex) || changed;
        changed =
            dispatchRuntimeEvent(id, "onSelectionChanged", selectedIndex) ||
            changed;
      }
    }

    runtimeInteraction = {};

    if (changed)
      refreshFromDocument();
  }

  bool beginDragForSelection(WidgetId anchorId, DragMode mode,
                             juce::Point<float> startMouse) {
    auto dragIds = document.editorState().selection;
    if (dragIds.empty() && anchorId > kRootId)
      dragIds.push_back(anchorId);

    DragState nextDrag;
    nextDrag.active = true;
    nextDrag.anchorWidgetId = anchorId;
    nextDrag.mode = mode;
    nextDrag.startMouse = startMouse;
    nextDrag.items.reserve(dragIds.size());

    for (const auto dragId : dragIds) {
      const auto *widget = findWidgetModel(dragId);
      if (widget == nullptr)
        continue;
      if (!isWidgetEffectivelyVisible(dragId) ||
          isWidgetEffectivelyLocked(dragId))
        continue;

      DragItemState item;
      item.widgetId = dragId;
      item.widgetType = widget->type;
      item.minSize = widgetFactory.minSizeFor(widget->type);
      item.startBounds = widget->bounds;
      item.currentBounds = widget->bounds;
      nextDrag.items.push_back(item);
    }

    if (nextDrag.items.empty()) {
      dragState = {};
      return false;
    }

    nextDrag.startSelectionBounds = nextDrag.items.front().startBounds;
    for (size_t i = 1; i < nextDrag.items.size(); ++i)
      nextDrag.startSelectionBounds = unionRect(nextDrag.startSelectionBounds,
                                                nextDrag.items[i].startBounds);

    constexpr float kScaleEpsilon = 0.0001f;
    if (nextDrag.startSelectionBounds.getWidth() > kScaleEpsilon) {
      float minScaleX = 0.0f;
      for (const auto &item : nextDrag.items) {
        if (item.startBounds.getWidth() > kScaleEpsilon)
          minScaleX =
              std::max(minScaleX, item.minSize.x / item.startBounds.getWidth());
      }

      nextDrag.minScaleX = minScaleX;
    }

    if (nextDrag.startSelectionBounds.getHeight() > kScaleEpsilon) {
      float minScaleY = 0.0f;
      for (const auto &item : nextDrag.items) {
        if (item.startBounds.getHeight() > kScaleEpsilon)
          minScaleY = std::max(minScaleY,
                               item.minSize.y / item.startBounds.getHeight());
      }

      nextDrag.minScaleY = minScaleY;
    }

    dragState = std::move(nextDrag);
    return true;
  }

  void handleWidgetMouseDown(WidgetId id, bool resizeHit,
                             const juce::MouseEvent &event) {
    grabKeyboardFocus();
    refreshAltPreviewState();
    hasMouseLocalPoint = true;
    lastMouseLocalPoint = event.getEventRelativeTo(this).position;
    clearTransientSnapGuides();

    if (!event.mods.isLeftButtonDown())
      return;

    if (isRunMode()) {
      handleWidgetRuntimeMouseDown(id, event);
      return;
    }

    if (!isWidgetEffectivelyVisible(id) || isWidgetEffectivelyLocked(id))
      return;

    marqueeState = {};
    const auto canvasPos = event.getEventRelativeTo(this).position;

    if (activeGroupEditId.has_value() &&
        !isWidgetInGroup(id, *activeGroupEditId))
      activeGroupEditId.reset();

    if (modifiersAllowResizeDrag(event.mods) &&
        isMultiSelectionResizeHandleHit(canvasPos)) {
      beginDragForSelection(id, DragMode::resize, canvasPos);
      return;
    }

    const auto selectionUnit = event.mods.isAltDown()
                                   ? altSelectionUnitForWidget(id)
                                   : selectionUnitForWidget(id);

    if (event.mods.isCommandDown()) {
      auto selection = document.editorState().selection;

      const auto unitFullySelected =
          std::all_of(selectionUnit.begin(), selectionUnit.end(),
                      [&selection](WidgetId selectionId) {
                        return containsWidgetId(selection, selectionId);
                      });

      if (unitFullySelected)
        removeSelectionIds(selection, selectionUnit);
      else
        addUniqueSelectionIds(selection, selectionUnit);

      document.setSelection(std::move(selection));
      syncSelectionToViews();
      dragState = {};
      return;
    }

    if (event.mods.isShiftDown()) {
      auto selection = document.editorState().selection;
      addUniqueSelectionIds(selection, selectionUnit);

      document.setSelection(std::move(selection));
      syncSelectionToViews();
      dragState = {};
      return;
    }

    const auto &currentSelection = document.editorState().selection;
    const auto unitFullySelected =
        std::all_of(selectionUnit.begin(), selectionUnit.end(),
                    [&currentSelection](WidgetId selectionId) {
                      return containsWidgetId(currentSelection, selectionId);
                    });
    const auto altSelectionMatches =
        selectionSetsEqual(currentSelection, selectionUnit);

    const auto shouldUpdateSelection =
        event.mods.isAltDown() ? !altSelectionMatches : !unitFullySelected;
    if (shouldUpdateSelection) {
      document.setSelection(selectionUnit);
      syncSelectionToViews();
    }

    if (event.getNumberOfClicks() >= 2) {
      if (!activeGroupEditId.has_value()) {
        if (const auto selectedGroupId = selectedWholeGroupId();
            selectedGroupId.has_value()) {
          activeGroupEditId = *selectedGroupId;
          document.selectSingle(id);
          syncSelectionToViews();
          dragState = {};
          return;
        }
      }
    }

    const auto useResize =
        resizeHit || (modifiersAllowResizeDrag(event.mods) &&
                      isMultiSelectionResizeHandleHit(canvasPos));
    beginDragForSelection(id, useResize ? DragMode::resize : DragMode::move,
                          canvasPos);
  }

  void handleWidgetMouseDrag(WidgetId id, const juce::MouseEvent &event) {
    if (isRunMode()) {
      handleWidgetRuntimeMouseDrag(id, event);
      return;
    }

    if (!dragState.active || dragState.anchorWidgetId != id)
      return;

    perf.dragPreviewUpdateCount += 1;
    const auto canvasPos = event.getEventRelativeTo(this).position;
    hasMouseLocalPoint = true;
    lastMouseLocalPoint = canvasPos;
    const auto deltaPixels = canvasPos - dragState.startMouse;
    const auto delta = juce::Point<float>(deltaPixels.x / zoomLevel,
                                          deltaPixels.y / zoomLevel);
    std::vector<WidgetId> draggedIds;
    draggedIds.reserve(dragState.items.size());
    for (const auto &item : dragState.items)
      draggedIds.push_back(item.widgetId);

    const auto previousTransientGuides = transientSnapGuides;
    const auto previousTransientSpacingHints = transientSmartSpacingHints;
    auto moveDelta = delta;
    bool changed = false;

    constexpr float kScaleEpsilon = 0.0001f;
    const auto baseSelection = dragState.startSelectionBounds;
    const auto baseW = baseSelection.getWidth();
    const auto baseH = baseSelection.getHeight();

    float nextSelectionW = baseW + delta.x;
    float nextSelectionH = baseH + delta.y;
    nextSelectionW = std::max(0.0f, nextSelectionW);
    nextSelectionH = std::max(0.0f, nextSelectionH);

    if (baseW > kScaleEpsilon)
      nextSelectionW = std::max(nextSelectionW, baseW * dragState.minScaleX);
    if (baseH > kScaleEpsilon)
      nextSelectionH = std::max(nextSelectionH, baseH * dragState.minScaleY);

    if (!snapSettings.snapEnabled) {
      clearTransientSnapGuides();
    } else if (dragState.mode == DragMode::move) {
      const auto proposedSelection = baseSelection.translated(delta.x, delta.y);
      const auto snapResult =
          snapEngine.compute(makeSnapRequest(proposedSelection, draggedIds));
      moveDelta.x = snapResult.snappedBounds.getX() - baseSelection.getX();
      moveDelta.y = snapResult.snappedBounds.getY() - baseSelection.getY();
      updateTransientSnapGuides(snapResult);
    } else {
      std::optional<float> snappedGuideX;
      std::optional<float> snappedGuideY;
      bool snappedRightFromSmart = false;
      bool snappedBottomFromSmart = false;
      auto snappedRight = baseSelection.getX() + nextSelectionW;
      auto snappedBottom = baseSelection.getY() + nextSelectionH;
      auto bestRightDelta = std::numeric_limits<float>::max();
      auto bestBottomDelta = std::numeric_limits<float>::max();
      const auto tolerance = std::max(0.0f, snapSettings.tolerance);

      auto considerRightGuide = [&](float guideX, bool fromSmart) {
        const auto proposedRight = baseSelection.getX() + nextSelectionW;
        const auto deltaToGuide = std::abs(proposedRight - guideX);
        if (deltaToGuide > tolerance || deltaToGuide >= bestRightDelta)
          return;

        bestRightDelta = deltaToGuide;
        snappedRight = guideX;
        snappedRightFromSmart = fromSmart;
        snappedGuideX =
            fromSmart ? std::optional<float>(guideX) : std::optional<float>{};
      };

      auto considerBottomGuide = [&](float guideY, bool fromSmart) {
        const auto proposedBottom = baseSelection.getY() + nextSelectionH;
        const auto deltaToGuide = std::abs(proposedBottom - guideY);
        if (deltaToGuide > tolerance || deltaToGuide >= bestBottomDelta)
          return;

        bestBottomDelta = deltaToGuide;
        snappedBottom = guideY;
        snappedBottomFromSmart = fromSmart;
        snappedGuideY =
            fromSmart ? std::optional<float>(guideY) : std::optional<float>{};
      };

      if (snapSettings.enableGridSnap && snapSettings.gridSize > 0.0f) {
        const auto grid = snapSettings.gridSize;
        considerRightGuide(
            std::round((baseSelection.getX() + nextSelectionW) / grid) * grid,
            false);
        considerBottomGuide(
            std::round((baseSelection.getY() + nextSelectionH) / grid) * grid,
            false);
      }

      if (snapSettings.enableSmartSnap) {
        const auto request = makeSnapRequest(
            baseSelection.withSizeKeepingCentre(nextSelectionW, nextSelectionH)
                .withPosition(baseSelection.getPosition()),
            draggedIds);

        for (const auto &nearby : request.nearbyBounds) {
          considerRightGuide(nearby.getX(), true);
          considerRightGuide(nearby.getRight(), true);
          considerRightGuide(nearby.getCentreX(), true);

          considerBottomGuide(nearby.getY(), true);
          considerBottomGuide(nearby.getBottom(), true);
          considerBottomGuide(nearby.getCentreY(), true);
        }

        for (const auto guideX : request.verticalGuides)
          considerRightGuide(guideX, true);

        for (const auto guideY : request.horizontalGuides)
          considerBottomGuide(guideY, true);
      }

      nextSelectionW = std::max(0.0f, snappedRight - baseSelection.getX());
      nextSelectionH = std::max(0.0f, snappedBottom - baseSelection.getY());

      if (baseW > kScaleEpsilon)
        nextSelectionW = std::max(nextSelectionW, baseW * dragState.minScaleX);
      if (baseH > kScaleEpsilon)
        nextSelectionH = std::max(nextSelectionH, baseH * dragState.minScaleY);

      transientSnapGuides.clear();
      transientSmartSpacingHints.clear();
      if (snappedRightFromSmart && snappedGuideX.has_value())
        transientSnapGuides.push_back({true, *snappedGuideX});
      if (snappedBottomFromSmart && snappedGuideY.has_value())
        transientSnapGuides.push_back({false, *snappedGuideY});
    }

    const auto guidesEqual = [](const std::vector<Guide> &lhs,
                                const std::vector<Guide> &rhs) {
      if (lhs.size() != rhs.size())
        return false;

      for (size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].vertical != rhs[i].vertical)
          return false;
        if (!areClose(lhs[i].worldPosition, rhs[i].worldPosition))
          return false;
      }

      return true;
    };
    const auto spacingHintsEqual =
        [](const std::vector<Interaction::SmartSpacingHint> &lhs,
           const std::vector<Interaction::SmartSpacingHint> &rhs) {
          if (lhs.size() != rhs.size())
            return false;

          for (size_t i = 0; i < lhs.size(); ++i) {
            if (lhs[i].horizontal != rhs[i].horizontal)
              return false;
            if (!areClose(lhs[i].axisPosition, rhs[i].axisPosition))
              return false;
            if (!areClose(lhs[i].firstStart, rhs[i].firstStart))
              return false;
            if (!areClose(lhs[i].firstEnd, rhs[i].firstEnd))
              return false;
            if (!areClose(lhs[i].secondStart, rhs[i].secondStart))
              return false;
            if (!areClose(lhs[i].secondEnd, rhs[i].secondEnd))
              return false;
            if (!areClose(lhs[i].gap, rhs[i].gap))
              return false;
          }

          return true;
        };
    const auto guidesChanged =
        !guidesEqual(previousTransientGuides, transientSnapGuides);
    const auto spacingHintsChanged = !spacingHintsEqual(
        previousTransientSpacingHints, transientSmartSpacingHints);
    const auto transientOverlaysChanged = guidesChanged || spacingHintsChanged;

    auto previousSelectionBounds = dragState.items.front().currentBounds;
    for (size_t i = 1; i < dragState.items.size(); ++i)
      previousSelectionBounds =
          unionRect(previousSelectionBounds, dragState.items[i].currentBounds);

    bool hasDirty = false;
    juce::Rectangle<float> dirtyBounds;

    for (auto &item : dragState.items) {
      auto nextBounds = item.startBounds;
      const auto previousBounds = item.currentBounds;
      if (dragState.mode == DragMode::move) {
        nextBounds = item.startBounds.translated(moveDelta.x, moveDelta.y);
      } else {
        if (baseW > kScaleEpsilon) {
          const auto relX =
              (item.startBounds.getX() - baseSelection.getX()) / baseW;
          const auto relW = item.startBounds.getWidth() / baseW;
          nextBounds.setX(baseSelection.getX() + relX * nextSelectionW);
          nextBounds.setWidth(relW * nextSelectionW);
        } else {
          nextBounds.setWidth(item.startBounds.getWidth());
        }

        if (baseH > kScaleEpsilon) {
          const auto relY =
              (item.startBounds.getY() - baseSelection.getY()) / baseH;
          const auto relH = item.startBounds.getHeight() / baseH;
          nextBounds.setY(baseSelection.getY() + relY * nextSelectionH);
          nextBounds.setHeight(relH * nextSelectionH);
        } else {
          nextBounds.setHeight(item.startBounds.getHeight());
        }

        nextBounds.setWidth(std::max(item.minSize.x, nextBounds.getWidth()));
        nextBounds.setHeight(std::max(item.minSize.y, nextBounds.getHeight()));
      }

      nextBounds = clampBoundsToCanvas(nextBounds, item.currentBounds);

      if (areRectsEqual(nextBounds, item.currentBounds))
        continue;

      item.currentBounds = nextBounds;
      changed = true;

      if (!hasDirty) {
        dirtyBounds = worldToViewRect(unionRect(previousBounds, nextBounds));
        hasDirty = true;
      } else {
        dirtyBounds =
            unionRect(dirtyBounds,
                      worldToViewRect(unionRect(previousBounds, nextBounds)));
      }

      if (auto *view = findWidgetView(item.widgetId))
        view->setViewBounds(worldToViewRect(nextBounds));
    }

    auto appendGuideDirty = [this, &hasDirty, &dirtyBounds](
                                const std::vector<Guide> &guideList) {
      const auto visibleCanvas = visibleCanvasViewBounds();
      if (visibleCanvas.getWidth() <= 0.0f || visibleCanvas.getHeight() <= 0.0f)
        return;

      const auto appendRect =
          [&hasDirty, &dirtyBounds](const juce::Rectangle<float> &rect) {
            if (rect.getWidth() <= 0.0f || rect.getHeight() <= 0.0f)
              return;

            if (!hasDirty) {
              dirtyBounds = rect;
              hasDirty = true;
              return;
            }

            dirtyBounds = unionRect(dirtyBounds, rect);
          };

      for (const auto &guide : guideList) {
        if (guide.vertical) {
          const auto x = worldToView({guide.worldPosition, 0.0f}).x;
          appendRect({x - 3.0f, visibleCanvas.getY(), 6.0f,
                      visibleCanvas.getHeight()});
        } else {
          const auto y = worldToView({0.0f, guide.worldPosition}).y;
          appendRect(
              {visibleCanvas.getX(), y - 3.0f, visibleCanvas.getWidth(), 6.0f});
        }
      }
    };

    auto appendSpacingHintDirty =
        [this, &hasDirty, &dirtyBounds](
            const std::vector<Interaction::SmartSpacingHint> &hints) {
          const auto visibleCanvas = visibleCanvasViewBounds();
          if (visibleCanvas.getWidth() <= 0.0f ||
              visibleCanvas.getHeight() <= 0.0f)
            return;

          const auto appendRect = [&hasDirty, &dirtyBounds, &visibleCanvas](
                                      juce::Rectangle<float> rect) {
            rect = rect.getIntersection(visibleCanvas);
            if (rect.getWidth() <= 0.0f || rect.getHeight() <= 0.0f)
              return;

            if (!hasDirty) {
              dirtyBounds = rect;
              hasDirty = true;
              return;
            }

            dirtyBounds = unionRect(dirtyBounds, rect);
          };

          for (const auto &hint : hints) {
            if (hint.horizontal) {
              const auto y = worldToView({0.0f, hint.axisPosition}).y;
              const auto x1a = worldToView({hint.firstStart, 0.0f}).x;
              const auto x1b = worldToView({hint.firstEnd, 0.0f}).x;
              const auto x2a = worldToView({hint.secondStart, 0.0f}).x;
              const auto x2b = worldToView({hint.secondEnd, 0.0f}).x;

              appendRect({std::min(x1a, x1b) - 4.0f, y - 3.0f,
                          std::abs(x1b - x1a) + 8.0f, 6.0f});
              appendRect({std::min(x2a, x2b) - 4.0f, y - 3.0f,
                          std::abs(x2b - x2a) + 8.0f, 6.0f});
              appendRect(
                  {((x1a + x1b) * 0.5f) - 26.0f, y - 27.0f, 52.0f, 16.0f});
              appendRect(
                  {((x2a + x2b) * 0.5f) - 26.0f, y - 27.0f, 52.0f, 16.0f});
            } else {
              const auto x = worldToView({hint.axisPosition, 0.0f}).x;
              const auto y1a = worldToView({0.0f, hint.firstStart}).y;
              const auto y1b = worldToView({0.0f, hint.firstEnd}).y;
              const auto y2a = worldToView({0.0f, hint.secondStart}).y;
              const auto y2b = worldToView({0.0f, hint.secondEnd}).y;

              appendRect({x - 3.0f, std::min(y1a, y1b) - 4.0f, 6.0f,
                          std::abs(y1b - y1a) + 8.0f});
              appendRect({x - 3.0f, std::min(y2a, y2b) - 4.0f, 6.0f,
                          std::abs(y2b - y2a) + 8.0f});
              appendRect({x + 4.0f, ((y1a + y1b) * 0.5f) - 9.0f, 52.0f, 18.0f});
              appendRect({x + 4.0f, ((y2a + y2b) * 0.5f) - 9.0f, 52.0f, 18.0f});
            }
          }
        };

    if (changed) {
      auto nextSelectionBounds = dragState.items.front().currentBounds;
      for (size_t i = 1; i < dragState.items.size(); ++i)
        nextSelectionBounds =
            unionRect(nextSelectionBounds, dragState.items[i].currentBounds);

      const auto selectionDirty = worldToViewRect(
          unionRect(previousSelectionBounds, nextSelectionBounds));
      dirtyBounds =
          hasDirty ? unionRect(dirtyBounds, selectionDirty) : selectionDirty;
    }

    if (guidesChanged) {
      appendGuideDirty(previousTransientGuides);
      appendGuideDirty(transientSnapGuides);
    }
    if (spacingHintsChanged) {
      appendSpacingHintDirty(previousTransientSpacingHints);
      appendSpacingHintDirty(transientSmartSpacingHints);
    }

    if (changed || transientOverlaysChanged) {
      if (hasDirty) {
        dirtyBounds = dirtyBounds.expanded(4.0f, 4.0f);
        repaint(dirtyBounds.getSmallestIntegerContainer());
      } else {
        repaint();
      }
    }
  }

  void handleWidgetMouseUp(WidgetId id,
                           const juce::MouseEvent *event = nullptr) {
    if (isRunMode()) {
      handleWidgetRuntimeMouseUp(id, event);
      return;
    }

    if (!dragState.active || dragState.anchorWidgetId != id)
      return;

    const auto drag = dragState;
    dragState = {};
    clearTransientSnapGuides();

    std::vector<WidgetBoundsUpdate> updates;
    updates.reserve(drag.items.size());
    for (const auto &item : drag.items) {
      const auto clampedBounds =
          clampBoundsToCanvas(item.currentBounds, item.startBounds);
      if (!areRectsEqual(item.startBounds, clampedBounds))
        updates.push_back({item.widgetId, clampedBounds});
    }

    if (!updates.empty())
      document.setWidgetsBounds(updates);

    refreshFromDocument();

    if (normalizeSelectionAfterAltReleasePending && !altPreviewEnabled) {
      normalizeSelectionAfterAltReleasePending = false;
      normalizeSelectionForCurrentModifierState();
    }
  }

  std::vector<WidgetId>
  collectMarqueeHitIds(const juce::Rectangle<float> &marqueeBounds) const {
    std::vector<WidgetId> hits;
    if (marqueeBounds.getWidth() <= 0.0f && marqueeBounds.getHeight() <= 0.0f)
      return hits;

    const auto restrictToGroupId = activeGroupEditId;
    for (const auto &widget : document.snapshot().widgets) {
      if (restrictToGroupId.has_value() &&
          !isWidgetInGroup(widget.id, *restrictToGroupId))
        continue;
      if (!isWidgetEffectivelyVisible(widget.id))
        continue;

      if (widget.bounds.intersects(marqueeBounds))
        hits.push_back(widget.id);
    }

    return hits;
  }

  void applyMarqueeSelection() {
    auto hits = collectMarqueeHitIds(viewToWorldRect(marqueeState.bounds));
    if (!activeGroupEditId.has_value())
      hits = expandToSelectionUnits(hits);

    auto nextSelection = marqueeState.additive || marqueeState.toggle
                             ? document.editorState().selection
                             : std::vector<WidgetId>{};

    if (marqueeState.toggle) {
      for (const auto id : hits) {
        if (const auto it =
                std::find(nextSelection.begin(), nextSelection.end(), id);
            it != nextSelection.end())
          nextSelection.erase(it);
        else
          nextSelection.push_back(id);
      }
    } else {
      addUniqueSelectionIds(nextSelection, hits);
    }

    document.setSelection(std::move(nextSelection));
    syncSelectionToViews();
  }

  bool nudgeSelection(juce::Point<float> delta) {
    if (areClose(delta.x, 0.0f) && areClose(delta.y, 0.0f))
      return false;

    std::vector<WidgetBoundsUpdate> updates;
    const auto &selection = document.editorState().selection;
    updates.reserve(selection.size());

    for (const auto id : selection) {
      const auto *widget = findWidgetModel(id);
      if (widget == nullptr)
        continue;
      if (!isWidgetEffectivelyVisible(id) || isWidgetEffectivelyLocked(id))
        continue;

      updates.push_back(
          {id, clampBoundsToCanvas(widget->bounds.translated(delta.x, delta.y),
                                   widget->bounds)});
    }

    if (updates.empty())
      return false;
    if (!document.setWidgetsBounds(updates))
      return false;

    refreshFromDocument();
    return true;
  }

  void notifyStateChanged() {
    if (onStateChanged != nullptr)
      onStateChanged();
  }

  void notifyViewportChanged() {
    if (onViewportChanged != nullptr)
      onViewportChanged();
  }

  void emitRuntimeLog(const juce::String &action, const juce::String &detail) {
    if (onRuntimeLog != nullptr)
      onRuntimeLog(action, detail);
  }

  void refreshAltPreviewState() {
    if (isRunMode()) {
      if (altPreviewEnabled || normalizeSelectionAfterAltReleasePending) {
        altPreviewEnabled = false;
        normalizeSelectionAfterAltReleasePending = false;
        repaint();
      }
      return;
    }

    const auto wasAltDown = altPreviewEnabled;
    const auto nextAltDown =
        juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown();
    if (altPreviewEnabled == nextAltDown)
      return;

    altPreviewEnabled = nextAltDown;

    if (wasAltDown && !nextAltDown) {
      if (dragState.active || marqueeState.active) {
        normalizeSelectionAfterAltReleasePending = true;
      } else {
        normalizeSelectionForCurrentModifierState();
      }
    } else if (nextAltDown) {
      normalizeSelectionAfterAltReleasePending = false;
    }

    repaint();
  }

  void syncSelectionToViews() {
    const auto syncStart = std::chrono::steady_clock::now();
    if (activeGroupEditId.has_value()) {
      const auto *activeGroup = findGroupById(*activeGroupEditId);
      if (activeGroup == nullptr) {
        activeGroupEditId.reset();
      } else {
        const auto hasOutsideMember = std::any_of(
            document.editorState().selection.begin(),
            document.editorState().selection.end(),
            [this](WidgetId selectedId) {
              return !activeGroupEditId.has_value() ||
                     !isWidgetInGroup(selectedId, *activeGroupEditId);
            });
        if (hasOutsideMember)
          activeGroupEditId.reset();
      }
    }

    const auto previousSelection = lastSelectionSnapshot;
    const auto &selection = document.editorState().selection;
    const auto showSelectionVisuals = !isRunMode();
    const auto showWidgetHandles =
        showSelectionVisuals && selection.size() == 1;

    bool hasDirtyBounds = false;
    juce::Rectangle<float> dirtyBounds;
    const auto addDirtyBounds =
        [&hasDirtyBounds, &dirtyBounds](const juce::Rectangle<float> &bounds) {
          if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f)
            return;
          if (!hasDirtyBounds) {
            dirtyBounds = bounds;
            hasDirtyBounds = true;
            return;
          }

          dirtyBounds = unionRect(dirtyBounds, bounds);
        };

    for (auto &view : widgetViews) {
      const auto isSelected =
          showSelectionVisuals && containsWidgetId(selection, view->widgetId());
      const auto *group = findGroupByMember(view->widgetId());
      const auto isGrouped = group != nullptr;
      const auto groupedInActiveEdit =
          isGrouped && activeGroupEditId.has_value() &&
          isWidgetInGroup(view->widgetId(), *activeGroupEditId);
      if (view->setSelectionVisual(isSelected, isSelected && showWidgetHandles,
                                   isGrouped, groupedInActiveEdit)) {
        addDirtyBounds(view->getBounds().toFloat());
      }
    }

    const auto appendSelectionBounds =
        [this, &addDirtyBounds](const std::vector<WidgetId> &ids) {
          bool hasSelectionBounds = false;
          juce::Rectangle<float> selectionBounds;
          for (const auto id : ids) {
            const auto *view = findWidgetView(id);
            if (view == nullptr)
              continue;

            const auto bounds = view->getBounds().toFloat();
            if (!hasSelectionBounds) {
              selectionBounds = bounds;
              hasSelectionBounds = true;
            } else {
              selectionBounds = unionRect(selectionBounds, bounds);
            }
          }

          if (hasSelectionBounds)
            addDirtyBounds(selectionBounds);
        };

    appendSelectionBounds(previousSelection);
    appendSelectionBounds(selection);
    lastSelectionSnapshot = selection;

    bool requestedPartialRepaint = false;
    float dirtyAreaPx = 0.0f;
    if (hasDirtyBounds) {
      repaint(dirtyBounds.expanded(6.0f, 6.0f).getSmallestIntegerContainer());
      requestedPartialRepaint = true;
      dirtyAreaPx =
          std::max(0.0f, dirtyBounds.getWidth() * dirtyBounds.getHeight());
    }

    const auto syncEnd = std::chrono::steady_clock::now();
    const auto syncMs =
        std::chrono::duration<double, std::milli>(syncEnd - syncStart).count();
    perf.selectionSyncCount += 1;
    perf.lastSelectionSyncMs = syncMs;
    perf.maxSelectionSyncMs = std::max(perf.maxSelectionSyncMs, syncMs);
    if (requestedPartialRepaint) {
      perf.selectionSyncRequestedPartialRepaintCount += 1;
      perf.lastDirtyAreaPx = dirtyAreaPx;
    }

    const auto shouldLogSync =
        syncMs >= slowCanvasSelectionSyncLogThresholdMs ||
        (periodicCanvasPerfLogInterval > 0 &&
         perf.selectionSyncCount % periodicCanvasPerfLogInterval == 0);
    if (shouldLogSync) {
      DBG(juce::String("[Gyeol][Canvas][Perf] selectionSync#") +
          juce::String(static_cast<int64_t>(perf.selectionSyncCount)) +
          " ms=" + juce::String(syncMs, 3) +
          " selection=" + juce::String(static_cast<int>(selection.size())) +
          " partialReq=" + juce::String(requestedPartialRepaint ? 1 : 0) +
          " dirtyPx=" + juce::String(static_cast<int>(dirtyAreaPx)) +
          " maxSelectionSyncMs=" + juce::String(perf.maxSelectionSyncMs, 3));
    }

    notifyStateChanged();
  }

public:
  struct PerfStats {
    uint64_t refreshCount = 0;
    uint64_t paintCount = 0;
    uint64_t selectionSyncCount = 0;
    uint64_t dragPreviewUpdateCount = 0;
    uint64_t refreshRequestedPartialRepaintCount = 0;
    uint64_t refreshRequestedFullRepaintCount = 0;
    uint64_t selectionSyncRequestedPartialRepaintCount = 0;
    double lastRefreshMs = 0.0;
    double maxRefreshMs = 0.0;
    double lastPaintMs = 0.0;
    double maxPaintMs = 0.0;
    double lastSelectionSyncMs = 0.0;
    double maxSelectionSyncMs = 0.0;
    float lastDirtyAreaPx = 0.0f;
    int lastWidgetViewCount = 0;
    int lastSelectionCount = 0;
  };

  const PerfStats &performanceStats() const noexcept { return perf; }

private:
  static constexpr double slowCanvasRefreshLogThresholdMs = 8.0;
  static constexpr double slowCanvasPaintLogThresholdMs = 8.0;
  static constexpr double slowCanvasSelectionSyncLogThresholdMs = 4.0;
  static constexpr uint64_t periodicCanvasPerfLogInterval = 120;

  DocumentHandle &document;
  const Widgets::WidgetFactory &widgetFactory;
  CanvasRenderer renderer;
  std::vector<std::unique_ptr<WidgetComponent>> widgetViews;
  std::function<void()> onStateChanged;
  std::function<void()> onViewportChanged;
  std::function<void(const juce::String &, const juce::String &)> onRuntimeLog;
  std::function<std::optional<WidgetId>()> activeLayerResolver;
  std::function<void(const juce::String &, juce::Point<float>)>
      onWidgetLibraryDrop;
  float zoomLevel = 1.0f;
  juce::Point<float> viewOriginWorld;
  DragState dragState;
  MarqueeState marqueeState;
  PanState panState;
  Interaction::SnapEngine snapEngine;
  Interaction::SnapSettings snapSettings;
  std::vector<Guide> guides;
  std::vector<Guide> transientSnapGuides;
  std::vector<Interaction::SmartSpacingHint> transientSmartSpacingHints;
  GuideDragState guideDragState;
  RuntimeInteractionState runtimeInteraction;
  Runtime::RuntimeParamBridge runtimeParamBridge;
  Runtime::RuntimeDiagnostics runtimeDiagnostics;
  Runtime::RuntimeActionExecutor runtimeActionExecutor;
  Runtime::RuntimeBindingEngine runtimeBindingEngine;
  InteractionMode interactionModeValue = InteractionMode::preview;
  bool previewBindingSimulationEnabled = false;
  bool widgetLibraryDropPreviewActive = false;
  juce::Point<float> widgetLibraryDropPreviewView;
  bool assetDropPreviewActive = false;
  juce::Point<float> assetDropPreviewView;
  WidgetId assetDropPreviewWidgetId = kRootId;
  bool assetDropPreviewValid = false;
  juce::String assetDropPreviewRefKey;
  bool hasMouseLocalPoint = false;
  juce::Point<float> lastMouseLocalPoint;
  std::optional<WidgetId> activeGroupEditId;
  bool altPreviewEnabled = false;
  bool normalizeSelectionAfterAltReleasePending = false;
  std::vector<WidgetId> lastSelectionSnapshot;
  juce::String lastRuntimeBindingFailureDigest;
  PerfStats perf;

  friend class WidgetComponent;
};

void WidgetComponent::mouseDown(const juce::MouseEvent &event) {
  owner.handleWidgetMouseDown(widget.id, isResizeHandleHit(event.position),
                              event);
}

void WidgetComponent::mouseDrag(const juce::MouseEvent &event) {
  owner.handleWidgetMouseDrag(widget.id, event);
}

void WidgetComponent::mouseUp(const juce::MouseEvent &event) {
  owner.handleWidgetMouseUp(widget.id, &event);
}

void WidgetComponent::mouseDoubleClick(const juce::MouseEvent &event) {
  owner.handleCanvasDoubleClick(event.getEventRelativeTo(&owner).position);
}

void WidgetComponent::mouseMove(const juce::MouseEvent &event) {
  owner.updateMouseTrackerFromChild(event.getEventRelativeTo(&owner).position);
  const auto hot = isResizeHandleHit(event.position);
  if (hot != resizeHandleHot) {
    resizeHandleHot = hot;
    repaint();
  }
}

void WidgetComponent::mouseExit(const juce::MouseEvent &) {
  owner.clearMouseTrackerFromChild();
  if (resizeHandleHot) {
    resizeHandleHot = false;
    repaint();
  }
}

void WidgetComponent::mouseWheelMove(const juce::MouseEvent &event,
                                     const juce::MouseWheelDetails &wheel) {
  owner.applyWheelZoomAtPoint(event.getEventRelativeTo(&owner).position, wheel);
}
} // namespace Gyeol::Ui

namespace Gyeol {
class EditorHandle::Impl : private juce::AsyncUpdater {
private:
  enum class BreakpointClass : int;
  enum class DensityPreset : int;
  struct DensityTokens;
  struct LayoutProfile;
  struct ShortcutBinding;

public:
  explicit Impl(EditorHandle &ownerIn)
      : owner(ownerIn), widgetRegistry(Widgets::makeDefaultWidgetRegistry()),
        widgetFactory(widgetRegistry), canvas(docHandle, widgetFactory),
        gridSnapPanel(), navigatorPanel(),
        layerTreePanel(docHandle, widgetFactory),
        widgetLibraryPanel(widgetRegistry),
        assetsPanel(docHandle, widgetFactory),
        leftPanels(juce::TabbedButtonBar::TabsAtTop),
        rightPanels(juce::TabbedButtonBar::TabsAtTop),
        propertyPanel(docHandle, widgetFactory),
        eventActionPanel(docHandle, widgetRegistry),
        validationPanel(docHandle, widgetRegistry), exportPreviewPanel(),
        performancePanel(), historyPanel(),
        deleteSelected(juce::CharPointer_UTF8("\xE2\x9C\x95")),
        groupSelected(juce::CharPointer_UTF8("\xE2\x8A\x9E")),
        ungroupSelected(juce::CharPointer_UTF8("\xE2\x8A\x9F")),
        arrangeMenuButton(juce::CharPointer_UTF8("\xE2\x89\xA3")),
        dumpJsonButton("{ }"),
        exportJuceButton(juce::CharPointer_UTF8("\xE2\x87\xA7")),
        undoButton(juce::CharPointer_UTF8("\xE2\x86\x90")),
        redoButton(juce::CharPointer_UTF8("\xE2\x86\x92")),
        previewModeButton(juce::CharPointer_UTF8("\xE2\x97\x89"
                                                 " Preview")),
        runModeButton(juce::CharPointer_UTF8("\xE2\x96\xB6"
                                             " Run")) {
    owner.setWantsKeyboardFocus(true);
    owner.setFocusContainerType(
        juce::Component::FocusContainerType::focusContainer);

    initializeSettingsFile();
    loadUiSettings();
    applyThemeState();

    leftPanels.setTabBarDepth(30);
    leftPanels.addTab("Layers", palette(GyeolPalette::PanelBackground),
                      &layerTreePanel, false);
    leftPanels.addTab("Library", palette(GyeolPalette::PanelBackground),
                      &widgetLibraryPanel, false);
    leftPanels.addTab("Assets", palette(GyeolPalette::PanelBackground),
                      &assetsPanel, false);
    leftPanels.setCurrentTabIndex(0, juce::dontSendNotification);

    rightPanels.setTabBarDepth(30);
    rightPanels.addTab("Inspector", palette(GyeolPalette::PanelBackground),
                       &propertyPanel, false);
    rightPanels.addTab("Event/Action", palette(GyeolPalette::PanelBackground),
                       &eventActionPanel, false);
    rightPanels.addTab("Validation", palette(GyeolPalette::PanelBackground),
                       &validationPanel, false);
    rightPanels.addTab("Export Preview", palette(GyeolPalette::PanelBackground),
                       &exportPreviewPanel, false);
    rightPanels.addTab("Performance", palette(GyeolPalette::PanelBackground),
                       &performancePanel, false);
    rightPanels.setCurrentTabIndex(0, juce::dontSendNotification);

    owner.addAndMakeVisible(leftPanels);
    owner.addAndMakeVisible(canvas);
    owner.addChildComponent(navigatorPanel);
    owner.addAndMakeVisible(rightPanels);
    owner.addAndMakeVisible(historyPanel);
    owner.addChildComponent(gridSnapPanel);
    canvas.setActiveLayerResolver([this] { return resolveActiveLayerId(); });
    canvas.setWidgetLibraryDropCallback(
        [this](const juce::String &typeKey, juce::Point<float> worldPosition) {
          createWidgetFromLibrary(typeKey, worldPosition);
        });
    gridSnapPanel.setSettings(canvas.currentSnapSettings());
    gridSnapPanel.setSettingsChangedCallback(
        [this](const Ui::Interaction::SnapSettings &settings) {
          canvas.setSnapSettings(settings);
        });
    gridSnapPanel.setVisible(false);
    canvas.setStateChangedCallback([this] { handleCanvasStateChanged(); });
    canvas.setViewportChangedCallback(
        [this] { refreshViewDiagnosticsPanels(); });
    canvas.setRuntimeLogCallback(
        [this](const juce::String &action, const juce::String &detail) {
          appendHistoryEntry(action, detail);
        });
    navigatorPanel.setNavigateRequestedCallback(
        [this](juce::Point<float> worldPoint) {
          canvas.focusWorldPoint(worldPoint);
        });

    layerTreePanel.setSelectionChangedCallback(
        [this](std::vector<WidgetId> selection) {
          if (selection == docHandle.editorState().selection) {
            syncInspectorTargetFromState();
            return;
          }

          docHandle.setSelection(std::move(selection));
          canvas.syncSelectionFromDocument();
          requestDeferredUiRefresh(true, true);
          refreshToolbarState();
          syncInspectorTargetFromState();
        });
    layerTreePanel.setActiveLayerChangedCallback(
        [this](std::optional<WidgetId> layerId) {
          activeLayerOverrideId = layerId;
          syncInspectorTargetFromState();
        });
    layerTreePanel.setDropRequestCallback(
        [this](const Ui::Interaction::LayerTreeDropRequest &request)
            -> juce::Result {
          DBG("[Gyeol][LayerTreeDnD][EditorHandle] drop request dragged=" +
              juce::String(static_cast<int>(request.dragged.size())) +
              " parentKind=" +
              juce::String(static_cast<int>(request.parent.kind)) +
              " parentId=" + juce::String(request.parent.id) +
              " insertIndex=" + juce::String(request.insertIndex));
          const auto result =
              layerOrderEngine.applyTreeDrop(docHandle, request);
          if (result.failed()) {
            DBG("[Gyeol][LayerTreeDnD][EditorHandle] drop failed: " +
                result.getErrorMessage());
            return result;
          }

          refreshCanvasAndRequestPanels(true, true, true);
          appendHistoryEntry(
              "LayerTree DnD",
              "dragged=" +
                  juce::String(static_cast<int>(request.dragged.size())) +
                  ", parent=" + juce::String(request.parent.id));
          DBG("[Gyeol][LayerTreeDnD][EditorHandle] drop applied");
          return juce::Result::ok();
        });
    layerTreePanel.setNodePropsChangedCallback(
        [this](const SetPropsAction &action) -> juce::Result {
          if (!docHandle.setProps(action))
            return juce::Result::fail("Failed to apply node property update");

          refreshCanvasAndRequestPanels(true, true, true);
          appendHistoryEntry(
              "Set Props",
              "kind=" + juce::String(static_cast<int>(action.kind)) +
                  ", ids=" + juce::String(static_cast<int>(action.ids.size())));
          return juce::Result::ok();
        });
    layerTreePanel.setCreateLayerRequestedCallback(
        [this]() -> std::optional<WidgetId> {
          CreateAction action;
          action.kind = NodeKind::layer;

          CreateLayerPayload payload;
          payload.name =
              "Layer " + juce::String(static_cast<int>(
                             docHandle.snapshot().layers.size() + 1));
          action.payload = payload;

          const auto newLayerId = docHandle.createNode(action);
          if (newLayerId <= kRootId)
            return std::nullopt;

          activeLayerOverrideId = newLayerId;
          refreshCanvasAndRequestPanels(true, true, true);
          appendHistoryEntry("Create Layer", "id=" + juce::String(newLayerId));

          return newLayerId;
        });
    layerTreePanel.setDeleteLayerRequestedCallback(
        [this](WidgetId layerId) -> juce::Result {
          if (layerId <= kRootId)
            return juce::Result::fail("Invalid layer id");

          DeleteAction action;
          action.kind = NodeKind::layer;
          action.ids = {layerId};
          action.policy = DeleteLayerPolicy{};
          if (!docHandle.deleteNodes(action))
            return juce::Result::fail("Failed to delete layer");

          if (activeLayerOverrideId.has_value() &&
              *activeLayerOverrideId == layerId)
            activeLayerOverrideId.reset();

          refreshCanvasAndRequestPanels(true, true, true);
          appendHistoryEntry("Delete Layer", "id=" + juce::String(layerId));
          return juce::Result::ok();
        });
    widgetLibraryPanel.setCreateRequestedCallback(
        [this](const juce::String &typeKey) {
          createWidgetFromLibrary(typeKey);
        });
    assetsPanel.setAssetsChangedCallback([this](const juce::String &reason) {
      validationPanel.markDirty();
      refreshToolbarState();
      appendHistoryEntry("Assets", reason);
      requestDeferredUiRefresh(false, true);
    });
    assetsPanel.setAssetUsageNavigateCallback(
        [this](WidgetId widgetId) { focusWidgetFromAssetUsage(widgetId); });
    eventActionPanel.setBindingsChangedCallback([this] {
      validationPanel.markDirty();
      refreshToolbarState();
      appendHistoryEntry("Runtime State", "Event/Action panel");
      requestDeferredUiRefresh(false, true);
    });
    validationPanel.setAutoRefreshEnabled(true);
    exportPreviewPanel.setAutoRefreshEnabled(false);
    exportPreviewPanel.setGeneratePreviewCallback(
        [this](const juce::String &componentClassName,
               Ui::Panels::ExportPreviewPanel::PreviewData &outData)
            -> juce::Result {
          return generateExportPreview(componentClassName, outData);
        });
    exportPreviewPanel.setExportRequestedCallback(
        [this](const juce::String &componentClassName) {
          runJuceExport(componentClassName);
        });
    historyPanel.setUndoRequestedCallback(
        [this] { performUndoFromHistoryPanel(); });
    historyPanel.setRedoRequestedCallback(
        [this] { performRedoFromHistoryPanel(); });
    historyPanel.setCollapseToggledCallback([this](bool) { owner.resized(); });
    {
      Ui::Panels::PropertyPanel::CommitCallbacks callbacks;
      callbacks.onSetPropsPreview =
          [this](const SetPropsAction &action) -> juce::Result {
        return docHandle.previewSetProps(action)
                   ? juce::Result::ok()
                   : juce::Result::fail("previewSetProps failed");
      };
      callbacks.onSetBoundsPreview =
          [this](const SetBoundsAction &action) -> juce::Result {
        return docHandle.previewSetBounds(action)
                   ? juce::Result::ok()
                   : juce::Result::fail("previewSetBounds failed");
      };
      callbacks.onPreviewApplied = [this] {
        refreshCanvasAndRequestPanels(true, true, false);
      };
      callbacks.onCommitted = [this] {
        requestDeferredUiRefresh(false, true);
        appendHistoryEntry("Inspector Commit", "Property/transform applied");
      };
      propertyPanel.setCommitCallbacks(std::move(callbacks));
    }

    buildCreateButtons();

    owner.addAndMakeVisible(deleteSelected);
    owner.addAndMakeVisible(groupSelected);
    owner.addAndMakeVisible(ungroupSelected);
    owner.addAndMakeVisible(arrangeMenuButton);
    owner.addAndMakeVisible(dumpJsonButton);
    owner.addAndMakeVisible(exportJuceButton);
    owner.addAndMakeVisible(undoButton);
    owner.addAndMakeVisible(redoButton);
    owner.addAndMakeVisible(previewModeButton);
    owner.addAndMakeVisible(runModeButton);
    owner.addAndMakeVisible(previewBindingSimToggle);
    owner.addAndMakeVisible(gridSnapMenuButton);
    owner.addAndMakeVisible(leftDockButton);
    owner.addAndMakeVisible(rightDockButton);
    owner.addAndMakeVisible(uiMenuButton);
    owner.addAndMakeVisible(shortcutHint);

    deleteSelected.setTooltip("Delete");
    groupSelected.setTooltip("Group");
    ungroupSelected.setTooltip("Ungroup");
    arrangeMenuButton.setTooltip("Arrange");
    dumpJsonButton.setTooltip("Dump JSON");
    exportJuceButton.setTooltip("Export JUCE");
    undoButton.setTooltip("Undo");
    redoButton.setTooltip("Redo");
    leftDockButton.setTooltip("Toggle Left Dock Overlay");
    rightDockButton.setTooltip("Toggle Right Dock Overlay");
    uiMenuButton.setTooltip("UI Command Palette");

    previewModeButton.setClickingTogglesState(true);
    runModeButton.setClickingTogglesState(true);
    previewBindingSimToggle.setClickingTogglesState(true);

    deleteSelected.onClick = [this] { deleteCurrentSelection(); };

    groupSelected.onClick = [this] {
      suppressNextCanvasMutationHistory = true;
      if (canvas.groupSelection())
        appendHistoryEntry("Group", "Selection grouped");
      else
        suppressNextCanvasMutationHistory = false;
    };

    ungroupSelected.onClick = [this] {
      suppressNextCanvasMutationHistory = true;
      if (canvas.ungroupSelection())
        appendHistoryEntry("Ungroup", "Selection ungrouped");
      else
        suppressNextCanvasMutationHistory = false;
    };

    arrangeMenuButton.onClick = [this] { showArrangeMenu(); };

    dumpJsonButton.onClick = [this] {
      juce::String json;
      const auto result = Serialization::serializeDocumentToJsonString(
          docHandle.snapshot(), docHandle.editorState(), json);
      if (result.failed()) {
        DBG("[Gyeol] JSON dump failed: " + result.getErrorMessage());
        return;
      }

      DBG("[Gyeol] ----- Document JSON BEGIN -----");
      DBG(json);
      DBG("[Gyeol] ----- Document JSON END -----");

      DBG("[Gyeol] ----- Export Mapping BEGIN -----");
      for (const auto &mapping : widgetFactory.exportMappings())
        DBG("[Gyeol] " + mapping.typeKey + " -> " + mapping.exportTargetType);
      DBG("[Gyeol] ----- Export Mapping END -----");
    };

    undoButton.onClick = [this] { performUndoFromToolbar(); };

    exportJuceButton.onClick = [this] { runJuceExport(); };

    redoButton.onClick = [this] { performRedoFromToolbar(); };

    previewModeButton.onClick = [this] {
      if (suppressModeToggleCallbacks)
        return;
      setEditorMode(Ui::CanvasComponent::InteractionMode::preview);
    };

    runModeButton.onClick = [this] {
      if (suppressModeToggleCallbacks)
        return;
      if (canvas.isRunMode())
        setEditorMode(Ui::CanvasComponent::InteractionMode::preview);
      else
        setEditorMode(Ui::CanvasComponent::InteractionMode::run);
    };

    gridSnapMenuButton.onClick = [this] {
      gridSnapPanel.setVisible(!gridSnapPanel.isVisible());
      if (gridSnapPanel.isVisible()) {
        const auto tokens = currentDensityTokens();
        const auto popupWidth = juce::roundToInt(220.0f * uiScaleFactor());
        const auto popupHeight = juce::roundToInt(170.0f * uiScaleFactor());
        gridSnapPanel.setBounds(gridSnapMenuButton.getRight() - popupWidth,
                                tokens.toolbarHeight + tokens.spacing,
                                popupWidth, popupHeight);
      }
    };

    leftDockButton.onClick = [this] { toggleDockVisibility(true); };

    rightDockButton.onClick = [this] { toggleDockVisibility(false); };
    uiMenuButton.onClick = [this] {
      juce::PopupMenu menu;
      menu.addItem(1, "Theme: Deep Dark");
      menu.addItem(2, "Theme: Light");
      menu.addItem(3, "Theme: High Contrast");
      menu.addSeparator();
      menu.addItem(4, "Density: Compact");
      menu.addItem(5, "Density: Comfortable");
      menu.addItem(6, "Density: Spacious");
      menu.addSeparator();
      menu.addItem(7, reducedMotionEnabled ? "Reduced Motion: On"
                                           : "Reduced Motion: Off");
      menu.addItem(8, lowEndRenderingMode ? "Low-End Rendering: On"
                                          : "Low-End Rendering: Off");
      menu.addSeparator();
      menu.addItem(9, "Open Command Palette");
      menu.addItem(10, "Open Quick Switcher");
      menu.addItem(11, "Shortcut Conflict Inspector");
      menu.addItem(12, "Shortcut Editor");
      menu.addSeparator();
      menu.addItem(13, "Run Accessibility Audit");
      menu.addItem(14, "Run HiDPI Validation");
      menu.addItem(15, "Capture UI Baseline");
      menu.addItem(16, "Run UI Quality Gate");

      const juce::Component::SafePointer<EditorHandle> safeOwner(&owner);
      menu.showMenuAsync(
          juce::PopupMenu::Options().withTargetComponent(&uiMenuButton),
          [this, safeOwner](int picked) {
            if (safeOwner == nullptr || picked <= 0)
              return;

            bool stateChanged = false;
            bool repaintRequired = false;

            if (picked == 1) {
              themeVariant = GyeolThemeVariant::deepDark;
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 2) {
              themeVariant = GyeolThemeVariant::light;
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 3) {
              themeVariant = GyeolThemeVariant::highContrast;
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 4) {
              densityPreset = DensityPreset::compact;
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 5) {
              densityPreset = DensityPreset::comfortable;
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 6) {
              densityPreset = DensityPreset::spacious;
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 7) {
              reducedMotionEnabled = !reducedMotionEnabled;
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 8) {
              lowEndRenderingMode = !lowEndRenderingMode;
              updateLowEndRenderingMode();
              stateChanged = true;
              repaintRequired = true;
            } else if (picked == 9) {
              showUiCommandPalette(false);
            } else if (picked == 10) {
              showUiCommandPalette(true);
            } else if (picked == 11) {
              showShortcutConflictInspector();
            } else if (picked == 12) {
              openShortcutEditor();
            } else if (picked == 13) {
              runAccessibilityAudit();
            } else if (picked == 14) {
              runHiDpiLayoutValidation();
            } else if (picked == 15) {
              captureUiBaselineSnapshots();
            } else if (picked == 16) {
              runUiQualityGate();
            }

            if (stateChanged) {
              applyThemeState();
              owner.resized();
              if (repaintRequired)
                owner.repaint();
              saveUiSettings();
            }
          });
    };
    previewBindingSimToggle.onClick = [this] {
      if (suppressPreviewBindingToggleCallbacks)
        return;

      if (canvas.isRunMode()) {
        suppressPreviewBindingToggleCallbacks = true;
        previewBindingSimToggle.setToggleState(false,
                                               juce::dontSendNotification);
        suppressPreviewBindingToggleCallbacks = false;
        return;
      }

      setPreviewBindingSimulation(previewBindingSimToggle.getToggleState());
    };

    rebuildShortcutBindings();
    applyAccessibilityMetadata();
    applyVirtualizationHints();
    updateLowEndRenderingMode();

    shortcutHint.setJustificationType(juce::Justification::centredRight);
    shortcutHint.setColour(juce::Label::textColourId,
                           juce::Colour::fromRGB(170, 175, 186));
    shortcutHint.setInterceptsMouseClicks(false, false);
    updateShortcutHintForMode();
    setEditorMode(Ui::CanvasComponent::InteractionMode::preview);

    refreshAllPanelsFromDocument();
  }

  ~Impl() override {
    saveUiSettings();
    cancelPendingUpdate();
  }

  DocumentHandle &document() noexcept { return docHandle; }
  const DocumentHandle &document() const noexcept { return docHandle; }

  void paint(juce::Graphics &g, juce::Rectangle<int> bounds) {
    const auto paintStartMs = juce::Time::getMillisecondCounterHiRes();

    g.fillAll(palette(GyeolPalette::CanvasBackground));

    const auto tokens = currentDensityTokens();
    auto toolbarBounds = bounds.removeFromTop(tokens.toolbarHeight);
    g.setColour(palette(GyeolPalette::HeaderBackground));
    g.fillRect(toolbarBounds);

    g.setColour(palette(GyeolPalette::BorderDefault, 0.92f));
    g.drawHorizontalLine(toolbarBounds.getBottom(),
                         static_cast<float>(toolbarBounds.getX()),
                         static_cast<float>(toolbarBounds.getRight()));

    for (int x : separatorXs) {
      g.drawVerticalLine(
          x, static_cast<float>(toolbarBounds.getY() + tokens.spacing),
          static_cast<float>(toolbarBounds.getBottom() - tokens.spacing));
    }

    if (breakpointClass == BreakpointClass::narrow) {
      const auto &profile = activeLayoutProfile();
      if ((profile.leftOverlayOpen || profile.rightOverlayOpen) &&
          !lastContentBounds.isEmpty()) {
        g.setColour(palette(GyeolPalette::OverlayBackground, 0.28f));
        g.fillRect(lastContentBounds);
      }
    }

    lastToolbarPaintMs =
        juce::Time::getMillisecondCounterHiRes() - paintStartMs;
  }
  void resized(juce::Rectangle<int> bounds) {
    const auto layoutStartMs = juce::Time::getMillisecondCounterHiRes();
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto throttledResize = (nowMs - lastResizeMs) < 18.0;
    if (throttledResize)
      ++resizeThrottleCount;
    lastResizeMs = nowMs;

    breakpointClass = classifyBreakpoint(bounds.getWidth());
    forceSingleColumnEditorsActive =
        breakpointClass != BreakpointClass::narrow && bounds.getWidth() < 1120;

    const auto tokens = currentDensityTokens();
    applyDensityTypography(tokens, forceSingleColumnEditorsActive);
    applyCompactionRules(bounds.getWidth());

    auto &profile = ensureLayoutProfile(breakpointClass, bounds);

    separatorXs.clear();
    auto toolbar = bounds.removeFromTop(tokens.toolbarHeight)
                       .reduced(tokens.spacing * 2, tokens.spacing);

    const auto compactToolbar = bounds.getWidth() < 1280;
    const auto iconButtonWidth = tokens.buttonWidth;
    const auto compactButtonWidth = iconButtonWidth + tokens.spacing * 2;
    const auto modeButtonWidth =
        juce::roundToInt(static_cast<float>(tokens.buttonWidth) * 2.2f);

    auto place = [&toolbar, tokens](juce::Button &button, int width) {
      auto slot = toolbar.removeFromLeft(width);
      const auto controlHeight =
          juce::jmin(tokens.controlHeight, slot.getHeight());
      const auto controlY =
          slot.getY() + (slot.getHeight() - controlHeight) / 2;
      button.setBounds(slot.withY(controlY).withHeight(controlHeight));
      toolbar.removeFromLeft(tokens.spacing);
    };

    auto addSeparator = [&toolbar, this, tokens]() {
      toolbar.removeFromLeft(tokens.spacing);
      separatorXs.push_back(toolbar.getX());
      toolbar.removeFromLeft(tokens.spacing * 2);
    };

    for (auto &entry : createButtons)
      place(*entry.button, iconButtonWidth);

    addSeparator();
    place(undoButton, iconButtonWidth);
    place(redoButton, iconButtonWidth);
    place(deleteSelected, iconButtonWidth);
    place(groupSelected, iconButtonWidth);
    place(ungroupSelected, iconButtonWidth);
    place(arrangeMenuButton, iconButtonWidth);

    addSeparator();
    place(dumpJsonButton, compactToolbar ? compactButtonWidth
                                         : compactButtonWidth + tokens.spacing);
    place(exportJuceButton, compactToolbar
                                ? compactButtonWidth
                                : compactButtonWidth + tokens.spacing);

    addSeparator();
    place(previewModeButton, modeButtonWidth);
    place(runModeButton, modeButtonWidth);
    place(previewBindingSimToggle, modeButtonWidth + tokens.spacing);
    place(gridSnapMenuButton, compactToolbar
                                  ? compactButtonWidth
                                  : compactButtonWidth + tokens.spacing);

    addSeparator();
    place(leftDockButton, compactButtonWidth);
    place(rightDockButton, compactButtonWidth);
    place(uiMenuButton, compactButtonWidth);

    shortcutHint.setBounds(toolbar);

    auto content = bounds.reduced(tokens.spacing * 2);
    lastContentBounds = content;

    const auto minLeft = juce::roundToInt(200.0f * uiScaleFactor());
    const auto maxLeft = juce::jmax(
        minLeft, juce::jmin(juce::roundToInt(420.0f * uiScaleFactor()),
                            juce::roundToInt(content.getWidth() * 0.62f)));
    const auto minRight = juce::roundToInt(250.0f * uiScaleFactor());
    const auto maxRight = juce::jmax(
        minRight, juce::jmin(juce::roundToInt(520.0f * uiScaleFactor()),
                             juce::roundToInt(content.getWidth() * 0.68f)));

    profile.leftWidth = juce::jlimit(minLeft, maxLeft, profile.leftWidth);
    profile.rightWidth = juce::jlimit(minRight, maxRight, profile.rightWidth);

    const auto historyMax =
        juce::jmax(tokens.historyCollapsed + tokens.spacing * 4,
                   juce::roundToInt(content.getHeight() * 0.40f));
    profile.historyHeight =
        juce::jlimit(tokens.historyCollapsed + tokens.spacing * 2, historyMax,
                     profile.historyHeight);

    const auto historyDockHeight = historyPanel.isCollapsed()
                                       ? tokens.historyCollapsed
                                       : profile.historyHeight;
    auto historyBounds = content.removeFromBottom(historyDockHeight);

    panelLayoutTelemetryMs.clear();
    const auto setBoundsWithTelemetry =
        [this](juce::Component &component,
               const juce::Rectangle<int> &nextBounds,
               const juce::String &name) {
          const auto beginMs = juce::Time::getMillisecondCounterHiRes();
          component.setBounds(nextBounds);
          panelLayoutTelemetryMs[name] =
              juce::Time::getMillisecondCounterHiRes() - beginMs;
        };

    if (breakpointClass == BreakpointClass::narrow) {
      profile.leftDockVisible = true;
      profile.rightDockVisible = true;

      setBoundsWithTelemetry(canvas, content, "canvas");
      setBoundsWithTelemetry(historyPanel, historyBounds, "history");

      leftPanels.setVisible(profile.leftOverlayOpen);
      rightPanels.setVisible(profile.rightOverlayOpen);

      if (profile.leftOverlayOpen) {
        auto overlayBounds = content.reduced(tokens.spacing);
        const auto overlayWidth = juce::jlimit(
            minLeft,
            juce::jmax(minLeft, content.getWidth() - tokens.spacing * 8),
            profile.leftWidth);
        overlayBounds.setWidth(overlayWidth);
        setBoundsWithTelemetry(leftPanels, overlayBounds, "leftOverlay");
        leftPanels.toFront(false);
      }

      if (profile.rightOverlayOpen) {
        auto overlayBounds = content.reduced(tokens.spacing);
        const auto overlayWidth = juce::jlimit(
            minRight,
            juce::jmax(minRight, content.getWidth() - tokens.spacing * 8),
            profile.rightWidth);
        overlayBounds.setWidth(overlayWidth);
        overlayBounds.setX(content.getRight() - overlayWidth - tokens.spacing);
        setBoundsWithTelemetry(rightPanels, overlayBounds, "rightOverlay");
        rightPanels.toFront(false);
      }
    } else {
      profile.leftOverlayOpen = false;

      auto canvasBounds = content;
      if (profile.leftDockVisible) {
        auto leftBounds = canvasBounds.removeFromLeft(profile.leftWidth);
        setBoundsWithTelemetry(leftPanels, leftBounds, "leftDock");
        leftPanels.setVisible(true);
      } else {
        leftPanels.setVisible(false);
      }

      if (forceSingleColumnEditorsActive) {
        if (profile.rightOverlayOpen) {
          auto overlayBounds = content.reduced(tokens.spacing);
          const auto overlayWidth = juce::jlimit(
              minRight,
              juce::jmax(minRight, content.getWidth() - tokens.spacing * 8),
              profile.rightWidth);
          overlayBounds.setWidth(overlayWidth);
          overlayBounds.setX(content.getRight() - overlayWidth -
                             tokens.spacing);
          setBoundsWithTelemetry(rightPanels, overlayBounds, "rightOverlay");
          rightPanels.setVisible(true);
          rightPanels.toFront(false);
        } else {
          rightPanels.setVisible(false);
        }
      } else {
        profile.rightOverlayOpen = false;
        if (profile.rightDockVisible) {
          auto rightBounds = canvasBounds.removeFromRight(profile.rightWidth);
          setBoundsWithTelemetry(rightPanels, rightBounds, "rightDock");
          rightPanels.setVisible(true);
        } else {
          rightPanels.setVisible(false);
        }
      }

      setBoundsWithTelemetry(canvas, canvasBounds, "canvas");
      setBoundsWithTelemetry(historyPanel, historyBounds, "history");
    }

    auto navBounds = canvas.getBounds();
    navBounds.setSize(tokens.navSize, tokens.navSize);
    navBounds.setPosition(
        canvas.getRight() - tokens.navSize - tokens.spacing * 4,
        canvas.getBottom() - tokens.navSize - tokens.spacing * 4);
    setBoundsWithTelemetry(navigatorPanel, navBounds, "navigator");

    if (gridSnapPanel.isVisible()) {
      const auto popupWidth = juce::roundToInt(220.0f * uiScaleFactor());
      const auto popupHeight = juce::roundToInt(170.0f * uiScaleFactor());
      gridSnapPanel.setBounds(gridSnapMenuButton.getRight() - popupWidth,
                              tokens.toolbarHeight + tokens.spacing, popupWidth,
                              popupHeight);
    }

    juce::StringArray telemetryTokens;
    for (const auto &[name, elapsedMs] : panelLayoutTelemetryMs)
      telemetryTokens.add(name + "=" + juce::String(elapsedMs, 3) + "ms");
    panelLayoutSummary = telemetryTokens.joinIntoString(", ");

    if (!applyingLayoutForSnapshot)
      saveUiSettings();

    if (!throttledResize)
      refreshViewDiagnosticsPanels();
    else if (!isUpdatePending())
      triggerAsyncUpdate();

    lastLayoutMs = juce::Time::getMillisecondCounterHiRes() - layoutStartMs;
  }
  bool keyPressed(const juce::KeyPress &key) {
    const auto keyCode = key.getKeyCode();
    const auto mods = key.getModifiers();
    const auto *focused = juce::Component::getCurrentlyFocusedComponent();

    if (triggerRegisteredShortcut(key))
      return true;

    if (keyCode == juce::KeyPress::tabKey) {
      if (focused != nullptr && isTextInputComponent(focused))
        return false;
      owner.moveKeyboardFocusToSibling(!mods.isShiftDown());
      return true;
    }
    if (!mods.isAnyModifierKeyDown() && keyCode == juce::KeyPress::returnKey &&
        focused != nullptr) {
      if (auto *focusedButton = dynamic_cast<juce::Button *>(
              const_cast<juce::Component *>(focused))) {
        focusedButton->triggerClick();
        return true;
      }
    }

    if (!mods.isAnyModifierKeyDown() && keyCode == juce::KeyPress::escapeKey) {
      auto &profile = activeLayoutProfile();
      const auto hadOverlays =
          profile.leftOverlayOpen || profile.rightOverlayOpen;
      if (hadOverlays) {
        profile.leftOverlayOpen = false;
        profile.rightOverlayOpen = false;
        owner.resized();
        owner.repaint();
        saveUiSettings();
        return true;
      }

      if (gridSnapPanel.isVisible()) {
        gridSnapPanel.setVisible(false);
        owner.resized();
        return true;
      }
    }

    if (!mods.isAnyModifierKeyDown() && focused != nullptr &&
        !isTextInputComponent(focused) &&
        (keyCode == juce::KeyPress::leftKey ||
         keyCode == juce::KeyPress::rightKey ||
         keyCode == juce::KeyPress::upKey ||
         keyCode == juce::KeyPress::downKey)) {
      const auto step = (keyCode == juce::KeyPress::rightKey ||
                         keyCode == juce::KeyPress::downKey)
                            ? 1
                            : -1;
      auto cycleTabs = [step](juce::TabbedComponent &tabs) {
        const auto count = tabs.getNumTabs();
        if (count <= 0)
          return;
        const auto current = tabs.getCurrentTabIndex();
        auto next = current + step;
        if (next < 0)
          next = count - 1;
        if (next >= count)
          next = 0;
        tabs.setCurrentTabIndex(next, juce::sendNotification);
      };

      const auto isFocusedOnTabHeader = [focused](juce::TabbedComponent &tabs) {
        if (focused == &tabs)
          return true;

        const auto &tabBar = tabs.getTabbedButtonBar();
        const auto *current = focused;
        while (current != nullptr) {
          if (current == &tabBar)
            return true;
          current = current->getParentComponent();
        }
        return false;
      };

      if (isFocusedOnTabHeader(leftPanels)) {
        cycleTabs(leftPanels);
        return true;
      }

      if (isFocusedOnTabHeader(rightPanels)) {
        cycleTabs(rightPanels);
        return true;
      }
    }

    if (previewBindingSimulationActive && !canvas.isRunMode()) {
      if (!mods.isAnyModifierKeyDown() &&
          keyCode == juce::KeyPress::escapeKey) {
        setPreviewBindingSimulation(false);
        return true;
      }

      return true;
    }

    if (canvas.isRunMode()) {
      if (!mods.isAnyModifierKeyDown() &&
          key.getKeyCode() == juce::KeyPress::escapeKey) {
        setEditorMode(Ui::CanvasComponent::InteractionMode::preview);
        return true;
      }
      return canvas.keyPressed(key);
    }

    const auto isLeft = keyCode == juce::KeyPress::leftKey;
    const auto isRight = keyCode == juce::KeyPress::rightKey;
    const auto isUp = keyCode == juce::KeyPress::upKey;
    const auto isDown = keyCode == juce::KeyPress::downKey;
    const auto isH = keyCode == 'h' || keyCode == 'H';
    const auto isV = keyCode == 'v' || keyCode == 'V';
    const auto isOpenBracket = keyCode == '[' || keyCode == '{';
    const auto isCloseBracket = keyCode == ']' || keyCode == '}';
    const auto isDelete = keyCode == juce::KeyPress::deleteKey ||
                          keyCode == juce::KeyPress::backspaceKey;

    if (!mods.isAnyModifierKeyDown() && isDelete) {
      if (deleteCurrentSelection())
        return true;
    }

    if (mods.isCommandDown() && mods.isAltDown()) {
      if (!mods.isShiftDown()) {
        if (isLeft)
          return applyArrangeCommand(ArrangeCommand::alignLeft);
        if (isRight)
          return applyArrangeCommand(ArrangeCommand::alignRight);
        if (isUp)
          return applyArrangeCommand(ArrangeCommand::alignTop);
        if (isDown)
          return applyArrangeCommand(ArrangeCommand::alignBottom);
        if (isH)
          return applyArrangeCommand(ArrangeCommand::alignHCenter);
        if (isV)
          return applyArrangeCommand(ArrangeCommand::alignVCenter);
      } else {
        if (isH)
          return applyArrangeCommand(ArrangeCommand::distributeHorizontalGap);
        if (isV)
          return applyArrangeCommand(ArrangeCommand::distributeVerticalGap);
      }
    }

    if (mods.isCommandDown() && !mods.isAltDown() &&
        (isOpenBracket || isCloseBracket)) {
      Ui::Interaction::LayerMoveCommand command =
          Ui::Interaction::LayerMoveCommand::bringForward;
      if (isCloseBracket)
        command = mods.isShiftDown()
                      ? Ui::Interaction::LayerMoveCommand::bringToFront
                      : Ui::Interaction::LayerMoveCommand::bringForward;
      else
        command = mods.isShiftDown()
                      ? Ui::Interaction::LayerMoveCommand::sendToBack
                      : Ui::Interaction::LayerMoveCommand::sendBackward;

      const auto result = layerOrderEngine.moveSelection(docHandle, command);
      if (result.wasOk()) {
        refreshCanvasAndRequestPanels(true, true, true);
        appendHistoryEntry(
            "Layer Order",
            command == Ui::Interaction::LayerMoveCommand::bringForward
                ? "Bring Forward"
            : command == Ui::Interaction::LayerMoveCommand::sendBackward
                ? "Send Backward"
            : command == Ui::Interaction::LayerMoveCommand::bringToFront
                ? "Bring To Front"
                : "Send To Back");
      } else {
        DBG("[Gyeol] Layer move skipped: " + result.getErrorMessage());
      }

      return true;
    }

    return canvas.keyPressed(key);
  }

private:
  void refreshNavigatorSceneFromDocument() {
    const auto &snapshot = docHandle.snapshot();
    const auto &selection = docHandle.editorState().selection;
    std::unordered_set<WidgetId> selectedWidgetIds(selection.begin(),
                                                   selection.end());

    std::vector<Ui::Panels::NavigatorPanel::SceneItem> items;
    items.reserve(snapshot.widgets.size());
    for (const auto &widget : snapshot.widgets) {
      Ui::Panels::NavigatorPanel::SceneItem item;
      item.bounds = widget.bounds;
      item.selected = selectedWidgetIds.count(widget.id) > 0;
      item.visible = widget.visible;
      item.locked = widget.locked;
      items.push_back(std::move(item));
    }

    navigatorPanel.setScene(canvas.canvasWorldBounds(), std::move(items));
  }

  void refreshViewDiagnosticsPanels() {
    const auto telemetryStartMs = juce::Time::getMillisecondCounterHiRes();

    navigatorPanel.setViewState(canvas.visibleWorldBounds(),
                                canvas.currentZoomLevel());

    const auto visWorld = canvas.visibleWorldBounds();
    const auto canWorld = canvas.canvasWorldBounds();
    const bool shouldShowNavigator =
        !lowEndRenderingMode && (canWorld.getWidth() > visWorld.getWidth() ||
                                 canWorld.getHeight() > visWorld.getHeight());
    navigatorPanel.setVisible(shouldShowNavigator);

    Ui::Panels::PerformancePanel::Snapshot perfSnapshot;
    const auto &perf = canvas.performanceStats();
    const auto &documentSnapshot = docHandle.snapshot();
    perfSnapshot.refreshCount = perf.refreshCount;
    perfSnapshot.paintCount = perf.paintCount;
    perfSnapshot.selectionSyncCount = perf.selectionSyncCount;
    perfSnapshot.dragPreviewUpdateCount = perf.dragPreviewUpdateCount;
    perfSnapshot.refreshRequestedPartialRepaintCount =
        perf.refreshRequestedPartialRepaintCount;
    perfSnapshot.refreshRequestedFullRepaintCount =
        perf.refreshRequestedFullRepaintCount;
    perfSnapshot.selectionSyncRequestedPartialRepaintCount =
        perf.selectionSyncRequestedPartialRepaintCount;
    perfSnapshot.lastRefreshMs = perf.lastRefreshMs;
    perfSnapshot.maxRefreshMs = perf.maxRefreshMs;
    perfSnapshot.lastPaintMs = perf.lastPaintMs;
    perfSnapshot.maxPaintMs = perf.maxPaintMs;
    perfSnapshot.lastSelectionSyncMs = perf.lastSelectionSyncMs;
    perfSnapshot.maxSelectionSyncMs = perf.maxSelectionSyncMs;
    perfSnapshot.lastDirtyAreaPx = perf.lastDirtyAreaPx;
    perfSnapshot.widgetViewCount = perf.lastWidgetViewCount;
    perfSnapshot.selectionCount = perf.lastSelectionCount;
    perfSnapshot.documentWidgetCount =
        static_cast<int>(documentSnapshot.widgets.size());
    perfSnapshot.documentGroupCount =
        static_cast<int>(documentSnapshot.groups.size());
    perfSnapshot.documentLayerCount =
        static_cast<int>(documentSnapshot.layers.size());
    perfSnapshot.documentAssetCount =
        static_cast<int>(documentSnapshot.assets.size());
    perfSnapshot.zoomLevel = canvas.currentZoomLevel();
    perfSnapshot.viewOriginWorld = canvas.currentViewOriginWorld();
    perfSnapshot.visibleWorldBounds = canvas.visibleWorldBounds();

    perfSnapshot.editorToolbarPaintMs = lastToolbarPaintMs;
    perfSnapshot.editorLayoutMs = lastLayoutMs;
    perfSnapshot.editorTelemetryMs = lastTelemetryMs;
    perfSnapshot.editorResizeThrottleCount = resizeThrottleCount;
    perfSnapshot.lowEndRenderingMode = lowEndRenderingMode;
    perfSnapshot.responsiveBreakpoint = breakpointName(breakpointClass);
    perfSnapshot.densityPreset = densityName(densityPreset);
    perfSnapshot.themeVariant = themeName(themeVariant);
    perfSnapshot.panelLayoutSummary = panelLayoutSummary;
    perfSnapshot.panelPaintSummary =
        "toolbar=" + juce::String(lastToolbarPaintMs, 3) + "ms";

    perfSnapshot.deferredRefreshRequestCount = deferredRefreshRequestCount;
    perfSnapshot.deferredRefreshCoalescedCount = deferredRefreshCoalescedCount;
    perfSnapshot.deferredRefreshFlushCount = deferredRefreshFlushCount;

    performancePanel.setSnapshot(perfSnapshot);

    lastTelemetryMs =
        juce::Time::getMillisecondCounterHiRes() - telemetryStartMs;
  }
  void refreshAllPanelsFromDocument() {
    canvas.refreshFromDocument();
    layerTreePanel.refreshFromDocument();
    widgetLibraryPanel.refreshFromRegistry();
    assetsPanel.refreshFromDocument();
    refreshNavigatorSceneFromDocument();
    eventActionPanel.refreshFromDocument();
    validationPanel.refreshValidation();
    exportPreviewPanel.markDirty();
    refreshViewDiagnosticsPanels();
    refreshToolbarState();
    syncInspectorTargetFromState();
    historyPanel.setStackState(docHandle.undoDepth(), docHandle.redoDepth(),
                               docHandle.historySerial());
    lastDocumentDigest = computeDocumentDigest();
  }

public:
  void refreshFromDocument() { refreshAllPanelsFromDocument(); }

private:
  enum class ArrangeCommand {
    alignLeft,
    alignRight,
    alignTop,
    alignBottom,
    alignHCenter,
    alignVCenter,
    distributeHorizontalGap,
    distributeVerticalGap
  };

  struct ArrangeUnit {
    WidgetId id = kRootId;
    NodeKind kind = NodeKind::widget;
    juce::Rectangle<float> bounds;
    std::vector<WidgetId> memberWidgetIds;
  };

  const WidgetModel *findWidgetById(const DocumentModel &document,
                                    WidgetId id) const noexcept {
    const auto it = std::find_if(
        document.widgets.begin(), document.widgets.end(),
        [id](const WidgetModel &widget) { return widget.id == id; });
    return it != document.widgets.end() ? &(*it) : nullptr;
  }

  const GroupModel *findGroupById(const DocumentModel &document,
                                  WidgetId id) const noexcept {
    const auto it =
        std::find_if(document.groups.begin(), document.groups.end(),
                     [id](const GroupModel &group) { return group.id == id; });
    return it != document.groups.end() ? &(*it) : nullptr;
  }

  std::vector<WidgetId>
  collectGroupWidgetIdsRecursive(const DocumentModel &document,
                                 WidgetId groupId) const {
    std::vector<WidgetId> collected;
    std::unordered_set<WidgetId> visitedGroups;
    std::unordered_set<WidgetId> seenWidgets;

    std::function<void(WidgetId)> visit = [&](WidgetId id) {
      if (!visitedGroups.insert(id).second)
        return;

      const auto *group = findGroupById(document, id);
      if (group == nullptr)
        return;

      for (const auto widgetId : group->memberWidgetIds) {
        if (seenWidgets.insert(widgetId).second)
          collected.push_back(widgetId);
      }

      for (const auto childGroupId : group->memberGroupIds)
        visit(childGroupId);

      for (const auto &candidate : document.groups) {
        if (candidate.parentGroupId.has_value() &&
            *candidate.parentGroupId == id)
          visit(candidate.id);
      }
    };

    visit(groupId);
    return collected;
  }

  std::optional<juce::Rectangle<float>>
  unionBoundsForWidgets(const DocumentModel &document,
                        const std::vector<WidgetId> &widgetIds) const {
    std::optional<juce::Rectangle<float>> bounds;
    for (const auto widgetId : widgetIds) {
      const auto *widget = findWidgetById(document, widgetId);
      if (widget == nullptr)
        continue;

      if (!bounds.has_value())
        bounds = widget->bounds;
      else
        bounds = bounds->getUnion(widget->bounds);
    }

    return bounds;
  }

  std::vector<ArrangeUnit> buildArrangeUnits() const {
    const auto &document = docHandle.snapshot();
    const auto &selection = docHandle.editorState().selection;
    if (selection.empty())
      return {};

    std::unordered_set<WidgetId> selectedWidgetIds(selection.begin(),
                                                   selection.end());

    struct GroupCandidate {
      WidgetId groupId = kRootId;
      std::optional<WidgetId> parentGroupId;
      juce::Rectangle<float> bounds;
      std::vector<WidgetId> members;
    };

    std::vector<GroupCandidate> candidates;
    candidates.reserve(document.groups.size());
    for (const auto &group : document.groups) {
      const auto members = collectGroupWidgetIdsRecursive(document, group.id);
      if (members.size() < 2)
        continue;

      const auto fullySelected =
          std::all_of(members.begin(), members.end(), [&](WidgetId widgetId) {
            return selectedWidgetIds.count(widgetId) > 0;
          });
      if (!fullySelected)
        continue;

      const auto bounds = unionBoundsForWidgets(document, members);
      if (!bounds.has_value())
        continue;

      GroupCandidate candidate;
      candidate.groupId = group.id;
      candidate.parentGroupId = group.parentGroupId;
      candidate.bounds = *bounds;
      candidate.members = members;
      candidates.push_back(std::move(candidate));
    }

    std::unordered_set<WidgetId> candidateGroupIds;
    candidateGroupIds.reserve(candidates.size());
    for (const auto &candidate : candidates)
      candidateGroupIds.insert(candidate.groupId);

    std::vector<ArrangeUnit> units;
    std::unordered_set<WidgetId> coveredWidgets;

    for (const auto &candidate : candidates) {
      auto parentId = candidate.parentGroupId;
      bool hasSelectedAncestor = false;
      while (parentId.has_value()) {
        if (candidateGroupIds.count(*parentId) > 0) {
          hasSelectedAncestor = true;
          break;
        }

        const auto *parent = findGroupById(document, *parentId);
        if (parent == nullptr)
          break;
        parentId = parent->parentGroupId;
      }

      if (hasSelectedAncestor)
        continue;

      ArrangeUnit unit;
      unit.id = candidate.groupId;
      unit.kind = NodeKind::group;
      unit.bounds = candidate.bounds;
      unit.memberWidgetIds = candidate.members;
      units.push_back(std::move(unit));

      for (const auto memberId : candidate.members)
        coveredWidgets.insert(memberId);
    }

    std::unordered_set<WidgetId> seenWidgetUnits;
    for (const auto widgetId : selection) {
      if (coveredWidgets.count(widgetId) > 0)
        continue;
      if (!seenWidgetUnits.insert(widgetId).second)
        continue;

      const auto *widget = findWidgetById(document, widgetId);
      if (widget == nullptr)
        continue;

      ArrangeUnit unit;
      unit.id = widgetId;
      unit.kind = NodeKind::widget;
      unit.bounds = widget->bounds;
      unit.memberWidgetIds = {widgetId};
      units.push_back(std::move(unit));
    }

    return units;
  }

  bool applyArrangeCommand(ArrangeCommand command) {
    const auto units = buildArrangeUnits();
    if (units.size() < 2)
      return true;

    const auto isDistribute =
        command == ArrangeCommand::distributeHorizontalGap ||
        command == ArrangeCommand::distributeVerticalGap;
    if (isDistribute && units.size() < 3)
      return true;

    std::vector<WidgetModel> arrangeWidgets;
    arrangeWidgets.reserve(units.size());
    for (const auto &unit : units) {
      WidgetModel model;
      model.id = unit.id;
      model.bounds = unit.bounds;
      arrangeWidgets.push_back(std::move(model));
    }

    std::vector<Ui::Interaction::BoundsPatch> patches;
    if (!isDistribute) {
      Ui::Interaction::AlignOptions options;
      options.target = Ui::Interaction::AlignTarget::selectionBounds;

      Ui::Interaction::AlignEdge edge = Ui::Interaction::AlignEdge::left;
      switch (command) {
      case ArrangeCommand::alignLeft:
        edge = Ui::Interaction::AlignEdge::left;
        break;
      case ArrangeCommand::alignRight:
        edge = Ui::Interaction::AlignEdge::right;
        break;
      case ArrangeCommand::alignTop:
        edge = Ui::Interaction::AlignEdge::top;
        break;
      case ArrangeCommand::alignBottom:
        edge = Ui::Interaction::AlignEdge::bottom;
        break;
      case ArrangeCommand::alignHCenter:
        edge = Ui::Interaction::AlignEdge::hCenter;
        break;
      case ArrangeCommand::alignVCenter:
        edge = Ui::Interaction::AlignEdge::vCenter;
        break;
      default:
        break;
      }

      patches = alignDistributeEngine.computeAlignPatches(arrangeWidgets, edge,
                                                          options);
    } else {
      const auto axis = command == ArrangeCommand::distributeHorizontalGap
                            ? Ui::Interaction::DistributeAxis::horizontal
                            : Ui::Interaction::DistributeAxis::vertical;
      patches =
          alignDistributeEngine.computeDistributePatches(arrangeWidgets, axis);
    }

    if (patches.empty())
      return true;

    const auto &document = docHandle.snapshot();
    std::unordered_map<WidgetId, const ArrangeUnit *> unitsById;
    unitsById.reserve(units.size());
    for (const auto &unit : units)
      unitsById[unit.id] = &unit;

    std::unordered_map<WidgetId, juce::Rectangle<float>>
        currentBoundsByWidgetId;
    currentBoundsByWidgetId.reserve(document.widgets.size());
    for (const auto &widget : document.widgets)
      currentBoundsByWidgetId[widget.id] = widget.bounds;

    std::unordered_map<WidgetId, juce::Rectangle<float>> nextBoundsByWidgetId;
    for (const auto &patch : patches) {
      const auto itUnit = unitsById.find(patch.id);
      if (itUnit == unitsById.end() || itUnit->second == nullptr)
        continue;

      const auto *unit = itUnit->second;
      const auto deltaX = patch.bounds.getX() - unit->bounds.getX();
      const auto deltaY = patch.bounds.getY() - unit->bounds.getY();
      constexpr float arrangeEpsilon = 0.0001f;
      if (std::abs(deltaX) <= arrangeEpsilon &&
          std::abs(deltaY) <= arrangeEpsilon)
        continue;

      for (const auto memberWidgetId : unit->memberWidgetIds) {
        const auto itCurrent = currentBoundsByWidgetId.find(memberWidgetId);
        if (itCurrent == currentBoundsByWidgetId.end())
          continue;

        nextBoundsByWidgetId[memberWidgetId] =
            itCurrent->second.translated(deltaX, deltaY);
      }
    }

    if (nextBoundsByWidgetId.empty())
      return true;

    std::vector<WidgetBoundsUpdate> updates;
    updates.reserve(nextBoundsByWidgetId.size());
    for (const auto &[widgetId, bounds] : nextBoundsByWidgetId) {
      WidgetBoundsUpdate update;
      update.id = widgetId;
      update.bounds = bounds;
      updates.push_back(update);
    }

    if (!docHandle.setWidgetsBounds(updates)) {
      DBG("[Gyeol] Arrange command failed to apply bounds updates");
      return true;
    }

    refreshCanvasAndRequestPanels(true, true, true);
    appendHistoryEntry("Arrange", "Selection align/distribute applied");
    return true;
  }

  void showArrangeMenu() {
    enum MenuId {
      kAlignLeftId = 1,
      kAlignRightId = 2,
      kAlignTopId = 3,
      kAlignBottomId = 4,
      kAlignHCenterId = 5,
      kAlignVCenterId = 6,
      kDistHorizontalId = 7,
      kDistVerticalId = 8
    };

    juce::PopupMenu menu;
    menu.addItem(kAlignLeftId, "Align Left\tCtrl/Cmd+Alt+Left");
    menu.addItem(kAlignRightId, "Align Right\tCtrl/Cmd+Alt+Right");
    menu.addItem(kAlignTopId, "Align Top\tCtrl/Cmd+Alt+Up");
    menu.addItem(kAlignBottomId, "Align Bottom\tCtrl/Cmd+Alt+Down");
    menu.addSeparator();
    menu.addItem(kAlignHCenterId, "Align H Center\tCtrl/Cmd+Alt+H");
    menu.addItem(kAlignVCenterId, "Align V Center\tCtrl/Cmd+Alt+V");
    menu.addSeparator();
    menu.addItem(kDistHorizontalId,
                 "Distribute Horizontal Gap\tCtrl/Cmd+Alt+Shift+H");
    menu.addItem(kDistVerticalId,
                 "Distribute Vertical Gap\tCtrl/Cmd+Alt+Shift+V");

    const juce::Component::SafePointer<EditorHandle> safeOwner(&owner);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&arrangeMenuButton),
        [this, safeOwner](int result) {
          if (safeOwner == nullptr || result <= 0)
            return;

          switch (result) {
          case kAlignLeftId:
            applyArrangeCommand(ArrangeCommand::alignLeft);
            break;
          case kAlignRightId:
            applyArrangeCommand(ArrangeCommand::alignRight);
            break;
          case kAlignTopId:
            applyArrangeCommand(ArrangeCommand::alignTop);
            break;
          case kAlignBottomId:
            applyArrangeCommand(ArrangeCommand::alignBottom);
            break;
          case kAlignHCenterId:
            applyArrangeCommand(ArrangeCommand::alignHCenter);
            break;
          case kAlignVCenterId:
            applyArrangeCommand(ArrangeCommand::alignVCenter);
            break;
          case kDistHorizontalId:
            applyArrangeCommand(ArrangeCommand::distributeHorizontalGap);
            break;
          case kDistVerticalId:
            applyArrangeCommand(ArrangeCommand::distributeVerticalGap);
            break;
          default:
            break;
          }
        });
  }

  bool deleteCurrentSelection() {
    if (const auto node = layerTreePanel.selectedNode(); node.has_value()) {
      if (node->kind == NodeKind::layer) {
        DeleteAction action;
        action.kind = NodeKind::layer;
        action.ids = {node->id};
        action.policy = DeleteLayerPolicy{};
        if (!docHandle.deleteNodes(action))
          return false;

        if (activeLayerOverrideId.has_value() &&
            *activeLayerOverrideId == node->id)
          activeLayerOverrideId.reset();

        refreshCanvasAndRequestPanels(true, true, true);
        appendHistoryEntry("Delete Layer", "id=" + juce::String(node->id));
        return true;
      }

      if (node->kind == NodeKind::group) {
        DeleteAction action;
        action.kind = NodeKind::group;
        action.ids = {node->id};
        action.policy = DeleteGroupPolicy{};
        if (!docHandle.deleteNodes(action))
          return false;

        refreshCanvasAndRequestPanels(true, true, true);
        appendHistoryEntry("Delete Group", "id=" + juce::String(node->id));
        return true;
      }
    }

    suppressNextCanvasMutationHistory = true;
    if (canvas.deleteSelection()) {
      appendHistoryEntry("Delete Widget", "Canvas selection");
      return true;
    }

    suppressNextCanvasMutationHistory = false;
    return false;
  }

  std::optional<WidgetId> resolveActiveLayerId() const {
    const auto &layers = docHandle.snapshot().layers;
    if (layers.empty())
      return std::nullopt;

    if (activeLayerOverrideId.has_value()) {
      const auto it = std::find_if(layers.begin(), layers.end(),
                                   [this](const LayerModel &layer) {
                                     return layer.id == *activeLayerOverrideId;
                                   });
      if (it != layers.end())
        return it->id;
    }

    const auto topIt =
        std::max_element(layers.begin(), layers.end(),
                         [](const LayerModel &lhs, const LayerModel &rhs) {
                           if (lhs.order != rhs.order)
                             return lhs.order < rhs.order;
                           return lhs.id < rhs.id;
                         });
    return topIt != layers.end() ? std::optional<WidgetId>{topIt->id}
                                 : std::optional<WidgetId>{};
  }

  Ui::Panels::PropertyPanel::InspectorTarget resolveInspectorTarget() const {
    Ui::Panels::PropertyPanel::InspectorTarget target;

    if (const auto selectedNode = layerTreePanel.selectedNode();
        selectedNode.has_value()) {
      if (selectedNode->kind == NodeKind::layer) {
        target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::layer;
        target.nodeId = selectedNode->id;
        return target;
      }

      if (selectedNode->kind == NodeKind::group) {
        target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::group;
        target.nodeId = selectedNode->id;
        return target;
      }

      const auto &selection = docHandle.editorState().selection;
      if (selection.size() > 1 &&
          std::find(selection.begin(), selection.end(), selectedNode->id) !=
              selection.end()) {
        target.kind =
            Ui::Panels::PropertyPanel::InspectorTargetKind::widgetMulti;
        target.widgetIds = selection;
        return target;
      }

      target.kind =
          Ui::Panels::PropertyPanel::InspectorTargetKind::widgetSingle;
      target.nodeId = selectedNode->id;
      target.widgetIds = {selectedNode->id};
      return target;
    }

    const auto &selection = docHandle.editorState().selection;
    if (selection.empty()) {
      target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::none;
      return target;
    }

    if (selection.size() == 1) {
      target.kind =
          Ui::Panels::PropertyPanel::InspectorTargetKind::widgetSingle;
      target.nodeId = selection.front();
      target.widgetIds = selection;
      return target;
    }

    target.kind = Ui::Panels::PropertyPanel::InspectorTargetKind::widgetMulti;
    target.widgetIds = selection;
    return target;
  }

  void focusWidgetFromAssetUsage(WidgetId widgetId) {
    if (widgetId <= kRootId)
      return;

    const auto &snapshot = docHandle.snapshot();
    const auto exists =
        std::any_of(snapshot.widgets.begin(), snapshot.widgets.end(),
                    [widgetId](const WidgetModel &widget) {
                      return widget.id == widgetId;
                    });
    if (!exists)
      return;

    const auto &selection = docHandle.editorState().selection;
    if (selection.size() != 1 || selection.front() != widgetId)
      docHandle.selectSingle(widgetId);

    refreshCanvasAndRequestPanels(true, true, true);
    canvas.focusWidget(widgetId);
    canvas.grabKeyboardFocus();
  }

  void requestDeferredUiRefresh(bool refreshLayerTree, bool refreshInspector) {
    deferredRefreshRequestCount += 1;
    const auto alreadyPending = pendingLayerTreeRefresh ||
                                pendingInspectorSync ||
                                pendingEventActionSync || pendingAssetsSync;
    pendingLayerTreeRefresh = pendingLayerTreeRefresh || refreshLayerTree;
    pendingInspectorSync = pendingInspectorSync || refreshInspector;
    pendingEventActionSync = pendingEventActionSync || refreshLayerTree;
    pendingAssetsSync = pendingAssetsSync || refreshLayerTree;
    if (alreadyPending)
      deferredRefreshCoalescedCount += 1;
    if ((pendingLayerTreeRefresh || pendingInspectorSync ||
         pendingEventActionSync || pendingAssetsSync) &&
        !isUpdatePending())
      triggerAsyncUpdate();
  }

  void refreshCanvasAndRequestPanels(bool refreshCanvas, bool refreshLayerTree,
                                     bool refreshInspector) {
    if (refreshCanvas) {
      suppressNextCanvasMutationHistory = true;
      canvas.refreshFromDocument();
    }
    refreshToolbarState();
    requestDeferredUiRefresh(refreshLayerTree, refreshInspector);
  }

  void handleAsyncUpdate() override {
    deferredRefreshFlushCount += 1;
    const auto shouldRefreshLayerTree = pendingLayerTreeRefresh;
    const auto shouldSyncInspector = pendingInspectorSync;
    const auto shouldSyncEventAction = pendingEventActionSync;
    const auto shouldSyncAssets = pendingAssetsSync;
    pendingLayerTreeRefresh = false;
    pendingInspectorSync = false;
    pendingEventActionSync = false;
    pendingAssetsSync = false;

    if (shouldRefreshLayerTree)
      layerTreePanel.refreshFromDocument();
    if (shouldSyncInspector)
      syncInspectorTargetFromState();
    if (shouldSyncEventAction)
      eventActionPanel.refreshFromDocument();
    if (shouldSyncAssets)
      assetsPanel.refreshFromDocument();
    if (shouldRefreshLayerTree || shouldSyncInspector ||
        shouldSyncEventAction || shouldSyncAssets)
      refreshNavigatorSceneFromDocument();

    refreshViewDiagnosticsPanels();

    if (deferredRefreshFlushCount % 120 == 0) {
      DBG(juce::String("[Gyeol][Editor][Perf] deferredRefresh flush#") +
          juce::String(static_cast<int64_t>(deferredRefreshFlushCount)) +
          " requests=" +
          juce::String(static_cast<int64_t>(deferredRefreshRequestCount)) +
          " coalesced=" +
          juce::String(static_cast<int64_t>(deferredRefreshCoalescedCount)));
    }
  }

  void syncInspectorTargetFromState() {
    propertyPanel.setInspectorTarget(resolveInspectorTarget());
    propertyPanel.refreshFromDocument();
  }

  struct CreateButtonEntry {
    WidgetType type = WidgetType::button;
    std::unique_ptr<juce::TextButton> button;
  };

  WidgetId createWidgetAtWorldPosition(WidgetType type,
                                       juce::Point<float> origin,
                                       bool applySnap) {
    if (applySnap)
      origin = canvas.snapCreateOrigin(type, origin);

    const auto createdId = widgetFactory.createWidget(docHandle, type, origin,
                                                      resolveActiveLayerId());
    if (createdId <= kRootId)
      return 0;

    if (const auto *descriptor = widgetRegistry.find(type);
        descriptor != nullptr)
      widgetLibraryPanel.noteWidgetCreated(descriptor->typeKey);

    docHandle.selectSingle(createdId);
    refreshCanvasAndRequestPanels(true, true, true);
    appendHistoryEntry("Create Widget", "id=" + juce::String(createdId));
    canvas.grabKeyboardFocus();
    return createdId;
  }

  WidgetId createWidgetAtViewportCenter(WidgetType type) {
    juce::Point<float> origin;
    const auto viewport = canvas.viewportBounds();
    if (!viewport.isEmpty()) {
      const auto viewCenter = viewport.getCentre().toFloat();
      origin = canvas.viewToWorld(viewCenter);
    } else {
      const auto index = static_cast<int>(docHandle.snapshot().widgets.size());
      origin.x = 24.0f + static_cast<float>((index % 10) * 20);
      origin.y = 24.0f + static_cast<float>(((index / 10) % 6) * 20);
    }

    return createWidgetAtWorldPosition(type, origin, false);
  }

  WidgetId createWidgetFromLibrary(
      const juce::String &typeKey,
      std::optional<juce::Point<float>> worldPosition = std::nullopt) {
    if (const auto *descriptor = widgetRegistry.findByKey(typeKey);
        descriptor != nullptr) {
      if (worldPosition.has_value())
        return createWidgetAtWorldPosition(descriptor->type, *worldPosition,
                                           true);
      return createWidgetAtViewportCenter(descriptor->type);
    }

    DBG("[Gyeol][WidgetLibrary] Unknown typeKey create request: " + typeKey);
    return 0;
  }

  void buildCreateButtons() {
    createButtons.clear();
    createButtons.reserve(widgetRegistry.all().size());

    for (const auto &descriptor : widgetRegistry.all()) {
      CreateButtonEntry entry;
      entry.type = descriptor.type;

      juce::String iconText;
      if (descriptor.typeKey == "button")
        iconText = juce::CharPointer_UTF8("\xE2\x96\xA1"); // Square
      else if (descriptor.typeKey == "slider")
        iconText = juce::CharPointer_UTF8("\xE2\x86\x94"); // Left-Right Arrow
      else if (descriptor.typeKey == "meter")
        iconText = juce::CharPointer_UTF8("\xE2\x96\xAE"); // Vertical Rectangle
      else if (descriptor.typeKey == "label")
        iconText = "T";
      else
        iconText = "+";

      entry.button = std::make_unique<juce::TextButton>(iconText);
      const auto name = descriptor.displayName.isNotEmpty()
                            ? descriptor.displayName
                            : descriptor.typeKey;
      entry.button->setTooltip("Add " + name);

      owner.addAndMakeVisible(*entry.button);
      entry.button->onClick = [this, type = entry.type] {
        createWidgetAtViewportCenter(type);
      };

      createButtons.push_back(std::move(entry));
    }
  }

  juce::File resolveProjectRootDirectory() const {
    auto searchDirectory = juce::File::getCurrentWorkingDirectory();

    for (int depth = 0; depth < 10; ++depth) {
      if (searchDirectory.getChildFile("DadeumStudio.jucer").existsAsFile())
        return searchDirectory;

      const auto parent = searchDirectory.getParentDirectory();
      if (parent == searchDirectory)
        break;

      searchDirectory = parent;
    }

    return juce::File::getCurrentWorkingDirectory();
  }

  juce::File
  makeExportOutputDirectory(const juce::File &exportRootDirectory,
                            const juce::String &componentClassName) const {
    const auto safeClassName =
        juce::File::createLegalFileName(componentClassName).trim();
    const auto baseClassName = safeClassName.isNotEmpty()
                                   ? safeClassName
                                   : juce::String("ExportedComponent");
    const auto timestampUtc =
        juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    const auto baseFolderName = baseClassName + "_" + timestampUtc;

    auto candidate = exportRootDirectory.getChildFile(baseFolderName);
    int suffix = 1;
    while (candidate.exists())
      candidate = exportRootDirectory.getChildFile(baseFolderName + "_" +
                                                   juce::String(suffix++));

    return candidate;
  }

  juce::Result createZipPackageFromDirectory(const juce::File &sourceDirectory,
                                             const juce::File &zipFile) const {
    if (!sourceDirectory.exists() || !sourceDirectory.isDirectory())
      return juce::Result::fail("Zip source directory is invalid: " +
                                sourceDirectory.getFullPathName());

    juce::ZipFile::Builder builder;
    bool hasEntries = false;
    const auto rootPrefix = sourceDirectory.getFileName();

    for (const auto &entry : juce::RangedDirectoryIterator(
             sourceDirectory, true, "*", juce::File::findFiles)) {
      const auto file = entry.getFile();
      if (!file.existsAsFile())
        continue;

      auto stored =
          file.getRelativePathFrom(sourceDirectory).replaceCharacter('\\', '/');
      if (stored.isEmpty())
        stored = file.getFileName();
      stored = rootPrefix + "/" + stored;

      builder.addFile(file, 9, stored);
      hasEntries = true;
    }

    if (!hasEntries)
      return juce::Result::fail("No files to package in export directory.");

    if (zipFile.existsAsFile() && !zipFile.deleteFile())
      return juce::Result::fail("Failed to overwrite zip file: " +
                                zipFile.getFullPathName());

    juce::FileOutputStream stream(zipFile);
    if (!stream.openedOk())
      return juce::Result::fail("Failed to create zip stream: " +
                                zipFile.getFullPathName());

    double progress = 0.0;
    if (!builder.writeToStream(stream, &progress))
      return juce::Result::fail("Failed to write zip package.");

    return juce::Result::ok();
  }

  juce::Result
  generateExportPreview(const juce::String &requestedClassName,
                        Ui::Panels::ExportPreviewPanel::PreviewData &outData) {
    const auto projectRoot = resolveProjectRootDirectory();
    const auto className =
        juce::File::createLegalFileName(requestedClassName).trim().isNotEmpty()
            ? juce::File::createLegalFileName(requestedClassName).trim()
            : juce::String("GyeolExportedComponent");

    auto previewRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                           .getChildFile("GyeolExportPreview");
    previewRoot.createDirectory();
    const auto outputDirectory =
        makeExportOutputDirectory(previewRoot, className);

    Export::ExportOptions options;
    options.outputDirectory = outputDirectory;
    options.projectRootDirectory = projectRoot;
    options.componentClassName = className;
    options.overwriteExistingFiles = true;
    options.writeManifestJson = true;

    Export::ExportReport report;
    const auto result = Export::exportToJuceComponent(
        docHandle.snapshot(), widgetRegistry, options, report);
    outData.outputPath = outputDirectory.getFullPathName();
    juce::StringArray assetSummary;
    assetSummary.add("[Assets Summary]");
    assetSummary.add("total: " + juce::String(report.totalAssetCount));
    assetSummary.add("copied: " + juce::String(report.copiedResourceCount));
    assetSummary.add("reused: " + juce::String(report.reusedAssetCount));
    assetSummary.add("skipped: " + juce::String(report.skippedAssetCount));
    assetSummary.add("missing: " + juce::String(report.missingAssetCount));
    assetSummary.add("failed: " + juce::String(report.failedAssetCount));
    outData.reportText =
        assetSummary.joinIntoString("\n") + "\n\n" + report.toText();

    const auto loadTextOrFallback =
        [](const juce::File &file,
           const juce::String &fallback) -> juce::String {
      if (!file.existsAsFile())
        return fallback;
      return file.loadFileAsString();
    };

    if (result.wasOk()) {
      outData.headerText = loadTextOrFallback(report.generatedHeaderFile,
                                              "// header not generated");
      outData.sourceText = loadTextOrFallback(report.generatedSourceFile,
                                              "// source not generated");
      outData.manifestText = loadTextOrFallback(report.manifestFile, "{}");
    } else {
      outData.headerText.clear();
      outData.sourceText.clear();
      outData.manifestText.clear();
    }

    outputDirectory.deleteRecursively();
    return result;
  }

  void runJuceExport(const juce::String &requestedClassName = {}) {
    const auto projectRoot = resolveProjectRootDirectory();
    const auto exportRootDirectory =
        projectRoot.getChildFile("Builds").getChildFile("GyeolExport");
    const auto componentClassName =
        juce::File::createLegalFileName(requestedClassName).trim().isNotEmpty()
            ? juce::File::createLegalFileName(requestedClassName).trim()
            : juce::String("GyeolExportedComponent");
    const auto outputDirectory =
        makeExportOutputDirectory(exportRootDirectory, componentClassName);

    Export::ExportOptions options;
    options.outputDirectory = outputDirectory;
    options.projectRootDirectory = projectRoot;
    options.componentClassName = componentClassName;
    options.overwriteExistingFiles = true;
    options.writeManifestJson = true;

    Export::ExportReport report;
    const auto result = Export::exportToJuceComponent(
        docHandle.snapshot(), widgetRegistry, options, report);

    DBG("[Gyeol] ----- Export Report BEGIN -----");
    DBG(report.toText());
    DBG("[Gyeol] ----- Export Report END -----");

    if (result.failed()) {
      appendHistoryEntry("Export", "Failed: " + result.getErrorMessage());
      juce::NativeMessageBox::showMessageBoxAsync(
          juce::MessageBoxIconType::WarningIcon, "Gyeol Export",
          "Export failed.\n" + result.getErrorMessage() +
              "\n\nOutput: " + outputDirectory.getFullPathName() +
              "\nSee: " + report.reportFile.getFullPathName());
      return;
    }

    const auto zipFile =
        outputDirectory.getSiblingFile(outputDirectory.getFileName() + ".zip");
    const auto zipResult =
        createZipPackageFromDirectory(outputDirectory, zipFile);
    const auto zipSucceeded = zipResult.wasOk();
    if (zipSucceeded)
      appendHistoryEntry("Export Package",
                         "ZIP created: " + zipFile.getFileName());
    else
      appendHistoryEntry("Export Package",
                         "ZIP failed: " + zipResult.getErrorMessage());

    juce::NativeMessageBox::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon, "Gyeol Export",
        "Export complete.\n\nOutput: " + outputDirectory.getFullPathName() +
            "\n\n" + report.generatedHeaderFile.getFileName() + "\n" +
            report.generatedSourceFile.getFileName() + "\n" +
            report.manifestFile.getFileName() + "\n" +
            report.reportFile.getFileName() +
            (zipSucceeded ? ("\n\nPackage ZIP:\n" + zipFile.getFullPathName())
                          : ("\n\nPackage ZIP failed:\n" +
                             zipResult.getErrorMessage())));
    appendHistoryEntry("Export", "Success: " + outputDirectory.getFileName());
    exportPreviewPanel.markDirty();
  }

  void appendHistoryEntry(const juce::String &action,
                          const juce::String &detail) {
    historyPanel.setStackState(docHandle.undoDepth(), docHandle.redoDepth(),
                               docHandle.historySerial());
    historyPanel.appendEntry(action, detail);
    historyPanel.setCanUndoRedo(docHandle.canUndo(), docHandle.canRedo());
  }

  std::uint64_t computeDocumentDigest() const {
    auto hash = static_cast<std::uint64_t>(1469598103934665603ULL);
    const auto mix = [&hash](std::uint64_t value) {
      hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    };
    const auto mixFloat = [&mix](float value) {
      const auto quantized = static_cast<std::int64_t>(
          std::llround(static_cast<double>(value) * 1000.0));
      mix(static_cast<std::uint64_t>(quantized));
    };
    const auto mixBool = [&mix](bool value) { mix(value ? 1ULL : 0ULL); };
    const auto mixString = [&mix](const juce::String &value) {
      mix(static_cast<std::uint64_t>(value.hashCode64()));
    };

    const auto &snapshot = docHandle.snapshot();
    mix(static_cast<std::uint64_t>(snapshot.widgets.size()));
    mix(static_cast<std::uint64_t>(snapshot.groups.size()));
    mix(static_cast<std::uint64_t>(snapshot.layers.size()));

    for (const auto &widget : snapshot.widgets) {
      mix(static_cast<std::uint64_t>(widget.id));
      mix(static_cast<std::uint64_t>(widget.type));
      mixFloat(widget.bounds.getX());
      mixFloat(widget.bounds.getY());
      mixFloat(widget.bounds.getWidth());
      mixFloat(widget.bounds.getHeight());
      mixBool(widget.visible);
      mixBool(widget.locked);
      mixFloat(widget.opacity);

      mix(static_cast<std::uint64_t>(widget.properties.size()));
      for (int i = 0; i < widget.properties.size(); ++i) {
        mixString(widget.properties.getName(i).toString());
        mixString(widget.properties.getValueAt(i).toString());
      }
    }

    for (const auto &group : snapshot.groups) {
      mix(static_cast<std::uint64_t>(group.id));
      mixString(group.name);
      mixBool(group.visible);
      mixBool(group.locked);
      mixFloat(group.opacity);
      mix(static_cast<std::uint64_t>(group.parentGroupId.value_or(kRootId)));
      for (const auto memberId : group.memberWidgetIds)
        mix(static_cast<std::uint64_t>(memberId));
      for (const auto childGroupId : group.memberGroupIds)
        mix(static_cast<std::uint64_t>(childGroupId));
    }

    for (const auto &layer : snapshot.layers) {
      mix(static_cast<std::uint64_t>(layer.id));
      mixString(layer.name);
      mix(static_cast<std::uint64_t>(layer.order));
      mixBool(layer.visible);
      mixBool(layer.locked);
      for (const auto memberId : layer.memberWidgetIds)
        mix(static_cast<std::uint64_t>(memberId));
      for (const auto groupId : layer.memberGroupIds)
        mix(static_cast<std::uint64_t>(groupId));
    }

    return hash;
  }

  void handleCanvasStateChanged() {
    refreshToolbarState();
    requestDeferredUiRefresh(true, true);
    refreshNavigatorSceneFromDocument();
    refreshViewDiagnosticsPanels();

    const auto nextDigest = computeDocumentDigest();
    const auto changed = nextDigest != lastDocumentDigest;
    if (changed) {
      validationPanel.markDirty();
      exportPreviewPanel.markDirty();

      if (!suppressNextCanvasMutationHistory)
        appendHistoryEntry("Canvas Edit", "Direct canvas interaction");
      lastDocumentDigest = nextDigest;
    }

    suppressNextCanvasMutationHistory = false;
  }

  void performUndoFromToolbar() {
    if (canvas.isRunMode())
      return;

    suppressNextCanvasMutationHistory = true;
    if (canvas.performUndo())
      appendHistoryEntry("Undo", "Toolbar");
    else
      suppressNextCanvasMutationHistory = false;
  }

  void performRedoFromToolbar() {
    if (canvas.isRunMode())
      return;

    suppressNextCanvasMutationHistory = true;
    if (canvas.performRedo())
      appendHistoryEntry("Redo", "Toolbar");
    else
      suppressNextCanvasMutationHistory = false;
  }

  void performUndoFromHistoryPanel() {
    if (canvas.isRunMode())
      return;

    suppressNextCanvasMutationHistory = true;
    if (canvas.performUndo())
      appendHistoryEntry("Undo", "History panel");
    else
      suppressNextCanvasMutationHistory = false;
  }

  void performRedoFromHistoryPanel() {
    if (canvas.isRunMode())
      return;

    suppressNextCanvasMutationHistory = true;
    if (canvas.performRedo())
      appendHistoryEntry("Redo", "History panel");
    else
      suppressNextCanvasMutationHistory = false;
  }

  void updateShortcutHintForMode() {
    const auto previewText =
        "Preview: Cmd/Ctrl+K palette  Cmd/Ctrl+P switcher  Del delete  "
        "Cmd/Ctrl+G group  Cmd/Ctrl+Shift+G ungroup  "
        "Cmd/Ctrl+Alt+Arrows/H/V align  Cmd/Ctrl+Alt+Shift+H/V distribute  "
        "Cmd/Ctrl+[ ] layer order  Cmd/Ctrl+Z/Y undo/redo";
    const auto previewSimText = "Preview(Sim Bind): bindings simulated as "
                                "temporary preview. Toggle off to rollback.";
    const auto runText = "Run: interact with widgets, runtimeBindings execute, "
                         "click Run again or Esc to return Preview(rollback)";
    if (canvas.isRunMode())
      shortcutHint.setText(runText, juce::dontSendNotification);
    else if (previewBindingSimulationActive)
      shortcutHint.setText(previewSimText, juce::dontSendNotification);
    else
      shortcutHint.setText(previewText, juce::dontSendNotification);
  }

  void setPreviewBindingSimulation(bool enabled) {
    if (previewBindingSimulationActive == enabled)
      return;

    if (enabled && canvas.isRunMode())
      return;

    if (enabled) {
      if (!docHandle.beginCoalescedEdit(previewBindingSimCoalescedEditKey)) {
        DBG("[Gyeol][PreviewSim] beginCoalescedEdit failed");
        return;
      }
    } else {
      if (!docHandle.endCoalescedEdit(previewBindingSimCoalescedEditKey, false))
        DBG("[Gyeol][PreviewSim] rollback failed");
    }

    previewBindingSimulationActive = enabled;
    canvas.setPreviewBindingSimulationEnabled(enabled);
    canvas.setEnabled(!previewBindingSimulationActive || canvas.isRunMode());

    suppressPreviewBindingToggleCallbacks = true;
    previewBindingSimToggle.setToggleState(previewBindingSimulationActive,
                                           juce::dontSendNotification);
    suppressPreviewBindingToggleCallbacks = false;

    const auto interactionLocked =
        canvas.isRunMode() || previewBindingSimulationActive;
    leftPanels.setEnabled(!interactionLocked);
    rightPanels.setEnabled(!interactionLocked);
    gridSnapPanel.setEnabled(!interactionLocked);

    updateShortcutHintForMode();
    refreshToolbarState();
    refreshAllPanelsFromDocument();
  }

  void setEditorMode(Ui::CanvasComponent::InteractionMode mode) {
    if (mode == Ui::CanvasComponent::InteractionMode::run &&
        previewBindingSimulationActive)
      setPreviewBindingSimulation(false);

    const auto previousMode = canvas.interactionMode();
    const auto modeChanged = previousMode != mode;

    if (modeChanged && mode == Ui::CanvasComponent::InteractionMode::run) {
      if (!docHandle.beginCoalescedEdit(runSessionCoalescedEditKey))
        DBG("[Gyeol][RunMode] beginCoalescedEdit failed");
    } else if (modeChanged &&
               previousMode == Ui::CanvasComponent::InteractionMode::run) {
      if (!docHandle.endCoalescedEdit(runSessionCoalescedEditKey, false))
        DBG("[Gyeol][RunMode] rollback failed");
    }

    canvas.setInteractionMode(mode);
    canvas.setEnabled(!previewBindingSimulationActive || canvas.isRunMode());

    suppressModeToggleCallbacks = true;
    previewModeButton.setToggleState(
        mode == Ui::CanvasComponent::InteractionMode::preview,
        juce::dontSendNotification);
    runModeButton.setToggleState(mode ==
                                     Ui::CanvasComponent::InteractionMode::run,
                                 juce::dontSendNotification);
    suppressModeToggleCallbacks = false;

    const auto interactionLocked =
        canvas.isRunMode() || previewBindingSimulationActive;
    leftPanels.setEnabled(!interactionLocked);
    rightPanels.setEnabled(!interactionLocked);
    gridSnapPanel.setEnabled(!interactionLocked);
    historyPanel.setEnabled(true);

    updateShortcutHintForMode();
    refreshAllPanelsFromDocument();
    refreshToolbarState();
    syncInspectorTargetFromState();
    requestDeferredUiRefresh(false, true);
  }

  static int breakpointStorageKey(BreakpointClass bp) {
    switch (bp) {
    case BreakpointClass::wide:
      return 0;
    case BreakpointClass::desktop:
      return 1;
    case BreakpointClass::tablet:
      return 2;
    case BreakpointClass::narrow:
    default:
      return 3;
    }
  }

  static BreakpointClass classifyBreakpoint(int width) {
    if (width >= 1600)
      return BreakpointClass::wide;
    if (width >= 1200)
      return BreakpointClass::desktop;
    if (width >= 900)
      return BreakpointClass::tablet;
    return BreakpointClass::narrow;
  }

  static juce::String breakpointName(BreakpointClass bp) {
    switch (bp) {
    case BreakpointClass::wide:
      return "wide >=1600";
    case BreakpointClass::desktop:
      return "desktop 1200-1599";
    case BreakpointClass::tablet:
      return "tablet 900-1199";
    case BreakpointClass::narrow:
    default:
      return "narrow <900";
    }
  }

  static juce::String breakpointToken(BreakpointClass bp) {
    switch (bp) {
    case BreakpointClass::wide:
      return "wide";
    case BreakpointClass::desktop:
      return "desktop";
    case BreakpointClass::tablet:
      return "tablet";
    case BreakpointClass::narrow:
    default:
      return "narrow";
    }
  }

  static juce::String densityName(DensityPreset preset) {
    switch (preset) {
    case DensityPreset::compact:
      return "Compact";
    case DensityPreset::comfortable:
      return "Comfortable";
    case DensityPreset::spacious:
    default:
      return "Spacious";
    }
  }

  static juce::String themeName(GyeolThemeVariant variant) {
    switch (variant) {
    case GyeolThemeVariant::deepDark:
      return "Deep Dark";
    case GyeolThemeVariant::light:
      return "Light";
    case GyeolThemeVariant::highContrast:
    default:
      return "High Contrast";
    }
  }

  float uiScaleFactor() const {
    if (uiScaleOverride.has_value())
      return juce::jlimit(1.0f, 2.5f, *uiScaleOverride);

    return juce::jlimit(1.0f, 2.5f, owner.getDesktopScaleFactor());
  }

  DensityTokens currentDensityTokens() const {
    DensityTokens tokens;
    switch (densityPreset) {
    case DensityPreset::compact:
      tokens.spacing = 3;
      tokens.rowHeight = 24;
      tokens.controlHeight = 28;
      tokens.buttonWidth = 32;
      tokens.toolbarHeight = 38;
      tokens.tabDepth = 26;
      tokens.historyExpanded = 150;
      tokens.historyCollapsed = 30;
      tokens.navSize = 132;
      tokens.fontScale = 0.92f;
      break;
    case DensityPreset::comfortable:
      tokens.spacing = 4;
      tokens.rowHeight = 28;
      tokens.controlHeight = 32;
      tokens.buttonWidth = 36;
      tokens.toolbarHeight = 44;
      tokens.tabDepth = 30;
      tokens.historyExpanded = 190;
      tokens.historyCollapsed = 36;
      tokens.navSize = 160;
      tokens.fontScale = 1.0f;
      break;
    case DensityPreset::spacious:
    default:
      tokens.spacing = 6;
      tokens.rowHeight = 32;
      tokens.controlHeight = 36;
      tokens.buttonWidth = 42;
      tokens.toolbarHeight = 50;
      tokens.tabDepth = 34;
      tokens.historyExpanded = 230;
      tokens.historyCollapsed = 40;
      tokens.navSize = 188;
      tokens.fontScale = 1.08f;
      break;
    }

    const auto scale = uiScaleFactor();
    const auto scaled = [scale](int value) {
      return juce::jmax(1, juce::roundToInt(static_cast<float>(value) * scale));
    };

    tokens.spacing = scaled(tokens.spacing);
    tokens.rowHeight = scaled(tokens.rowHeight);
    tokens.controlHeight = scaled(tokens.controlHeight);
    tokens.buttonWidth = scaled(tokens.buttonWidth);
    tokens.toolbarHeight = scaled(tokens.toolbarHeight);
    tokens.tabDepth = scaled(tokens.tabDepth);
    tokens.historyExpanded = scaled(tokens.historyExpanded);
    tokens.historyCollapsed = scaled(tokens.historyCollapsed);
    tokens.navSize = scaled(tokens.navSize);
    tokens.fontScale *= juce::jlimit(1.0f, 1.4f, scale);
    return tokens;
  }

  LayoutProfile defaultLayoutProfileFor(BreakpointClass bp,
                                        juce::Rectangle<int> bounds) const {
    LayoutProfile profile;
    switch (bp) {
    case BreakpointClass::wide:
      profile.leftWidth = 320;
      profile.rightWidth = 400;
      profile.historyHeight = 230;
      break;
    case BreakpointClass::desktop:
      profile.leftWidth = 300;
      profile.rightWidth = 360;
      profile.historyHeight = 200;
      break;
    case BreakpointClass::tablet:
      profile.leftWidth = 240;
      profile.rightWidth = 300;
      profile.historyHeight = 170;
      break;
    case BreakpointClass::narrow:
    default:
      profile.leftWidth =
          juce::roundToInt(static_cast<float>(bounds.getWidth()) * 0.72f);
      profile.rightWidth =
          juce::roundToInt(static_cast<float>(bounds.getWidth()) * 0.72f);
      profile.historyHeight = 140;
      break;
    }

    return profile;
  }

  LayoutProfile &ensureLayoutProfile(BreakpointClass bp,
                                     juce::Rectangle<int> bounds) {
    const auto key = breakpointStorageKey(bp);
    if (auto it = layoutProfiles.find(key); it != layoutProfiles.end())
      return it->second;

    return layoutProfiles.emplace(key, defaultLayoutProfileFor(bp, bounds))
        .first->second;
  }

  LayoutProfile &activeLayoutProfile() {
    return ensureLayoutProfile(breakpointClass, lastContentBounds.isEmpty()
                                                    ? owner.getLocalBounds()
                                                    : lastContentBounds);
  }

  void initializeSettingsFile() {
    if (settingsFile != nullptr)
      return;

    juce::PropertiesFile::Options options;
    options.applicationName = "DadeumStudio";
    options.filenameSuffix = "settings";
    options.folderName = "DadeumStudio";
    options.osxLibrarySubFolder = "Application Support";
    options.commonToAllUsers = false;
    settingsFile = std::make_unique<juce::PropertiesFile>(options);
  }

  void applyThemeState() {
    setGyeolThemeVariant(themeVariant);
    setGyeolReducedMotionEnabled(reducedMotionEnabled);

    if (auto *lf = dynamic_cast<GyeolCustomLookAndFeel *>(
            &juce::LookAndFeel::getDefaultLookAndFeel())) {
      lf->refreshFromTheme();
    }
  }

  void loadUiSettings() {
    initializeSettingsFile();
    if (settingsFile == nullptr || settingsLoaded)
      return;

    const auto themeRaw = settingsFile->getIntValue("ui.themeVariant", 0);
    if (themeRaw >= 0 && themeRaw <= 2)
      themeVariant = static_cast<GyeolThemeVariant>(themeRaw);

    const auto densityRaw = settingsFile->getIntValue("ui.densityPreset", 1);
    if (densityRaw >= 0 && densityRaw <= 2)
      densityPreset = static_cast<DensityPreset>(densityRaw);

    reducedMotionEnabled =
        settingsFile->getBoolValue("ui.reducedMotion", false);
    lowEndRenderingMode =
        settingsFile->getBoolValue("ui.lowEndRendering", false);

    const auto currentBounds = owner.getLocalBounds();
    for (const auto bp : {BreakpointClass::wide, BreakpointClass::desktop,
                          BreakpointClass::tablet, BreakpointClass::narrow}) {
      auto profile = defaultLayoutProfileFor(bp, currentBounds);
      const auto prefix = "layout." + breakpointToken(bp) + ".";
      profile.leftWidth =
          settingsFile->getIntValue(prefix + "leftWidth", profile.leftWidth);
      profile.rightWidth =
          settingsFile->getIntValue(prefix + "rightWidth", profile.rightWidth);
      profile.historyHeight = settingsFile->getIntValue(
          prefix + "historyHeight", profile.historyHeight);
      profile.leftDockVisible = settingsFile->getBoolValue(
          prefix + "leftDockVisible", profile.leftDockVisible);
      profile.rightDockVisible = settingsFile->getBoolValue(
          prefix + "rightDockVisible", profile.rightDockVisible);
      profile.leftOverlayOpen =
          settingsFile->getBoolValue(prefix + "leftOverlayOpen", false);
      profile.rightOverlayOpen =
          settingsFile->getBoolValue(prefix + "rightOverlayOpen", false);
      layoutProfiles[breakpointStorageKey(bp)] = profile;
    }

    settingsLoaded = true;
  }

  void saveUiSettings() {
    initializeSettingsFile();
    if (settingsFile == nullptr)
      return;

    settingsFile->setValue("ui.themeVariant", static_cast<int>(themeVariant));
    settingsFile->setValue("ui.densityPreset", static_cast<int>(densityPreset));
    settingsFile->setValue("ui.reducedMotion", reducedMotionEnabled);
    settingsFile->setValue("ui.lowEndRendering", lowEndRenderingMode);

    for (const auto &[key, profile] : layoutProfiles) {
      BreakpointClass bp = BreakpointClass::desktop;
      switch (key) {
      case 0:
        bp = BreakpointClass::wide;
        break;
      case 1:
        bp = BreakpointClass::desktop;
        break;
      case 2:
        bp = BreakpointClass::tablet;
        break;
      case 3:
      default:
        bp = BreakpointClass::narrow;
        break;
      }

      const auto prefix = "layout." + breakpointToken(bp) + ".";
      settingsFile->setValue(prefix + "leftWidth", profile.leftWidth);
      settingsFile->setValue(prefix + "rightWidth", profile.rightWidth);
      settingsFile->setValue(prefix + "historyHeight", profile.historyHeight);
      settingsFile->setValue(prefix + "leftDockVisible",
                             profile.leftDockVisible);
      settingsFile->setValue(prefix + "rightDockVisible",
                             profile.rightDockVisible);
      settingsFile->setValue(prefix + "leftOverlayOpen",
                             profile.leftOverlayOpen);
      settingsFile->setValue(prefix + "rightOverlayOpen",
                             profile.rightOverlayOpen);
    }

    for (const auto &binding : shortcutBindings) {
      const auto key = "shortcut." + binding.id;
      const auto effective = binding.userKey;
      settingsFile->setValue(key, effective.isValid()
                                      ? effective.getTextDescription()
                                      : juce::String());
    }

    settingsFile->saveIfNeeded();
  }

  void loadShortcutOverrides() {
    if (settingsFile == nullptr)
      return;

    for (auto &binding : shortcutBindings) {
      const auto raw = settingsFile->getValue("shortcut." + binding.id, {});
      if (raw.isEmpty()) {
        binding.userKey = {};
        continue;
      }

      const auto parsed = juce::KeyPress::createFromDescription(raw);
      binding.userKey = parsed;
    }
  }

  void applyDensityTypography(const DensityTokens &tokens,
                              bool forceSingleColumnEditors) {
    leftPanels.setTabBarDepth(tokens.tabDepth);
    rightPanels.setTabBarDepth(tokens.tabDepth);
    historyPanel.setRowHeight(tokens.rowHeight);
    propertyPanel.setResponsiveDensity(tokens.rowHeight,
                                       forceSingleColumnEditors);
    shortcutHint.setFont(makePanelFont(owner, 10.5f * tokens.fontScale, false));
  }
  void applyCompactionRules(int width) {
    const auto compactToolbar = width < 1280;
    const auto iconToolbar = width < 1050;

    dumpJsonButton.setButtonText(
        iconToolbar ? "{}" : (compactToolbar ? "JSON" : "{ }"));
    exportJuceButton.setButtonText(
        iconToolbar ? "EX" : (compactToolbar ? "Export" : "?"));
    previewModeButton.setButtonText(
        iconToolbar ? "P" : (compactToolbar ? "Preview" : "? Preview"));
    const auto runModeLabel =
        iconToolbar
            ? juce::String("R")
            : (compactToolbar ? juce::String("Run") : juce::String("Run >"));
    runModeButton.setButtonText(runModeLabel);
    gridSnapMenuButton.setButtonText(
        iconToolbar ? "#" : (compactToolbar ? "Grid" : "# Grid"));
    leftDockButton.setButtonText(iconToolbar ? "L" : "Dock L");
    rightDockButton.setButtonText(
        iconToolbar ? (forceSingleColumnEditorsActive ? "E" : "R")
                    : (forceSingleColumnEditorsActive ? "Edit" : "Dock R"));
    rightDockButton.setTooltip(forceSingleColumnEditorsActive
                                   ? "Toggle editor popover"
                                   : "Toggle right dock");
    uiMenuButton.setButtonText(iconToolbar ? "UI" : "UI");

    if (width < 1200) {
      leftPanels.setTabName(0, "Lyr");
      leftPanels.setTabName(1, "Lib");
      leftPanels.setTabName(2, "Ast");
      rightPanels.setTabName(0, "Insp");
      rightPanels.setTabName(1, "Act");
      rightPanels.setTabName(2, "Val");
      rightPanels.setTabName(3, "Exp");
      rightPanels.setTabName(4, "Perf");
    } else {
      leftPanels.setTabName(0, "Layers");
      leftPanels.setTabName(1, "Library");
      leftPanels.setTabName(2, "Assets");
      rightPanels.setTabName(0, "Inspector");
      rightPanels.setTabName(1, "Event/Action");
      rightPanels.setTabName(2, "Validation");
      rightPanels.setTabName(3, "Export Preview");
      rightPanels.setTabName(4, "Performance");
    }
  }

  void applyAccessibilityMetadata() {
    owner.setTitle("Gyeol Editor");
    owner.setDescription("Responsive editor shell for UI authoring.");

    leftPanels.setTitle("Left Dock");
    leftPanels.setDescription("Layers, widget library, and assets tabs.");
    rightPanels.setTitle("Right Dock");
    rightPanels.setDescription(
        "Inspector, event, validation, export and performance tabs.");
    canvas.setTitle("Canvas");
    canvas.setDescription("Main design surface for widget editing.");

    for (auto &entry : createButtons) {
      if (entry.button != nullptr) {
        entry.button->setTitle("Create Widget");
        entry.button->setDescription(entry.button->getTooltip());
      }
    }

    for (auto *button :
         {&deleteSelected, &groupSelected, &ungroupSelected, &arrangeMenuButton,
          &dumpJsonButton, &exportJuceButton, &undoButton, &redoButton,
          &previewModeButton, &runModeButton, &gridSnapMenuButton,
          &leftDockButton, &rightDockButton, &uiMenuButton}) {
      button->setTitle(button->getTooltip());
      button->setDescription(button->getTooltip());
    }

    previewBindingSimToggle.setTitle("Preview Binding Simulation");
    previewBindingSimToggle.setDescription(
        "Temporarily simulate runtime bindings in preview mode.");
  }

  void applyVirtualizationHints() {
    layerTreePanel.setBufferedToImage(true);
    eventActionPanel.setBufferedToImage(true);
    assetsPanel.setBufferedToImage(true);
    widgetLibraryPanel.setBufferedToImage(true);
    historyPanel.setBufferedToImage(true);
  }

  void updateLowEndRenderingMode() {
    leftPanels.setBufferedToImage(lowEndRenderingMode);
    rightPanels.setBufferedToImage(lowEndRenderingMode);
    navigatorPanel.setBufferedToImage(lowEndRenderingMode);
    gridSnapPanel.setBufferedToImage(lowEndRenderingMode);
    validationPanel.setBufferedToImage(lowEndRenderingMode);
    performancePanel.setBufferedToImage(lowEndRenderingMode);
    canvas.setBufferedToImage(lowEndRenderingMode);

    // Keep list/tree-heavy panels buffered regardless of quality mode.
    layerTreePanel.setBufferedToImage(true);
    eventActionPanel.setBufferedToImage(true);
    assetsPanel.setBufferedToImage(true);
    widgetLibraryPanel.setBufferedToImage(true);
    historyPanel.setBufferedToImage(true);
  }
  void rememberRecentSelection() {
    const auto &selection = docHandle.editorState().selection;
    for (const auto id : selection) {
      recentWidgetIds.erase(
          std::remove(recentWidgetIds.begin(), recentWidgetIds.end(), id),
          recentWidgetIds.end());
      recentWidgetIds.insert(recentWidgetIds.begin(), id);
    }

    if (recentWidgetIds.size() > 20)
      recentWidgetIds.resize(20);
  }
  juce::KeyPress effectiveShortcut(const ShortcutBinding &binding) const {
    return binding.userKey.isValid() ? binding.userKey : binding.defaultKey;
  }

  void cycleDensityPreset() {
    switch (densityPreset) {
    case DensityPreset::compact:
      densityPreset = DensityPreset::comfortable;
      break;
    case DensityPreset::comfortable:
      densityPreset = DensityPreset::spacious;
      break;
    case DensityPreset::spacious:
    default:
      densityPreset = DensityPreset::compact;
      break;
    }

    owner.resized();
    owner.repaint();
    saveUiSettings();
  }

  void toggleDockVisibility(bool leftDock) {
    auto &profile = activeLayoutProfile();
    const auto rightPopoverMode = !leftDock &&
                                  breakpointClass != BreakpointClass::narrow &&
                                  forceSingleColumnEditorsActive;

    if (breakpointClass == BreakpointClass::narrow) {
      if (leftDock) {
        profile.leftOverlayOpen = !profile.leftOverlayOpen;
        if (profile.leftOverlayOpen)
          profile.rightOverlayOpen = false;
      } else {
        profile.rightOverlayOpen = !profile.rightOverlayOpen;
        if (profile.rightOverlayOpen)
          profile.leftOverlayOpen = false;
      }
    } else if (rightPopoverMode) {
      profile.rightOverlayOpen = !profile.rightOverlayOpen;
    } else {
      profile.rightOverlayOpen = false;
      if (leftDock)
        profile.leftDockVisible = !profile.leftDockVisible;
      else
        profile.rightDockVisible = !profile.rightDockVisible;
    }

    owner.resized();
    owner.repaint();
    saveUiSettings();
  }

  void rebuildShortcutBindings() {
    shortcutBindings.clear();

    const auto addBinding =
        [this](juce::String id, juce::String label, juce::String keywords,
               juce::KeyPress defaultKey, std::function<void()> action) {
          ShortcutBinding binding;
          binding.id = std::move(id);
          binding.label = std::move(label);
          binding.keywords = std::move(keywords);
          binding.defaultKey = defaultKey;
          binding.action = std::move(action);
          shortcutBindings.push_back(std::move(binding));
        };

    addBinding("command_palette", "Command Palette",
               "command palette actions search",
               juce::KeyPress('k', juce::ModifierKeys::commandModifier, 0),
               [this] { showUiCommandPalette(false); });
    addBinding("quick_switcher", "Quick Switcher",
               "quick switcher panels widgets",
               juce::KeyPress('p', juce::ModifierKeys::commandModifier, 0),
               [this] { showUiCommandPalette(true); });
    addBinding("toggle_left_dock", "Toggle Left Dock", "left dock overlay",
               juce::KeyPress('1', juce::ModifierKeys::commandModifier, 0),
               [this] { toggleDockVisibility(true); });
    addBinding("toggle_right_dock", "Toggle Right Dock", "right dock overlay",
               juce::KeyPress('2', juce::ModifierKeys::commandModifier, 0),
               [this] { toggleDockVisibility(false); });
    addBinding("cycle_density", "Cycle Density",
               "density compact comfortable spacious",
               juce::KeyPress('.', juce::ModifierKeys::commandModifier, 0),
               [this] { cycleDensityPreset(); });
    addBinding("reduced_motion", "Toggle Reduced Motion",
               "accessibility reduced motion",
               juce::KeyPress('m', juce::ModifierKeys::commandModifier, 0),
               [this] {
                 reducedMotionEnabled = !reducedMotionEnabled;
                 setGyeolReducedMotionEnabled(reducedMotionEnabled);
                 owner.repaint();
                 saveUiSettings();
               });
    addBinding("low_end_mode", "Toggle Low-End Rendering",
               "low end rendering performance",
               juce::KeyPress('l',
                              juce::ModifierKeys::commandModifier |
                                  juce::ModifierKeys::shiftModifier,
                              0),
               [this] {
                 lowEndRenderingMode = !lowEndRenderingMode;
                 updateLowEndRenderingMode();
                 owner.resized();
                 owner.repaint();
                 saveUiSettings();
               });
    addBinding("shortcut_inspector", "Shortcut Conflict Inspector",
               "shortcut conflicts",
               juce::KeyPress('/', juce::ModifierKeys::commandModifier, 0),
               [this] { showShortcutConflictInspector(); });
    addBinding("shortcut_editor", "Shortcut Editor", "shortcut edit remap",
               juce::KeyPress(';', juce::ModifierKeys::commandModifier, 0),
               [this] { openShortcutEditor(); });

    loadShortcutOverrides();
  }

  bool triggerRegisteredShortcut(const juce::KeyPress &key) {
    const auto *focused = juce::Component::getCurrentlyFocusedComponent();
    const auto isTextInputFocused =
        focused != nullptr && isTextInputComponent(focused);

    if (isTextInputFocused && !key.getModifiers().isCommandDown()) {
      return false;
    }

    for (auto &binding : shortcutBindings) {
      const auto effective = effectiveShortcut(binding);
      if (!effective.isValid())
        continue;

      if (effective == key) {
        if (binding.action)
          binding.action();
        return true;
      }
    }

    return false;
  }

  void showShortcutConflictInspector() {
    std::map<juce::String, juce::StringArray> byShortcut;
    for (const auto &binding : shortcutBindings) {
      const auto effective = effectiveShortcut(binding);
      if (!effective.isValid())
        continue;

      byShortcut[shortcutText(effective)].add(binding.label);
    }

    juce::StringArray lines;
    int conflictCount = 0;
    for (const auto &[shortcut, labels] : byShortcut) {
      if (labels.size() <= 1)
        continue;
      ++conflictCount;
      lines.add(shortcut + " => " + labels.joinIntoString(", "));
    }

    if (conflictCount == 0)
      lines.add("No shortcut conflicts detected.");

    juce::NativeMessageBox::showMessageBoxAsync(
        conflictCount > 0 ? juce::MessageBoxIconType::WarningIcon
                          : juce::MessageBoxIconType::InfoIcon,
        "Shortcut Conflict Inspector", lines.joinIntoString("\n"));
  }

  void showShortcutEditorChoices(size_t bindingIndex) {
    if (bindingIndex >= shortcutBindings.size())
      return;

    static const std::array<std::pair<juce::String, juce::KeyPress>, 8>
        kPresetKeys = {
            {{"Ctrl/Cmd+1",
              juce::KeyPress('1', juce::ModifierKeys::commandModifier, 0)},
             {"Ctrl/Cmd+2",
              juce::KeyPress('2', juce::ModifierKeys::commandModifier, 0)},
             {"Ctrl/Cmd+3",
              juce::KeyPress('3', juce::ModifierKeys::commandModifier, 0)},
             {"Ctrl/Cmd+K",
              juce::KeyPress('k', juce::ModifierKeys::commandModifier, 0)},
             {"Ctrl/Cmd+P",
              juce::KeyPress('p', juce::ModifierKeys::commandModifier, 0)},
             {"Ctrl/Cmd+M",
              juce::KeyPress('m', juce::ModifierKeys::commandModifier, 0)},
             {"Ctrl/Cmd+Shift+L",
              juce::KeyPress('l',
                             juce::ModifierKeys::commandModifier |
                                 juce::ModifierKeys::shiftModifier,
                             0)},
             {"Ctrl/Cmd+Shift+U",
              juce::KeyPress('u',
                             juce::ModifierKeys::commandModifier |
                                 juce::ModifierKeys::shiftModifier,
                             0)}}};

    juce::PopupMenu menu;
    int menuId = 1;
    for (const auto &entry : kPresetKeys)
      menu.addItem(menuId++, entry.first);
    menu.addSeparator();
    menu.addItem(100, "Clear custom mapping");

    const juce::Component::SafePointer<EditorHandle> safeOwner(&owner);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&uiMenuButton),
        [this, safeOwner, bindingIndex](int picked) {
          if (safeOwner == nullptr || picked <= 0 ||
              bindingIndex >= shortcutBindings.size())
            return;

          if (picked == 100) {
            shortcutBindings[bindingIndex].userKey = {};
          } else if (picked >= 1 && picked <= 8) {
            static const std::array<juce::KeyPress, 8> kPresetValues = {
                juce::KeyPress('1', juce::ModifierKeys::commandModifier, 0),
                juce::KeyPress('2', juce::ModifierKeys::commandModifier, 0),
                juce::KeyPress('3', juce::ModifierKeys::commandModifier, 0),
                juce::KeyPress('k', juce::ModifierKeys::commandModifier, 0),
                juce::KeyPress('p', juce::ModifierKeys::commandModifier, 0),
                juce::KeyPress('m', juce::ModifierKeys::commandModifier, 0),
                juce::KeyPress('l',
                               juce::ModifierKeys::commandModifier |
                                   juce::ModifierKeys::shiftModifier,
                               0),
                juce::KeyPress('u',
                               juce::ModifierKeys::commandModifier |
                                   juce::ModifierKeys::shiftModifier,
                               0)};
            shortcutBindings[bindingIndex].userKey =
                kPresetValues[static_cast<size_t>(picked - 1)];
          }

          saveUiSettings();
          showShortcutConflictInspector();
        });
  }

  void openShortcutEditor() {
    juce::PopupMenu menu;
    int id = 1;
    for (const auto &binding : shortcutBindings) {
      const auto effective = effectiveShortcut(binding);
      menu.addItem(id++, binding.label + "\t" + shortcutText(effective));
    }

    menu.addSeparator();
    menu.addItem(1000, "Reset all custom shortcuts");

    const juce::Component::SafePointer<EditorHandle> safeOwner(&owner);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&uiMenuButton),
        [this, safeOwner](int picked) {
          if (safeOwner == nullptr || picked <= 0)
            return;

          if (picked == 1000) {
            for (auto &binding : shortcutBindings)
              binding.userKey = {};
            saveUiSettings();
            showShortcutConflictInspector();
            return;
          }

          if (picked >= 1 &&
              picked <= static_cast<int>(shortcutBindings.size())) {
            showShortcutEditorChoices(static_cast<size_t>(picked - 1));
          }
        });
  }

  void showUiCommandPalette(bool quickSwitcher) {
    struct CommandItem {
      juce::String label;
      juce::String hint;
      juce::String searchText;
      std::function<void()> action;
    };

    const auto showPaletteForQuery = [this, quickSwitcher](
                                         const juce::String &query) {
      const auto addCommand = [](std::vector<CommandItem> &list,
                                 juce::String label, juce::String hint,
                                 juce::String searchText,
                                 std::function<void()> action) {
        CommandItem item;
        item.label = std::move(label);
        item.hint = std::move(hint);
        item.searchText = std::move(searchText);
        item.action = std::move(action);
        list.push_back(std::move(item));
      };

      const auto queryLower = query.trim().toLowerCase();

      auto commands = std::make_shared<std::vector<CommandItem>>();
      commands->reserve(256);

      if (!quickSwitcher) {
        for (const auto &binding : shortcutBindings) {
          const auto effective = effectiveShortcut(binding);
          const auto hint = shortcutText(effective);
          addCommand(*commands, binding.label, hint,
                     binding.label + " " + binding.keywords + " " + hint,
                     binding.action);
        }

        addCommand(*commands, "Run Accessibility Audit", {},
                   "accessibility audit quality gate",
                   [this] { runAccessibilityAudit(); });
        addCommand(*commands, "Run HiDPI Validation", {},
                   "hidpi validation scaling",
                   [this] { runHiDpiLayoutValidation(); });
        addCommand(*commands, "Run UI Quality Gate", {},
                   "quality gate ui regression",
                   [this] { runUiQualityGate(); });
        addCommand(*commands, "Capture UI Baseline", {},
                   "snapshot baseline capture",
                   [this] { captureUiBaselineSnapshots(); });
        addCommand(*commands, "Open Quick Switcher", "Ctrl/Cmd+P",
                   "quick switcher panels widgets sections",
                   [this] { showUiCommandPalette(true); });
      } else {
        addCommand(*commands, "Focus Canvas", {}, "canvas center viewport",
                   [this] { canvas.grabKeyboardFocus(); });

        addCommand(*commands, "Panel: Left / Layers", {},
                   "left panel layers tree", [this] {
                     leftPanels.setCurrentTabIndex(0, juce::sendNotification);
                     leftPanels.grabKeyboardFocus();
                   });
        addCommand(*commands, "Panel: Left / Library", {},
                   "left panel widget library", [this] {
                     leftPanels.setCurrentTabIndex(1, juce::sendNotification);
                     leftPanels.grabKeyboardFocus();
                   });
        addCommand(*commands, "Panel: Left / Assets", {}, "left panel assets",
                   [this] {
                     leftPanels.setCurrentTabIndex(2, juce::sendNotification);
                     leftPanels.grabKeyboardFocus();
                   });

        addCommand(*commands, "Panel: Right / Inspector", {},
                   "right panel inspector property", [this] {
                     rightPanels.setCurrentTabIndex(0, juce::sendNotification);
                     rightPanels.grabKeyboardFocus();
                   });
        addCommand(*commands, "Panel: Right / Event/Action", {},
                   "right panel event action runtime", [this] {
                     rightPanels.setCurrentTabIndex(1, juce::sendNotification);
                     rightPanels.grabKeyboardFocus();
                   });
        addCommand(*commands, "Panel: Right / Validation", {},
                   "right panel validation", [this] {
                     rightPanels.setCurrentTabIndex(2, juce::sendNotification);
                     rightPanels.grabKeyboardFocus();
                   });
        addCommand(*commands, "Panel: Right / Export Preview", {},
                   "right panel export preview", [this] {
                     rightPanels.setCurrentTabIndex(3, juce::sendNotification);
                     rightPanels.grabKeyboardFocus();
                   });
        addCommand(*commands, "Panel: Right / Performance", {},
                   "right panel performance", [this] {
                     rightPanels.setCurrentTabIndex(4, juce::sendNotification);
                     rightPanels.grabKeyboardFocus();
                   });

        addCommand(*commands, "Section: Toolbar", {}, "section toolbar top",
                   [this] { uiMenuButton.grabKeyboardFocus(); });
        addCommand(*commands, "Section: Left Dock", {}, "section left dock",
                   [this] { leftPanels.grabKeyboardFocus(); });
        addCommand(*commands, "Section: Right Dock", {}, "section right dock",
                   [this] { rightPanels.grabKeyboardFocus(); });
        addCommand(*commands, "Section: Bottom History", {},
                   "section bottom history",
                   [this] { historyPanel.grabKeyboardFocus(); });

        const auto &snapshot = docHandle.snapshot();
        std::unordered_set<WidgetId> addedWidgetIds;
        addedWidgetIds.reserve(snapshot.widgets.size());

        const auto addWidgetCommand =
            [this, &addCommand, commands,
             &addedWidgetIds](const WidgetModel &widget, bool isRecent) {
              if (!addedWidgetIds.insert(widget.id).second)
                return;

              juce::String typeName = "Widget";
              juce::String typeKey;
              if (const auto *descriptor = widgetRegistry.find(widget.type);
                  descriptor != nullptr) {
                typeName = descriptor->displayName.isNotEmpty()
                               ? descriptor->displayName
                               : descriptor->typeKey;
                typeKey = descriptor->typeKey;
              }

              const auto customName =
                  widget.properties.getWithDefault("name", juce::var())
                      .toString()
                      .trim();
              const auto displayName =
                  customName.isNotEmpty()
                      ? customName
                      : (typeName + " #" + juce::String(widget.id));

              const auto label = "Widget: " + displayName + " (#" +
                                 juce::String(widget.id) + ")";
              const auto hint = isRecent ? juce::String("recent") : typeKey;
              const auto search = label + " " + hint + " widget section panel";

              addCommand(*commands, label, hint, search,
                         [this, widgetId = widget.id] {
                           focusWidgetFromAssetUsage(widgetId);
                         });
            };

        for (const auto recentId : recentWidgetIds) {
          const auto it =
              std::find_if(snapshot.widgets.begin(), snapshot.widgets.end(),
                           [recentId](const WidgetModel &widget) {
                             return widget.id == recentId;
                           });
          if (it != snapshot.widgets.end())
            addWidgetCommand(*it, true);
        }

        for (const auto &widget : snapshot.widgets)
          addWidgetCommand(widget, false);
      }

      if (commands->empty())
        return;

      auto visibleIndices = std::make_shared<std::vector<size_t>>();
      visibleIndices->reserve(commands->size());

      for (size_t i = 0; i < commands->size(); ++i) {
        const auto &command = (*commands)[i];
        const auto searchable =
            (command.label + " " + command.hint + " " + command.searchText)
                .toLowerCase();
        if (queryLower.isEmpty() || searchable.contains(queryLower))
          visibleIndices->push_back(i);
      }

      if (visibleIndices->empty()) {
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            quickSwitcher ? "Quick Switcher" : "Command Palette",
            "No matching result.");
        return;
      }

      if (!queryLower.isEmpty() && visibleIndices->size() == 1) {
        auto &command = (*commands)[visibleIndices->front()];
        if (command.action)
          command.action();
        return;
      }

      juce::PopupMenu menu;
      auto menuToCommand = std::make_shared<std::vector<size_t>>();
      menuToCommand->reserve(visibleIndices->size());

      constexpr int kMaxVisibleCommands = 96;
      int menuId = 1;
      for (const auto index : *visibleIndices) {
        if (menuId > kMaxVisibleCommands)
          break;

        const auto &command = (*commands)[index];
        const auto text = command.hint.isNotEmpty()
                              ? (command.label + "\t" + command.hint)
                              : command.label;
        menu.addItem(menuId++, text);
        menuToCommand->push_back(index);
      }

      if (visibleIndices->size() > static_cast<size_t>(kMaxVisibleCommands)) {
        menu.addSeparator();
        const auto hiddenCount =
            static_cast<int>(visibleIndices->size()) - kMaxVisibleCommands;
        menu.addItem(100000,
                     "... " + juce::String(hiddenCount) +
                         " more results (refine query)",
                     false, false);
      }

      const juce::Component::SafePointer<EditorHandle> safeOwner(&owner);
      menu.showMenuAsync(
          juce::PopupMenu::Options().withTargetComponent(
              quickSwitcher ? static_cast<juce::Component *>(&rightDockButton)
                            : static_cast<juce::Component *>(&uiMenuButton)),
          [commands, menuToCommand, safeOwner](int picked) {
            if (safeOwner == nullptr || picked <= 0)
              return;

            const auto menuIndex = static_cast<size_t>(picked - 1);
            if (menuIndex >= menuToCommand->size())
              return;

            const auto commandIndex = (*menuToCommand)[menuIndex];
            if (commandIndex >= commands->size())
              return;

            auto &command = (*commands)[commandIndex];
            if (command.action)
              command.action();
          });
    };

    auto *queryDialog = new juce::AlertWindow(
        quickSwitcher ? "Quick Switcher" : "Command Palette",
        quickSwitcher ? "Move to widget/panel/section"
                      : "Search command and run",
        juce::MessageBoxIconType::NoIcon, &owner);
    queryDialog->addTextEditor("query", {},
                               quickSwitcher ? "Type widget/panel/section"
                                             : "Type command keyword");
    queryDialog->addButton("Open", 1,
                           juce::KeyPress(juce::KeyPress::returnKey));
    queryDialog->addButton("Cancel", 0,
                           juce::KeyPress(juce::KeyPress::escapeKey));

    const juce::Component::SafePointer<EditorHandle> safeOwner(&owner);
    const juce::Component::SafePointer<juce::AlertWindow> safeDialog(
        queryDialog);
    queryDialog->enterModalState(
        true,
        juce::ModalCallbackFunction::create([safeOwner, safeDialog,
                                             showPaletteForQuery](int picked) {
          if (safeOwner == nullptr || safeDialog == nullptr || picked != 1)
            return;

          const auto query = safeDialog->getTextEditorContents("query").trim();
          showPaletteForQuery(query);
        }),
        true);
  }
  static bool saveImageAsPng(const juce::Image &image, const juce::File &file) {
    if (!file.getParentDirectory().exists())
      file.getParentDirectory().createDirectory();

    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
      return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream(image, stream);
  }

  juce::File uiSnapshotRootDirectory() const {
    auto root =
        resolveProjectRootDirectory().getChildFile("Builds").getChildFile(
            "UISnapshots");
    if (!root.exists())
      root.createDirectory();
    return root;
  }

  bool captureSnapshotsToDirectory(const juce::File &rootDirectory,
                                   juce::String &errorMessage) {
    struct Profile {
      const char *name = "desktop";
      int width = 1280;
      int height = 820;
    };

    const std::array<Profile, 4> profiles{{{"wide", 1700, 940},
                                           {"desktop", 1400, 900},
                                           {"tablet", 1024, 820},
                                           {"narrow", 840, 760}}};

    struct Target {
      const char *name = "editor";
      juce::Component *component = nullptr;
    };

    const std::array<Target, 6> targets{{{"editor", &owner},
                                         {"canvas", &canvas},
                                         {"leftDock", &leftPanels},
                                         {"rightDock", &rightPanels},
                                         {"history", &historyPanel},
                                         {"performance", &performancePanel}}};

    const auto originalBounds = owner.getLocalBounds();
    const auto originalBreakpoint = breakpointClass;
    applyingLayoutForSnapshot = true;

    for (const auto &profile : profiles) {
      resized({0, 0, profile.width, profile.height});
      const auto profileDir = rootDirectory.getChildFile(profile.name);
      if (!profileDir.exists() && !profileDir.createDirectory()) {
        errorMessage = "Failed to create snapshot directory: " +
                       profileDir.getFullPathName();
        applyingLayoutForSnapshot = false;
        resized(originalBounds);
        breakpointClass = originalBreakpoint;
        return false;
      }

      for (const auto &target : targets) {
        if (target.component == nullptr)
          continue;
        if (!target.component->isVisible())
          continue;

        const auto image = target.component->createComponentSnapshot(
            target.component->getLocalBounds());
        if (!image.isValid())
          continue;

        const auto outputFile =
            profileDir.getChildFile(juce::String(target.name) + ".png");
        if (!saveImageAsPng(image, outputFile)) {
          errorMessage =
              "Failed to write snapshot: " + outputFile.getFullPathName();
          applyingLayoutForSnapshot = false;
          resized(originalBounds);
          breakpointClass = originalBreakpoint;
          return false;
        }
      }
    }

    applyingLayoutForSnapshot = false;
    resized(originalBounds);
    breakpointClass = originalBreakpoint;
    owner.repaint();
    return true;
  }

  void captureUiBaselineSnapshots() {
    auto baselineDir = uiSnapshotRootDirectory().getChildFile("baseline");
    if (!baselineDir.exists())
      baselineDir.createDirectory();

    juce::String error;
    if (!captureSnapshotsToDirectory(baselineDir, error)) {
      juce::NativeMessageBox::showMessageBoxAsync(
          juce::MessageBoxIconType::WarningIcon, "UI Snapshot Baseline", error);
      return;
    }

    juce::NativeMessageBox::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon, "UI Snapshot Baseline",
        "Baseline snapshots captured under:\n" + baselineDir.getFullPathName());
  }

  static double computeImageDiffRatio(const juce::Image &lhs,
                                      const juce::Image &rhs,
                                      int perChannelTolerance) {
    if (!lhs.isValid() || !rhs.isValid())
      return 1.0;

    if (lhs.getWidth() != rhs.getWidth() || lhs.getHeight() != rhs.getHeight())
      return 1.0;

    juce::Image::BitmapData leftData(lhs, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData rightData(rhs, juce::Image::BitmapData::readOnly);

    const auto width = lhs.getWidth();
    const auto height = lhs.getHeight();
    const auto totalPixels = static_cast<double>(width * height);
    double changedPixels = 0.0;

    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const auto leftPixel = leftData.getPixelColour(x, y);
        const auto rightPixel = rightData.getPixelColour(x, y);

        const auto diffR = std::abs(leftPixel.getRed() - rightPixel.getRed());
        const auto diffG =
            std::abs(leftPixel.getGreen() - rightPixel.getGreen());
        const auto diffB = std::abs(leftPixel.getBlue() - rightPixel.getBlue());
        const auto diffA =
            std::abs(leftPixel.getAlpha() - rightPixel.getAlpha());

        if (diffR > perChannelTolerance || diffG > perChannelTolerance ||
            diffB > perChannelTolerance || diffA > perChannelTolerance) {
          changedPixels += 1.0;
        }
      }
    }

    if (totalPixels <= 0.0)
      return 0.0;

    return changedPixels / totalPixels;
  }

  void runUiQualityGate() {
    const auto root = uiSnapshotRootDirectory();
    const auto baselineDir = root.getChildFile("baseline");
    const auto candidateDir = root.getChildFile("candidate");

    if (!baselineDir.exists()) {
      juce::NativeMessageBox::showMessageBoxAsync(
          juce::MessageBoxIconType::WarningIcon, "UI Quality Gate",
          "No baseline snapshots found. Capture baseline first.");
      return;
    }

    if (candidateDir.exists())
      candidateDir.deleteRecursively();
    candidateDir.createDirectory();

    juce::String captureError;
    if (!captureSnapshotsToDirectory(candidateDir, captureError)) {
      juce::NativeMessageBox::showMessageBoxAsync(
          juce::MessageBoxIconType::WarningIcon, "UI Quality Gate",
          "Candidate snapshot capture failed:\n" + captureError);
      return;
    }

    constexpr double perImageThreshold = 0.015;
    constexpr double aggregateThreshold = 0.010;
    constexpr int pixelTolerance = 14;

    juce::StringArray lines;
    int comparedCount = 0;
    int failedCount = 0;
    double accumulated = 0.0;

    for (const auto &entry : juce::RangedDirectoryIterator(
             baselineDir, true, "*.png", juce::File::findFiles)) {
      const auto baselineFile = entry.getFile();
      const auto relative = baselineFile.getRelativePathFrom(baselineDir);
      const auto candidateFile = candidateDir.getChildFile(relative);
      if (!candidateFile.existsAsFile()) {
        ++failedCount;
        lines.add("Missing candidate: " + relative);
        continue;
      }

      const auto baselineImage = juce::ImageFileFormat::loadFrom(baselineFile);
      const auto candidateImage =
          juce::ImageFileFormat::loadFrom(candidateFile);
      const auto ratio =
          computeImageDiffRatio(baselineImage, candidateImage, pixelTolerance);
      ++comparedCount;
      accumulated += ratio;

      if (ratio > perImageThreshold) {
        ++failedCount;
        lines.add(relative + " diff=" + juce::String(ratio, 4) + " (FAIL)");
      }
    }

    const auto aggregate =
        comparedCount > 0 ? accumulated / comparedCount : 1.0;
    const auto aggregateFailed = aggregate > aggregateThreshold;
    if (aggregateFailed)
      ++failedCount;

    lines.insert(0, "Compared: " + juce::String(comparedCount));
    lines.insert(1, "Aggregate diff: " + juce::String(aggregate, 4));
    lines.insert(2,
                 "Thresholds: per-image=" + juce::String(perImageThreshold, 3) +
                     " aggregate=" + juce::String(aggregateThreshold, 3));

    const auto reportFile = root.getChildFile("ui_quality_gate_report.txt");
    reportFile.replaceWithText(lines.joinIntoString("\n"));

    juce::NativeMessageBox::showMessageBoxAsync(
        failedCount > 0 ? juce::MessageBoxIconType::WarningIcon
                        : juce::MessageBoxIconType::InfoIcon,
        "UI Quality Gate",
        (failedCount > 0 ? "FAILED\n\n" : "PASSED\n\n") +
            lines.joinIntoString("\n") +
            "\n\nReport: " + reportFile.getFullPathName());
  }
  void runHiDpiLayoutValidation() {
    struct Profile {
      const char *name = "desktop";
      int width = 1280;
      int height = 820;
    };

    const std::array<Profile, 4> profiles{{{"wide", 1700, 940},
                                           {"desktop", 1400, 900},
                                           {"tablet", 1024, 820},
                                           {"narrow", 840, 760}}};
    const std::array<float, 3> scales{{1.25f, 1.5f, 2.0f}};

    const auto originalBounds = owner.getLocalBounds();
    const auto originalBreakpoint = breakpointClass;
    const auto originalScaleOverride = uiScaleOverride;
    const auto originalSnapshotFlag = applyingLayoutForSnapshot;

    juce::StringArray failures;
    applyingLayoutForSnapshot = true;

    const auto validateComponent =
        [&failures](juce::Component &component, const juce::String &name,
                    const juce::Rectangle<int> &rootBounds, float scale,
                    const juce::String &profileName) {
          if (!component.isVisible())
            return;

          const auto b = component.getBounds();
          if (b.isEmpty()) {
            failures.add("scale=" + juce::String(scale, 2) + " " + profileName +
                         " " + name + " has empty bounds");
            return;
          }

          if (!rootBounds.contains(b)) {
            failures.add("scale=" + juce::String(scale, 2) + " " + profileName +
                         " " + name + " exceeds viewport bounds");
          }
        };

    for (const auto scale : scales) {
      uiScaleOverride = scale;
      for (const auto &profile : profiles) {
        const juce::Rectangle<int> simulatedBounds{0, 0, profile.width,
                                                   profile.height};
        resized(simulatedBounds);

        const auto tokens = currentDensityTokens();
        if (tokens.controlHeight > tokens.toolbarHeight - tokens.spacing * 2) {
          failures.add("scale=" + juce::String(scale, 2) + " " + profile.name +
                       " controlHeight exceeds toolbar capacity");
        }

        validateComponent(leftPanels, "leftDock", simulatedBounds, scale,
                          profile.name);
        validateComponent(rightPanels, "rightDock", simulatedBounds, scale,
                          profile.name);
        validateComponent(canvas, "canvas", simulatedBounds, scale,
                          profile.name);
        validateComponent(historyPanel, "history", simulatedBounds, scale,
                          profile.name);
        validateComponent(navigatorPanel, "navigator", simulatedBounds, scale,
                          profile.name);
        validateComponent(gridSnapPanel, "gridSnap", simulatedBounds, scale,
                          profile.name);
      }
    }

    uiScaleOverride = originalScaleOverride;
    applyingLayoutForSnapshot = originalSnapshotFlag;
    resized(originalBounds);
    breakpointClass = originalBreakpoint;
    owner.repaint();

    if (failures.isEmpty()) {
      juce::NativeMessageBox::showMessageBoxAsync(
          juce::MessageBoxIconType::InfoIcon, "HiDPI Layout Validation",
          "PASS\nChecked scales: 125%, 150%, 200%\nNo clipping detected.");
      return;
    }

    juce::StringArray summary;
    summary.add("FAIL");
    summary.add("Checked scales: 125%, 150%, 200%");
    summary.add("Issues: " + juce::String(failures.size()));
    for (int i = 0; i < juce::jmin(12, failures.size()); ++i)
      summary.add("- " + failures[i]);

    juce::NativeMessageBox::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon, "HiDPI Layout Validation",
        summary.joinIntoString("\n"));
  }
  void runAccessibilityAudit() {
    int issueCount = 0;
    juce::StringArray lines;

    const auto panelBg = palette(GyeolPalette::PanelBackground);
    const auto textPrimary = palette(GyeolPalette::TextPrimary);
    const auto textContrast = contrastRatio(panelBg, textPrimary);
    if (textContrast < 4.5) {
      ++issueCount;
      lines.add("Contrast: panel/text ratio below 4.5 (" +
                juce::String(textContrast, 2) + ")");
    } else {
      lines.add("Contrast: panel/text ratio " + juce::String(textContrast, 2));
    }

    const auto focusContrast =
        contrastRatio(panelBg, palette(GyeolPalette::BorderActive));
    if (focusContrast < 3.0) {
      ++issueCount;
      lines.add("Focus visibility ratio below 3.0 (" +
                juce::String(focusContrast, 2) + ")");
    } else {
      lines.add("Focus visibility ratio " + juce::String(focusContrast, 2));
    }

    int missingMetadataCount = 0;
    int smallHitTargetCount = 0;
    std::function<void(juce::Component &)> walk = [&](juce::Component &c) {
      if (dynamic_cast<juce::Button *>(&c) != nullptr ||
          dynamic_cast<juce::ComboBox *>(&c) != nullptr ||
          dynamic_cast<juce::TextEditor *>(&c) != nullptr ||
          dynamic_cast<juce::Slider *>(&c) != nullptr) {
        const auto bounds = c.getBounds();
        if (bounds.getWidth() < 28 || bounds.getHeight() < 28)
          ++smallHitTargetCount;

        if (c.getTitle().trim().isEmpty() ||
            c.getDescription().trim().isEmpty())
          ++missingMetadataCount;
      }

      for (int i = 0; i < c.getNumChildComponents(); ++i) {
        if (auto *child = c.getChildComponent(i))
          walk(*child);
      }
    };

    walk(owner);

    if (smallHitTargetCount > 0) {
      issueCount += smallHitTargetCount;
      lines.add("Hit target warnings: " + juce::String(smallHitTargetCount));
    } else {
      lines.add("Hit target checks: passed");
    }

    if (missingMetadataCount > 0) {
      issueCount += missingMetadataCount;
      lines.add("Metadata warnings: " + juce::String(missingMetadataCount));
    } else {
      lines.add("Metadata checks: passed");
    }

    const auto summary = "Accessibility audit completed\n\n" +
                         lines.joinIntoString("\n") +
                         "\n\nIssues: " + juce::String(issueCount);
    juce::NativeMessageBox::showMessageBoxAsync(
        issueCount > 0 ? juce::MessageBoxIconType::WarningIcon
                       : juce::MessageBoxIconType::InfoIcon,
        "UI Accessibility Audit", summary);
  }
  void refreshToolbarState() {
    const auto runMode = canvas.isRunMode();
    const auto interactionLocked = runMode || previewBindingSimulationActive;

    for (auto &entry : createButtons) {
      if (entry.button != nullptr)
        entry.button->setEnabled(!interactionLocked);
    }

    deleteSelected.setEnabled(!interactionLocked &&
                              !docHandle.editorState().selection.empty());
    groupSelected.setEnabled(!interactionLocked && canvas.canGroupSelection());
    ungroupSelected.setEnabled(!interactionLocked &&
                               canvas.canUngroupSelection());
    arrangeMenuButton.setEnabled(!interactionLocked &&
                                 docHandle.editorState().selection.size() >= 2);
    dumpJsonButton.setEnabled(!interactionLocked);
    exportJuceButton.setEnabled(!interactionLocked);
    undoButton.setEnabled(!interactionLocked && docHandle.canUndo());
    redoButton.setEnabled(!interactionLocked && docHandle.canRedo());
    historyPanel.setCanUndoRedo(!interactionLocked && docHandle.canUndo(),
                                !interactionLocked && docHandle.canRedo());
    previewBindingSimToggle.setEnabled(!runMode);
    gridSnapMenuButton.setEnabled(!interactionLocked);
    leftDockButton.setEnabled(true);
    rightDockButton.setEnabled(true);
    uiMenuButton.setEnabled(true);
    suppressPreviewBindingToggleCallbacks = true;
    previewBindingSimToggle.setToggleState(previewBindingSimulationActive,
                                           juce::dontSendNotification);
    suppressPreviewBindingToggleCallbacks = false;
    historyPanel.setStackState(docHandle.undoDepth(), docHandle.redoDepth(),
                               docHandle.historySerial());
  }
  EditorHandle &owner;
  static constexpr int toolbarHeight = 44;
  static constexpr int layerPanelWidth = 300;
  static constexpr int rightPanelWidth = 360;
  static constexpr int historyPanelHeight = 190;
  static constexpr const char *runSessionCoalescedEditKey =
      "gyeol.run.preview-session";
  static constexpr const char *previewBindingSimCoalescedEditKey =
      "gyeol.preview.binding-sim";

  DocumentHandle docHandle;
  Widgets::WidgetRegistry widgetRegistry;
  Widgets::WidgetFactory widgetFactory;
  Ui::CanvasComponent canvas;
  Ui::Interaction::AlignDistributeEngine alignDistributeEngine;
  Ui::Interaction::LayerOrderEngine layerOrderEngine;
  Ui::Panels::GridSnapPanel gridSnapPanel;
  Ui::Panels::NavigatorPanel navigatorPanel;
  Ui::Panels::LayerTreePanel layerTreePanel;
  Ui::Panels::WidgetLibraryPanel widgetLibraryPanel;
  Ui::Panels::AssetsPanel assetsPanel;
  juce::TabbedComponent leftPanels;
  juce::TabbedComponent rightPanels;
  Ui::Panels::PropertyPanel propertyPanel;
  Ui::Panels::EventActionPanel eventActionPanel;
  Ui::Panels::ValidationPanel validationPanel;
  Ui::Panels::ExportPreviewPanel exportPreviewPanel;
  Ui::Panels::PerformancePanel performancePanel;
  Ui::Panels::HistoryPanel historyPanel;
  std::optional<WidgetId> activeLayerOverrideId;
  std::uint64_t lastDocumentDigest = 0;
  bool suppressNextCanvasMutationHistory = false;
  std::vector<CreateButtonEntry> createButtons;
  juce::TextButton deleteSelected;
  juce::TextButton groupSelected;
  juce::TextButton ungroupSelected;
  juce::TextButton arrangeMenuButton;
  juce::TextButton dumpJsonButton;
  juce::TextButton exportJuceButton;
  juce::TextButton undoButton;
  juce::TextButton redoButton;
  juce::TextButton previewModeButton;
  juce::TextButton runModeButton;
  juce::ToggleButton previewBindingSimToggle{"Sim Bind"};
  juce::TextButton gridSnapMenuButton{"# Grid"};
  juce::TextButton leftDockButton{"Dock L"};
  juce::TextButton rightDockButton{"Dock R"};
  juce::TextButton uiMenuButton{"UI"};
  juce::Label shortcutHint;
  bool suppressModeToggleCallbacks = false;
  bool suppressPreviewBindingToggleCallbacks = false;
  bool previewBindingSimulationActive = false;
  bool pendingLayerTreeRefresh = false;
  bool pendingInspectorSync = false;
  bool pendingEventActionSync = false;
  bool pendingAssetsSync = false;
  uint64_t deferredRefreshRequestCount = 0;
  uint64_t deferredRefreshCoalescedCount = 0;
  uint64_t deferredRefreshFlushCount = 0;
  enum class BreakpointClass { wide, desktop, tablet, narrow };
  enum class DensityPreset { compact, comfortable, spacious };

  struct DensityTokens {
    int spacing = 4;
    int rowHeight = 28;
    int controlHeight = 32;
    int buttonWidth = 36;
    int toolbarHeight = 44;
    int tabDepth = 30;
    int historyExpanded = 190;
    int historyCollapsed = 36;
    int navSize = 160;
    float fontScale = 1.0f;
  };

  struct LayoutProfile {
    int leftWidth = 300;
    int rightWidth = 360;
    int historyHeight = 190;
    bool leftDockVisible = true;
    bool rightDockVisible = true;
    bool leftOverlayOpen = false;
    bool rightOverlayOpen = false;
  };

  struct ShortcutBinding {
    juce::String id;
    juce::String label;
    juce::String keywords;
    juce::KeyPress defaultKey;
    juce::KeyPress userKey;
    std::function<void()> action;
  };

  BreakpointClass breakpointClass = BreakpointClass::desktop;
  DensityPreset densityPreset = DensityPreset::comfortable;
  GyeolThemeVariant themeVariant = GyeolThemeVariant::deepDark;
  bool reducedMotionEnabled = false;
  bool lowEndRenderingMode = false;
  std::map<int, LayoutProfile> layoutProfiles;
  std::vector<ShortcutBinding> shortcutBindings;
  std::unique_ptr<juce::PropertiesFile> settingsFile;
  uint64_t resizeThrottleCount = 0;
  double lastToolbarPaintMs = 0.0;
  double lastLayoutMs = 0.0;
  double lastTelemetryMs = 0.0;
  double lastResizeMs = 0.0;
  std::map<juce::String, double> panelLayoutTelemetryMs;
  juce::String panelLayoutSummary;
  std::vector<WidgetId> recentWidgetIds;
  bool settingsLoaded = false;
  bool applyingLayoutForSnapshot = false;
  bool forceSingleColumnEditorsActive = false;
  std::optional<float> uiScaleOverride;
  juce::Rectangle<int> lastContentBounds;
  std::vector<int> separatorXs;
};

EditorHandle::EditorHandle() : impl(std::make_unique<Impl>(*this)) {}

EditorHandle::~EditorHandle() = default;

DocumentHandle &EditorHandle::document() noexcept { return impl->document(); }

const DocumentHandle &EditorHandle::document() const noexcept {
  return impl->document();
}

void EditorHandle::refreshFromDocument() { impl->refreshFromDocument(); }

void EditorHandle::paint(juce::Graphics &g) {
  impl->paint(g, getLocalBounds());
}

void EditorHandle::resized() { impl->resized(getLocalBounds()); }

bool EditorHandle::keyPressed(const juce::KeyPress &key) {
  if (impl->keyPressed(key))
    return true;

  return juce::Component::keyPressed(key);
}

std::unique_ptr<EditorHandle> createEditor() {
  return std::make_unique<EditorHandle>();
}
} // namespace Gyeol
