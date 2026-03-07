#include "Teul/Public/EditorHandle.h"
#include "Teul/Editor/EditorHandleImpl.h"

namespace Teul {

EditorHandle::EditorHandle(juce::AudioDeviceManager *audioDeviceManager,
                           ParamBindingSummaryResolver bindingSummaryResolver,
                           ParamBindingRevisionProvider bindingRevisionProvider) {
  impl = std::make_unique<Impl>(*this, audioDeviceManager,
                                std::move(bindingSummaryResolver),
                                std::move(bindingRevisionProvider));
  resized();
}

EditorHandle::~EditorHandle() = default;

TGraphDocument &EditorHandle::document() noexcept { return impl->document(); }

const TGraphDocument &EditorHandle::document() const noexcept {
  return impl->document();
}

void EditorHandle::refreshFromDocument() { impl->refreshFromDocument(); }

TExportResult EditorHandle::runExportDryRun(
    const TExportOptions &options) const {
  if (impl == nullptr || impl->registry() == nullptr)
    return {};

  return TExporter::run(impl->document(), *impl->registry(), options);
}

void EditorHandle::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff121212));
}

void EditorHandle::resized() {
  if (impl == nullptr)
    return;

  impl->layout(getLocalBounds());
}

std::unique_ptr<EditorHandle> createEditor(
    juce::AudioDeviceManager *audioDeviceManager,
    ParamBindingSummaryResolver bindingSummaryResolver,
    ParamBindingRevisionProvider bindingRevisionProvider) {
  return std::make_unique<EditorHandle>(audioDeviceManager,
                                        std::move(bindingSummaryResolver),
                                        std::move(bindingRevisionProvider));
}

} // namespace Teul
