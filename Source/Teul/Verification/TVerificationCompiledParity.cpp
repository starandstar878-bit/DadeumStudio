#include "Teul/Verification/TVerificationCompiledParity.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Teul {
namespace {

struct CompiledParityCaseSpec {
  const TVerificationGraphFixture *fixture = nullptr;
  TVerificationRenderProfile profile;
  TVerificationStimulusSpec stimulus;
  juce::String caseId;
  juce::String runtimeClassName;
  juce::File exportDirectory;
  juce::File artifactDirectory;
  juce::File manifestFile;
  juce::File hostObjectFile;
  juce::File generatedObjectFile;
  juce::File executableFile;
  juce::File compileOutputFile;
  juce::File linkOutputFile;
  juce::File runOutputFile;
  juce::File linkResponseFile;
  TExportReport exportReport;
};

juce::String sanitizePathFragment(const juce::String &text) {
  juce::String cleaned;
  for (const auto ch : text) {
    if (juce::CharacterFunctions::isLetterOrDigit(ch))
      cleaned << ch;
    else
      cleaned << '_';
  }
  return cleaned.trimCharactersAtEnd("_").trimCharactersAtStart("_");
}

juce::String makeIdentifierFragment(const juce::String &text) {
  juce::String result;
  bool uppercaseNext = true;
  for (const auto ch : text) {
    if (juce::CharacterFunctions::isLetterOrDigit(ch)) {
      result << (uppercaseNext ? juce::CharacterFunctions::toUpperCase(ch) : ch);
      uppercaseNext = false;
    } else {
      uppercaseNext = true;
    }
  }
  if (result.isEmpty())
    result = "Case";
  if (juce::CharacterFunctions::isDigit(result[0]))
    result = "Case" + result;
  return result;
}

void writeTextArtifact(const juce::File &file, const juce::String &text) {
  juce::ignoreUnused(file.replaceWithText(text, false, false, "\r\n"));
}

bool writeJsonArtifact(const juce::File &file, const juce::var &json) {
  return file.replaceWithText(juce::JSON::toString(json, true), false, false,
                              "\r\n");
}

juce::String relativeArtifactPath(const juce::File &root, const juce::File &file) {
  const auto path = file.getFullPathName();
  if (path.isEmpty())
    return {};
  return file.getRelativePathFrom(root).replaceCharacter('\\', '/');
}

juce::var makeArtifactFileEntry(const juce::String &role,
                                const juce::File &root,
                                const juce::File &file) {
  auto *entry = new juce::DynamicObject();
  entry->setProperty("role", role);
  entry->setProperty("relativePath", relativeArtifactPath(root, file));
  entry->setProperty("exists", file.exists());
  return juce::var(entry);
}

bool parseBoolText(const juce::String &text) {
  const auto trimmed = text.trim().toLowerCase();
  return trimmed == "1" || trimmed == "true" || trimmed == "yes" || trimmed == "on";
}

std::map<juce::String, juce::String> parseSummaryText(const juce::String &text) {
  std::map<juce::String, juce::String> values;
  juce::StringArray lines;
  lines.addLines(text);
  for (const auto &line : lines) {
    const auto trimmed = line.trim();
    if (trimmed.isEmpty())
      continue;
    const auto separator = trimmed.indexOfChar('=');
    if (separator <= 0)
      continue;
    const auto key = trimmed.substring(0, separator).trim();
    const auto value = trimmed.substring(separator + 1).trim();
    values[key] = value;
  }
  return values;
}

juce::String resolveVcVars64Path() {
  for (const auto &candidate : {
           juce::String(
               "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"),
           juce::String(
               "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat")}) {
    if (juce::File(candidate).existsAsFile())
      return candidate;
  }
  return {};
}

juce::StringArray teulCompileDefinitions() {
  juce::StringArray definitions;
  definitions.addArray({
      "_CRT_SECURE_NO_WARNINGS",
      "WIN32",
      "_WINDOWS",
      "DEBUG",
      "_DEBUG",
      "JUCE_PROJUCER_VERSION=0x8000c",
      "JUCE_MODULE_AVAILABLE_juce_audio_basics=1",
      "JUCE_MODULE_AVAILABLE_juce_audio_devices=1",
      "JUCE_MODULE_AVAILABLE_juce_audio_formats=1",
      "JUCE_MODULE_AVAILABLE_juce_audio_plugin_client=1",
      "JUCE_MODULE_AVAILABLE_juce_audio_processors=1",
      "JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1",
      "JUCE_MODULE_AVAILABLE_juce_core=1",
      "JUCE_MODULE_AVAILABLE_juce_data_structures=1",
      "JUCE_MODULE_AVAILABLE_juce_events=1",
      "JUCE_MODULE_AVAILABLE_juce_graphics=1",
      "JUCE_MODULE_AVAILABLE_juce_gui_basics=1",
      "JUCE_MODULE_AVAILABLE_juce_gui_extra=1",
      "JUCE_MODULE_AVAILABLE_juce_video=1",
      "JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1",
      "JUCE_STRICT_REFCOUNTEDPOINTER=1",
      "JUCE_STANDALONE_APPLICATION=1",
      "JUCER_VS2026_78A5042=1",
      "JUCE_APP_VERSION=1.0.0",
      "JUCE_APP_VERSION_HEX=0x10000",
      "JucePlugin_Build_VST=0",
      "JucePlugin_Build_VST3=0",
      "JucePlugin_Build_AU=0",
      "JucePlugin_Build_AUv3=0",
      "JucePlugin_Build_AAX=0",
      "JucePlugin_Build_Standalone=0",
      "JucePlugin_Build_Unity=0",
      "JucePlugin_Build_LV2=0",
  });
  return definitions;
}

juce::Result runChildProcess(const juce::StringArray &arguments,
                             juce::String &outputOut,
                             int &exitCodeOut,
                             int timeoutMs = 120000) {
  juce::ChildProcess childProcess;
  if (!childProcess.start(arguments)) {
    return juce::Result::fail("Failed to start process: " +
                              arguments.joinIntoString(" "));
  }
  if (!childProcess.waitForProcessToFinish(timeoutMs)) {
    childProcess.kill();
    outputOut = childProcess.readAllProcessOutput();
    exitCodeOut = childProcess.getExitCode();
    return juce::Result::fail("Process timed out: " +
                              arguments.joinIntoString(" "));
  }
  outputOut = childProcess.readAllProcessOutput();
  exitCodeOut = childProcess.getExitCode();
  return juce::Result::ok();
}

juce::Result runCommandLine(const juce::String &commandLine,
                            juce::String &outputOut,
                            int &exitCodeOut,
                            int timeoutMs = 120000) {
  juce::ChildProcess childProcess;
  if (!childProcess.start(commandLine))
    return juce::Result::fail("Failed to start process: " + commandLine);
  if (!childProcess.waitForProcessToFinish(timeoutMs)) {
    childProcess.kill();
    outputOut = childProcess.readAllProcessOutput();
    exitCodeOut = childProcess.getExitCode();
    return juce::Result::fail("Process timed out: " + commandLine);
  }
  outputOut = childProcess.readAllProcessOutput();
  exitCodeOut = childProcess.getExitCode();
  return juce::Result::ok();
}

juce::File resolveDebugAppDirectory() {
  for (const auto &candidate : {
           juce::File::getCurrentWorkingDirectory().getChildFile("Builds").getChildFile("VisualStudio2026").getChildFile("x64").getChildFile("Debug").getChildFile("App"),
           juce::File::getCurrentWorkingDirectory().getChildFile("Builds").getChildFile("VisualStudio2022").getChildFile("x64").getChildFile("Debug").getChildFile("App")}) {
    if (candidate.isDirectory() && candidate.getChildFile("DadeumStudio.exe").existsAsFile())
      return candidate;
  }
  return {};
}

std::vector<juce::File> collectAppObjectFiles(const juce::File &appDirectory) {
  std::vector<juce::File> objectFiles;
  for (juce::RangedDirectoryIterator it(appDirectory, true, "*.obj", juce::File::findFiles);
       it != juce::RangedDirectoryIterator(); ++it) {
    const auto file = it->getFile();
    if (file.getFileName().equalsIgnoreCase("Main.obj"))
      continue;
    objectFiles.push_back(file);
  }
  std::sort(objectFiles.begin(), objectFiles.end(), [](const auto &a, const auto &b) {
    return a.getFullPathName().compareNatural(b.getFullPathName()) < 0;
  });
  return objectFiles;
}

const TVerificationGraphFixture *findFixtureById(
    const std::vector<TVerificationGraphFixture> &fixtures,
    const juce::String &fixtureId) {
  for (const auto &fixture : fixtures) {
    if (fixture.fixtureId == fixtureId)
      return &fixture;
  }
  return nullptr;
}

std::vector<CompiledParityCaseSpec> makeRepresentativeCaseSpecs(
    const std::vector<TVerificationGraphFixture> &fixtures,
    const juce::File &suiteDirectory,
    juce::String *errorMessageOut) {
  struct RequestedCase {
    juce::String fixtureId;
    TVerificationRenderProfile profile;
    TVerificationStimulusSpec stimulus;
  };
  const std::vector<RequestedCase> requestedCases = {
      {"G1", makePrimaryVerificationRenderProfile(), makeStaticRenderStimulus()},
      {"G2", makePrimaryVerificationRenderProfile(), makeSweepAutomationStimulus("LowPass", "cutoff", 320.0f, 7200.0f)},
      {"G3", makePrimaryVerificationRenderProfile(), makeSweepAutomationStimulus("Stereo Pan", "pan", -1.0f, 1.0f)},
      {"G4", makePrimaryVerificationRenderProfile(), makeMidiPhraseStimulus()},
      {"G5", makePrimaryVerificationRenderProfile(), makeStepAutomationStimulus("Delay", "mix", 0.15f, 0.75f, 0.25)},
  };

  std::vector<CompiledParityCaseSpec> caseSpecs;
  caseSpecs.reserve(requestedCases.size());
  for (const auto &requestedCase : requestedCases) {
    const auto *fixture = findFixtureById(fixtures, requestedCase.fixtureId);
    if (fixture == nullptr) {
      if (errorMessageOut != nullptr)
        *errorMessageOut = "Representative compiled parity fixture was not found: " + requestedCase.fixtureId;
      return {};
    }

    CompiledParityCaseSpec caseSpec;
    caseSpec.fixture = fixture;
    caseSpec.profile = requestedCase.profile;
    caseSpec.stimulus = requestedCase.stimulus;
    caseSpec.caseId = sanitizePathFragment(fixture->fixtureId) + "_" + sanitizePathFragment(requestedCase.stimulus.stimulusId) + "_" + sanitizePathFragment(requestedCase.profile.profileId);
    caseSpec.runtimeClassName = "TeulCompiledParity" + makeIdentifierFragment(caseSpec.caseId);
    caseSpec.exportDirectory = suiteDirectory.getChildFile(caseSpec.caseId);
    caseSpec.artifactDirectory = caseSpec.exportDirectory.getChildFile("CompiledParity");
    caseSpec.manifestFile = caseSpec.artifactDirectory.getChildFile("compiled-parity-manifest.json");
    caseSpec.hostObjectFile = caseSpec.artifactDirectory.getChildFile("CompiledParityCaseHost.obj");
    caseSpec.generatedObjectFile = caseSpec.artifactDirectory.getChildFile(caseSpec.runtimeClassName + ".obj");
    caseSpec.executableFile = caseSpec.artifactDirectory.getChildFile("CompiledParityCaseHost.exe");
    caseSpec.compileOutputFile = caseSpec.artifactDirectory.getChildFile("compile-output.txt");
    caseSpec.linkOutputFile = caseSpec.artifactDirectory.getChildFile("link-output.txt");
    caseSpec.runOutputFile = caseSpec.artifactDirectory.getChildFile("run-output.txt");
    caseSpec.linkResponseFile = caseSpec.artifactDirectory.getChildFile("link.rsp");
    caseSpecs.push_back(std::move(caseSpec));
  }
  return caseSpecs;
}

juce::var makeManifestJson(const CompiledParityCaseSpec &caseSpec,
                           float maxAbsoluteTolerance,
                           double rmsTolerance) {
  auto *root = new juce::DynamicObject();
  root->setProperty("graphId", caseSpec.fixture->fixtureId);
  root->setProperty("stimulusId", caseSpec.stimulus.stimulusId);
  root->setProperty("profileId", caseSpec.profile.profileId);
  root->setProperty("exportDirectory", caseSpec.exportDirectory.getFullPathName());
  root->setProperty("artifactDirectory", caseSpec.artifactDirectory.getFullPathName());
  root->setProperty("runtimeClassName", caseSpec.runtimeClassName);
  root->setProperty("maxAbsoluteTolerance", maxAbsoluteTolerance);
  root->setProperty("rmsTolerance", rmsTolerance);

  auto *profileObject = new juce::DynamicObject();
  profileObject->setProperty("profileId", caseSpec.profile.profileId);
  profileObject->setProperty("sampleRate", caseSpec.profile.sampleRate);
  profileObject->setProperty("blockSize", caseSpec.profile.blockSize);
  profileObject->setProperty("outputChannels", caseSpec.profile.outputChannels);
  profileObject->setProperty("durationSeconds", caseSpec.profile.durationSeconds);
  root->setProperty("profile", juce::var(profileObject));

  auto *stimulusObject = new juce::DynamicObject();
  stimulusObject->setProperty("stimulusId", caseSpec.stimulus.stimulusId);
  stimulusObject->setProperty("displayName", caseSpec.stimulus.displayName);

  juce::Array<juce::var> laneArray;
  for (const auto &lane : caseSpec.stimulus.automationLanes) {
    auto *laneObject = new juce::DynamicObject();
    laneObject->setProperty("nodeLabel", lane.nodeLabel);
    laneObject->setProperty("paramKey", lane.paramKey);
    laneObject->setProperty("mode", lane.mode == TVerificationAutomationMode::Linear ? "linear" : "step");
    laneObject->setProperty("startValue", lane.startValue);
    laneObject->setProperty("endValue", lane.endValue);
    laneObject->setProperty("stepIntervalSeconds", lane.stepIntervalSeconds);
    laneArray.add(juce::var(laneObject));
  }
  stimulusObject->setProperty("automationLanes", juce::var(laneArray));

  juce::Array<juce::var> midiArray;
  for (const auto &event : caseSpec.stimulus.midiEvents) {
    auto *eventObject = new juce::DynamicObject();
    eventObject->setProperty("sampleOffset", event.sampleOffset);
    juce::Array<juce::var> bytes;
    juce::uint8 rawData[4] = {0, 0, 0, 0};
    const int dataSize = event.message.getRawDataSize();
    const auto *data = event.message.getRawData();
    for (int index = 0; index < dataSize; ++index)
      bytes.add(static_cast<int>(data[index]));
    eventObject->setProperty("bytes", juce::var(bytes));
    midiArray.add(juce::var(eventObject));
  }
  stimulusObject->setProperty("midiEvents", juce::var(midiArray));
  root->setProperty("stimulus", juce::var(stimulusObject));

  return juce::var(root);
}
juce::String buildCompileCommand(const juce::File &sourceFile,
                                 const juce::File &objectFile,
                                 const juce::File &pdbFile,
                                 const juce::StringArray &extraDefinitions,
                                 const std::vector<juce::File> &includeDirectories) {
  juce::StringArray tokens;
  tokens.add("cl");
  tokens.add("/nologo");
  tokens.add("/c");
  tokens.add("/std:c++17");
  tokens.add("/EHsc");
  tokens.add("/MDd");
  tokens.add("/Zi");
  tokens.add("/FS");
  tokens.add("/Fd" + pdbFile.getFullPathName().quoted());
  for (const auto &directory : includeDirectories)
    tokens.add("/I" + directory.getFullPathName().quoted());
  for (const auto &definition : teulCompileDefinitions())
    tokens.add("/D" + definition);
  for (const auto &definition : extraDefinitions)
    tokens.add("/D" + definition);
  tokens.add("/Fo" + objectFile.getFullPathName().quoted());
  tokens.add(sourceFile.getFullPathName().quoted());
  return tokens.joinIntoString(" ");
}

juce::String buildLinkResponse(const juce::File &executableFile,
                               const juce::File &pdbFile,
                               const std::vector<juce::File> &objectFiles) {
  juce::StringArray lines;
  lines.add("/NOLOGO");
  lines.add("/OUT:" + executableFile.getFullPathName().quoted());
  lines.add("/PDB:" + pdbFile.getFullPathName().quoted());
  lines.add("/DEBUG");
  lines.add("/SUBSYSTEM:CONSOLE");
  lines.add("/MACHINE:X64");
  lines.add("/NODEFAULTLIB:libcmt.lib");
  lines.add("/NODEFAULTLIB:msvcrt.lib");
  for (const auto &objectFile : objectFiles)
    lines.add(objectFile.getFullPathName().quoted());
  return lines.joinIntoString("\r\n");
}

TVerificationCompiledParityCaseReport parseCaseReport(
    const CompiledParityCaseSpec &caseSpec) {
  TVerificationCompiledParityCaseReport report;
  report.graphId = caseSpec.fixture != nullptr ? caseSpec.fixture->fixtureId : juce::String();
  report.stimulusId = caseSpec.stimulus.stimulusId;
  report.profileId = caseSpec.profile.profileId;
  report.runtimeClassName = caseSpec.runtimeClassName;
  report.exportDirectory = caseSpec.exportDirectory.getFullPathName();
  report.artifactDirectory = caseSpec.artifactDirectory.getFullPathName();
  report.exportReport = caseSpec.exportReport;
  report.exportSucceeded = caseSpec.exportDirectory.isDirectory();

  const auto summaryFile = caseSpec.artifactDirectory.getChildFile("compiled-parity-summary.txt");
  if (!summaryFile.existsAsFile()) {
    report.failureReason = "Compiled parity case summary is missing.";
    return report;
  }

  const auto values = parseSummaryText(summaryFile.loadFileAsString());
  if (const auto it = values.find("graphId"); it != values.end())
    report.graphId = it->second;
  if (const auto it = values.find("stimulusId"); it != values.end())
    report.stimulusId = it->second;
  if (const auto it = values.find("profileId"); it != values.end())
    report.profileId = it->second;
  if (const auto it = values.find("passed"); it != values.end())
    report.passed = parseBoolText(it->second);
  if (const auto it = values.find("totalSamples"); it != values.end())
    report.totalSamples = it->second.getIntValue();
  if (const auto it = values.find("comparedChannels"); it != values.end())
    report.comparedChannels = it->second.getIntValue();
  if (const auto it = values.find("firstMismatchChannel"); it != values.end())
    report.firstMismatchChannel = it->second.getIntValue();
  if (const auto it = values.find("firstMismatchSample"); it != values.end())
    report.firstMismatchSample = it->second.getIntValue();
  if (const auto it = values.find("maxAbsoluteError"); it != values.end())
    report.maxAbsoluteError = it->second.getFloatValue();
  if (const auto it = values.find("rmsError"); it != values.end())
    report.rmsError = it->second.getDoubleValue();
  if (const auto it = values.find("failureReason"); it != values.end())
    report.failureReason = it->second;
  return report;
}

juce::Result exportCompiledParityCase(const TNodeRegistry &registry,
                                      CompiledParityCaseSpec &caseSpec) {
  juce::ignoreUnused(caseSpec.exportDirectory.createDirectory());
  juce::ignoreUnused(caseSpec.artifactDirectory.createDirectory());

  TExportOptions options;
  options.mode = TExportMode::RuntimeModule;
  options.outputDirectory = caseSpec.exportDirectory;
  options.projectRootDirectory = juce::File::getCurrentWorkingDirectory();
  options.runtimeClassName = caseSpec.runtimeClassName;
  options.overwriteExistingFiles = true;
  options.writeManifestJson = true;
  options.writeGraphJson = true;
  options.writeRuntimeDataJson = true;
  options.writeRuntimeModuleFiles = true;
  options.writeBuildHints = true;
  options.packageAssets = true;

  const auto exportResult =
      TExporter::exportToDirectory(caseSpec.fixture->document, registry, options,
                                   caseSpec.exportReport);
  if (exportResult.failed())
    return exportResult;

  if (!caseSpec.exportReport.generatedHeaderFile.existsAsFile() ||
      !caseSpec.exportReport.generatedSourceFile.existsAsFile() ||
      !caseSpec.exportReport.graphFile.existsAsFile() ||
      !caseSpec.exportReport.runtimeDataFile.existsAsFile() ||
      !caseSpec.exportReport.manifestFile.existsAsFile()) {
    return juce::Result::fail(
        "Compiled parity export is missing generated RuntimeModule artifacts.");
  }

  if (!writeJsonArtifact(caseSpec.manifestFile,
                         makeManifestJson(caseSpec, 1.0e-5f, 1.0e-6))) {
    return juce::Result::fail("Failed to write compiled parity manifest JSON.");
  }

  return juce::Result::ok();
}

juce::Result compileAndRunCase(const CompiledParityCaseSpec &caseSpec,
                               const juce::File &appDirectory,
                               const std::vector<juce::File> &appObjectFiles,
                               const juce::String &vcVarsPath,
                               int &compileExitCodeOut,
                               int &runExitCodeOut) {
  const auto hostSource = juce::File::getCurrentWorkingDirectory()
                              .getChildFile("Source")
                              .getChildFile("Teul")
                              .getChildFile("Verification")
                              .getChildFile("CompiledParity")
                              .getChildFile("CompiledParityCaseHost.cpp");
  if (!hostSource.existsAsFile())
    return juce::Result::fail("CompiledParityCaseHost.cpp is missing.");

  const auto wrapperFile = caseSpec.artifactDirectory.getChildFile("CompiledParityRuntimeWrapper.h");
  writeTextArtifact(wrapperFile,
                    juce::String("#pragma once\r\n#include \"") +
                        caseSpec.runtimeClassName + ".h\"\r\nusing TeulCompiledParityRuntimeClass = " +
                        caseSpec.runtimeClassName + ";\r\n");
  const auto hostPdbFile = caseSpec.artifactDirectory.getChildFile("CompiledParityCaseHost.pdb");
  const auto generatedPdbFile = caseSpec.artifactDirectory.getChildFile(caseSpec.runtimeClassName + ".pdb");
  const auto executablePdbFile = caseSpec.artifactDirectory.getChildFile("CompiledParityCaseHostExe.pdb");

  const std::vector<juce::File> includeDirectories = {
      caseSpec.artifactDirectory,
      caseSpec.exportDirectory,
      juce::File::getCurrentWorkingDirectory().getChildFile("Source"),
      juce::File::getCurrentWorkingDirectory().getChildFile("JuceLibraryCode"),
      juce::File("C:\\JUCE\\modules")};

  const auto hostCompileCommand = buildCompileCommand(
      hostSource, caseSpec.hostObjectFile, hostPdbFile, {}, includeDirectories);
  const auto generatedCompileCommand = buildCompileCommand(
      caseSpec.exportReport.generatedSourceFile, caseSpec.generatedObjectFile,
      generatedPdbFile, {}, includeDirectories);

  juce::String compileOutput;
  juce::String compileCommand =
      juce::String("cmd.exe /c call ") + vcVarsPath.quoted() +
      " >nul && " + hostCompileCommand + " && " + generatedCompileCommand;
  const auto compileResult =
      runCommandLine(compileCommand, compileOutput, compileExitCodeOut, 300000);
  writeTextArtifact(caseSpec.compileOutputFile, compileOutput);
  if (compileResult.failed())
    return compileResult;
  if (compileExitCodeOut != 0)
    return juce::Result::fail("Compiled parity host compilation failed.\n" + compileOutput);

  std::vector<juce::File> linkObjects;
  linkObjects.push_back(caseSpec.hostObjectFile);
  linkObjects.push_back(caseSpec.generatedObjectFile);
  linkObjects.insert(linkObjects.end(), appObjectFiles.begin(), appObjectFiles.end());
  if (!caseSpec.linkResponseFile.replaceWithText(
          buildLinkResponse(caseSpec.executableFile, executablePdbFile, linkObjects),
          false, false, "\r\n")) {
    return juce::Result::fail("Failed to write compiled parity link response file.");
  }

  juce::String linkOutput;
  const juce::String linkCommand =
      juce::String("cmd.exe /c call ") + vcVarsPath.quoted() +
      " >nul && link @" + caseSpec.linkResponseFile.getFullPathName().quoted();
  const auto linkResult =
      runCommandLine(linkCommand, linkOutput, compileExitCodeOut, 300000);
  writeTextArtifact(caseSpec.linkOutputFile, linkOutput);
  if (linkResult.failed())
    return linkResult;
  if (compileExitCodeOut != 0)
    return juce::Result::fail("Compiled parity host linking failed.\n" + linkOutput);
  if (!caseSpec.executableFile.existsAsFile())
    return juce::Result::fail("Compiled parity host executable was not produced.");

  juce::String runOutput;
  const juce::StringArray runArguments = {
      caseSpec.executableFile.getFullPathName(),
      "--manifest=" + caseSpec.manifestFile.getFullPathName()};
  const auto runResult = runChildProcess(runArguments, runOutput, runExitCodeOut, 300000);
  writeTextArtifact(caseSpec.runOutputFile, runOutput);
  if (runResult.failed())
    return runResult;
  if (runExitCodeOut != 0)
    return juce::Result::fail("Compiled parity host reported one or more failures.\n" + runOutput);

  juce::ignoreUnused(appDirectory);
  return juce::Result::ok();
}
juce::String buildSuiteSummaryText(const TVerificationCompiledParitySuiteReport &report) {
  juce::String summary;
  summary << "suiteId=" << report.suiteId << "\r\n";
  summary << "modeId=compiled-runtime" << "\r\n";
  summary << "passed=" << (report.passed ? "true" : "false") << "\r\n";
  summary << "artifactDirectory=" << report.artifactDirectory << "\r\n";
  summary << "executablePath=" << report.executablePath << "\r\n";
  summary << "compileExitCode=" << report.compileExitCode << "\r\n";
  summary << "runExitCode=" << report.runExitCode << "\r\n";
  summary << "totalCaseCount=" << report.totalCaseCount << "\r\n";
  summary << "passedCaseCount=" << report.passedCaseCount << "\r\n";
  summary << "failedCaseCount=" << report.failedCaseCount << "\r\n";
  if (report.failureReason.isNotEmpty())
    summary << "failureReason=" << report.failureReason << "\r\n";
  summary << "\r\n";
  for (const auto &caseReport : report.caseReports) {
    summary << "case=" << caseReport.graphId << "/" << caseReport.stimulusId << "/"
            << caseReport.profileId << "\r\n";
    summary << "passed=" << (caseReport.passed ? "true" : "false") << "\r\n";
    if (caseReport.failureReason.isNotEmpty())
      summary << "failureReason=" << caseReport.failureReason << "\r\n";
    summary << "\r\n";
  }
  return summary;
}

juce::Result writeCaseArtifactBundle(const CompiledParityCaseSpec &caseSpec,
                                     const TVerificationCompiledParityCaseReport &report) {
  juce::Array<juce::var> files;
  const auto addFile = [&](const juce::String &role, const juce::File &file) {
    files.add(makeArtifactFileEntry(role, caseSpec.artifactDirectory, file));
  };
  addFile("manifest", caseSpec.manifestFile);
  addFile("wrapperHeader", caseSpec.artifactDirectory.getChildFile("CompiledParityRuntimeWrapper.h"));
  addFile("summary", caseSpec.artifactDirectory.getChildFile("compiled-parity-summary.txt"));
  addFile("failure", caseSpec.artifactDirectory.getChildFile("compiled-parity-failure.txt"));
  addFile("compileOutput", caseSpec.compileOutputFile);
  addFile("linkOutput", caseSpec.linkOutputFile);
  addFile("runOutput", caseSpec.runOutputFile);
  addFile("hostObject", caseSpec.hostObjectFile);
  addFile("generatedObject", caseSpec.generatedObjectFile);
  addFile("hostExecutable", caseSpec.executableFile);
  addFile("generatedHeader", caseSpec.exportReport.generatedHeaderFile);
  addFile("generatedSource", caseSpec.exportReport.generatedSourceFile);
  addFile("editableGraph", caseSpec.exportReport.graphFile);
  addFile("runtimeData", caseSpec.exportReport.runtimeDataFile);
  addFile("manifestJson", caseSpec.exportReport.manifestFile);
  addFile("exportReport", caseSpec.exportReport.reportFile);

  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", "compiled-runtime-parity-case");
  root->setProperty("graphId", report.graphId);
  root->setProperty("stimulusId", report.stimulusId);
  root->setProperty("profileId", report.profileId);
  root->setProperty("passed", report.passed);
  root->setProperty("artifactDirectory", report.artifactDirectory);
  root->setProperty("exportDirectory", report.exportDirectory);
  root->setProperty("runtimeClassName", report.runtimeClassName);
  root->setProperty("totalSamples", report.totalSamples);
  root->setProperty("comparedChannels", report.comparedChannels);
  root->setProperty("maxAbsoluteError", report.maxAbsoluteError);
  root->setProperty("rmsError", report.rmsError);
  root->setProperty("firstMismatchChannel", report.firstMismatchChannel);
  root->setProperty("firstMismatchSample", report.firstMismatchSample);
  root->setProperty("exportSucceeded", report.exportSucceeded);
  if (report.failureReason.isNotEmpty())
    root->setProperty("failureReason", report.failureReason);
  root->setProperty("files", juce::var(files));

  if (!writeJsonArtifact(caseSpec.artifactDirectory.getChildFile("artifact-bundle.json"),
                         juce::var(root))) {
    return juce::Result::fail("Failed to write compiled parity case artifact bundle.");
  }
  return juce::Result::ok();
}

juce::Result writeSuiteArtifactBundle(const juce::File &suiteDirectory,
                                      const TVerificationCompiledParitySuiteReport &report,
                                      const std::vector<CompiledParityCaseSpec> &caseSpecs) {
  juce::Array<juce::var> files;
  const auto addFile = [&](const juce::String &role, const juce::File &file) {
    files.add(makeArtifactFileEntry(role, suiteDirectory, file));
  };
  addFile("suiteSummary", suiteDirectory.getChildFile("compiled-runtime-parity-summary.txt"));
  addFile("suiteFailure", suiteDirectory.getChildFile("compiled-runtime-parity-failure.txt"));

  juce::Array<juce::var> cases;
  for (size_t index = 0; index < caseSpecs.size(); ++index) {
    const auto &caseSpec = caseSpecs[index];
    addFile("caseDirectory", caseSpec.exportDirectory);
    if (index < report.caseReports.size()) {
      auto *caseObject = new juce::DynamicObject();
      caseObject->setProperty("graphId", report.caseReports[index].graphId);
      caseObject->setProperty("stimulusId", report.caseReports[index].stimulusId);
      caseObject->setProperty("profileId", report.caseReports[index].profileId);
      caseObject->setProperty("passed", report.caseReports[index].passed);
      caseObject->setProperty("artifactDirectory", report.caseReports[index].artifactDirectory);
      caseObject->setProperty("exportDirectory", report.caseReports[index].exportDirectory);
      if (report.caseReports[index].failureReason.isNotEmpty())
        caseObject->setProperty("failureReason", report.caseReports[index].failureReason);
      cases.add(juce::var(caseObject));
    }
  }

  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", "compiled-runtime-parity-suite");
  root->setProperty("suiteId", report.suiteId);
  root->setProperty("passed", report.passed);
  root->setProperty("artifactDirectory", report.artifactDirectory);
  root->setProperty("executablePath", report.executablePath);
  root->setProperty("compileExitCode", report.compileExitCode);
  root->setProperty("runExitCode", report.runExitCode);
  root->setProperty("totalCaseCount", report.totalCaseCount);
  root->setProperty("passedCaseCount", report.passedCaseCount);
  root->setProperty("failedCaseCount", report.failedCaseCount);
  if (report.failureReason.isNotEmpty())
    root->setProperty("failureReason", report.failureReason);
  root->setProperty("files", juce::var(files));
  root->setProperty("cases", juce::var(cases));

  if (!writeJsonArtifact(suiteDirectory.getChildFile("artifact-bundle.json"), juce::var(root)))
    return juce::Result::fail("Failed to write compiled parity suite artifact bundle.");
  return juce::Result::ok();
}

} // namespace

bool runRepresentativeCompiledRuntimeParity(
    const TNodeRegistry &registry,
    TVerificationCompiledParitySuiteReport &reportOut,
    float maxAbsoluteTolerance,
    double rmsTolerance) {
  reportOut = {};
  reportOut.suiteId = "representative-primary";

  const auto suiteDirectory = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile("Builds")
                                  .getChildFile("TeulVerification")
                                  .getChildFile("CompiledRuntimeParity")
                                  .getChildFile("RepresentativePrimary");
  reportOut.artifactDirectory = suiteDirectory.getFullPathName();
  juce::ignoreUnused(suiteDirectory.createDirectory());

  const auto appDirectory = resolveDebugAppDirectory();
  if (!appDirectory.isDirectory()) {
    reportOut.failureReason = "Teul debug app directory was not found for compiled runtime parity.";
    writeTextArtifact(suiteDirectory.getChildFile("compiled-runtime-parity-summary.txt"),
                      buildSuiteSummaryText(reportOut));
    return false;
  }

  const auto vcVarsPath = resolveVcVars64Path();
  if (vcVarsPath.isEmpty()) {
    reportOut.failureReason = "Visual Studio vcvars64.bat was not found for compiled runtime parity.";
    writeTextArtifact(suiteDirectory.getChildFile("compiled-runtime-parity-summary.txt"),
                      buildSuiteSummaryText(reportOut));
    return false;
  }

  const auto fixtures = makeRepresentativeVerificationGraphSet(registry);
  juce::String errorMessage;
  auto caseSpecs = makeRepresentativeCaseSpecs(fixtures, suiteDirectory, &errorMessage);
  if (caseSpecs.empty()) {
    reportOut.failureReason = errorMessage.isNotEmpty() ? errorMessage : "Representative compiled runtime parity case list is empty.";
    writeTextArtifact(suiteDirectory.getChildFile("compiled-runtime-parity-summary.txt"),
                      buildSuiteSummaryText(reportOut));
    return false;
  }

  const auto appObjectFiles = collectAppObjectFiles(appDirectory);
  if (appObjectFiles.empty()) {
    reportOut.failureReason = "Compiled runtime parity could not find app object files.";
    writeTextArtifact(suiteDirectory.getChildFile("compiled-runtime-parity-summary.txt"),
                      buildSuiteSummaryText(reportOut));
    return false;
  }

  bool suitePassed = true;
  for (auto &caseSpec : caseSpecs) {
    auto exportResult = exportCompiledParityCase(registry, caseSpec);
    if (exportResult.wasOk() &&
        !writeJsonArtifact(caseSpec.manifestFile,
                           makeManifestJson(caseSpec, maxAbsoluteTolerance, rmsTolerance))) {
      exportResult = juce::Result::fail("Failed to write compiled parity manifest JSON.");
    }

    int compileExitCode = -1;
    int runExitCode = -1;
    if (exportResult.wasOk()) {
      const auto runResult = compileAndRunCase(caseSpec, appDirectory, appObjectFiles,
                                               vcVarsPath, compileExitCode,
                                               runExitCode);
      reportOut.compileExitCode = compileExitCode;
      reportOut.runExitCode = runExitCode;
      if (runResult.failed())
        exportResult = runResult;
    }

    auto caseReport = parseCaseReport(caseSpec);
    caseReport.exportReport = caseSpec.exportReport;
    caseReport.exportSucceeded = exportResult.wasOk();
    if (exportResult.failed()) {
      caseReport.passed = false;
      caseReport.failureReason = exportResult.getErrorMessage();
      writeTextArtifact(caseSpec.artifactDirectory.getChildFile("compiled-parity-summary.txt"),
                        juce::String("graphId=") + caseReport.graphId + "\r\n" +
                            "stimulusId=" + caseReport.stimulusId + "\r\n" +
                            "profileId=" + caseReport.profileId + "\r\n" +
                            "passed=false\r\n" +
                            "failureReason=" + caseReport.failureReason + "\r\n");
      writeTextArtifact(caseSpec.artifactDirectory.getChildFile("compiled-parity-failure.txt"),
                        caseReport.failureReason + "\r\n");
      suitePassed = false;
    }

    if (!caseReport.passed)
      suitePassed = false;
    reportOut.caseReports.push_back(caseReport);
    juce::ignoreUnused(writeCaseArtifactBundle(caseSpec, reportOut.caseReports.back()));
  }

  reportOut.totalCaseCount = static_cast<int>(reportOut.caseReports.size());
  reportOut.passedCaseCount = 0;
  for (const auto &caseReport : reportOut.caseReports)
    if (caseReport.passed)
      ++reportOut.passedCaseCount;
  reportOut.failedCaseCount = reportOut.totalCaseCount - reportOut.passedCaseCount;
  reportOut.passed = suitePassed && reportOut.failedCaseCount == 0;
  if (!reportOut.passed && reportOut.failureReason.isEmpty())
    reportOut.failureReason = "Representative compiled runtime parity reported one or more failures.";
  if (!reportOut.caseReports.empty())
    reportOut.executablePath = caseSpecs.front().executableFile.getFullPathName();

  writeTextArtifact(suiteDirectory.getChildFile("compiled-runtime-parity-summary.txt"),
                    buildSuiteSummaryText(reportOut));
  if (!reportOut.passed)
    writeTextArtifact(suiteDirectory.getChildFile("compiled-runtime-parity-failure.txt"),
                      reportOut.failureReason + "\r\n");
  juce::ignoreUnused(writeSuiteArtifactBundle(suiteDirectory, reportOut, caseSpecs));
  return reportOut.passed;
}

} // namespace Teul