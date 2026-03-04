const fs = require('fs');

const path = 'Source/Gyeol/Editor/Panels/EventActionPanel.cpp';
let content = fs.readFileSync(path, 'utf8');

content = content.replace(/juce::FontOptions\(([\d\.]+)f/g, (match, p1) => {
    let size = parseFloat(p1);
    size += 1.5;
    return \juce::FontOptions(\f\;
});

content = content.replace(/bindingList\.setRowHeight\(\d+\)/g, 'bindingList.setRowHeight(58)');
content = content.replace(/actionList\.setRowHeight\(\d+\)/g, 'actionList.setRowHeight(50)');
content = content.replace(/runtimeParamList\.setRowHeight\(\d+\)/g, 'runtimeParamList.setRowHeight(50)');
content = content.replace(/propertyBindingList\.setRowHeight\(\d+\)/g, 'propertyBindingList.setRowHeight(44)');

const actionPaintPattern = /void EventActionPanel::ActionListModel::paintListBoxItem[\\s\\S]*?g\\.drawFittedText\\(summary, area, juce::Justification::centredLeft, 1\\);\\n\\}/;
const newActionPaint = \oid EventActionPanel::ActionListModel::paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected) {
  const auto *binding = owner.selectedBinding();
  if (binding == nullptr || rowNumber < 0 || rowNumber >= static_cast<int>(binding->actions.size()))
    return;

  const auto &action = binding->actions[static_cast<size_t>(rowNumber)];
  const auto accent = actionAccent(action.kind);

  auto card = juce::Rectangle<float>(1.0f, 1.0f, static_cast<float>(width) - 2.0f, static_cast<float>(height) - 2.0f);
  drawCardBackground(g, card, rowIsSelected, accent);
  drawLeftAccentBar(g, card, accent, rowIsSelected);

  auto area = card.toNearestInt().reduced(10, 6);
  auto topRow = area.removeFromTop(20);

  const auto getKoLabel = [](RuntimeActionKind k) -> juce::String {
    switch (k) {
    case RuntimeActionKind::setRuntimeParam: return juce::String::fromUTF8(u8"\\\\uD30C\\\\uB77C\\\\uBBF8\\\\uD130 \\\\uC124\\\\uC815");
    case RuntimeActionKind::adjustRuntimeParam: return juce::String::fromUTF8(u8"\\\\uD30C\\\\uB77C\\\\uBBF8\\\\uD130 \\\\uC870\\\\uC815");
    case RuntimeActionKind::toggleRuntimeParam: return juce::String::fromUTF8(u8"\\\\uD30C\\\\uB77C\\\\uBBF8\\\\uD130 \\\\uD1A0\\\\uAE00");
    case RuntimeActionKind::setNodeProps: return juce::String::fromUTF8(u8"\\\\uC18D\\\\uC131 \\\\uBCC0\\\\uACBD");
    case RuntimeActionKind::setNodeBounds: return juce::String::fromUTF8(u8"\\\\uC704\\\\uCE58/\\\\uD06C\\\\uAE30 \\\\uBCC0\\\\uACBD");
    }
    return {};
  };

  const auto title = juce::String(rowNumber + 1) + ". " + getKoLabel(action.kind);
  
  g.setColour(palette(GyeolPalette::TextPrimary));
  g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
  g.drawFittedText(title, topRow, juce::Justification::centredLeft, 1);

  auto summary = owner.actionSummary(action);
  g.setColour(palette(GyeolPalette::TextSecondary));
  g.setFont(juce::FontOptions(10.5f));
  g.drawFittedText(summary, area, juce::Justification::centredLeft, 1);
}\;

content = content.replace(actionPaintPattern, newActionPaint);
fs.writeFileSync(path, content, 'utf8');

console.log('Script done.');
