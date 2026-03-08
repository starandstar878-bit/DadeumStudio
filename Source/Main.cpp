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
#include "Teul/Verification/TVerificationStress.h"
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
