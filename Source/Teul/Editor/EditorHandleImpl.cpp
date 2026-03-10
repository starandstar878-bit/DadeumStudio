#include "Teul/Editor/EditorHandleImpl.h"

#include "Teul/Editor/Canvas/TGraphCanvas.h"
#include "Teul/Editor/Panels/DiagnosticsDrawer.h"
#include "Teul/Editor/Panels/NodeLibraryPanel.h"
#include "Teul/Editor/Panels/NodePropertiesPanel.h"
#include "Teul/Editor/Panels/PresetBrowserPanel.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/History/TCommands.h"
#include "Teul/Serialization/TFileIo.h"
#include "Teul/Serialization/TStatePresetIO.h"

#include <algorithm>

namespace Teul {

namespace {

juce::String teulGraphDisplayName(const TGraphDocument &document) {
  return document.meta.name.isNotEmpty() ? document.meta.name
                                        : juce::String("Untitled");
}

juce::String formatRecoveryCountDiff(const juce::String &label, int currentCount,
                                     int snapshotCount) {
  const int delta = snapshotCount - currentCount;
  juce::String text = label + " " + juce::String(currentCount) + " -> " +
                      juce::String(snapshotCount);
  if (delta > 0)
    text << " (+" << juce::String(delta) << ")";
  else if (delta < 0)
    text << " (" << juce::String(delta) << ")";
  else
    text << " (=)";
  return text;
}

juce::Result buildRecoveryDiffPreview(const TGraphDocument &currentDocument,
                                      const juce::File &recoveryFile,
                                      juce::String &summaryText,
                                      juce::String &detailText,
                                      bool &warning) {
  TGraphDocument recoveryDocument;
  TSchemaMigrationReport migrationReport;
  if (!TFileIo::loadFromFile(recoveryDocument, recoveryFile, &migrationReport))
    return juce::Result::fail(
        "Recovery preview failed: autosave snapshot could not be loaded.");

  const auto currentName = teulGraphDisplayName(currentDocument);
  const auto recoveryName = teulGraphDisplayName(recoveryDocument);
  const bool nameChanged = currentName != recoveryName;
  const bool nodeCountChanged =
      currentDocument.nodes.size() != recoveryDocument.nodes.size();
  const bool connectionCountChanged =
      currentDocument.connections.size() != recoveryDocument.connections.size();
  const bool frameCountChanged =
      currentDocument.frames.size() != recoveryDocument.frames.size();

  summaryText = "Recovery Diff Preview";

  juce::StringArray parts;
  parts.add(formatRecoveryCountDiff("Nodes", (int)currentDocument.nodes.size(),
                                    (int)recoveryDocument.nodes.size()));
  parts.add(formatRecoveryCountDiff(
      "Wires", (int)currentDocument.connections.size(),
      (int)recoveryDocument.connections.size()));
  if (!currentDocument.frames.empty() || !recoveryDocument.frames.empty()) {
    parts.add(formatRecoveryCountDiff("Frames",
                                      (int)currentDocument.frames.size(),
                                      (int)recoveryDocument.frames.size()));
  }

  detailText = parts.joinIntoString("  |  ");
  if (nameChanged)
    detailText << "\r\nGraph: " << currentName << " -> " << recoveryName;
  else
    detailText << "\r\nGraph: " << currentName;

  juce::StringArray compatibilityParts;
  if (migrationReport.migrated)
    compatibilityParts.add("migrated");
  if (migrationReport.usedLegacyAliases)
    compatibilityParts.add("legacy aliases");
  if (migrationReport.degraded)
    compatibilityParts.add("degraded");
  if (!compatibilityParts.isEmpty())
    detailText << "\r\nCompatibility: "
               << compatibilityParts.joinIntoString(" | ");

  if (!migrationReport.warnings.isEmpty())
    detailText << "\r\nWarnings: "
               << migrationReport.warnings.joinIntoString(" | ");

  if (!nameChanged && !nodeCountChanged && !connectionCountChanged &&
      !frameCountChanged && !migrationReport.degraded &&
      migrationReport.warnings.isEmpty()) {
    detailText << "\r\nStructure matches the current document.";
  }

  warning = nameChanged || nodeCountChanged || connectionCountChanged ||
            frameCountChanged || migrationReport.migrated ||
            migrationReport.degraded || !migrationReport.warnings.isEmpty();
  return juce::Result::ok();
}

enum class RailOrientation { vertical, horizontal };

struct RailCardPortView {
  juce::String portId;
  juce::String label;
  juce::Colour accent;
};

struct RailCardView {
  juce::String itemId;
  juce::String title;
  juce::String subtitle;
  juce::String badge;
  juce::Colour accent = juce::Colour(0xff64748b);
  std::vector<RailCardPortView> ports;
  bool warning = false;
  bool groupedStereo = false;
};

TRailKind fallbackRailKind(const juce::String &railId) {
  if (railId == "input-rail")
    return TRailKind::input;
  if (railId == "output-rail")
    return TRailKind::output;
  return TRailKind::controlSource;
}

juce::String compactRailLabel(TRailKind kind) {
  switch (kind) {
  case TRailKind::input:
    return "IN";
  case TRailKind::output:
    return "OUT";
  case TRailKind::controlSource:
    return "CTRL";
  }

  return "RAIL";
}

juce::Colour railAccent(TRailKind kind) {
  switch (kind) {
  case TRailKind::input:
    return juce::Colour(0xff2dd4bf);
  case TRailKind::output:
    return juce::Colour(0xff22d3ee);
  case TRailKind::controlSource:
    return juce::Colour(0xfffbbf24);
  }

  return juce::Colour(0xff94a3b8);
}

juce::Colour endpointAccent(const TSystemRailEndpoint &endpoint) {
  switch (endpoint.kind) {
  case TSystemRailEndpointKind::audioInput:
  case TSystemRailEndpointKind::audioOutput:
    return endpoint.stereo ? juce::Colour(0xff2dd4bf)
                           : juce::Colour(0xff22d3ee);
  case TSystemRailEndpointKind::midiInput:
  case TSystemRailEndpointKind::midiOutput:
    return juce::Colour(0xff34d399);
  }

  return juce::Colour(0xff94a3b8);
}

juce::String endpointBadgeText(const TSystemRailEndpoint &endpoint) {
  switch (endpoint.kind) {
  case TSystemRailEndpointKind::midiInput:
  case TSystemRailEndpointKind::midiOutput:
    return "MIDI";
  case TSystemRailEndpointKind::audioInput:
  case TSystemRailEndpointKind::audioOutput:
    return endpoint.stereo ? "L/R" : "AUDIO";
  }

  return "I/O";
}

juce::Colour controlSourceAccent(TControlSourceKind kind) {
  switch (kind) {
  case TControlSourceKind::expression:
    return juce::Colour(0xfffbbf24);
  case TControlSourceKind::footswitch:
    return juce::Colour(0xfffb923c);
  case TControlSourceKind::trigger:
    return juce::Colour(0xffef4444);
  case TControlSourceKind::midiCc:
  case TControlSourceKind::midiNote:
    return juce::Colour(0xff34d399);
  case TControlSourceKind::macro:
    return juce::Colour(0xff60a5fa);
  }

  return juce::Colour(0xff94a3b8);
}

juce::Colour controlPortAccent(TControlPortKind kind) {
  switch (kind) {
  case TControlPortKind::value:
    return juce::Colour(0xfffbbf24);
  case TControlPortKind::gate:
    return juce::Colour(0xfffb923c);
  case TControlPortKind::trigger:
    return juce::Colour(0xffef4444);
  }

  return juce::Colour(0xff94a3b8);
}

juce::String controlSourceBadgeText(const TControlSource &source) {
  switch (source.kind) {
  case TControlSourceKind::expression:
    return "EXP";
  case TControlSourceKind::footswitch:
    return "FS";
  case TControlSourceKind::trigger:
    return "TRIG";
  case TControlSourceKind::midiCc:
    return "CC";
  case TControlSourceKind::midiNote:
    return "NOTE";
  case TControlSourceKind::macro:
    return "MACRO";
  }

  return "SRC";
}

juce::String describeControlSource(const TControlSource &source) {
  juce::String text;
  switch (source.kind) {
  case TControlSourceKind::expression:
    text = "Continuous value";
    break;
  case TControlSourceKind::footswitch:
    text = source.mode == TControlSourceMode::toggle ? "Toggle footswitch"
                                                      : "Momentary footswitch";
    break;
  case TControlSourceKind::trigger:
    text = "Trigger pulse";
    break;
  case TControlSourceKind::midiCc:
    text = "MIDI CC source";
    break;
  case TControlSourceKind::midiNote:
    text = "MIDI note source";
    break;
  case TControlSourceKind::macro:
    text = "Macro lane";
    break;
  }

  if (source.missing)
    text << " | Missing";
  else if (!source.confirmed)
    text << " | Learn pending";
  else if (source.autoDetected)
    text << " | Auto";

  return text;
}

std::vector<const TSystemRailEndpoint *>
collectRailEndpoints(const std::vector<TSystemRailEndpoint> &endpoints,
                    const juce::String &railId) {
  std::vector<const TSystemRailEndpoint *> result;
  for (const auto &endpoint : endpoints) {
    if (endpoint.railId == railId)
      result.push_back(&endpoint);
  }

  std::sort(result.begin(), result.end(), [](const auto *lhs, const auto *rhs) {
    if (lhs->order != rhs->order)
      return lhs->order < rhs->order;
    return lhs->displayName.compareNatural(rhs->displayName) < 0;
  });
  return result;
}

std::vector<RailCardView> buildRailCards(const TGraphDocument &document,
                                         const juce::String &railId) {
  std::vector<RailCardView> cards;

  if (railId == "input-rail" || railId == "output-rail") {
    const auto endpoints = railId == "input-rail"
                               ? collectRailEndpoints(document.controlState.inputEndpoints,
                                                      railId)
                               : collectRailEndpoints(document.controlState.outputEndpoints,
                                                      railId);
    for (const auto *endpoint : endpoints) {
      RailCardView card;
      card.itemId = endpoint->endpointId;
      card.title = endpoint->displayName;
      card.subtitle = endpoint->subtitle.isNotEmpty()
                          ? endpoint->subtitle
                          : juce::String("System endpoint");
      card.badge = endpointBadgeText(*endpoint);
      card.accent = endpointAccent(*endpoint);
      card.warning = endpoint->missing;
      card.groupedStereo = endpoint->stereo && endpoint->ports.size() >= 2;
      for (const auto &port : endpoint->ports) {
        const auto portAccent = port.dataType == TPortDataType::MIDI
                                    ? juce::Colour(0xff34d399)
                                    : juce::Colour(0xff2dd4bf);
        card.ports.push_back({port.portId, port.displayName, portAccent});
      }
      cards.push_back(std::move(card));
    }
  } else {
    for (const auto &source : document.controlState.sources) {
      if (source.railId != railId)
        continue;

      RailCardView card;
      card.itemId = source.sourceId;
      card.title = source.displayName;
      card.subtitle = describeControlSource(source);
      card.badge = controlSourceBadgeText(source);
      card.accent = controlSourceAccent(source.kind);
      card.warning = source.missing;
      for (const auto &port : source.ports)
        card.ports.push_back({port.portId, port.displayName, controlPortAccent(port.kind)});
      cards.push_back(std::move(card));
    }
  }

  if (!cards.empty())
    return cards;

  RailCardView empty;
  empty.accent = railAccent(fallbackRailKind(railId));
  if (railId == "control-rail") {
    empty.title = "No control sources";
    empty.subtitle = "Learn mode and device profiles will appear here.";
    empty.badge = "LEARN";
  } else {
    empty.title = "No endpoints";
    empty.subtitle = "System I/O endpoints will appear in this rail.";
    empty.badge = "PATCH";
  }
  cards.push_back(std::move(empty));
  return cards;
}

juce::String inputEndpointNodeTypeKey(const TSystemRailEndpoint &endpoint) {
  switch (endpoint.kind) {
  case TSystemRailEndpointKind::audioInput:
    return "Teul.Source.AudioInput";
  case TSystemRailEndpointKind::midiInput:
    return "Teul.Source.MidiInput";
  case TSystemRailEndpointKind::audioOutput:
  case TSystemRailEndpointKind::midiOutput:
    break;
  }

  return {};
}

juce::String inputEndpointPortName(const TSystemRailEndpoint &endpoint,
                                   const juce::String &endpointPortId) {
  if (endpoint.kind == TSystemRailEndpointKind::midiInput)
    return "MIDI Out";

  const auto endpointPort = std::find_if(
      endpoint.ports.begin(), endpoint.ports.end(),
      [&endpointPortId](const TSystemRailPort &port) {
        return port.portId == endpointPortId;
      });
  const auto portLabel = endpointPort != endpoint.ports.end()
                             ? endpointPort->displayName.trim().toUpperCase()
                             : endpointPortId.trim().toUpperCase();

  if (portLabel == "R" || portLabel.contains("RIGHT"))
    return "R Out";

  return "L Out";
}

const TPort *findPortByName(const TNode &node, juce::StringRef portName,
                            TPortDirection direction) {
  for (const auto &port : node.ports) {
    if (port.direction == direction && port.name == portName)
      return &port;
  }

  return nullptr;
}

NodeId findSystemEndpointNodeId(const TGraphDocument &document,
                                const TSystemRailEndpoint &endpoint,
                                juce::StringRef typeKey) {
  for (const auto &node : document.nodes) {
    if (node.typeKey == typeKey && node.label == endpoint.displayName)
      return node.nodeId;
  }

  return kInvalidNodeId;
}

NodeId createNodeFromDescriptor(TGraphDocument &document,
                                const TNodeDescriptor &descriptor,
                                juce::Point<float> worldPos,
                                const juce::String &label) {
  TNode node;
  node.nodeId = document.allocNodeId();
  node.typeKey = descriptor.typeKey;
  node.label = label;
  node.x = worldPos.x;
  node.y = worldPos.y;

  for (const auto &param : descriptor.paramSpecs)
    node.params[param.key] = param.defaultValue;

  for (const auto &portSpec : descriptor.portSpecs) {
    TPort port;
    port.portId = document.allocPortId();
    port.direction = portSpec.direction;
    port.dataType = portSpec.dataType;
    port.name = portSpec.name;
    port.ownerNodeId = node.nodeId;
    if (portSpec.name.startsWithIgnoreCase("R"))
      port.channelIndex = 1;
    node.ports.push_back(port);
  }

  document.executeCommand(std::make_unique<AddNodeCommand>(node));
  return node.nodeId;
}

bool beginInputRailPortDrag(TGraphDocument &document,
                            const TNodeRegistry &registry,
                            TGraphCanvas &canvas,
                            const juce::String &endpointId,
                            const juce::String &endpointPortId,
                            juce::Point<float> sourceAnchorView,
                            juce::Point<float> mousePosView) {
  const auto *endpoint = document.controlState.findEndpoint(endpointId);
  if (endpoint == nullptr || endpoint->railId != "input-rail")
    return false;

  const auto typeKey = inputEndpointNodeTypeKey(*endpoint);
  if (typeKey.isEmpty())
    return false;

  NodeId nodeId = findSystemEndpointNodeId(document, *endpoint, typeKey);
  if (nodeId == kInvalidNodeId) {
    const auto *descriptor = registry.descriptorFor(typeKey);
    if (descriptor == nullptr)
      return false;

    const float canvasHeight = juce::jmax(120.0f, (float)canvas.getHeight());
    const float spawnY = juce::jlimit(72.0f, canvasHeight - 96.0f,
                                      juce::jmax(72.0f, mousePosView.y));
    nodeId = createNodeFromDescriptor(document, *descriptor,
                                      canvas.viewToWorld({72.0f, spawnY}),
                                      endpoint->displayName);
    canvas.rebuildNodeComponents();
  }

  const auto *node = document.findNode(nodeId);
  if (node == nullptr)
    return false;

  const auto portName = inputEndpointPortName(*endpoint, endpointPortId);
  const auto *sourcePort = findPortByName(*node, portName, TPortDirection::Output);
  if (sourcePort == nullptr)
    return false;

  const float anchorX = juce::jlimit(2.0f, juce::jmax(2.0f, (float)canvas.getWidth() - 2.0f),
                                     sourceAnchorView.x);
  const float mouseX = juce::jlimit(-48.0f, (float)canvas.getWidth() + 48.0f,
                                    mousePosView.x);
  canvas.beginConnectionDragFromPoint(*sourcePort, {anchorX, sourceAnchorView.y},
                                      {mouseX, mousePosView.y});
  return true;
}

} // namespace

class RailPanel : public juce::Component {
public:
  using CollapseHandler = std::function<void()>;
  using CardSelectionHandler = std::function<void(const juce::String &cardId)>;
  using PortDragBeginHandler = std::function<bool(const juce::String &cardId,
                                                  const juce::String &portId,
                                                  juce::Point<float> sourceAnchorTarget,
                                                  juce::Point<float> mousePosTarget)>;
  using PortDragMotionHandler =
      std::function<void(juce::Point<float> mousePosTarget)>;

  RailPanel(TGraphDocument &documentIn, juce::String railIdIn,
            RailOrientation orientationIn)
      : document(documentIn), railId(std::move(railIdIn)),
        orientation(orientationIn) {
    addAndMakeVisible(collapseButton);
    setWantsKeyboardFocus(true);
    collapseButton.onClick = [this] { toggleCollapsed(); };
    refreshFromDocument();
  }

  void setCollapseHandler(CollapseHandler handler) {
    collapseHandler = std::move(handler);
  }

  void setCardSelectionHandler(CardSelectionHandler handler) {
    cardSelectionHandler = std::move(handler);
    if (cardSelectionHandler == nullptr) {
      selectedCardId.clear();
      hoveredCardId.clear();
      cardHitZones.clear();
      portHitZones.clear();
    }
    repaint();
  }

  void setPortDragTargetComponent(juce::Component *targetComponent) {
    portDragTargetComponent = targetComponent;
  }

  void setPortDragHandlers(PortDragBeginHandler beginHandler,
                           PortDragMotionHandler updateHandler,
                           PortDragMotionHandler endHandler) {
    portDragBeginHandler = std::move(beginHandler);
    portDragUpdateHandler = std::move(updateHandler);
    portDragEndHandler = std::move(endHandler);
    if (portDragBeginHandler == nullptr) {
      activePortDrag = {};
      portHitZones.clear();
    }
  }

  void setSelectedCardId(const juce::String &cardId) {
    if (selectedCardId == cardId)
      return;

    selectedCardId = cardId;
    repaint();
  }

  void refreshFromDocument() {
    const auto accent = railAccent(currentRailKind());
    const auto collapsed = isRailCollapsed();
    collapseButton.setButtonText(collapseGlyph(collapsed));
    collapseButton.setColour(juce::TextButton::buttonColourId,
                             accent.withAlpha(0.18f));
    collapseButton.setColour(juce::TextButton::buttonOnColourId,
                             accent.withAlpha(0.28f));
    collapseButton.setColour(juce::TextButton::textColourOffId,
                             juce::Colours::white.withAlpha(0.92f));
    collapseButton.setColour(juce::TextButton::textColourOnId,
                             juce::Colours::white.withAlpha(0.97f));
    collapseButton.setTooltip(collapsed ? "Expand rail" : "Collapse rail");
    repaint();
  }

  bool isRailCollapsed() const noexcept {
    if (const auto *rail = document.controlState.findRail(railId))
      return rail->collapsed;
    return false;
  }

  void mouseMove(const juce::MouseEvent &event) override {
    if (isRailCollapsed()) {
      if (hoveredCardId.isNotEmpty()) {
        hoveredCardId.clear();
        repaint();
      }
      return;
    }

    const auto point = event.position.roundToInt();
    juce::String hoveredId;
    for (auto it = cardHitZones.rbegin(); it != cardHitZones.rend(); ++it) {
      if (it->second.contains(point)) {
        hoveredId = it->first;
        break;
      }
    }

    if (hoveredCardId != hoveredId) {
      hoveredCardId = hoveredId;
      repaint();
    }
  }

  void mouseExit(const juce::MouseEvent &) override {
    if (hoveredCardId.isEmpty())
      return;

    hoveredCardId.clear();
    repaint();
  }

  void focusGained(FocusChangeType) override { repaint(); }
  void focusLost(FocusChangeType) override { repaint(); }

  void mouseDown(const juce::MouseEvent &event) override {
    if (isRailCollapsed())
      return;

    grabKeyboardFocus();

    const auto point = event.position;
    for (auto it = portHitZones.rbegin(); it != portHitZones.rend(); ++it) {
      if (!it->bounds.contains(point))
        continue;

      if (cardSelectionHandler != nullptr) {
        if (selectedCardId != it->cardId)
          selectedCardId = it->cardId;
        repaint();
        cardSelectionHandler(it->cardId);
      }

      if (portDragBeginHandler != nullptr && portDragTargetComponent != nullptr) {
        const auto anchorPosTarget = localPointToPortDragTarget(
            it->bounds.getCentre().roundToInt().toFloat());
        const bool started = portDragBeginHandler(
            it->cardId, it->portId, anchorPosTarget,
            localPointToPortDragTarget(event.position));
        if (started) {
          activePortDrag.active = true;
          activePortDrag.cardId = it->cardId;
          activePortDrag.portId = it->portId;
          activePortDrag.anchorPosTarget = anchorPosTarget;
          repaint();
        }
        return;
      }

      return;
    }

    if (cardSelectionHandler == nullptr)
      return;

    const auto pointInt = point.roundToInt();
    for (auto it = cardHitZones.rbegin(); it != cardHitZones.rend(); ++it) {
      if (!it->second.contains(pointInt))
        continue;

      if (selectedCardId != it->first)
        selectedCardId = it->first;
      repaint();
      cardSelectionHandler(it->first);
      return;
    }

    if (selectedCardId.isNotEmpty()) {
      selectedCardId.clear();
      repaint();
      cardSelectionHandler({});
    }
  }

  void mouseDrag(const juce::MouseEvent &event) override {
    if (!activePortDrag.active || portDragUpdateHandler == nullptr ||
        portDragTargetComponent == nullptr)
      return;

    portDragUpdateHandler(localPointToPortDragTarget(event.position));
  }

  void mouseUp(const juce::MouseEvent &event) override {
    if (!activePortDrag.active)
      return;

    if (portDragEndHandler != nullptr && portDragTargetComponent != nullptr)
      portDragEndHandler(localPointToPortDragTarget(event.position));

    activePortDrag = {};
  }

  void paint(juce::Graphics &g) override {
    cardHitZones.clear();
    portHitZones.clear();

    const auto kind = currentRailKind();
    const auto accent = railAccent(kind);
    const auto collapsed = isRailCollapsed();
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    if (bounds.isEmpty())
      return;

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xee111827),
                                           bounds.getCentreX(), bounds.getY(),
                                           juce::Colour(0xee0b1220),
                                           bounds.getCentreX(),
                                           bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, collapsed ? 10.0f : 14.0f);
    g.setColour(accent.withAlpha(collapsed ? 0.48f : 0.72f));
    g.drawRoundedRectangle(bounds, collapsed ? 10.0f : 14.0f, 1.0f);

    auto content = getLocalBounds().reduced(collapsed ? 8 : 12,
                                            collapsed ? 8 : 10);
    auto header = content.removeFromTop(collapsed ? 18 : 24);
    auto titleArea = header;
    titleArea.removeFromRight(32);

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(juce::FontOptions(collapsed ? 10.5f : 12.5f, juce::Font::bold));
    g.drawText(collapsed ? compactRailLabel(kind) : railTitle(), titleArea,
               juce::Justification::centredLeft, false);

    if (collapsed) {
      auto compactArea = content;
      g.setColour(accent.withAlpha(0.78f));
      g.setFont(10.0f);
      g.drawFittedText(compactRailLabel(kind), compactArea,
                       juce::Justification::centred, 2, 0.8f);
      return;
    }

    const auto cards = buildRailCards(document, railId);
    g.setColour(juce::Colours::white.withAlpha(0.52f));
    g.setFont(10.0f);
    g.drawText(metaLabel((int)cards.size()), content.removeFromTop(14),
               juce::Justification::centredLeft, false);
    content.removeFromTop(6);

    if (orientation == RailOrientation::vertical)
      drawVerticalCards(g, content, cards);
    else
      drawHorizontalCards(g, content, cards);
  }

  void resized() override {
    auto buttonArea = getLocalBounds().reduced(8).removeFromTop(22);
    collapseButton.setBounds(buttonArea.removeFromRight(24));
  }

private:
  static void drawBadge(juce::Graphics &g, juce::Rectangle<int> area,
                        const juce::String &text, juce::Colour accent) {
    g.setColour(accent.withAlpha(0.16f));
    g.fillRoundedRectangle(area.toFloat(), 7.0f);
    g.setColour(accent.withAlpha(0.82f));
    g.drawRoundedRectangle(area.toFloat(), 7.0f, 1.0f);
    g.setColour(accent.brighter(0.18f));
    g.setFont(10.0f);
    g.drawText(text, area, juce::Justification::centred, false);
  }

  static void drawSocket(juce::Graphics &g, juce::Rectangle<float> area,
                         juce::Colour accent, bool capOnRight) {
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(area, area.getHeight() * 0.45f);
    g.setColour(accent.withAlpha(0.92f));
    g.drawRoundedRectangle(area, area.getHeight() * 0.45f, 1.0f);

    juce::Rectangle<float> cap;
    if (capOnRight) {
      cap = juce::Rectangle<float>(area.getRight() - 4.0f, area.getY() + 1.0f,
                                   4.0f, area.getHeight() - 2.0f);
    } else {
      cap = juce::Rectangle<float>(area.getX(), area.getY() + 1.0f, 4.0f,
                                   area.getHeight() - 2.0f);
    }

    g.setColour(accent.withAlpha(0.78f));
    g.fillRoundedRectangle(cap, cap.getHeight() * 0.45f);
  }

  static void drawGroupedStereoSocket(juce::Graphics &g,
                                      juce::Rectangle<float> area,
                                      juce::Colour accent,
                                      bool capOnRight) {
    drawSocket(g, area, accent, capOnRight);
    const auto inner = area.reduced(4.0f, 3.0f);
    const float dotSize = juce::jmin(5.0f, inner.getHeight());
    g.setColour(accent.withAlpha(0.92f));
    g.fillEllipse(inner.getX(), inner.getCentreY() - dotSize * 0.5f, dotSize,
                  dotSize);
    g.fillEllipse(inner.getRight() - dotSize,
                  inner.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
  }

  void drawVerticalCards(juce::Graphics &g, juce::Rectangle<int> area,
                         const std::vector<RailCardView> &cards) {
    const bool portsOnRight = currentRailKind() != TRailKind::output;
    const int gap = 8;
    const int count = juce::jmax(1, (int)cards.size());
    const int cardHeight = juce::jlimit(68, 96,
                                        (area.getHeight() - gap * (count - 1)) /
                                            count);

    for (int index = 0; index < count && area.getHeight() >= 58; ++index) {
      const auto cardArea = area.removeFromTop(cardHeight).toFloat();
      area.removeFromTop(gap);
      drawVerticalCard(g, cardArea, cards[(size_t)index], portsOnRight);
    }
  }

  void drawVerticalCard(juce::Graphics &g, juce::Rectangle<float> area,
                        const RailCardView &card, bool portsOnRight) {
    const auto accent = card.warning ? juce::Colour(0xffef4444) : card.accent;
    const bool selected = card.itemId.isNotEmpty() && card.itemId == selectedCardId;
    const bool hovered = card.itemId.isNotEmpty() && card.itemId == hoveredCardId;
    const bool focused = selected && hasKeyboardFocus(true);
    g.setColour(juce::Colour(0xe50f172a));
    g.fillRoundedRectangle(area, 12.0f);
    if (hovered && !selected) {
      g.setColour(accent.withAlpha(0.08f));
      g.fillRoundedRectangle(area.reduced(2.0f), 10.0f);
    }
    g.setColour(accent.withAlpha(selected ? 0.98f : hovered ? 0.86f : 0.72f));
    g.drawRoundedRectangle(area, 12.0f, selected ? 2.0f : hovered ? 1.4f : 1.0f);
    if (focused) {
      g.setColour(juce::Colours::white.withAlpha(0.62f));
      g.drawRoundedRectangle(area.reduced(3.0f), 9.0f, 1.0f);
    }

    auto strip = area;
    strip.setWidth(4.0f);
    strip.setX(portsOnRight ? area.getRight() - 4.0f : area.getX());
    g.setColour(accent.withAlpha(selected ? 0.92f : 0.68f));
    g.fillRoundedRectangle(strip, 2.0f);

    auto content = area.toNearestInt().reduced(10, 8);
    auto titleRow = content.removeFromTop(18);
    auto badgeArea = titleRow.removeFromRight(58);
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(card.title, titleRow, juce::Justification::centredLeft, false);
    drawBadge(g, badgeArea, card.badge, accent);

    g.setColour(juce::Colours::white.withAlpha(0.62f));
    g.setFont(10.5f);
    g.drawFittedText(card.subtitle, content.removeFromTop(28),
                     juce::Justification::topLeft, 2, 0.9f);

    auto portsArea = content.removeFromBottom(26);
    if (card.groupedStereo && card.ports.size() >= 2) {
      auto row = portsArea.removeFromTop(18);
      auto socketArea = portsOnRight ? row.removeFromRight(34)
                                     : row.removeFromLeft(34);
      auto labelArea = row;
      g.setColour(juce::Colours::white.withAlpha(0.82f));
      g.setFont(10.0f);
      g.drawText("L / R", labelArea, juce::Justification::centredLeft, false);
      auto groupedSocketBounds =
          socketArea.toFloat().withSizeKeepingCentre(30.0f, 10.0f);
      drawGroupedStereoSocket(g, groupedSocketBounds, accent, portsOnRight);
      auto leftSocketBounds = groupedSocketBounds;
      auto rightSocketBounds = groupedSocketBounds;
      leftSocketBounds.setWidth(groupedSocketBounds.getWidth() * 0.5f);
      rightSocketBounds.setX(leftSocketBounds.getRight());
      rightSocketBounds.setWidth(groupedSocketBounds.getWidth() * 0.5f);
      addPortHitZone(card.itemId, card.ports[0].portId, leftSocketBounds);
      addPortHitZone(card.itemId, card.ports[1].portId, rightSocketBounds);
    } else {
      const int portCount = juce::jmin(3, (int)card.ports.size());
      if (portCount > 0) {
        const int rowHeight = juce::jmax(16, portsArea.getHeight() / portCount);
        for (int index = 0; index < portCount; ++index) {
          auto row = portsArea.removeFromTop(rowHeight);
          auto socketArea = portsOnRight ? row.removeFromRight(18)
                                         : row.removeFromLeft(18);
          auto labelArea = row;
          g.setColour(juce::Colours::white.withAlpha(0.78f));
          g.setFont(10.0f);
          g.drawText(card.ports[(size_t)index].label, labelArea,
                     juce::Justification::centredLeft, false);
          const auto socketBounds =
              socketArea.toFloat().withSizeKeepingCentre(14.0f, 8.0f);
          drawSocket(g, socketBounds, card.ports[(size_t)index].accent,
                     portsOnRight);
          addPortHitZone(card.itemId, card.ports[(size_t)index].portId,
                         socketBounds);
        }
      }
    }

    if (card.itemId.isNotEmpty())
      cardHitZones.push_back({card.itemId, area.toNearestInt()});
  }

  void drawHorizontalCards(juce::Graphics &g, juce::Rectangle<int> area,
                           const std::vector<RailCardView> &cards) {
    const int gap = 10;
    const int count = juce::jmax(1, (int)cards.size());
    const int cardWidth = juce::jlimit(150, 240,
                                       (area.getWidth() - gap * (count - 1)) /
                                           count);

    for (int index = 0; index < count && area.getWidth() >= 120; ++index) {
      const auto cardArea = area.removeFromLeft(cardWidth).toFloat();
      area.removeFromLeft(gap);
      drawHorizontalCard(g, cardArea, cards[(size_t)index]);
    }
  }

  void drawHorizontalCard(juce::Graphics &g, juce::Rectangle<float> area,
                          const RailCardView &card) {
    const auto accent = card.warning ? juce::Colour(0xffef4444) : card.accent;
    const bool selected = card.itemId.isNotEmpty() && card.itemId == selectedCardId;
    const bool hovered = card.itemId.isNotEmpty() && card.itemId == hoveredCardId;
    const bool focused = selected && hasKeyboardFocus(true);
    g.setColour(juce::Colour(0xe50f172a));
    g.fillRoundedRectangle(area, 13.0f);
    if (hovered && !selected) {
      g.setColour(accent.withAlpha(0.08f));
      g.fillRoundedRectangle(area.reduced(2.0f), 11.0f);
    }
    g.setColour(accent.withAlpha(selected ? 0.98f : hovered ? 0.86f : 0.72f));
    g.drawRoundedRectangle(area, 13.0f, selected ? 2.0f : hovered ? 1.4f : 1.0f);
    if (focused) {
      g.setColour(juce::Colours::white.withAlpha(0.62f));
      g.drawRoundedRectangle(area.reduced(3.0f), 10.0f, 1.0f);
    }

    auto content = area.toNearestInt().reduced(12, 10);
    auto titleRow = content.removeFromTop(18);
    auto badgeArea = titleRow.removeFromRight(60);
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(card.title, titleRow, juce::Justification::centredLeft, false);
    drawBadge(g, badgeArea, card.badge, accent);

    g.setColour(juce::Colours::white.withAlpha(0.64f));
    g.setFont(10.5f);
    g.drawFittedText(card.subtitle, content.removeFromTop(30),
                     juce::Justification::topLeft, 2, 0.9f);

    auto portArea = content.removeFromBottom(18);
    int portX = portArea.getX();
    for (const auto &port : card.ports) {
      const int portWidth = juce::jlimit(44, 72, 18 + port.label.length() * 7);
      if (portX + portWidth > portArea.getRight())
        break;

      juce::Rectangle<int> pill(portX, portArea.getY(), portWidth,
                                portArea.getHeight());
      portX += portWidth + 6;
      g.setColour(port.accent.withAlpha(0.16f));
      g.fillRoundedRectangle(pill.toFloat(), 8.0f);
      g.setColour(port.accent.withAlpha(0.88f));
      g.drawRoundedRectangle(pill.toFloat(), 8.0f, 1.0f);
      g.setColour(port.accent.brighter(0.18f));
      g.setFont(10.0f);
      g.drawText(port.label, pill, juce::Justification::centred, false);
      addPortHitZone(card.itemId, port.portId, pill.toFloat());
    }

    if (card.itemId.isNotEmpty())
      cardHitZones.push_back({card.itemId, area.toNearestInt()});
  }

  void addPortHitZone(const juce::String &cardId, const juce::String &portId,
                      juce::Rectangle<float> bounds) {
    if (cardId.isEmpty() || portId.isEmpty())
      return;

    portHitZones.push_back({cardId, portId, bounds.expanded(4.0f, 5.0f)});
  }

  juce::Point<float> localPointToPortDragTarget(juce::Point<float> localPoint) const {
    if (portDragTargetComponent == nullptr)
      return localPoint;

    return portDragTargetComponent
        ->getLocalPoint(this, localPoint.roundToInt())
        .toFloat();
  }

  juce::String railTitle() const {
    if (const auto *rail = document.controlState.findRail(railId))
      return rail->title;

    switch (currentRailKind()) {
    case TRailKind::input:
      return "Inputs";
    case TRailKind::output:
      return "Outputs";
    case TRailKind::controlSource:
      return "Controls";
    }

    return "Rail";
  }

  juce::String metaLabel(int cardCount) const {
    const auto label = currentRailKind() == TRailKind::controlSource
                           ? "sources"
                           : "endpoints";
    return juce::String(cardCount) + " " + label;
  }

  juce::String collapseGlyph(bool collapsed) const {
    switch (currentRailKind()) {
    case TRailKind::input:
      return collapsed ? ">" : "<";
    case TRailKind::output:
      return collapsed ? "<" : ">";
    case TRailKind::controlSource:
      return collapsed ? "^" : "v";
    }

    return collapsed ? ">" : "<";
  }

  TRailKind currentRailKind() const {
    if (const auto *rail = document.controlState.findRail(railId))
      return rail->kind;
    return fallbackRailKind(railId);
  }

  void toggleCollapsed() {
    document.controlState.ensureDefaultRails();
    if (auto *rail = document.controlState.findRail(railId)) {
      rail->collapsed = !rail->collapsed;
      document.touch(false);
    }

    refreshFromDocument();
    if (collapseHandler != nullptr)
      collapseHandler();
  }

  struct PortHitZone {
    juce::String cardId;
    juce::String portId;
    juce::Rectangle<float> bounds;
  };

  struct ActivePortDragState {
    bool active = false;
    juce::String cardId;
    juce::String portId;
    juce::Point<float> anchorPosTarget;
  };

  TGraphDocument &document;
  juce::String railId;
  RailOrientation orientation;
  juce::TextButton collapseButton;
  CollapseHandler collapseHandler;
  CardSelectionHandler cardSelectionHandler;
  PortDragBeginHandler portDragBeginHandler;
  PortDragMotionHandler portDragUpdateHandler;
  PortDragMotionHandler portDragEndHandler;
  juce::Component *portDragTargetComponent = nullptr;
  juce::String selectedCardId;
  juce::String hoveredCardId;
  ActivePortDragState activePortDrag;
  std::vector<std::pair<juce::String, juce::Rectangle<int>>> cardHitZones;
  std::vector<PortHitZone> portHitZones;
};

class ControlSourceInspectorPanel : public juce::Component {
public:
  ControlSourceInspectorPanel(TGraphDocument &documentIn,
                              const TNodeRegistry &registryIn)
      : document(documentIn), registry(registryIn) {
    addAndMakeVisible(headerLabel);
    addAndMakeVisible(kindLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(portsLabel);
    addAndMakeVisible(assignmentsLabel);
    addAndMakeVisible(portsBox);
    addAndMakeVisible(assignmentsBox);
    addAndMakeVisible(closeButton);

    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.95f));
    headerLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    kindLabel.setJustificationType(juce::Justification::centredLeft);
    kindLabel.setColour(juce::Label::textColourId,
                        juce::Colours::white.withAlpha(0.68f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.58f));

    portsLabel.setText("Ports", juce::dontSendNotification);
    portsLabel.setJustificationType(juce::Justification::centredLeft);
    portsLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.78f));

    assignmentsLabel.setText("Assignments", juce::dontSendNotification);
    assignmentsLabel.setJustificationType(juce::Justification::centredLeft);
    assignmentsLabel.setColour(juce::Label::textColourId,
                               juce::Colours::white.withAlpha(0.78f));

    configureReadOnlyBox(portsBox);
    configureReadOnlyBox(assignmentsBox);

    closeButton.setButtonText("Hide");
    closeButton.onClick = [this] { hidePanel(); };
    setVisible(false);
  }

  void setLayoutChangedCallback(std::function<void()> callback) {
    onLayoutChanged = std::move(callback);
  }

  bool isPanelOpen() const noexcept {
    return isVisible() && currentSourceId.isNotEmpty();
  }

  const juce::String &selectedSourceId() const noexcept { return currentSourceId; }

  void inspectSource(const juce::String &sourceId) {
    const auto normalized = sourceId.trim();
    if (normalized.isEmpty()) {
      hidePanel();
      return;
    }

    const bool wasOpen = isPanelOpen();
    const bool changed = currentSourceId != normalized;
    currentSourceId = normalized;
    setVisible(true);
    refreshFromDocument();

    if ((!wasOpen || changed) && onLayoutChanged != nullptr)
      onLayoutChanged();
  }

  void refreshFromDocument() {
    if (currentSourceId.isEmpty())
      return;

    const auto *source = document.controlState.findSource(currentSourceId);
    if (source == nullptr) {
      hidePanel();
      return;
    }

    headerLabel.setText(source->displayName, juce::dontSendNotification);
    kindLabel.setText(sourceKindLabel(source->kind) + " / " +
                          sourceModeLabel(source->mode),
                      juce::dontSendNotification);
    statusLabel.setText(buildStatusLine(*source), juce::dontSendNotification);
    portsBox.setText(buildPortSummary(*source), false);
    assignmentsBox.setText(buildAssignmentSummary(*source), false);
    repaint();
  }

  void hidePanel() {
    if (!isPanelOpen() && currentSourceId.isEmpty())
      return;

    currentSourceId.clear();
    setVisible(false);
    if (onLayoutChanged != nullptr)
      onLayoutChanged();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xee111827),
                                           bounds.getCentreX(), bounds.getY(),
                                           juce::Colour(0xee0b1220),
                                           bounds.getCentreX(),
                                           bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0x66fbbf24));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12, 10);
    auto header = area.removeFromTop(24);
    closeButton.setBounds(header.removeFromRight(54));
    headerLabel.setBounds(header);
    kindLabel.setBounds(area.removeFromTop(18));
    statusLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);
    portsLabel.setBounds(area.removeFromTop(18));
    portsBox.setBounds(area.removeFromTop(116));
    area.removeFromTop(8);
    assignmentsLabel.setBounds(area.removeFromTop(18));
    assignmentsBox.setBounds(area);
  }

private:
  static void configureReadOnlyBox(juce::TextEditor &box) {
    box.setMultiLine(true);
    box.setReadOnly(true);
    box.setScrollbarsShown(true);
    box.setColour(juce::TextEditor::backgroundColourId,
                  juce::Colour(0x66111827));
    box.setColour(juce::TextEditor::outlineColourId,
                  juce::Colour(0xff324154));
    box.setColour(juce::TextEditor::textColourId,
                  juce::Colours::white.withAlpha(0.82f));
  }

  static juce::String sourceKindLabel(TControlSourceKind kind) {
    switch (kind) {
    case TControlSourceKind::expression:
      return "Expression";
    case TControlSourceKind::footswitch:
      return "Footswitch";
    case TControlSourceKind::trigger:
      return "Trigger";
    case TControlSourceKind::midiCc:
      return "MIDI CC";
    case TControlSourceKind::midiNote:
      return "MIDI Note";
    case TControlSourceKind::macro:
      return "Macro";
    }

    return "Source";
  }

  static juce::String sourceModeLabel(TControlSourceMode mode) {
    switch (mode) {
    case TControlSourceMode::continuous:
      return "Continuous";
    case TControlSourceMode::momentary:
      return "Momentary";
    case TControlSourceMode::toggle:
      return "Toggle";
    }

    return "Continuous";
  }

  static juce::String portKindLabel(TControlPortKind kind) {
    switch (kind) {
    case TControlPortKind::value:
      return "Value";
    case TControlPortKind::gate:
      return "Gate";
    case TControlPortKind::trigger:
      return "Trigger";
    }

    return "Port";
  }

  juce::String buildStatusLine(const TControlSource &source) const {
    juce::String text;
    if (source.missing)
      text = "Status: Missing";
    else if (!source.confirmed)
      text = "Status: Learn pending";
    else if (source.autoDetected)
      text = "Status: Auto detected";
    else
      text = "Status: Manual";

    if (source.deviceProfileId.isNotEmpty())
      text << " | Profile: " << source.deviceProfileId;
    return text;
  }

  juce::String buildPortSummary(const TControlSource &source) const {
    juce::StringArray lines;
    for (const auto &port : source.ports)
      lines.add(port.displayName + "  |  " + portKindLabel(port.kind) +
                "  |  id " + port.portId);

    if (lines.isEmpty())
      lines.add("No ports available.");
    return lines.joinIntoString("\n");
  }

  juce::String portNameForId(const TControlSource &source,
                             const juce::String &portId) const {
    for (const auto &port : source.ports) {
      if (port.portId == portId)
        return port.displayName;
    }

    return portId;
  }

  juce::String targetNameForAssignment(
      const TControlSourceAssignment &assignment) const {
    const auto *node = document.findNode(assignment.targetNodeId);
    if (node == nullptr)
      return "Missing node / " + assignment.targetParamId;

    juce::String nodeName = node->label;
    if (nodeName.isEmpty()) {
      if (const auto *desc = registry.descriptorFor(node->typeKey))
        nodeName = desc->displayName;
      if (nodeName.isEmpty())
        nodeName = node->typeKey;
    }

    return nodeName + " / " + assignment.targetParamId;
  }

  juce::String buildAssignmentSummary(const TControlSource &source) const {
    juce::StringArray lines;
    for (const auto &assignment : document.controlState.assignments) {
      if (assignment.sourceId != source.sourceId)
        continue;

      lines.add(portNameForId(source, assignment.portId) + " -> " +
                targetNameForAssignment(assignment));
    }

    if (lines.isEmpty())
      lines.add("No assignments yet.");
    return lines.joinIntoString("\n");
  }

  TGraphDocument &document;
  const TNodeRegistry &registry;
  juce::String currentSourceId;
  juce::Label headerLabel;
  juce::Label kindLabel;
  juce::Label statusLabel;
  juce::Label portsLabel;
  juce::Label assignmentsLabel;
  juce::TextEditor portsBox;
  juce::TextEditor assignmentsBox;
  juce::TextButton closeButton;
  std::function<void()> onLayoutChanged;
};

class SystemEndpointInspectorPanel : public juce::Component {
public:
  explicit SystemEndpointInspectorPanel(TGraphDocument &documentIn)
      : document(documentIn) {
    addAndMakeVisible(headerLabel);
    addAndMakeVisible(kindLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(portsLabel);
    addAndMakeVisible(detailsLabel);
    addAndMakeVisible(portsBox);
    addAndMakeVisible(detailsBox);
    addAndMakeVisible(closeButton);

    headerLabel.setJustificationType(juce::Justification::centredLeft);
    headerLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.95f));
    headerLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

    kindLabel.setJustificationType(juce::Justification::centredLeft);
    kindLabel.setColour(juce::Label::textColourId,
                        juce::Colours::white.withAlpha(0.68f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId,
                          juce::Colours::white.withAlpha(0.58f));

    portsLabel.setText("Ports", juce::dontSendNotification);
    portsLabel.setJustificationType(juce::Justification::centredLeft);
    portsLabel.setColour(juce::Label::textColourId,
                         juce::Colours::white.withAlpha(0.78f));

    detailsLabel.setText("Endpoint", juce::dontSendNotification);
    detailsLabel.setJustificationType(juce::Justification::centredLeft);
    detailsLabel.setColour(juce::Label::textColourId,
                           juce::Colours::white.withAlpha(0.78f));

    configureReadOnlyBox(portsBox);
    configureReadOnlyBox(detailsBox);

    closeButton.setButtonText("Hide");
    closeButton.onClick = [this] { hidePanel(); };
    setVisible(false);
  }

  void setLayoutChangedCallback(std::function<void()> callback) {
    onLayoutChanged = std::move(callback);
  }

  bool isPanelOpen() const noexcept {
    return isVisible() && currentEndpointId.isNotEmpty();
  }

  const juce::String &selectedEndpointId() const noexcept {
    return currentEndpointId;
  }

  void inspectEndpoint(const juce::String &endpointId) {
    const auto normalized = endpointId.trim();
    if (normalized.isEmpty()) {
      hidePanel();
      return;
    }

    const bool wasOpen = isPanelOpen();
    const bool changed = currentEndpointId != normalized;
    currentEndpointId = normalized;
    setVisible(true);
    refreshFromDocument();

    if ((!wasOpen || changed) && onLayoutChanged != nullptr)
      onLayoutChanged();
  }

  void refreshFromDocument() {
    if (currentEndpointId.isEmpty())
      return;

    const auto *endpoint = document.controlState.findEndpoint(currentEndpointId);
    if (endpoint == nullptr) {
      hidePanel();
      return;
    }

    headerLabel.setText(endpoint->displayName, juce::dontSendNotification);
    kindLabel.setText(buildKindLine(*endpoint), juce::dontSendNotification);
    statusLabel.setText(buildStatusLine(*endpoint), juce::dontSendNotification);
    portsBox.setText(buildPortSummary(*endpoint), false);
    detailsBox.setText(buildDetailSummary(*endpoint), false);
    repaint();
  }

  void hidePanel() {
    if (!isPanelOpen() && currentEndpointId.isEmpty())
      return;

    currentEndpointId.clear();
    setVisible(false);
    if (onLayoutChanged != nullptr)
      onLayoutChanged();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    const auto accent = currentAccent();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xee111827),
                                           bounds.getCentreX(), bounds.getY(),
                                           juce::Colour(0xee0b1220),
                                           bounds.getCentreX(),
                                           bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(accent.withAlpha(0.42f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12, 10);
    auto header = area.removeFromTop(24);
    closeButton.setBounds(header.removeFromRight(54));
    headerLabel.setBounds(header);
    kindLabel.setBounds(area.removeFromTop(18));
    statusLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(8);
    portsLabel.setBounds(area.removeFromTop(18));
    portsBox.setBounds(area.removeFromTop(116));
    area.removeFromTop(8);
    detailsLabel.setBounds(area.removeFromTop(18));
    detailsBox.setBounds(area);
  }

private:
  static void configureReadOnlyBox(juce::TextEditor &box) {
    box.setMultiLine(true);
    box.setReadOnly(true);
    box.setScrollbarsShown(true);
    box.setColour(juce::TextEditor::backgroundColourId,
                  juce::Colour(0x66111827));
    box.setColour(juce::TextEditor::outlineColourId,
                  juce::Colour(0xff324154));
    box.setColour(juce::TextEditor::textColourId,
                  juce::Colours::white.withAlpha(0.82f));
  }

  static juce::String endpointKindLabel(TSystemRailEndpointKind kind) {
    switch (kind) {
    case TSystemRailEndpointKind::audioInput:
      return "Audio Input";
    case TSystemRailEndpointKind::audioOutput:
      return "Audio Output";
    case TSystemRailEndpointKind::midiInput:
      return "MIDI Input";
    case TSystemRailEndpointKind::midiOutput:
      return "MIDI Output";
    }

    return "Endpoint";
  }

  static juce::String dataTypeLabel(TPortDataType type) {
    switch (type) {
    case TPortDataType::Audio:
      return "Audio";
    case TPortDataType::CV:
      return "CV";
    case TPortDataType::MIDI:
      return "MIDI";
    case TPortDataType::Control:
      return "Control";
    }

    return "Signal";
  }

  juce::Colour currentAccent() const {
    if (const auto *endpoint = document.controlState.findEndpoint(currentEndpointId))
      return endpointAccent(*endpoint);
    return juce::Colour(0xff60a5fa);
  }

  juce::String buildKindLine(const TSystemRailEndpoint &endpoint) const {
    juce::String line = endpointKindLabel(endpoint.kind);
    if (endpoint.stereo)
      line << " / Stereo";
    else if (endpoint.ports.size() > 1)
      line << " / " << juce::String((int)endpoint.ports.size()) << " ports";
    else
      line << " / Single";
    return line;
  }

  juce::String buildStatusLine(const TSystemRailEndpoint &endpoint) const {
    juce::String line = endpoint.missing ? "Status: Missing" : "Status: Ready";
    if (const auto *rail = document.controlState.findRail(endpoint.railId))
      line << " | Rail: " << rail->title;
    return line;
  }

  juce::String buildPortSummary(const TSystemRailEndpoint &endpoint) const {
    juce::StringArray lines;
    for (const auto &port : endpoint.ports) {
      lines.add(port.displayName + "  |  " + dataTypeLabel(port.dataType) +
                "  |  id " + port.portId);
    }

    if (lines.isEmpty())
      lines.add("No ports available.");
    return lines.joinIntoString("\n");
  }

  juce::String buildDetailSummary(const TSystemRailEndpoint &endpoint) const {
    juce::StringArray lines;
    lines.add("Endpoint id: " + endpoint.endpointId);
    lines.add("Rail id: " + endpoint.railId);
    lines.add("Kind: " + endpointKindLabel(endpoint.kind));
    lines.add("Stereo: " + juce::String(endpoint.stereo ? "Yes" : "No"));
    lines.add("Port count: " + juce::String((int)endpoint.ports.size()));
    if (endpoint.subtitle.isNotEmpty())
      lines.add("Detail: " + endpoint.subtitle);
    return lines.joinIntoString("\n");
  }

  TGraphDocument &document;
  juce::String currentEndpointId;
  juce::Label headerLabel;
  juce::Label kindLabel;
  juce::Label statusLabel;
  juce::Label portsLabel;
  juce::Label detailsLabel;
  juce::TextEditor portsBox;
  juce::TextEditor detailsBox;
  juce::TextButton closeButton;
  std::function<void()> onLayoutChanged;
};
class RuntimeStatusStrip : public juce::Component {
public:
  void setStats(const TGraphRuntime::RuntimeStats &newStats) {
    stats = newStats;
    repaint();
  }

  void setSessionStatus(const TEditorSessionStatus &newStatus,
                        bool dirtyState) {
    sessionStatus = newStatus;
    dirty = dirtyState;
    repaint();
  }

  void setTransientMessage(const juce::String &text, juce::Colour accent) {
    transientMessage = text;
    transientAccent = accent;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    const juce::Colour frameAccent =
        (transientMessage.isNotEmpty() ? transientAccent : statusAccent())
            .withAlpha(0.30f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xee0b1220),
                                           bounds.getCentreX(), bounds.getY(),
                                           juce::Colour(0xee111827),
                                           bounds.getCentreX(),
                                           bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(frameAccent);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);

    auto content = getLocalBounds().reduced(12, 8);
    auto primaryRow = content.removeFromTop(20);
    auto secondaryRow = content.removeFromTop(16);
    content.removeFromTop(4);
    auto badgeRow = content.removeFromTop(20);

    auto cpuChip = primaryRow.removeFromRight(92);
    const juce::String primaryText = juce::String::formatted(
        "%.1f kHz  |  %d blk  |  %d in / %d out",
        stats.sampleRate * 0.001,
        stats.preparedBlockSize,
        stats.lastInputChannels,
        stats.lastOutputChannels);
    const juce::String summaryText = juce::String::formatted(
        "Gen %llu  |  Nodes %d  |  Buffers %d  |  Process %.2f ms",
        static_cast<unsigned long long>(stats.activeGeneration),
        stats.activeNodeCount,
        stats.allocatedPortChannels,
        stats.lastProcessMilliseconds);

    g.setColour(juce::Colours::white.withAlpha(0.97f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(primaryText, primaryRow, juce::Justification::centredLeft,
               false);

    drawCpuChip(g, cpuChip);

    auto summaryArea = secondaryRow;
    if (transientMessage.isNotEmpty() && secondaryRow.getWidth() > 180) {
      const int messageWidth = juce::jlimit(
          144, juce::jmax(190, secondaryRow.getWidth() / 2),
          30 + transientMessage.length() * 7);
      auto messageArea = summaryArea.removeFromRight(
          juce::jmin(messageWidth, summaryArea.getWidth()));
      summaryArea.removeFromRight(8);
      drawMessageChip(g, messageArea, transientMessage, transientAccent);
    }

    if (summaryArea.getWidth() > 40) {
      g.setColour(juce::Colours::white.withAlpha(0.62f));
      g.setFont(11.0f);
      g.drawText(summaryText, summaryArea, juce::Justification::centredLeft,
                 false);
    }

    int badgeX = badgeRow.getX();
    auto drawBadge = [&](const juce::String &text, juce::Colour colour) {
      if (text.isEmpty())
        return;

      const int badgeWidth = juce::jlimit(66, 168, 22 + text.length() * 7);
      if (badgeX + badgeWidth > badgeRow.getRight())
        return;

      juce::Rectangle<int> badge(badgeX, badgeRow.getY(), badgeWidth,
                                 badgeRow.getHeight());
      badgeX += badgeWidth + 6;

      g.setColour(colour.withAlpha(0.16f));
      g.fillRoundedRectangle(badge.toFloat(), 8.0f);
      g.setColour(colour.withAlpha(0.86f));
      g.drawRoundedRectangle(badge.toFloat(), 8.0f, 1.0f);
      g.setColour(colour.brighter(0.18f));
      g.setFont(10.0f);
      g.drawText(text, badge, juce::Justification::centred, false);
    };

    bool drewBadge = false;
    if (stats.rebuildPending) {
      drawBadge("Deferred Apply", juce::Colour(0xfff59e0b));
      drewBadge = true;
    }
    if (stats.smoothingActiveCount > 0) {
      drawBadge("Smooth " + juce::String(stats.smoothingActiveCount),
                juce::Colour(0xff60a5fa));
      drewBadge = true;
    }
    if (stats.xrunDetected) {
      drawBadge("XRUN", juce::Colour(0xffef4444));
      drewBadge = true;
    }
    if (stats.clipDetected) {
      drawBadge("Clip", juce::Colour(0xfff97316));
      drewBadge = true;
    }
    if (stats.denormalDetected) {
      drawBadge("Denormal", juce::Colour(0xffeab308));
      drewBadge = true;
    }
    if (stats.mutedFallbackActive) {
      drawBadge("Muted Fallback", juce::Colour(0xff94a3b8));
      drewBadge = true;
    }
    if (!drewBadge)
      drawBadge("Stable", juce::Colour(0xff22c55e));

    drawBadge(dirty ? "Dirty" : "Saved",
              dirty ? juce::Colour(0xfff59e0b) : juce::Colour(0xff64748b));
    if (sessionStatus.hasAutosaveSnapshot) {
      const auto timeLabel = sessionStatus.lastAutosaveTime.toMilliseconds() > 0
                                 ? sessionStatus.lastAutosaveTime.formatted("%H:%M")
                                 : juce::String("--:--");
      drawBadge("Autosave " + timeLabel, juce::Colour(0xff38bdf8));
    }
  }

private:
  juce::Colour statusAccent() const {
    if (stats.xrunDetected)
      return juce::Colour(0xffef4444);
    if (stats.clipDetected)
      return juce::Colour(0xfff97316);
    if (stats.denormalDetected)
      return juce::Colour(0xffeab308);
    if (stats.rebuildPending)
      return juce::Colour(0xfff59e0b);
    if (stats.mutedFallbackActive)
      return juce::Colour(0xff94a3b8);
    return juce::Colour(0xff22c55e);
  }

  juce::Colour cpuAccent() const {
    if (stats.xrunDetected || stats.clipDetected)
      return juce::Colour(0xffef4444);
    if (stats.cpuLoadPercent >= 65.0f)
      return juce::Colour(0xfff97316);
    if (stats.cpuLoadPercent >= 35.0f)
      return juce::Colour(0xfff59e0b);
    return juce::Colour(0xff60a5fa);
  }

  void drawCpuChip(juce::Graphics &g, juce::Rectangle<int> area) const {
    const juce::Colour accent = cpuAccent();
    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(area.toFloat(), 9.0f);
    g.setColour(accent.withAlpha(0.90f));
    g.drawRoundedRectangle(area.toFloat(), 9.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.97f));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText(juce::String::formatted("CPU %.1f%%", stats.cpuLoadPercent),
               area, juce::Justification::centred, false);
  }

  void drawMessageChip(juce::Graphics &g, juce::Rectangle<int> area,
                       const juce::String &text, juce::Colour accent) const {
    g.setColour(accent.withAlpha(0.18f));
    g.fillRoundedRectangle(area.toFloat(), 7.0f);
    g.setColour(accent.withAlpha(0.92f));
    g.drawRoundedRectangle(area.toFloat(), 7.0f, 1.0f);
    g.setColour(accent.brighter(0.18f));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft,
               false);
  }

  TGraphRuntime::RuntimeStats stats;
  TEditorSessionStatus sessionStatus;
  bool dirty = false;
  juce::String transientMessage;
  juce::Colour transientAccent = juce::Colour(0xff60a5fa);
};

class DocumentNoticeBanner : public juce::Component {
public:
  DocumentNoticeBanner() {
    addAndMakeVisible(dismissButton);
    dismissButton.setButtonText("Dismiss");
    dismissButton.onClick = [this] {
      if (dismissHandler != nullptr)
        dismissHandler();
    };
    setVisible(false);
  }

  void setDismissHandler(std::function<void()> handler) {
    dismissHandler = std::move(handler);
  }

  void setNotice(const TDocumentNotice &newNotice) {
    notice = newNotice;
    setVisible(notice.active);
    repaint();
  }

  void paint(juce::Graphics &g) override {
    if (!notice.active)
      return;

    const auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty())
      return;

    const auto accent = accentForLevel(notice.level);
    g.setColour(juce::Colour(0xf4121826));
    g.fillRoundedRectangle(bounds, 11.0f);
    g.setColour(accent.withAlpha(0.85f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 11.0f, 1.0f);

    auto textArea = getLocalBounds().reduced(14, 9);
    textArea.removeFromRight(92);

    g.setColour(juce::Colours::white.withAlpha(0.96f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(notice.title, textArea.removeFromTop(20),
               juce::Justification::centredLeft, false);

    if (notice.detail.isNotEmpty()) {
      g.setColour(juce::Colours::white.withAlpha(0.72f));
      g.setFont(11.0f);
      g.drawFittedText(notice.detail, textArea, juce::Justification::topLeft,
                       2, 0.92f);
    }
  }

  void resized() override {
    dismissButton.setBounds(getLocalBounds().removeFromRight(88).reduced(10, 10));
  }

private:
  static juce::Colour accentForLevel(TDocumentNoticeLevel level) {
    switch (level) {
    case TDocumentNoticeLevel::degraded:
      return juce::Colour(0xffef4444);
    case TDocumentNoticeLevel::warning:
      return juce::Colour(0xfff59e0b);
    case TDocumentNoticeLevel::info:
      return juce::Colour(0xff38bdf8);
    }

    return juce::Colour(0xff38bdf8);
  }

  TDocumentNotice notice;
  juce::TextButton dismissButton;
  std::function<void()> dismissHandler;
};

EditorHandle::Impl::Impl(
    EditorHandle &ownerIn, juce::AudioDeviceManager *audioDeviceManagerIn,
    ParamBindingSummaryResolver bindingSummaryResolverIn,
    ParamBindingRevisionProvider bindingRevisionProviderIn)
    : owner(ownerIn), registryStore(makeDefaultNodeRegistry()),
      runtime(registryStore.get()), audioDeviceManager(audioDeviceManagerIn),
      bindingRevisionProvider(std::move(bindingRevisionProviderIn)) {
  canvas = std::make_unique<TGraphCanvas>(doc, *registryStore);
  canvas->setConnectionLevelProvider([this](const TConnection &connection) {
    return runtime.getPortLevel(connection.from.portId);
  });
  canvas->setPortLevelProvider(
      [this](PortId portId) { return runtime.getPortLevel(portId); });
  canvas->setBindingSummaryResolver(bindingSummaryResolverIn);
  canvas->setNodePropertiesRequestHandler(
      [this](NodeId nodeId) { openProperties(nodeId); });
  owner.addAndMakeVisible(*canvas);

  libraryPanel = NodeLibraryPanel::create(
      *registryStore, [this](const juce::String &typeKey) {
        if (canvas != nullptr)
          canvas->addNodeByTypeAtView(typeKey, canvas->getViewCenter(), true);
      });
  owner.addAndMakeVisible(*libraryPanel);

  propertiesPanel = NodePropertiesPanel::create(
      doc, *canvas, *registryStore, &runtime,
      std::move(bindingSummaryResolverIn));
  propertiesPanel->setLayoutChangedCallback([this] { owner.resized(); });
  owner.addAndMakeVisible(*propertiesPanel);

  diagnosticsDrawer = DiagnosticsDrawer::create();
  diagnosticsDrawer->setLayoutChangedCallback([this] { owner.resized(); });
  diagnosticsDrawer->setFocusRequestHandler(
      [this](const juce::String &graphId, const juce::String &query) {
        return focusDiagnosticTarget(graphId, query);
      });
  owner.addAndMakeVisible(*diagnosticsDrawer);
  diagnosticsDrawer->setVisible(false);

  presetBrowserPanel = PresetBrowserPanel::create();
  presetBrowserPanel->setLayoutChangedCallback([this] { owner.resized(); });
  presetBrowserPanel->setPrimaryActionHandler(
      [this](const TPresetEntry &entry) -> juce::Result {
        if (canvas == nullptr)
          return juce::Result::fail("Preset action failed: canvas unavailable.");

        if (entry.presetKind == "teul.patch") {
          const auto result =
              canvas->insertPatchPresetFromFile(entry.file, canvas->getViewCenter());
          if (result.wasOk())
            pushRuntimeMessage("Patch preset inserted", juce::Colour(0xff22c55e),
                               44);
          return result;
        }

        if (entry.presetKind == "teul.state") {
          const auto result = canvas->applyStatePresetFromFile(entry.file);
          if (result.wasOk())
            pushRuntimeMessage("State preset applied", juce::Colour(0xff38bdf8),
                               44);
          return result;
        }

        if (entry.presetKind == "teul.recovery") {
          TSchemaMigrationReport migrationReport;
          if (!TFileIo::loadFromFile(doc, entry.file, &migrationReport)) {
            return juce::Result::fail(
                "Recovery restore failed: autosave snapshot could not be loaded.");
          }

          rebuildAll(true);
          pushRuntimeMessage("Autosave snapshot restored",
                             migrationReport.degraded
                                 ? juce::Colour(0xfff59e0b)
                                 : juce::Colour(0xff22c55e),
                             56);
          return juce::Result::ok();
        }

        return juce::Result::fail(
            "Preset action failed: unsupported preset kind.");
      });
  presetBrowserPanel->setSecondaryActionHandler(
      [this](const TPresetEntry &entry) -> juce::Result {
        if (entry.presetKind != "teul.recovery")
          return juce::Result::fail(
              "Preset action failed: no secondary action available.");

        if (!entry.file.existsAsFile())
          return juce::Result::fail(
              "Recovery discard failed: autosave snapshot file was not found.");

        const auto stateFile =
            entry.file.getParentDirectory().getChildFile("autosave-session-state.json");
        if (!entry.file.deleteFile())
          return juce::Result::fail(
              "Recovery discard failed: autosave snapshot file could not be removed.");
        if (stateFile.existsAsFile() && !stateFile.deleteFile())
          return juce::Result::fail(
              "Recovery discard failed: session marker file could not be removed.");

        sessionStatus.hasAutosaveSnapshot = false;
        sessionStatus.lastAutosaveTime = {};
        refreshSessionStatusUi(true);
        pushRuntimeMessage("Autosave snapshot discarded",
                           juce::Colour(0xff94a3b8), 44);
        return juce::Result::ok();
      });
  presetBrowserPanel->setEntryPreviewHandler(
      [this](const TPresetEntry &entry, juce::String &summaryText,
             juce::String &detailText, bool &warning) {
        if (entry.presetKind == "teul.recovery") {
          const auto result = buildRecoveryDiffPreview(doc, entry.file, summaryText,
                                                       detailText, warning);
          if (result.failed()) {
            summaryText = "Recovery Diff Preview Unavailable";
            detailText = result.getErrorMessage();
            warning = true;
          }
          return;
        }

        if (entry.presetKind != "teul.state")
          return;

        TStatePresetDiffPreview preview;
        const auto result =
            TStatePresetIO::previewAgainstDocument(doc, entry.file, &preview);
        if (result.failed()) {
          summaryText = "State Diff Preview Unavailable";
          detailText = result.getErrorMessage();
          warning = true;
          return;
        }

        summaryText = "State Diff Preview";
        juce::StringArray parts;
        parts.add(juce::String(preview.changedNodeCount) + " changed nodes");
        parts.add(juce::String(preview.changedParamValueCount) + " param deltas");
        if (preview.changedBypassCount > 0)
          parts.add(juce::String(preview.changedBypassCount) + " bypass deltas");
        if (preview.missingNodeCount > 0)
          parts.add(juce::String(preview.missingNodeCount) + " missing nodes");

        detailText = parts.joinIntoString("  |  ");
        if (!preview.changedNodeLabels.isEmpty()) {
          auto names = preview.changedNodeLabels;
          if (names.size() > 4)
            names.removeRange(4, names.size() - 4);
          detailText << "\r\nTargets: " << names.joinIntoString(", ");
          if (preview.changedNodeLabels.size() > names.size())
            detailText << " +" << juce::String(preview.changedNodeLabels.size() - names.size())
                       << " more";
        }
        if (!preview.warnings.isEmpty())
          detailText << "\r\nWarnings: " << preview.warnings.joinIntoString(" | ");
        warning = preview.degraded || preview.missingNodeCount > 0;
      });
  owner.addAndMakeVisible(*presetBrowserPanel);
  presetBrowserPanel->setVisible(false);
  presetBrowserPanel->setSessionPreview("Session: Saved",
                                        "Waiting for the first autosave snapshot.",
                                        false);

  runtimeStatusStrip = std::make_unique<RuntimeStatusStrip>();
  owner.addAndMakeVisible(*runtimeStatusStrip);

  documentNoticeBanner = std::make_unique<DocumentNoticeBanner>();
  documentNoticeBanner->setDismissHandler([this] {
    doc.clearTransientNotice();
    refreshDocumentNoticeUi(true);
  });
  owner.addAndMakeVisible(*documentNoticeBanner);
  documentNoticeBanner->setVisible(false);

  controlSourceInspector =
      std::make_unique<ControlSourceInspectorPanel>(doc, *registryStore);
  controlSourceInspector->setLayoutChangedCallback(
      [this] {
        owner.resized();
        refreshRailUi();
      });
  owner.addAndMakeVisible(*controlSourceInspector);
  controlSourceInspector->setVisible(false);

  systemEndpointInspector =
      std::make_unique<SystemEndpointInspectorPanel>(doc);
  systemEndpointInspector->setLayoutChangedCallback(
      [this] {
        owner.resized();
        refreshRailUi();
      });
  owner.addAndMakeVisible(*systemEndpointInspector);
  systemEndpointInspector->setVisible(false);

  inputRail = std::make_unique<RailPanel>(doc, "input-rail",
                                          RailOrientation::vertical);
  inputRail->setCollapseHandler([this] { refreshRailUi(true); });
  inputRail->setCardSelectionHandler(
      [this](const juce::String &endpointId) { inspectSystemEndpoint(endpointId); });
  inputRail->setPortDragTargetComponent(canvas.get());
  inputRail->setPortDragHandlers(
      [this](const juce::String &endpointId, const juce::String &portId,
             juce::Point<float> sourceAnchorView,
             juce::Point<float> mousePosView) {
        return beginInputRailPortDrag(doc, *registryStore, *canvas, endpointId,
                                      portId, sourceAnchorView, mousePosView);
      },
      [this](juce::Point<float> mousePosView) {
        if (canvas != nullptr)
          canvas->updateConnectionDrag(mousePosView);
      },
      [this](juce::Point<float> mousePosView) {
        if (canvas != nullptr)
          canvas->endConnectionDrag(mousePosView);
      });
  owner.addAndMakeVisible(*inputRail);

  outputRail = std::make_unique<RailPanel>(doc, "output-rail",
                                           RailOrientation::vertical);
  outputRail->setCollapseHandler([this] { refreshRailUi(true); });
  outputRail->setCardSelectionHandler(
      [this](const juce::String &endpointId) { inspectSystemEndpoint(endpointId); });
  owner.addAndMakeVisible(*outputRail);

  controlRail = std::make_unique<RailPanel>(doc, "control-rail",
                                            RailOrientation::horizontal);
  controlRail->setCollapseHandler([this] { refreshRailUi(true); });
  controlRail->setCardSelectionHandler(
      [this](const juce::String &sourceId) { inspectControlSource(sourceId); });
  owner.addAndMakeVisible(*controlRail);

  canvas->setNodeSelectionChangedHandler(
      [this](const std::vector<NodeId> &selectedNodeIds) {
        handleSelectionChanged(selectedNodeIds);
      });
  canvas->setFrameSelectionChangedHandler(
      [this](int frameId) { handleFrameSelectionChanged(frameId); });

  owner.addAndMakeVisible(toggleLibraryButton);
  owner.addAndMakeVisible(quickAddButton);
  owner.addAndMakeVisible(findNodeButton);
  owner.addAndMakeVisible(commandPaletteButton);
  owner.addAndMakeVisible(toggleHeatmapButton);
  owner.addAndMakeVisible(toggleProbeButton);
  owner.addAndMakeVisible(toggleOverlayButton);
  owner.addAndMakeVisible(toggleDiagnosticsButton);
  owner.addAndMakeVisible(togglePresetsButton);

  toggleLibraryButton.setButtonText("Library");
  quickAddButton.setButtonText("Quick Add");
  findNodeButton.setButtonText("Find Node");
  commandPaletteButton.setButtonText("Cmd");
  toggleHeatmapButton.setButtonText("Heat");
  toggleProbeButton.setButtonText("Probe");
  toggleOverlayButton.setButtonText("Overlay");
  toggleDiagnosticsButton.setButtonText("Diagnostics");
  togglePresetsButton.setButtonText("Presets");

  toggleHeatmapButton.setClickingTogglesState(true);
  toggleProbeButton.setClickingTogglesState(true);
  toggleOverlayButton.setClickingTogglesState(true);
  toggleDiagnosticsButton.setClickingTogglesState(true);
  togglePresetsButton.setClickingTogglesState(true);
  toggleHeatmapButton.setTooltip("Toggle node cost tint and heat rails");
  toggleProbeButton.setTooltip("Toggle node probe rails and selected readouts");
  toggleOverlayButton.setTooltip("Toggle runtime overlay card");
  toggleDiagnosticsButton.setTooltip("Show latest verification and benchmark results");
  togglePresetsButton.setTooltip(
      "Browse reusable presets and insert or apply them");

  toggleLibraryButton.onClick = [this] {
    libraryVisible = !libraryVisible;
    if (libraryPanel != nullptr)
      libraryPanel->setVisible(libraryVisible);
    owner.resized();
  };

  quickAddButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->openQuickAddAt(canvas->getViewCenter());
  };

  findNodeButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->openNodeSearchPrompt();
  };

  commandPaletteButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->openCommandPalette();
  };

  toggleHeatmapButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->setRuntimeHeatmapEnabled(toggleHeatmapButton.getToggleState());
    syncRuntimeViewButtons();
  };

  toggleProbeButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->setLiveProbeEnabled(toggleProbeButton.getToggleState());
    syncRuntimeViewButtons();
  };

  toggleOverlayButton.onClick = [this] {
    if (canvas != nullptr)
      canvas->setDebugOverlayEnabled(toggleOverlayButton.getToggleState());
    syncRuntimeViewButtons();
  };

  toggleDiagnosticsButton.onClick = [this] {
    if (toggleDiagnosticsButton.getToggleState() && presetBrowserPanel != nullptr)
      presetBrowserPanel->setBrowserOpen(false);
    if (diagnosticsDrawer != nullptr) {
      diagnosticsDrawer->setDrawerOpen(toggleDiagnosticsButton.getToggleState());
      if (toggleDiagnosticsButton.getToggleState())
        diagnosticsDrawer->refreshArtifacts(true);
    }
    syncRuntimeViewButtons();
  };

  togglePresetsButton.onClick = [this] {
    if (togglePresetsButton.getToggleState() && diagnosticsDrawer != nullptr)
      diagnosticsDrawer->setDrawerOpen(false);
    if (presetBrowserPanel != nullptr) {
      if (togglePresetsButton.getToggleState())
        presetBrowserPanel->refreshEntries(true);
      presetBrowserPanel->setBrowserOpen(togglePresetsButton.getToggleState());
    }
    syncRuntimeViewButtons();
  };

  syncRuntimeViewButtons();

  rebuildAll(true);

  if (audioDeviceManager != nullptr)
    audioDeviceManager->addAudioCallback(&runtime);

  startTimerHz(20);
}

EditorHandle::Impl::~Impl() {
  stopTimer();

  if (canvas != nullptr) {
    canvas->setNodeSelectionChangedHandler({});
    canvas->setFrameSelectionChangedHandler({});
    canvas->setNodePropertiesRequestHandler({});
    canvas->setConnectionLevelProvider({});
    canvas->setPortLevelProvider({});
    canvas->setBindingSummaryResolver({});
  }

  if (propertiesPanel != nullptr) {
    propertiesPanel->setLayoutChangedCallback({});
    propertiesPanel->setParamProvider(nullptr);
  }

  if (diagnosticsDrawer != nullptr) {
    diagnosticsDrawer->setFocusRequestHandler({});
    diagnosticsDrawer->setLayoutChangedCallback({});
  }

  if (presetBrowserPanel != nullptr) {
    presetBrowserPanel->setPrimaryActionHandler({});
    presetBrowserPanel->setSecondaryActionHandler({});
    presetBrowserPanel->setEntryPreviewHandler({});
    presetBrowserPanel->setLayoutChangedCallback({});
  }

  if (documentNoticeBanner != nullptr)
    documentNoticeBanner->setDismissHandler({});

  if (controlSourceInspector != nullptr)
    controlSourceInspector->setLayoutChangedCallback({});
  if (systemEndpointInspector != nullptr)
    systemEndpointInspector->setLayoutChangedCallback({});

  if (inputRail != nullptr) {
    inputRail->setCollapseHandler({});
    inputRail->setCardSelectionHandler({});
  }
  if (outputRail != nullptr) {
    outputRail->setCollapseHandler({});
    outputRail->setCardSelectionHandler({});
  }
  if (controlRail != nullptr) {
    controlRail->setCollapseHandler({});
    controlRail->setCardSelectionHandler({});
  }

  if (audioDeviceManager != nullptr)
    audioDeviceManager->removeAudioCallback(&runtime);
}

TGraphDocument &EditorHandle::Impl::document() noexcept { return doc; }

const TGraphDocument &EditorHandle::Impl::document() const noexcept {
  return doc;
}

const TNodeRegistry *EditorHandle::Impl::registry() const noexcept {
  return registryStore.get();
}

void EditorHandle::Impl::refreshFromDocument() { rebuildAll(true); }

void EditorHandle::Impl::setSessionStatus(const TEditorSessionStatus &status) {
  sessionStatus = status;
  refreshSessionStatusUi(true);
}

void EditorHandle::Impl::layout(juce::Rectangle<int> area) {
  auto top = area.removeFromTop(40).reduced(6, 4);

  toggleLibraryButton.setBounds(top.removeFromLeft(78));
  top.removeFromLeft(4);
  quickAddButton.setBounds(top.removeFromLeft(92));
  top.removeFromLeft(4);
  findNodeButton.setBounds(top.removeFromLeft(92));
  top.removeFromLeft(4);
  commandPaletteButton.setBounds(top.removeFromLeft(60));
  top.removeFromLeft(8);
  toggleHeatmapButton.setBounds(top.removeFromLeft(82));
  top.removeFromLeft(4);
  toggleProbeButton.setBounds(top.removeFromLeft(92));
  top.removeFromLeft(4);
  toggleOverlayButton.setBounds(top.removeFromLeft(108));
  top.removeFromLeft(4);
  toggleDiagnosticsButton.setBounds(top.removeFromLeft(118));
  top.removeFromLeft(4);
  togglePresetsButton.setBounds(top.removeFromLeft(96));

  if (runtimeStatusStrip != nullptr) {
    auto statusArea = area.removeFromTop(60).reduced(6, 4);
    runtimeStatusStrip->setBounds(statusArea);
  }

  if (documentNoticeBanner != nullptr) {
    if (documentNoticeBanner->isVisible()) {
      auto bannerArea = area.removeFromTop(56).reduced(6, 3);
      documentNoticeBanner->setBounds(bannerArea);
    } else {
      documentNoticeBanner->setBounds({});
    }
  }

  if (libraryPanel != nullptr) {
    libraryPanel->setVisible(libraryVisible);
    if (libraryVisible) {
      auto left = area.removeFromLeft(244);
      libraryPanel->setBounds(left.reduced(0, 2));
    }
  }

  const bool endpointInspectorOpen = systemEndpointInspector != nullptr &&
                                     systemEndpointInspector->isPanelOpen();
  const bool controlInspectorOpen = controlSourceInspector != nullptr &&
                                    controlSourceInspector->isPanelOpen();
  const bool propertiesOpen = propertiesPanel != nullptr &&
                              propertiesPanel->isPanelOpen();
  if (endpointInspectorOpen || controlInspectorOpen || propertiesOpen) {
    auto right = area.removeFromRight(336);
    const auto inspectorBounds = right.reduced(0, 2);
    if (systemEndpointInspector != nullptr)
      systemEndpointInspector->setBounds(endpointInspectorOpen
                                             ? inspectorBounds
                                             : juce::Rectangle<int>());
    if (controlSourceInspector != nullptr)
      controlSourceInspector->setBounds(controlInspectorOpen
                                            ? inspectorBounds
                                            : juce::Rectangle<int>());
    if (propertiesPanel != nullptr)
      propertiesPanel->setBounds((!endpointInspectorOpen &&
                                  !controlInspectorOpen && propertiesOpen)
                                     ? inspectorBounds
                                     : juce::Rectangle<int>());
  } else {
    if (systemEndpointInspector != nullptr)
      systemEndpointInspector->setBounds({});
    if (controlSourceInspector != nullptr)
      controlSourceInspector->setBounds({});
    if (propertiesPanel != nullptr)
      propertiesPanel->setBounds({});
  }

  if (presetBrowserPanel != nullptr && presetBrowserPanel->isBrowserOpen()) {
    auto drawerArea = area.removeFromBottom(548).reduced(0, 2);
    presetBrowserPanel->setBounds(drawerArea);
  } else if (diagnosticsDrawer != nullptr && diagnosticsDrawer->isDrawerOpen()) {
    auto drawerArea = area.removeFromBottom(654).reduced(0, 2);
    diagnosticsDrawer->setBounds(drawerArea);
  }

  if (presetBrowserPanel != nullptr && !presetBrowserPanel->isBrowserOpen())
    presetBrowserPanel->setBounds({});
  if (diagnosticsDrawer != nullptr && !diagnosticsDrawer->isDrawerOpen())
    diagnosticsDrawer->setBounds({});

  if (controlRail != nullptr) {
    const bool collapsed = controlRail->isRailCollapsed();
    const int targetHeight =
        collapsed ? 40
                  : juce::jlimit(104, 188,
                                 juce::roundToInt((float)area.getHeight() * 0.16f));
    const int railHeight = juce::jmin(targetHeight,
                                      juce::jmax(collapsed ? 40 : 96,
                                                 area.getHeight() / 3));
    controlRail->setBounds(area.removeFromBottom(railHeight).reduced(0, 2));
  }

  if (inputRail != nullptr) {
    const bool collapsed = inputRail->isRailCollapsed();
    const int targetWidth =
        collapsed ? 44
                  : juce::jlimit(104, 168,
                                 juce::roundToInt((float)area.getWidth() * 0.13f));
    const int railWidth = juce::jmin(targetWidth,
                                     juce::jmax(collapsed ? 44 : 92,
                                                area.getWidth() / 4));
    inputRail->setBounds(area.removeFromLeft(railWidth).reduced(0, 2));
  }

  if (outputRail != nullptr) {
    const bool collapsed = outputRail->isRailCollapsed();
    const int targetWidth =
        collapsed ? 44
                  : juce::jlimit(104, 168,
                                 juce::roundToInt((float)area.getWidth() * 0.13f));
    const int railWidth = juce::jmin(targetWidth,
                                     juce::jmax(collapsed ? 44 : 92,
                                                area.getWidth() / 4));
    outputRail->setBounds(area.removeFromRight(railWidth).reduced(0, 2));
  }

  if (canvas != nullptr)
    canvas->setBounds(area.reduced(0, 2));
}

void EditorHandle::Impl::timerCallback() {
  const auto currentRuntimeRevision = doc.getRuntimeRevision();
  if (currentRuntimeRevision != lastRuntimeRevision) {
    runtime.buildGraph(doc);
    lastRuntimeRevision = currentRuntimeRevision;
  }

  const auto currentDocumentRevision = doc.getDocumentRevision();
  if (currentDocumentRevision != lastDocumentRevision) {
    if (propertiesPanel != nullptr)
      propertiesPanel->refreshFromDocument();
    if (controlSourceInspector != nullptr)
      controlSourceInspector->refreshFromDocument();
    if (systemEndpointInspector != nullptr)
      systemEndpointInspector->refreshFromDocument();
    refreshRailUi();
    lastDocumentRevision = currentDocumentRevision;
  }

  const auto currentBindingRevision =
      bindingRevisionProvider != nullptr ? bindingRevisionProvider() : 0;
  if (currentBindingRevision != lastBindingRevision) {
    if (propertiesPanel != nullptr)
      propertiesPanel->refreshBindingSummaries();
    lastBindingRevision = currentBindingRevision;
  }

  refreshRuntimeUi();
  refreshDocumentNoticeUi();
  refreshSessionStatusUi();
  if (diagnosticsDrawer != nullptr)
    diagnosticsDrawer->refreshArtifacts();
}

void EditorHandle::Impl::rebuildAll(bool rebuildRuntime) {
  if (canvas != nullptr)
    canvas->rebuildNodeComponents();
  if (rebuildRuntime)
    runtime.buildGraph(doc);
  if (propertiesPanel != nullptr) {
    propertiesPanel->setParamProvider(&runtime);
    propertiesPanel->refreshFromDocument();
  }
  if (controlSourceInspector != nullptr)
    controlSourceInspector->refreshFromDocument();
  if (systemEndpointInspector != nullptr)
    systemEndpointInspector->refreshFromDocument();

  lastDocumentRevision = doc.getDocumentRevision();
  lastRuntimeRevision = doc.getRuntimeRevision();
  refreshRailUi();
  refreshRuntimeUi(true);
  refreshDocumentNoticeUi(true);
  refreshSessionStatusUi(true);
  owner.resized();
}

void EditorHandle::Impl::handleSelectionChanged(
    const std::vector<NodeId> &selectedNodeIds) {
  if (propertiesPanel == nullptr)
    return;

  if (!selectedNodeIds.empty())
    clearRailInspectors();

  if (selectedNodeIds.size() == 1)
    inspectNodeWithReveal(selectedNodeIds.front());
  else
    propertiesPanel->hidePanel();
}

void EditorHandle::Impl::handleFrameSelectionChanged(int frameId) {
  if (propertiesPanel == nullptr)
    return;

  if (frameId > 0) {
    clearRailInspectors();
    propertiesPanel->inspectFrame(frameId);
    return;
  }

  if (canvas == nullptr || canvas->getSelectedNodeIds().empty())
    propertiesPanel->hidePanel();
}

void EditorHandle::Impl::openProperties(NodeId nodeId) {
  if (propertiesPanel != nullptr)
    inspectNodeWithReveal(nodeId);
}

void EditorHandle::Impl::inspectNodeWithReveal(NodeId nodeId) {
  if (propertiesPanel == nullptr)
    return;

  clearRailInspectors();

  const bool wasOpen = propertiesPanel->isPanelOpen();
  propertiesPanel->inspectNode(nodeId);

  if (!wasOpen && canvas != nullptr)
    canvas->ensureNodeVisible(nodeId, 28.0f);
}

void EditorHandle::Impl::inspectControlSource(const juce::String &sourceId) {
  if (controlSourceInspector == nullptr)
    return;

  const auto normalized = sourceId.trim();
  if (normalized.isEmpty()) {
    clearControlSourceInspector();
    return;
  }

  if (propertiesPanel != nullptr)
    propertiesPanel->hidePanel();
  if (systemEndpointInspector != nullptr)
    systemEndpointInspector->hidePanel();

  controlSourceInspector->inspectSource(normalized);
  refreshRailUi();
}

void EditorHandle::Impl::inspectSystemEndpoint(const juce::String &endpointId) {
  if (systemEndpointInspector == nullptr)
    return;

  const auto normalized = endpointId.trim();
  if (normalized.isEmpty()) {
    clearSystemEndpointInspector();
    return;
  }

  if (propertiesPanel != nullptr)
    propertiesPanel->hidePanel();
  if (controlSourceInspector != nullptr)
    controlSourceInspector->hidePanel();

  systemEndpointInspector->inspectEndpoint(normalized);
  refreshRailUi();
}

void EditorHandle::Impl::clearControlSourceInspector() {
  if (controlSourceInspector != nullptr)
    controlSourceInspector->hidePanel();
  refreshRailUi();
}

void EditorHandle::Impl::clearSystemEndpointInspector() {
  if (systemEndpointInspector != nullptr)
    systemEndpointInspector->hidePanel();
  refreshRailUi();
}

void EditorHandle::Impl::clearRailInspectors() {
  if (controlSourceInspector != nullptr)
    controlSourceInspector->hidePanel();
  if (systemEndpointInspector != nullptr)
    systemEndpointInspector->hidePanel();
  refreshRailUi();
}

bool EditorHandle::Impl::focusDiagnosticTarget(const juce::String &graphId,
                                               const juce::String &query) {
  juce::ignoreUnused(graphId);
  if (canvas == nullptr)
    return false;

  return canvas->focusNodeByQuery(query);
}

void EditorHandle::Impl::refreshDocumentNoticeUi(bool force) {
  if (documentNoticeBanner == nullptr)
    return;

  const auto noticeRevision = doc.getTransientNoticeRevision();
  if (!force && noticeRevision == lastDocumentNoticeRevision)
    return;

  const bool wasVisible = documentNoticeBanner->isVisible();
  documentNoticeBanner->setNotice(doc.getTransientNotice());
  lastDocumentNoticeRevision = noticeRevision;

  if (wasVisible != documentNoticeBanner->isVisible())
    owner.resized();
}

void EditorHandle::Impl::refreshSessionStatusUi(bool force) {
  const bool dirty =
      doc.getDocumentRevision() != sessionStatus.lastPersistedDocumentRevision;
  const auto autosaveMillis = sessionStatus.lastAutosaveTime.toMilliseconds();
  if (!force && dirty == lastSessionDirty &&
      sessionStatus.hasAutosaveSnapshot == lastSessionHasAutosaveSnapshot &&
      autosaveMillis == lastSessionAutosaveMillis) {
    return;
  }

  if (runtimeStatusStrip != nullptr)
    runtimeStatusStrip->setSessionStatus(sessionStatus, dirty);

  if (presetBrowserPanel != nullptr) {
    juce::String summary;
    juce::String detail;
    if (dirty) {
      summary = "Session: Dirty";
      detail = sessionStatus.hasAutosaveSnapshot
                   ? "Latest autosave " +
                         (autosaveMillis > 0
                              ? sessionStatus.lastAutosaveTime.formatted(
                                    "%Y-%m-%d %H:%M")
                              : juce::String("time unavailable"))
                   : juce::String("No autosave snapshot has been written yet.");
    } else {
      summary = "Session: Saved";
      detail = sessionStatus.hasAutosaveSnapshot
                   ? "Autosave is up to date as of " +
                         (autosaveMillis > 0
                              ? sessionStatus.lastAutosaveTime.formatted(
                                    "%Y-%m-%d %H:%M")
                              : juce::String("time unavailable"))
                   : juce::String("Waiting for the first autosave snapshot.");
    }

    if (sessionStatus.hasAutosaveSnapshot)
      detail << " | Use Recovery to inspect or discard the snapshot.";

    presetBrowserPanel->setSessionPreview(summary, detail, dirty);
  }

  lastSessionDirty = dirty;
  lastSessionHasAutosaveSnapshot = sessionStatus.hasAutosaveSnapshot;
  lastSessionAutosaveMillis = autosaveMillis;
}

void EditorHandle::Impl::refreshRailUi(bool relayout) {
  juce::String selectedEndpointId;
  juce::String selectedEndpointRailId;
  if (systemEndpointInspector != nullptr)
    selectedEndpointId = systemEndpointInspector->selectedEndpointId();
  if (const auto *endpoint = doc.controlState.findEndpoint(selectedEndpointId))
    selectedEndpointRailId = endpoint->railId;

  if (inputRail != nullptr) {
    inputRail->setSelectedCardId(selectedEndpointRailId == "input-rail"
                                     ? selectedEndpointId
                                     : juce::String());
    inputRail->refreshFromDocument();
  }

  if (outputRail != nullptr) {
    outputRail->setSelectedCardId(selectedEndpointRailId == "output-rail"
                                      ? selectedEndpointId
                                      : juce::String());
    outputRail->refreshFromDocument();
  }

  if (controlRail != nullptr) {
    const auto selectedSourceId = controlSourceInspector != nullptr
                                      ? controlSourceInspector->selectedSourceId()
                                      : juce::String();
    controlRail->setSelectedCardId(selectedSourceId);
    controlRail->refreshFromDocument();
  }

  if (relayout)
    owner.resized();
}

void EditorHandle::Impl::refreshRuntimeUi(bool forceMessage) {
  const auto stats = runtime.getRuntimeStats();

  if ((stats.xrunDetected && !lastRuntimeStats.xrunDetected)) {
    pushRuntimeMessage("Audio block exceeded budget", juce::Colour(0xffef4444),
                       72);
  } else if (stats.clipDetected && !lastRuntimeStats.clipDetected) {
    pushRuntimeMessage("Output clip detected", juce::Colour(0xfff97316), 72);
  } else if (stats.denormalDetected && !lastRuntimeStats.denormalDetected) {
    pushRuntimeMessage("Denormal activity detected", juce::Colour(0xffeab308),
                       66);
  } else if (stats.mutedFallbackActive && !lastRuntimeStats.mutedFallbackActive) {
    pushRuntimeMessage("Muted fallback is active", juce::Colour(0xff94a3b8),
                       60);
  } else if (stats.rebuildPending && !lastRuntimeStats.rebuildPending) {
    pushRuntimeMessage("Deferred apply queued for safe commit",
                       juce::Colour(0xfff59e0b), 66);
  } else if (!stats.rebuildPending && lastRuntimeStats.rebuildPending) {
    pushRuntimeMessage("Deferred apply committed", juce::Colour(0xff22c55e),
                       48);
  } else if (forceMessage ||
             stats.sampleRate != lastRuntimeStats.sampleRate ||
             stats.preparedBlockSize != lastRuntimeStats.preparedBlockSize ||
             stats.lastInputChannels != lastRuntimeStats.lastInputChannels ||
             stats.lastOutputChannels != lastRuntimeStats.lastOutputChannels) {
    pushRuntimeMessage(
        juce::String::formatted(
            "Runtime prepared: %.1f kHz / %d blk / %d in / %d out",
            stats.sampleRate * 0.001, stats.preparedBlockSize,
            stats.lastInputChannels, stats.lastOutputChannels),
        juce::Colour(0xff60a5fa), 42);
  }

  if (runtimeMessageTicksRemaining > 0) {
    --runtimeMessageTicksRemaining;
  } else if (runtimeMessageText.isNotEmpty()) {
    runtimeMessageText.clear();
  }

  if (runtimeStatusStrip != nullptr) {
    runtimeStatusStrip->setStats(stats);
    runtimeStatusStrip->setTransientMessage(runtimeMessageText,
                                            runtimeMessageAccent);
  }

  syncRuntimeViewButtons();
  if (canvas != nullptr) {
    TGraphCanvas::RuntimeOverlayState overlay;
    overlay.sampleRate = stats.sampleRate;
    overlay.blockSize = stats.preparedBlockSize;
    overlay.inputChannels = stats.lastInputChannels;
    overlay.outputChannels = stats.lastOutputChannels;
    overlay.activeNodeCount = stats.activeNodeCount;
    overlay.allocatedPortChannels = stats.allocatedPortChannels;
    overlay.smoothingActiveCount = stats.smoothingActiveCount;
    overlay.activeGeneration = stats.activeGeneration;
    overlay.pendingGeneration = stats.pendingGeneration;
    overlay.rebuildPending = stats.rebuildPending;
    overlay.clipDetected = stats.clipDetected;
    overlay.denormalDetected = stats.denormalDetected;
    overlay.xrunDetected = stats.xrunDetected;
    overlay.mutedFallbackActive = stats.mutedFallbackActive;
    overlay.cpuLoadPercent = stats.cpuLoadPercent;
    canvas->setRuntimeOverlayState(overlay);
  }

  lastRuntimeStats = stats;
}
void EditorHandle::Impl::syncRuntimeViewButtons() {
  auto syncButton = [](juce::TextButton &button, bool enabled,
                       juce::Colour accent, const juce::String &onText,
                       const juce::String &offText) {
    button.setToggleState(enabled, juce::dontSendNotification);
    button.setButtonText(enabled ? onText : offText);
    button.setColour(juce::TextButton::buttonOnColourId,
                     accent.withAlpha(0.92f));
    button.setColour(juce::TextButton::buttonColourId,
                     enabled ? accent.withAlpha(0.58f)
                             : juce::Colour(0xff111827));
    button.setColour(juce::TextButton::textColourOnId,
                     juce::Colours::white.withAlpha(0.99f));
    button.setColour(juce::TextButton::textColourOffId,
                     enabled ? juce::Colours::white.withAlpha(0.97f)
                             : juce::Colours::white.withAlpha(0.78f));
  };

  if (canvas != nullptr) {
    syncButton(toggleHeatmapButton, canvas->isRuntimeHeatmapEnabled(),
               juce::Colour(0xfff97316), "Heat On", "Heat");
    syncButton(toggleProbeButton, canvas->isLiveProbeEnabled(),
               juce::Colour(0xff60a5fa), "Probe On", "Probe");
    syncButton(toggleOverlayButton, canvas->isDebugOverlayEnabled(),
               juce::Colour(0xff22c55e), "Overlay On", "Overlay");
  }

  syncButton(toggleDiagnosticsButton,
             diagnosticsDrawer != nullptr && diagnosticsDrawer->isDrawerOpen(),
             juce::Colour(0xff38bdf8), "Diagnostics On", "Diagnostics");
  syncButton(togglePresetsButton,
             presetBrowserPanel != nullptr && presetBrowserPanel->isBrowserOpen(),
             juce::Colour(0xff8b5cf6), "Presets On", "Presets");
}
void EditorHandle::Impl::pushRuntimeMessage(const juce::String &text,
                                            juce::Colour accent,
                                            int ticks) {
  runtimeMessageText = text;
  runtimeMessageAccent = accent;
  runtimeMessageTicksRemaining = juce::jmax(1, ticks);
}

} // namespace Teul
