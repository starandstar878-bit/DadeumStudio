// BOM
#include "MainComponent.h"
#include "Teul2/Document/TDocumentStore.h"

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

bool matchesBindingKey(const juce::String &text,
                       const juce::String &paramId) {
  const auto trimmed = text.trim();
  return trimmed.isNotEmpty() && trimmed.equalsIgnoreCase(paramId.trim());
}

bool expressionMentionsBindingKey(const juce::String &expression,
                                  const juce::String &paramId) {
  const auto normalizedParamId = paramId.trim();
  if (normalizedParamId.isEmpty())
    return false;

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

    if (matchesBindingKey(expression.substring(start, index), normalizedParamId))
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

juce::String buildTeul2BindingSummary(const Gyeol::DocumentHandle &document,
                                      const juce::String &paramId) {
  const auto normalizedParamId = paramId.trim();
  if (normalizedParamId.isEmpty())
    return {};

  const auto &snapshot = document.snapshot();

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
    if (!expressionMentionsBindingKey(binding.expression, normalizedParamId))
      continue;

    const auto *widget = findWidget(binding.targetWidgetId);
    auto item = (widget != nullptr ? gyeolWidgetDisplayName(*widget)
                                   : ("Widget #" + juce::String(binding.targetWidgetId))) +
                " [read " + binding.targetProperty.trim() + "]";
    if (!binding.enabled)
      item << " off";
    addItem(item);
  }

  for (const auto &binding : snapshot.runtimeBindings) {
    bool touchesParam = false;
    for (const auto &action : binding.actions) {
      switch (action.kind) {
      case Gyeol::RuntimeActionKind::setRuntimeParam:
      case Gyeol::RuntimeActionKind::adjustRuntimeParam:
      case Gyeol::RuntimeActionKind::toggleRuntimeParam:
        touchesParam = matchesBindingKey(action.paramKey, normalizedParamId);
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

static constexpr int kSessionAutosaveIntervalMs = 4000;

static juce::File resolveSessionDirectory() {
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

  return dir;
}

static bool writeSessionStateSnapshot(const juce::File &file,
                                      bool cleanShutdown) {
  const auto parent = file.getParentDirectory();
  if (!parent.exists() && !parent.createDirectory())
    return false;

  auto *root = new juce::DynamicObject();
  root->setProperty("cleanShutdown", cleanShutdown);
  root->setProperty("updatedAtMillis",
                    static_cast<juce::int64>(juce::Time::currentTimeMillis()));
  return file.replaceWithText(juce::JSON::toString(juce::var(root), true), false,
                              false, "\r\n");
}

static bool wasLastSessionShutdownClean(const juce::File &file) {
  if (!file.existsAsFile())
    return true;

  juce::var json;
  if (juce::JSON::parse(file.loadFileAsString(), json).failed())
    return true;

  const auto *root = json.getDynamicObject();
  if (root == nullptr)
    return true;

  const auto value = root->getProperty("cleanShutdown");
  if (value.isBool())
    return static_cast<bool>(value);

  const auto textValue = value.toString().trim();
  if (textValue.isEmpty())
    return true;

  return textValue.equalsIgnoreCase("true") || textValue == "1";
}

Teul::TDocumentNoticeLevel moreSevereNoticeLevel(
    Teul::TDocumentNoticeLevel lhs,
    Teul::TDocumentNoticeLevel rhs) noexcept {
  return static_cast<int>(lhs) >= static_cast<int>(rhs) ? lhs : rhs;
}

juce::String mergeNoticeDetail(const juce::String &first,
                               const juce::String &second) {
  juce::StringArray parts;
  auto addPart = [&parts](const juce::String &text) {
    const auto normalized = text.trim();
    if (normalized.isNotEmpty() && !parts.contains(normalized))
      parts.add(normalized);
  };

  addPart(first);
  addPart(second);
  return parts.joinIntoString(" | ");
}

// =============================================================================
//  MainComponent
// =============================================================================
MainComponent::MainComponent(AppServices &services) : appServices(services) {
  gyeolPage = Gyeol::createEditor();
  teulPage = Teul::createTeulEditor(
      &appServices.audioDeviceManager,
      [this](const juce::String &paramId) -> juce::String {
        if (gyeolPage == nullptr)
          return {};
        return buildTeul2BindingSummary(gyeolPage->document(), paramId);
      },
      [this]() -> std::uint64_t {
        if (gyeolPage == nullptr)
          return 0;
        return gyeolPage->document().historySerial();
      });

  addChildComponent(*gyeolPage);
  addChildComponent(*teulPage);

  pageTabBar = std::make_unique<AppPageTabBar>(currentPage);
  addAndMakeVisible(*pageTabBar);
  pageTabBar->onPageSelected = [this](AppPage page) { switchToPage(page); };

  switchToPage(currentPage);
  restoreSession();

  if (gyeolPage != nullptr)
    lastPersistedGyeolHistorySerial = gyeolPage->document().historySerial();
  if (teulPage != nullptr)
    lastPersistedTeulDocumentRevision =
        teulPage->document().getDocumentRevision();

  updateTeulSessionStatus();
  juce::ignoreUnused(writeSessionStateSnapshot(sessionStateFilePath(), false));
  startTimer(kSessionAutosaveIntervalMs);
  setSize(600, 400);
}

MainComponent::~MainComponent() {
  stopTimer();
  persistSession();
  juce::ignoreUnused(writeSessionStateSnapshot(sessionStateFilePath(), true));
}

void MainComponent::timerCallback() { persistSession(); }

void MainComponent::switchToPage(AppPage page) {
  currentPage = page;
  gyeolPage->setVisible(page == AppPage::Gyeol);
  teulPage->setVisible(page == AppPage::Teul2);
  resized();
}

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized() {
  auto bounds = getLocalBounds();

  if (pageTabBar != nullptr)
    pageTabBar->setBounds(bounds.removeFromBottom(40));

  if (gyeolPage != nullptr)
    gyeolPage->setBounds(bounds);
  if (teulPage != nullptr)
    teulPage->setBounds(bounds);
}

juce::File MainComponent::sessionFilePath() {
  return resolveSessionDirectory().getChildFile("autosave-session.json");
}

juce::File MainComponent::teulSessionFilePath() {
  return resolveSessionDirectory().getChildFile("autosave-teul2.teul2");
}

juce::File MainComponent::sessionStateFilePath() {
  return resolveSessionDirectory().getChildFile("autosave-session-state.json");
}

void MainComponent::updateTeulSessionStatus() const {
  if (teulPage == nullptr)
    return;

  Teul::TEditorSessionStatus status;
  status.lastPersistedDocumentRevision = lastPersistedTeulDocumentRevision;
  const auto autosaveFile = teulSessionFilePath();
  status.hasAutosaveSnapshot = autosaveFile.existsAsFile();
  if (status.hasAutosaveSnapshot)
    status.lastAutosaveTime = autosaveFile.getLastModificationTime();
  teulPage->setSessionStatus(status);
}

void MainComponent::restoreSession() {
  const auto previousSessionWasClean =
      wasLastSessionShutdownClean(sessionStateFilePath());
  if (!previousSessionWasClean) {
    DBG("[Session] Previous shutdown was not clean. Restoring autosave snapshots.");
  }

  if (gyeolPage != nullptr) {
    const auto file = sessionFilePath();
    if (file.existsAsFile()) {
      const auto result = gyeolPage->document().loadFromFile(file);
      if (result.failed()) {
        DBG("[Gyeol] Session restore failed: " + result.getErrorMessage());
      } else {
        gyeolPage->refreshFromDocument();
      }
    }
  }

  if (teulPage != nullptr) {
    const auto file = teulSessionFilePath();
    if (file.existsAsFile()) {
      if (!Teul::TFileIo::loadFromFile(teulPage->document(), file)) {
        DBG("[Teul2] Session restore failed: " + file.getFullPathName());
      } else {
        if (!previousSessionWasClean) {
          const auto existingNotice = teulPage->document().getTransientNotice();
          auto detail = juce::String(
              "Previous shutdown was not clean; restored the latest autosave snapshot.");
          if (existingNotice.active) {
            detail = mergeNoticeDetail(detail, existingNotice.title);
            detail = mergeNoticeDetail(detail, existingNotice.detail);
          }

          const auto level =
              existingNotice.active
                  ? moreSevereNoticeLevel(existingNotice.level,
                                          Teul::TDocumentNoticeLevel::warning)
                  : Teul::TDocumentNoticeLevel::warning;
          const auto title =
              existingNotice.active
                  ? juce::String("Recovered Teul2 autosave with warnings")
                  : juce::String("Recovered Teul2 autosave");
          teulPage->document().setTransientNotice(level, title, detail);
        }

        teulPage->refreshFromDocument();
      }
    }
  }

  updateTeulSessionStatus();
}

void MainComponent::persistSession() const {
  auto ensureParentDirectory = [](const juce::File &file) {
    const auto parent = file.getParentDirectory();
    return parent.exists() || parent.createDirectory();
  };

  bool wroteAutosave = false;

  if (gyeolPage != nullptr) {
    const auto file = sessionFilePath();
    const auto historySerial = gyeolPage->document().historySerial();
    if (historySerial != lastPersistedGyeolHistorySerial || !file.existsAsFile()) {
      if (!ensureParentDirectory(file)) {
        DBG("[Gyeol] Session save failed: could not create autosave directory.");
      } else {
        const auto result = gyeolPage->document().saveToFile(file);
        if (result.failed()) {
          DBG("[Gyeol] Session save failed: " + result.getErrorMessage());
        } else {
          lastPersistedGyeolHistorySerial = historySerial;
          wroteAutosave = true;
        }
      }
    }
  }

  if (teulPage != nullptr) {
    const auto file = teulSessionFilePath();
    const auto documentRevision = teulPage->document().getDocumentRevision();
    if (documentRevision != lastPersistedTeulDocumentRevision ||
        !file.existsAsFile()) {
      if (!ensureParentDirectory(file)) {
        DBG("[Teul2] Session save failed: could not create autosave directory.");
      } else if (!Teul::TFileIo::saveToFile(teulPage->document(), file)) {
        DBG("[Teul2] Session save failed: " + file.getFullPathName());
      } else {
        lastPersistedTeulDocumentRevision = documentRevision;
        wroteAutosave = true;
      }
    }
  }

  if (wroteAutosave)
    juce::ignoreUnused(writeSessionStateSnapshot(sessionStateFilePath(), false));

  updateTeulSessionStatus();
}
