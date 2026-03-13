/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include "AppServices.h"
#include "Gyeol/Export/JuceComponentExport.h"
#include "Gyeol/Runtime/PropertyBindingResolver.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include "Teul/Export/TExport.h"
#include "Teul/Registry/TNodeRegistry.h"
#include "Teul/Verification/TVerificationParity.h"
#include "Teul/Verification/TVerificationBenchmark.h"
#include "Teul/Verification/TVerificationGoldenAudio.h"
#include "Teul/Verification/TVerificationCompiledParity.h"
#include "Teul/Verification/TVerificationStress.h"
#include "Teul/Serialization/TPatchPresetIO.h"
#include "Teul/Serialization/TStatePresetIO.h"
#include "Teul/Serialization/TFileIo.h"
#include "Teul/Serialization/TSerializer.h"
#include "Teul/Public/EditorHandle.h"
#include "MainComponent.h"
#include <JuceHeader.h>
#include <algorithm>
#include <iostream>
#include <map>

namespace {
juce::StringArray parseCommandLineArgs(const juce::String &commandLine) {
  juce::StringArray args;
  args.addTokens(commandLine, true);
  args.trim();
  args.removeEmptyStrings();
  return args;
}

bool hasArg(const juce::StringArray &args, const juce::String &key) {
  for (const auto &arg : args) {
    if (arg == key)
      return true;
  }

  return false;
}

juce::String argValue(const juce::StringArray &args,
                      const juce::String &prefix) {
  for (const auto &arg : args) {
    if (arg.startsWith(prefix))
      return arg.fromFirstOccurrenceOf(prefix, false, false).unquoted();
  }

  return {};
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

juce::StringArray teulCompileSmokeDefinitions() {
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
juce::String relativeArtifactPath(const juce::File &root,
                                  const juce::File &file) {
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
bool writeJsonArtifact(const juce::File &file, const juce::var &json) {
  return file.replaceWithText(juce::JSON::toString(json, true), false, false,
                              "\r\n");
}

juce::var stringArrayToJsonArray(const juce::StringArray &values) {
  juce::Array<juce::var> items;
  for (const auto &value : values)
    items.add(value);
  return juce::var(items);
}

juce::String summarizeWarnings(const juce::StringArray &values) {
  return values.joinIntoString(" | ");
}

bool writeSessionStateArtifact(const juce::File &file, bool cleanShutdown) {
  auto *root = new juce::DynamicObject();
  root->setProperty("cleanShutdown", cleanShutdown);
  root->setProperty(
      "updatedAtMillis",
      static_cast<juce::int64>(juce::Time::currentTimeMillis()));
  return file.replaceWithText(juce::JSON::toString(juce::var(root), true), false,
                              false, "\r\n");
}

bool readSessionStateArtifact(const juce::File &file, bool &cleanShutdownOut) {
  juce::var json;
  if (juce::JSON::parse(file.loadFileAsString(), json).failed())
    return false;

  const auto *root = json.getDynamicObject();
  if (root == nullptr)
    return false;

  const auto value = root->getProperty("cleanShutdown");
  if (value.isBool()) {
    cleanShutdownOut = static_cast<bool>(value);
    return true;
  }

  const auto textValue = value.toString().trim();
  if (textValue.isEmpty())
    return false;

  cleanShutdownOut = textValue.equalsIgnoreCase("true") || textValue == "1";
  return true;
}
juce::Result writeTeulRuntimeCompileSmokeArtifactBundle(
    const juce::File &outputDirectory,
    const juce::String &runtimeClassName,
    const juce::String &vcVarsPath,
    bool passed,
    const juce::String &failureReason,
    int exportExitCode,
    int compileExitCode) {
  juce::ignoreUnused(outputDirectory.createDirectory());
  juce::Array<juce::var> files;
  const auto addFile = [&](const juce::String &role, const juce::File &file) {
    if (file.getFullPathName().isNotEmpty())
      files.add(makeArtifactFileEntry(role, outputDirectory, file));
  };
  addFile("generatedHeader", outputDirectory.getChildFile(runtimeClassName + ".h"));
  addFile("generatedSource", outputDirectory.getChildFile(runtimeClassName + ".cpp"));
  addFile("objectFile", outputDirectory.getChildFile(runtimeClassName + ".obj"));
  addFile("phase5ExportLog",
          outputDirectory.getChildFile("phase5-export-smoke.txt"));
  addFile("compileOutput", outputDirectory.getChildFile("compile-output.txt"));
  addFile("exportManifest", outputDirectory.getChildFile("export-manifest.json"));
  addFile("exportReport", outputDirectory.getChildFile("export-report.txt"));
  addFile("editableGraph", outputDirectory.getChildFile("editable-graph.teul"));
  addFile("runtimeData", outputDirectory.getChildFile("export-runtime.json"));
  addFile("assetDirectory", outputDirectory.getChildFile("Assets"));
  auto *root = new juce::DynamicObject();
  root->setProperty("kind", "teul-verification-artifact-bundle");
  root->setProperty("scope", "runtime-compile-smoke");
  root->setProperty("passed", passed);
  root->setProperty("artifactDirectory", outputDirectory.getFullPathName());
  root->setProperty("runtimeClassName", runtimeClassName);
  root->setProperty("vcvars64Path", vcVarsPath);
  root->setProperty("exportExitCode", exportExitCode);
  root->setProperty("compileExitCode", compileExitCode);
  if (failureReason.isNotEmpty())
    root->setProperty("failureReason", failureReason);
  root->setProperty("files", juce::var(files));
  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  if (!writeJsonArtifact(bundleFile, juce::var(root))) {
    return juce::Result::fail(
        "Failed to write runtime compile smoke artifact bundle.");
  }
  return juce::Result::ok();
}
juce::String buildTeulCompileSmokeScript(const juce::File &outputDirectory,
                                         const juce::String &runtimeClassName) {
  const auto vcVarsPath = resolveVcVars64Path();
  const auto generatedSource =
      outputDirectory.getChildFile(runtimeClassName + ".cpp");
  const auto objectFile = outputDirectory.getChildFile(runtimeClassName + ".obj");
  const auto projectRoot = juce::File::getCurrentWorkingDirectory();

  juce::StringArray tokens;
  tokens.add("call");
  tokens.add(vcVarsPath.quoted());
  tokens.add(">nul");
  tokens.add("&&");
  tokens.add("cl");
  tokens.add("/nologo");
  tokens.add("/c");
  tokens.add("/std:c++17");
  tokens.add("/EHsc");
  tokens.add("/MDd");
  tokens.add("/I" + outputDirectory.getFullPathName().quoted());
  tokens.add("/I" + projectRoot.getChildFile("Source").getFullPathName().quoted());
  tokens.add("/I" + projectRoot.getChildFile("JuceLibraryCode").getFullPathName().quoted());
  tokens.add("/I\"C:\\JUCE\\modules\"");

  for (const auto &definition : teulCompileSmokeDefinitions())
    tokens.add("/D" + definition);

  tokens.add("/Fo" + objectFile.getFullPathName().quoted());
  tokens.add(generatedSource.getFullPathName().quoted());
  return tokens.joinIntoString(" ");
}

juce::int64 parseWidgetIdFromVar(const juce::var &value) {
  if (value.isInt() || value.isInt64())
    return static_cast<juce::int64>(value);

  return value.toString().trim().getLargeIntValue();
}

bool valueIsTruthy(const juce::var &value) {
  if (value.isBool())
    return static_cast<bool>(value);
  if (value.isInt() || value.isInt64() || value.isDouble())
    return std::abs(static_cast<double>(value)) > 0.000000000001;

  const auto text = value.toString().trim().toLowerCase();
  return text == "1" || text == "true" || text == "yes" || text == "on";
}

juce::String
resolveRuntimeParamKey(const std::map<juce::String, juce::var> &params,
                       const juce::String &requestedKey) {
  const auto trimmed = requestedKey.trim();
  if (trimmed.isEmpty())
    return {};

  if (params.find(trimmed) != params.end())
    return trimmed;

  for (const auto &entry : params) {
    if (entry.first.equalsIgnoreCase(trimmed))
      return entry.first;
  }

  return trimmed;
}

int inferTeulPortChannelIndex(juce::StringRef portName) {
  const auto lowered = juce::String(portName).trim().toLowerCase();
  if (lowered == "r" || lowered == "r in" || lowered == "r out" ||
      lowered.startsWith("r ") || lowered.startsWith("right")) {
    return 1;
  }

  return 0;
}

Teul::TNode makeTeulNodeFromDescriptor(const Teul::TNodeDescriptor &descriptor,
                                       Teul::TGraphDocument &document,
                                       float x,
                                       float y,
                                       const juce::String &label = {}) {
  Teul::TNode node;
  node.nodeId = document.allocNodeId();
  node.typeKey = descriptor.typeKey;
  node.x = x;
  node.y = y;
  node.label = label;

  for (const auto &spec : descriptor.paramSpecs)
    node.params[spec.key] = spec.defaultValue;

  for (const auto &portSpec : descriptor.portSpecs) {
    Teul::TPort port;
    port.portId = document.allocPortId();
    port.ownerNodeId = node.nodeId;
    port.direction = portSpec.direction;
    port.dataType = portSpec.dataType;
    port.name = portSpec.name;
    port.channelIndex = inferTeulPortChannelIndex(portSpec.name);
    node.ports.push_back(port);
  }

  return node;
}

const Teul::TPort *findTeulPortByName(const Teul::TNode &node,
                                      juce::StringRef portName) {
  for (const auto &port : node.ports) {
    if (port.name.equalsIgnoreCase(juce::String(portName)))
      return &port;
  }

  return nullptr;
}

juce::Result addTeulConnection(Teul::TGraphDocument &document,
                               Teul::NodeId fromNodeId,
                               juce::StringRef fromPortName,
                               Teul::NodeId toNodeId,
                               juce::StringRef toPortName) {
  const auto *fromNode = document.findNode(fromNodeId);
  const auto *toNode = document.findNode(toNodeId);
  if (fromNode == nullptr || toNode == nullptr)
    return juce::Result::fail("Teul smoke graph references a missing node.");

  const auto *fromPort = findTeulPortByName(*fromNode, fromPortName);
  const auto *toPort = findTeulPortByName(*toNode, toPortName);
  if (fromPort == nullptr || toPort == nullptr) {
    return juce::Result::fail("Teul smoke graph references a missing port.");
  }

  Teul::TConnection connection;
  connection.connectionId = document.allocConnectionId();
  connection.from.nodeId = fromNodeId;
  connection.from.portId = fromPort->portId;
  connection.to.nodeId = toNodeId;
  connection.to.portId = toPort->portId;
  document.connections.push_back(connection);
  return juce::Result::ok();
}

Teul::TNode *findTeulNodeByLabel(Teul::TGraphDocument &document,
                                 const juce::String &label) {
  for (auto &node : document.nodes) {
    if (node.label == label)
      return &node;
  }

  return nullptr;
}

Teul::TGraphDocument
makeTeulPhase5SmokeDocument(const Teul::TNodeRegistry &registry,
                            const juce::File &assetSourceFile) {
  Teul::TGraphDocument document;
  document.meta.name = "Teul Phase5 Smoke";

  const auto *oscillatorDescriptor =
      registry.descriptorFor("Teul.Source.Oscillator");
  const auto *vcaDescriptor = registry.descriptorFor("Teul.Mixer.VCA");
  const auto *constantDescriptor = registry.descriptorFor("Teul.Source.Constant");
  const auto *outputDescriptor = registry.descriptorFor("Teul.Routing.AudioOut");
  if (oscillatorDescriptor == nullptr || vcaDescriptor == nullptr ||
      constantDescriptor == nullptr || outputDescriptor == nullptr) {
    return document;
  }

  auto oscillator = makeTeulNodeFromDescriptor(*oscillatorDescriptor, document,
                                               80.0f, 120.0f, "Carrier");
  oscillator.params["waveform"] = 3;
  oscillator.params["frequency"] = 330.0;
  oscillator.params["gain"] = 0.55;
  oscillator.params["impulsePath"] = assetSourceFile.getFullPathName();

  auto vca = makeTeulNodeFromDescriptor(*vcaDescriptor, document, 320.0f,
                                        120.0f, "Amp");
  vca.params["gain"] = 0.75;

  auto cvConstant = makeTeulNodeFromDescriptor(*constantDescriptor, document,
                                               320.0f, 280.0f, "CV");
  cvConstant.params["value"] = 0.85;

  auto deadConstant = makeTeulNodeFromDescriptor(*constantDescriptor, document,
                                                 560.0f, 300.0f, "Dead");
  deadConstant.params["value"] = 0.25;

  auto output = makeTeulNodeFromDescriptor(*outputDescriptor, document, 560.0f,
                                           120.0f, "Main Out");
  output.params["volume"] = 0.9;

  const auto oscillatorId = oscillator.nodeId;
  const auto vcaId = vca.nodeId;
  const auto cvConstantId = cvConstant.nodeId;
  const auto outputId = output.nodeId;

  document.nodes.push_back(std::move(oscillator));
  document.nodes.push_back(std::move(vca));
  document.nodes.push_back(std::move(cvConstant));
  document.nodes.push_back(std::move(deadConstant));
  document.nodes.push_back(std::move(output));

  juce::ignoreUnused(addTeulConnection(document, oscillatorId, "Out", vcaId,
                                       "In"));
  juce::ignoreUnused(addTeulConnection(document, cvConstantId, "Value", vcaId,
                                       "CV"));
  juce::ignoreUnused(addTeulConnection(document, vcaId, "Out", outputId,
                                       "L In"));
  juce::ignoreUnused(addTeulConnection(document, vcaId, "Out", outputId,
                                       "R In"));
  return document;
}

Teul::TGraphDocument
makeTeulPhase5InvalidSmokeDocument(const Teul::TNodeRegistry &registry,
                                   const juce::File &assetSourceFile) {
  auto document = makeTeulPhase5SmokeDocument(registry, assetSourceFile);

  std::vector<Teul::NodeId> removedIds;
  document.nodes.erase(
      std::remove_if(document.nodes.begin(), document.nodes.end(),
                     [&](const auto &node) {
                       if (node.typeKey == "Teul.Routing.AudioOut") {
                         removedIds.push_back(node.nodeId);
                         return true;
                       }
                       return false;
                     }),
      document.nodes.end());

  document.connections.erase(
      std::remove_if(document.connections.begin(), document.connections.end(),
                     [&](const auto &connection) {
                       return std::find(removedIds.begin(), removedIds.end(),
                                        connection.from.nodeId) != removedIds.end() ||
                              std::find(removedIds.begin(), removedIds.end(),
                                        connection.to.nodeId) != removedIds.end();
                     }),
      document.connections.end());
  return document;
}

Gyeol::DocumentModel makePhase6SmokeDocument() {
  Gyeol::DocumentModel document;
  document.schemaVersion = Gyeol::currentSchemaVersion();

  Gyeol::WidgetModel button;
  button.id = 1001;
  button.type = Gyeol::WidgetType::button;
  button.bounds = {24.0f, 20.0f, 110.0f, 32.0f};
  button.properties.set("text", juce::String("Apply"));

  Gyeol::WidgetModel slider;
  slider.id = 1002;
  slider.type = Gyeol::WidgetType::slider;
  slider.bounds = {160.0f, 20.0f, 220.0f, 32.0f};
  slider.properties.set("slider.style", juce::String("linearHorizontal"));
  slider.properties.set("slider.rangeMin", 0.0);
  slider.properties.set("slider.rangeMax", 1.0);
  slider.properties.set("slider.step", 0.0);
  slider.properties.set("value", 0.25);
  slider.properties.set("minValue", 0.0);
  slider.properties.set("maxValue", 1.0);

  Gyeol::WidgetModel label;
  label.id = 1003;
  label.type = Gyeol::WidgetType::label;
  label.bounds = {24.0f, 70.0f, 220.0f, 28.0f};
  label.properties.set("text", juce::String("Idle"));

  document.widgets.push_back(button);
  document.widgets.push_back(slider);
  document.widgets.push_back(label);

  Gyeol::LayerModel layer;
  layer.id = 5001;
  layer.name = "Layer 1";
  layer.order = 0;
  layer.visible = true;
  layer.locked = false;
  layer.memberWidgetIds = {button.id, slider.id, label.id};
  document.layers.push_back(layer);

  Gyeol::RuntimeParamModel paramA;
  paramA.key = "A";
  paramA.type = Gyeol::RuntimeParamValueType::number;
  paramA.defaultValue = 0.25;
  paramA.description = "Smoke number";
  paramA.exposed = true;

  Gyeol::RuntimeParamModel paramB;
  paramB.key = "B";
  paramB.type = Gyeol::RuntimeParamValueType::boolean;
  paramB.defaultValue = false;
  paramB.description = "Smoke toggle";
  paramB.exposed = true;

  document.runtimeParams.push_back(paramA);
  document.runtimeParams.push_back(paramB);

  Gyeol::PropertyBindingModel sliderBinding;
  sliderBinding.id = 2001;
  sliderBinding.name = "Slider from A";
  sliderBinding.enabled = true;
  sliderBinding.targetWidgetId = slider.id;
  sliderBinding.targetProperty = "value";
  sliderBinding.expression = "A";

  Gyeol::PropertyBindingModel sliderMinBinding;
  sliderMinBinding.id = 2002;
  sliderMinBinding.name = "Slider min from A";
  sliderMinBinding.enabled = true;
  sliderMinBinding.targetWidgetId = slider.id;
  sliderMinBinding.targetProperty = "minValue";
  sliderMinBinding.expression = "A * 0.5";

  document.propertyBindings.push_back(sliderBinding);
  document.propertyBindings.push_back(sliderMinBinding);

  Gyeol::RuntimeBindingModel clickBinding;
  clickBinding.id = 3001;
  clickBinding.name = "Button click -> update A and label";
  clickBinding.enabled = true;
  clickBinding.sourceWidgetId = button.id;
  clickBinding.eventKey = "onClick";

  Gyeol::RuntimeActionModel setParamAAction;
  setParamAAction.kind = Gyeol::RuntimeActionKind::setRuntimeParam;
  setParamAAction.paramKey = "A";
  setParamAAction.value = 0.9;
  clickBinding.actions.push_back(setParamAAction);

  Gyeol::RuntimeActionModel patchLabelTextAction;
  patchLabelTextAction.kind = Gyeol::RuntimeActionKind::setNodeProps;
  patchLabelTextAction.target.kind = Gyeol::NodeKind::widget;
  patchLabelTextAction.target.id = label.id;
  patchLabelTextAction.patch.set("text", juce::String("Clicked"));
  clickBinding.actions.push_back(patchLabelTextAction);

  Gyeol::RuntimeActionModel moveLabelAction;
  moveLabelAction.kind = Gyeol::RuntimeActionKind::setNodeBounds;
  moveLabelAction.targetWidgetId = label.id;
  moveLabelAction.bounds = {250.0f, 70.0f, 180.0f, 28.0f};
  clickBinding.actions.push_back(moveLabelAction);

  Gyeol::RuntimeBindingModel commitBinding;
  commitBinding.id = 3002;
  commitBinding.name = "Slider commit -> set A from payload";
  commitBinding.enabled = true;
  commitBinding.sourceWidgetId = slider.id;
  commitBinding.eventKey = "onValueCommit";

  Gyeol::RuntimeActionModel payloadToParamAction;
  payloadToParamAction.kind = Gyeol::RuntimeActionKind::setRuntimeParam;
  payloadToParamAction.paramKey = "A";
  commitBinding.actions.push_back(payloadToParamAction);

  document.runtimeBindings.push_back(clickBinding);
  document.runtimeBindings.push_back(commitBinding);
  return document;
}

Gyeol::DocumentModel makePhase6InvalidSmokeDocument() {
  auto document = makePhase6SmokeDocument();
  if (!document.propertyBindings.empty()) {
    document.propertyBindings.front().expression = "A +";
  } else {
    Gyeol::PropertyBindingModel invalidBinding;
    invalidBinding.id = 9001;
    invalidBinding.name = "Invalid expression";
    invalidBinding.enabled = true;
    invalidBinding.targetWidgetId = 1002;
    invalidBinding.targetProperty = "value";
    invalidBinding.expression = "A +";
    document.propertyBindings.push_back(invalidBinding);
  }

  return document;
}

juce::Result runPhase6ExportSmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("GyeolExport")
            .getChildFile("Phase6Smoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  const auto document = makePhase6SmokeDocument();
  const auto registry = Gyeol::Widgets::makeDefaultWidgetRegistry();

  Gyeol::Export::ExportOptions options;
  options.outputDirectory = outputDirectory;
  options.projectRootDirectory = juce::File::getCurrentWorkingDirectory();
  options.componentClassName = "GyeolPhase6SmokeComponent";
  options.overwriteExistingFiles = true;
  options.writeManifestJson = true;
  options.writeRuntimeDataJson = true;

  Gyeol::Export::ExportReport report;
  const auto result =
      Gyeol::Export::exportToJuceComponent(document, registry, options, report);
  std::cout << report.toText() << std::endl;

  if (result.failed())
    return result;

  if (!report.generatedHeaderFile.existsAsFile() ||
      !report.generatedSourceFile.existsAsFile() ||
      !report.manifestFile.existsAsFile() ||
      !report.runtimeDataFile.existsAsFile()) {
    return juce::Result::fail("Smoke export output missing expected files.");
  }

  const auto runtimeJson = juce::JSON::parse(report.runtimeDataFile);
  auto *runtimeRoot = runtimeJson.getDynamicObject();
  if (runtimeRoot == nullptr)
    return juce::Result::fail("Smoke runtime JSON parse failed.");

  const auto *runtimeParamArray =
      runtimeRoot->getProperty("runtimeParams").getArray();
  const auto *propertyBindingArray =
      runtimeRoot->getProperty("propertyBindings").getArray();
  const auto *runtimeBindingArray =
      runtimeRoot->getProperty("runtimeBindings").getArray();
  if (runtimeParamArray == nullptr || runtimeParamArray->isEmpty())
    return juce::Result::fail("Smoke runtime JSON has no runtimeParams.");
  if (propertyBindingArray == nullptr || propertyBindingArray->isEmpty())
    return juce::Result::fail("Smoke runtime JSON has no propertyBindings.");
  if (runtimeBindingArray == nullptr || runtimeBindingArray->isEmpty())
    return juce::Result::fail("Smoke runtime JSON has no runtimeBindings.");

  // Scenario smoke: button(onClick) -> setRuntimeParam(A) ->
  // propertyBinding(slider.value=A).
  std::map<juce::String, juce::var> simulatedParams;
  for (const auto &paramVar : *runtimeParamArray) {
    auto *paramObject = paramVar.getDynamicObject();
    if (paramObject == nullptr)
      continue;

    const auto key = paramObject->getProperty("key").toString().trim();
    if (key.isEmpty())
      continue;
    simulatedParams[key] = paramObject->getProperty("defaultValue");
  }

  bool clickBindingExecuted = false;
  for (const auto &bindingVar : *runtimeBindingArray) {
    auto *bindingObject = bindingVar.getDynamicObject();
    if (bindingObject == nullptr)
      continue;
    if (bindingObject->hasProperty("enabled") &&
        !valueIsTruthy(bindingObject->getProperty("enabled"))) {
      continue;
    }

    if (parseWidgetIdFromVar(bindingObject->getProperty("sourceWidgetId")) !=
        1001)
      continue;
    if (bindingObject->getProperty("eventKey").toString().trim() != "onClick")
      continue;

    auto *actions = bindingObject->getProperty("actions").getArray();
    if (actions == nullptr)
      continue;

    clickBindingExecuted = true;
    for (const auto &actionVar : *actions) {
      auto *action = actionVar.getDynamicObject();
      if (action == nullptr)
        continue;

      const auto kind =
          action->getProperty("kind").toString().trim().toLowerCase();
      if (kind == "setruntimeparam") {
        const auto requestedKey =
            action->getProperty("paramKey").toString().trim();
        if (requestedKey.isEmpty())
          continue;

        const auto resolvedKey =
            resolveRuntimeParamKey(simulatedParams, requestedKey);
        simulatedParams[resolvedKey] = action->getProperty("value");
      } else if (kind == "adjustruntimeparam") {
        const auto requestedKey =
            action->getProperty("paramKey").toString().trim();
        if (requestedKey.isEmpty())
          continue;

        const auto resolvedKey =
            resolveRuntimeParamKey(simulatedParams, requestedKey);
        const auto delta = static_cast<double>(action->getProperty("delta"));
        const auto current =
            simulatedParams.count(resolvedKey) > 0
                ? static_cast<double>(simulatedParams[resolvedKey])
                : 0.0;
        simulatedParams[resolvedKey] = current + delta;
      } else if (kind == "toggleruntimeparam") {
        const auto requestedKey =
            action->getProperty("paramKey").toString().trim();
        if (requestedKey.isEmpty())
          continue;

        const auto resolvedKey =
            resolveRuntimeParamKey(simulatedParams, requestedKey);
        const auto current = simulatedParams.count(resolvedKey) > 0
                                 ? valueIsTruthy(simulatedParams[resolvedKey])
                                 : false;
        simulatedParams[resolvedKey] = !current;
      }
    }
  }

  if (!clickBindingExecuted)
    return juce::Result::fail(
        "Smoke scenario missing onClick binding execution path.");

  bool sliderBindingEvaluated = false;
  for (const auto &bindingVar : *propertyBindingArray) {
    auto *bindingObject = bindingVar.getDynamicObject();
    if (bindingObject == nullptr)
      continue;
    if (bindingObject->hasProperty("enabled") &&
        !valueIsTruthy(bindingObject->getProperty("enabled"))) {
      continue;
    }

    if (parseWidgetIdFromVar(bindingObject->getProperty("targetWidgetId")) !=
        1002)
      continue;
    if (bindingObject->getProperty("targetProperty").toString().trim() !=
        "value")
      continue;

    const auto expression = bindingObject->getProperty("expression").toString();
    const auto evaluation =
        Gyeol::Runtime::PropertyBindingResolver::evaluateExpression(
            expression, simulatedParams);
    if (!evaluation.success)
      return juce::Result::fail(
          "Smoke scenario property binding evaluation failed: " +
          evaluation.error);

    if (std::abs(evaluation.value - 0.9) > 0.0001) {
      return juce::Result::fail(
          "Smoke scenario expected slider value 0.9 after onClick, got " +
          juce::String(evaluation.value, 6));
    }

    sliderBindingEvaluated = true;
    break;
  }

  if (!sliderBindingEvaluated)
    return juce::Result::fail(
        "Smoke scenario missing slider.value property binding.");

  const auto sourceText = report.generatedSourceFile.loadFileAsString();
  if (!sourceText.contains("initializeRuntimeBridge()") ||
      !sourceText.contains("dispatchRuntimeEvent(") ||
      !sourceText.contains("applyPropertyBindings()") ||
      !sourceText.contains("applyRuntimeAction(")) {
    return juce::Result::fail(
        "Smoke generated source missing runtime bridge functions.");
  }

  // Error-scenario smoke: malformed expression should fail export gracefully.
  const auto invalidDocument = makePhase6InvalidSmokeDocument();
  auto invalidOptions = options;
  invalidOptions.outputDirectory = outputDirectory.getSiblingFile(
      outputDirectory.getFileName() + "_invalid");

  Gyeol::Export::ExportReport invalidReport;
  const auto invalidResult = Gyeol::Export::exportToJuceComponent(
      invalidDocument, registry, invalidOptions, invalidReport);
  if (invalidResult.wasOk() || invalidReport.errorCount <= 0)
    return juce::Result::fail(
        "Smoke invalid export must fail with validation errors.");

  std::cout << "Phase6 smoke export directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << "Phase6 smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}

juce::Result runTeulPhase7ParitySmoke(const juce::StringArray &args) {
  juce::ignoreUnused(args);
  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");
  Teul::TVerificationParityReport report;
  const bool passed = Teul::runInitialG1StaticParitySmoke(*registry, report);
  if (report.artifactDirectory.isEmpty()) {
    return juce::Result::fail(
        "Teul parity smoke did not produce an artifact directory.");
  }
  const auto artifactDirectory = juce::File(report.artifactDirectory);
  if (!artifactDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul parity smoke artifact directory is missing: " +
        artifactDirectory.getFullPathName());
  }
  const auto summaryFile = artifactDirectory.getChildFile("parity-summary.txt");
  const auto exportReportFile =
      artifactDirectory.getChildFile("export-report.txt");
  const auto bundleFile = artifactDirectory.getChildFile("artifact-bundle.json");
  if (!summaryFile.existsAsFile() || !exportReportFile.existsAsFile() ||
      !bundleFile.existsAsFile()) {
    return juce::Result::fail(
        "Teul parity smoke is missing expected report artifacts.");
  }
  std::cout << "Teul Phase7 parity artifact directory: "
            << artifactDirectory.getFullPathName() << std::endl;
  std::cout << summaryFile.loadFileAsString() << std::endl;
  if (!passed) {
    const auto failureReason = report.failureReason.trim();
    return juce::Result::fail(
        failureReason.isNotEmpty()
            ? failureReason
            : juce::String(
                  "Teul parity smoke failed without a detailed reason."));
  }
  if (!report.importedDocumentLoaded || report.comparedChannels <= 0 ||
      report.totalSamples <= 0) {
    return juce::Result::fail(
        "Teul parity smoke did not complete a valid round-trip comparison.");
  }
  std::cout << "Teul Phase7 parity smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}
juce::Result runTeulPhase7ParityMatrix(const juce::StringArray &args) {
  juce::ignoreUnused(args);

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  Teul::TVerificationParitySuiteReport report;
  const bool passed = Teul::runRepresentativePrimaryParityMatrix(*registry, report);
  if (report.artifactDirectory.isEmpty()) {
    return juce::Result::fail(
        "Teul parity matrix did not produce an artifact directory.");
  }

  const auto artifactDirectory = juce::File(report.artifactDirectory);
  const auto summaryFile = artifactDirectory.getChildFile("matrix-summary.txt");
  if (!artifactDirectory.isDirectory() || !summaryFile.existsAsFile()) {
    return juce::Result::fail(
        "Teul parity matrix is missing the expected matrix summary artifact.");
  }

  std::cout << "Teul Phase7 parity matrix artifact directory: "
            << artifactDirectory.getFullPathName() << std::endl;
  std::cout << summaryFile.loadFileAsString() << std::endl;

  if (!passed) {
    return juce::Result::fail(
        "Teul representative parity matrix reported one or more failures.");
  }

  if (report.totalCaseCount <= 0 || report.failedCaseCount != 0) {
    return juce::Result::fail(
        "Teul parity matrix did not complete a valid representative run.");
  }

  std::cout << "Teul Phase7 parity matrix checks: PASS" << std::endl;
  return juce::Result::ok();
}


juce::Result runTeulPhase7StressSoak(const juce::StringArray &args) {
  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto iterationArg = argValue(args, "--iteration-count=");
  int iterationCount = 32;
  if (iterationArg.isNotEmpty()) {
    iterationCount = iterationArg.getIntValue();
    if (iterationCount <= 0) {
      return juce::Result::fail(
          "Stress/soak iteration count must be greater than zero.");
    }
  }

  Teul::TVerificationStressSuiteReport report;
  const bool passed =
      Teul::runRepresentativeStressSoakSuite(*registry, report, iterationCount);
  if (report.artifactDirectory.isEmpty()) {
    return juce::Result::fail(
        "Teul stress/soak run did not produce an artifact directory.");
  }

  const auto artifactDirectory = juce::File(report.artifactDirectory);
  const auto summaryFile = artifactDirectory.getChildFile("stress-summary.txt");
  const auto bundleFile = artifactDirectory.getChildFile("artifact-bundle.json");
  if (!artifactDirectory.isDirectory() || !summaryFile.existsAsFile() ||
      !bundleFile.existsAsFile()) {
    return juce::Result::fail(
        "Teul stress/soak run is missing expected suite artifacts.");
  }

  std::cout << "Teul Phase7 stress/soak artifact directory: "
            << artifactDirectory.getFullPathName() << std::endl;
  std::cout << summaryFile.loadFileAsString() << std::endl;

  if (!passed) {
    return juce::Result::fail(
        "Teul representative stress/soak suite reported one or more failures.");
  }

  if (report.totalCaseCount <= 0 || report.failedCaseCount != 0) {
    return juce::Result::fail(
        "Teul stress/soak suite did not complete a valid representative run.");
  }

  std::cout << "Teul Phase7 stress/soak checks: PASS" << std::endl;
  return juce::Result::ok();
}

juce::Result runTeulPhase7GoldenAudioRecord(const juce::StringArray &args) {
  juce::ignoreUnused(args);
  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  Teul::TVerificationGoldenAudioSuiteReport report;
  const bool passed = Teul::runRepresentativeGoldenAudioRecord(*registry, report);
  const auto baselineDirectory = juce::File(report.baselineDirectory);
  const auto summaryFile = baselineDirectory.getChildFile("golden-suite-summary.txt");
  const auto bundleFile = baselineDirectory.getChildFile("artifact-bundle.json");
  if (!baselineDirectory.isDirectory() || !summaryFile.existsAsFile() ||
      !bundleFile.existsAsFile()) {
    return juce::Result::fail(
        "Teul golden audio record did not produce expected baseline artifacts.");
  }

  std::cout << "Teul Phase7 golden record baseline directory: "
            << baselineDirectory.getFullPathName() << std::endl;
  std::cout << summaryFile.loadFileAsString() << std::endl;

  if (!passed || report.failedCaseCount != 0)
    return juce::Result::fail("Teul representative golden audio record reported one or more failures.");

  std::cout << "Teul Phase7 golden record checks: PASS" << std::endl;
  return juce::Result::ok();
}

juce::Result runTeulPhase7GoldenAudioVerify(const juce::StringArray &args) {
  juce::ignoreUnused(args);
  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  Teul::TVerificationGoldenAudioSuiteReport report;
  const bool passed = Teul::runRepresentativeGoldenAudioVerify(*registry, report);
  const auto artifactDirectory = juce::File(report.artifactDirectory);
  const auto summaryFile = artifactDirectory.getChildFile("golden-suite-summary.txt");
  const auto bundleFile = artifactDirectory.getChildFile("artifact-bundle.json");
  if (!artifactDirectory.isDirectory() || !summaryFile.existsAsFile() ||
      !bundleFile.existsAsFile()) {
    return juce::Result::fail(
        "Teul golden audio verify did not produce expected verification artifacts.");
  }

  std::cout << "Teul Phase7 golden verify artifact directory: "
            << artifactDirectory.getFullPathName() << std::endl;
  std::cout << summaryFile.loadFileAsString() << std::endl;

  if (!passed || report.failedCaseCount != 0)
    return juce::Result::fail("Teul representative golden audio verify reported one or more failures.");

  std::cout << "Teul Phase7 golden verify checks: PASS" << std::endl;
  return juce::Result::ok();
}


juce::Result runTeulPhase7CompiledRuntimeParity(const juce::StringArray &args) {
  juce::ignoreUnused(args);
  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  Teul::TVerificationCompiledParitySuiteReport report;
  const bool passed =
      Teul::runRepresentativeCompiledRuntimeParity(*registry, report);
  if (report.artifactDirectory.isEmpty()) {
    return juce::Result::fail(
        "Teul compiled runtime parity did not produce an artifact directory.");
  }

  const auto artifactDirectory = juce::File(report.artifactDirectory);
  const auto summaryFile =
      artifactDirectory.getChildFile("compiled-runtime-parity-summary.txt");
  const auto bundleFile = artifactDirectory.getChildFile("artifact-bundle.json");
  if (!artifactDirectory.isDirectory() || !summaryFile.existsAsFile() ||
      !bundleFile.existsAsFile()) {
    return juce::Result::fail(
        "Teul compiled runtime parity is missing expected suite artifacts.");
  }

  std::cout << "Teul Phase7 compiled runtime parity artifact directory: "
            << artifactDirectory.getFullPathName() << std::endl;
  std::cout << summaryFile.loadFileAsString() << std::endl;

  if (!passed || report.failedCaseCount != 0) {
    return juce::Result::fail(
        report.failureReason.isNotEmpty()
            ? report.failureReason
            : juce::String(
                  "Teul representative compiled runtime parity reported one or more failures."));
  }

  if (report.totalCaseCount <= 0) {
    return juce::Result::fail(
        "Teul compiled runtime parity did not complete a valid representative run.");
  }

  std::cout << "Teul Phase7 compiled runtime parity checks: PASS"
            << std::endl;
  return juce::Result::ok();
}
juce::Result runTeulPhase8PatchPresetSmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulPatchPresetSmoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  if (!outputDirectory.createDirectory() && !outputDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul patch preset smoke output directory could not be created.");
  }

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto assetSource = outputDirectory.getChildFile("PatchPresetSmokeImpulse.wav");
  if (!assetSource.replaceWithText("teul patch preset smoke asset", false,
                                   false, "\r\n")) {
    return juce::Result::fail(
        "Failed to create patch preset smoke asset file.");
  }

  auto sourceDocument = makeTeulPhase5SmokeDocument(*registry, assetSource);
  if (sourceDocument.nodes.size() < 3) {
    return juce::Result::fail(
        "Teul patch preset smoke graph does not contain enough nodes.");
  }

  Teul::TFrameRegion frame;
  frame.frameId = sourceDocument.allocFrameId();
  frame.frameUuid = juce::Uuid().toString();
  frame.title = "Smoke Patch Preset";
  frame.logicalGroup = true;
  frame.membershipExplicit = true;
  frame.colorArgb = 0x334d8bf7;

  juce::Rectangle<float> memberBounds;
  bool hasMemberBounds = false;
  const int memberCount = juce::jmin(3, (int)sourceDocument.nodes.size());
  for (int index = 0; index < memberCount; ++index) {
    const auto &node = sourceDocument.nodes[(size_t)index];
    const juce::Rectangle<float> nodeRect(node.x, node.y, 160.0f, 90.0f);
    memberBounds = hasMemberBounds ? memberBounds.getUnion(nodeRect) : nodeRect;
    hasMemberBounds = true;
  }
  if (!hasMemberBounds) {
    return juce::Result::fail(
        "Teul patch preset smoke group could not capture members.");
  }
  memberBounds = memberBounds.expanded(24.0f, 24.0f);
  frame.x = memberBounds.getX();
  frame.y = memberBounds.getY();
  frame.width = memberBounds.getWidth();
  frame.height = memberBounds.getHeight();
  sourceDocument.frames.push_back(frame);
  for (int index = 0; index < memberCount; ++index) {
    sourceDocument.addNodeToFrameExclusive(
        sourceDocument.nodes[(size_t)index].nodeId, frame.frameId);
  }

  const auto presetFile = outputDirectory.getChildFile("smoke_patch_preset")
                              .withFileExtension(
                                  Teul::TPatchPresetIO::fileExtension());
  Teul::TPatchPresetSummary savedSummary;
  const auto saveResult = Teul::TPatchPresetIO::saveFrameToFile(
      sourceDocument, frame.frameId, presetFile, &savedSummary);
  if (saveResult.failed())
    return saveResult;

  Teul::TGraphDocument loadedPresetDocument;
  Teul::TPatchPresetSummary loadedSummary;
  const auto loadResult = Teul::TPatchPresetIO::loadFromFile(
      loadedPresetDocument, loadedSummary, presetFile);
  if (loadResult.failed())
    return loadResult;

  if ((int)loadedPresetDocument.nodes.size() != savedSummary.nodeCount ||
      (int)loadedPresetDocument.connections.size() !=
          savedSummary.connectionCount ||
      loadedPresetDocument.frames.size() != 1) {
    return juce::Result::fail(
        "Teul patch preset smoke load changed the saved preset topology.");
  }

  Teul::TGraphDocument insertedDocument;
  std::vector<Teul::NodeId> insertedNodeIds;
  int insertedFrameId = 0;
  Teul::TPatchPresetSummary insertedSummary;
  const auto insertResult = Teul::TPatchPresetIO::insertFromFile(
      insertedDocument, presetFile, {120.0f, 160.0f}, &insertedNodeIds,
      &insertedFrameId, &insertedSummary);
  if (insertResult.failed())
    return insertResult;

  if ((int)insertedNodeIds.size() != savedSummary.nodeCount ||
      (int)insertedDocument.connections.size() != savedSummary.connectionCount ||
      insertedFrameId == 0 || insertedDocument.frames.size() != 1) {
    return juce::Result::fail(
        "Teul patch preset smoke insert produced an unexpected graph shape.");
  }

  const auto *insertedFrame = insertedDocument.findFrame(insertedFrameId);
  if (insertedFrame == nullptr ||
      (int)insertedFrame->memberNodeIds.size() != savedSummary.nodeCount) {
    return juce::Result::fail(
        "Inserted patch preset group did not restore logical membership.");
  }

  const auto summaryFile = outputDirectory.getChildFile("patch-preset-summary.txt");
  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  const auto summaryText =
      juce::String("savedNodes=") + juce::String(savedSummary.nodeCount) +
      "\r\n" +
      "savedConnections=" + juce::String(savedSummary.connectionCount) +
      "\r\n" +
      "insertedNodes=" + juce::String((int)insertedNodeIds.size()) +
      "\r\n" +
      "insertedFrameId=" + juce::String(insertedFrameId) + "\r\n" +
      "presetFile=" + presetFile.getFullPathName() + "\r\n" +
      "passed=true\r\n";
  if (!summaryFile.replaceWithText(summaryText, false, false, "\r\n")) {
    return juce::Result::fail(
        "Teul patch preset smoke could not write its summary file.");
  }

  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("patchPreset", outputDirectory, presetFile));
  files.add(makeArtifactFileEntry("summary", outputDirectory, summaryFile));
  auto *bundleRoot = new juce::DynamicObject();
  bundleRoot->setProperty("kind", "teul-verification-artifact-bundle");
  bundleRoot->setProperty("scope", "patch-preset-smoke");
  bundleRoot->setProperty("passed", true);
  bundleRoot->setProperty("artifactDirectory", outputDirectory.getFullPathName());
  bundleRoot->setProperty("presetName", savedSummary.presetName);
  bundleRoot->setProperty("savedNodeCount", savedSummary.nodeCount);
  bundleRoot->setProperty("savedConnectionCount", savedSummary.connectionCount);
  bundleRoot->setProperty("insertedNodeCount", (int)insertedNodeIds.size());
  bundleRoot->setProperty("files", juce::var(files));
  if (!writeJsonArtifact(bundleFile, juce::var(bundleRoot))) {
    return juce::Result::fail(
        "Teul patch preset smoke could not write its artifact bundle.");
  }

  std::cout << "Teul Phase8 patch preset smoke directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << summaryText << std::endl;
  std::cout << "Teul Phase8 patch preset smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}

juce::Result runTeulPhase8StatePresetSmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulStatePresetSmoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  if (!outputDirectory.createDirectory() && !outputDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul state preset smoke output directory could not be created.");
  }

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto assetSource =
      outputDirectory.getChildFile("StatePresetSmokeImpulse.wav");
  if (!assetSource.replaceWithText("teul state preset smoke asset", false,
                                   false, "\r\n")) {
    return juce::Result::fail(
        "Failed to create state preset smoke asset file.");
  }

  auto document = makeTeulPhase5SmokeDocument(*registry, assetSource);
  auto *carrierNode = findTeulNodeByLabel(document, "Carrier");
  auto *ampNode = findTeulNodeByLabel(document, "Amp");
  auto *cvNode = findTeulNodeByLabel(document, "CV");
  if (carrierNode == nullptr || ampNode == nullptr || cvNode == nullptr) {
    return juce::Result::fail(
        "Teul state preset smoke graph could not resolve its target nodes.");
  }

  const auto expectedFrequency = (double)carrierNode->params["frequency"];
  const auto expectedCarrierGain = (double)carrierNode->params["gain"];
  const auto expectedAmpGain = (double)ampNode->params["gain"];
  const auto expectedCvValue = (double)cvNode->params["value"];
  const auto expectedAmpBypassed = ampNode->bypassed;

  const auto presetFile = outputDirectory.getChildFile("smoke_state_preset")
                              .withFileExtension(
                                  Teul::TStatePresetIO::fileExtension());
  Teul::TStatePresetSummary savedSummary;
  const auto saveResult = Teul::TStatePresetIO::saveDocumentToFile(
      document, presetFile, &savedSummary);
  if (saveResult.failed())
    return saveResult;

  carrierNode->params["frequency"] = 110.0;
  carrierNode->params["gain"] = 0.10;
  ampNode->params["gain"] = 0.20;
  ampNode->bypassed = !expectedAmpBypassed;
  cvNode->params["value"] = 0.15;

  Teul::TStatePresetApplyReport applyReport;
  const auto applyResult =
      Teul::TStatePresetIO::applyToDocument(document, presetFile, &applyReport);
  if (applyResult.failed())
    return applyResult;

  carrierNode = findTeulNodeByLabel(document, "Carrier");
  ampNode = findTeulNodeByLabel(document, "Amp");
  cvNode = findTeulNodeByLabel(document, "CV");
  if (carrierNode == nullptr || ampNode == nullptr || cvNode == nullptr) {
    return juce::Result::fail(
        "Teul state preset smoke graph lost its target nodes after apply.");
  }

  const auto restoredFrequency = (double)carrierNode->params["frequency"];
  const auto restoredCarrierGain = (double)carrierNode->params["gain"];
  const auto restoredAmpGain = (double)ampNode->params["gain"];
  const auto restoredCvValue = (double)cvNode->params["value"];
  if (std::abs(restoredFrequency - expectedFrequency) > 1.0e-9 ||
      std::abs(restoredCarrierGain - expectedCarrierGain) > 1.0e-9 ||
      std::abs(restoredAmpGain - expectedAmpGain) > 1.0e-9 ||
      std::abs(restoredCvValue - expectedCvValue) > 1.0e-9 ||
      ampNode->bypassed != expectedAmpBypassed) {
    return juce::Result::fail(
        "Teul state preset smoke failed to restore the saved node state.");
  }

  const auto summaryFile =
      outputDirectory.getChildFile("state-preset-summary.txt");
  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  const auto summaryText =
      juce::String("preset=") + savedSummary.presetName + "\r\n" +
      "targetGraph=" + savedSummary.targetGraphName + "\r\n" +
      "savedNodeStates=" + juce::String(savedSummary.nodeStateCount) +
      "\r\n" + "savedParamValues=" +
      juce::String(savedSummary.paramValueCount) + "\r\n" +
      "appliedNodes=" + juce::String(applyReport.appliedNodeCount) +
      "\r\n" + "skippedNodes=" +
      juce::String(applyReport.skippedNodeCount) + "\r\n" +
      "appliedParamValues=" +
      juce::String(applyReport.appliedParamValueCount) + "\r\n" +
      "presetFile=" + presetFile.getFullPathName() + "\r\n" +
      "passed=true\r\n";
  if (!summaryFile.replaceWithText(summaryText, false, false, "\r\n")) {
    return juce::Result::fail(
        "Teul state preset smoke could not write its summary file.");
  }

  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("statePreset", outputDirectory, presetFile));
  files.add(makeArtifactFileEntry("summary", outputDirectory, summaryFile));
  auto *bundleRoot = new juce::DynamicObject();
  bundleRoot->setProperty("kind", "teul-verification-artifact-bundle");
  bundleRoot->setProperty("scope", "state-preset-smoke");
  bundleRoot->setProperty("passed", true);
  bundleRoot->setProperty("artifactDirectory", outputDirectory.getFullPathName());
  bundleRoot->setProperty("presetName", savedSummary.presetName);
  bundleRoot->setProperty("savedNodeStateCount", savedSummary.nodeStateCount);
  bundleRoot->setProperty("savedParamValueCount", savedSummary.paramValueCount);
  bundleRoot->setProperty("appliedNodeCount", applyReport.appliedNodeCount);
  bundleRoot->setProperty("appliedParamValueCount",
                          applyReport.appliedParamValueCount);
  bundleRoot->setProperty("files", juce::var(files));
  if (!writeJsonArtifact(bundleFile, juce::var(bundleRoot))) {
    return juce::Result::fail(
        "Teul state preset smoke could not write its artifact bundle.");
  }

  std::cout << "Teul Phase8 state preset smoke directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << summaryText << std::endl;
  std::cout << "Teul Phase8 state preset smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}


juce::Result runTeulPhase8AutosaveRecoverySmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulAutosaveRecoverySmoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  if (!outputDirectory.createDirectory() && !outputDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul autosave recovery smoke output directory could not be created.");
  }

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto assetSource =
      outputDirectory.getChildFile("AutosaveRecoverySmokeImpulse.wav");
  if (!assetSource.replaceWithText("teul autosave recovery smoke asset", false,
                                   false, "\r\n")) {
    return juce::Result::fail(
        "Failed to create autosave recovery smoke asset file.");
  }

  auto sourceDocument = makeTeulPhase5SmokeDocument(*registry, assetSource);
  sourceDocument.meta.name = "Autosave Recovery Smoke";
  auto *carrierNode = findTeulNodeByLabel(sourceDocument, "Carrier");
  auto *ampNode = findTeulNodeByLabel(sourceDocument, "Amp");
  if (carrierNode == nullptr || ampNode == nullptr) {
    return juce::Result::fail(
        "Teul autosave recovery smoke graph could not resolve its target nodes.");
  }

  carrierNode->params["frequency"] = 176.0;
  ampNode->params["gain"] = 0.42;
  ampNode->bypassed = true;

  const auto expectedFrequency = (double)carrierNode->params["frequency"];
  const auto expectedAmpGain = (double)ampNode->params["gain"];
  const auto expectedAmpBypassed = ampNode->bypassed;

  const auto sessionDirectory = outputDirectory.getChildFile("Session");
  if (!sessionDirectory.createDirectory() && !sessionDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul autosave recovery smoke session directory could not be created.");
  }

  const auto autosaveFile = sessionDirectory.getChildFile("autosave-teul.teul");
  const auto stateFile =
      sessionDirectory.getChildFile("autosave-session-state.json");
  if (!Teul::TFileIo::saveToFile(sourceDocument, autosaveFile)) {
    return juce::Result::fail(
        "Teul autosave recovery smoke could not write its autosave file.");
  }
  if (!writeSessionStateArtifact(stateFile, false)) {
    return juce::Result::fail(
        "Teul autosave recovery smoke could not write its recovery marker.");
  }

  bool cleanShutdown = true;
  if (!readSessionStateArtifact(stateFile, cleanShutdown) || cleanShutdown) {
    return juce::Result::fail(
        "Teul autosave recovery smoke expected an unclean shutdown marker.");
  }

  Teul::TGraphDocument restoredDocument;
  if (!Teul::TFileIo::loadFromFile(restoredDocument, autosaveFile)) {
    return juce::Result::fail(
        "Teul autosave recovery smoke could not restore the autosaved graph.");
  }

  carrierNode = findTeulNodeByLabel(restoredDocument, "Carrier");
  ampNode = findTeulNodeByLabel(restoredDocument, "Amp");
  if (carrierNode == nullptr || ampNode == nullptr) {
    return juce::Result::fail(
        "Teul autosave recovery smoke restored graph is missing expected nodes.");
  }

  if (restoredDocument.meta.name != sourceDocument.meta.name ||
      restoredDocument.nodes.size() != sourceDocument.nodes.size() ||
      restoredDocument.connections.size() != sourceDocument.connections.size() ||
      std::abs((double)carrierNode->params["frequency"] - expectedFrequency) >
          1.0e-9 ||
      std::abs((double)ampNode->params["gain"] - expectedAmpGain) > 1.0e-9 ||
      ampNode->bypassed != expectedAmpBypassed) {
    return juce::Result::fail(
        "Teul autosave recovery smoke did not restore the saved document state.");
  }

  if (!writeSessionStateArtifact(stateFile, true)) {
    return juce::Result::fail(
        "Teul autosave recovery smoke could not update its clean shutdown marker.");
  }

  cleanShutdown = false;
  if (!readSessionStateArtifact(stateFile, cleanShutdown) || !cleanShutdown) {
    return juce::Result::fail(
        "Teul autosave recovery smoke expected a clean shutdown marker after finalize.");
  }

  const auto summaryFile =
      outputDirectory.getChildFile("autosave-recovery-summary.txt");
  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  const auto summaryText =
      juce::String("graph=") + restoredDocument.meta.name + "\r\n" +
      "savedNodes=" + juce::String((int)sourceDocument.nodes.size()) +
      "\r\n" + "savedConnections=" +
      juce::String((int)sourceDocument.connections.size()) + "\r\n" +
      "restoredNodes=" + juce::String((int)restoredDocument.nodes.size()) +
      "\r\n" + "cleanShutdownMarker=" +
      juce::String(cleanShutdown ? "true" : "false") + "\r\n" +
      "autosaveFile=" + autosaveFile.getFullPathName() + "\r\n" +
      "passed=true\r\n";
  if (!summaryFile.replaceWithText(summaryText, false, false, "\r\n")) {
    return juce::Result::fail(
        "Teul autosave recovery smoke could not write its summary file.");
  }

  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("autosave", outputDirectory, autosaveFile));
  files.add(makeArtifactFileEntry("sessionState", outputDirectory, stateFile));
  files.add(makeArtifactFileEntry("summary", outputDirectory, summaryFile));
  auto *bundleRoot = new juce::DynamicObject();
  bundleRoot->setProperty("kind", "teul-verification-artifact-bundle");
  bundleRoot->setProperty("scope", "autosave-recovery-smoke");
  bundleRoot->setProperty("passed", true);
  bundleRoot->setProperty("artifactDirectory",
                          outputDirectory.getFullPathName());
  bundleRoot->setProperty("savedNodeCount", (int)sourceDocument.nodes.size());
  bundleRoot->setProperty("savedConnectionCount",
                          (int)sourceDocument.connections.size());
  bundleRoot->setProperty("restoredNodeCount",
                          (int)restoredDocument.nodes.size());
  bundleRoot->setProperty("files", juce::var(files));
  if (!writeJsonArtifact(bundleFile, juce::var(bundleRoot))) {
    return juce::Result::fail(
        "Teul autosave recovery smoke could not write its artifact bundle.");
  }

  std::cout << "Teul Phase8 autosave recovery smoke directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << summaryText << std::endl;
  std::cout << "Teul Phase8 autosave recovery smoke checks: PASS"
            << std::endl;
  return juce::Result::ok();
}


juce::Result runTeulPhase8CompatibilitySmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulCompatibilitySmoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  if (!outputDirectory.createDirectory() && !outputDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul compatibility smoke output directory could not be created.");
  }

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto assetSource = outputDirectory.getChildFile("CompatibilitySmokeImpulse.wav");
  if (!assetSource.replaceWithText("teul compatibility smoke asset", false,
                                   false, "\r\n")) {
    return juce::Result::fail(
        "Failed to create compatibility smoke asset file.");
  }

  auto sourceDocument = makeTeulPhase5SmokeDocument(*registry, assetSource);
  sourceDocument.meta.name = "Compatibility Smoke";
  if (sourceDocument.nodes.size() < 3) {
    return juce::Result::fail(
        "Teul compatibility smoke graph does not contain enough nodes.");
  }

  Teul::TFrameRegion frame;
  frame.frameId = sourceDocument.allocFrameId();
  frame.frameUuid = juce::Uuid().toString();
  frame.title = "Compatibility Patch";
  frame.logicalGroup = true;
  frame.membershipExplicit = true;
  frame.colorArgb = 0x334d8bf7;

  juce::Rectangle<float> memberBounds;
  bool hasMemberBounds = false;
  for (int index = 0; index < 3; ++index) {
    const auto &node = sourceDocument.nodes[(size_t)index];
    const juce::Rectangle<float> nodeRect(node.x, node.y, 160.0f, 90.0f);
    memberBounds = hasMemberBounds ? memberBounds.getUnion(nodeRect) : nodeRect;
    hasMemberBounds = true;
  }
  if (!hasMemberBounds)
    return juce::Result::fail("Teul compatibility smoke could not compute frame bounds.");
  memberBounds = memberBounds.expanded(24.0f, 24.0f);
  frame.x = memberBounds.getX();
  frame.y = memberBounds.getY();
  frame.width = memberBounds.getWidth();
  frame.height = memberBounds.getHeight();
  sourceDocument.frames.push_back(frame);
  for (int index = 0; index < 3; ++index) {
    sourceDocument.addNodeToFrameExclusive(sourceDocument.nodes[(size_t)index].nodeId,
                                           frame.frameId);
  }

  auto makeLegacyGraphJson = [](const Teul::TGraphDocument &document) {
    const auto current = Teul::TSerializer::toJson(document);
    const auto *root = current.getDynamicObject();
    auto *legacyRoot = new juce::DynamicObject();
    legacyRoot->setProperty("schemaVersion", 1);
    legacyRoot->setProperty("nextNodeId", root->getProperty("next_node_id"));
    legacyRoot->setProperty("nextPortId", root->getProperty("next_port_id"));
    legacyRoot->setProperty("nextConnectionId", root->getProperty("next_conn_id"));
    legacyRoot->setProperty("nextFrameId", root->getProperty("next_frame_id"));
    legacyRoot->setProperty("nextBookmarkId",
                            root->getProperty("next_bookmark_id"));

    auto *legacyMeta = new juce::DynamicObject();
    if (auto *meta = root->getProperty("meta").getDynamicObject()) {
      legacyMeta->setProperty("name", meta->getProperty("name"));
      legacyMeta->setProperty("canvasOffsetX",
                              meta->getProperty("canvas_offset_x"));
      legacyMeta->setProperty("canvasOffsetY",
                              meta->getProperty("canvas_offset_y"));
      legacyMeta->setProperty("canvasZoom", meta->getProperty("canvas_zoom"));
      legacyMeta->setProperty("sampleRate", meta->getProperty("sample_rate"));
      legacyMeta->setProperty("blockSize", meta->getProperty("block_size"));
    }
    legacyRoot->setProperty("graphMeta", legacyMeta);

    juce::Array<juce::var> legacyNodes;
    if (auto *nodes = root->getProperty("nodes").getArray()) {
      for (const auto &nodeVar : *nodes) {
        auto *legacyNode = new juce::DynamicObject();
        if (auto *node = nodeVar.getDynamicObject()) {
          legacyNode->setProperty("nodeId", node->getProperty("id"));
          legacyNode->setProperty("typeKey", node->getProperty("type"));
          legacyNode->setProperty("posX", node->getProperty("x"));
          legacyNode->setProperty("posY", node->getProperty("y"));
          legacyNode->setProperty("collapsed",
                                  node->getProperty("collapsed"));
          legacyNode->setProperty("isBypassed",
                                  node->getProperty("bypassed"));
          legacyNode->setProperty("label", node->getProperty("label"));
          legacyNode->setProperty("colorTag",
                                  node->getProperty("color_tag"));
          legacyNode->setProperty("paramValues",
                                  node->getProperty("params"));

          juce::Array<juce::var> legacyPorts;
          if (auto *ports = node->getProperty("ports").getArray()) {
            for (const auto &portVar : *ports) {
              auto *legacyPort = new juce::DynamicObject();
              if (auto *port = portVar.getDynamicObject()) {
                legacyPort->setProperty("portId", port->getProperty("id"));
                legacyPort->setProperty("portDirection",
                                        port->getProperty("direction"));
                legacyPort->setProperty("dataType", port->getProperty("type"));
                legacyPort->setProperty("portName", port->getProperty("name"));
                legacyPort->setProperty("channelIndex",
                                        port->getProperty("channel_index"));
              }
              legacyPorts.add(juce::var(legacyPort));
            }
          }
          legacyNode->setProperty("port_list", legacyPorts);
        }
        legacyNodes.add(juce::var(legacyNode));
      }
    }
    legacyRoot->setProperty("node_list", legacyNodes);

    juce::Array<juce::var> legacyConnections;
    if (auto *connections = root->getProperty("connections").getArray()) {
      for (const auto &connectionVar : *connections) {
        auto *legacyConnection = new juce::DynamicObject();
        if (auto *connection = connectionVar.getDynamicObject()) {
          legacyConnection->setProperty("connectionId",
                                        connection->getProperty("id"));
          legacyConnection->setProperty("source",
                                        connection->getProperty("from"));
          legacyConnection->setProperty("target",
                                        connection->getProperty("to"));
        }
        legacyConnections.add(juce::var(legacyConnection));
      }
    }
    legacyRoot->setProperty("connection_list", legacyConnections);

    juce::Array<juce::var> legacyFrames;
    if (auto *frames = root->getProperty("frames").getArray()) {
      for (const auto &frameVar : *frames) {
        auto *legacyFrame = new juce::DynamicObject();
        if (auto *frameObject = frameVar.getDynamicObject()) {
          legacyFrame->setProperty("frameId", frameObject->getProperty("id"));
          legacyFrame->setProperty("frameUuid",
                                   frameObject->getProperty("uuid"));
          legacyFrame->setProperty("title", frameObject->getProperty("title"));
          legacyFrame->setProperty("posX", frameObject->getProperty("x"));
          legacyFrame->setProperty("posY", frameObject->getProperty("y"));
          legacyFrame->setProperty("width", frameObject->getProperty("width"));
          legacyFrame->setProperty("height",
                                   frameObject->getProperty("height"));
          legacyFrame->setProperty("colorArgb",
                                   frameObject->getProperty("color_argb"));
          legacyFrame->setProperty("isCollapsed",
                                   frameObject->getProperty("collapsed"));
          legacyFrame->setProperty("isLocked",
                                   frameObject->getProperty("locked"));
          legacyFrame->setProperty("logicalGroup",
                                   frameObject->getProperty("logical_group"));
          legacyFrame->setProperty(
              "membershipExplicit",
              frameObject->getProperty("membership_explicit"));
          legacyFrame->setProperty("memberNodeIds",
                                   frameObject->getProperty("member_node_ids"));
        }
        legacyFrames.add(juce::var(legacyFrame));
      }
    }
    legacyRoot->setProperty("frame_regions", legacyFrames);

    juce::Array<juce::var> legacyBookmarks;
    if (auto *bookmarks = root->getProperty("bookmarks").getArray()) {
      for (const auto &bookmarkVar : *bookmarks) {
        auto *legacyBookmark = new juce::DynamicObject();
        if (auto *bookmark = bookmarkVar.getDynamicObject()) {
          legacyBookmark->setProperty("bookmarkId",
                                      bookmark->getProperty("id"));
          legacyBookmark->setProperty("name", bookmark->getProperty("name"));
          legacyBookmark->setProperty("focusX",
                                      bookmark->getProperty("focus_x"));
          legacyBookmark->setProperty("focusY",
                                      bookmark->getProperty("focus_y"));
          legacyBookmark->setProperty("zoom", bookmark->getProperty("zoom"));
          legacyBookmark->setProperty("colorTag",
                                      bookmark->getProperty("color_tag"));
        }
        legacyBookmarks.add(juce::var(legacyBookmark));
      }
    }
    legacyRoot->setProperty("bookmark_list", legacyBookmarks);
    return juce::var(legacyRoot);
  };

  const auto legacyDocumentJson = makeLegacyGraphJson(sourceDocument);
  Teul::TGraphDocument restoredLegacyDocument;
  Teul::TSchemaMigrationReport legacyDocumentMigration;
  if (!Teul::TSerializer::fromJson(restoredLegacyDocument, legacyDocumentJson,
                                   &legacyDocumentMigration)) {
    return juce::Result::fail(
        "Teul compatibility smoke failed to restore the legacy document payload.");
  }
  auto *legacyCarrierNode = findTeulNodeByLabel(restoredLegacyDocument, "Carrier");
  if (legacyCarrierNode == nullptr ||
      restoredLegacyDocument.nodes.size() != sourceDocument.nodes.size() ||
      restoredLegacyDocument.connections.size() !=
          sourceDocument.connections.size() ||
      restoredLegacyDocument.frames.size() != sourceDocument.frames.size() ||
      !legacyDocumentMigration.migrated ||
      !legacyDocumentMigration.usedLegacyAliases ||
      legacyDocumentMigration.degraded ||
      legacyDocumentMigration.warnings.isEmpty() ||
      legacyDocumentMigration.appliedSteps.size() != 2 ||
      !legacyDocumentMigration.appliedSteps.contains("document:v1->v2") ||
      !legacyDocumentMigration.appliedSteps.contains("document:v2->v3")) {
    return juce::Result::fail(
        "Teul compatibility smoke legacy document restore mismatch.");
  }

  const auto currentPatchFile = outputDirectory.getChildFile("current_patch")
                                    .withFileExtension(
                                        Teul::TPatchPresetIO::fileExtension());
  Teul::TPatchPresetSummary currentPatchSummary;
  const auto patchSaveResult = Teul::TPatchPresetIO::saveFrameToFile(
      sourceDocument, frame.frameId, currentPatchFile, &currentPatchSummary);
  if (patchSaveResult.failed())
    return patchSaveResult;

  Teul::TGraphDocument currentPatchDocument;
  Teul::TPatchPresetSummary currentPatchLoadSummary;
  Teul::TPatchPresetLoadReport currentPatchLoadReport;
  const auto currentPatchLoadResult = Teul::TPatchPresetIO::loadFromFile(
      currentPatchDocument, currentPatchLoadSummary, currentPatchFile,
      &currentPatchLoadReport);
  if (currentPatchLoadResult.failed())
    return currentPatchLoadResult;

  const auto currentPatchJson = juce::JSON::parse(currentPatchFile);
  if (!currentPatchJson.isObject()) {
    return juce::Result::fail(
        "Teul compatibility smoke could not parse the current patch preset.");
  }
  const auto *currentPatchRoot = currentPatchJson.getDynamicObject();
  auto *legacyPatchRoot = new juce::DynamicObject();
  legacyPatchRoot->setProperty("format", "teul.patch_preset");
  legacyPatchRoot->setProperty("presetName",
                               currentPatchRoot->getProperty("preset_name"));
  legacyPatchRoot->setProperty(
      "sourceFrameUuid", currentPatchRoot->getProperty("source_frame_uuid"));
  auto *legacyPatchSummary = new juce::DynamicObject();
  legacyPatchSummary->setProperty("presetName", currentPatchSummary.presetName);
  legacyPatchSummary->setProperty("sourceFrameUuid",
                                  currentPatchSummary.sourceFrameUuid);
  legacyPatchSummary->setProperty("nodeCount", currentPatchSummary.nodeCount);
  legacyPatchSummary->setProperty("connectionCount",
                                  currentPatchSummary.connectionCount);
  legacyPatchSummary->setProperty("frameCount", currentPatchSummary.frameCount);
  legacyPatchSummary->setProperty("boundsX",
                                  currentPatchSummary.bounds.getX());
  legacyPatchSummary->setProperty("boundsY",
                                  currentPatchSummary.bounds.getY());
  legacyPatchSummary->setProperty("boundsWidth",
                                  currentPatchSummary.bounds.getWidth());
  legacyPatchSummary->setProperty("boundsHeight",
                                  currentPatchSummary.bounds.getHeight());
  legacyPatchRoot->setProperty("presetSummary", legacyPatchSummary);
  legacyPatchRoot->setProperty("graphPayload",
                               makeLegacyGraphJson(currentPatchDocument));

  const auto legacyPatchFile = outputDirectory.getChildFile("legacy_patch")
                                   .withFileExtension(
                                       Teul::TPatchPresetIO::fileExtension());
  if (!writeJsonArtifact(legacyPatchFile, juce::var(legacyPatchRoot))) {
    return juce::Result::fail(
        "Teul compatibility smoke could not write the legacy patch preset.");
  }

  Teul::TGraphDocument loadedLegacyPatchDocument;
  Teul::TPatchPresetSummary loadedLegacyPatchSummary;
  Teul::TPatchPresetLoadReport legacyPatchLoadReport;
  const auto patchLoadResult = Teul::TPatchPresetIO::loadFromFile(
      loadedLegacyPatchDocument, loadedLegacyPatchSummary, legacyPatchFile,
      &legacyPatchLoadReport);
  if (patchLoadResult.failed())
    return patchLoadResult;

  Teul::TGraphDocument insertedPatchDocument;
  std::vector<Teul::NodeId> insertedNodeIds;
  int insertedFrameId = 0;
  const auto patchInsertResult = Teul::TPatchPresetIO::insertFromFile(
      insertedPatchDocument, legacyPatchFile, {120.0f, 120.0f},
      &insertedNodeIds, &insertedFrameId, nullptr);
  if (patchInsertResult.failed())
    return patchInsertResult;

  if ((int)loadedLegacyPatchDocument.nodes.size() !=
          currentPatchSummary.nodeCount ||
      (int)insertedNodeIds.size() != currentPatchSummary.nodeCount ||
      insertedFrameId == 0 || currentPatchLoadReport.migrated ||
      currentPatchLoadReport.usedLegacyAliases ||
      !legacyPatchLoadReport.migrated ||
      !legacyPatchLoadReport.usedLegacyAliases ||
      legacyPatchLoadReport.degraded ||
      legacyPatchLoadReport.warnings.isEmpty() ||
      legacyPatchLoadReport.appliedSteps.size() != 1 ||
      !legacyPatchLoadReport.appliedSteps.contains("patch:v1->v2") ||
      !legacyPatchLoadReport.graphMigration.migrated ||
      !legacyPatchLoadReport.graphMigration.usedLegacyAliases ||
      legacyPatchLoadReport.graphMigration.degraded ||
      legacyPatchLoadReport.graphMigration.warnings.isEmpty()) {
    return juce::Result::fail(
        "Teul compatibility smoke legacy patch preset restore mismatch.");
  }

  const auto currentStateFile = outputDirectory.getChildFile("current_state")
                                    .withFileExtension(
                                        Teul::TStatePresetIO::fileExtension());
  Teul::TStatePresetSummary currentStateSummary;
  const auto stateSaveResult = Teul::TStatePresetIO::saveDocumentToFile(
      sourceDocument, currentStateFile, &currentStateSummary);
  if (stateSaveResult.failed())
    return stateSaveResult;

  const auto currentStateJson = juce::JSON::parse(currentStateFile);
  if (!currentStateJson.isObject()) {
    return juce::Result::fail(
        "Teul compatibility smoke could not parse the current state preset.");
  }
  const auto *currentStateRoot = currentStateJson.getDynamicObject();
  auto *legacyStateRoot = new juce::DynamicObject();
  legacyStateRoot->setProperty("format", "teul.state_preset");
  legacyStateRoot->setProperty("presetName",
                               currentStateRoot->getProperty("preset_name"));
  legacyStateRoot->setProperty(
      "targetGraphName", currentStateRoot->getProperty("target_graph_name"));
  auto *legacyStateSummaryObject = new juce::DynamicObject();
  legacyStateSummaryObject->setProperty("presetName", currentStateSummary.presetName);
  legacyStateSummaryObject->setProperty("targetGraphName",
                                  currentStateSummary.targetGraphName);
  legacyStateSummaryObject->setProperty("nodeStateCount",
                                  currentStateSummary.nodeStateCount);
  legacyStateSummaryObject->setProperty("paramValueCount",
                                  currentStateSummary.paramValueCount);
  legacyStateRoot->setProperty("presetSummary", legacyStateSummaryObject);

  juce::Array<juce::var> legacyNodeStates;
  if (auto *nodeStates = currentStateRoot->getProperty("node_states").getArray()) {
    for (const auto &nodeStateVar : *nodeStates) {
      auto *legacyNodeState = new juce::DynamicObject();
      if (auto *nodeState = nodeStateVar.getDynamicObject()) {
        legacyNodeState->setProperty("nodeId", nodeState->getProperty("node_id"));
        legacyNodeState->setProperty("type", nodeState->getProperty("type_key"));
        legacyNodeState->setProperty("label", nodeState->getProperty("label"));
        legacyNodeState->setProperty("isBypassed",
                                     nodeState->getProperty("bypassed"));
        legacyNodeState->setProperty("paramValues",
                                     nodeState->getProperty("params"));
      }
      legacyNodeStates.add(juce::var(legacyNodeState));
    }
  }
  legacyStateRoot->setProperty("nodeStates", legacyNodeStates);

  const auto legacyStateFile = outputDirectory.getChildFile("legacy_state")
                                   .withFileExtension(
                                       Teul::TStatePresetIO::fileExtension());
  if (!writeJsonArtifact(legacyStateFile, juce::var(legacyStateRoot))) {
    return juce::Result::fail(
        "Teul compatibility smoke could not write the legacy state preset.");
  }

  auto mutatedDocument = sourceDocument;
  auto *mutatedCarrierNode = findTeulNodeByLabel(mutatedDocument, "Carrier");
  auto *mutatedAmpNode = findTeulNodeByLabel(mutatedDocument, "Amp");
  if (mutatedCarrierNode == nullptr || mutatedAmpNode == nullptr) {
    return juce::Result::fail(
        "Teul compatibility smoke could not resolve mutated document nodes.");
  }

  const auto expectedFrequency =
      (double)findTeulNodeByLabel(sourceDocument, "Carrier")->params["frequency"];
  const auto expectedAmpGain =
      (double)findTeulNodeByLabel(sourceDocument, "Amp")->params["gain"];
  const auto expectedAmpBypassed =
      findTeulNodeByLabel(sourceDocument, "Amp")->bypassed;

  mutatedCarrierNode->params["frequency"] = 91.0;
  mutatedAmpNode->params["gain"] = 0.18;
  mutatedAmpNode->bypassed = !expectedAmpBypassed;

  std::vector<Teul::TStatePresetNodeState> legacyStateNodes;
  Teul::TStatePresetSummary legacyStateSummary;
  Teul::TStatePresetLoadReport legacyStateLoadReport;
  const auto legacyStateLoadResult = Teul::TStatePresetIO::loadFromFile(
      legacyStateNodes, legacyStateSummary, legacyStateFile,
      &legacyStateLoadReport);
  if (legacyStateLoadResult.failed())
    return legacyStateLoadResult;

  Teul::TStatePresetApplyReport legacyStateApplyReport;
  const auto stateApplyResult = Teul::TStatePresetIO::applyToDocument(
      mutatedDocument, legacyStateFile, &legacyStateApplyReport);
  if (stateApplyResult.failed())
    return stateApplyResult;

  mutatedCarrierNode = findTeulNodeByLabel(mutatedDocument, "Carrier");
  mutatedAmpNode = findTeulNodeByLabel(mutatedDocument, "Amp");
  if (mutatedCarrierNode == nullptr || mutatedAmpNode == nullptr ||
      std::abs((double)mutatedCarrierNode->params["frequency"] -
               expectedFrequency) > 1.0e-9 ||
      std::abs((double)mutatedAmpNode->params["gain"] - expectedAmpGain) >
          1.0e-9 ||
      mutatedAmpNode->bypassed != expectedAmpBypassed ||
      !legacyStateLoadReport.migrated ||
      !legacyStateLoadReport.usedLegacyAliases ||
      legacyStateLoadReport.degraded ||
      legacyStateLoadReport.warnings.isEmpty() ||
      legacyStateLoadReport.appliedSteps.size() != 1 ||
      !legacyStateLoadReport.appliedSteps.contains("state:v1->v2") ||
      legacyStateApplyReport.degraded ||
      legacyStateApplyReport.warnings.isEmpty()) {
    return juce::Result::fail(
        "Teul compatibility smoke legacy state preset apply mismatch.");
  }

  const auto summaryFile =
      outputDirectory.getChildFile("compatibility-summary.txt");
  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  const auto summaryText =
      juce::String("legacyDocumentNodes=") +
      juce::String((int)restoredLegacyDocument.nodes.size()) + "\r\n" +
      "legacyDocumentMigrated=" +
      juce::String(legacyDocumentMigration.migrated ? "true" : "false") +
      "\r\n" +
      "legacyDocumentUsedAliases=" +
      juce::String(legacyDocumentMigration.usedLegacyAliases ? "true" : "false") +
      "\r\n" +
      "legacyDocumentDegraded=" +
      juce::String(legacyDocumentMigration.degraded ? "true" : "false") +
      "\r\n" +
      "legacyDocumentWarningCount=" +
      juce::String(legacyDocumentMigration.warnings.size()) + "\r\n" +
      "legacyDocumentWarnings=" +
      summarizeWarnings(legacyDocumentMigration.warnings) + "\r\n" +
      "legacyDocumentAppliedStepCount=" +
      juce::String(legacyDocumentMigration.appliedSteps.size()) + "\r\n" +
      "legacyDocumentAppliedSteps=" +
      summarizeWarnings(legacyDocumentMigration.appliedSteps) + "\r\n" +
      "legacyPatchNodes=" +
      juce::String((int)loadedLegacyPatchDocument.nodes.size()) + "\r\n" +
      "legacyPatchMigrated=" +
      juce::String(legacyPatchLoadReport.migrated ? "true" : "false") +
      "\r\n" +
      "legacyPatchUsedAliases=" +
      juce::String(legacyPatchLoadReport.usedLegacyAliases ? "true" : "false") +
      "\r\n" +
      "legacyPatchDegraded=" +
      juce::String(legacyPatchLoadReport.degraded ? "true" : "false") +
      "\r\n" +
      "legacyPatchWarningCount=" +
      juce::String(legacyPatchLoadReport.warnings.size()) + "\r\n" +
      "legacyPatchWarnings=" +
      summarizeWarnings(legacyPatchLoadReport.warnings) + "\r\n" +
      "legacyPatchAppliedStepCount=" +
      juce::String(legacyPatchLoadReport.appliedSteps.size()) + "\r\n" +
      "legacyPatchAppliedSteps=" +
      summarizeWarnings(legacyPatchLoadReport.appliedSteps) + "\r\n" +
      "legacyPatchGraphMigrated=" +
      juce::String(legacyPatchLoadReport.graphMigration.migrated ? "true" : "false") +
      "\r\n" +
      "legacyPatchGraphDegraded=" +
      juce::String(legacyPatchLoadReport.graphMigration.degraded ? "true" : "false") +
      "\r\n" +
      "legacyPatchGraphWarningCount=" +
      juce::String(legacyPatchLoadReport.graphMigration.warnings.size()) +
      "\r\n" +
      "legacyPatchGraphWarnings=" +
      summarizeWarnings(legacyPatchLoadReport.graphMigration.warnings) +
      "\r\n" +
      "legacyStateAppliedNodes=" +
      juce::String(legacyStateApplyReport.appliedNodeCount) + "\r\n" +
      "legacyStateMigrated=" +
      juce::String(legacyStateLoadReport.migrated ? "true" : "false") +
      "\r\n" +
      "legacyStateUsedAliases=" +
      juce::String(legacyStateLoadReport.usedLegacyAliases ? "true" : "false") +
      "\r\n" +
      "legacyStateDegraded=" +
      juce::String(legacyStateLoadReport.degraded ? "true" : "false") +
      "\r\n" +
      "legacyStateWarningCount=" +
      juce::String(legacyStateLoadReport.warnings.size()) + "\r\n" +
      "legacyStateWarnings=" +
      summarizeWarnings(legacyStateLoadReport.warnings) + "\r\n" +
      "legacyStateAppliedStepCount=" +
      juce::String(legacyStateLoadReport.appliedSteps.size()) + "\r\n" +
      "legacyStateAppliedSteps=" +
      summarizeWarnings(legacyStateLoadReport.appliedSteps) + "\r\n" +
      "legacyStateApplyDegraded=" +
      juce::String(legacyStateApplyReport.degraded ? "true" : "false") +
      "\r\n" +
      "legacyStateApplyWarningCount=" +
      juce::String(legacyStateApplyReport.warnings.size()) + "\r\n" +
      "legacyStateApplyWarnings=" +
      summarizeWarnings(legacyStateApplyReport.warnings) + "\r\n" +
      "legacyPatchFile=" + legacyPatchFile.getFullPathName() + "\r\n" +
      "legacyStateFile=" + legacyStateFile.getFullPathName() + "\r\n" +
      "passed=true\r\n";
  if (!summaryFile.replaceWithText(summaryText, false, false, "\r\n")) {
    return juce::Result::fail(
        "Teul compatibility smoke could not write its summary file.");
  }

  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("legacyPatchPreset", outputDirectory,
                                  legacyPatchFile));
  files.add(makeArtifactFileEntry("legacyStatePreset", outputDirectory,
                                  legacyStateFile));
  files.add(makeArtifactFileEntry("summary", outputDirectory, summaryFile));
  auto *bundleRoot = new juce::DynamicObject();
  bundleRoot->setProperty("kind", "teul-verification-artifact-bundle");
  bundleRoot->setProperty("scope", "compatibility-smoke");
  bundleRoot->setProperty("passed", true);
  bundleRoot->setProperty("artifactDirectory",
                          outputDirectory.getFullPathName());
  bundleRoot->setProperty("legacyDocumentNodeCount",
                          (int)restoredLegacyDocument.nodes.size());
  bundleRoot->setProperty("legacyDocumentMigrated",
                          legacyDocumentMigration.migrated);
  bundleRoot->setProperty("legacyDocumentUsedAliases",
                          legacyDocumentMigration.usedLegacyAliases);
  bundleRoot->setProperty("legacyDocumentDegraded",
                          legacyDocumentMigration.degraded);
  bundleRoot->setProperty("legacyDocumentWarningCount",
                          legacyDocumentMigration.warnings.size());
  bundleRoot->setProperty("legacyDocumentWarnings",
                          stringArrayToJsonArray(legacyDocumentMigration.warnings));
  bundleRoot->setProperty("legacyDocumentAppliedStepCount",
                          legacyDocumentMigration.appliedSteps.size());
  bundleRoot->setProperty("legacyDocumentAppliedSteps",
                          stringArrayToJsonArray(legacyDocumentMigration.appliedSteps));
  bundleRoot->setProperty("legacyPatchNodeCount",
                          (int)loadedLegacyPatchDocument.nodes.size());
  bundleRoot->setProperty("legacyPatchMigrated",
                          legacyPatchLoadReport.migrated);
  bundleRoot->setProperty("legacyPatchUsedAliases",
                          legacyPatchLoadReport.usedLegacyAliases);
  bundleRoot->setProperty("legacyPatchDegraded",
                          legacyPatchLoadReport.degraded);
  bundleRoot->setProperty("legacyPatchWarningCount",
                          legacyPatchLoadReport.warnings.size());
  bundleRoot->setProperty("legacyPatchWarnings",
                          stringArrayToJsonArray(legacyPatchLoadReport.warnings));
  bundleRoot->setProperty("legacyPatchAppliedStepCount",
                          legacyPatchLoadReport.appliedSteps.size());
  bundleRoot->setProperty("legacyPatchAppliedSteps",
                          stringArrayToJsonArray(legacyPatchLoadReport.appliedSteps));
  bundleRoot->setProperty("legacyPatchGraphMigrated",
                          legacyPatchLoadReport.graphMigration.migrated);
  bundleRoot->setProperty("legacyPatchGraphDegraded",
                          legacyPatchLoadReport.graphMigration.degraded);
  bundleRoot->setProperty("legacyPatchGraphWarningCount",
                          legacyPatchLoadReport.graphMigration.warnings.size());
  bundleRoot->setProperty(
      "legacyPatchGraphWarnings",
      stringArrayToJsonArray(legacyPatchLoadReport.graphMigration.warnings));
  bundleRoot->setProperty("legacyStateAppliedNodeCount",
                          legacyStateApplyReport.appliedNodeCount);
  bundleRoot->setProperty("legacyStateMigrated",
                          legacyStateLoadReport.migrated);
  bundleRoot->setProperty("legacyStateUsedAliases",
                          legacyStateLoadReport.usedLegacyAliases);
  bundleRoot->setProperty("legacyStateDegraded",
                          legacyStateLoadReport.degraded);
  bundleRoot->setProperty("legacyStateWarningCount",
                          legacyStateLoadReport.warnings.size());
  bundleRoot->setProperty("legacyStateWarnings",
                          stringArrayToJsonArray(legacyStateLoadReport.warnings));
  bundleRoot->setProperty("legacyStateAppliedStepCount",
                          legacyStateLoadReport.appliedSteps.size());
  bundleRoot->setProperty("legacyStateAppliedSteps",
                          stringArrayToJsonArray(legacyStateLoadReport.appliedSteps));
  bundleRoot->setProperty("legacyStateApplyDegraded",
                          legacyStateApplyReport.degraded);
  bundleRoot->setProperty("legacyStateApplyWarningCount",
                          legacyStateApplyReport.warnings.size());
  bundleRoot->setProperty("legacyStateApplyWarnings",
                          stringArrayToJsonArray(legacyStateApplyReport.warnings));
  bundleRoot->setProperty("files", juce::var(files));
  if (!writeJsonArtifact(bundleFile, juce::var(bundleRoot))) {
    return juce::Result::fail(
        "Teul compatibility smoke could not write its artifact bundle.");
  }

  std::cout << "Teul Phase8 compatibility smoke directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << summaryText << std::endl;
  std::cout << "Teul Phase8 compatibility smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}


juce::Result runTeulPhase8ControlModelSmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulControlModelSmoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  if (!outputDirectory.createDirectory() && !outputDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul control model smoke output directory could not be created.");
  }

  Teul::TGraphDocument sourceDocument;
  sourceDocument.meta.name = "Teul Control Model Smoke";
  sourceDocument.controlState.ensureDefaultRails();
  sourceDocument.controlState.sources.clear();
  sourceDocument.controlState.deviceProfiles.clear();
  sourceDocument.controlState.assignments.clear();
  sourceDocument.controlState.missingDeviceProfileIds.clear();

  Teul::TControlSource expressionSource;
  expressionSource.sourceId = "exp-1";
  expressionSource.deviceProfileId = "vox-floor";
  expressionSource.railId = "control-rail";
  expressionSource.displayName = "EXP 1";
  expressionSource.kind = Teul::TControlSourceKind::expression;
  expressionSource.mode = Teul::TControlSourceMode::continuous;
  expressionSource.autoDetected = true;
  expressionSource.confirmed = true;
  expressionSource.ports.push_back(
      {"exp-1-value", "Value", Teul::TControlPortKind::value});

  Teul::TControlSource footswitchSource;
  footswitchSource.sourceId = "fs-1";
  footswitchSource.deviceProfileId = "vox-floor";
  footswitchSource.railId = "control-rail";
  footswitchSource.displayName = "FS 1";
  footswitchSource.kind = Teul::TControlSourceKind::footswitch;
  footswitchSource.mode = Teul::TControlSourceMode::momentary;
  footswitchSource.autoDetected = true;
  footswitchSource.confirmed = true;
  footswitchSource.ports.push_back(
      {"fs-1-gate", "Gate", Teul::TControlPortKind::gate});
  footswitchSource.ports.push_back(
      {"fs-1-trigger", "Trigger", Teul::TControlPortKind::trigger});

  sourceDocument.controlState.sources.push_back(expressionSource);
  sourceDocument.controlState.sources.push_back(footswitchSource);

  Teul::TDeviceProfile deviceProfile;
  deviceProfile.profileId = "vox-floor";
  deviceProfile.deviceId = "midi:vox-floor";
  deviceProfile.displayName = "VOX Floor Controller";
  deviceProfile.autoDetected = true;

  Teul::TDeviceSourceProfile expressionProfile;
  expressionProfile.sourceId = "exp-1";
  expressionProfile.displayName = "EXP 1";
  expressionProfile.kind = Teul::TControlSourceKind::expression;
  expressionProfile.mode = Teul::TControlSourceMode::continuous;
  expressionProfile.ports.push_back(
      {"exp-1-value", "Value", Teul::TControlPortKind::value});
  expressionProfile.bindings.push_back(
      {"VOX Floor Controller", "vox-floor-hw", 1, 11, -1});

  Teul::TDeviceSourceProfile footswitchProfile;
  footswitchProfile.sourceId = "fs-1";
  footswitchProfile.displayName = "FS 1";
  footswitchProfile.kind = Teul::TControlSourceKind::footswitch;
  footswitchProfile.mode = Teul::TControlSourceMode::momentary;
  footswitchProfile.ports.push_back(
      {"fs-1-gate", "Gate", Teul::TControlPortKind::gate});
  footswitchProfile.ports.push_back(
      {"fs-1-trigger", "Trigger", Teul::TControlPortKind::trigger});
  footswitchProfile.bindings.push_back(
      {"VOX Floor Controller", "vox-floor-hw", 1, -1, 36});

  deviceProfile.sources.push_back(expressionProfile);
  deviceProfile.sources.push_back(footswitchProfile);
  sourceDocument.controlState.deviceProfiles.push_back(deviceProfile);

  sourceDocument.controlState.assignments.push_back(
      {"exp-1", "exp-1-value", 1, "mix"});
  sourceDocument.controlState.assignments.push_back(
      {"fs-1", "fs-1-trigger", 1, "bypass"});
  sourceDocument.controlState.missingDeviceProfileIds.push_back(
      "missing-stage-rig");

  const auto documentJson = Teul::TSerializer::toJson(sourceDocument);
  const auto documentFile =
      outputDirectory.getChildFile("control-model-document.json");
  if (!documentFile.replaceWithText(juce::JSON::toString(documentJson, true),
                                    false, false, "\r\n")) {
    return juce::Result::fail(
        "Teul control model smoke could not write the round-trip document.");
  }

  Teul::TGraphDocument restoredDocument;
  Teul::TSchemaMigrationReport migrationReport;
  if (!Teul::TSerializer::fromJson(restoredDocument, documentJson,
                                   &migrationReport)) {
    return juce::Result::fail(
        "Teul control model smoke could not restore the round-trip document.");
  }

  const auto *controlRail =
      restoredDocument.controlState.findRail("control-rail");
  const auto *expressionRestored =
      restoredDocument.controlState.findSource("exp-1");
  const auto *footswitchRestored =
      restoredDocument.controlState.findSource("fs-1");
  const auto *profileRestored =
      restoredDocument.controlState.findDeviceProfile("vox-floor");
  const bool roundTripPassed =
      restoredDocument.controlState.rails.size() == 3 &&
      controlRail != nullptr &&
      controlRail->kind == Teul::TRailKind::controlSource &&
      restoredDocument.controlState.sources.size() == 2 &&
      expressionRestored != nullptr && expressionRestored->ports.size() == 1 &&
      footswitchRestored != nullptr && footswitchRestored->ports.size() == 2 &&
      restoredDocument.controlState.deviceProfiles.size() == 1 &&
      profileRestored != nullptr && profileRestored->sources.size() == 2 &&
      restoredDocument.controlState.assignments.size() == 2 &&
      restoredDocument.controlState.missingDeviceProfileIds.size() == 1 &&
      !migrationReport.degraded;

  const bool markedMissing =
      restoredDocument.controlState.markDeviceProfileMissing("vox-floor");
  const auto *expressionMissing =
      restoredDocument.controlState.findSource("exp-1");
  const auto *footswitchMissing =
      restoredDocument.controlState.findSource("fs-1");
  const bool missingPropagationPassed =
      markedMissing && expressionMissing != nullptr && expressionMissing->missing &&
      footswitchMissing != nullptr && footswitchMissing->missing &&
      restoredDocument.controlState.missingDeviceProfileIds.size() == 2;

  const bool reconnected = restoredDocument.controlState.markDeviceProfilePresent(
      "vox-floor-reconnected", "midi:vox-floor", "VOX Floor Controller", true);
  const auto *reconnectedProfile =
      restoredDocument.controlState.findDeviceProfile("vox-floor-reconnected");
  const auto *oldProfile =
      restoredDocument.controlState.findDeviceProfile("vox-floor");
  const auto *expressionReconnected =
      restoredDocument.controlState.findSource("exp-1");
  const bool reconnectPassed =
      reconnected && reconnectedProfile != nullptr && oldProfile == nullptr &&
      expressionReconnected != nullptr &&
      expressionReconnected->deviceProfileId == "vox-floor-reconnected" &&
      !expressionReconnected->missing &&
      restoredDocument.controlState.missingDeviceProfileIds.size() == 1;

  const bool armed = restoredDocument.controlState.setLearnArmed("exp-1", true);
  const bool learned =
      restoredDocument.controlState.applyLearnedBindingToArmedSource(
          {"VOX Floor Controller", "vox-floor-hw", 1, 74, -1},
          "vox-floor-reconnected", "midi:vox-floor", "VOX Floor Controller",
          Teul::TControlSourceKind::midiCc,
          Teul::TControlSourceMode::continuous, "EXP Learn", true, true);
  const auto *expressionLearned =
      restoredDocument.controlState.findSource("exp-1");
  const auto *learnedProfileSource =
      restoredDocument.controlState.findDeviceSourceProfile(
          "vox-floor-reconnected", "exp-1");
  const bool learnedBindingPresent =
      learnedProfileSource != nullptr &&
      std::any_of(learnedProfileSource->bindings.begin(),
                  learnedProfileSource->bindings.end(),
                  [](const Teul::TDeviceBindingSignature &binding) {
                    return binding.controllerNumber == 74 &&
                           binding.midiChannel == 1 &&
                           binding.hardwareId == "vox-floor-hw";
                  });
  const bool learnApplyPassed =
      armed && learned && expressionLearned != nullptr &&
      expressionLearned->kind == Teul::TControlSourceKind::midiCc &&
      expressionLearned->mode == Teul::TControlSourceMode::continuous &&
      expressionLearned->displayName == "EXP Learn" &&
      !restoredDocument.controlState.isLearnArmed("exp-1") &&
      learnedBindingPresent;

  auto pumpControlQueues = []() {
    for (int attempt = 0; attempt < 6; ++attempt) {
      juce::Thread::sleep(40);
      juce::Timer::callPendingTimersSynchronously();
    }
  };

  Teul::EditorHandle editor(nullptr);
  editor.document() = restoredDocument;
  editor.refreshFromDocument();
  const bool editorMarkedMissing =
      editor.reportControlDeviceProfileMissing("vox-floor-reconnected");
  const auto *editorExpressionMissing =
      editor.document().controlState.findSource("exp-1");
  const bool editorMissingPassed =
      editorMarkedMissing && editorExpressionMissing != nullptr &&
      editorExpressionMissing->missing;

  const bool queuedSyncApplied = editor.syncControlDeviceProfiles(
      {{"vox-floor-runtime", "midi:vox-floor", "VOX Floor Controller", true}},
      true);
  const auto *runtimeProfile =
      editor.document().controlState.findDeviceProfile("vox-floor-runtime");
  const auto *editorExpressionResynced =
      editor.document().controlState.findSource("exp-1");
  const juce::String queuedSyncSourceProfileId =
      editorExpressionResynced != nullptr ? editorExpressionResynced->deviceProfileId
                                          : juce::String{};
  const bool queuedSyncSourceMissing =
      editorExpressionResynced != nullptr && editorExpressionResynced->missing;
  const bool queuedSyncPassed =
      queuedSyncApplied && runtimeProfile != nullptr &&
      editorExpressionResynced != nullptr &&
      editorExpressionResynced->deviceProfileId == "vox-floor-runtime" &&
      !editorExpressionResynced->missing;

  editor.document().controlState.setLearnArmed("fs-1", true);
  editor.enqueueLearnedControlBinding(
      {"VOX Floor Controller", "vox-floor-hw", 1, -1, 42},
      "vox-floor-runtime", "midi:vox-floor", "VOX Floor Controller",
      Teul::TControlSourceKind::midiNote,
      Teul::TControlSourceMode::momentary, "FS Learn", true, true);
  pumpControlQueues();
  const auto *queueLearnedSource =
      editor.document().controlState.findSource("fs-1");
  const auto *queueLearnedProfileSource =
      editor.document().controlState.findDeviceSourceProfile(
          "vox-floor-runtime", "fs-1");
  const bool queuedBindingPresent =
      queueLearnedProfileSource != nullptr &&
      std::any_of(queueLearnedProfileSource->bindings.begin(),
                  queueLearnedProfileSource->bindings.end(),
                  [](const Teul::TDeviceBindingSignature &binding) {
                    return binding.noteNumber == 42 &&
                           binding.midiChannel == 1 &&
                           binding.hardwareId == "vox-floor-hw";
                  });
  const bool queuedLearnPassed =
      queueLearnedSource != nullptr &&
      queueLearnedSource->kind == Teul::TControlSourceKind::midiNote &&
      queueLearnedSource->displayName == "FS Learn" &&
      !editor.document().controlState.isLearnArmed("fs-1") &&
      queuedBindingPresent;

  const bool passed = roundTripPassed && missingPropagationPassed &&
                      reconnectPassed && learnApplyPassed &&
                      editorMissingPassed && queuedSyncPassed &&
                      queuedLearnPassed;

  const auto summaryFile =
      outputDirectory.getChildFile("control-model-summary.txt");
  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  const juce::String summaryText =
      juce::StringArray{
          "railCount=" +
              juce::String((int)restoredDocument.controlState.rails.size()),
          "sourceCount=" +
              juce::String((int)restoredDocument.controlState.sources.size()),
          "deviceProfileCount=" +
              juce::String((int)restoredDocument.controlState.deviceProfiles.size()),
          "assignmentCount=" +
              juce::String((int)restoredDocument.controlState.assignments.size()),
          "missingDeviceProfileCount=" +
              juce::String(
                  (int)restoredDocument.controlState.missingDeviceProfileIds.size()),
          "roundTripPassed=" +
              juce::String(roundTripPassed ? "true" : "false"),
          "missingPropagationPassed=" +
              juce::String(missingPropagationPassed ? "true" : "false"),
          "reconnectPassed=" +
              juce::String(reconnectPassed ? "true" : "false"),
          "learnApplyPassed=" +
              juce::String(learnApplyPassed ? "true" : "false"),
          "editorMissingPassed=" +
              juce::String(editorMissingPassed ? "true" : "false"),
          "queuedSyncPassed=" +
              juce::String(queuedSyncPassed ? "true" : "false"),
          "queuedSyncSourceProfileId=" + queuedSyncSourceProfileId,
          "queuedSyncSourceMissing=" +
              juce::String(queuedSyncSourceMissing ? "true" : "false"),
          "queuedLearnPassed=" +
              juce::String(queuedLearnPassed ? "true" : "false"),
          "migrated=" +
              juce::String(migrationReport.migrated ? "true" : "false"),
          "passed=" + juce::String(passed ? "true" : "false")}
          .joinIntoString("\r\n") +
      "\r\n";
  if (!summaryFile.replaceWithText(summaryText, false, false, "\r\n")) {
    return juce::Result::fail(
        "Teul control model smoke could not write its summary file.");
  }

  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("document", outputDirectory, documentFile));
  files.add(makeArtifactFileEntry("summary", outputDirectory, summaryFile));
  auto *bundleRoot = new juce::DynamicObject();
  bundleRoot->setProperty("kind", "teul-verification-artifact-bundle");
  bundleRoot->setProperty("scope", "control-model-smoke");
  bundleRoot->setProperty("passed", passed);
  bundleRoot->setProperty("artifactDirectory",
                          outputDirectory.getFullPathName());
  bundleRoot->setProperty("railCount",
                          (int)restoredDocument.controlState.rails.size());
  bundleRoot->setProperty("sourceCount",
                          (int)restoredDocument.controlState.sources.size());
  bundleRoot->setProperty(
      "deviceProfileCount",
      (int)restoredDocument.controlState.deviceProfiles.size());
  bundleRoot->setProperty(
      "assignmentCount",
      (int)restoredDocument.controlState.assignments.size());
  bundleRoot->setProperty(
      "missingDeviceProfileCount",
      (int)restoredDocument.controlState.missingDeviceProfileIds.size());
  bundleRoot->setProperty("roundTripPassed", roundTripPassed);
  bundleRoot->setProperty("missingPropagationPassed", missingPropagationPassed);
  bundleRoot->setProperty("reconnectPassed", reconnectPassed);
  bundleRoot->setProperty("learnApplyPassed", learnApplyPassed);
  bundleRoot->setProperty("editorMissingPassed", editorMissingPassed);
  bundleRoot->setProperty("queuedSyncPassed", queuedSyncPassed);
  bundleRoot->setProperty("queuedSyncSourceProfileId",
                          queuedSyncSourceProfileId);
  bundleRoot->setProperty("queuedSyncSourceMissing", queuedSyncSourceMissing);
  bundleRoot->setProperty("queuedLearnPassed", queuedLearnPassed);
  bundleRoot->setProperty("files", juce::var(files));
  if (!writeJsonArtifact(bundleFile, juce::var(bundleRoot))) {
    return juce::Result::fail(
        "Teul control model smoke could not write its artifact bundle.");
  }

  if (!passed)
    return juce::Result::fail("Teul control model smoke checks failed.");

  std::cout << "Teul Phase8 control model smoke directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << summaryText << std::endl;
  std::cout << "Teul Phase8 control model smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}

juce::Result runTeulPhase8CompatibilityMatrix(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulCompatibilityMatrix_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  if (!outputDirectory.createDirectory() && !outputDirectory.isDirectory()) {
    return juce::Result::fail(
        "Teul compatibility matrix output directory could not be created.");
  }

  struct CompatibilityMatrixCaseResult {
    juce::String caseId;
    bool passed = false;
    bool migrated = false;
    bool usedLegacyAliases = false;
    bool degraded = false;
    int warningCount = 0;
    juce::String warnings;
    juce::String detail;
    juce::String artifactRelativePath;
  };

  std::vector<CompatibilityMatrixCaseResult> caseResults;
  juce::Array<juce::var> caseEntries;
  int passedCaseCount = 0;
  int failedCaseCount = 0;

  auto appendCase = [&](const CompatibilityMatrixCaseResult &result) {
    caseResults.push_back(result);
    auto *entry = new juce::DynamicObject();
    entry->setProperty("caseId", result.caseId);
    entry->setProperty("passed", result.passed);
    entry->setProperty("migrated", result.migrated);
    entry->setProperty("usedLegacyAliases", result.usedLegacyAliases);
    entry->setProperty("degraded", result.degraded);
    entry->setProperty("warningCount", result.warningCount);
    if (result.warnings.isNotEmpty())
      entry->setProperty("warnings", result.warnings);
    if (result.detail.isNotEmpty())
      entry->setProperty("detail", result.detail);
    if (result.artifactRelativePath.isNotEmpty())
      entry->setProperty("artifactRelativePath", result.artifactRelativePath);
    caseEntries.add(juce::var(entry));
    if (result.passed)
      ++passedCaseCount;
    else
      ++failedCaseCount;
  };

  auto loadJsonFile = [](const juce::File &file, juce::var &jsonOut) {
    return !juce::JSON::parse(file.loadFileAsString(), jsonOut).failed() &&
           jsonOut.isObject();
  };

  {
    const auto aliasOutputDirectory = outputDirectory.getChildFile("alias_full");
    juce::StringArray aliasArgs;
    aliasArgs.add("--output-dir=" + aliasOutputDirectory.getFullPathName());
    const auto aliasResult = runTeulPhase8CompatibilitySmoke(aliasArgs);

    CompatibilityMatrixCaseResult caseResult;
    caseResult.caseId = "alias-full";
    caseResult.migrated = true;
    caseResult.usedLegacyAliases = true;
    caseResult.artifactRelativePath =
        relativeArtifactPath(outputDirectory, aliasOutputDirectory);

    if (aliasResult.failed()) {
      caseResult.detail = aliasResult.getErrorMessage();
    } else {
      juce::var aliasBundleJson;
      const auto aliasBundleFile =
          aliasOutputDirectory.getChildFile("artifact-bundle.json");
      if (!loadJsonFile(aliasBundleFile, aliasBundleJson)) {
        caseResult.detail =
            "Legacy alias smoke bundle is missing or invalid.";
      } else if (const auto *bundle = aliasBundleJson.getDynamicObject()) {
        const auto readBool = [&](const char *key) {
          return (bool)bundle->getProperty(key);
        };
        const auto readInt = [&](const char *key) {
          return (int)bundle->getProperty(key);
        };
        caseResult.warningCount =
            readInt("legacyDocumentWarningCount") +
            readInt("legacyPatchWarningCount") +
            readInt("legacyPatchGraphWarningCount") +
            readInt("legacyStateWarningCount") +
            readInt("legacyStateApplyWarningCount");
        caseResult.degraded =
            readBool("legacyDocumentDegraded") ||
            readBool("legacyPatchDegraded") ||
            readBool("legacyPatchGraphDegraded") ||
            readBool("legacyStateDegraded") ||
            readBool("legacyStateApplyDegraded");
        caseResult.passed =
            readBool("legacyDocumentMigrated") &&
            readBool("legacyDocumentUsedAliases") &&
            readBool("legacyPatchMigrated") &&
            readBool("legacyPatchUsedAliases") &&
            readBool("legacyPatchGraphMigrated") &&
            readBool("legacyStateMigrated") &&
            readBool("legacyStateUsedAliases") &&
            caseResult.warningCount > 0 && !caseResult.degraded;
        if (!caseResult.passed) {
          caseResult.detail =
              "Legacy alias smoke bundle is missing expected warning/degraded fields.";
        }
      }
    }

    appendCase(caseResult);
  }

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto assetSource =
      outputDirectory.getChildFile("CompatibilityMatrixImpulse.wav");
  if (!assetSource.replaceWithText("teul compatibility matrix asset", false,
                                   false, "\r\n")) {
    return juce::Result::fail(
        "Failed to create compatibility matrix asset file.");
  }

  auto sourceDocument = makeTeulPhase5SmokeDocument(*registry, assetSource);
  sourceDocument.meta.name = "Compatibility Matrix";
  if (sourceDocument.nodes.size() < 3) {
    return juce::Result::fail(
        "Teul compatibility matrix graph does not contain enough nodes.");
  }

  Teul::TFrameRegion frame;
  frame.frameId = sourceDocument.allocFrameId();
  frame.frameUuid = juce::Uuid().toString();
  frame.title = "Compatibility Matrix Patch";
  frame.logicalGroup = true;
  frame.membershipExplicit = true;
  frame.colorArgb = 0x334d8bf7;

  juce::Rectangle<float> memberBounds;
  bool hasMemberBounds = false;
  for (int index = 0; index < 3; ++index) {
    const auto &node = sourceDocument.nodes[(size_t)index];
    const juce::Rectangle<float> nodeRect(node.x, node.y, 160.0f, 90.0f);
    memberBounds = hasMemberBounds ? memberBounds.getUnion(nodeRect) : nodeRect;
    hasMemberBounds = true;
  }
  if (!hasMemberBounds) {
    return juce::Result::fail(
        "Teul compatibility matrix could not compute frame bounds.");
  }

  memberBounds = memberBounds.expanded(24.0f, 24.0f);
  frame.x = memberBounds.getX();
  frame.y = memberBounds.getY();
  frame.width = memberBounds.getWidth();
  frame.height = memberBounds.getHeight();
  sourceDocument.frames.push_back(frame);
  for (int index = 0; index < 3; ++index) {
    sourceDocument.addNodeToFrameExclusive(sourceDocument.nodes[(size_t)index].nodeId,
                                           frame.frameId);
  }

  const auto expectedFrequency =
      (double)findTeulNodeByLabel(sourceDocument, "Carrier")->params["frequency"];
  const auto expectedAmpGain =
      (double)findTeulNodeByLabel(sourceDocument, "Amp")->params["gain"];
  const auto expectedAmpBypassed =
      findTeulNodeByLabel(sourceDocument, "Amp")->bypassed;

  {
    const auto documentFile = outputDirectory.getChildFile("document_schema_v1.json");
    auto documentJson = Teul::TSerializer::toJson(sourceDocument);
    if (auto *root = documentJson.getDynamicObject())
      root->setProperty("schema_version", 1);
    if (!writeJsonArtifact(documentFile, documentJson)) {
      return juce::Result::fail(
          "Teul compatibility matrix could not write the document schema case.");
    }

    Teul::TGraphDocument restoredDocument;
    Teul::TSchemaMigrationReport migrationReport;
    CompatibilityMatrixCaseResult caseResult;
    caseResult.caseId = "document-schema-v1";
    caseResult.artifactRelativePath =
        relativeArtifactPath(outputDirectory, documentFile);
    caseResult.passed =
        Teul::TSerializer::fromJson(restoredDocument, documentJson,
                                    &migrationReport) &&
        restoredDocument.nodes.size() == sourceDocument.nodes.size() &&
        restoredDocument.connections.size() == sourceDocument.connections.size() &&
        restoredDocument.frames.size() == sourceDocument.frames.size() &&
        migrationReport.migrated && !migrationReport.usedLegacyAliases &&
        !migrationReport.degraded && migrationReport.warnings.size() > 0 &&
        migrationReport.sourceSchemaVersion == 1 &&
        migrationReport.targetSchemaVersion ==
            Teul::TSerializer::currentSchemaVersion() &&
        migrationReport.appliedSteps.size() == 2 &&
        migrationReport.appliedSteps.contains("document:v1->v2") &&
        migrationReport.appliedSteps.contains("document:v2->v3");
    caseResult.migrated = migrationReport.migrated;
    caseResult.usedLegacyAliases = migrationReport.usedLegacyAliases;
    caseResult.degraded = migrationReport.degraded;
    caseResult.warningCount = migrationReport.warnings.size();
    caseResult.warnings = summarizeWarnings(migrationReport.warnings);
    if (!caseResult.passed) {
      caseResult.detail =
          "Current-key document v1 case did not restore with the expected migration report.";
    }
    appendCase(caseResult);
  }

  const auto currentPatchFile = outputDirectory.getChildFile("current_patch")
                                    .withFileExtension(
                                        Teul::TPatchPresetIO::fileExtension());
  Teul::TPatchPresetSummary currentPatchSummary;
  const auto patchSaveResult = Teul::TPatchPresetIO::saveFrameToFile(
      sourceDocument, frame.frameId, currentPatchFile, &currentPatchSummary);
  if (patchSaveResult.failed())
    return patchSaveResult;

  auto runPatchCase = [&](const juce::String &caseId,
                          bool removeSummary) -> juce::Result {
    juce::var patchJson;
    if (!loadJsonFile(currentPatchFile, patchJson)) {
      return juce::Result::fail(
          "Teul compatibility matrix could not parse the current patch preset.");
    }

    auto *root = patchJson.getDynamicObject();
    root->setProperty("schema_version", 1);
    if (removeSummary)
      root->removeProperty(juce::Identifier("summary"));

    const auto caseFile = outputDirectory.getChildFile(caseId)
                              .withFileExtension(Teul::TPatchPresetIO::fileExtension());
    if (!writeJsonArtifact(caseFile, patchJson)) {
      return juce::Result::fail(
          "Teul compatibility matrix could not write a patch case file.");
    }

    Teul::TGraphDocument loadedDocument;
    Teul::TPatchPresetSummary loadedSummary;
    Teul::TPatchPresetLoadReport loadReport;
    CompatibilityMatrixCaseResult caseResult;
    caseResult.caseId = caseId;
    caseResult.artifactRelativePath =
        relativeArtifactPath(outputDirectory, caseFile);

    const auto loadResult = Teul::TPatchPresetIO::loadFromFile(
        loadedDocument, loadedSummary, caseFile, &loadReport);
    const auto warningSummary = summarizeWarnings(loadReport.warnings);
    caseResult.passed =
        loadResult.wasOk() &&
        loadedDocument.nodes.size() == (size_t)currentPatchSummary.nodeCount &&
        loadedDocument.connections.size() ==
            (size_t)currentPatchSummary.connectionCount &&
        loadedSummary.nodeCount == currentPatchSummary.nodeCount &&
        loadedSummary.connectionCount == currentPatchSummary.connectionCount &&
        loadedSummary.frameCount == currentPatchSummary.frameCount &&
        loadReport.migrated && !loadReport.usedLegacyAliases &&
        !loadReport.degraded && loadReport.warnings.size() > 0 &&
        loadReport.appliedSteps.size() == 1 &&
        loadReport.appliedSteps.contains("patch:v1->v2") &&
        !loadReport.graphMigration.migrated &&
        !loadReport.graphMigration.usedLegacyAliases &&
        !loadReport.graphMigration.degraded &&
        (!removeSummary || warningSummary.containsIgnoreCase("summary"));
    caseResult.migrated = loadReport.migrated;
    caseResult.usedLegacyAliases = loadReport.usedLegacyAliases;
    caseResult.degraded = loadReport.degraded || loadReport.graphMigration.degraded;
    caseResult.warningCount = loadReport.warnings.size();
    caseResult.warnings = warningSummary;
    if (!caseResult.passed) {
      caseResult.detail = loadResult.failed()
                              ? loadResult.getErrorMessage()
                              : "Patch schema compatibility case did not meet its expected migration report contract.";
    }

    appendCase(caseResult);
    return juce::Result::ok();
  };

  const auto patchSchemaResult =
      runPatchCase("patch_schema_v1_current_keys", false);
  if (patchSchemaResult.failed())
    return patchSchemaResult;
  const auto patchSummaryResult =
      runPatchCase("patch_schema_v1_without_summary", true);
  if (patchSummaryResult.failed())
    return patchSummaryResult;

  const auto currentStateFile = outputDirectory.getChildFile("current_state")
                                    .withFileExtension(
                                        Teul::TStatePresetIO::fileExtension());
  Teul::TStatePresetSummary currentStateSummary;
  const auto stateSaveResult = Teul::TStatePresetIO::saveDocumentToFile(
      sourceDocument, currentStateFile, &currentStateSummary);
  if (stateSaveResult.failed())
    return stateSaveResult;

  auto runStateCase = [&](const juce::String &caseId,
                          bool removeSummary,
                          bool expectPartialApply) -> juce::Result {
    juce::var stateJson;
    if (!loadJsonFile(currentStateFile, stateJson)) {
      return juce::Result::fail(
          "Teul compatibility matrix could not parse the current state preset.");
    }

    auto *root = stateJson.getDynamicObject();
    root->setProperty("schema_version", 1);
    if (removeSummary)
      root->removeProperty(juce::Identifier("summary"));

    const auto caseFile = outputDirectory.getChildFile(caseId)
                              .withFileExtension(Teul::TStatePresetIO::fileExtension());
    if (!writeJsonArtifact(caseFile, stateJson)) {
      return juce::Result::fail(
          "Teul compatibility matrix could not write a state case file.");
    }

    std::vector<Teul::TStatePresetNodeState> nodeStates;
    Teul::TStatePresetSummary loadedSummary;
    Teul::TStatePresetLoadReport loadReport;
    const auto loadResult = Teul::TStatePresetIO::loadFromFile(
        nodeStates, loadedSummary, caseFile, &loadReport);

    auto mutatedDocument = sourceDocument;
    auto *mutatedCarrierNode = findTeulNodeByLabel(mutatedDocument, "Carrier");
    auto *mutatedAmpNode = findTeulNodeByLabel(mutatedDocument, "Amp");
    if (mutatedCarrierNode == nullptr || mutatedAmpNode == nullptr) {
      return juce::Result::fail(
          "Teul compatibility matrix could not resolve state case nodes.");
    }
    mutatedCarrierNode->params["frequency"] = 91.0;
    mutatedAmpNode->params["gain"] = 0.18;
    mutatedAmpNode->bypassed = !expectedAmpBypassed;
    if (expectPartialApply) {
      mutatedAmpNode->typeKey = "Teul.Debug.Missing";
      mutatedAmpNode->label = "Amp Missing";
    }

    Teul::TStatePresetApplyReport applyReport;
    const auto applyResult =
        Teul::TStatePresetIO::applyToDocument(mutatedDocument, caseFile, &applyReport);

    mutatedCarrierNode = findTeulNodeByLabel(mutatedDocument, "Carrier");
    auto *restoredAmpNode = findTeulNodeByLabel(mutatedDocument, "Amp");
    const auto applyWarningSummary = summarizeWarnings(applyReport.warnings);
    const auto loadWarningSummary = summarizeWarnings(loadReport.warnings);

    CompatibilityMatrixCaseResult caseResult;
    caseResult.caseId = caseId;
    caseResult.artifactRelativePath =
        relativeArtifactPath(outputDirectory, caseFile);
    if (expectPartialApply) {
      caseResult.passed =
          loadResult.wasOk() && applyResult.wasOk() &&
          loadedSummary.nodeStateCount == currentStateSummary.nodeStateCount &&
          loadedSummary.paramValueCount == currentStateSummary.paramValueCount &&
          loadReport.migrated && !loadReport.usedLegacyAliases &&
          !loadReport.degraded && loadReport.appliedSteps.size() == 1 &&
          loadReport.appliedSteps.contains("state:v1->v2") &&
          applyReport.degraded &&
          applyReport.skippedNodeCount > 0 &&
          applyWarningSummary.containsIgnoreCase("skipped") &&
          mutatedCarrierNode != nullptr &&
          std::abs((double)mutatedCarrierNode->params["frequency"] -
                   expectedFrequency) <= 1.0e-9 &&
          restoredAmpNode == nullptr;
    } else {
      caseResult.passed =
          loadResult.wasOk() && applyResult.wasOk() &&
          loadedSummary.nodeStateCount == currentStateSummary.nodeStateCount &&
          loadedSummary.paramValueCount == currentStateSummary.paramValueCount &&
          loadReport.migrated && !loadReport.usedLegacyAliases &&
          !loadReport.degraded && loadReport.appliedSteps.size() == 1 &&
          loadReport.appliedSteps.contains("state:v1->v2") &&
          !applyReport.degraded &&
          applyReport.warnings.size() > 0 &&
          mutatedCarrierNode != nullptr && restoredAmpNode != nullptr &&
          std::abs((double)mutatedCarrierNode->params["frequency"] -
                   expectedFrequency) <= 1.0e-9 &&
          std::abs((double)restoredAmpNode->params["gain"] - expectedAmpGain) <=
              1.0e-9 &&
          restoredAmpNode->bypassed == expectedAmpBypassed &&
          (!removeSummary || loadWarningSummary.containsIgnoreCase("summary"));
    }
    caseResult.migrated = loadReport.migrated;
    caseResult.usedLegacyAliases = loadReport.usedLegacyAliases;
    caseResult.degraded = applyReport.degraded || loadReport.degraded;
    caseResult.warningCount = applyReport.warnings.size();
    caseResult.warnings = applyWarningSummary;
    if (!caseResult.passed) {
      caseResult.detail = loadResult.failed()
                              ? loadResult.getErrorMessage()
                              : (applyResult.failed()
                                     ? applyResult.getErrorMessage()
                                     : "State schema compatibility case did not meet its expected migration/apply contract.");
    }

    appendCase(caseResult);
    return juce::Result::ok();
  };

  const auto stateSchemaResult =
      runStateCase("state_schema_v1_current_keys", false, false);
  if (stateSchemaResult.failed())
    return stateSchemaResult;
  const auto stateSummaryResult =
      runStateCase("state_schema_v1_without_summary", true, false);
  if (stateSummaryResult.failed())
    return stateSummaryResult;
  const auto statePartialResult =
      runStateCase("state_schema_v1_partial_apply", false, true);
  if (statePartialResult.failed())
    return statePartialResult;

  const bool passed = failedCaseCount == 0;
  juce::String summaryText =
      "totalCaseCount=" + juce::String((int)caseResults.size()) + "\r\n" +
      "passedCaseCount=" + juce::String(passedCaseCount) + "\r\n" +
      "failedCaseCount=" + juce::String(failedCaseCount) + "\r\n" +
      "passed=" + juce::String(passed ? "true" : "false") + "\r\n";

  for (const auto &caseResult : caseResults) {
    summaryText += "case." + caseResult.caseId + ".passed=" +
                   juce::String(caseResult.passed ? "true" : "false") +
                   "\r\n";
    summaryText += "case." + caseResult.caseId + ".migrated=" +
                   juce::String(caseResult.migrated ? "true" : "false") +
                   "\r\n";
    summaryText += "case." + caseResult.caseId + ".usedLegacyAliases=" +
                   juce::String(caseResult.usedLegacyAliases ? "true" : "false") +
                   "\r\n";
    summaryText += "case." + caseResult.caseId + ".degraded=" +
                   juce::String(caseResult.degraded ? "true" : "false") +
                   "\r\n";
    summaryText += "case." + caseResult.caseId + ".warningCount=" +
                   juce::String(caseResult.warningCount) + "\r\n";
    if (caseResult.warnings.isNotEmpty()) {
      summaryText += "case." + caseResult.caseId + ".warnings=" +
                     caseResult.warnings.replaceCharacters("\r\n", "  ") +
                     "\r\n";
    }
    if (caseResult.detail.isNotEmpty()) {
      summaryText += "case." + caseResult.caseId + ".detail=" +
                     caseResult.detail.replaceCharacters("\r\n", "  ") +
                     "\r\n";
    }
  }

  const auto summaryFile =
      outputDirectory.getChildFile("compatibility-matrix-summary.txt");
  if (!summaryFile.replaceWithText(summaryText, false, false, "\r\n")) {
    return juce::Result::fail(
        "Teul compatibility matrix could not write its summary file.");
  }

  juce::Array<juce::var> files;
  files.add(makeArtifactFileEntry("summary", outputDirectory, summaryFile));
  files.add(makeArtifactFileEntry("aliasFull", outputDirectory,
                                  outputDirectory.getChildFile("alias_full")));
  files.add(makeArtifactFileEntry("currentPatch", outputDirectory, currentPatchFile));
  files.add(makeArtifactFileEntry("currentState", outputDirectory, currentStateFile));

  auto *bundleRoot = new juce::DynamicObject();
  bundleRoot->setProperty("kind", "teul-verification-artifact-bundle");
  bundleRoot->setProperty("scope", "compatibility-matrix");
  bundleRoot->setProperty("passed", passed);
  bundleRoot->setProperty("artifactDirectory", outputDirectory.getFullPathName());
  bundleRoot->setProperty("totalCaseCount", (int)caseResults.size());
  bundleRoot->setProperty("passedCaseCount", passedCaseCount);
  bundleRoot->setProperty("failedCaseCount", failedCaseCount);
  bundleRoot->setProperty("cases", juce::var(caseEntries));
  bundleRoot->setProperty("files", juce::var(files));

  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  if (!writeJsonArtifact(bundleFile, juce::var(bundleRoot))) {
    return juce::Result::fail(
        "Teul compatibility matrix could not write its artifact bundle.");
  }

  std::cout << "Teul Phase8 compatibility matrix directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << summaryText << std::endl;
  std::cout << "Teul Phase8 compatibility matrix checks: "
            << (passed ? "PASS" : "FAIL") << std::endl;

  if (!passed) {
    return juce::Result::fail(
        "Teul compatibility matrix detected failing cases.");
  }

  return juce::Result::ok();
}

juce::Result runTeulPhase7BenchmarkGate(const juce::StringArray &args) {
  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto iterationArg = argValue(args, "--iteration-count=");
  int iterationCount = 8;
  if (iterationArg.isNotEmpty()) {
    iterationCount = iterationArg.getIntValue();
    if (iterationCount <= 0) {
      return juce::Result::fail(
          "Benchmark iteration count must be greater than zero.");
    }
  }

  Teul::TVerificationBenchmarkSuiteReport report;
  const bool passed =
      Teul::runRepresentativeBenchmarkGate(*registry, report, iterationCount);
  if (report.artifactDirectory.isEmpty()) {
    return juce::Result::fail(
        "Teul benchmark gate did not produce an artifact directory.");
  }

  const auto artifactDirectory = juce::File(report.artifactDirectory);
  const auto summaryFile = artifactDirectory.getChildFile("benchmark-summary.txt");
  const auto bundleFile = artifactDirectory.getChildFile("artifact-bundle.json");
  if (!artifactDirectory.isDirectory() || !summaryFile.existsAsFile() ||
      !bundleFile.existsAsFile()) {
    return juce::Result::fail(
        "Teul benchmark gate is missing expected suite artifacts.");
  }

  std::cout << "Teul Phase7 benchmark artifact directory: "
            << artifactDirectory.getFullPathName() << std::endl;
  std::cout << summaryFile.loadFileAsString() << std::endl;

  if (!passed) {
    return juce::Result::fail(
        "Teul representative benchmark gate reported one or more failures.");
  }

  if (report.totalCaseCount <= 0 || report.failedCaseCount != 0) {
    return juce::Result::fail(
        "Teul benchmark gate did not complete a valid representative run.");
  }

  std::cout << "Teul Phase7 benchmark gate checks: PASS" << std::endl;
  return juce::Result::ok();
}

juce::Result runTeulPhase7RuntimeCompileSmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  const auto runtimeClassName = juce::String("TeulPhase5SmokeRuntime");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulCompileSmoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }
  const auto vcVarsPath = resolveVcVars64Path();
  juce::String exportOutput;
  juce::String compileOutput;
  int exportExitCode = -1;
  int compileExitCode = -1;
  juce::Result result = juce::Result::ok();
  do {
    if (vcVarsPath.isEmpty()) {
      result = juce::Result::fail(
          "Visual Studio vcvars64.bat was not found for compile smoke.");
      break;
    }
    const juce::StringArray exportCommand = {
        juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getFullPathName(),
        "--teul-phase5-export-smoke",
        "--output-dir=" + outputDirectory.getFullPathName()};
    const auto exportRun =
        runChildProcess(exportCommand, exportOutput, exportExitCode, 120000);
    juce::ignoreUnused(outputDirectory.createDirectory());
    juce::ignoreUnused(outputDirectory.getChildFile("phase5-export-smoke.txt")
                           .replaceWithText(exportOutput, false, false, "\r\n"));
    if (exportRun.failed()) {
      result = exportRun;
      break;
    }
    if (exportExitCode != 0) {
      result = juce::Result::fail(
          "Teul Phase5 export smoke failed before compile smoke.\n" +
          exportOutput);
      break;
    }
    const auto generatedHeader =
        outputDirectory.getChildFile(runtimeClassName + ".h");
    const auto generatedSource =
        outputDirectory.getChildFile(runtimeClassName + ".cpp");
    if (!generatedHeader.existsAsFile() || !generatedSource.existsAsFile()) {
      result = juce::Result::fail(
          "Runtime compile smoke is missing generated RuntimeModule files.");
      break;
    }
    const auto compileCommand =
        juce::String("cmd.exe /c ") +
        buildTeulCompileSmokeScript(outputDirectory, runtimeClassName);
    const auto compileRun =
        runCommandLine(compileCommand, compileOutput, compileExitCode, 120000);
    juce::ignoreUnused(outputDirectory.getChildFile("compile-output.txt")
                           .replaceWithText(compileOutput, false, false, "\r\n"));
    if (compileRun.failed()) {
      result = compileRun;
      break;
    }
    if (compileExitCode != 0) {
      result = juce::Result::fail(
          "Generated RuntimeModule compile smoke failed.\n" + compileOutput);
      break;
    }
    const auto objectFile = outputDirectory.getChildFile(runtimeClassName + ".obj");
    if (!objectFile.existsAsFile()) {
      result = juce::Result::fail(
          "Generated RuntimeModule compile smoke did not produce an object file.");
      break;
    }
  } while (false);
  const auto bundleWrite = writeTeulRuntimeCompileSmokeArtifactBundle(
      outputDirectory,
      runtimeClassName,
      vcVarsPath,
      result.wasOk(),
      result.failed() ? result.getErrorMessage() : juce::String(),
      exportExitCode,
      compileExitCode);
  if (bundleWrite.failed())
    return bundleWrite;
  const auto bundleFile = outputDirectory.getChildFile("artifact-bundle.json");
  if (!bundleFile.existsAsFile()) {
    return juce::Result::fail(
        "Runtime compile smoke did not produce an artifact bundle.");
  }
  if (result.failed())
    return result;
  std::cout << "Teul Phase7 runtime compile smoke directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << compileOutput << std::endl;
  std::cout << "Teul Phase7 runtime compile smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}
juce::Result runTeulPhase5ExportSmoke(const juce::StringArray &args) {
  const auto outputArg = argValue(args, "--output-dir=");
  juce::File outputDirectory;
  if (outputArg.isNotEmpty()) {
    outputDirectory = juce::File(outputArg);
  } else {
    outputDirectory =
        juce::File::getCurrentWorkingDirectory()
            .getChildFile("Builds")
            .getChildFile("TeulExport")
            .getChildFile("Phase5Smoke_" +
                          juce::String(juce::Time::currentTimeMillis()));
  }

  auto registry = Teul::makeDefaultNodeRegistry();
  if (!registry)
    return juce::Result::fail("Failed to create Teul node registry.");

  const auto assetSource = outputDirectory.getSiblingFile("Phase5SmokeImpulse.wav");
  if (!assetSource.getParentDirectory().createDirectory() &&
      !assetSource.getParentDirectory().exists()) {
    return juce::Result::fail("Failed to create Teul smoke asset directory.");
  }
  if (!assetSource.replaceWithText("teul phase5 smoke asset", false, false,
                                   "\r\n")) {
    return juce::Result::fail("Failed to create Teul smoke asset file.");
  }

  const auto document = makeTeulPhase5SmokeDocument(*registry, assetSource);
  if (document.nodes.empty()) {
    return juce::Result::fail(
        "Teul smoke graph could not be constructed from the registry.");
  }

  Teul::TExportOptions options;
  options.mode = Teul::TExportMode::RuntimeModule;
  options.outputDirectory = outputDirectory;
  options.projectRootDirectory = juce::File::getCurrentWorkingDirectory();
  options.runtimeClassName = "TeulPhase5SmokeRuntime";
  options.overwriteExistingFiles = true;
  options.writeManifestJson = true;
  options.writeGraphJson = true;
  options.writeRuntimeDataJson = true;
  options.writeRuntimeModuleFiles = true;
  options.writeBuildHints = true;
  options.packageAssets = true;

  Teul::TExportReport report;
  const auto exportResult =
      Teul::TExporter::exportToDirectory(document, *registry, options, report);
  std::cout << report.toText() << std::endl;
  if (exportResult.failed())
    return exportResult;

  for (const auto &file : {report.graphFile, report.manifestFile,
                           report.runtimeDataFile, report.generatedHeaderFile,
                           report.generatedSourceFile, report.cmakeHintsFile,
                           report.jucerHintsFile, report.reportFile}) {
    if (!file.existsAsFile()) {
      return juce::Result::fail("Teul smoke export missing expected file: " +
                                file.getFullPathName());
    }
  }

  juce::var runtimeJson;
  {
    const auto parseResult =
        juce::JSON::parse(report.runtimeDataFile.loadFileAsString(), runtimeJson);
    if (parseResult.failed()) {
      return juce::Result::fail("Teul smoke runtime JSON parse failed: " +
                                parseResult.getErrorMessage());
    }
  }

  auto *runtimeRoot = runtimeJson.getDynamicObject();
  if (runtimeRoot == nullptr)
    return juce::Result::fail("Teul smoke runtime JSON root is invalid.");

  const auto *scheduleArray = runtimeRoot->getProperty("schedule").getArray();
  const auto *bufferPlanArray = runtimeRoot->getProperty("bufferPlan").getArray();
  const auto *assetArray = runtimeRoot->getProperty("assets").getArray();
  if (scheduleArray == nullptr || scheduleArray->isEmpty()) {
    return juce::Result::fail("Teul smoke runtime JSON has no schedule.");
  }
  if (bufferPlanArray == nullptr || bufferPlanArray->isEmpty()) {
    return juce::Result::fail("Teul smoke runtime JSON has no bufferPlan.");
  }
  if (assetArray == nullptr || assetArray->isEmpty()) {
    return juce::Result::fail("Teul smoke runtime JSON has no assets.");
  }

  if (report.summary.prunedNodeCount <= 0) {
    return juce::Result::fail("Teul smoke export expected at least one pruned node.");
  }
  if (report.summary.apvtsParamCount <= 0) {
    return juce::Result::fail("Teul smoke export expected APVTS parameters.");
  }
  if (report.summary.copiedAssetCount <= 0) {
    return juce::Result::fail("Teul smoke export expected a packaged asset copy.");
  }

  Teul::TGraphDocument importedDocument;
  const auto importResult =
      Teul::TExporter::importEditableGraphPackage(report.manifestFile,
                                                  importedDocument);
  if (importResult.failed())
    return importResult;
  if (importedDocument.nodes.size() != document.nodes.size() ||
      importedDocument.connections.size() != document.connections.size()) {
    return juce::Result::fail(
        "Teul smoke editable graph round-trip changed graph topology.");
  }

  const auto sourceText = report.generatedSourceFile.loadFileAsString();
  for (const auto &token : {"createParameterLayout()", "embeddedGraphJson()",
                            "embeddedRuntimeJson()", "setParamById(",
                            "nodeSchedule() const noexcept"}) {
    if (!sourceText.contains(token)) {
      return juce::Result::fail(
          juce::String("Teul generated runtime source missing token: ") + juce::String(token));
    }
  }

  auto invalidOptions = options;
  invalidOptions.outputDirectory = outputDirectory.getSiblingFile(
      outputDirectory.getFileName() + "_invalid");
  const auto invalidDocument =
      makeTeulPhase5InvalidSmokeDocument(*registry, assetSource);
  Teul::TExportReport invalidReport;
  const auto invalidResult = Teul::TExporter::exportToDirectory(
      invalidDocument, *registry, invalidOptions, invalidReport);
  if (invalidResult.wasOk() || invalidReport.errorCount() <= 0 ||
      !invalidReport.toText().contains("missing_primary_audio_output")) {
    return juce::Result::fail(
        "Teul invalid smoke export must fail with missing audio output diagnostics.");
  }

  std::cout << "Teul Phase5 smoke export directory: "
            << outputDirectory.getFullPathName() << std::endl;
  std::cout << "Teul Phase5 smoke checks: PASS" << std::endl;
  return juce::Result::ok();
}

} // namespace

//==============================================================================
class DadeumStudioApplication : public juce::JUCEApplication {
public:
  //==============================================================================
  DadeumStudioApplication() {}

  // AppServices ??????壤굿??Β??????????源놁７???沃섃뫖荑???????????????ル뒌??????轅붽틓?????
  // AudioDeviceManager ????????????黎앸럽??筌뚭퍏???紐꾨퓠?熬곣뫀猷???????硫멸킐?????椰????
  // MainComponent(??Teul) ???耀붾굝???????????癰궽블뀮???????獄쏅챶留?????轅붽틓?????
  AppServices appServices;

  const juce::String getApplicationName() override {
    return ProjectInfo::projectName;
  }
  const juce::String getApplicationVersion() override {
    return ProjectInfo::versionString;
  }
  bool moreThanOneInstanceAllowed() override { return true; }

  //==============================================================================
  void initialise(const juce::String &commandLine) override {
    const auto args = parseCommandLineArgs(commandLine);
    if (hasArg(args, "--teul-phase5-export-smoke")) {
      const auto smokeResult = runTeulPhase5ExportSmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase5 smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }


    if (hasArg(args, "--teul-phase7-golden-audio-record")) {
      const auto smokeResult = runTeulPhase7GoldenAudioRecord(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 golden audio record failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase7-golden-audio-verify")) {
      const auto smokeResult = runTeulPhase7GoldenAudioVerify(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 golden audio verify failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase7-parity-smoke")) {
      const auto smokeResult = runTeulPhase7ParitySmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 parity smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }
    if (hasArg(args, "--teul-phase7-parity-matrix")) {
      const auto smokeResult = runTeulPhase7ParityMatrix(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 parity matrix failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase7-compiled-runtime-parity")) {
      const auto smokeResult = runTeulPhase7CompiledRuntimeParity(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 compiled runtime parity failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }
    if (hasArg(args, "--teul-phase7-runtime-compile-smoke")) {
      const auto smokeResult = runTeulPhase7RuntimeCompileSmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 runtime compile smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase7-stress-soak")) {
      const auto smokeResult = runTeulPhase7StressSoak(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 stress/soak failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase8-patch-preset-smoke")) {
      const auto smokeResult = runTeulPhase8PatchPresetSmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase8 patch preset smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase8-state-preset-smoke")) {
      const auto smokeResult = runTeulPhase8StatePresetSmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase8 state preset smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase8-autosave-recovery-smoke")) {
      const auto smokeResult = runTeulPhase8AutosaveRecoverySmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase8 autosave recovery smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase8-compatibility-smoke")) {
      const auto smokeResult = runTeulPhase8CompatibilitySmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase8 compatibility smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase8-control-model-smoke")) {
      const auto smokeResult = runTeulPhase8ControlModelSmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase8 control model smoke failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase8-compatibility-matrix")) {
      const auto smokeResult = runTeulPhase8CompatibilityMatrix(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase8 compatibility matrix failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--teul-phase7-benchmark-gate")) {
      const auto smokeResult = runTeulPhase7BenchmarkGate(args);
      if (smokeResult.failed()) {
        std::cerr << "Teul Phase7 benchmark gate failed: "
                  << smokeResult.getErrorMessage() << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    if (hasArg(args, "--phase6-export-smoke")) {
      const auto smokeResult = runPhase6ExportSmoke(args);
      if (smokeResult.failed()) {
        std::cerr << "Phase6 smoke failed: " << smokeResult.getErrorMessage()
                  << std::endl;
        setApplicationReturnValue(1);
      } else {
        setApplicationReturnValue(0);
      }

      quit();
      return;
    }

    // This method is where you should put your application's initialisation
    // code..
    mainWindow.reset(new MainWindow(getApplicationName(), appServices));
  }

  void shutdown() override {
    // Add your application's shutdown code here..

    mainWindow = nullptr; // (deletes our window)
  }

  //==============================================================================
  void systemRequestedQuit() override {
    // This is called when the app is being asked to quit: you can ignore this
    // request and let the app carry on running, or call quit() to allow the app
    // to close.
    quit();
  }

  void anotherInstanceStarted(const juce::String &commandLine) override {
    // When another instance of the app is launched while this one is running,
    // this method is invoked, and the commandLine parameter tells you what
    // the other instance's command-line arguments were.
    juce::ignoreUnused(commandLine);
  }

  //==============================================================================
  /*
      This class implements the desktop window that contains an instance of
      our MainComponent class.
  */
  class MainWindow : public juce::DocumentWindow {
  public:
    MainWindow(juce::String name, AppServices &services)
        : DocumentWindow(
              name,
              juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                  juce::ResizableWindow::backgroundColourId),
              DocumentWindow::allButtons) {
      setUsingNativeTitleBar(true);
      setContentOwned(new MainComponent(services), true);

#if JUCE_IOS || JUCE_ANDROID
      setFullScreen(true);
#else
      setResizable(true, true);
      if (const auto *display =
              juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        windowedBounds =
            display->userArea.reduced(display->userArea.getWidth() / 10,
                                      display->userArea.getHeight() / 10);
      else
        windowedBounds = {120, 80, 1440, 900};

      setBounds(windowedBounds);
      enterFullscreenMode();
#endif

      setVisible(true);
      toFront(true);
      juce::Process::makeForegroundProcess();
      grabKeyboardFocus();
    }

    bool keyPressed(const juce::KeyPress &key) override {
      if (key.getKeyCode() == juce::KeyPress::F11Key) {
        toggleFullscreenMode();
        return true;
      }

      if (key.getKeyCode() == juce::KeyPress::escapeKey && fullscreenMode) {
        exitFullscreenMode();
        return true;
      }

      return DocumentWindow::keyPressed(key);
    }

    void closeButtonPressed() override {
      // This is called when the user tries to close this window. Here, we'll
      // just ask the app to quit when this happens, but you can change this to
      // do whatever you need.
      JUCEApplication::getInstance()->systemRequestedQuit();
    }

    /* Note: Be careful if you override any DocumentWindow methods - the base
       class uses a lot of them, so by overriding you might break its
       functionality. It's best to do all your work in your content component
       instead, but if you really have to override any DocumentWindow methods,
       make sure your subclass also calls the superclass's method.
    */

  private:
    void toggleFullscreenMode() {
      if (fullscreenMode)
        exitFullscreenMode();
      else
        enterFullscreenMode();
    }

    void enterFullscreenMode() {
      if (fullscreenMode)
        return;

      windowedBounds = getBounds();
      setUsingNativeTitleBar(false);
      setTitleBarHeight(0);
      setFullScreen(true);
      fullscreenMode = true;
    }

    void exitFullscreenMode() {
      if (!fullscreenMode)
        return;

      setFullScreen(false);
      setUsingNativeTitleBar(true);

      if (!windowedBounds.isEmpty())
        setBounds(windowedBounds);

      fullscreenMode = false;
    }

    bool fullscreenMode = false;
    juce::Rectangle<int> windowedBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

private:
  std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(DadeumStudioApplication)
