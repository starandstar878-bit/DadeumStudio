#pragma once

#include "Teul/Public/EditorHandle.h"
#include "Teul/Model/TGraphDocument.h"
#include "Teul/Runtime/TGraphRuntime.h"

#include <memory>
#include <vector>

namespace Teul {

class TGraphCanvas;
class TNodeRegistry;
class NodeLibraryPanel;
class NodePropertiesPanel;

struct EditorHandle::Impl : private juce::Timer {
  explicit Impl(EditorHandle &owner,
                juce::AudioDeviceManager *audioDeviceManager,
                ParamBindingSummaryResolver bindingSummaryResolver,
                ParamBindingRevisionProvider bindingRevisionProvider);
  ~Impl() override;

  TGraphDocument &document() noexcept;
  const TGraphDocument &document() const noexcept;
  const TNodeRegistry *registry() const noexcept;
  void refreshFromDocument();
  void layout(juce::Rectangle<int> area);

private:
  void timerCallback() override;
  void rebuildAll(bool rebuildRuntime);
  void handleSelectionChanged(const std::vector<NodeId> &selectedNodeIds);
  void openProperties(NodeId nodeId);

  EditorHandle &owner;
  TGraphDocument doc;
  std::unique_ptr<TNodeRegistry> registryStore;
  TGraphRuntime runtime;
  std::unique_ptr<TGraphCanvas> canvas;
  std::unique_ptr<NodeLibraryPanel> libraryPanel;
  std::unique_ptr<NodePropertiesPanel> propertiesPanel;
  juce::AudioDeviceManager *audioDeviceManager = nullptr;

  juce::TextButton toggleLibraryButton;
  juce::TextButton quickAddButton;
  juce::TextButton findNodeButton;
  juce::TextButton commandPaletteButton;

  bool libraryVisible = true;
  std::uint64_t lastDocumentRevision = 0;
  std::uint64_t lastRuntimeRevision = 0;
  std::uint64_t lastBindingRevision = 0;
  ParamBindingRevisionProvider bindingRevisionProvider;
};

} // namespace Teul
