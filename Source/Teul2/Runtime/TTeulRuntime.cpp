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

  TCompiledGraph::Ptr activeState;
};

TTeulRuntime::TTeulRuntime(const TNodeRegistry *registry)
    : impl(std::make_unique<Impl>(registry)) {}

TTeulRuntime::~TTeulRuntime() = default;

bool TTeulRuntime::buildGraph(const TTeulDocument &document) {
  if (impl == nullptr || impl->registry == nullptr)
    return false;

  const auto runtimeRevision = document.getRuntimeRevision();
  impl->pushRuntimeEvent(TRuntimeEvent::makeGraphBuildRequested(runtimeRevision));

  // TGraphCompiler를 통한 실제 런타임 그래프 빌드
  auto newGraph = TGraphCompiler::compileDocument(
      document, *impl->registry, impl->stats.sampleRate, impl->stats.preparedBlockSize);

  if (newGraph == nullptr) {
    impl->pushRuntimeEvent(TRuntimeEvent::makeGraphBuildFailed(runtimeRevision));
    return false;
  }

  // 빌드된 그래프를 활성 상태로 전환 (원자적 교체는 Phase 3-4에서 상세화)
  impl->activeState = std::move(newGraph);

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

  if (impl->activeState != nullptr) {
    for (auto &entry : impl->activeState->sortedNodes) {
      if (entry.instance)
        entry.instance->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
    }
  }
}

void TTeulRuntime::releaseResources() {
  if (impl == nullptr)
    return;

  impl->pushRuntimeEvent(TRuntimeEvent::makeReleaseResources());

  if (impl->activeState != nullptr) {
    for (auto &entry : impl->activeState->sortedNodes) {
      if (entry.instance)
        entry.instance->releaseResources();
    }
  }
}

void TTeulRuntime::processBlock(juce::AudioBuffer<float> &deviceBuffer,
                                juce::MidiBuffer &midiMessages) {
  if (impl == nullptr)
    return;

  // Teul2 전용 DSP 엔진의 실전 처리 (Phase 3-4 구현 예정)
  // 현재는 컴파일된 그래프의 존재 여부만 확인하고 버퍼를 클리어합니다.
  if (impl->activeState != nullptr) {
    // TODO: impl->activeState->sortedNodes 순회하며 processSamples 호출 로직 추가 예정
  }

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

  juce::AudioBuffer<float> buffer(const_cast<float **>(outputChannelData), numOutputChannels, numSamples);
  juce::MidiBuffer midi;
  processBlock(buffer, midi);
}

void TTeulRuntime::queueParameterChange(NodeId nodeId,
                                         const juce::String &paramKey,
                                         float value) {
  if (impl == nullptr)
    return;

  if (impl->activeState != nullptr) {
    for (auto &dispatch : impl->activeState->paramDispatches) {
      if (dispatch.nodeId == nodeId && dispatch.paramKey == paramKey) {
        if (dispatch.instance)
          dispatch.instance->setParameterValue(paramKey, value);
        break;
      }
    }
  }

  impl->stats.paramChangeCount++;
}

bool TTeulRuntime::applyControlSourceValue(const juce::String &sourceId,
                                            const juce::String &portId,
                                            float normalizedValue) {
  return false;
}

float TTeulRuntime::getPortLevel(PortId portId) const noexcept {
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
  if (impl && impl->activeState && impl->registry) {
    // 임시: 활성 노드들로부터 노출 파라미터 리스트 생성
    std::vector<TTeulExposedParam> result;
    for (const auto& entry : impl->activeState->sortedNodes) {
        auto params = impl->registry->listExposedParamsForNode(entry.nodeSnapshot);
        result.insert(result.end(), params.begin(), params.end());
    }
    return result;
  }
  return {};
}

juce::var TTeulRuntime::getParam(const juce::String &paramId) const {
  return juce::var{};
}

bool TTeulRuntime::setParam(const juce::String &paramId,
                             const juce::var &value) {
  return false;
}

void TTeulRuntime::addListener(Listener *listener) {
  juce::ignoreUnused(listener);
}

void TTeulRuntime::removeListener(Listener *listener) {
  juce::ignoreUnused(listener);
}

} // namespace Teul