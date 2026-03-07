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
