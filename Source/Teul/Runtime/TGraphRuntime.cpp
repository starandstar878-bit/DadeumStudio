#include "TGraphRuntime.h"
#include <map>
#include <queue>
#include <cstring>

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

TGraphRuntime::~TGraphRuntime() { releaseResources(); }

bool TGraphRuntime::buildGraph(const TGraphDocument &doc) {
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
    NodeId curr = q.front();
    q.pop();
    sortedIds.push_back(curr);

    for (NodeId neighbor : adj[curr]) {
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

  for (const auto &id : sortedIds) {
    const TNode *n = doc.findNode(id);
    if (!n)
      continue;

    NodeEntry entry;
    entry.nodeId = n->nodeId;
    entry.nodeData = n;

    for (const auto &p : n->ports) {
      entry.portChannels[p.portId] = portChannelCounter;
      globalPortIndexMap[p.portId] = portChannelCounter;
      ++portChannelCounter;
    }

    if (nodeRegistry) {
      if (auto desc = nodeRegistry->descriptorFor(n->typeKey)) {
        if (desc->instanceFactory) {
          entry.instance = desc->instanceFactory();
          if (entry.instance)
            entry.instance->prepareToPlay(currentSampleRate, currentBlockSize);
        }
      }
    }

    newSortedNodes.push_back(std::move(entry));
  }

  auto newState = new RenderState();
  newState->sortedNodes = std::move(newSortedNodes);
  newState->globalPortBuffer.setSize(
      portChannelCounter > 0 ? portChannelCounter : 1, currentBlockSize,
      false, false, true);
  newState->globalPortBuffer.clear();
  newState->totalAllocatedChannels = portChannelCounter;

  activeState.set(newState);

  {
    const juce::ScopedLock lock(paramSurfaceLock);
    rebuildParamSurfaceLocked(doc);
  }

  surfaceChangedPending.store(true, std::memory_order_release);
  triggerAsyncUpdate();
  return true;
}

void TGraphRuntime::prepareToPlay(double sampleRate,
                                  int maximumExpectedSamplesPerBlock) {
  currentSampleRate = sampleRate;
  currentBlockSize = maximumExpectedSamplesPerBlock;

  auto state = activeState.get();
  if (!state)
    return;

  for (auto &entry : state->sortedNodes) {
    if (entry.instance)
      entry.instance->prepareToPlay(sampleRate, maximumExpectedSamplesPerBlock);
  }
}

void TGraphRuntime::releaseResources() {
  auto state = activeState.get();
  if (!state)
    return;

  for (auto &entry : state->sortedNodes) {
    if (entry.instance)
      entry.instance->releaseResources();
  }
}

void TGraphRuntime::processBlock(juce::AudioBuffer<float> &deviceBuffer,
                                 juce::MidiBuffer &midiMessages) {
  auto state = activeState.get();
  if (!state) {
    deviceBuffer.clear();
    return;
  }

  state->globalPortBuffer.clear();
  const int numSamples = deviceBuffer.getNumSamples();

  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramQueueFifo.prepareToRead(paramQueueFifo.getNumReady(), start1, size1,
                               start2, size2);

  auto applyParamChange = [&](int index) {
    const auto &change = paramQueueData[index];
    const juce::String key = juce::String::fromUTF8(change.paramKey);
    for (auto &entry : state->sortedNodes) {
      if (entry.nodeId == change.nodeId && entry.instance) {
        entry.instance->setParameterValue(key, change.value);
        break;
      }
    }

    reportParamValueChange(change.nodeId, key, change.value);
  };

  for (int i = 0; i < size1; ++i)
    applyParamChange(start1 + i);
  for (int i = 0; i < size2; ++i)
    applyParamChange(start2 + i);
  paramQueueFifo.finishedRead(size1 + size2);

  for (auto &entry : state->sortedNodes) {
    for (const auto &mix : entry.preProcessMixes) {
      state->globalPortBuffer.addFrom(mix.dstChannelIndex, 0,
                                      state->globalPortBuffer,
                                      mix.srcChannelIndex, 0, numSamples);
    }

    if (entry.instance && !entry.nodeData->bypassed) {
      TProcessContext ctx;
      ctx.globalPortBuffer = &state->globalPortBuffer;
      ctx.deviceAudioBuffer = &deviceBuffer;
      ctx.midiMessages = &midiMessages;
      ctx.portToChannel = &entry.portChannels;
      ctx.nodeData = entry.nodeData;
      ctx.paramValueReporter = this;
      entry.instance->processSamples(ctx);
    }
  }
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

  buffer.clear();

  juce::MidiBuffer midi;
  processBlock(buffer, midi);
}

void TGraphRuntime::queueParameterChange(NodeId nodeId,
                                         const juce::String &paramKey,
                                         float value) {
  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramQueueFifo.prepareToWrite(1, start1, size1, start2, size2);

  if (size1 > 0) {
    paramQueueData[start1].nodeId = nodeId;
    paramQueueData[start1].value = value;
    std::memset(paramQueueData[start1].paramKey, 0,
                sizeof(paramQueueData[start1].paramKey));
    paramKey.copyToUTF8(paramQueueData[start1].paramKey,
                        sizeof(paramQueueData[start1].paramKey));
  }

  paramQueueFifo.finishedWrite(size1 + size2);
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

bool TGraphRuntime::setParam(const juce::String &paramId, const juce::var &value) {
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

void TGraphRuntime::addListener(Listener *listener) {
  listeners.add(listener);
}

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
  changed.reserve((std::size_t)(size1 + size2));

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
    listeners.call([&](Listener &listener) {
      listener.teulParamValueChanged(param);
    });
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

bool TGraphRuntime::updateParamSurfaceValueLocked(NodeId nodeId,
                                                  const juce::String &paramKey,
                                                  const juce::var &value,
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

void TGraphRuntime::enqueueParamNotification(NodeId nodeId,
                                             const juce::String &paramKey,
                                             float value) {
  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  paramNotificationFifo.prepareToWrite(1, start1, size1, start2, size2);

  if (size1 > 0) {
    auto &slot = paramNotificationData[start1];
    slot.nodeId = nodeId;
    slot.value = value;
    std::memset(slot.paramKey, 0, sizeof(slot.paramKey));
    paramKey.copyToUTF8(slot.paramKey, sizeof(slot.paramKey));
    triggerAsyncUpdate();
  }

  paramNotificationFifo.finishedWrite(size1 + size2);
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

} // namespace Teul