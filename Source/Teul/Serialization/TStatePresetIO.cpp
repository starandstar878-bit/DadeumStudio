#include "TStatePresetIO.h"

namespace Teul {
namespace {

juce::File withStatePresetExtension(const juce::File &file) {
  const auto extension = TStatePresetIO::fileExtension();
  if (file.hasFileExtension(extension))
    return file;

  return file.withFileExtension(extension);
}

bool writeJsonFile(const juce::File &file, const juce::var &json) {
  return file.replaceWithText(juce::JSON::toString(json, true), false, false,
                              "\r\n");
}

juce::String sanitizePresetName(const juce::String &rawName) {
  juce::String text = rawName.trim();
  if (text.isEmpty())
    text = "StatePreset";

  for (const auto character : {'<', '>', ':', '"', '/', '\\', '|', '?', '*'})
    text = text.replaceCharacter(character, '_');

  return text;
}

int countParamValues(const std::vector<TStatePresetNodeState> &nodeStates) {
  int count = 0;
  for (const auto &nodeState : nodeStates)
    count += (int)nodeState.params.size();
  return count;
}

juce::var nodeStateToJson(const TStatePresetNodeState &nodeState) {
  auto *object = new juce::DynamicObject();
  object->setProperty("node_id", (int64_t)nodeState.nodeId);
  object->setProperty("type_key", nodeState.typeKey);
  object->setProperty("label", nodeState.label);
  object->setProperty("bypassed", nodeState.bypassed);

  auto *paramsObject = new juce::DynamicObject();
  for (const auto &[key, value] : nodeState.params)
    paramsObject->setProperty(key, value);
  object->setProperty("params", paramsObject);
  return juce::var(object);
}

bool jsonToNodeState(TStatePresetNodeState &nodeState, const juce::var &json) {
  if (!json.isObject())
    return false;

  nodeState.nodeId = (NodeId)(int64_t)json.getProperty("node_id", 0);
  nodeState.typeKey = json.getProperty("type_key", "").toString();
  nodeState.label = json.getProperty("label", "").toString();
  nodeState.bypassed = (bool)json.getProperty("bypassed", false);
  nodeState.params.clear();

  if (auto *paramsObject =
          json.getProperty("params", juce::var()).getDynamicObject()) {
    for (const auto &[key, value] : paramsObject->getProperties())
      nodeState.params[key.toString()] = value;
  }

  return nodeState.nodeId != kInvalidNodeId && nodeState.typeKey.isNotEmpty();
}

juce::var summaryToJson(const TStatePresetSummary &summary) {
  auto *object = new juce::DynamicObject();
  object->setProperty("preset_name", summary.presetName);
  object->setProperty("target_graph_name", summary.targetGraphName);
  object->setProperty("node_state_count", summary.nodeStateCount);
  object->setProperty("param_value_count", summary.paramValueCount);
  return juce::var(object);
}

void hydrateSummaryFromJson(TStatePresetSummary &summary,
                            const juce::var &summaryVar) {
  if (auto *object = summaryVar.getDynamicObject()) {
    summary.presetName = object->getProperty("preset_name").toString();
    summary.targetGraphName =
        object->getProperty("target_graph_name").toString();
    summary.nodeStateCount = object->hasProperty("node_state_count")
                                 ? (int)object->getProperty("node_state_count")
                                 : 0;
    summary.paramValueCount = object->hasProperty("param_value_count")
                                  ? (int)object->getProperty("param_value_count")
                                  : 0;
  }
}

TNode *findFallbackNode(TGraphDocument &document,
                        const TStatePresetNodeState &nodeState) {
  TNode *match = nullptr;
  for (auto &node : document.nodes) {
    if (node.typeKey != nodeState.typeKey)
      continue;
    if (nodeState.label.isNotEmpty() && node.label != nodeState.label)
      continue;

    if (match != nullptr)
      return nullptr;

    match = &node;
  }

  return match;
}

TNode *findTargetNode(TGraphDocument &document,
                      const TStatePresetNodeState &nodeState) {
  if (auto *node = document.findNode(nodeState.nodeId)) {
    if (node->typeKey == nodeState.typeKey)
      return node;
  }

  return findFallbackNode(document, nodeState);
}

} // namespace

juce::String TStatePresetIO::fileExtension() { return ".teulstate"; }

juce::File TStatePresetIO::defaultPresetDirectory() {
  return juce::File::getCurrentWorkingDirectory()
      .getChildFile("Builds")
      .getChildFile("TeulStatePresets");
}

juce::Result TStatePresetIO::saveDocumentToFile(const TGraphDocument &document,
                                                const juce::File &file,
                                                TStatePresetSummary *summaryOut) {
  std::vector<TStatePresetNodeState> nodeStates;
  nodeStates.reserve(document.nodes.size());

  for (const auto &node : document.nodes) {
    TStatePresetNodeState nodeState;
    nodeState.nodeId = node.nodeId;
    nodeState.typeKey = node.typeKey;
    nodeState.label = node.label;
    nodeState.bypassed = node.bypassed;
    nodeState.params = node.params;
    nodeStates.push_back(std::move(nodeState));
  }

  TStatePresetSummary summary;
  summary.presetName = sanitizePresetName(file.getFileNameWithoutExtension());
  if (summary.presetName == "StatePreset" && document.meta.name.isNotEmpty())
    summary.presetName = sanitizePresetName(document.meta.name + " State");
  summary.targetGraphName = document.meta.name;
  summary.nodeStateCount = (int)nodeStates.size();
  summary.paramValueCount = countParamValues(nodeStates);

  auto *root = new juce::DynamicObject();
  root->setProperty("format", "teul.state_preset");
  root->setProperty("schema_version", 1);
  root->setProperty("preset_name", summary.presetName);
  root->setProperty("target_graph_name", summary.targetGraphName);
  root->setProperty("summary", summaryToJson(summary));

  juce::Array<juce::var> nodeStatesArray;
  for (const auto &nodeState : nodeStates)
    nodeStatesArray.add(nodeStateToJson(nodeState));
  root->setProperty("node_states", nodeStatesArray);

  const auto targetFile = withStatePresetExtension(file);
  if (!targetFile.getParentDirectory().createDirectory() &&
      !targetFile.getParentDirectory().isDirectory()) {
    return juce::Result::fail(
        "State preset save failed: output directory could not be created.");
  }

  if (!writeJsonFile(targetFile, juce::var(root)))
    return juce::Result::fail("State preset save failed: file write failed.");

  if (summaryOut != nullptr)
    *summaryOut = summary;

  return juce::Result::ok();
}

juce::Result TStatePresetIO::loadFromFile(
    std::vector<TStatePresetNodeState> &nodeStatesOut, TStatePresetSummary &summaryOut,
    const juce::File &file) {
  if (!file.existsAsFile())
    return juce::Result::fail("State preset load failed: file not found.");

  const auto json = juce::JSON::parse(file);
  if (json.isVoid())
    return juce::Result::fail("State preset load failed: invalid JSON.");

  auto *root = json.getDynamicObject();
  if (root == nullptr)
    return juce::Result::fail("State preset load failed: invalid preset root.");

  if (root->getProperty("format").toString() != "teul.state_preset") {
    return juce::Result::fail(
        "State preset load failed: unsupported preset format.");
  }

  nodeStatesOut.clear();
  summaryOut = {};
  summaryOut.presetName = root->getProperty("preset_name").toString();
  summaryOut.targetGraphName = root->getProperty("target_graph_name").toString();
  hydrateSummaryFromJson(summaryOut, root->getProperty("summary"));
  if (summaryOut.presetName.isEmpty())
    summaryOut.presetName = file.getFileNameWithoutExtension();

  if (auto *nodeStatesArray = root->hasProperty("node_states")
                                  ? root->getProperty("node_states").getArray()
                                  : nullptr) {
    for (const auto &nodeStateVar : *nodeStatesArray) {
      TStatePresetNodeState nodeState;
      if (jsonToNodeState(nodeState, nodeStateVar))
        nodeStatesOut.push_back(std::move(nodeState));
    }
  }

  if (summaryOut.nodeStateCount <= 0)
    summaryOut.nodeStateCount = (int)nodeStatesOut.size();
  if (summaryOut.paramValueCount <= 0)
    summaryOut.paramValueCount = countParamValues(nodeStatesOut);

  return juce::Result::ok();
}

juce::Result TStatePresetIO::applyToDocument(TGraphDocument &document,
                                             const juce::File &file,
                                             TStatePresetApplyReport *reportOut) {
  std::vector<TStatePresetNodeState> nodeStates;
  TStatePresetSummary summary;
  const auto loadResult = loadFromFile(nodeStates, summary, file);
  if (loadResult.failed())
    return loadResult;

  TStatePresetApplyReport report;
  report.summary = summary;

  for (const auto &nodeState : nodeStates) {
    auto *node = findTargetNode(document, nodeState);
    if (node == nullptr) {
      ++report.skippedNodeCount;
      continue;
    }

    node->bypassed = nodeState.bypassed;
    for (const auto &[key, value] : nodeState.params) {
      node->params[key] = value;
      ++report.appliedParamValueCount;
    }

    report.appliedNodeIds.push_back(node->nodeId);
    ++report.appliedNodeCount;
  }

  if (report.appliedNodeCount <= 0) {
    if (reportOut != nullptr)
      *reportOut = report;
    return juce::Result::fail(
        "State preset apply failed: no matching nodes were found.");
  }

  document.touch(false);

  if (reportOut != nullptr)
    *reportOut = report;

  return juce::Result::ok();
}

} // namespace Teul
