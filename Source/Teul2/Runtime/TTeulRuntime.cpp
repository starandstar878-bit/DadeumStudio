#include "Teul2/Runtime/TTeulRuntime.h"

#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
#include "Teul2/Runtime/AudioGraph/TRuntimeDiagnostics.h"
#include "Teul2/Runtime/IOControl/TRuntimeDeviceManager.h"
#include "Teul2/Runtime/IOControl/TRuntimeEvent.h"
#include "Teul2/Runtime/IOControl/TRuntimeEventQueue.h"

namespace Teul {

struct TTeulRuntime::Impl {
  explicit Impl(const TNodeRegistry *registryIn) : registry(registryIn) {}

  void pushRuntimeEvent(TRuntimeEvent event) {
    eventQueue.push(std::move(event));
    deviceManager.consume(eventQueue.drain());
  }

  const TNodeRegistry *registry;
  TRuntimeEventQueue eventQueue;
  TRuntimeDeviceManager deviceManager;
  TRuntimeStats stats;

  // FUTURE: Teul2 고유의 DSP 엔진 인스턴스 (Phase 3)
  // std::unique_ptr<TGraphProcessor> graphProcessor;
};

TTeulRuntime::TTeulRuntime(const TNodeRegistry *registry)
    : impl(std::make_unique<Impl>(registry)) {}

TTeulRuntime::~TTeulRuntime() = default;

bool TTeulRuntime::buildGraph(const TTeulDocument &document) {
  if (impl == nullptr)
    return false;

  const auto runtimeRevision = document.getRuntimeRevision();
  impl->pushRuntimeEvent(TRuntimeEvent::makeGraphBuildRequested(runtimeRevision));

  // Teul2 전용 컴파일: 현재는 JSON 변환까지만 수행 (실행 로직은 Phase 3 에서)
  juce::var json = TGraphCompiler::compileDocumentJson(document);

  impl->pushRuntimeEvent(TRuntimeEvent::makeGraphBuildCommitted(runtimeRevision));
  return true;
}

void TTeulRuntime::prepareToPlay(double sampleRate,
                                 int maximumExpectedSamplesPerBlock) {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makePrepareToPlay(sampleRate, maximumExpectedSamplesPerBlock));
  
  impl->stats.sampleRate = sampleRate;
  impl->stats.preparedBlockSize = maximumExpectedSamplesPerBlock;
}

void TTeulRuntime::releaseResources() {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeReleaseResources());
}

void TTeulRuntime::processBlock(juce::AudioBuffer<float> &deviceBuffer,
                                juce::MidiBuffer &midiMessages) {
  if (impl == nullptr)
    return;

  // Teul2 고유 DSP 엔진의 실시간 처리 (Phase 3-4 구현 예정)
  deviceBuffer.clear();
  midiMessages.clear();
  
  impl->stats.processBlockCount++;
}

void TTeulRuntime::setCurrentChannelLayout(int inputChannels,
                                           int outputChannels) noexcept {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeChannelLayoutChanged(inputChannels, outputChannels));
  
  impl->stats.lastInputChannels = inputChannels;
  impl->stats.lastOutputChannels = outputChannels;
}

void TTeulRuntime::setMidiOutputSink(
    std::function<void(const juce::MidiBuffer &)> sinkCallback) {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeMidiOutputSinkChanged(static_cast<bool>(sinkCallback)));
}

void TTeulRuntime::audioDeviceAboutToStart(juce::AudioIODevice *device) {
  if (impl == nullptr)
    return;

  if (device != nullptr) {
    impl->pushRuntimeEvent(TRuntimeEvent::makeAudioDeviceStarted(
        device->getName(), device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples()));
  }
}

void TTeulRuntime::audioDeviceStopped() {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeAudioDeviceStopped());
}

void TTeulRuntime::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext &context) {
  if (impl == nullptr)
    return;

  // 오디오 콜백 루프
  juce::AudioBuffer<float> buffer(const_cast<float **>(outputChannelData), numOutputChannels, numSamples);
  juce::MidiBuffer midi;
  processBlock(buffer, midi);
}

void TTeulRuntime::queueParameterChange(NodeId nodeId,
                                         const juce::String &paramKey,
                                         float value) {
  if (impl == nullptr)
    return;

  // 파라미터 업데이트 큐잉
  impl->stats.paramChangeCount++;
}

bool TTeulRuntime::applyControlSourceValue(const juce::String &sourceId,
                                            const juce::String &portId,
                                            float normalizedValue) {
  return false;
}

float TTeulRuntime::getPortLevel(PortId portId) const noexcept {
  juce::ignoreUnused(portId);
  return 0.0f;
}

TTeulRuntime::RuntimeStats TTeulRuntime::getRuntimeStats() const noexcept {
  if (impl == nullptr)
    return {};

  RuntimeStats stats = impl->stats;
  TRuntimeDiagnostics::mergeCoordinatorState(stats, impl->deviceManager.snapshot());
  return stats;
}

std::vector<TTeulExposedParam> TTeulRuntime::listExposedParams() const {
  return {};
}

juce::var TTeulRuntime::getParam(const juce::String &paramId) const {
  juce::ignoreUnused(paramId);
  return juce::var{};
}

bool TTeulRuntime::setParam(const juce::String &paramId,
                             const juce::var &value) {
  juce::ignoreUnused(paramId, value);
  return false;
}

void TTeulRuntime::addListener(Listener *listener) {
  juce::ignoreUnused(listener);
}

void TTeulRuntime::removeListener(Listener *listener) {
  juce::ignoreUnused(listener);
}

} // namespace Teul