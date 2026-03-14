#include "Teul2/Runtime/TTeulRuntime.h"
 
 #include "Teul2/Document/TTeulDocument.h"
 #include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
 #include "Teul2/Runtime/AudioGraph/TGraphProcessor.h"
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
 
   // 빌드된 그래프를 활성 상태로 전환
   impl->activeState = std::move(newGraph);
 
   impl->pushRuntimeEvent(TRuntimeEvent::makeGraphBuildCommitted(
       runtimeRevision, impl->activeState->nodeCount,
       impl->activeState->audioPortCount, impl->activeState->controlPortCount));
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
 
   const auto processStartTicks = juce::Time::getHighResolutionTicks();
   const int numSamples = deviceBuffer.getNumSamples();
   auto state = impl->activeState.get();
 
   if (state == nullptr) {
     deviceBuffer.clear();
     midiMessages.clear();
     return;
   }
 
   // Global Port Buffer 초기화
   state->globalPortBuffer.clear();
 
   // 1. Rail Audio Input -> Global Port Buffer
   for (const auto &railInput : state->railInputSources) {
     if (railInput.dataType != TPortDataType::Audio || 
         railInput.deviceChannelIndex < 0 || 
         railInput.deviceChannelIndex >= deviceBuffer.getNumChannels())
       continue;
 
     state->globalPortBuffer.copyFrom(railInput.channelIndex, 0,
                                      deviceBuffer, railInput.deviceChannelIndex, 0, numSamples);
   }
 
   deviceBuffer.clear();
 
   // 2. Topological Node Execution
   for (std::size_t entryIndex = 0; entryIndex < state->sortedNodes.size(); ++entryIndex) {
     auto &entry = state->sortedNodes[entryIndex];
     
     if (entry.instance) {
       TProcessContext ctx;
       ctx.globalPortBuffer = &state->globalPortBuffer;
       ctx.deviceAudioBuffer = &deviceBuffer;
       ctx.midiMessages = nullptr; // Node-specific MIDI handled separately
       ctx.portToChannel = &entry.portChannels;
       ctx.nodeData = &entry.nodeSnapshot;
       
       entry.instance->processSamples(ctx);
     }
   }
 
   // 3. Rail Audio Output (Global Port Buffer -> Device Buffer)
   for (const auto &railOutput : state->railOutputTargets) {
     if (railOutput.dataType != TPortDataType::Audio ||
         railOutput.sourceChannelIndex < 0 ||
         railOutput.sourceChannelIndex >= state->globalPortBuffer.getNumChannels() ||
         railOutput.deviceChannelIndex < 0 ||
         railOutput.deviceChannelIndex >= deviceBuffer.getNumChannels())
       continue;
 
     deviceBuffer.addFrom(railOutput.deviceChannelIndex, 0,
                          state->globalPortBuffer, railOutput.sourceChannelIndex, 0, numSamples);
   }
 
   // Health Detection
   bool clipped = false;
   bool denormal = false;
   for (int ch = 0; ch < deviceBuffer.getNumChannels(); ++ch) {
     const float *samples = deviceBuffer.getReadPointer(ch);
     for (int i = 0; i < numSamples; ++i) {
       const float mag = std::abs(samples[i]);
       if (mag > 1.0f) clipped = true;
       if (mag > 0.0f && mag < 1.0e-20f) denormal = true;
     }
   }
 
   impl->stats.clipDetected = clipped;
   impl->stats.denormalDetected = denormal;
 
   // Signal Level Metering
   if (state->portLevels) {
     for (std::size_t i = 0; i < state->portTelemetry.size(); ++i) {
       const auto &telemetry = state->portTelemetry[i];
       if (telemetry.channelIndex >= 0 && telemetry.channelIndex < state->globalPortBuffer.getNumChannels()) {
         const float measured = TRuntimeDiagnostics::measureSignalLevel(
             state->globalPortBuffer.getReadPointer(telemetry.channelIndex), numSamples);
         const float previous = state->portLevels[i].load(std::memory_order_relaxed);
         state->portLevels[i].store(TRuntimeDiagnostics::smoothMeterLevel(previous, measured), std::memory_order_relaxed);
       }
     }
   }
 
   // Performance Stats Update
   const auto elapsedMicros = TRuntimeDiagnostics::ticksToMicros(juce::Time::getHighResolutionTicks() - processStartTicks);
   impl->stats.lastProcessMilliseconds = TRuntimeDiagnostics::microsToMilliseconds(elapsedMicros);
   if (impl->stats.lastProcessMilliseconds > impl->stats.maxProcessMilliseconds)
     impl->stats.maxProcessMilliseconds = impl->stats.lastProcessMilliseconds;
 
   const double blockBudgetMicros = impl->stats.sampleRate > 0.0 ? ((double)numSamples / impl->stats.sampleRate) * 1000000.0 : 0.0;
   impl->stats.xrunDetected = (blockBudgetMicros > 0.0 && (double)elapsedMicros > blockBudgetMicros);
 
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
    for (auto &entry : impl->activeState->sortedNodes) {
      if (entry.nodeId == nodeId && entry.instance) {
        entry.instance->setParameterValue(paramKey, value);
        break;
      }
    }
   }
 
   impl->stats.paramChangeCount++;
 }
 
 bool TTeulRuntime::applyControlSourceValue(const juce::String &sourceId,
                                            const juce::String &portId,
                                            float normalizedValue) {
   juce::ignoreUnused(sourceId, portId, normalizedValue);
   return false;
 }
 
 float TTeulRuntime::getPortLevel(PortId portId) const noexcept {
   if (impl && impl->activeState && impl->activeState->portLevels) {
     const auto it = impl->activeState->portTelemetryIndex.find(portId);
     if (it != impl->activeState->portTelemetryIndex.end())
       return impl->activeState->portLevels[it->second].load(std::memory_order_relaxed);
   }
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
     std::vector<TTeulExposedParam> result;
     for (const auto &entry : impl->activeState->sortedNodes) {
       auto params = impl->registry->listExposedParamsForNode(entry.nodeSnapshot);
       result.insert(result.end(), params.begin(), params.end());
     }
     return result;
   }
   return {};
 }
 
 juce::var TTeulRuntime::getParam(const juce::String &paramId) const {
   const int separatorIndex = paramId.indexOf("::");
   if (separatorIndex <= 0)
     return {};
 
   const juce::String nodeIdStr = paramId.substring(0, separatorIndex);
   const juce::String paramKey = paramId.substring(separatorIndex + 2);
 
   if (nodeIdStr.isNotEmpty() && impl && impl->activeState) {
     const NodeId nodeId = static_cast<NodeId>(nodeIdStr.getIntValue());
     for (const auto &entry : impl->activeState->sortedNodes) {
       if (entry.nodeId == nodeId) {
         const auto it = entry.nodeSnapshot.params.find(paramKey);
         if (it != entry.nodeSnapshot.params.end())
           return it->second;
         break;
       }
     }
   }
   return {};
 }
 
 bool TTeulRuntime::setParam(const juce::String &paramId,
                             const juce::var &value) {
   const int separatorIndex = paramId.indexOf("::");
   if (separatorIndex <= 0)
     return false;
 
   const juce::String nodeIdStr = paramId.substring(0, separatorIndex);
   const juce::String paramKey = paramId.substring(separatorIndex + 2);
 
   if (nodeIdStr.isNotEmpty()) {
     const NodeId nodeId = static_cast<NodeId>(nodeIdStr.getIntValue());
     queueParameterChange(nodeId, paramKey, static_cast<float>(value));
     return true;
   }
   return false;
 }
 
 TRuntimeDeviceManager &TTeulRuntime::getDeviceManager() noexcept {
   return impl->deviceManager;
 }
 
 void TTeulRuntime::addListener(Listener *listener) {
   juce::ignoreUnused(listener);
 }
 
 void TTeulRuntime::removeListener(Listener *listener) {
   juce::ignoreUnused(listener);
 }
 
 } // namespace Teul
