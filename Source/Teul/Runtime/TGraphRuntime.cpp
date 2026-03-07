#include "TGraphRuntime.h"

#include <cmath>
#include <cstring>
#include <map>
#include <queue>
#include <set>

namespace Teul {
namespace {

juce::var coerceNumericValue(const juce::var &prototype, float value) {
  if (prototype.isBool())
    return value >= 0.5f;
  if (prototype.isInt())
    return juce::roundToInt(value);
  if (prototype.isInt64())
    return static_cast<juce::int64>(juce::roundToInt(value));
  if (prototype.isDouble())
    return static_cast<double>(value);
  return value;
}

} // namespace

TGraphRuntime::TGraphRuntime(const TNodeRegistry *registry)
    : nodeRegistry(registry) {}

TGraphRuntime::~TGraphRuntime() {
  cancelPendingUpdate();
  releaseResources();
  pendingState.set(nullptr);
  activeState.set(nullptr);
}

bool TGraphRuntime::buildGraph(const TGraphDocument &doc) {
  rebuildRequestCount.fetch_add(1, std::memory_order_relaxed);
  const auto buildStartTicks = juce::Time::getHighResolutionTicks();

  std::map<NodeId, int> inDegree;
  std::map<NodeId, std::vector<NodeId>> adj;

  for (const auto &node : doc.nodes)
    inDegree[node.nodeId] = 0;

  for (const auto &conn : doc.connections) {
    adj[conn.from.nodeId].push_back(conn.to.nodeId);
    inDegree[conn.to.nodeId]++;
  }

  std::queue<NodeId> q;
  for (auto &pair : inDegree) {
    if (pair.second == 0)
      q.push(pair.first);
  }

  std::vector<NodeId> sortedIds;
  while (!q.empty()) {
    const NodeId curr = q.front();
    q.pop();
    sortedIds.push_back(curr);

    for (const NodeId neighbor : adj[curr]) {
      if (--inDegree[neighbor] == 0)
        q.push(neighbor);
    }
  }

  if (sortedIds.size() != doc.nodes.size())
    return false;

  std::vector<NodeEntry> newSortedNodes;
  newSortedNodes.reserve(sortedIds.size());

  int portChannelCounter = 0;
  std::map<PortId, int> globalPortIndexMap;
  const double sampleRate = currentSampleRate.load(std::memory_order_relaxed);
  const int blockSize = currentBlockSize.load(std::memory_order_relaxed);

  for (const auto &id : sortedIds) {
    const TNode *node = doc.findNode(id);
    if (node == nullptr)
      continue;

    NodeEntry entry;
    entry.nodeId = node->nodeId;
    entry.nodeSnapshot = *node;

    for (const auto &port : entry.nodeSnapshot.ports) {
      entry.portChannels[port.portId] = portChannelCounter;
      globalPortIndexMap[port.portId] = portChannelCounter;
      ++portChannelCounter;
    }

    const TNodeDescriptor *desc = nullptr;
    if (nodeRegistry != nullptr)
      desc = nodeRegistry->descriptorFor(entry.nodeSnapshot.typeKey);

    if (desc != nullptr && desc->instanceFactory) {
      entry.instance = desc->instanceFactory();
      if (entry.instance) {
        entry.instance->prepareToPlay(sampleRate, blockSize);
        entry.instance->reset();
        for (const auto &[key, value] : entry.nodeSnapshot.params)
          entry.instance->setParameterValue(key, paramValueToFloat(value));
      }
    }

    newSortedNodes.push_back(std::move(entry));
  }

  std::map<NodeId, std::size_t> entryIndexByNodeId;
  for (std::size_t index = 0; index < newSortedNodes.size(); ++index)
    entryIndexByNodeId[newSortedNodes[index].nodeId] = index;

  for (const auto &conn : doc.connections) {
    const auto srcChannelIt = globalPortIndexMap.find(conn.from.portId);
    const auto dstEntryIt = entryIndexByNodeId.find(conn.to.nodeId);
    if (srcChannelIt == globalPortIndexMap.end() ||
        dstEntryIt == entryIndexByNodeId.end()) {
      continue;
    }

    auto &dstEntry = newSortedNodes[dstEntryIt->second];
    const auto dstChannelIt = dstEntry.portChannels.find(conn.to.portId);
    if (dstChannelIt == dstEntry.portChannels.end())
      continue;

    dstEntry.preProcessMixes.push_back(
        {srcChannelIt->second, dstChannelIt->second});
  }

  auto newState = new RenderState();
  newState->generation =
      buildGenerationCounter.fetch_add(1, std::memory_order_relaxed) + 1;
  newState->sortedNodes = std::move(newSortedNodes);
  newState->globalPortBuffer.setSize(
      portChannelCounter > 0 ? portChannelCounter : 1, juce::jmax(1, blockSize),
      false, false, true);
  newState->globalPortBuffer.clear();
  newState->totalAllocatedChannels = portChannelCounter;

  for (const auto &entry : newState->sortedNodes) {
    for (const auto &port : entry.nodeSnapshot.ports) {
      if (port.direction != TPortDirection::Output ||
          port.dataType == TPortDataType::MIDI) {
        continue;
      }

      const auto channelIt = entry.portChannels.find(port.portId);
      if (channelIt == entry.portChannels.end())
        continue;

      newState->portTelemetryIndex[port.portId] = newState->portTelemetry.size();
      newState->portTelemetry.push_back(
          {port.portId, entry.nodeId, channelIt->second, port.dataType});
    }

    std::set<juce::String> dispatchKeys;
    if (nodeRegistry != nullptr) {
      if (const auto *desc = nodeRegistry->descriptorFor(entry.nodeSnapshot.typeKey)) {
        for (const auto &spec : desc->paramSpecs)
          dispatchKeys.insert(spec.key);
      }
    }

    for (const auto &[key, value] : entry.nodeSnapshot.params) {
      juce::ignoreUnused(value);
      dispatchKeys.insert(key);
    }

    for (const auto &key : dispatchKeys) {
      ParamDispatch dispatch;
      dispatch.nodeId = entry.nodeId;
      dispatch.instance = entry.instance.get();
      dispatch.paramKey = key;
      writeParamKey(dispatch.paramKeyUtf8, key);
      newState->paramDispatches.push_back(std::move(dispatch));
    }
  }

  if (!newState->portTelemetry.empty()) {
    newState->portLevels =
        std::make_unique<std::atomic<float>[]>(newState->portTelemetry.size());
    for (std::size_t index = 0; index < newState->portTelemetry.size(); ++index)
      newState->portLevels[index].store(0.0f, std::memory_order_relaxed);
  }

  {
    const juce::ScopedLock lock(paramSurfaceLock);
    rebuildParamSurfaceLocked(doc);
    rebuildQueuedParamDispatchLocked(*newState);
  }

  const std::uint64_t buildMicros =
      ticksToMicros(juce::Time::getHighResolutionTicks() - buildStartTicks);
  lastBuildMicros.store(buildMicros, std::memory_order_relaxed);
  updateAtomicMax(maxBuildMicros, buildMicros);

  if (!activeState.hasState()) {
    activeState.set(newState);
    pendingState.set(nullptr);
    activeGeneration.store(newState->generation, std::memory_order_release);
    pendingGeneration.store(0, std::memory_order_release);
    rebuildPending.store(false, std::memory_order_release);
    rebuildCommitCount.fetch_add(1, std::memory_order_relaxed);
    activeNodeCount.store(static_cast<int>(newState->sortedNodes.size()),
                          std::memory_order_relaxed);
    allocatedPortChannels.store(newState->totalAllocatedChannels,
                                std::memory_order_relaxed);
  } else {
    pendingState.set(newState);
    pendingGeneration.store(newState->generation, std::memory_order_release);
    rebuildPending.store(true, std::memory_order_release);
  }

  surfaceChangedPending.store(true, std::memory_order_release);
  triggerAsyncUpdate();
  return true;
}

void TGraphRuntime::prepareToPlay(double sampleRate,
                                  int maximumExpectedSamplesPerBlock) {
  currentSampleRate.store(sampleRate, std::memory_order_relaxed);
  currentBlockSize.store(juce::jmax(1, maximumExpectedSamplesPerBlock),
                         std::memory_order_relaxed);

  if (const auto state = activeState.get())
    prepareStateForPlayback(*state, sampleRate, maximumExpectedSamplesPerBlock);

  if (const auto state = pendingState.get())
    prepareStateForPlayback(*state, sampleRate, maximumExpectedSamplesPerBlock);
}

void TGraphRuntime::releaseResources() {
  auto releaseState = [](const RenderState::Ptr &state) {
    if (!state)
      return;

    for (auto &entry : state->sortedNodes) {
      if (entry.instance)
        entry.instance->releaseResources();
    }
  };

  releaseState(activeState.get());
  releaseState(pendingState.get());
}

void TGraphRuntime::processBlock(juce::AudioBuffer<float> &deviceBuffer,
                                 juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  const auto processStartTicks = juce::Time::getHighResolutionTicks();

  commitPendingStateIfNeeded();

  processBlockCount.fetch_add(1, std::memory_order_relaxed);
  lastOutputChannels.store(deviceBuffer.getNumChannels(),
                           std::memory_order_relaxed);
  updateAtomicMax(largestBlockSeen, deviceBuffer.getNumSamples());
  updateAtomicMax(largestOutputChannelCountSeen, deviceBuffer.getNumChannels());

  const auto state = activeState.get();
  deviceBuffer.clear();

  if (!state) {
    const auto elapsedMicros =
        ticksToMicros(juce::Time::getHighResolutionTicks() - processStartTicks);
    lastProcessMicros.store(elapsedMicros, std::memory_order_relaxed);
    updateAtomicMax(maxProcessMicros, elapsedMicros);
    return;
  }

  const int numSamples = juce::jmin(deviceBuffer.getNumSamples(),
                                    state->globalPortBuffer.getNumSamples());
  if (numSamples <= 0) {
    const auto elapsedMicros =
        ticksToMicros(juce::Time::getHighResolutionTicks() - processStartTicks);
    lastProcessMicros.store(elapsedMicros, std::memory_order_relaxed);
    updateAtomicMax(maxProcessMicros, elapsedMicros);
    return;
  }

  state->globalPortBuffer.clear();

  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramQueueFifo.prepareToRead(paramQueueFifo.getNumReady(), start1, size1,
                               start2, size2);

  auto applyParamChange = [&](const ParamChange &change) {
    const ParamDispatch *dispatch = nullptr;

    if (change.generation == state->generation && change.dispatchSlot >= 0 &&
        change.dispatchSlot < static_cast<int>(state->paramDispatches.size())) {
      dispatch = &state->paramDispatches[static_cast<std::size_t>(change.dispatchSlot)];
    } else {
      for (const auto &candidate : state->paramDispatches) {
        if (dispatchMatches(candidate, change.nodeId, change.paramKey)) {
          dispatch = &candidate;
          break;
        }
      }
    }

    if (dispatch == nullptr || dispatch->instance == nullptr) {
      droppedParamChangeCount.fetch_add(1, std::memory_order_relaxed);
      return;
    }

    dispatch->instance->setParameterValue(dispatch->paramKey, change.value);
    paramChangeCount.fetch_add(1, std::memory_order_relaxed);
    reportParamValueChange(change.nodeId, dispatch->paramKey, change.value);
  };

  for (int i = 0; i < size1; ++i)
    applyParamChange(paramQueueData[start1 + i]);
  for (int i = 0; i < size2; ++i)
    applyParamChange(paramQueueData[start2 + i]);
  paramQueueFifo.finishedRead(size1 + size2);

  for (auto &entry : state->sortedNodes) {
    for (const auto &mix : entry.preProcessMixes) {
      state->globalPortBuffer.addFrom(mix.dstChannelIndex, 0,
                                      state->globalPortBuffer,
                                      mix.srcChannelIndex, 0, numSamples);
    }

    if (entry.instance && !entry.nodeSnapshot.bypassed) {
      TProcessContext ctx;
      ctx.globalPortBuffer = &state->globalPortBuffer;
      ctx.deviceAudioBuffer = &deviceBuffer;
      ctx.midiMessages = &midiMessages;
      ctx.portToChannel = &entry.portChannels;
      ctx.nodeData = &entry.nodeSnapshot;
      ctx.paramValueReporter = this;
      entry.instance->processSamples(ctx);
    }
  }

  if (state->portLevels) {
    for (std::size_t index = 0; index < state->portTelemetry.size(); ++index) {
      const auto &telemetry = state->portTelemetry[index];
      if (telemetry.channelIndex < 0 ||
          telemetry.channelIndex >= state->globalPortBuffer.getNumChannels()) {
        continue;
      }

      const float measured = measureSignalLevel(
          state->globalPortBuffer.getReadPointer(telemetry.channelIndex),
          numSamples);
      const float previous =
          state->portLevels[index].load(std::memory_order_relaxed);
      state->portLevels[index].store(
          smoothMeterLevel(previous, measured), std::memory_order_relaxed);
    }
  }

  const auto elapsedMicros =
      ticksToMicros(juce::Time::getHighResolutionTicks() - processStartTicks);
  lastProcessMicros.store(elapsedMicros, std::memory_order_relaxed);
  updateAtomicMax(maxProcessMicros, elapsedMicros);
}

float TGraphRuntime::getPortLevel(PortId portId) const noexcept {
  const auto state = activeState.get();
  if (!state || !state->portLevels)
    return 0.0f;

  const auto it = state->portTelemetryIndex.find(portId);
  if (it == state->portTelemetryIndex.end())
    return 0.0f;

  return state->portLevels[it->second].load(std::memory_order_relaxed);
}

TGraphRuntime::RuntimeStats TGraphRuntime::getRuntimeStats() const noexcept {
  RuntimeStats stats;
  stats.sampleRate = currentSampleRate.load(std::memory_order_relaxed);
  stats.preparedBlockSize = currentBlockSize.load(std::memory_order_relaxed);
  stats.lastOutputChannels = lastOutputChannels.load(std::memory_order_relaxed);
  stats.activeNodeCount = activeNodeCount.load(std::memory_order_relaxed);
  stats.allocatedPortChannels =
      allocatedPortChannels.load(std::memory_order_relaxed);
  stats.largestBlockSeen = largestBlockSeen.load(std::memory_order_relaxed);
  stats.largestOutputChannelCountSeen =
      largestOutputChannelCountSeen.load(std::memory_order_relaxed);
  stats.processBlockCount = processBlockCount.load(std::memory_order_relaxed);
  stats.rebuildRequestCount =
      rebuildRequestCount.load(std::memory_order_relaxed);
  stats.rebuildCommitCount =
      rebuildCommitCount.load(std::memory_order_relaxed);
  stats.paramChangeCount = paramChangeCount.load(std::memory_order_relaxed);
  stats.droppedParamChangeCount =
      droppedParamChangeCount.load(std::memory_order_relaxed);
  stats.droppedParamNotificationCount =
      droppedParamNotificationCount.load(std::memory_order_relaxed);
  stats.activeGeneration = activeGeneration.load(std::memory_order_relaxed);
  stats.pendingGeneration = pendingGeneration.load(std::memory_order_relaxed);
  stats.rebuildPending = rebuildPending.load(std::memory_order_relaxed);
  stats.lastBuildMilliseconds =
      microsToMilliseconds(lastBuildMicros.load(std::memory_order_relaxed));
  stats.maxBuildMilliseconds =
      microsToMilliseconds(maxBuildMicros.load(std::memory_order_relaxed));
  stats.lastProcessMilliseconds =
      microsToMilliseconds(lastProcessMicros.load(std::memory_order_relaxed));
  stats.maxProcessMilliseconds =
      microsToMilliseconds(maxProcessMicros.load(std::memory_order_relaxed));
  return stats;
}

void TGraphRuntime::audioDeviceAboutToStart(juce::AudioIODevice *device) {
  if (device) {
    prepareToPlay(device->getCurrentSampleRate(),
                  device->getCurrentBufferSizeSamples());
  }
}

void TGraphRuntime::audioDeviceStopped() { releaseResources(); }

void TGraphRuntime::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels,
    float *const *outputChannelData, int numOutputChannels, int numSamples,
    const juce::AudioIODeviceCallbackContext &context) {
  juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels,
                                  numSamples);
  juce::ignoreUnused(inputChannelData, numInputChannels, context);

  deviceCallbackMidiScratch.clear();
  processBlock(buffer, deviceCallbackMidiScratch);
}

void TGraphRuntime::queueParameterChange(NodeId nodeId,
                                         const juce::String &paramKey,
                                         float value) {
  int dispatchSlot = -1;
  std::uint64_t generation = 0;
  {
    const juce::ScopedLock lock(paramSurfaceLock);
    const juce::String paramId = makeTeulParamId(nodeId, paramKey);
    const auto it = queuedParamDispatchSlotById.find(paramId);
    if (it != queuedParamDispatchSlotById.end()) {
      dispatchSlot = it->second;
      generation = queuedParamDispatchGeneration;
    }
  }

  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramQueueFifo.prepareToWrite(1, start1, size1, start2, size2);

  ParamChange *slot = nullptr;
  if (size1 > 0)
    slot = &paramQueueData[start1];
  else if (size2 > 0)
    slot = &paramQueueData[start2];

  if (slot == nullptr) {
    droppedParamChangeCount.fetch_add(1, std::memory_order_relaxed);
    paramQueueFifo.finishedWrite(0);
    return;
  }

  slot->nodeId = nodeId;
  slot->generation = generation;
  slot->dispatchSlot = dispatchSlot;
  slot->value = value;
  writeParamKey(slot->paramKey, sizeof(slot->paramKey), paramKey);
  paramQueueFifo.finishedWrite(1);
}

std::vector<TTeulExposedParam> TGraphRuntime::listExposedParams() const {
  const juce::ScopedLock lock(paramSurfaceLock);
  return exposedParams;
}

juce::var TGraphRuntime::getParam(const juce::String &paramId) const {
  const juce::ScopedLock lock(paramSurfaceLock);
  const auto it = exposedParamIndexById.find(paramId);
  if (it == exposedParamIndexById.end())
    return {};

  return exposedParams[it->second].currentValue;
}

bool TGraphRuntime::setParam(const juce::String &paramId,
                             const juce::var &value) {
  TTeulExposedParam updated;
  NodeId nodeId = kInvalidNodeId;
  juce::String paramKey;

  {
    const juce::ScopedLock lock(paramSurfaceLock);
    const auto it = exposedParamIndexById.find(paramId);
    if (it == exposedParamIndexById.end())
      return false;

    auto &param = exposedParams[it->second];
    const juce::var prototype = param.currentValue.isVoid() ? param.defaultValue
                                                            : param.currentValue;
    const juce::var coerced = coerceValueLike(prototype, value);
    if (!updateParamSurfaceValueLocked(param.nodeId, param.paramKey, coerced,
                                       &updated)) {
      return false;
    }

    nodeId = updated.nodeId;
    paramKey = updated.paramKey;
  }

  queueParameterChange(nodeId, paramKey, paramValueToFloat(updated.currentValue));
  return true;
}

void TGraphRuntime::addListener(Listener *listener) { listeners.add(listener); }

void TGraphRuntime::removeListener(Listener *listener) {
  listeners.remove(listener);
}

void TGraphRuntime::handleAsyncUpdate() {
  const bool surfaceChanged =
      surfaceChangedPending.exchange(false, std::memory_order_acq_rel);

  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramNotificationFifo.prepareToRead(paramNotificationFifo.getNumReady(),
                                      start1, size1, start2, size2);

  std::vector<TTeulExposedParam> changed;
  changed.reserve(static_cast<std::size_t>(size1 + size2));

  auto drainNotification = [&](const PendingParamNotification &notification) {
    const juce::String key = juce::String::fromUTF8(notification.paramKey);
    TTeulExposedParam updated;

    const bool didUpdate = [&] {
      const juce::ScopedLock lock(paramSurfaceLock);
      const juce::String paramId = makeTeulParamId(notification.nodeId, key);
      const auto it = exposedParamIndexById.find(paramId);
      if (it == exposedParamIndexById.end())
        return false;

      auto &param = exposedParams[it->second];
      const juce::var prototype = param.currentValue.isVoid() ? param.defaultValue
                                                              : param.currentValue;
      const juce::var coerced = coerceNumericValue(prototype, notification.value);
      return updateParamSurfaceValueLocked(notification.nodeId, key, coerced,
                                           &updated);
    }();

    if (didUpdate)
      changed.push_back(std::move(updated));
  };

  for (int i = 0; i < size1; ++i)
    drainNotification(paramNotificationData[start1 + i]);
  for (int i = 0; i < size2; ++i)
    drainNotification(paramNotificationData[start2 + i]);
  paramNotificationFifo.finishedRead(size1 + size2);

  if (surfaceChanged) {
    listeners.call(
        [](Listener &listener) { listener.teulParamSurfaceChanged(); });
  }

  for (const auto &param : changed) {
    listeners.call(
        [&](Listener &listener) { listener.teulParamValueChanged(param); });
  }
}

void TGraphRuntime::reportParamValueChange(NodeId nodeId,
                                           const juce::String &paramKey,
                                           float value) {
  enqueueParamNotification(nodeId, paramKey, value);
}

void TGraphRuntime::rebuildParamSurfaceLocked(const TGraphDocument &doc) {
  surfaceDocument = doc;
  exposedParams.clear();
  exposedParamIndexById.clear();

  for (const auto &node : surfaceDocument.nodes) {
    std::vector<TTeulExposedParam> nodeParams;
    if (nodeRegistry)
      nodeParams = nodeRegistry->listExposedParamsForNode(node);
    else {
      for (const auto &[key, currentValue] : node.params) {
        TTeulExposedParam param;
        param.paramId = makeTeulParamId(node.nodeId, key);
        param.nodeId = node.nodeId;
        param.nodeTypeKey = node.typeKey;
        param.nodeDisplayName = node.label.isNotEmpty() ? node.label : node.typeKey;
        param.paramKey = key;
        param.paramLabel = key;
        param.defaultValue = currentValue;
        param.currentValue = currentValue;
        nodeParams.push_back(std::move(param));
      }
    }

    for (auto &param : nodeParams) {
      exposedParamIndexById[param.paramId] = exposedParams.size();
      exposedParams.push_back(std::move(param));
    }
  }
}

bool TGraphRuntime::updateParamSurfaceValueLocked(
    NodeId nodeId, const juce::String &paramKey, const juce::var &value,
    TTeulExposedParam *updatedParam) {
  TNode *node = surfaceDocument.findNode(nodeId);
  if (node == nullptr)
    return false;

  node->params[paramKey] = value;

  const juce::String paramId = makeTeulParamId(nodeId, paramKey);
  const auto it = exposedParamIndexById.find(paramId);
  if (it == exposedParamIndexById.end())
    return false;

  auto &param = exposedParams[it->second];
  param.currentValue = value;

  if (updatedParam != nullptr)
    *updatedParam = param;

  return true;
}

void TGraphRuntime::rebuildQueuedParamDispatchLocked(const RenderState &state) {
  queuedParamDispatchSlotById.clear();
  queuedParamDispatchGeneration = state.generation;

  for (std::size_t index = 0; index < state.paramDispatches.size(); ++index) {
    const auto &dispatch = state.paramDispatches[index];
    queuedParamDispatchSlotById[makeTeulParamId(dispatch.nodeId, dispatch.paramKey)] =
        static_cast<int>(index);
  }
}

void TGraphRuntime::enqueueParamNotification(NodeId nodeId,
                                             const juce::String &paramKey,
                                             float value) {
  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramNotificationFifo.prepareToWrite(1, start1, size1, start2, size2);

  PendingParamNotification *slot = nullptr;
  if (size1 > 0)
    slot = &paramNotificationData[start1];
  else if (size2 > 0)
    slot = &paramNotificationData[start2];

  if (slot == nullptr) {
    droppedParamNotificationCount.fetch_add(1, std::memory_order_relaxed);
    paramNotificationFifo.finishedWrite(0);
    return;
  }

  slot->nodeId = nodeId;
  slot->value = value;
  writeParamKey(slot->paramKey, sizeof(slot->paramKey), paramKey);
  paramNotificationFifo.finishedWrite(1);
  triggerAsyncUpdate();
}

void TGraphRuntime::prepareStateForPlayback(
    RenderState &state, double sampleRate, int maximumExpectedSamplesPerBlock) {
  const int blockSize = juce::jmax(1, maximumExpectedSamplesPerBlock);
  const int numChannels = juce::jmax(1, state.totalAllocatedChannels);

  if (state.globalPortBuffer.getNumChannels() != numChannels ||
      state.globalPortBuffer.getNumSamples() != blockSize) {
    state.globalPortBuffer.setSize(numChannels, blockSize, false, false, true);
  }

  state.globalPortBuffer.clear();

  for (auto &entry : state.sortedNodes) {
    if (entry.instance)
      entry.instance->prepareToPlay(sampleRate, blockSize);
  }
}

bool TGraphRuntime::commitPendingStateIfNeeded() noexcept {
  auto nextState = pendingState.take();
  if (!nextState)
    return false;

  activeState.set(nextState.get());
  activeGeneration.store(nextState->generation, std::memory_order_release);
  pendingGeneration.store(0, std::memory_order_release);
  rebuildPending.store(false, std::memory_order_release);
  rebuildCommitCount.fetch_add(1, std::memory_order_relaxed);
  activeNodeCount.store(static_cast<int>(nextState->sortedNodes.size()),
                        std::memory_order_relaxed);
  allocatedPortChannels.store(nextState->totalAllocatedChannels,
                              std::memory_order_relaxed);
  return true;
}

float TGraphRuntime::paramValueToFloat(const juce::var &value) {
  if (value.isBool())
    return static_cast<float>((bool)value ? 1.0 : 0.0);
  if (value.isInt())
    return static_cast<float>((int)value);
  if (value.isInt64())
    return static_cast<float>((juce::int64)value);
  if (value.isDouble())
    return static_cast<float>((double)value);
  if (value.isString())
    return value.toString().getFloatValue();
  return static_cast<float>(value);
}

juce::var TGraphRuntime::coerceValueLike(const juce::var &prototype,
                                         const juce::var &candidate) {
  if (prototype.isBool()) {
    if (candidate.isBool())
      return (bool)candidate;
    return paramValueToFloat(candidate) >= 0.5f;
  }
  if (prototype.isInt())
    return juce::roundToInt(paramValueToFloat(candidate));
  if (prototype.isInt64())
    return static_cast<juce::int64>(juce::roundToInt(paramValueToFloat(candidate)));
  if (prototype.isDouble())
    return static_cast<double>(paramValueToFloat(candidate));
  if (prototype.isString())
    return candidate.toString();
  return candidate;
}

float TGraphRuntime::measureSignalLevel(const float *samples,
                                        int numSamples) noexcept {
  if (samples == nullptr || numSamples <= 0)
    return 0.0f;

  float peak = 0.0f;
  for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    peak = juce::jmax(peak, std::abs(samples[sampleIndex]));

  return juce::jlimit(0.0f, 1.0f, peak);
}

float TGraphRuntime::smoothMeterLevel(float previousLevel,
                                      float measuredLevel) noexcept {
  if (measuredLevel >= previousLevel)
    return measuredLevel;

  return juce::jmax(measuredLevel, previousLevel * 0.84f);
}

void TGraphRuntime::writeParamKey(char *dest,
                                  std::size_t destSize,
                                  const juce::String &paramKey) {
  if (dest == nullptr || destSize == 0)
    return;

  std::memset(dest, 0, destSize);
  paramKey.copyToUTF8(dest, static_cast<int>(destSize));
}

void TGraphRuntime::writeParamKey(std::array<char, 32> &dest,
                                  const juce::String &paramKey) {
  writeParamKey(dest.data(), dest.size(), paramKey);
}

bool TGraphRuntime::dispatchMatches(const ParamDispatch &dispatch,
                                    NodeId nodeId,
                                    const char *paramKey) noexcept {
  if (dispatch.nodeId != nodeId || paramKey == nullptr)
    return false;

  return std::strncmp(dispatch.paramKeyUtf8.data(), paramKey,
                      dispatch.paramKeyUtf8.size()) == 0;
}

std::uint64_t TGraphRuntime::ticksToMicros(juce::int64 tickDelta) noexcept {
  const auto ticksPerSecond = juce::Time::getHighResolutionTicksPerSecond();
  if (tickDelta <= 0 || ticksPerSecond <= 0)
    return 0;

  return static_cast<std::uint64_t>((tickDelta * 1000000) / ticksPerSecond);
}

double TGraphRuntime::microsToMilliseconds(std::uint64_t micros) noexcept {
  return static_cast<double>(micros) / 1000.0;
}

void TGraphRuntime::updateAtomicMax(std::atomic<std::uint64_t> &target,
                                    std::uint64_t candidate) noexcept {
  auto current = target.load(std::memory_order_relaxed);
  while (candidate > current && !target.compare_exchange_weak(
                                  current, candidate,
                                  std::memory_order_relaxed,
                                  std::memory_order_relaxed)) {
  }
}

void TGraphRuntime::updateAtomicMax(std::atomic<int> &target,
                                    int candidate) noexcept {
  auto current = target.load(std::memory_order_relaxed);
  while (candidate > current && !target.compare_exchange_weak(
                                  current, candidate,
                                  std::memory_order_relaxed,
                                  std::memory_order_relaxed)) {
  }
}

} // namespace Teul