#include "Teul/Editor/Canvas/TGraphCanvas.h"

#include "Teul/Editor/Search/SearchController.h"
#include "Teul/History/TCommands.h"
#include "Teul/Registry/TNodeRegistry.h"

#include <algorithm>
#include <set>

namespace Teul {

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
          frame.frameUuid = juce::Uuid().toString();
          frame.title = "Frame " + juce::String(frame.frameId);
          frame.colorArgb = 0x334d8bf7;
          frame.logicalGroup = true;
          frame.membershipExplicit = true;

          if (!self.selectedNodeIds.empty()) {
            juce::Rectangle<float> memberBounds;
            bool hasBounds = false;

            for (const auto nodeId : self.selectedNodeIds) {
              const auto *node = self.document.findNode(nodeId);
              if (node == nullptr)
                continue;

              frame.addMember(nodeId);
              const juce::Rectangle<float> nodeRect(node->x, node->y, 160.0f,
                                                    90.0f);
              memberBounds = hasBounds ? memberBounds.getUnion(nodeRect)
                                       : nodeRect;
              hasBounds = true;
            }

            if (hasBounds) {
              memberBounds = memberBounds.expanded(24.0f, 24.0f);
              frame.x = memberBounds.getX();
              frame.y = memberBounds.getY();
              frame.width = memberBounds.getWidth();
              frame.height = memberBounds.getHeight();
            }
          }

          if (frame.width <= 0.0f || frame.height <= 0.0f) {
            auto world = self.viewToWorld(pointView);
            frame.x = world.x - 160.0f;
            frame.y = world.y - 110.0f;
            frame.width = 320.0f;
            frame.height = 220.0f;
          }

          self.document.frames.push_back(frame);
          self.document.touch(false);
          self.updateChildPositions();
          self.repaint();
          self.pushStatusHint(frame.memberNodeIds.empty()
                                  ? "Frame created. Use the frame menu to capture members."
                                  : "Frame group created from selection.");
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

void TGraphCanvas::requestNodeContextMenu(NodeId nodeId,
                                          juce::Point<float> pointView,
                                          juce::Point<float> pointScreen) {
  if (!isNodeSelected(nodeId))
    selectOnlyNode(nodeId);

  showNodeContextMenu(nodeId, pointView, pointScreen);
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
  const TNodeDescriptor *desc = findDescriptorByTypeKey(replacementTypeKey);

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

  const bool canCaptureSelection = !selectedNodeIds.empty();
  const bool canReleaseSelection = std::any_of(
      selectedNodeIds.begin(), selectedNodeIds.end(),
      [&](NodeId nodeId) { return frameIt->containsNode(nodeId); });
  const bool canFitToMembers =
      frameIt->membershipExplicit && !frameIt->memberNodeIds.empty();

  juce::PopupMenu menu;
  menu.addItem(1, frameIt->collapsed ? "Expand Frame" : "Collapse Frame");
  menu.addItem(2, frameIt->locked ? "Unlock Frame" : "Lock Frame");
  menu.addItem(3, "Delete Frame");
  menu.addSeparator();
  menu.addItem(4, "Capture Selected Nodes", canCaptureSelection);
  menu.addItem(5, "Release Selected Nodes", canReleaseSelection);
  menu.addItem(6, "Fit To Members", canFitToMembers);

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
        if (result == 4) {
          bool changed = false;
          for (const auto nodeId : self.selectedNodeIds) {
            if (it->containsNode(nodeId))
              continue;
            it->membershipExplicit = true;
            it->addMember(nodeId);
            changed = true;
          }
          if (changed) {
            const auto bounds = self.getFrameMemberBoundsWorld(*it);
            it->x = bounds.getX();
            it->y = bounds.getY();
            it->width = bounds.getWidth();
            it->height = bounds.getHeight();
            self.document.touch(false);
            self.updateChildPositions();
            self.repaint();
            self.pushStatusHint("Selected nodes captured into frame group.");
          }
          return;
        }
        if (result == 5) {
          bool changed = false;
          for (const auto nodeId : self.selectedNodeIds) {
            if (!it->containsNode(nodeId))
              continue;
            it->membershipExplicit = true;
            it->removeMember(nodeId);
            changed = true;
          }
          if (changed) {
            if (!it->memberNodeIds.empty()) {
              const auto bounds = self.getFrameMemberBoundsWorld(*it);
              it->x = bounds.getX();
              it->y = bounds.getY();
              it->width = bounds.getWidth();
              it->height = bounds.getHeight();
            }
            self.document.touch(false);
            self.updateChildPositions();
            self.repaint();
            self.pushStatusHint("Selected nodes released from frame group.");
          }
          return;
        }
        if (result == 6) {
          if (it->membershipExplicit && !it->memberNodeIds.empty()) {
            const auto bounds = self.getFrameMemberBoundsWorld(*it);
            it->x = bounds.getX();
            it->y = bounds.getY();
            it->width = bounds.getWidth();
            it->height = bounds.getHeight();
            self.document.touch(false);
            self.updateChildPositions();
            self.repaint();
            self.pushStatusHint("Frame resized to current group members.");
          }
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

} // namespace Teul
