#include "TGraphCanvas.h"
#include "../TeulPalette.h"

#include "../../History/TCommands.h"
#include "../../Registry/TNodeRegistry.h"
#include "../Node/TNodeComponent.h"
#include "../Port/TPortComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace Teul {

static const TNodeRegistry *getSharedRegistry() {
  static auto reg = makeDefaultNodeRegistry();
  return reg.get();
}


static juce::String toLowerCase(const juce::String &text) {
  return text.toLowerCase();
}

static juce::String nodeLabelForUi(const TNode &node,
                                   const TNodeRegistry *registry) {
  if (node.label.isNotEmpty())
    return node.label;

  if (registry != nullptr) {
    if (const auto *desc = registry->descriptorFor(node.typeKey)) {
      if (desc->displayName.isNotEmpty())
        return desc->displayName;
    }
  }

  return node.typeKey;
}

static juce::String categoryLabelForTypeKey(const juce::String &typeKey) {
  const auto tail = typeKey.fromFirstOccurrenceOf("Teul.", false, false);
  const int dot = tail.indexOfChar('.');
  return dot > 0 ? tail.substring(0, dot) : "Node";
}

static int fuzzySubsequenceScore(const juce::String &textLower,
                                 const juce::String &queryLower) {
  if (queryLower.isEmpty())
    return 0;

  int scan = 0;
  int score = 0;
  int contiguous = 0;

  for (int qi = 0; qi < queryLower.length(); ++qi) {
    const auto qc = queryLower[qi];
    bool found = false;

    for (int ti = scan; ti < textLower.length(); ++ti) {
      if (textLower[ti] != qc)
        continue;

      const int gap = ti - scan;
      score += juce::jmax(2, 18 - gap * 2);
      contiguous = (ti == scan ? contiguous + 1 : 0);
      score += contiguous * 5;
      scan = ti + 1;
      found = true;
      break;
    }

    if (!found)
      return std::numeric_limits<int>::min();
  }

  score -= juce::jmax(0, textLower.length() - queryLower.length());
  return score;
}

static int scoreTextMatch(const juce::String &textRaw,
                          const juce::String &queryRaw) {
  const juce::String text = toLowerCase(textRaw.trim());
  const juce::String query = toLowerCase(queryRaw.trim());
  if (query.isEmpty())
    return 1;
  if (text.isEmpty())
    return std::numeric_limits<int>::min();
  if (text == query)
    return 420;
  if (text.startsWith(query))
    return 340 - juce::jmin(80, text.length() - query.length());

  const int index = text.indexOf(query);
  if (index >= 0)
    return 260 - index * 3;

  const int fuzzy = fuzzySubsequenceScore(text, query);
  if (fuzzy != std::numeric_limits<int>::min())
    return 140 + fuzzy;

  return std::numeric_limits<int>::min();
}

static const char *kLibraryDragPrefix = "teul.node:";

static juce::String extractLibraryDragTypeKey(const juce::var &description) {
  const juce::String text = description.toString();
  if (!text.startsWith(kLibraryDragPrefix))
    return {};

  return text.fromFirstOccurrenceOf(kLibraryDragPrefix, false, false);
}

struct SearchEntry {
  juce::String title;
  juce::String subtitle;
  std::function<void()> onSelect;
};

class TGraphCanvas::SearchOverlay final : public juce::Component,
                                          private juce::ListBoxModel,
                                          private juce::TextEditor::Listener,
                                          private juce::KeyListener {
public:
  using Provider = std::function<std::vector<SearchEntry>(const juce::String &)>;

  explicit SearchOverlay(TGraphCanvas &ownerIn) : owner(ownerIn) {
    addAndMakeVisible(searchEditor);
    addAndMakeVisible(listBox);

    searchEditor.setTextToShowWhenEmpty("Search...", juce::Colours::grey);
    searchEditor.setColour(juce::TextEditor::backgroundColourId,
                           juce::Colour(0xff141414));
    searchEditor.setColour(juce::TextEditor::textColourId,
                           juce::Colours::white);
    searchEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colour(0xff2d2d2d));
    searchEditor.setEscapeAndReturnKeysConsumed(false);
    searchEditor.addListener(this);
    searchEditor.addKeyListener(this);
    searchEditor.setSelectAllWhenFocused(true);

    listBox.setModel(this);
    listBox.setRowHeight(38);
    listBox.setColour(juce::ListBox::backgroundColourId,
                      juce::Colour(0x00000000));
    listBox.addKeyListener(this);

    setVisible(false);
  }

  void present(const juce::String &titleIn, const juce::String &placeholderIn,
               Provider providerIn, bool anchoredIn,
               juce::Point<float> anchorIn) {
    title = titleIn;
    provider = std::move(providerIn);
    anchored = anchoredIn;
    anchorPoint = anchorIn;
    searchEditor.setTextToShowWhenEmpty(placeholderIn, juce::Colours::grey);
    searchEditor.setText({}, juce::dontSendNotification);
    setVisible(true);
    toFront(false);
    refreshItems();
    searchEditor.grabKeyboardFocus();
  }

  bool isOpen() const noexcept { return isVisible(); }

  void dismiss() {
    if (!isVisible())
      return;

    setVisible(false);
    items.clear();
    listBox.updateContent();
    owner.grabKeyboardFocus();
  }

  void resized() override {
    const int width = 420;
    const int rows = juce::jlimit(5, 9, juce::jmax(5, (int)items.size()));
    const int desiredHeight = 112 + rows * listBox.getRowHeight();
    const int height = juce::jmin(desiredHeight, getHeight() - 32);

    if (anchored) {
      int x = juce::roundToInt(anchorPoint.x - width * 0.35f);
      int y = juce::roundToInt(anchorPoint.y + 12.0f);
      x = juce::jlimit(16, juce::jmax(16, getWidth() - width - 16), x);
      y = juce::jlimit(16, juce::jmax(16, getHeight() - height - 16), y);
      panelBounds = {x, y, width, height};
    } else {
      panelBounds = {(getWidth() - width) / 2,
                     juce::jmax(18, getHeight() / 7), width, height};
    }

    auto area = panelBounds.reduced(14);
    area.removeFromTop(22);
    searchEditor.setBounds(area.removeFromTop(28));
    area.removeFromTop(10);
    listBox.setBounds(area);
  }

  void paint(juce::Graphics &g) override {
    if (!isVisible())
      return;

    g.fillAll(juce::Colour(0x66000000));
    g.setColour(juce::Colour(0xff111111));
    g.fillRoundedRectangle(panelBounds.toFloat(), 12.0f);
    g.setColour(juce::Colour(0xff2f2f2f));
    g.drawRoundedRectangle(panelBounds.toFloat(), 12.0f, 1.0f);

    auto titleBounds = panelBounds.withHeight(34).reduced(14, 8);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(title, titleBounds, juce::Justification::centredLeft, false);

    if (items.empty()) {
      g.setColour(juce::Colours::white.withAlpha(0.45f));
      g.setFont(13.0f);
      g.drawText("No matches", listBox.getBounds(),
                 juce::Justification::centred, false);
    }
  }

  void mouseDown(const juce::MouseEvent &event) override {
    if (!panelBounds.contains(event.getPosition()))
      dismiss();
  }

private:
  int getNumRows() override { return (int)items.size(); }

  void paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override {
    if (row < 0 || row >= (int)items.size())
      return;

    const auto &item = items[(size_t)row];
    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1);

    if (rowIsSelected) {
      g.setColour(juce::Colour(0xff234a7e));
      g.fillRoundedRectangle(bounds.toFloat(), 7.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(13.0f);
    g.drawText(item.title, bounds.removeFromTop(18).reduced(10, 0),
               juce::Justification::centredLeft, false);

    g.setColour(juce::Colours::white.withAlpha(0.52f));
    g.setFont(11.0f);
    g.drawText(item.subtitle, bounds.reduced(10, 0),
               juce::Justification::centredLeft, false);
  }

  void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override {
    activateRow(row);
  }

  void returnKeyPressed(int row) override { activateRow(row); }

  void textEditorTextChanged(juce::TextEditor &) override { refreshItems(); }

  bool keyPressed(const juce::KeyPress &key, juce::Component *) override {
    if (key == juce::KeyPress::escapeKey) {
      dismiss();
      return true;
    }

    if (key == juce::KeyPress::returnKey) {
      activateRow(listBox.getSelectedRow());
      return true;
    }

    if (key == juce::KeyPress::downKey || key == juce::KeyPress::tabKey) {
      moveSelection(1);
      return true;
    }

    if (key == juce::KeyPress::upKey) {
      moveSelection(-1);
      return true;
    }

    return false;
  }

  void refreshItems() {
    items = provider ? provider(searchEditor.getText()) : std::vector<SearchEntry>{};
    listBox.updateContent();

    if (items.empty()) {
      listBox.selectRow(-1);
    } else {
      const int current = juce::jlimit(0, (int)items.size() - 1,
                                       juce::jmax(0, listBox.getSelectedRow()));
      listBox.selectRow(current);
      listBox.scrollToEnsureRowIsOnscreen(current);
    }

    resized();
    repaint();
  }

  void moveSelection(int delta) {
    if (items.empty())
      return;

    const int row = juce::jlimit(0, (int)items.size() - 1,
                                 juce::jmax(0, listBox.getSelectedRow()) + delta);
    listBox.selectRow(row);
    listBox.scrollToEnsureRowIsOnscreen(row);
  }

  void activateRow(int row) {
    if (row < 0 || row >= (int)items.size())
      return;

    auto action = items[(size_t)row].onSelect;
    dismiss();
    if (action)
      action();
  }

  TGraphCanvas &owner;
  juce::TextEditor searchEditor;
  juce::ListBox listBox;
  juce::Rectangle<int> panelBounds;
  juce::String title;
  Provider provider;
  std::vector<SearchEntry> items;
  bool anchored = false;
  juce::Point<float> anchorPoint;
};

TGraphCanvas::TGraphCanvas(TGraphDocument &doc) : document(doc) {
  setWantsKeyboardFocus(true);

  nodeRegistry = getSharedRegistry();

  viewOriginWorld = {doc.meta.canvasOffsetX, doc.meta.canvasOffsetY};
  zoomLevel = doc.meta.canvasZoom;

  if (zoomLevel < 0.1f)
    zoomLevel = 1.0f;

  rebuildNodeComponents();

  searchOverlay = std::make_unique<SearchOverlay>(*this);
  addAndMakeVisible(searchOverlay.get());
  searchOverlay->setBounds(getLocalBounds());
  searchOverlay->setVisible(false);

  startTimerHz(30);
}

TGraphCanvas::~TGraphCanvas() {
  stopTimer();
  document.meta.canvasOffsetX = viewOriginWorld.x;
  document.meta.canvasOffsetY = viewOriginWorld.y;
  document.meta.canvasZoom = zoomLevel;
}

void TGraphCanvas::setConnectionLevelProvider(ConnectionLevelProvider provider) {
  connectionLevelProvider = std::move(provider);
}

void TGraphCanvas::setNodePropertiesRequestHandler(
    NodePropertiesRequestHandler handler) {
  nodePropertiesRequestHandler = std::move(handler);
}

void TGraphCanvas::setNodeSelectionChangedHandler(
    NodeSelectionChangedHandler handler) {
  nodeSelectionChangedHandler = std::move(handler);
}

juce::Point<float> TGraphCanvas::getViewCenter() const {
  return {getWidth() * 0.5f, getHeight() * 0.5f};
}

std::vector<const TNodeDescriptor *> TGraphCanvas::getAllNodeDescriptors() const {
  std::vector<const TNodeDescriptor *> result;
  if (nodeRegistry == nullptr)
    return result;

  const auto &all = nodeRegistry->getAllDescriptors();
  result.reserve(all.size());
  for (const auto &desc : all)
    result.push_back(&desc);

  std::stable_sort(result.begin(), result.end(),
                   [](const TNodeDescriptor *a, const TNodeDescriptor *b) {
                     if (a->category != b->category)
                       return a->category < b->category;
                     return a->displayName < b->displayName;
                   });

  return result;
}

int TGraphCanvas::scoreDescriptorMatch(const TNodeDescriptor &desc,
                                       const juce::String &query) const {
  const juce::String q = toLowerCase(query.trim());

  int best = scoreTextMatch(desc.displayName, q);
  best = juce::jmax(best, scoreTextMatch(desc.typeKey, q));
  best = juce::jmax(best, scoreTextMatch(desc.category, q));

  if (best == std::numeric_limits<int>::min())
    return best;

  auto recentIt = std::find(recentNodeTypes.begin(), recentNodeTypes.end(),
                            desc.typeKey);
  if (recentIt != recentNodeTypes.end()) {
    const int recentIndex = (int)std::distance(recentNodeTypes.begin(), recentIt);
    best += juce::jmax(18, 96 - recentIndex * 8);
  }

  if (q.isEmpty())
    best += 10;

  return best;
}

bool TGraphCanvas::insertNodeOnConnection(ConnectionId connectionId,
                                          NodeId insertedNodeId) {
  const TConnection *existing = document.findConnection(connectionId);
  TNode *inserted = document.findNode(insertedNodeId);
  if (existing == nullptr || inserted == nullptr)
    return false;

  const TConnection original = *existing;
  const TPort *sourcePort = findPortModel(original.from.nodeId, original.from.portId);
  if (sourcePort == nullptr)
    return false;

  PortId inPortId = kInvalidPortId;
  PortId outPortId = kInvalidPortId;

  for (const auto &port : inserted->ports) {
    if (port.direction == TPortDirection::Input &&
        port.dataType == sourcePort->dataType) {
      inPortId = port.portId;
      break;
    }
  }

  for (const auto &port : inserted->ports) {
    if (port.direction == TPortDirection::Output &&
        port.dataType == sourcePort->dataType) {
      outPortId = port.portId;
      break;
    }
  }

  if (inPortId == kInvalidPortId || outPortId == kInvalidPortId)
    return false;

  document.executeCommand(std::make_unique<DeleteConnectionCommand>(connectionId));

  TConnection left;
  left.connectionId = document.allocConnectionId();
  left.from = original.from;
  left.to = {insertedNodeId, inPortId};
  document.executeCommand(std::make_unique<AddConnectionCommand>(left));

  TConnection right;
  right.connectionId = document.allocConnectionId();
  right.from = {insertedNodeId, outPortId};
  right.to = original.to;
  document.executeCommand(std::make_unique<AddConnectionCommand>(right));

  selectedConnectionId = right.connectionId;
  return true;
}

bool TGraphCanvas::addNodeByTypeAtView(const juce::String &typeKey,
                                       juce::Point<float> viewPos,
                                       bool tryInsertOnWire) {
  if (nodeRegistry == nullptr)
    return false;

  const TNodeDescriptor *desc = nodeRegistry->descriptorFor(typeKey);
  if (desc == nullptr)
    return false;

  TNode node;
  node.nodeId = document.allocNodeId();
  node.typeKey = desc->typeKey;

  const auto world = viewToWorld(viewPos);
  node.x = world.x;
  node.y = world.y;

  for (const auto &param : desc->paramSpecs)
    node.params[param.key] = param.defaultValue;

  for (const auto &portSpec : desc->portSpecs) {
    TPort port;
    port.portId = document.allocPortId();
    port.direction = portSpec.direction;
    port.dataType = portSpec.dataType;
    port.name = portSpec.name;
    port.ownerNodeId = node.nodeId;
    node.ports.push_back(port);
  }

  document.executeCommand(std::make_unique<AddNodeCommand>(node));
  rememberRecentNode(desc->typeKey);

  if (tryInsertOnWire) {
    const ConnectionId hit = hitTestConnection(viewPos, 10.0f);
    if (hit != kInvalidConnectionId)
      insertNodeOnConnection(hit, node.nodeId);
  }

  rebuildNodeComponents();
  pushStatusHint("Added " + desc->displayName + ".");
  return true;
}

void TGraphCanvas::focusNode(NodeId nodeId) {
  const TNode *node = document.findNode(nodeId);
  if (node == nullptr)
    return;

  viewOriginWorld.x = node->x - (getWidth() * 0.5f) / zoomLevel;
  viewOriginWorld.y = node->y - (getHeight() * 0.5f) / zoomLevel;
  updateChildPositions();
  repaint();
}

bool TGraphCanvas::focusNodeByQuery(const juce::String &query) {
  const juce::String q = toLowerCase(query.trim());
  if (q.isEmpty())
    return false;

  NodeId bestNodeId = kInvalidNodeId;
  int bestScore = std::numeric_limits<int>::min();

  for (const auto &node : document.nodes) {
    const TNodeDescriptor *desc =
        nodeRegistry ? nodeRegistry->descriptorFor(node.typeKey) : nullptr;

    int score = scoreTextMatch(node.label, q);
    score = juce::jmax(score, scoreTextMatch(node.typeKey, q));
    if (desc != nullptr)
      score = juce::jmax(score, scoreTextMatch(desc->displayName, q));

    if (score > bestScore) {
      bestScore = score;
      bestNodeId = node.nodeId;
    }
  }

  if (bestNodeId == kInvalidNodeId || bestScore == std::numeric_limits<int>::min())
    return false;

  focusNode(bestNodeId);
  return true;
}

void TGraphCanvas::rebuildNodeComponents() {
  nodeComponents.clear();

  for (const auto &n : document.nodes) {
    const TNodeDescriptor *desc = nullptr;
    if (nodeRegistry)
      desc = nodeRegistry->descriptorFor(n.typeKey);

    auto comp = std::make_unique<TNodeComponent>(*this, n.nodeId, desc);
    addAndMakeVisible(comp.get());
    nodeComponents.push_back(std::move(comp));
  }

  if (searchOverlay != nullptr)
    searchOverlay->toFront(false);

  updateChildPositions();
  syncNodeSelectionToComponents();
  repaint();
}

void TGraphCanvas::updateChildPositions() {
  for (auto &tnc : nodeComponents) {
    const TNode *nodeData = document.findNode(tnc->getNodeId());
    if (!nodeData)
      continue;

    const bool hidden = isNodeHiddenByCollapsedFrame(*nodeData);
    tnc->setVisible(!hidden);
    if (hidden)
      continue;

    tnc->setTopLeftPosition(
        worldToView(juce::Point<float>(nodeData->x, nodeData->y)).roundToInt());
    tnc->setTransform(juce::AffineTransform::scale(zoomLevel));
  }

  recalcMiniMapCache();
}

void TGraphCanvas::paint(juce::Graphics &g) {
  g.fillAll(TeulPalette::CanvasBackground);
  drawInfiniteGrid(g);
  drawFrames(g);
  drawConnections(g);
  drawSelectionOverlay(g);
  drawMiniMap(g);
  drawLibraryDropPreview(g);
  drawZoomIndicator(g);
  drawStatusHint(g);
}

void TGraphCanvas::resized() {
  updateChildPositions();
  if (searchOverlay != nullptr)
    searchOverlay->setBounds(getLocalBounds());
}

void TGraphCanvas::drawInfiniteGrid(juce::Graphics &g) {
  g.saveState();

  const float baseGridSize = 40.0f;
  const float scaledGridSize = baseGridSize * zoomLevel;

  if (scaledGridSize < 5.0f) {
    g.restoreState();
    return;
  }

  const auto localBounds = getLocalBounds().toFloat();
  juce::Rectangle<float> worldBounds(viewOriginWorld.x, viewOriginWorld.y,
                                     localBounds.getWidth() / zoomLevel,
                                     localBounds.getHeight() / zoomLevel);

  const float startX =
      std::floor(worldBounds.getX() / baseGridSize) * baseGridSize;
  const float startY =
      std::floor(worldBounds.getY() / baseGridSize) * baseGridSize;

  g.setColour(TeulPalette::GridDot);

  for (float wx = startX; wx < worldBounds.getRight(); wx += baseGridSize) {
    for (float wy = startY; wy < worldBounds.getBottom(); wy += baseGridSize) {
      const float vx = (wx - viewOriginWorld.x) * zoomLevel;
      const float vy = (wy - viewOriginWorld.y) * zoomLevel;
      g.fillRect(vx - 1.0f, vy - 1.0f, 2.0f, 2.0f);
    }
  }

  g.restoreState();
}

void TGraphCanvas::drawZoomIndicator(juce::Graphics &g) {
  juce::String zoomText =
      juce::String(juce::roundToInt(zoomLevel * 100.0f)) + "%";

  auto area =
      getLocalBounds().removeFromBottom(30).removeFromRight(80).reduced(5);
  g.setColour(juce::Colour(0x80000000));
  g.fillRoundedRectangle(area.toFloat(), 4.0f);

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(zoomText, area, juce::Justification::centred, false);
}

juce::Colour TGraphCanvas::colorForPortType(TPortDataType type) const {
  switch (type) {
  case TPortDataType::Audio:
    return TeulPalette::PortAudio;
  case TPortDataType::CV:
    return TeulPalette::PortCV;
  case TPortDataType::Gate:
    return TeulPalette::PortGate;
  case TPortDataType::MIDI:
    return TeulPalette::PortMIDI;
  case TPortDataType::Control:
    return TeulPalette::PortControl;
  default:
    return TeulPalette::WireNormal;
  }
}

juce::Path TGraphCanvas::makeWirePath(juce::Point<float> from,
                                      juce::Point<float> to) const {
  juce::Path path;
  path.startNewSubPath(from);

  const float dx = std::abs(to.x - from.x);
  const float tangent = juce::jlimit(40.0f, 220.0f, dx * 0.5f);

  path.cubicTo(from.x + tangent, from.y, to.x - tangent, to.y, to.x, to.y);
  return path;
}

const TPort *TGraphCanvas::findPortModel(NodeId nodeId, PortId portId) const {
  if (const TNode *node = document.findNode(nodeId))
    return node->findPort(portId);

  return nullptr;
}

TNodeComponent *TGraphCanvas::findNodeComponent(NodeId nodeId) noexcept {
  for (auto &n : nodeComponents) {
    if (n->getNodeId() == nodeId)
      return n.get();
  }
  return nullptr;
}

const TNodeComponent *
TGraphCanvas::findNodeComponent(NodeId nodeId) const noexcept {
  for (const auto &n : nodeComponents) {
    if (n->getNodeId() == nodeId)
      return n.get();
  }
  return nullptr;
}

TPortComponent *TGraphCanvas::findPortComponent(NodeId nodeId,
                                                PortId portId) noexcept {
  if (auto *nodeComp = findNodeComponent(nodeId))
    return nodeComp->findPortComponent(portId);

  return nullptr;
}

const TPortComponent *
TGraphCanvas::findPortComponent(NodeId nodeId, PortId portId) const noexcept {
  if (const auto *nodeComp = findNodeComponent(nodeId))
    return nodeComp->findPortComponent(portId);

  return nullptr;
}

juce::Point<float> TGraphCanvas::portCentreInCanvas(NodeId nodeId,
                                                     PortId portId) const {
  if (const auto *portComp = findPortComponent(nodeId, portId)) {
    const auto localCentre = portComp->getLocalBounds().getCentre();
    return getLocalPoint(portComp, localCentre).toFloat();
  }

  if (const auto *node = document.findNode(nodeId))
    return worldToView({node->x, node->y});

  return {};
}

ConnectionId TGraphCanvas::hitTestConnection(juce::Point<float> pointView,
                                             float hitThickness) const {
  for (auto it = document.connections.rbegin(); it != document.connections.rend();
       ++it) {
    const auto fromPos = portCentreInCanvas(it->from.nodeId, it->from.portId);
    const auto toPos = portCentreInCanvas(it->to.nodeId, it->to.portId);

    juce::Path hitPath;
    juce::PathStrokeType(hitThickness).createStrokedPath(
        hitPath, makeWirePath(fromPos, toPos));

    if (hitPath.contains(pointView.x, pointView.y))
      return it->connectionId;
  }

  return kInvalidConnectionId;
}

void TGraphCanvas::drawConnections(juce::Graphics &g) {
  for (const auto &conn : document.connections) {
    const auto fromPos = portCentreInCanvas(conn.from.nodeId, conn.from.portId);
    const auto toPos = portCentreInCanvas(conn.to.nodeId, conn.to.portId);

    const juce::Path wirePath = makeWirePath(fromPos, toPos);
    const TPort *sourcePort = findPortModel(conn.from.nodeId, conn.from.portId);
    const TPortDataType sourceType =
        sourcePort ? sourcePort->dataType : TPortDataType::Audio;

    const float level =
        connectionLevelProvider
            ? juce::jlimit(0.0f, 1.0f, connectionLevelProvider(conn))
            : 0.0f;

    const bool isSelected = (conn.connectionId == selectedConnectionId);
    const float alpha = isSelected
                            ? 1.0f
                            : juce::jlimit(0.55f, 0.95f, 0.75f + level * 0.2f);

    juce::Colour wireColor = colorForPortType(sourceType).withAlpha(alpha);
    if (isSelected)
      wireColor = wireColor.brighter(0.2f);

    g.setColour(wireColor);
    g.strokePath(
        wirePath,
        juce::PathStrokeType(isSelected ? 3.0f : 2.0f,
                             juce::PathStrokeType::curved,
                             juce::PathStrokeType::rounded));

    juce::Path pulsePath;
    const float dashLengths[2] = {7.0f, 34.0f};
    juce::PathStrokeType(2.8f, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded)
        .createDashedStroke(pulsePath, wirePath, dashLengths, 2,
                            juce::AffineTransform(),
                            flowPhase * (dashLengths[0] + dashLengths[1]));

    const float pulseAlpha = juce::jlimit(0.15f, 0.85f, 0.25f + level * 0.55f);
    g.setColour(wireColor.brighter(0.65f).withAlpha(pulseAlpha));
    g.fillPath(pulsePath);
  }

  if (wireDragState.active) {
    const juce::Path previewPath =
        makeWirePath(wireDragState.sourcePosView, wireDragState.mousePosView);

    const bool hasTarget = wireDragState.targetNodeId != kInvalidNodeId &&
                           wireDragState.targetPortId != kInvalidPortId;

    const bool connectable = hasTarget && isCurrentDragTargetConnectable();

    juce::Colour previewColor =
        colorForPortType(wireDragState.sourceType).withAlpha(0.9f);

    if (hasTarget && !connectable)
      previewColor = juce::Colours::red.withAlpha(0.9f);

    juce::Path dashed;
    const float dashes[2] = {9.0f, 6.0f};
    juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded)
        .createDashedStroke(dashed, previewPath, dashes, 2);

    g.setColour(previewColor);
    g.fillPath(dashed);

    g.setColour(previewColor.withAlpha(0.65f));
    g.fillEllipse(juce::Rectangle<float>(wireDragState.mousePosView.x - 3.0f,
                                         wireDragState.mousePosView.y - 3.0f,
                                         6.0f, 6.0f));
  }

  if (disconnectAnimation.active) {
    juce::Path hanging = makeWirePath(disconnectAnimation.anchorPosView,
                                      disconnectAnimation.loosePosView);

    juce::Colour animColor =
        colorForPortType(disconnectAnimation.type)
            .withAlpha(0.75f * disconnectAnimation.alpha);

    g.setColour(animColor);
    g.strokePath(hanging,
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    g.setColour(animColor.brighter(0.3f).withAlpha(disconnectAnimation.alpha));
    g.fillEllipse(juce::Rectangle<float>(disconnectAnimation.loosePosView.x - 4.0f,
                                         disconnectAnimation.loosePosView.y - 4.0f,
                                         8.0f, 8.0f));
  }
}


void TGraphCanvas::drawMiniMap(juce::Graphics &g) {
  const float miniW = 220.0f;
  const float miniH = 140.0f;
  const float margin = 12.0f;

  miniMapRectView = {getWidth() - miniW - margin, getHeight() - miniH - margin,
                     miniW, miniH};
  recalcMiniMapCache();

  const float sx = miniW / miniMapWorldBounds.getWidth();
  const float sy = miniH / miniMapWorldBounds.getHeight();

  auto worldToMini = [&](juce::Point<float> p) {
    return juce::Point<float>(
        miniMapRectView.getX() + (p.x - miniMapWorldBounds.getX()) * sx,
        miniMapRectView.getY() + (p.y - miniMapWorldBounds.getY()) * sy);
  };

  g.setColour(juce::Colour(0xe0121212));
  g.fillRoundedRectangle(miniMapRectView, 6.0f);
  g.setColour(juce::Colour(0xff3b3b3b));
  g.drawRoundedRectangle(miniMapRectView, 6.0f, 1.0f);

  for (const auto &frame : document.frames) {
    auto topLeft = worldToMini({frame.x, frame.y});
    auto bottomRight = worldToMini({frame.x + frame.width, frame.y + frame.height});
    juce::Rectangle<float> rect(
        juce::jmin(topLeft.x, bottomRight.x),
        juce::jmin(topLeft.y, bottomRight.y),
        std::abs(bottomRight.x - topLeft.x),
        std::abs(bottomRight.y - topLeft.y));
    g.setColour(juce::Colour(frame.colorArgb).withAlpha(0.35f));
    g.fillRect(rect);
  }

  for (const auto &node : document.nodes) {
    auto topLeft = worldToMini({node.x, node.y});
    g.setColour(isNodeSelected(node.nodeId) ? juce::Colours::white
                                            : juce::Colour(0xff9ca3af));
    g.fillRect(topLeft.x, topLeft.y, 160.0f * sx, 90.0f * sy);
  }

  const juce::Rectangle<float> viewportWorld(viewOriginWorld.x, viewOriginWorld.y,
                                             getWidth() / zoomLevel,
                                             getHeight() / zoomLevel);
  auto viewTopLeft = worldToMini(viewportWorld.getTopLeft());
  miniMapViewportRectView = {viewTopLeft.x, viewTopLeft.y,
                             viewportWorld.getWidth() * sx,
                             viewportWorld.getHeight() * sy};

  g.setColour(juce::Colours::white.withAlpha(0.9f));
  g.drawRect(miniMapViewportRectView, 1.2f);
}

void TGraphCanvas::clearDragTargetHighlight() {
  for (auto &node : nodeComponents) {
    for (const auto &port : node->getInputPorts()) {
      port->setDragTargetHighlight(false, true);
    }
  }
}

void TGraphCanvas::updateDragTargetFromMouse(juce::Point<float> mousePosView) {
  clearDragTargetHighlight();

  wireDragState.targetNodeId = kInvalidNodeId;
  wireDragState.targetPortId = kInvalidPortId;
  wireDragState.targetTypeMatch = false;
  wireDragState.targetCycleFree = false;

  if (!wireDragState.active)
    return;

  const auto mousePosInt = mousePosView.roundToInt();

  for (auto &node : nodeComponents) {
    for (const auto &inputPort : node->getInputPorts()) {
      const auto hitArea =
          getLocalArea(inputPort.get(), inputPort->getLocalBounds()).expanded(4);

      if (!hitArea.contains(mousePosInt))
        continue;

      const auto &candidatePort = inputPort->getPortData();
      wireDragState.targetNodeId = candidatePort.ownerNodeId;
      wireDragState.targetPortId = candidatePort.portId;
      wireDragState.targetTypeMatch =
          (candidatePort.dataType == wireDragState.sourceType);
      wireDragState.targetCycleFree =
          !document.wouldCreateCycle(wireDragState.sourceNodeId,
                                     candidatePort.ownerNodeId);

      inputPort->setDragTargetHighlight(true, isCurrentDragTargetConnectable());
      return;
    }
  }
}

bool TGraphCanvas::isCurrentDragTargetConnectable() const {
  if (!wireDragState.active)
    return false;

  if (wireDragState.sourceNodeId == kInvalidNodeId ||
      wireDragState.sourcePortId == kInvalidPortId ||
      wireDragState.targetNodeId == kInvalidNodeId ||
      wireDragState.targetPortId == kInvalidPortId)
    return false;

  if (!wireDragState.targetTypeMatch || !wireDragState.targetCycleFree)
    return false;

  if (wireDragState.sourceNodeId == wireDragState.targetNodeId &&
      wireDragState.sourcePortId == wireDragState.targetPortId)
    return false;

  const TPort *sourcePort =
      findPortModel(wireDragState.sourceNodeId, wireDragState.sourcePortId);
  const TPort *targetPort =
      findPortModel(wireDragState.targetNodeId, wireDragState.targetPortId);

  if (!sourcePort || !targetPort)
    return false;

  if (sourcePort->direction != TPortDirection::Output ||
      targetPort->direction != TPortDirection::Input)
    return false;

  const bool duplicateExists =
      std::any_of(document.connections.begin(), document.connections.end(),
                  [&](const TConnection &conn) {
                    return conn.from.nodeId == wireDragState.sourceNodeId &&
                           conn.from.portId == wireDragState.sourcePortId &&
                           conn.to.nodeId == wireDragState.targetNodeId &&
                           conn.to.portId == wireDragState.targetPortId;
                  });

  return !duplicateExists;
}

void TGraphCanvas::tryCreateConnectionFromDrag() {
  if (!isCurrentDragTargetConnectable())
    return;

  TConnection conn;
  conn.connectionId = document.allocConnectionId();
  conn.from.nodeId = wireDragState.sourceNodeId;
  conn.from.portId = wireDragState.sourcePortId;
  conn.to.nodeId = wireDragState.targetNodeId;
  conn.to.portId = wireDragState.targetPortId;

  document.executeCommand(std::make_unique<AddConnectionCommand>(conn));
  selectedConnectionId = conn.connectionId;
}

void TGraphCanvas::beginConnectionDrag(const TPort &sourcePort,
                                       juce::Point<float> mousePosView) {
  if (sourcePort.direction != TPortDirection::Output)
    return;

  wireDragState = WireDragState{};
  wireDragState.active = true;
  wireDragState.sourceNodeId = sourcePort.ownerNodeId;
  wireDragState.sourcePortId = sourcePort.portId;
  wireDragState.sourceType = sourcePort.dataType;
  wireDragState.mousePosView = mousePosView;

  if (findPortComponent(sourcePort.ownerNodeId, sourcePort.portId) != nullptr)
    wireDragState.sourcePosView =
        portCentreInCanvas(sourcePort.ownerNodeId, sourcePort.portId);
  else
    wireDragState.sourcePosView = mousePosView;

  selectedConnectionId = kInvalidConnectionId;

  updateDragTargetFromMouse(mousePosView);
  repaint();
}

void TGraphCanvas::updateConnectionDrag(juce::Point<float> mousePosView) {
  if (!wireDragState.active)
    return;

  wireDragState.mousePosView = mousePosView;
  updateDragTargetFromMouse(mousePosView);
  repaint();
}

void TGraphCanvas::endConnectionDrag(juce::Point<float> mousePosView) {
  if (!wireDragState.active)
    return;

  wireDragState.mousePosView = mousePosView;
  updateDragTargetFromMouse(mousePosView);
  tryCreateConnectionFromDrag();
  cancelConnectionDrag();
}

void TGraphCanvas::cancelConnectionDrag() {
  clearDragTargetHighlight();
  wireDragState = WireDragState{};
  repaint();
}

void TGraphCanvas::startDisconnectAnimation(juce::Point<float> anchorPosView,
                                            juce::Point<float> loosePosView,
                                            juce::Point<float> impulseView,
                                            TPortDataType type) {
  disconnectAnimation.active = true;
  disconnectAnimation.anchorPosView = anchorPosView;
  disconnectAnimation.loosePosView = loosePosView;
  disconnectAnimation.velocityViewPerSecond = impulseView +
                                              juce::Point<float>(0.0f, 120.0f);
  disconnectAnimation.alpha = 1.0f;
  disconnectAnimation.type = type;
}

void TGraphCanvas::deleteConnectionWithAnimation(ConnectionId connectionId,
                                                 juce::Point<float> breakPointView,
                                                 juce::Point<float> impulseView) {
  const TConnection *conn = document.findConnection(connectionId);
  if (!conn)
    return;

  const auto fromPos = portCentreInCanvas(conn->from.nodeId, conn->from.portId);
  const auto toPos = portCentreInCanvas(conn->to.nodeId, conn->to.portId);

  const float fromDistSq = fromPos.getDistanceSquaredFrom(breakPointView);
  const float toDistSq = toPos.getDistanceSquaredFrom(breakPointView);

  const juce::Point<float> anchorPos = (fromDistSq <= toDistSq) ? toPos : fromPos;

  const TPort *sourcePort = findPortModel(conn->from.nodeId, conn->from.portId);
  const TPortDataType sourceType =
      sourcePort ? sourcePort->dataType : TPortDataType::Audio;

  startDisconnectAnimation(anchorPos, breakPointView, impulseView, sourceType);

  document.executeCommand(
      std::make_unique<DeleteConnectionCommand>(connectionId));

  if (selectedConnectionId == connectionId)
    selectedConnectionId = kInvalidConnectionId;

  repaint();
}

bool TGraphCanvas::isInterestedInDragSource(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  return extractLibraryDragTypeKey(dragSourceDetails.description).isNotEmpty();
}

void TGraphCanvas::itemDragEnter(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  itemDragMove(dragSourceDetails);
}

void TGraphCanvas::itemDragMove(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  const juce::String typeKey =
      extractLibraryDragTypeKey(dragSourceDetails.description);
  const auto point = dragSourceDetails.localPosition.toFloat();

  if (typeKey.isEmpty() || !getLocalBounds().contains(point.roundToInt())) {
    itemDragExit(dragSourceDetails);
    return;
  }

  libraryDropPreview.active = true;
  libraryDropPreview.pointView = point;
  libraryDropPreview.typeKey = typeKey;
  repaint();
}

void TGraphCanvas::itemDragExit(
    const juce::DragAndDropTarget::SourceDetails &) {
  if (!libraryDropPreview.active)
    return;

  libraryDropPreview = LibraryDropPreviewState{};
  repaint();
}

void TGraphCanvas::itemDropped(
    const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) {
  const juce::String typeKey =
      extractLibraryDragTypeKey(dragSourceDetails.description);
  const auto point = dragSourceDetails.localPosition.toFloat();
  itemDragExit(dragSourceDetails);

  if (typeKey.isEmpty() || !getLocalBounds().contains(point.roundToInt()))
    return;

  addNodeByTypeAtView(typeKey, point, true);
}

void TGraphCanvas::mouseDown(const juce::MouseEvent &event) {
  grabKeyboardFocus();

  if (event.mods.isRightButtonDown()) {
    const int frameId = hitTestFrame(event.position);
    if (frameId != 0) {
      showFrameContextMenu(
          frameId, event.position,
          localPointToGlobal(event.position.roundToInt()).toFloat());
      return;
    }

    showCanvasContextMenu(event.position,
                          localPointToGlobal(event.position.roundToInt()).toFloat());
    return;
  }

  if (miniMapRectView.contains(event.position)) {
    const auto world = miniMapToWorld(event.position);

    if (miniMapViewportRectView.contains(event.position)) {
      miniMapDragState.active = true;
      miniMapDragState.worldOffset = world - viewOriginWorld;
    } else {
      viewOriginWorld = {world.x - (getWidth() * 0.5f) / zoomLevel,
                         world.y - (getHeight() * 0.5f) / zoomLevel};
      updateChildPositions();
      repaint();
    }
    return;
  }

  if (event.mods.isAltDown() && event.mods.isLeftButtonDown()) {
    const ConnectionId hit = hitTestConnection(event.position);
    if (hit != kInvalidConnectionId) {
      deleteConnectionWithAnimation(hit, event.position, {0.0f, 140.0f});
      connectionBreakDragArmed = false;
      pressedConnectionId = kInvalidConnectionId;
      return;
    }
  }

  if (event.mods.isLeftButtonDown() && !event.mods.isMiddleButtonDown()) {
    const ConnectionId hit = hitTestConnection(event.position);
    if (hit != kInvalidConnectionId) {
      selectedConnectionId = hit;
      pressedConnectionId = hit;
      pressedConnectionPointView = event.position;
      connectionBreakDragArmed = true;
      repaint();
      return;
    }

    if (selectedConnectionId != kInvalidConnectionId) {
      selectedConnectionId = kInvalidConnectionId;
      repaint();
    }

    const int frameId = hitTestFrame(event.position);
    if (frameId != 0 && isPointInFrameTitle(frameId, event.position)) {
      startFrameDrag(frameId, event.position);
      return;
    }
  }

  const bool isPanTrigger =
      event.mods.isMiddleButtonDown() || event.mods.isAltDown() ||
      juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);

  if (isPanTrigger) {
    panState.active = true;
    panState.startMouseView = event.position;
    panState.startViewOriginWorld = viewOriginWorld;
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    return;
  }

  if (event.mods.isLeftButtonDown()) {
    marqueeState.active = true;
    marqueeState.additive = event.mods.isShiftDown() || event.mods.isCtrlDown() ||
                            event.mods.isCommandDown();
    marqueeState.startView = event.position;
    marqueeState.rectView = {event.position.x, event.position.y, 0.0f, 0.0f};
    marqueeState.baseSelection =
        marqueeState.additive ? selectedNodeIds : std::vector<NodeId>{};

    if (!marqueeState.additive)
      clearNodeSelection();
  }
}

void TGraphCanvas::mouseDrag(const juce::MouseEvent &event) {
  if (miniMapDragState.active) {
    const auto world = miniMapToWorld(event.position);
    viewOriginWorld = world - miniMapDragState.worldOffset;
    updateChildPositions();
    repaint();
    return;
  }

  if (connectionBreakDragArmed && pressedConnectionId != kInvalidConnectionId) {
    const juce::Point<float> delta = event.position - pressedConnectionPointView;
    if (delta.getDistanceFromOrigin() > 6.0f) {
      deleteConnectionWithAnimation(pressedConnectionId, event.position,
                                    delta * 6.0f);
      connectionBreakDragArmed = false;
      pressedConnectionId = kInvalidConnectionId;
      return;
    }
  }

  if (panState.active) {
    const juce::Point<float> deltaView = event.position - panState.startMouseView;
    const juce::Point<float> deltaWorld = deltaView / zoomLevel;

    viewOriginWorld = panState.startViewOriginWorld - deltaWorld;
    updateChildPositions();
    repaint();
    return;
  }

  if (frameDragState.active) {
    updateFrameDrag(event.position);
    return;
  }

  if (marqueeState.active) {
    marqueeState.rectView = juce::Rectangle<float>::leftTopRightBottom(
        juce::jmin(marqueeState.startView.x, event.position.x),
        juce::jmin(marqueeState.startView.y, event.position.y),
        juce::jmax(marqueeState.startView.x, event.position.x),
        juce::jmax(marqueeState.startView.y, event.position.y));
    updateMarqueeSelection();
    repaint();
  }
}

void TGraphCanvas::mouseUp(const juce::MouseEvent &event) {
  juce::ignoreUnused(event);

  connectionBreakDragArmed = false;
  pressedConnectionId = kInvalidConnectionId;

  if (miniMapDragState.active)
    miniMapDragState = MiniMapDragState{};

  if (panState.active) {
    panState.active = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
  }

  if (frameDragState.active)
    endFrameDrag();

  if (marqueeState.active) {
    marqueeState.active = false;
    repaint();
  }
}

void TGraphCanvas::mouseWheelMove(const juce::MouseEvent &event,
                                  const juce::MouseWheelDetails &wheel) {
  if (event.mods.isCtrlDown() || event.mods.isCommandDown() ||
      event.mods.isAltDown()) {
    const float zoomDelta = wheel.deltaY * 2.0f;
    const float nextZoom = juce::jlimit(0.1f, 5.0f, zoomLevel + zoomDelta);
    setZoomLevel(nextZoom, event.position);
  } else {
    const float panSpeedPixels = 50.0f;
    const float deltaXWorld = (wheel.deltaX * panSpeedPixels) / zoomLevel;
    const float deltaYWorld = (wheel.deltaY * panSpeedPixels) / zoomLevel;

    viewOriginWorld.x -= deltaXWorld;
    viewOriginWorld.y -= deltaYWorld;
    updateChildPositions();
    repaint();
  }
}

bool TGraphCanvas::keyPressed(const juce::KeyPress &key) {
  if (key == juce::KeyPress::spaceKey) {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    return true;
  }

  const bool ctrlOrCmd = key.getModifiers().isCtrlDown() ||
                         key.getModifiers().isCommandDown();
  const bool alt = key.getModifiers().isAltDown();
  const auto ch =
      juce::CharacterFunctions::toLowerCase(key.getTextCharacter());

  if (ctrlOrCmd && ch == 'p') {
    showCommandPaletteOverlay();
    return true;
  }

  if (ctrlOrCmd && ch == 'f') {
    showNodeSearchOverlay();
    return true;
  }

  if (key == juce::KeyPress::tabKey) {
    showQuickAddPrompt(getViewCenter());
    return true;
  }

  if (key == juce::KeyPress::escapeKey) {
    if (wireDragState.active)
      cancelConnectionDrag();
    return true;
  }

  if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
    if (selectedConnectionId != kInvalidConnectionId) {
      const bool confirmed = juce::AlertWindow::showOkCancelBox(
          juce::MessageBoxIconType::WarningIcon, "Delete Connection",
          "Delete selected connection? Ctrl+Z will undo.", "OK", "Cancel", nullptr, nullptr);
      if (confirmed)
        deleteConnectionWithAnimation(selectedConnectionId, getViewCenter(),
                                      {0.0f, 120.0f});
      return true;
    }

    deleteSelectionWithPrompt();
    return true;
  }

  if (ctrlOrCmd && !key.getModifiers().isShiftDown() && ch == 'z') {
    if (document.undo()) {
      rebuildNodeComponents();
      pushStatusHint("Undo");
    }
    return true;
  }

  if ((ctrlOrCmd && key.getModifiers().isShiftDown() && ch == 'z') ||
      (ctrlOrCmd && ch == 'y')) {
    if (document.redo()) {
      rebuildNodeComponents();
      pushStatusHint("Redo");
    }
    return true;
  }

  if (ctrlOrCmd && ch == 'd') {
    duplicateSelection();
    return true;
  }

  if (ch == 'b') {
    toggleBypassSelection();
    return true;
  }

  if (ctrlOrCmd && ch == 'l') {
    alignSelectionLeft();
    return true;
  }

  if (ctrlOrCmd && ch == 't') {
    alignSelectionTop();
    return true;
  }

  if (ctrlOrCmd && alt && ch == 'h') {
    distributeSelectionHorizontally();
    return true;
  }

  if (ctrlOrCmd && alt && ch == 'v') {
    distributeSelectionVertically();
    return true;
  }

  return false;
}

bool TGraphCanvas::keyStateChanged(bool isKeyDown) {
  juce::ignoreUnused(isKeyDown);

  if (!juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)) {
    if (!panState.active)
      setMouseCursor(juce::MouseCursor::NormalCursor);
  } else {
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
  }

  return false;
}

void TGraphCanvas::setZoomLevel(float newZoom,
                                juce::Point<float> anchorPosView) {
  if (juce::exactlyEqual(newZoom, zoomLevel))
    return;

  const juce::Point<float> anchorWorld = viewToWorld(anchorPosView);

  zoomLevel = newZoom;

  viewOriginWorld.x = anchorWorld.x - (anchorPosView.x / zoomLevel);
  viewOriginWorld.y = anchorWorld.y - (anchorPosView.y / zoomLevel);

  updateChildPositions();
  repaint();
}

juce::Point<float> TGraphCanvas::viewToWorld(juce::Point<float> viewPos) const {
  return viewOriginWorld + (viewPos / zoomLevel);
}

juce::Point<float> TGraphCanvas::worldToView(juce::Point<float> worldPos) const {
  return (worldPos - viewOriginWorld) * zoomLevel;
}


void TGraphCanvas::showQuickAddPrompt(juce::Point<float> pointView) {
  quickAddAnchorView = pointView;
  if (searchOverlay == nullptr)
    return;

  searchOverlay->present(
      "Quick Add", "Type to filter nodes...",
      [this, pointView](const juce::String &query) {
        struct Candidate {
          const TNodeDescriptor *desc = nullptr;
          int score = std::numeric_limits<int>::min();
        };

        std::vector<Candidate> candidates;
        for (const auto *desc : getAllNodeDescriptors()) {
          if (desc == nullptr)
            continue;

          const int score = scoreDescriptorMatch(*desc, query);
          if (score == std::numeric_limits<int>::min())
            continue;

          candidates.push_back({desc, score});
        }

        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate &a, const Candidate &b) {
                           if (a.score != b.score)
                             return a.score > b.score;
                           if (a.desc->category != b.desc->category)
                             return a.desc->category < b.desc->category;
                           return a.desc->displayName < b.desc->displayName;
                         });

        if (candidates.size() > 40)
          candidates.resize(40);

        std::vector<SearchEntry> entries;
        entries.reserve(candidates.size());
        for (const auto &candidate : candidates) {
          SearchEntry entry;
          entry.title = candidate.desc->displayName;
          entry.subtitle = candidate.desc->category + " / " + candidate.desc->typeKey;
          if (std::find(recentNodeTypes.begin(), recentNodeTypes.end(),
                        candidate.desc->typeKey) != recentNodeTypes.end()) {
            entry.subtitle = "Recent / " + entry.subtitle;
          }

          const juce::String typeKey = candidate.desc->typeKey;
          entry.onSelect = [this, typeKey, pointView] {
            addNodeByTypeAtView(typeKey, pointView, true);
          };
          entries.push_back(std::move(entry));
        }

        return entries;
      },
      true, pointView);
}

void TGraphCanvas::showNodeSearchOverlay() {
  if (searchOverlay == nullptr)
    return;

  searchOverlay->present(
      "Jump To Node", "Search node names...",
      [this](const juce::String &query) {
        struct Candidate {
          NodeId nodeId = kInvalidNodeId;
          juce::String title;
          juce::String subtitle;
          int score = std::numeric_limits<int>::min();
        };

        std::vector<Candidate> candidates;
        for (const auto &node : document.nodes) {
          const auto *desc =
              nodeRegistry ? nodeRegistry->descriptorFor(node.typeKey) : nullptr;
          Candidate candidate;
          candidate.nodeId = node.nodeId;
          candidate.title = nodeLabelForUi(node, nodeRegistry);
          const juce::String category =
              desc != nullptr ? desc->category : categoryLabelForTypeKey(node.typeKey);
          candidate.subtitle = category + " / " + node.typeKey;
          candidate.score = scoreTextMatch(candidate.title, query);
          candidate.score = juce::jmax(candidate.score,
                                       scoreTextMatch(candidate.subtitle, query));
          if (candidate.score == std::numeric_limits<int>::min())
            continue;
          candidates.push_back(std::move(candidate));
        }

        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate &a, const Candidate &b) {
                           if (a.score != b.score)
                             return a.score > b.score;
                           return a.title < b.title;
                         });

        if (candidates.size() > 48)
          candidates.resize(48);

        std::vector<SearchEntry> entries;
        entries.reserve(candidates.size());
        for (const auto &candidate : candidates) {
          SearchEntry entry;
          entry.title = candidate.title;
          entry.subtitle = candidate.subtitle;
          const NodeId nodeId = candidate.nodeId;
          const juce::String nodeTitle = candidate.title;
          entry.onSelect = [this, nodeId, nodeTitle] {
            focusNode(nodeId);
            pushStatusHint("Focused " + nodeTitle + ".");
          };
          entries.push_back(std::move(entry));
        }

        return entries;
      },
      false, getViewCenter());
}

void TGraphCanvas::showCommandPaletteOverlay() {
  if (searchOverlay == nullptr)
    return;

  searchOverlay->present(
      "Command Palette", "Search commands...",
      [this](const juce::String &query) {
        struct Candidate {
          SearchEntry entry;
          int score = std::numeric_limits<int>::min();
        };

        std::vector<Candidate> candidates;
        auto addCommand = [&](const juce::String &title,
                              const juce::String &subtitle,
                              std::function<void()> action) {
          int score = scoreTextMatch(title, query);
          score = juce::jmax(score, scoreTextMatch(subtitle, query));
          score = juce::jmax(score, scoreTextMatch(title + " " + subtitle, query));
          if (score == std::numeric_limits<int>::min())
            return;

          Candidate candidate;
          candidate.entry.title = title;
          candidate.entry.subtitle = subtitle;
          candidate.entry.onSelect = std::move(action);
          candidate.score = score;
          candidates.push_back(std::move(candidate));
        };

        addCommand("Quick Add", "Insert a node at the current view center",
                   [this] { showQuickAddPrompt(getViewCenter()); });
        addCommand("Jump To Node", "Search nodes and focus the camera",
                   [this] { showNodeSearchOverlay(); });
        addCommand("Add Bookmark", "Store the current viewport as a bookmark",
                   [this] {
                     TBookmark bookmark;
                     bookmark.bookmarkId = document.allocBookmarkId();
                     bookmark.name = "Bookmark " + juce::String(bookmark.bookmarkId);
                     bookmark.focusX = viewOriginWorld.x;
                     bookmark.focusY = viewOriginWorld.y;
                     bookmark.zoom = zoomLevel;
                     document.bookmarks.push_back(bookmark);
                     document.touch(false);
                     pushStatusHint("Added bookmark.");
                   });
        addCommand("Duplicate Selection", "Ctrl+D",
                   [this] { duplicateSelection(); });
        addCommand("Delete Selection", "Delete or Backspace",
                   [this] { deleteSelectionWithPrompt(); });
        addCommand("Disconnect Selection", "Disconnect all selected wires",
                   [this] { disconnectSelectionWithPrompt(); });
        addCommand("Toggle Bypass", "Toggle bypass on selected nodes",
                   [this] { toggleBypassSelection(); });
        addCommand("Align Left", "Align selected nodes to the left edge",
                   [this] { alignSelectionLeft(); });
        addCommand("Align Top", "Align selected nodes to the top edge",
                   [this] { alignSelectionTop(); });
        addCommand("Distribute Horizontally",
                   "Evenly distribute selected nodes horizontally",
                   [this] { distributeSelectionHorizontally(); });
        addCommand("Distribute Vertically",
                   "Evenly distribute selected nodes vertically",
                   [this] { distributeSelectionVertically(); });

        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const Candidate &a, const Candidate &b) {
                           if (a.score != b.score)
                             return a.score > b.score;
                           return a.entry.title < b.entry.title;
                         });

        std::vector<SearchEntry> entries;
        entries.reserve(candidates.size());
        for (auto &candidate : candidates)
          entries.push_back(std::move(candidate.entry));
        return entries;
      },
      false, getViewCenter());
}

void TGraphCanvas::showCanvasContextMenu(juce::Point<float> pointView,
                                         juce::Point<float> pointScreen) {
  juce::PopupMenu menu;
  menu.addItem(1, "Quick Add...");

  juce::PopupMenu addMenu;
  std::vector<juce::String> addKeys;
  int addId = 1000;

  for (const auto *desc : getAllNodeDescriptors()) {
    if (desc == nullptr)
      continue;

    addMenu.addItem(addId, desc->displayName + " (" + desc->category + ")");
    addKeys.push_back(desc->typeKey);
    ++addId;

    if (addId > 1040)
      break;
  }

  menu.addSubMenu("Add Node", addMenu);
  menu.addSeparator();
  menu.addItem(20, "Create Frame Here");
  menu.addItem(21, "Add Bookmark Here");

  if (!document.bookmarks.empty()) {
    juce::PopupMenu bookmarks;
    int id = 2000;
    for (const auto &bookmark : document.bookmarks) {
      bookmarks.addItem(id, bookmark.name);
      ++id;
    }
    menu.addSubMenu("Jump To Bookmark", bookmarks);
  }

  auto safeThis = juce::Component::SafePointer<TGraphCanvas>(this);
  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetScreenArea(
          juce::Rectangle<int>((int)pointScreen.x, (int)pointScreen.y, 1, 1)),
      [safeThis, pointView, addKeys](int result) {
        if (safeThis == nullptr || result == 0)
          return;

        auto &self = *safeThis;

        if (result == 1) {
          self.showQuickAddPrompt(pointView);
          return;
        }

        if (result >= 1000 && result < 1000 + (int)addKeys.size()) {
          self.addNodeByTypeAtView(addKeys[(size_t)(result - 1000)], pointView, true);
          return;
        }

        if (result == 20) {
          TFrameRegion frame;
          frame.frameId = self.document.allocFrameId();
          frame.title = "Frame " + juce::String(frame.frameId);
          auto world = self.viewToWorld(pointView);
          frame.x = world.x - 160.0f;
          frame.y = world.y - 110.0f;
          frame.width = 320.0f;
          frame.height = 220.0f;
          frame.colorArgb = 0x334d8bf7;
          self.document.frames.push_back(frame);
          self.document.touch(false);
          self.repaint();
          return;
        }

        if (result == 21) {
          TBookmark bookmark;
          bookmark.bookmarkId = self.document.allocBookmarkId();
          bookmark.name = "Bookmark " + juce::String(bookmark.bookmarkId);
          auto world = self.viewToWorld(pointView);
          bookmark.focusX = world.x - (self.getWidth() * 0.5f) / self.zoomLevel;
          bookmark.focusY = world.y - (self.getHeight() * 0.5f) / self.zoomLevel;
          bookmark.zoom = self.zoomLevel;
          self.document.bookmarks.push_back(bookmark);
          self.document.touch(false);
          return;
        }

        if (result >= 2000 && result < 2000 + (int)self.document.bookmarks.size()) {
          const auto &bookmark = self.document.bookmarks[(size_t)(result - 2000)];
          self.viewOriginWorld = {bookmark.focusX, bookmark.focusY};
          self.zoomLevel = juce::jlimit(0.1f, 5.0f, bookmark.zoom);
          self.updateChildPositions();
          self.repaint();
          return;
        }
      });
}

void TGraphCanvas::timerCallback() {
  flowPhase += 0.04f;
  if (flowPhase >= 1.0f)
    flowPhase -= std::floor(flowPhase);

  if (disconnectAnimation.active) {
    const float dt = 1.0f / 30.0f;
    disconnectAnimation.velocityViewPerSecond.y += 1500.0f * dt;
    disconnectAnimation.loosePosView +=
        disconnectAnimation.velocityViewPerSecond * dt;

    disconnectAnimation.alpha = juce::jmax(0.0f, disconnectAnimation.alpha - 2.4f * dt);
    if (disconnectAnimation.alpha <= 0.01f)
      disconnectAnimation.active = false;
  }

  if (!document.connections.empty() || wireDragState.active ||
      disconnectAnimation.active) {
    repaint();
  }
}

void TGraphCanvas::openQuickAddAt(juce::Point<float> pointView) {
  showQuickAddPrompt(pointView);
}

void TGraphCanvas::openNodeSearchPrompt() { showNodeSearchOverlay(); }

void TGraphCanvas::openCommandPalette() { showCommandPaletteOverlay(); }

bool TGraphCanvas::isNodeSelected(NodeId nodeId) const {
  return std::find(selectedNodeIds.begin(), selectedNodeIds.end(), nodeId) !=
         selectedNodeIds.end();
}

void TGraphCanvas::clearNodeSelection() {
  if (selectedNodeIds.empty())
    return;

  selectedNodeIds.clear();
  syncNodeSelectionToComponents();
}

void TGraphCanvas::setNodeSelection(NodeId nodeId, bool selected) {
  auto it = std::find(selectedNodeIds.begin(), selectedNodeIds.end(), nodeId);

  if (selected) {
    if (it == selectedNodeIds.end())
      selectedNodeIds.push_back(nodeId);
  } else {
    if (it != selectedNodeIds.end())
      selectedNodeIds.erase(it);
  }

  syncNodeSelectionToComponents();
}

void TGraphCanvas::selectOnlyNode(NodeId nodeId) {
  selectedNodeIds.clear();
  selectedNodeIds.push_back(nodeId);
  syncNodeSelectionToComponents();
}

void TGraphCanvas::syncNodeSelectionToComponents() {
  for (auto &comp : nodeComponents)
    comp->isSelected = isNodeSelected(comp->getNodeId());

  if (nodeSelectionChangedHandler != nullptr)
    nodeSelectionChangedHandler(selectedNodeIds);

  repaint();
}

void TGraphCanvas::rememberRecentNode(const juce::String &typeKey) {
  if (typeKey.isEmpty())
    return;

  recentNodeTypes.erase(
      std::remove(recentNodeTypes.begin(), recentNodeTypes.end(), typeKey),
      recentNodeTypes.end());
  recentNodeTypes.insert(recentNodeTypes.begin(), typeKey);

  if (recentNodeTypes.size() > 16)
    recentNodeTypes.resize(16);
}

void TGraphCanvas::pushStatusHint(const juce::String &text) {
  statusHintText = text;
  statusHintAlpha = 1.0f;
  repaint();
}
bool TGraphCanvas::isNodeInsideFrame(const TNode &node,
                                     const TFrameRegion &frame) const {
  const juce::Rectangle<float> frameRect(frame.x, frame.y, frame.width,
                                         frame.height);
  const juce::Rectangle<float> nodeRect(node.x, node.y, 160.0f, 90.0f);
  return frameRect.intersects(nodeRect);
}

bool TGraphCanvas::isNodeHiddenByCollapsedFrame(const TNode &node) const {
  for (const auto &frame : document.frames) {
    if (frame.collapsed && isNodeInsideFrame(node, frame))
      return true;
  }
  return false;
}

bool TGraphCanvas::isNodeMoveLocked(NodeId nodeId) const {
  const TNode *node = document.findNode(nodeId);
  if (node == nullptr)
    return false;

  for (const auto &frame : document.frames) {
    if (frame.locked && isNodeInsideFrame(*node, frame))
      return true;
  }

  return false;
}

void TGraphCanvas::drawFrames(juce::Graphics &g) {
  const float titleHWorld = 28.0f / zoomLevel;

  for (const auto &frame : document.frames) {
    const float drawH = frame.collapsed ? titleHWorld : frame.height;

    const auto topLeft = worldToView({frame.x, frame.y});
    const auto bottomRight = worldToView({frame.x + frame.width,
                                          frame.y + drawH});

    juce::Rectangle<float> rect(
        juce::jmin(topLeft.x, bottomRight.x),
        juce::jmin(topLeft.y, bottomRight.y),
        std::abs(bottomRight.x - topLeft.x),
        std::abs(bottomRight.y - topLeft.y));

    juce::Colour base = juce::Colour(frame.colorArgb);
    if (base.isTransparent())
      base = juce::Colour(0x334d8bf7);

    g.setColour(base.withMultipliedAlpha(frame.locked ? 0.45f : 0.62f));
    g.fillRoundedRectangle(rect, 8.0f);

    g.setColour(base.withMultipliedAlpha(0.95f));
    g.drawRoundedRectangle(rect, 8.0f, 1.0f);

    auto titleRect = rect.removeFromTop(26.0f);
    g.setColour(base.brighter(0.2f).withAlpha(0.55f));
    g.fillRoundedRectangle(titleRect, 7.0f);

    juce::String title = frame.title;
    if (frame.locked)
      title += " [L]";
    if (frame.collapsed)
      title += " [C]";

    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(12.0f);
    g.drawText(title, titleRect.toNearestInt().reduced(8, 0),
               juce::Justification::centredLeft, false);
  }
}

void TGraphCanvas::drawLibraryDropPreview(juce::Graphics &g) {
  if (!libraryDropPreview.active)
    return;

  const auto *desc =
      nodeRegistry ? nodeRegistry->descriptorFor(libraryDropPreview.typeKey) : nullptr;
  const juce::String title =
      desc != nullptr ? desc->displayName : libraryDropPreview.typeKey;
  const juce::String subtitle =
      desc != nullptr ? desc->category + " / " + desc->typeKey
                      : libraryDropPreview.typeKey;

  auto bubble = juce::Rectangle<float>(libraryDropPreview.pointView.x + 14.0f,
                                       libraryDropPreview.pointView.y - 10.0f,
                                       240.0f, 44.0f);
  const auto bounds = getLocalBounds().toFloat().reduced(8.0f);
  bubble.setPosition(
      juce::jlimit(bounds.getX(), bounds.getRight() - bubble.getWidth(),
                   bubble.getX()),
      juce::jlimit(bounds.getY(), bounds.getBottom() - bubble.getHeight(),
                   bubble.getY()));

  g.setColour(juce::Colour(0xdd0f172a));
  g.fillRoundedRectangle(bubble, 10.0f);
  g.setColour(juce::Colour(0xff60a5fa));
  g.drawRoundedRectangle(bubble, 10.0f, 1.2f);

  g.setColour(juce::Colour(0xff93c5fd));
  g.drawEllipse(libraryDropPreview.pointView.x - 6.0f,
                libraryDropPreview.pointView.y - 6.0f, 12.0f, 12.0f, 1.5f);
  g.fillEllipse(libraryDropPreview.pointView.x - 2.5f,
                libraryDropPreview.pointView.y - 2.5f, 5.0f, 5.0f);

  auto textArea = bubble.toNearestInt().reduced(12, 7);
  g.setColour(juce::Colours::white.withAlpha(0.95f));
  g.setFont(13.0f);
  g.drawText(title, textArea.removeFromTop(16), juce::Justification::centredLeft,
             false);
  g.setColour(juce::Colours::white.withAlpha(0.55f));
  g.setFont(11.0f);
  g.drawText(subtitle, textArea, juce::Justification::centredLeft, false);
}

void TGraphCanvas::drawSelectionOverlay(juce::Graphics &g) {
  if (marqueeState.active) {
    g.setColour(juce::Colour(0x5560a5fa));
    g.fillRect(marqueeState.rectView);
    g.setColour(juce::Colour(0xff60a5fa));
    g.drawRect(marqueeState.rectView, 1.2f);
  }

  if (snapGuideState.xActive) {
    const float x = worldToView({snapGuideState.worldX, 0.0f}).x;
    g.setColour(juce::Colour(0x88fbbf24));
    g.drawLine(x, 0.0f, x, (float)getHeight(), 1.0f);
  }

  if (snapGuideState.yActive) {
    const float y = worldToView({0.0f, snapGuideState.worldY}).y;
    g.setColour(juce::Colour(0x88fbbf24));
    g.drawLine(0.0f, y, (float)getWidth(), y, 1.0f);
  }
}

void TGraphCanvas::drawStatusHint(juce::Graphics &g) {
  if (statusHintAlpha <= 0.01f || statusHintText.isEmpty())
    return;

  auto area = getLocalBounds().removeFromTop(28).reduced(10, 4);
  area.setWidth(juce::jmin(460, area.getWidth()));

  g.setColour(juce::Colour(0xcc111111).withAlpha(statusHintAlpha));
  g.fillRoundedRectangle(area.toFloat(), 5.0f);

  g.setColour(juce::Colours::white.withAlpha(statusHintAlpha));
  g.setFont(13.0f);
  g.drawText(statusHintText, area.reduced(8, 0), juce::Justification::centredLeft,
             false);
}

void TGraphCanvas::recalcMiniMapCache() {
  float minX = std::numeric_limits<float>::max();
  float minY = std::numeric_limits<float>::max();
  float maxX = -std::numeric_limits<float>::max();
  float maxY = -std::numeric_limits<float>::max();

  for (const auto &node : document.nodes) {
    minX = juce::jmin(minX, node.x);
    minY = juce::jmin(minY, node.y);
    maxX = juce::jmax(maxX, node.x + 160.0f);
    maxY = juce::jmax(maxY, node.y + 90.0f);
  }

  for (const auto &frame : document.frames) {
    minX = juce::jmin(minX, frame.x);
    minY = juce::jmin(minY, frame.y);
    maxX = juce::jmax(maxX, frame.x + frame.width);
    maxY = juce::jmax(maxY, frame.y + frame.height);
  }

  if (minX == std::numeric_limits<float>::max()) {
    minX = viewOriginWorld.x - 400.0f;
    minY = viewOriginWorld.y - 300.0f;
    maxX = viewOriginWorld.x + 400.0f;
    maxY = viewOriginWorld.y + 300.0f;
  }

  miniMapWorldBounds = juce::Rectangle<float>(
      minX, minY, juce::jmax(1.0f, maxX - minX), juce::jmax(1.0f, maxY - minY));
  miniMapCacheValid = true;
}

juce::Point<float> TGraphCanvas::miniMapToWorld(
    juce::Point<float> miniPoint) const {
  if (!miniMapCacheValid || miniMapRectView.getWidth() <= 0.0f ||
      miniMapRectView.getHeight() <= 0.0f) {
    return viewOriginWorld;
  }

  const float nx = (miniPoint.x - miniMapRectView.getX()) /
                   miniMapRectView.getWidth();
  const float ny = (miniPoint.y - miniMapRectView.getY()) /
                   miniMapRectView.getHeight();

  return {miniMapWorldBounds.getX() + nx * miniMapWorldBounds.getWidth(),
          miniMapWorldBounds.getY() + ny * miniMapWorldBounds.getHeight()};
}
int TGraphCanvas::hitTestFrame(juce::Point<float> pointView) const {
  const juce::Point<float> world = viewToWorld(pointView);

  for (auto it = document.frames.rbegin(); it != document.frames.rend(); ++it) {
    const float titleH = 28.0f / zoomLevel;
    const float drawH = it->collapsed ? titleH : it->height;

    juce::Rectangle<float> rect(it->x, it->y, it->width, drawH);
    if (rect.contains(world))
      return it->frameId;
  }

  return 0;
}

bool TGraphCanvas::isPointInFrameTitle(int frameId,
                                       juce::Point<float> pointView) const {
  for (const auto &frame : document.frames) {
    if (frame.frameId != frameId)
      continue;

    const juce::Point<float> world = viewToWorld(pointView);
    const float titleH = 28.0f / zoomLevel;
    juce::Rectangle<float> title(frame.x, frame.y, frame.width, titleH);
    return title.contains(world);
  }

  return false;
}

void TGraphCanvas::startFrameDrag(int frameId, juce::Point<float> mouseView) {
  frameDragState = FrameDragState{};

  for (const auto &frame : document.frames) {
    if (frame.frameId != frameId)
      continue;

    if (frame.locked)
      return;

    frameDragState.active = true;
    frameDragState.frameId = frameId;
    frameDragState.startMouseView = mouseView;
    frameDragState.startFramePosWorld = {frame.x, frame.y};

    for (const auto &node : document.nodes) {
      if (isNodeInsideFrame(node, frame))
        frameDragState.containedNodeStartWorld[node.nodeId] = {node.x, node.y};
    }

    return;
  }
}

void TGraphCanvas::updateFrameDrag(juce::Point<float> mouseView) {
  if (!frameDragState.active)
    return;

  auto frameIt = std::find_if(document.frames.begin(), document.frames.end(),
                              [&](const TFrameRegion &f) {
                                return f.frameId == frameDragState.frameId;
                              });
  if (frameIt == document.frames.end())
    return;

  const juce::Point<float> deltaWorld =
      (mouseView - frameDragState.startMouseView) / zoomLevel;

  frameIt->x = frameDragState.startFramePosWorld.x + deltaWorld.x;
  frameIt->y = frameDragState.startFramePosWorld.y + deltaWorld.y;

  for (const auto &pair : frameDragState.containedNodeStartWorld) {
    if (TNode *node = document.findNode(pair.first)) {
      node->x = pair.second.x + deltaWorld.x;
      node->y = pair.second.y + deltaWorld.y;
    }
  }

  updateChildPositions();
  repaint();
}

void TGraphCanvas::endFrameDrag() {
  bool didMove = false;

  if (frameDragState.active) {
    if (const auto *frame = document.findFrame(frameDragState.frameId)) {
      didMove = frame->x != frameDragState.startFramePosWorld.x ||
                frame->y != frameDragState.startFramePosWorld.y;
    }

    if (!didMove) {
      for (const auto &pair : frameDragState.containedNodeStartWorld) {
        if (const auto *node = document.findNode(pair.first)) {
          if (node->x != pair.second.x || node->y != pair.second.y) {
            didMove = true;
            break;
          }
        }
      }
    }
  }

  frameDragState = FrameDragState{};
  if (didMove)
    document.touch(false);
}

void TGraphCanvas::startNodeDrag(NodeId nodeId, juce::Point<float> mouseView) {
  nodeDragState = NodeDragState{};
  nodeDragState.active = true;
  nodeDragState.anchorNodeId = nodeId;
  nodeDragState.startMouseView = mouseView;

  for (const NodeId id : selectedNodeIds) {
    if (const TNode *node = document.findNode(id))
      nodeDragState.startWorldByNode[id] = {node->x, node->y};
  }
}

void TGraphCanvas::updateNodeDrag(juce::Point<float> mouseView) {
  if (!nodeDragState.active)
    return;

  juce::Point<float> deltaWorld =
      (mouseView - nodeDragState.startMouseView) / zoomLevel;

  snapGuideState = SnapGuideState{};

  if (selectedNodeIds.size() == 1) {
    const auto anchorIt =
        nodeDragState.startWorldByNode.find(nodeDragState.anchorNodeId);

    if (anchorIt != nodeDragState.startWorldByNode.end()) {
      const juce::Point<float> candidate = anchorIt->second + deltaWorld;
      const float threshold = 10.0f / zoomLevel;

      float bestDx = std::numeric_limits<float>::max();
      float bestDy = std::numeric_limits<float>::max();
      float snapX = candidate.x;
      float snapY = candidate.y;

      for (const auto &node : document.nodes) {
        if (node.nodeId == nodeDragState.anchorNodeId)
          continue;

        const float dx = std::abs(candidate.x - node.x);
        if (dx < bestDx && dx < threshold) {
          bestDx = dx;
          snapX = node.x;
        }

        const float dy = std::abs(candidate.y - node.y);
        if (dy < bestDy && dy < threshold) {
          bestDy = dy;
          snapY = node.y;
        }
      }

      if (bestDx != std::numeric_limits<float>::max()) {
        deltaWorld.x += (snapX - candidate.x);
        snapGuideState.xActive = true;
        snapGuideState.worldX = snapX;
      }

      if (bestDy != std::numeric_limits<float>::max()) {
        deltaWorld.y += (snapY - candidate.y);
        snapGuideState.yActive = true;
        snapGuideState.worldY = snapY;
      }
    }
  }

  for (const auto &pair : nodeDragState.startWorldByNode) {
    TNode *node = document.findNode(pair.first);
    if (node == nullptr)
      continue;

    if (isNodeMoveLocked(node->nodeId))
      continue;

    node->x = pair.second.x + deltaWorld.x;
    node->y = pair.second.y + deltaWorld.y;
  }

  updateChildPositions();
  repaint();
}

void TGraphCanvas::endNodeDrag() {
  bool didMove = false;
  if (nodeDragState.active) {
    for (const auto &pair : nodeDragState.startWorldByNode) {
      if (const auto *node = document.findNode(pair.first)) {
        if (node->x != pair.second.x || node->y != pair.second.y) {
          didMove = true;
          break;
        }
      }
    }
  }

  nodeDragState = NodeDragState{};
  snapGuideState = SnapGuideState{};
  if (didMove)
    document.touch(false);
  repaint();
}

void TGraphCanvas::updateMarqueeSelection() {
  std::vector<NodeId> next = marqueeState.baseSelection;

  for (const auto &comp : nodeComponents) {
    if (!comp->isVisible())
      continue;

    if (!marqueeState.rectView.intersects(comp->getBounds().toFloat()))
      continue;

    const NodeId id = comp->getNodeId();
    if (std::find(next.begin(), next.end(), id) == next.end())
      next.push_back(id);
  }

  selectedNodeIds = std::move(next);
  syncNodeSelectionToComponents();
}
void TGraphCanvas::requestNodeMouseDown(NodeId nodeId,
                                        const juce::MouseEvent &event) {
  if (!event.mods.isLeftButtonDown())
    return;

  const bool toggle = event.mods.isCtrlDown() || event.mods.isCommandDown();
  const bool additive = event.mods.isShiftDown();

  if (toggle) {
    setNodeSelection(nodeId, !isNodeSelected(nodeId));
  } else if (additive) {
    setNodeSelection(nodeId, true);
  } else {
    if (!isNodeSelected(nodeId) || selectedNodeIds.size() != 1)
      selectOnlyNode(nodeId);
  }

  if (!isNodeSelected(nodeId))
    return;

  if (isNodeMoveLocked(nodeId)) {
    pushStatusHint("Node is inside a locked frame.");
    return;
  }

  startNodeDrag(nodeId, event.getEventRelativeTo(this).position);
}

void TGraphCanvas::requestNodeMouseDrag(NodeId nodeId,
                                        const juce::MouseEvent &event) {
  juce::ignoreUnused(nodeId);

  if (!nodeDragState.active)
    return;

  updateNodeDrag(event.getEventRelativeTo(this).position);
}

void TGraphCanvas::requestNodeMouseUp(NodeId nodeId,
                                      const juce::MouseEvent &event) {
  juce::ignoreUnused(nodeId, event);

  if (nodeDragState.active)
    endNodeDrag();
}

void TGraphCanvas::requestNodeContextMenu(NodeId nodeId,
                                          juce::Point<float> pointView,
                                          juce::Point<float> pointScreen) {
  if (!isNodeSelected(nodeId))
    selectOnlyNode(nodeId);

  showNodeContextMenu(nodeId, pointView, pointScreen);
}

void TGraphCanvas::duplicateSelection() {
  if (selectedNodeIds.empty())
    return;

  const std::set<NodeId> selectedSet(selectedNodeIds.begin(), selectedNodeIds.end());

  std::unordered_map<NodeId, NodeId> newNodeByOld;
  std::unordered_map<PortId, PortId> newPortByOld;
  std::vector<NodeId> newSelection;

  for (const auto &node : document.nodes) {
    if (selectedSet.find(node.nodeId) == selectedSet.end())
      continue;

    TNode copy = node;
    const NodeId oldNodeId = copy.nodeId;
    copy.nodeId = document.allocNodeId();
    copy.x += 36.0f;
    copy.y += 24.0f;

    for (auto &port : copy.ports) {
      const PortId oldPortId = port.portId;
      port.portId = document.allocPortId();
      port.ownerNodeId = copy.nodeId;
      newPortByOld[oldPortId] = port.portId;
    }

    newNodeByOld[oldNodeId] = copy.nodeId;
    newSelection.push_back(copy.nodeId);
    document.executeCommand(std::make_unique<AddNodeCommand>(copy));
  }

  const std::vector<TConnection> existingConnections = document.connections;
  for (const auto &conn : existingConnections) {
    if (selectedSet.find(conn.from.nodeId) == selectedSet.end() ||
        selectedSet.find(conn.to.nodeId) == selectedSet.end()) {
      continue;
    }

    auto fromNodeIt = newNodeByOld.find(conn.from.nodeId);
    auto toNodeIt = newNodeByOld.find(conn.to.nodeId);
    auto fromPortIt = newPortByOld.find(conn.from.portId);
    auto toPortIt = newPortByOld.find(conn.to.portId);

    if (fromNodeIt == newNodeByOld.end() || toNodeIt == newNodeByOld.end() ||
        fromPortIt == newPortByOld.end() || toPortIt == newPortByOld.end()) {
      continue;
    }

    TConnection copyConn;
    copyConn.connectionId = document.allocConnectionId();
    copyConn.from = {fromNodeIt->second, fromPortIt->second};
    copyConn.to = {toNodeIt->second, toPortIt->second};
    document.executeCommand(std::make_unique<AddConnectionCommand>(copyConn));
  }

  selectedNodeIds = std::move(newSelection);
  rebuildNodeComponents();
  pushStatusHint("Duplicated selection. Undo: Ctrl+Z");
}

void TGraphCanvas::deleteSelectionWithPrompt() {
  if (selectedNodeIds.empty())
    return;

  const bool confirmed = juce::AlertWindow::showOkCancelBox(
      juce::MessageBoxIconType::WarningIcon, "Delete Nodes",
      "Delete selected nodes? You can undo with Ctrl+Z.", "OK", "Cancel", nullptr, nullptr);
  if (!confirmed)
    return;

  const std::vector<NodeId> targets = selectedNodeIds;
  clearNodeSelection();

  for (const NodeId id : targets)
    document.executeCommand(std::make_unique<DeleteNodeCommand>(id));

  selectedConnectionId = kInvalidConnectionId;
  rebuildNodeComponents();
  pushStatusHint("Selection deleted. Undo: Ctrl+Z");
}

void TGraphCanvas::disconnectSelectionWithPrompt() {
  if (selectedNodeIds.empty())
    return;

  std::vector<ConnectionId> toDelete;
  for (const auto &conn : document.connections) {
    if (isNodeSelected(conn.from.nodeId) || isNodeSelected(conn.to.nodeId))
      toDelete.push_back(conn.connectionId);
  }

  if (toDelete.empty())
    return;

  const bool confirmed = juce::AlertWindow::showOkCancelBox(
      juce::MessageBoxIconType::WarningIcon, "Disconnect Selected",
      "Disconnect all wires touching selected nodes? Ctrl+Z will undo.", "OK", "Cancel", nullptr, nullptr);
  if (!confirmed)
    return;

  for (const ConnectionId id : toDelete)
    document.executeCommand(std::make_unique<DeleteConnectionCommand>(id));

  selectedConnectionId = kInvalidConnectionId;
  pushStatusHint("Disconnected selection. Undo: Ctrl+Z");
  repaint();
}

void TGraphCanvas::toggleBypassSelection() {
  if (selectedNodeIds.empty())
    return;

  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id))
      node->bypassed = !node->bypassed;
  }

  document.touch(true);
  repaint();
}

void TGraphCanvas::alignSelectionLeft() {
  if (selectedNodeIds.size() < 2)
    return;

  float minX = std::numeric_limits<float>::max();
  for (const NodeId id : selectedNodeIds) {
    if (const TNode *node = document.findNode(id))
      minX = juce::jmin(minX, node->x);
  }

  bool didMove = false;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id)) {
      didMove = didMove || node->x != minX;
      node->x = minX;
    }
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}

void TGraphCanvas::alignSelectionTop() {
  if (selectedNodeIds.size() < 2)
    return;

  float minY = std::numeric_limits<float>::max();
  for (const NodeId id : selectedNodeIds) {
    if (const TNode *node = document.findNode(id))
      minY = juce::jmin(minY, node->y);
  }

  bool didMove = false;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id)) {
      didMove = didMove || node->y != minY;
      node->y = minY;
    }
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}

void TGraphCanvas::distributeSelectionHorizontally() {
  if (selectedNodeIds.size() < 3)
    return;

  std::vector<TNode *> nodes;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id))
      nodes.push_back(node);
  }

  if (nodes.size() < 3)
    return;

  std::sort(nodes.begin(), nodes.end(),
            [](const TNode *a, const TNode *b) { return a->x < b->x; });

  const float start = nodes.front()->x;
  const float end = nodes.back()->x;
  const float step = (end - start) / (float)(nodes.size() - 1);

  bool didMove = false;
  for (size_t i = 1; i + 1 < nodes.size(); ++i) {
    const float nextX = start + step * (float)i;
    didMove = didMove || nodes[i]->x != nextX;
    nodes[i]->x = nextX;
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}

void TGraphCanvas::distributeSelectionVertically() {
  if (selectedNodeIds.size() < 3)
    return;

  std::vector<TNode *> nodes;
  for (const NodeId id : selectedNodeIds) {
    if (TNode *node = document.findNode(id))
      nodes.push_back(node);
  }

  if (nodes.size() < 3)
    return;

  std::sort(nodes.begin(), nodes.end(),
            [](const TNode *a, const TNode *b) { return a->y < b->y; });

  const float start = nodes.front()->y;
  const float end = nodes.back()->y;
  const float step = (end - start) / (float)(nodes.size() - 1);

  bool didMove = false;
  for (size_t i = 1; i + 1 < nodes.size(); ++i) {
    const float nextY = start + step * (float)i;
    didMove = didMove || nodes[i]->y != nextY;
    nodes[i]->y = nextY;
  }

  if (didMove)
    document.touch(false);
  updateChildPositions();
}
bool TGraphCanvas::isReplacementCompatible(const TNode &oldNode,
                                           const TNodeDescriptor &replacement) const {
  auto countNodePorts = [](const std::vector<TPort> &ports, TPortDirection dir,
                           TPortDataType type) {
    int count = 0;
    for (const auto &p : ports) {
      if (p.direction == dir && p.dataType == type)
        ++count;
    }
    return count;
  };

  auto countSpecPorts = [](const std::vector<TPortSpec> &ports,
                           TPortDirection dir, TPortDataType type) {
    int count = 0;
    for (const auto &p : ports) {
      if (p.direction == dir && p.dataType == type)
        ++count;
    }
    return count;
  };

  for (int t = (int)TPortDataType::Audio; t <= (int)TPortDataType::Control; ++t) {
    const auto type = (TPortDataType)t;

    if (countSpecPorts(replacement.portSpecs, TPortDirection::Input, type) <
        countNodePorts(oldNode.ports, TPortDirection::Input, type))
      return false;

    if (countSpecPorts(replacement.portSpecs, TPortDirection::Output, type) <
        countNodePorts(oldNode.ports, TPortDirection::Output, type))
      return false;
  }

  return true;
}

bool TGraphCanvas::replaceNode(NodeId nodeId,
                               const juce::String &replacementTypeKey) {
  const TNode *oldNode = document.findNode(nodeId);
  const TNodeDescriptor *desc =
      nodeRegistry ? nodeRegistry->descriptorFor(replacementTypeKey) : nullptr;

  if (oldNode == nullptr || desc == nullptr)
    return false;

  if (!isReplacementCompatible(*oldNode, *desc))
    return false;

  const TNode oldSnapshot = *oldNode;

  std::vector<TConnection> related;
  for (const auto &conn : document.connections) {
    if (conn.from.nodeId == nodeId || conn.to.nodeId == nodeId)
      related.push_back(conn);
  }

  TNode next;
  next.nodeId = document.allocNodeId();
  next.typeKey = desc->typeKey;
  next.x = oldSnapshot.x;
  next.y = oldSnapshot.y;
  next.label = oldSnapshot.label;
  next.colorTag = oldSnapshot.colorTag;
  next.bypassed = oldSnapshot.bypassed;
  next.collapsed = oldSnapshot.collapsed;

  for (const auto &param : desc->paramSpecs)
    next.params[param.key] = param.defaultValue;

  for (const auto &spec : desc->portSpecs) {
    TPort port;
    port.portId = document.allocPortId();
    port.ownerNodeId = next.nodeId;
    port.direction = spec.direction;
    port.dataType = spec.dataType;
    port.name = spec.name;
    next.ports.push_back(port);
  }

  std::unordered_map<PortId, PortId> mapped;
  std::set<PortId> usedInputs;
  std::set<PortId> usedOutputs;

  for (const auto &oldPort : oldSnapshot.ports) {
    for (const auto &newPort : next.ports) {
      if (newPort.direction != oldPort.direction ||
          newPort.dataType != oldPort.dataType)
        continue;

      auto &used = oldPort.direction == TPortDirection::Input ? usedInputs
                                                              : usedOutputs;
      if (used.find(newPort.portId) != used.end())
        continue;

      used.insert(newPort.portId);
      mapped[oldPort.portId] = newPort.portId;
      break;
    }
  }

  std::vector<TConnection> rebuilt;
  for (auto conn : related) {
    if (conn.from.nodeId == nodeId) {
      auto it = mapped.find(conn.from.portId);
      if (it == mapped.end())
        continue;
      conn.from.nodeId = next.nodeId;
      conn.from.portId = it->second;
    }

    if (conn.to.nodeId == nodeId) {
      auto it = mapped.find(conn.to.portId);
      if (it == mapped.end())
        continue;
      conn.to.nodeId = next.nodeId;
      conn.to.portId = it->second;
    }

    conn.connectionId = document.allocConnectionId();
    rebuilt.push_back(conn);
  }

  document.executeCommand(std::make_unique<DeleteNodeCommand>(nodeId));
  document.executeCommand(std::make_unique<AddNodeCommand>(next));
  for (const auto &conn : rebuilt)
    document.executeCommand(std::make_unique<AddConnectionCommand>(conn));

  selectOnlyNode(next.nodeId);
  rebuildNodeComponents();
  pushStatusHint("Node replaced. Undo: Ctrl+Z");
  return true;
}
void TGraphCanvas::showNodeContextMenu(NodeId nodeId,
                                       juce::Point<float> pointView,
                                       juce::Point<float> pointScreen) {
  juce::ignoreUnused(pointView);

  juce::PopupMenu menu;
  menu.addItem(1, "Duplicate");
  menu.addItem(2, "Delete");
  menu.addItem(3, "Toggle Bypass");
  menu.addItem(4, "Disconnect All");
  menu.addItem(5, "Open Properties");

  juce::PopupMenu colorMenu;
  colorMenu.addItem(100, "None");
  colorMenu.addItem(101, "Red");
  colorMenu.addItem(102, "Orange");
  colorMenu.addItem(103, "Green");
  colorMenu.addItem(104, "Blue");
  menu.addSubMenu("Color Tag", colorMenu);

  std::vector<juce::String> replaceKeys;
  if (const TNode *node = document.findNode(nodeId)) {
    juce::PopupMenu replaceMenu;
    int rid = 300;

    for (const auto *desc : getAllNodeDescriptors()) {
      if (desc == nullptr || desc->typeKey == node->typeKey)
        continue;
      if (!isReplacementCompatible(*node, *desc))
        continue;

      replaceMenu.addItem(rid, desc->displayName + " (" + desc->category + ")");
      replaceKeys.push_back(desc->typeKey);
      ++rid;
      if (rid > 360)
        break;
    }

    if (!replaceKeys.empty())
      menu.addSubMenu("Replace Node", replaceMenu);
  }

  if (selectedNodeIds.size() > 1) {
    menu.addSeparator();
    menu.addItem(20, "Batch Duplicate Selected");
    menu.addItem(21, "Batch Delete Selected");
    menu.addItem(22, "Batch Toggle Bypass");
  }

  auto safeThis = juce::Component::SafePointer<TGraphCanvas>(this);
  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetScreenArea(
          juce::Rectangle<int>((int)pointScreen.x, (int)pointScreen.y, 1, 1)),
      [safeThis, nodeId, replaceKeys](int result) {
        if (safeThis == nullptr || result == 0)
          return;

        auto &self = *safeThis;

        if (result == 1) {
          self.duplicateSelection();
          return;
        }
        if (result == 2) {
          self.deleteSelectionWithPrompt();
          return;
        }
        if (result == 3) {
          self.toggleBypassSelection();
          return;
        }
        if (result == 4) {
          self.disconnectSelectionWithPrompt();
          return;
        }
        if (result == 5) {
          if (self.nodePropertiesRequestHandler != nullptr)
            self.nodePropertiesRequestHandler(nodeId);
          else if (const TNode *node = self.document.findNode(nodeId)) {
            juce::String msg = "Type: " + node->typeKey + "\n";
            msg += "Ports: " + juce::String((int)node->ports.size()) + "\n";
            msg += "Params: " + juce::String((int)node->params.size());
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon, "Node Properties", msg);
          }
          return;
        }

        if (result >= 100 && result <= 104) {
          juce::String tag;
          if (result == 101)
            tag = "red";
          else if (result == 102)
            tag = "orange";
          else if (result == 103)
            tag = "green";
          else if (result == 104)
            tag = "blue";

          for (const NodeId id : self.selectedNodeIds) {
            if (TNode *node = self.document.findNode(id))
              node->colorTag = tag;
          }

          self.document.touch(false);
          self.repaint();
          return;
        }

        if (result == 20) {
          self.duplicateSelection();
          return;
        }
        if (result == 21) {
          self.deleteSelectionWithPrompt();
          return;
        }
        if (result == 22) {
          self.toggleBypassSelection();
          return;
        }

        if (result >= 300 && result < 300 + (int)replaceKeys.size()) {
          self.replaceNode(nodeId, replaceKeys[(size_t)(result - 300)]);
          return;
        }
      });
}

void TGraphCanvas::showFrameContextMenu(int frameId,
                                        juce::Point<float> pointView,
                                        juce::Point<float> pointScreen) {
  juce::ignoreUnused(pointView);

  auto frameIt = std::find_if(document.frames.begin(), document.frames.end(),
                              [&](const TFrameRegion &f) {
                                return f.frameId == frameId;
                              });
  if (frameIt == document.frames.end())
    return;

  juce::PopupMenu menu;
  menu.addItem(1, frameIt->collapsed ? "Expand Frame" : "Collapse Frame");
  menu.addItem(2, frameIt->locked ? "Unlock Frame" : "Lock Frame");
  menu.addItem(3, "Delete Frame");

  juce::PopupMenu colorMenu;
  colorMenu.addItem(100, "Blue");
  colorMenu.addItem(101, "Green");
  colorMenu.addItem(102, "Amber");
  colorMenu.addItem(103, "Red");
  menu.addSubMenu("Frame Color", colorMenu);

  auto safeThis = juce::Component::SafePointer<TGraphCanvas>(this);
  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetScreenArea(
          juce::Rectangle<int>((int)pointScreen.x, (int)pointScreen.y, 1, 1)),
      [safeThis, frameId](int result) {
        if (safeThis == nullptr || result == 0)
          return;

        auto &self = *safeThis;
        auto it = std::find_if(self.document.frames.begin(),
                               self.document.frames.end(),
                               [&](const TFrameRegion &f) {
                                 return f.frameId == frameId;
                               });
        if (it == self.document.frames.end())
          return;

        if (result == 1) {
          it->collapsed = !it->collapsed;
          self.document.touch(false);
          self.updateChildPositions();
          self.repaint();
          return;
        }
        if (result == 2) {
          it->locked = !it->locked;
          self.document.touch(false);
          self.repaint();
          return;
        }
        if (result == 3) {
          self.document.frames.erase(it);
          self.document.touch(false);
          self.updateChildPositions();
          self.repaint();
          return;
        }
        if (result >= 100 && result <= 103) {
          if (result == 100)
            it->colorArgb = 0x334d8bf7;
          else if (result == 101)
            it->colorArgb = 0x3322c55e;
          else if (result == 102)
            it->colorArgb = 0x33f59e0b;
          else if (result == 103)
            it->colorArgb = 0x33ef4444;
          self.document.touch(false);
          self.repaint();
          return;
        }
      });
}
void TGraphCanvas::mouseDoubleClick(const juce::MouseEvent &event) {
  if (event.mods.isLeftButtonDown())
    showQuickAddPrompt(event.position);
}
} // namespace Teul