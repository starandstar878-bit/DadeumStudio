#include "TNodeRegistry.h"

namespace Teul {

void TNodeRegistry::registerNode(const TNodeDescriptor &desc) {
  // 이미 존재하는 타입 키면 무시하거나 덮어쓰기 로직 필요
  for (auto &existing : descriptors) {
    if (existing.typeKey == desc.typeKey) {
      existing = desc;
      return;
    }
  }
  descriptors.push_back(desc);
}

const TNodeDescriptor *
TNodeRegistry::descriptorFor(const juce::String &typeKey) const {
  for (const auto &desc : descriptors) {
    if (desc.typeKey == typeKey)
      return &desc;
  }
  return nullptr;
}

const std::vector<TNodeDescriptor> &TNodeRegistry::getAllDescriptors() const {
  return descriptors;
}

std::unique_ptr<TNodeRegistry> makeDefaultNodeRegistry() {
  auto registry = std::make_unique<TNodeRegistry>();

  auto reg =
      [&registry](const juce::String &typeKey, const juce::String &displayName,
                  const juce::String &category, std::vector<TParamSpec> params,
                  std::vector<TPortSpec> ports) {
        TNodeDescriptor desc;
        desc.typeKey = typeKey;
        desc.displayName = displayName;
        desc.category = category;
        desc.paramSpecs = std::move(params);
        desc.portSpecs = std::move(ports);
        registry->registerNode(desc);
      };

  // ---------------------------------------------------------------------------
  // 🔊 Source — 신호 생성
  // ---------------------------------------------------------------------------
  reg("Teul.Source.Oscillator", "Oscillator", "Source",
      {{"waveform", "Waveform", 0}, // 0:Sine, 1:Triangle, 2:Square, 3:Saw
       {"frequency", "Frequency", 440.0f},
       {"gain", "Gain", 0.707f}},
      {{TPortDirection::Input, TPortDataType::CV, "V/Oct"},
       {TPortDirection::Input, TPortDataType::CV, "Sync"},
       {TPortDirection::Output, TPortDataType::Audio, "Out"}});

  reg("Teul.Source.LFO", "LFO", "Source",
      {{"waveform", "Waveform", 0}, {"rate", "Rate", 1.0f}},
      {{TPortDirection::Output, TPortDataType::CV, "Out"}});

  reg("Teul.Source.NoiseGenerator", "Noise", "Source",
      {{"type", "Color", 0}}, // 0:White, 1:Pink, 2:Brown
      {{TPortDirection::Output, TPortDataType::Audio, "Out"}});

  reg("Teul.Source.MidiInput", "MIDI Input", "Source", {},
      {{TPortDirection::Output, TPortDataType::MIDI, "MIDI Out"}});

  reg("Teul.Source.Constant", "Constant", "Source", {{"value", "Value", 1.0f}},
      {{TPortDirection::Output, TPortDataType::CV, "Value"}});

  // ---------------------------------------------------------------------------
  // 🔬 Filter / EQ — 필터링
  // ---------------------------------------------------------------------------
  reg("Teul.Filter.LowPass", "LowPass Filter", "Filter",
      {{"cutoff", "Cutoff", 1000.0f}, {"resonance", "Resonance", 0.707f}},
      {{TPortDirection::Input, TPortDataType::Audio, "In"},
       {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
       {TPortDirection::Output, TPortDataType::Audio, "Out"}});

  reg("Teul.Filter.HighPass", "HighPass Filter", "Filter",
      {{"cutoff", "Cutoff", 1000.0f}, {"resonance", "Resonance", 0.707f}},
      {{TPortDirection::Input, TPortDataType::Audio, "In"},
       {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
       {TPortDirection::Output, TPortDataType::Audio, "Out"}});

  reg("Teul.Filter.BandPass", "BandPass Filter", "Filter",
      {{"cutoff", "Cutoff", 1000.0f}, {"q", "Q Factor", 1.0f}},
      {{TPortDirection::Input, TPortDataType::Audio, "In"},
       {TPortDirection::Input, TPortDataType::CV, "Freq CV"},
       {TPortDirection::Output, TPortDataType::Audio, "Out"}});

  // ---------------------------------------------------------------------------
  // 🌊 Modulation — 변조
  // ---------------------------------------------------------------------------
  reg("Teul.Mod.ADSR", "ADSR Envelope", "Modulation",
      {{"attack", "Attack", 10.0f},
       {"decay", "Decay", 100.0f},
       {"sustain", "Sustain", 0.5f},
       {"release", "Release", 300.0f}},
      {{TPortDirection::Input, TPortDataType::Gate, "Gate"},
       {TPortDirection::Output, TPortDataType::CV, "Env"}});

  // ---------------------------------------------------------------------------
  // 🏛️ Delay / Reverb — 공간·시간
  // ---------------------------------------------------------------------------
  reg("Teul.FX.Delay", "Delay", "FX",
      {{"time", "Time (ms)", 250.0f},
       {"feedback", "Feedback", 0.3f},
       {"mix", "Mix", 0.5f}},
      {{TPortDirection::Input, TPortDataType::Audio, "In"},
       {TPortDirection::Output, TPortDataType::Audio, "Out"}});

  reg("Teul.FX.Reverb", "Reverb", "FX",
      {{"roomSize", "Room Size", 0.5f},
       {"damping", "Damping", 0.5f},
       {"width", "Width", 1.0f},
       {"mix", "Mix", 0.3f}},
      {{TPortDirection::Input, TPortDataType::Audio, "L In"},
       {TPortDirection::Input, TPortDataType::Audio, "R In"},
       {TPortDirection::Output, TPortDataType::Audio, "L Out"},
       {TPortDirection::Output, TPortDataType::Audio, "R Out"}});

  // ---------------------------------------------------------------------------
  // 🎚️ Mixer / Routing — 믹싱·라우팅
  // ---------------------------------------------------------------------------
  reg("Teul.Mixer.VCA", "VCA", "Mixer", {{"gain", "Gain", 1.0f}},
      {{TPortDirection::Input, TPortDataType::Audio, "In"},
       {TPortDirection::Input, TPortDataType::CV, "CV"},
       {TPortDirection::Output, TPortDataType::Audio, "Out"}});

  reg("Teul.Mixer.StereoPanner", "Stereo Panner", "Mixer",
      {{"pan", "Pan", 0.0f}},
      {{TPortDirection::Input, TPortDataType::Audio, "In"},
       {TPortDirection::Input, TPortDataType::CV, "Pan CV"},
       {TPortDirection::Output, TPortDataType::Audio, "L Out"},
       {TPortDirection::Output, TPortDataType::Audio, "R Out"}});

  reg("Teul.Mixer.Mixer2", "Mixer (2-Ch)", "Mixer",
      {{"gain1", "Gain 1", 1.0f}, {"gain2", "Gain 2", 1.0f}},
      {{TPortDirection::Input, TPortDataType::Audio, "In 1"},
       {TPortDirection::Input, TPortDataType::Audio, "In 2"},
       {TPortDirection::Output, TPortDataType::Audio, "Out"}});

  reg("Teul.Routing.AudioOut", "Audio Output", "Routing",
      {{"volume", "Volume", 1.0f}},
      {{TPortDirection::Input, TPortDataType::Audio, "L In"},
       {TPortDirection::Input, TPortDataType::Audio, "R In"}});

  // ---------------------------------------------------------------------------
  // 🔢 Math / Logic — 수학·논리
  // ---------------------------------------------------------------------------
  reg("Teul.Math.Add", "Add", "Math", {},
      {{TPortDirection::Input, TPortDataType::CV, "A"},
       {TPortDirection::Input, TPortDataType::CV, "B"},
       {TPortDirection::Output, TPortDataType::CV, "A+B"}});

  reg("Teul.Math.Multiply", "Multiply", "Math", {},
      {{TPortDirection::Input, TPortDataType::CV, "A"},
       {TPortDirection::Input, TPortDataType::CV, "B"},
       {TPortDirection::Output, TPortDataType::CV, "A*B"}});

  reg("Teul.Math.Subtract", "Subtract", "Math", {},
      {{TPortDirection::Input, TPortDataType::CV, "A"},
       {TPortDirection::Input, TPortDataType::CV, "B"},
       {TPortDirection::Output, TPortDataType::CV, "A-B"}});

  reg("Teul.Math.Divide", "Divide", "Math", {},
      {{TPortDirection::Input, TPortDataType::CV, "A"},
       {TPortDirection::Input, TPortDataType::CV, "B"},
       {TPortDirection::Output, TPortDataType::CV, "A/B"}});

  reg("Teul.Math.Clamp", "Clamp", "Math",
      {{"min", "Min", 0.0f}, {"max", "Max", 1.0f}},
      {{TPortDirection::Input, TPortDataType::CV, "In"},
       {TPortDirection::Output, TPortDataType::CV, "Out"}});

  reg("Teul.Math.ValueMap", "Value Map", "Math",
      {{"inMin", "In Min", 0.0f},
       {"inMax", "In Max", 1.0f},
       {"outMin", "Out Min", -1.0f},
       {"outMax", "Out Max", 1.0f}},
      {{TPortDirection::Input, TPortDataType::CV, "In"},
       {TPortDirection::Output, TPortDataType::CV, "Out"}});

  // ---------------------------------------------------------------------------
  // 🎹 MIDI Processing — MIDI 처리
  // ---------------------------------------------------------------------------
  reg("Teul.Midi.MidiToCV", "MIDI to CV", "MIDI", {},
      {{TPortDirection::Input, TPortDataType::MIDI, "MIDI In"},
       {TPortDirection::Output, TPortDataType::CV, "V/Oct"},
       {TPortDirection::Output, TPortDataType::Gate, "Gate"},
       {TPortDirection::Output, TPortDataType::CV, "Velocity"}});

  reg("Teul.Midi.MidiOut", "MIDI Output", "MIDI", {},
      {{TPortDirection::Input, TPortDataType::MIDI, "MIDI In"}});

  return registry;
}

} // namespace Teul
