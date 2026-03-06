// BOM
#include "MainComponent.h"

#include <algorithm>

namespace {

juce::String gyeolWidgetTypeLabel(Gyeol::WidgetType type) {
  switch (type) {
  case Gyeol::WidgetType::button:
    return "Button";
  case Gyeol::WidgetType::slider:
    return "Slider";
  case Gyeol::WidgetType::knob:
    return "Knob";
  case Gyeol::WidgetType::label:
    return "Label";
  case Gyeol::WidgetType::meter:
    return "Meter";
  case Gyeol::WidgetType::toggle:
    return "Toggle";
  case Gyeol::WidgetType::comboBox:
    return "Combo";
  case Gyeol::WidgetType::textInput:
    return "Text";
  }

  return "Widget";
}

juce::String trimmedWidgetProperty(const Gyeol::WidgetModel &widget,
                                   const char *key) {
  const juce::Identifier propertyKey(key);
  if (!widget.properties.contains(propertyKey))
    return {};
  return widget.properties[propertyKey].toString().trim();
}

juce::String gyeolWidgetDisplayName(const Gyeol::WidgetModel &widget) {
  for (const auto *key : {"name", "text", "title", "label"}) {
    const auto value = trimmedWidgetProperty(widget, key);
    if (value.isNotEmpty())
      return value + " (#" + juce::String(widget.id) + ")";
  }

  return gyeolWidgetTypeLabel(widget.type) + " #" + juce::String(widget.id);
}

bool isBindingIdentifierStart(juce::juce_wchar ch) noexcept {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

bool isBindingIdentifierBody(juce::juce_wchar ch) noexcept {
  return isBindingIdentifierStart(ch) || (ch >= '0' && ch <= '9') || ch == '.';
}

bool matchesBindingKey(const juce::String &text, const juce::StringArray &keys,
                       juce::String *matchedKey = nullptr) {
  const auto trimmed = text.trim();
  if (trimmed.isEmpty())
    return false;

  for (const auto &key : keys) {
    if (!trimmed.equalsIgnoreCase(key))
      continue;
    if (matchedKey != nullptr)
      *matchedKey = key;
    return true;
  }

  return false;
}

bool expressionMentionsBindingKey(const juce::String &expression,
                                  const juce::StringArray &keys,
                                  juce::String *matchedKey = nullptr) {
  for (int index = 0; index < expression.length();) {
    const auto ch = expression[index];
    if (!isBindingIdentifierStart(ch)) {
      ++index;
      continue;
    }

    const int start = index++;
    while (index < expression.length() &&
           isBindingIdentifierBody(expression[index]))
      ++index;

    if (matchesBindingKey(expression.substring(start, index), keys, matchedKey))
      return true;
  }

  return false;
}

juce::String gyeolEventSummary(const juce::String &eventKey) {
  const auto normalized = eventKey.trim();
  if (normalized == "onClick")
    return "click";
  if (normalized == "onValueChanged")
    return "value";
  if (normalized == "onValueCommit")
    return "commit";
  if (normalized == "onToggleChanged")
    return "toggle";
  if (normalized == "onTextCommit")
    return "text";
  if (normalized == "onSelectionChanged")
    return "select";
  return normalized.isNotEmpty() ? normalized : juce::String("event");
}

juce::String buildTeulBindingSummary(const Gyeol::DocumentHandle &document,
                                     const juce::String &paramId,
                                     const juce::String &preferredBindingKey) {
  juce::StringArray candidateKeys;
  auto addKey = [&candidateKeys](const juce::String &key) {
    const auto trimmed = key.trim();
    if (trimmed.isNotEmpty() && !candidateKeys.contains(trimmed, true))
      candidateKeys.add(trimmed);
  };

  addKey(paramId);
  addKey(preferredBindingKey);
  if (candidateKeys.isEmpty())
    return {};

  const auto &snapshot = document.snapshot();
  const juce::String primaryKey = candidateKeys.isEmpty() ? juce::String() : candidateKeys[0];

  auto findWidget = [&snapshot](Gyeol::WidgetId id) -> const Gyeol::WidgetModel * {
    const auto it =
        std::find_if(snapshot.widgets.begin(), snapshot.widgets.end(),
                     [id](const Gyeol::WidgetModel &widget) { return widget.id == id; });
    return it != snapshot.widgets.end() ? &(*it) : nullptr;
  };

  juce::StringArray items;
  auto addItem = [&items](const juce::String &text) {
    if (text.isNotEmpty() && !items.contains(text, true))
      items.add(text);
  };

  for (const auto &binding : snapshot.propertyBindings) {
    juce::String matchedKey;
    if (!expressionMentionsBindingKey(binding.expression, candidateKeys, &matchedKey))
      continue;

    const auto *widget = findWidget(binding.targetWidgetId);
    auto item = (widget != nullptr ? gyeolWidgetDisplayName(*widget)
                                   : ("Widget #" + juce::String(binding.targetWidgetId))) +
                " [read " + binding.targetProperty.trim() + "]";
    if (!binding.enabled)
      item << " off";
    if (primaryKey.isNotEmpty() && matchedKey.isNotEmpty() &&
        !matchedKey.equalsIgnoreCase(primaryKey))
      item << " via " << matchedKey;
    addItem(item);
  }

  for (const auto &binding : snapshot.runtimeBindings) {
    juce::String matchedKey;
    bool touchesParam = false;
    for (const auto &action : binding.actions) {
      switch (action.kind) {
      case Gyeol::RuntimeActionKind::setRuntimeParam:
      case Gyeol::RuntimeActionKind::adjustRuntimeParam:
      case Gyeol::RuntimeActionKind::toggleRuntimeParam:
        touchesParam = matchesBindingKey(action.paramKey, candidateKeys, &matchedKey);
        break;
      default:
        break;
      }

      if (touchesParam)
        break;
    }

    if (!touchesParam)
      continue;

    const auto *widget = findWidget(binding.sourceWidgetId);
    auto item = (widget != nullptr ? gyeolWidgetDisplayName(*widget)
                                   : ("Widget #" + juce::String(binding.sourceWidgetId))) +
                " [write " + gyeolEventSummary(binding.eventKey) + "]";
    if (!binding.enabled)
      item << " off";
    if (primaryKey.isNotEmpty() && matchedKey.isNotEmpty() &&
        !matchedKey.equalsIgnoreCase(primaryKey))
      item << " via " << matchedKey;
    addItem(item);
  }

  if (items.isEmpty())
    return {};

  constexpr int kPreviewCount = 3;
  juce::String summary;
  const auto previewCount = juce::jmin(kPreviewCount, items.size());
  for (int i = 0; i < previewCount; ++i) {
    if (i > 0)
      summary << ", ";
    summary << items[i];
  }

  if (items.size() > previewCount)
    summary << " +" << juce::String(items.size() - previewCount) << " more";

  return summary;
}

} // namespace

// =============================================================================
//  MainComponent
// =============================================================================
MainComponent::MainComponent(AppServices &services) : appServices(services) {
  // ------------------------------------------------------------------
  //  ??륁뵠筌왖 ??밴쉐 (addChildComponent ??筌ｌ꼷?????ｊ볼????됱벉)
  // ------------------------------------------------------------------
  gyeolPage = Gyeol::createEditor();
  teulPage = Teul::createEditor(
      &appServices.audioDeviceManager,
      [this](const juce::String &paramId,
             const juce::String &preferredBindingKey) -> juce::String {
        if (gyeolPage == nullptr)
          return {};
        return buildTeulBindingSummary(gyeolPage->document(), paramId,
                                       preferredBindingKey);
      });

  addChildComponent(*gyeolPage);
  addChildComponent(*teulPage);

  // ------------------------------------------------------------------
  //  ??而???밴쉐 獄??꾩뮆媛??怨뚭퍙
  // ------------------------------------------------------------------
  pageTabBar = std::make_unique<AppPageTabBar>(currentPage);
  addAndMakeVisible(*pageTabBar);

  pageTabBar->onPageSelected = [this](AppPage page) { switchToPage(page); };

  // ------------------------------------------------------------------
  //  ?λ뜃由???륁뵠筌왖 ??뽮쉐??
  // ------------------------------------------------------------------
  switchToPage(currentPage);

  // ------------------------------------------------------------------
  //  ?紐꾨?癰귣벊??(Gyeol ?얜챷苑?
  //  TODO [Phase 1]: Teul 域밸챶????얜챷苑????ｍ뜞 癰귣벊??
  // ------------------------------------------------------------------
  restoreSession();

  setSize(600, 400);
}

MainComponent::~MainComponent() { persistSession(); }

// =============================================================================
//  ??륁뵠筌왖 ?袁れ넎
// =============================================================================
void MainComponent::switchToPage(AppPage page) {
  currentPage = page;

  //  ??ｋ┛疫?/ 癰귣똻?졿묾怨뺤춸 ??뺣뼄 ???紐꾨뮞??곷뮞????? ???댘??? ??놁벉.
  //  ??Teul ??TGraphRuntime ?? ??ｊ볼?紐껊즲 ??삳탵????살쟿??뽯퓠???④쑴????덉삂??뺣뼄.
  gyeolPage->setVisible(page == AppPage::Gyeol);
  teulPage->setVisible(page == AppPage::Teul);

  resized();
}

// =============================================================================
//  paint / resized
// =============================================================================
void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized() {
  auto bounds = getLocalBounds();

  // TODO [UI]: GlobalToolbar 揶쎛 ??룸┛筌??袁⑥삋 雅뚯눘苑???곸젫
  // if (globalToolbar != nullptr)
  //     globalToolbar->setBounds (bounds.removeFromTop (42));

  // ??롫뼊 ??而?
  if (pageTabBar != nullptr)
    pageTabBar->setBounds(bounds.removeFromBottom(40));

  // ??롢돢筌왖 = ??륁뵠筌왖 ?怨몃열 (筌뤴뫀諭???륁뵠筌왖 ??덉뵬 ??由경에???쇱젟)
  if (gyeolPage != nullptr)
    gyeolPage->setBounds(bounds);
  if (teulPage != nullptr)
    teulPage->setBounds(bounds);
}

// =============================================================================
//  ?紐꾨?????/ 癰귣벊??
// =============================================================================
juce::File MainComponent::sessionFilePath() {
  auto dir =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getChildFile("DadeumStudio");
  if (!dir.exists())
    dir.createDirectory();

  if (!dir.exists()) {
    dir = juce::File::getCurrentWorkingDirectory()
              .getChildFile("Builds")
              .getChildFile("GyeolSession");
    if (!dir.exists())
      dir.createDirectory();
  }

  return dir.getChildFile("autosave-session.json");
}

void MainComponent::restoreSession() {
  if (gyeolPage == nullptr)
    return;

  const auto file = sessionFilePath();
  if (!file.existsAsFile())
    return;

  const auto result = gyeolPage->document().loadFromFile(file);
  if (result.failed()) {
    DBG("[Gyeol] Session restore failed: " + result.getErrorMessage());
    return;
  }

  gyeolPage->refreshFromDocument();

  // TODO [Phase 1]: Teul ?紐꾨????ｍ뜞 癰귣벊??
  //   auto teulFile = sessionFilePath().getSiblingFile("autosave-teul.json");
  //   teulPage->document().loadFromFile(teulFile);
}

void MainComponent::persistSession() const {
  if (gyeolPage == nullptr)
    return;

  const auto file = sessionFilePath();
  const auto parent = file.getParentDirectory();
  if (!parent.exists())
    parent.createDirectory();

  const auto result = gyeolPage->document().saveToFile(file);
  if (result.failed())
    DBG("[Gyeol] Session save failed: " + result.getErrorMessage());

  // TODO [Phase 1]: Teul ?紐꾨????ｍ뜞 ????
  //   auto teulFile = file.getSiblingFile("autosave-teul.json");
  //   teulPage->document().saveToFile(teulFile);
}
