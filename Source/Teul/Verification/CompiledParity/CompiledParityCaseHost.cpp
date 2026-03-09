#include <JuceHeader.h>
#include "Teul/Bridge/ITeulParamProvider.h"
#include "Teul/Export/TExport.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/Verification/TVerificationStimulus.h"

#include "CompiledParityRuntimeWrapper.h"

namespace {

struct CaseManifest {
  juce::String graphId;
  juce::String stimulusId;
  juce::String profileId;
  juce::File exportDirectory;
  juce::File artifactDirectory;
  float maxAbsoluteTolerance = 1.0e-5f;
  double rmsTolerance = 1.0e-6;
  Teul::TVerificationRenderProfile profile;
  Teul::TVerificationStimulusSpec stimulus;
};

juce::String argValue(int argc, char *argv[], const juce::String &prefix) {
  for (int index = 1; index < argc; ++index) {
    const juce::String arg = juce::String::fromUTF8(argv[index]);
    if (arg.startsWith(prefix))
      return arg.fromFirstOccurrenceOf(prefix, false, false).unquoted();
  }
  return {};
}

Teul::NodeId findNodeIdByLabel(const Teul::TGraphDocument &document,
                               const juce::String &nodeLabel) {
  for (const auto &node : document.nodes) {
    if (node.label.equalsIgnoreCase(nodeLabel))
      return node.nodeId;
  }
  return Teul::kInvalidNodeId;
}

float valueForLaneAtSample(const Teul::TVerificationAutomationLane &lane,
                           int sampleIndex,
                           int totalSamples,
                           double sampleRate) {
  if (lane.mode == Teul::TVerificationAutomationMode::Linear) {
    if (totalSamples <= 1)
      return lane.endValue;
    const float progress = juce::jlimit(
        0.0f, 1.0f,
        static_cast<float>(sampleIndex) / static_cast<float>(totalSamples - 1));
    return lane.startValue + ((lane.endValue - lane.startValue) * progress);
  }

  const int stepIntervalSamples = juce::jmax(
      1, juce::roundToInt(juce::jmax(0.001, lane.stepIntervalSeconds) * sampleRate));
  const int segmentIndex = sampleIndex / stepIntervalSamples;
  return (segmentIndex % 2 == 0) ? lane.startValue : lane.endValue;
}

void writeTextArtifact(const juce::File &file, const juce::String &text) {
  juce::ignoreUnused(file.replaceWithText(text, false, false, "\r\n"));
}

bool parseAutomationMode(const juce::String &text,
                         Teul::TVerificationAutomationMode &modeOut) {
  if (text.equalsIgnoreCase("linear")) {
    modeOut = Teul::TVerificationAutomationMode::Linear;
    return true;
  }
  if (text.equalsIgnoreCase("step")) {
    modeOut = Teul::TVerificationAutomationMode::Step;
    return true;
  }
  return false;
}

bool loadManifest(const juce::File &manifestFile,
                  CaseManifest &manifestOut,
                  juce::String *errorMessageOut) {
  juce::var rootVar;
  const auto parseResult =
      juce::JSON::parse(manifestFile.loadFileAsString(), rootVar);
  if (parseResult.failed()) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Compiled parity manifest JSON parse failed: " +
                         parseResult.getErrorMessage();
    return false;
  }

  auto *root = rootVar.getDynamicObject();
  if (root == nullptr) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Compiled parity manifest root is invalid.";
    return false;
  }

  manifestOut.graphId = root->getProperty("graphId").toString();
  manifestOut.stimulusId = root->getProperty("stimulusId").toString();
  manifestOut.profileId = root->getProperty("profileId").toString();
  manifestOut.exportDirectory = juce::File(root->getProperty("exportDirectory").toString());
  manifestOut.artifactDirectory = juce::File(root->getProperty("artifactDirectory").toString());
  manifestOut.maxAbsoluteTolerance = static_cast<float>(root->getProperty("maxAbsoluteTolerance"));
  manifestOut.rmsTolerance = static_cast<double>(root->getProperty("rmsTolerance"));

  auto *profileObject = root->getProperty("profile").getDynamicObject();
  if (profileObject == nullptr) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Compiled parity manifest profile is missing.";
    return false;
  }
  manifestOut.profile.profileId = profileObject->getProperty("profileId").toString();
  manifestOut.profile.sampleRate = static_cast<double>(profileObject->getProperty("sampleRate"));
  manifestOut.profile.blockSize = static_cast<int>(profileObject->getProperty("blockSize"));
  manifestOut.profile.outputChannels = static_cast<int>(profileObject->getProperty("outputChannels"));
  manifestOut.profile.durationSeconds = static_cast<double>(profileObject->getProperty("durationSeconds"));

  auto *stimulusObject = root->getProperty("stimulus").getDynamicObject();
  if (stimulusObject == nullptr) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Compiled parity manifest stimulus is missing.";
    return false;
  }
  manifestOut.stimulus.stimulusId = stimulusObject->getProperty("stimulusId").toString();
  manifestOut.stimulus.displayName = stimulusObject->getProperty("displayName").toString();

  if (const auto *lanes = stimulusObject->getProperty("automationLanes").getArray()) {
    for (const auto &laneVar : *lanes) {
      auto *laneObject = laneVar.getDynamicObject();
      if (laneObject == nullptr)
        continue;
      Teul::TVerificationAutomationLane lane;
      lane.nodeLabel = laneObject->getProperty("nodeLabel").toString();
      lane.paramKey = laneObject->getProperty("paramKey").toString();
      if (!parseAutomationMode(laneObject->getProperty("mode").toString(), lane.mode)) {
        if (errorMessageOut != nullptr)
          *errorMessageOut = "Compiled parity manifest has an invalid automation mode.";
        return false;
      }
      lane.startValue = static_cast<float>(laneObject->getProperty("startValue"));
      lane.endValue = static_cast<float>(laneObject->getProperty("endValue"));
      lane.stepIntervalSeconds = static_cast<double>(laneObject->getProperty("stepIntervalSeconds"));
      manifestOut.stimulus.automationLanes.push_back(std::move(lane));
    }
  }

  if (const auto *events = stimulusObject->getProperty("midiEvents").getArray()) {
    for (const auto &eventVar : *events) {
      auto *eventObject = eventVar.getDynamicObject();
      if (eventObject == nullptr)
        continue;
      const auto *bytes = eventObject->getProperty("bytes").getArray();
      if (bytes == nullptr || bytes->isEmpty()) {
        if (errorMessageOut != nullptr)
          *errorMessageOut = "Compiled parity manifest MIDI event is missing bytes.";
        return false;
      }
      juce::Array<juce::var> byteValues = *bytes;
      juce::MemoryBlock block;
      block.setSize(static_cast<size_t>(byteValues.size()), true);
      auto *dest = static_cast<juce::uint8 *>(block.getData());
      for (int index = 0; index < byteValues.size(); ++index)
        dest[index] = static_cast<juce::uint8>(static_cast<int>(byteValues.getReference(index)) & 0xff);
      Teul::TVerificationMidiEvent event;
      event.sampleOffset = static_cast<int>(eventObject->getProperty("sampleOffset"));
      event.message = juce::MidiMessage(dest, static_cast<int>(block.getSize()), 0.0);
      manifestOut.stimulus.midiEvents.push_back(std::move(event));
    }
  }

  return true;
}

template <typename TRuntimeClass>
bool renderCompiledRuntimeWithStimulus(TRuntimeClass &runtimeModule,
                                       const Teul::TGraphDocument &document,
                                       const Teul::TVerificationRenderProfile &profile,
                                       const Teul::TVerificationStimulusSpec &stimulus,
                                       Teul::TVerificationRenderResult &resultOut,
                                       juce::String *errorMessageOut) {
  if (profile.sampleRate <= 0.0 || profile.blockSize <= 0 ||
      profile.outputChannels <= 0 || profile.durationSeconds <= 0.0) {
    if (errorMessageOut != nullptr)
      *errorMessageOut = "Invalid compiled parity render profile.";
    return false;
  }

  std::vector<std::pair<Teul::NodeId, Teul::TVerificationAutomationLane>> resolvedLanes;
  resolvedLanes.reserve(stimulus.automationLanes.size());
  for (const auto &lane : stimulus.automationLanes) {
    const auto nodeId = findNodeIdByLabel(document, lane.nodeLabel);
    if (nodeId == Teul::kInvalidNodeId) {
      if (errorMessageOut != nullptr)
        *errorMessageOut = "Compiled parity stimulus references a missing node label: " +
                           lane.nodeLabel;
      return false;
    }
    resolvedLanes.push_back({nodeId, lane});
  }

  std::vector<Teul::TVerificationMidiEvent> midiEvents = stimulus.midiEvents;
  std::sort(midiEvents.begin(), midiEvents.end(), [](const auto &a, const auto &b) {
    return a.sampleOffset < b.sampleOffset;
  });

  runtimeModule.reset();
  runtimeModule.prepare(profile.sampleRate, profile.blockSize);

  const int totalSamples = juce::jmax(
      1, juce::roundToInt(profile.durationSeconds * profile.sampleRate));
  resultOut.graphName = document.meta.name;
  resultOut.stimulusId = stimulus.stimulusId;
  resultOut.profileId = profile.profileId;
  resultOut.totalSamples = totalSamples;
  resultOut.audioBuffer.setSize(profile.outputChannels, totalSamples, false, false, true);
  resultOut.audioBuffer.clear();
  resultOut.renderedBlockCount = 0;

  std::size_t midiEventIndex = 0;
  for (int blockStart = 0; blockStart < totalSamples; blockStart += profile.blockSize) {
    const int blockSamples = juce::jmin(profile.blockSize, totalSamples - blockStart);
    for (const auto &resolvedLane : resolvedLanes) {
      const float value = valueForLaneAtSample(resolvedLane.second, blockStart,
                                               totalSamples, profile.sampleRate);
      runtimeModule.setParamById(
          Teul::makeTeulParamId(resolvedLane.first, resolvedLane.second.paramKey),
          value);
    }

    juce::AudioBuffer<float> blockBuffer(profile.outputChannels, blockSamples);
    juce::MidiBuffer midiBuffer;
    while (midiEventIndex < midiEvents.size() &&
           midiEvents[midiEventIndex].sampleOffset < (blockStart + blockSamples)) {
      const auto &event = midiEvents[midiEventIndex];
      if (event.sampleOffset >= blockStart)
        midiBuffer.addEvent(event.message, event.sampleOffset - blockStart);
      ++midiEventIndex;
    }

    runtimeModule.process(blockBuffer, midiBuffer);
    for (int channel = 0; channel < profile.outputChannels; ++channel) {
      resultOut.audioBuffer.copyFrom(channel, blockStart, blockBuffer, channel, 0,
                                     blockSamples);
    }
    ++resultOut.renderedBlockCount;
  }

  return true;
}

struct ComparisonReport {
  bool passed = false;
  int totalSamples = 0;
  int comparedChannels = 0;
  int firstMismatchChannel = -1;
  int firstMismatchSample = -1;
  float maxAbsoluteError = 0.0f;
  double rmsError = 0.0;
  juce::String failureReason;
};

ComparisonReport compareBuffers(const Teul::TVerificationRenderResult &runtimeRender,
                                const Teul::TVerificationRenderResult &compiledRender,
                                float maxAbsoluteTolerance,
                                double rmsTolerance) {
  ComparisonReport report;
  report.totalSamples = juce::jmin(runtimeRender.audioBuffer.getNumSamples(),
                                   compiledRender.audioBuffer.getNumSamples());
  report.comparedChannels = juce::jmin(runtimeRender.audioBuffer.getNumChannels(),
                                       compiledRender.audioBuffer.getNumChannels());
  if (report.totalSamples <= 0 || report.comparedChannels <= 0) {
    report.failureReason = "Compiled parity render output is empty.";
    return report;
  }

  if (runtimeRender.audioBuffer.getNumChannels() != compiledRender.audioBuffer.getNumChannels() ||
      runtimeRender.audioBuffer.getNumSamples() != compiledRender.audioBuffer.getNumSamples()) {
    report.failureReason = "Runtime and compiled output dimensions differ.";
    return report;
  }

  double squaredErrorSum = 0.0;
  double sampleCount = 0.0;
  for (int channel = 0; channel < report.comparedChannels; ++channel) {
    const float *lhs = runtimeRender.audioBuffer.getReadPointer(channel);
    const float *rhs = compiledRender.audioBuffer.getReadPointer(channel);
    for (int sample = 0; sample < report.totalSamples; ++sample) {
      const float left = std::isfinite(lhs[sample]) && std::abs(lhs[sample]) < 1.0e-20f
                             ? 0.0f
                             : lhs[sample];
      const float right = std::isfinite(rhs[sample]) && std::abs(rhs[sample]) < 1.0e-20f
                              ? 0.0f
                              : rhs[sample];
      if (!std::isfinite(left) || !std::isfinite(right)) {
        report.failureReason = "Compiled parity produced NaN or Inf.";
        report.firstMismatchChannel = channel;
        report.firstMismatchSample = sample;
        return report;
      }
      const float error = std::abs(left - right);
      report.maxAbsoluteError = juce::jmax(report.maxAbsoluteError, error);
      if (error > maxAbsoluteTolerance && report.firstMismatchChannel < 0) {
        report.firstMismatchChannel = channel;
        report.firstMismatchSample = sample;
      }
      squaredErrorSum += static_cast<double>(error) * static_cast<double>(error);
      sampleCount += 1.0;
    }
  }

  report.rmsError = sampleCount > 0.0 ? std::sqrt(squaredErrorSum / sampleCount) : 0.0;
  report.passed = report.maxAbsoluteError <= maxAbsoluteTolerance &&
                  report.rmsError <= rmsTolerance;
  if (!report.passed && report.failureReason.isEmpty())
    report.failureReason = "Compiled parity exceeded tolerance.";
  return report;
}

juce::String buildSummaryText(const CaseManifest &manifest,
                              const ComparisonReport &comparison) {
  juce::String summary;
  summary << "graphId=" << manifest.graphId << "\r\n";
  summary << "stimulusId=" << manifest.stimulusId << "\r\n";
  summary << "profileId=" << manifest.profileId << "\r\n";
  summary << "passed=" << (comparison.passed ? "true" : "false") << "\r\n";
  summary << "exportDirectory=" << manifest.exportDirectory.getFullPathName() << "\r\n";
  summary << "artifactDirectory=" << manifest.artifactDirectory.getFullPathName() << "\r\n";
  summary << "totalSamples=" << comparison.totalSamples << "\r\n";
  summary << "comparedChannels=" << comparison.comparedChannels << "\r\n";
  summary << "maxAbsoluteError=" << juce::String(comparison.maxAbsoluteError, 9) << "\r\n";
  summary << "rmsError=" << juce::String(comparison.rmsError, 12) << "\r\n";
  summary << "firstMismatchChannel=" << comparison.firstMismatchChannel << "\r\n";
  summary << "firstMismatchSample=" << comparison.firstMismatchSample << "\r\n";
  if (comparison.failureReason.isNotEmpty())
    summary << "failureReason=" << comparison.failureReason << "\r\n";
  return summary;
}

} // namespace

int main(int argc, char *argv[]) {
  juce::ScopedJuceInitialiser_GUI libraryInitialiser;

  const auto manifestArg = argValue(argc, argv, "--manifest=");
  if (manifestArg.isEmpty()) {
    std::cerr << "Missing --manifest=... argument." << std::endl;
    return 1;
  }

  CaseManifest manifest;
  juce::String errorMessage;
  if (!loadManifest(juce::File(manifestArg), manifest, &errorMessage)) {
    std::cerr << errorMessage << std::endl;
    return 1;
  }

  juce::ignoreUnused(manifest.artifactDirectory.createDirectory());

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry) {
    errorMessage = "Failed to create Teul node registry in compiled parity host.";
    std::cerr << errorMessage << std::endl;
    return 1;
  }

  Teul::TGraphDocument importedDocument;
  const auto importResult =
      Teul::TExporter::importEditableGraphPackage(manifest.exportDirectory,
                                                  importedDocument);
  if (importResult.failed()) {
    errorMessage = importResult.getErrorMessage();
    writeTextArtifact(manifest.artifactDirectory.getChildFile("compiled-parity-failure.txt"),
                      errorMessage + "\r\n");
    std::cerr << errorMessage << std::endl;
    return 1;
  }

  Teul::TVerificationRenderResult runtimeRender;
  if (!Teul::renderGraphWithStimulus(*registry, importedDocument, manifest.profile,
                                     manifest.stimulus, runtimeRender,
                                     &errorMessage)) {
    writeTextArtifact(manifest.artifactDirectory.getChildFile("compiled-parity-failure.txt"),
                      errorMessage + "\r\n");
    std::cerr << errorMessage << std::endl;
    return 1;
  }

  TeulCompiledParityRuntimeClass compiledRuntime;
  Teul::TVerificationRenderResult compiledRender;
  if (!renderCompiledRuntimeWithStimulus(compiledRuntime, importedDocument,
                                         manifest.profile, manifest.stimulus,
                                         compiledRender, &errorMessage)) {
    writeTextArtifact(manifest.artifactDirectory.getChildFile("compiled-parity-failure.txt"),
                      errorMessage + "\r\n");
    std::cerr << errorMessage << std::endl;
    return 1;
  }

  const auto comparison = compareBuffers(runtimeRender, compiledRender,
                                         manifest.maxAbsoluteTolerance,
                                         manifest.rmsTolerance);
  const auto summaryText = buildSummaryText(manifest, comparison);
  writeTextArtifact(manifest.artifactDirectory.getChildFile("compiled-parity-summary.txt"),
                    summaryText);
  if (!comparison.passed) {
    writeTextArtifact(manifest.artifactDirectory.getChildFile("compiled-parity-failure.txt"),
                      comparison.failureReason + "\r\n");
  }

  std::cout << summaryText << std::endl;
  return comparison.passed ? 0 : 1;
}