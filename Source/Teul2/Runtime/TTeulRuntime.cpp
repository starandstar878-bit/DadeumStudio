#include "Teul2/Runtime/TTeulRuntime.h"

#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
#include "Teul2/Runtime/AudioGraph/TRuntimeDiagnostics.h"
#include "Teul2/Runtime/IOControl/TRuntimeDeviceManager.h"
#include "Teul2/Runtime/IOControl/TRuntimeEvent.h"
#include "Teul2/Runtime/IOControl/TRuntimeEventQueue.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/Runtime/TGraphRuntime.h"

namespace Teul {
namespace {

TRuntimeStats copyLegacyRuntimeStats(
    const TGraphRuntime::RuntimeStats &legacyStats) noexcept {
  TRuntimeStats stats;
  stats.sampleRate = legacyStats.sampleRate;
  stats.preparedBlockSize = legacyStats.preparedBlockSize;
  stats.lastInputChannels = legacyStats.lastInputChannels;
  stats.lastOutputChannels = legacyStats.lastOutputChannels;
  stats.activeNodeCount = legacyStats.activeNodeCount;
  stats.allocatedPortChannels = legacyStats.allocatedPortChannels;
  stats.largestBlockSeen = legacyStats.largestBlockSeen;
  stats.largestOutputChannelCountSeen =
      legacyStats.largestOutputChannelCountSeen;
  stats.smoothingActiveCount = legacyStats.smoothingActiveCount;
  stats.processBlockCount = legacyStats.processBlockCount;
  stats.rebuildRequestCount = legacyStats.rebuildRequestCount;
  stats.rebuildCommitCount = legacyStats.rebuildCommitCount;
  stats.paramChangeCount = legacyStats.paramChangeCount;
  stats.droppedParamChangeCount = legacyStats.droppedParamChangeCount;
  stats.droppedParamNotificationCount =
      legacyStats.droppedParamNotificationCount;
  stats.activeGeneration = legacyStats.activeGeneration;
  stats.pendingGeneration = legacyStats.pendingGeneration;
  stats.rebuildPending = legacyStats.rebuildPending;
  stats.clipDetected = legacyStats.clipDetected;
  stats.denormalDetected = legacyStats.denormalDetected;
  stats.xrunDetected = legacyStats.xrunDetected;
  stats.mutedFallbackActive = legacyStats.mutedFallbackActive;
  stats.cpuLoadPercent = legacyStats.cpuLoadPercent;
  stats.lastBuildMilliseconds = legacyStats.lastBuildMilliseconds;
  stats.maxBuildMilliseconds = legacyStats.maxBuildMilliseconds;
  stats.lastProcessMilliseconds = legacyStats.lastProcessMilliseconds;
  stats.maxProcessMilliseconds = legacyStats.maxProcessMilliseconds;
  return stats;
}

} // namespace

struct TTeulRuntime::Impl {
  explicit Impl(const TNodeRegistry *registryIn) : runtime(registryIn) {}

  void pushRuntimeEvent(TRuntimeEvent event) {
    eventQueue.push(std::move(event));
    deviceManager.consume(eventQueue.drain());
  }

  TGraphDocument legacyDocument;
  TGraphRuntime runtime;
  TRuntimeEventQueue eventQueue;
  TRuntimeDeviceManager deviceManager;
};

TTeulRuntime::TTeulRuntime(const TNodeRegistry *registry)
    : impl(std::make_unique<Impl>(registry)) {}

TTeulRuntime::~TTeulRuntime() = default;

bool TTeulRuntime::buildGraph(const TTeulDocument &document) {
  if (impl == nullptr)
    return false;

  const auto runtimeRevision = document.getRuntimeRevision();
  impl->pushRuntimeEvent(
      TRuntimeEvent::makeGraphBuildRequested(runtimeRevision));

  if (!TGraphCompiler::compileLegacyDocument(impl->legacyDocument, document)) {
    impl->pushRuntimeEvent(TRuntimeEvent::makeGraphBuildFailed(runtimeRevision));
    return false;
  }

  const bool didBuild = impl->runtime.buildGraph(impl->legacyDocument);
  impl->pushRuntimeEvent(didBuild
                             ? TRuntimeEvent::makeGraphBuildCommitted(
                                   runtimeRevision)
                             : TRuntimeEvent::makeGraphBuildFailed(
                                   runtimeRevision));
  return didBuild;
}

void TTeulRuntime::prepareToPlay(double sampleRate,
                                 int maximumExpectedSamplesPerBlock) {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(
      TRuntimeEvent::makePrepareToPlay(sampleRate,
                                       maximumExpectedSamplesPerBlock));
  impl->runtime.prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
}

void TTeulRuntime::releaseResources() {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeReleaseResources());
  impl->runtime.releaseResources();
}

void TTeulRuntime::processBlock(juce::AudioBuffer<float> &deviceBuffer,
                                juce::MidiBuffer &midiMessages) {
  if (impl == nullptr)
    return;

  impl->runtime.processBlock(deviceBuffer, midiMessages);
}

void TTeulRuntime::setCurrentChannelLayout(int inputChannels,
                                           int outputChannels) noexcept {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeChannelLayoutChanged(
      inputChannels, outputChannels));
  impl->runtime.setCurrentChannelLayout(inputChannels, outputChannels);
}

void TTeulRuntime::setMidiOutputSink(
    std::function<void(const juce::MidiBuffer &)> sinkCallback) {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeMidiOutputSinkChanged(
      static_cast<bool>(sinkCallback)));
  impl->runtime.setMidiOutputSink(std::move(sinkCallback));
}

void TTeulRuntime::audioDeviceAboutToStart(juce::AudioIODevice *device) {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeAudioDeviceStarted(
      device != nullptr ? device->getName() : juce::String(),
      device != nullptr ? device->getCurrentSampleRate() : 0.0,
      device != nullptr ? device->getCurrentBufferSizeSamples() : 0));
  impl->runtime.audioDeviceAboutToStart(device);
}

void TTeulRuntime::audioDeviceStopped() {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeAudioDeviceStopped());
  impl->runtime.audioDeviceStopped();
}

void TTeulRuntime::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext &context) {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeChannelLayoutChanged(
      numInputChannels, numOutputChannels));
  impl->runtime.audioDeviceIOCallbackWithContext(
      inputChannelData, numInputChannels, outputChannelData,
      numOutputChannels, numSamples, context);
}

void TTeulRuntime::queueParameterChange(NodeId nodeId,
                                        const juce::String &paramKey,
                                        float value) {
  if (impl == nullptr)
    return;

  impl->runtime.queueParameterChange(nodeId, paramKey, value);
}

bool TTeulRuntime::applyControlSourceValue(const juce::String &sourceId,
                                           const juce::String &portId,
                                           float normalizedValue) {
  return impl != nullptr &&
         impl->runtime.applyControlSourceValue(sourceId, portId,
                                               normalizedValue);
}

float TTeulRuntime::getPortLevel(PortId portId) const noexcept {
  return impl != nullptr ? impl->runtime.getPortLevel(portId) : 0.0f;
}

TTeulRuntime::RuntimeStats TTeulRuntime::getRuntimeStats() const noexcept {
  if (impl == nullptr)
    return {};

  RuntimeStats stats = copyLegacyRuntimeStats(impl->runtime.getRuntimeStats());
  TRuntimeDiagnostics::mergeCoordinatorState(stats,
                                             impl->deviceManager.snapshot());
  return stats;
}

std::vector<TTeulExposedParam> TTeulRuntime::listExposedParams() const {
  return impl != nullptr ? impl->runtime.listExposedParams()
                         : std::vector<TTeulExposedParam>{};
}

juce::var TTeulRuntime::getParam(const juce::String &paramId) const {
  return impl != nullptr ? impl->runtime.getParam(paramId) : juce::var{};
}

bool TTeulRuntime::setParam(const juce::String &paramId,
                            const juce::var &value) {
  return impl != nullptr && impl->runtime.setParam(paramId, value);
}

void TTeulRuntime::addListener(Listener *listener) {
  if (impl == nullptr)
    return;

  impl->runtime.addListener(listener);
}

void TTeulRuntime::removeListener(Listener *listener) {
  if (impl == nullptr)
    return;

  impl->runtime.removeListener(listener);
}

} // namespace Teul