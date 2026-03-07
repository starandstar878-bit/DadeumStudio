#include "Teul/Editor/EditorHandleImpl.h"

#include "Teul/Editor/Panels/NodeLibraryPanel.h"
#include "Teul/Editor/Panels/NodePropertiesPanel.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/UI/Canvas/TGraphCanvas.h"

namespace Teul {

EditorHandle::Impl::Impl(
    EditorHandle &ownerIn, juce::AudioDeviceManager *audioDeviceManagerIn,
    ParamBindingSummaryResolver bindingSummaryResolverIn,
    ParamBindingRevisionProvider bindingRevisionProviderIn)
    : owner(ownerIn), registryStore(makeDefaultNodeRegistry()),
      runtime(registryStore.get()), audioDeviceManager(audioDeviceManagerIn),
      bindingRevisionProvider(std::move(bindingRevisionProviderIn)) {
  canvas = std::make_unique<TGraphCanvas>(doc);
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

  canvas->setNodeSelectionChangedHandler(
      [this](const std::vector<NodeId> &selectedNodeIds) {
        handleSelectionChanged(selectedNodeIds);
      });

  owner.addAndMakeVisible(toggleLibraryButton);
  owner.addAndMakeVisible(quickAddButton);
  owner.addAndMakeVisible(findNodeButton);
  owner.addAndMakeVisible(commandPaletteButton);

  toggleLibraryButton.setButtonText("Library");
  quickAddButton.setButtonText("Quick Add");
  findNodeButton.setButtonText("Find Node");
  commandPaletteButton.setButtonText("Cmd");

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

  rebuildAll(true);

  if (audioDeviceManager != nullptr)
    audioDeviceManager->addAudioCallback(&runtime);

  startTimerHz(20);
}

EditorHandle::Impl::~Impl() {
  stopTimer();
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

void EditorHandle::Impl::layout(juce::Rectangle<int> area) {
  auto top = area.removeFromTop(36).reduced(6, 4);

  toggleLibraryButton.setBounds(top.removeFromLeft(80));
  top.removeFromLeft(4);
  quickAddButton.setBounds(top.removeFromLeft(90));
  top.removeFromLeft(4);
  findNodeButton.setBounds(top.removeFromLeft(90));
  top.removeFromLeft(4);
  commandPaletteButton.setBounds(top.removeFromLeft(60));

  if (libraryPanel != nullptr) {
    libraryPanel->setVisible(libraryVisible);
    if (libraryVisible) {
      auto left = area.removeFromLeft(276);
      libraryPanel->setBounds(left);
    }
  }

  if (propertiesPanel != nullptr && propertiesPanel->isPanelOpen()) {
    auto right = area.removeFromRight(316);
    propertiesPanel->setBounds(right);
  }

  if (canvas != nullptr)
    canvas->setBounds(area);
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
    lastDocumentRevision = currentDocumentRevision;
  }

  const auto currentBindingRevision =
      bindingRevisionProvider != nullptr ? bindingRevisionProvider() : 0;
  if (currentBindingRevision != lastBindingRevision) {
    if (propertiesPanel != nullptr)
      propertiesPanel->refreshBindingSummaries();
    lastBindingRevision = currentBindingRevision;
  }
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

  lastDocumentRevision = doc.getDocumentRevision();
  lastRuntimeRevision = doc.getRuntimeRevision();
  owner.resized();
}

void EditorHandle::Impl::handleSelectionChanged(
    const std::vector<NodeId> &selectedNodeIds) {
  if (propertiesPanel == nullptr)
    return;

  if (selectedNodeIds.size() == 1)
    propertiesPanel->inspectNode(selectedNodeIds.front());
  else
    propertiesPanel->hidePanel();
}

void EditorHandle::Impl::openProperties(NodeId nodeId) {
  if (propertiesPanel != nullptr)
    propertiesPanel->inspectNode(nodeId);
}

} // namespace Teul
