#include "Teul/Verification/TVerificationFixtures.h"
namespace Teul {
namespace {
int inferPortChannelIndex(juce::StringRef portName) {
  const auto lowered = juce::String(portName).trim().toLowerCase();
  if (lowered == "r" || lowered == "r in" || lowered == "r out" ||
      lowered.startsWith("r ") || lowered.startsWith("right")) {
    return 1;
  }
  return 0;
}
TGraphDocument makeBaseDocument(const juce::String &name) {
  TGraphDocument document;
  document.meta.name = name;
  document.meta.sampleRate = 48000.0;
  document.meta.blockSize = 128;
  return document;
}
const TNodeDescriptor *requireDescriptor(const TNodeRegistry &registry,
                                         const juce::String &typeKey) {
  return registry.descriptorFor(typeKey);
}
TNode makeNodeFromDescriptor(const TNodeDescriptor &descriptor,
                             TGraphDocument &document,
                             float x,
                             float y,
                             const juce::String &label = {}) {
  TNode node;
  node.nodeId = document.allocNodeId();
  node.typeKey = descriptor.typeKey;
  node.x = x;
  node.y = y;
  node.label = label;
  for (const auto &spec : descriptor.paramSpecs)
    node.params[spec.key] = spec.defaultValue;
  for (const auto &portSpec : descriptor.portSpecs) {
    TPort port;
    port.portId = document.allocPortId();
    port.ownerNodeId = node.nodeId;
    port.direction = portSpec.direction;
    port.dataType = portSpec.dataType;
    port.name = portSpec.name;
    port.channelIndex = inferPortChannelIndex(portSpec.name);
    port.maxIncomingConnections = portSpec.maxIncomingConnections;
    port.maxOutgoingConnections = portSpec.maxOutgoingConnections;
    node.ports.push_back(port);
  }
  return node;
}
const TPort *findPortByName(const TNode &node, juce::StringRef portName) {
  for (const auto &port : node.ports) {
    if (port.name.equalsIgnoreCase(juce::String(portName)))
      return &port;
  }
  return nullptr;
}
bool addConnection(TGraphDocument &document,
                   NodeId fromNodeId,
                   juce::StringRef fromPortName,
                   NodeId toNodeId,
                   juce::StringRef toPortName) {
  const auto *fromNode = document.findNode(fromNodeId);
  const auto *toNode = document.findNode(toNodeId);
  if (fromNode == nullptr || toNode == nullptr)
    return false;
  const auto *fromPort = findPortByName(*fromNode, fromPortName);
  const auto *toPort = findPortByName(*toNode, toPortName);
  if (fromPort == nullptr || toPort == nullptr)
    return false;
  TConnection connection;
  connection.connectionId = document.allocConnectionId();
  connection.from.nodeId = fromNodeId;
  connection.from.portId = fromPort->portId;
  connection.to.nodeId = toNodeId;
  connection.to.portId = toPort->portId;
  document.connections.push_back(connection);
  return true;
}
bool appendStereoOutConnections(TGraphDocument &document,
                                NodeId sourceNodeId,
                                juce::StringRef leftPortName,
                                juce::StringRef rightPortName,
                                NodeId outputNodeId) {
  const bool leftOk = addConnection(document, sourceNodeId, leftPortName,
                                    outputNodeId, "L In");
  const bool rightOk = addConnection(document, sourceNodeId, rightPortName,
                                     outputNodeId, "R In");
  return leftOk && rightOk;
}
} // namespace
TGraphDocument makeVerificationGraphG1TonePath(const TNodeRegistry &registry) {
  TGraphDocument document = makeBaseDocument("G1 Tone Path");
  const auto *oscDesc = requireDescriptor(registry, "Teul.Source.Oscillator");
  const auto *vcaDesc = requireDescriptor(registry, "Teul.Mixer.VCA");
  const auto *outDesc = requireDescriptor(registry, "Teul.Routing.AudioOut");
  if (oscDesc == nullptr || vcaDesc == nullptr || outDesc == nullptr)
    return document;
  auto osc = makeNodeFromDescriptor(*oscDesc, document, 80.0f, 120.0f, "Tone");
  osc.params["waveform"] = 0;
  osc.params["frequency"] = 220.0f;
  osc.params["gain"] = 0.6f;
  auto vca = makeNodeFromDescriptor(*vcaDesc, document, 320.0f, 120.0f, "Amp");
  vca.params["gain"] = 0.75f;
  auto out = makeNodeFromDescriptor(*outDesc, document, 560.0f, 120.0f, "Main Out");
  out.params["volume"] = 0.9f;
  const auto oscId = osc.nodeId;
  const auto vcaId = vca.nodeId;
  const auto outId = out.nodeId;
  document.nodes.push_back(std::move(osc));
  document.nodes.push_back(std::move(vca));
  document.nodes.push_back(std::move(out));
  addConnection(document, oscId, "Out", vcaId, "In");
  addConnection(document, vcaId, "Out", outId, "L In");
  addConnection(document, vcaId, "Out", outId, "R In");
  return document;
}
TGraphDocument makeVerificationGraphG2FilterSweep(const TNodeRegistry &registry) {
  TGraphDocument document = makeBaseDocument("G2 Filter Sweep");
  const auto *oscDesc = requireDescriptor(registry, "Teul.Source.Oscillator");
  const auto *filterDesc = requireDescriptor(registry, "Teul.Filter.LowPass");
  const auto *outDesc = requireDescriptor(registry, "Teul.Routing.AudioOut");
  if (oscDesc == nullptr || filterDesc == nullptr || outDesc == nullptr)
    return document;
  auto osc = makeNodeFromDescriptor(*oscDesc, document, 80.0f, 120.0f, "Source");
  osc.params["waveform"] = 3;
  osc.params["frequency"] = 330.0f;
  osc.params["gain"] = 0.5f;
  auto filter = makeNodeFromDescriptor(*filterDesc, document, 320.0f, 120.0f,
                                       "LowPass");
  filter.params["cutoff"] = 1400.0f;
  filter.params["resonance"] = 0.9f;
  auto out = makeNodeFromDescriptor(*outDesc, document, 580.0f, 120.0f, "Main Out");
  out.params["volume"] = 0.9f;
  const auto oscId = osc.nodeId;
  const auto filterId = filter.nodeId;
  const auto outId = out.nodeId;
  document.nodes.push_back(std::move(osc));
  document.nodes.push_back(std::move(filter));
  document.nodes.push_back(std::move(out));
  addConnection(document, oscId, "Out", filterId, "In");
  addConnection(document, filterId, "Out", outId, "L In");
  addConnection(document, filterId, "Out", outId, "R In");
  return document;
}
TGraphDocument makeVerificationGraphG3StereoMotion(const TNodeRegistry &registry) {
  TGraphDocument document = makeBaseDocument("G3 Stereo Motion");
  const auto *oscDesc = requireDescriptor(registry, "Teul.Source.Oscillator");
  const auto *panDesc = requireDescriptor(registry, "Teul.Mixer.StereoPanner");
  const auto *outDesc = requireDescriptor(registry, "Teul.Routing.AudioOut");
  if (oscDesc == nullptr || panDesc == nullptr || outDesc == nullptr)
    return document;
  auto osc = makeNodeFromDescriptor(*oscDesc, document, 80.0f, 120.0f, "Source");
  osc.params["waveform"] = 1;
  osc.params["frequency"] = 220.0f;
  osc.params["gain"] = 0.55f;
  auto pan = makeNodeFromDescriptor(*panDesc, document, 320.0f, 120.0f,
                                    "Stereo Pan");
  pan.params["pan"] = -0.35f;
  auto out = makeNodeFromDescriptor(*outDesc, document, 590.0f, 120.0f, "Main Out");
  out.params["volume"] = 0.9f;
  const auto oscId = osc.nodeId;
  const auto panId = pan.nodeId;
  const auto outId = out.nodeId;
  document.nodes.push_back(std::move(osc));
  document.nodes.push_back(std::move(pan));
  document.nodes.push_back(std::move(out));
  addConnection(document, oscId, "Out", panId, "In");
  appendStereoOutConnections(document, panId, "L Out", "R Out", outId);
  return document;
}
TGraphDocument makeVerificationGraphG4MidiVoice(const TNodeRegistry &registry) {
  TGraphDocument document = makeBaseDocument("G4 MIDI Voice");
  const auto *midiInDesc = requireDescriptor(registry, "Teul.Source.MidiInput");
  const auto *midiToCvDesc = requireDescriptor(registry, "Teul.Midi.MidiToCV");
  const auto *adsrDesc = requireDescriptor(registry, "Teul.Mod.ADSR");
  const auto *oscDesc = requireDescriptor(registry, "Teul.Source.Oscillator");
  const auto *vcaDesc = requireDescriptor(registry, "Teul.Mixer.VCA");
  const auto *outDesc = requireDescriptor(registry, "Teul.Routing.AudioOut");
  if (midiInDesc == nullptr || midiToCvDesc == nullptr || adsrDesc == nullptr ||
      oscDesc == nullptr || vcaDesc == nullptr || outDesc == nullptr) {
    return document;
  }
  auto midiIn = makeNodeFromDescriptor(*midiInDesc, document, 60.0f, 80.0f,
                                       "MIDI In");
  auto midiToCv = makeNodeFromDescriptor(*midiToCvDesc, document, 270.0f, 80.0f,
                                         "Pitch CV");
  auto adsr = makeNodeFromDescriptor(*adsrDesc, document, 500.0f, 180.0f,
                                     "Env");
  adsr.params["attack"] = 12.0f;
  adsr.params["decay"] = 120.0f;
  adsr.params["sustain"] = 0.6f;
  adsr.params["release"] = 240.0f;
  auto osc = makeNodeFromDescriptor(*oscDesc, document, 500.0f, 40.0f, "Voice");
  osc.params["waveform"] = 2;
  osc.params["frequency"] = 16.3516f;
  osc.params["gain"] = 0.45f;
  auto vca = makeNodeFromDescriptor(*vcaDesc, document, 730.0f, 100.0f, "Amp");
  vca.params["gain"] = 0.9f;
  auto out = makeNodeFromDescriptor(*outDesc, document, 960.0f, 100.0f, "Main Out");
  out.params["volume"] = 0.9f;
  const auto midiInId = midiIn.nodeId;
  const auto midiToCvId = midiToCv.nodeId;
  const auto adsrId = adsr.nodeId;
  const auto oscId = osc.nodeId;
  const auto vcaId = vca.nodeId;
  const auto outId = out.nodeId;
  document.nodes.push_back(std::move(midiIn));
  document.nodes.push_back(std::move(midiToCv));
  document.nodes.push_back(std::move(adsr));
  document.nodes.push_back(std::move(osc));
  document.nodes.push_back(std::move(vca));
  document.nodes.push_back(std::move(out));
  addConnection(document, midiInId, "MIDI Out", midiToCvId, "MIDI In");
  addConnection(document, midiToCvId, "V/Oct", oscId, "V/Oct");
  addConnection(document, midiToCvId, "Gate", adsrId, "Gate");
  addConnection(document, oscId, "Out", vcaId, "In");
  addConnection(document, adsrId, "Env", vcaId, "CV");
  addConnection(document, vcaId, "Out", outId, "L In");
  addConnection(document, vcaId, "Out", outId, "R In");
  return document;
}
TGraphDocument makeVerificationGraphG5TimeTail(const TNodeRegistry &registry) {
  TGraphDocument document = makeBaseDocument("G5 Time Tail");
  const auto *oscDesc = requireDescriptor(registry, "Teul.Source.Oscillator");
  const auto *delayDesc = requireDescriptor(registry, "Teul.FX.Delay");
  const auto *outDesc = requireDescriptor(registry, "Teul.Routing.AudioOut");
  if (oscDesc == nullptr || delayDesc == nullptr || outDesc == nullptr)
    return document;
  auto osc = makeNodeFromDescriptor(*oscDesc, document, 80.0f, 120.0f, "Source");
  osc.params["waveform"] = 0;
  osc.params["frequency"] = 247.0f;
  osc.params["gain"] = 0.5f;
  auto delay = makeNodeFromDescriptor(*delayDesc, document, 330.0f, 120.0f,
                                      "Delay");
  delay.params["time"] = 180.0f;
  delay.params["feedback"] = 0.45f;
  delay.params["mix"] = 0.35f;
  auto out = makeNodeFromDescriptor(*outDesc, document, 590.0f, 120.0f, "Main Out");
  out.params["volume"] = 0.9f;
  const auto oscId = osc.nodeId;
  const auto delayId = delay.nodeId;
  const auto outId = out.nodeId;
  document.nodes.push_back(std::move(osc));
  document.nodes.push_back(std::move(delay));
  document.nodes.push_back(std::move(out));
  addConnection(document, oscId, "Out", delayId, "In");
  addConnection(document, delayId, "Out", outId, "L In");
  addConnection(document, delayId, "Out", outId, "R In");
  return document;
}
std::vector<TVerificationGraphFixture>
makeRepresentativeVerificationGraphSet(const TNodeRegistry &registry) {
  std::vector<TVerificationGraphFixture> fixtures;
  fixtures.push_back({"G1", "Tone Path", makeVerificationGraphG1TonePath(registry)});
  fixtures.push_back({"G2", "Filter Sweep", makeVerificationGraphG2FilterSweep(registry)});
  fixtures.push_back({"G3", "Stereo Motion", makeVerificationGraphG3StereoMotion(registry)});
  fixtures.push_back({"G4", "MIDI Voice", makeVerificationGraphG4MidiVoice(registry)});
  fixtures.push_back({"G5", "Time Tail", makeVerificationGraphG5TimeTail(registry)});
  return fixtures;
}
} // namespace Teul