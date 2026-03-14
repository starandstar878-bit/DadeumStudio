#include "Teul2/Editor/Panels/TInspectorPanel.h"
#include "Teul2/Editor/Theme/TeulPalette.h"
#include "Teul2/Editor/TIssueState.h"
#include "Teul2/Runtime/AudioGraph/TGraphCompiler.h"
#include <algorithm>

namespace Teul {

// --- Helper Functions (ported from TTeulEditor.cpp) ---

namespace {
    static void configureEditableField(juce::TextEditor& ed) {
        ed.setMultiLine(false);
        ed.setReturnKeyStartsNewLine(false);
        ed.setReadOnly(false);
        ed.setIndents(4, 0);
        ed.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        ed.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        ed.setColour(juce::TextEditor::focusedOutlineColourId, TeulPalette::AccentSlate().withAlpha(0.3f));
    }

    static void configureReadOnlyBox(juce::TextEditor& ed) {
        ed.setMultiLine(true);
        ed.setReadOnly(true);
        ed.setCaretVisible(false);
        ed.setColour(juce::TextEditor::backgroundColourId, TeulPalette::PanelBackgroundAlt());
        ed.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    }

    static void configureToggle(juce::ToggleButton& toggle, const juce::String& text) {
        toggle.setButtonText(text);
        toggle.setColour(juce::ToggleButton::textColourId, TeulPalette::PanelText());
        toggle.setColour(juce::ToggleButton::tickColourId, TeulPalette::AccentSky());
    }

    static void configureCombo(juce::ComboBox& combo) {
        combo.setEditableText(false);
        combo.setJustificationType(juce::Justification::centredLeft);
        combo.setColour(juce::ComboBox::backgroundColourId, TeulPalette::PanelBackgroundAlt());
        combo.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    }
} // namespace

// --- PlaceholderView Implementation ---

class TInspectorPanel::PlaceholderView : public juce::Component {
public:
    explicit PlaceholderView(TTeulDocument& doc) : document(doc) {
        titleLabel.setText("Teul Patch Editor", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, TeulPalette::PanelText());
        addAndMakeVisible(titleLabel);

        statsLabel.setColour(juce::Label::textColourId, TeulPalette::PanelTextMuted());
        statsLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(statsLabel);
        
        refresh();
    }

    void refresh() {
        const int nodeCount = (int)document.nodes.size();
        const int connCount = (int)document.connections.size();
        const int frameCount = (int)document.frames.size();

        juce::String stats;
        stats << "Project Statistics:\n"
              << " - Nodes: " << nodeCount << "\n"
              << " - Connections: " << connCount << "\n"
              << " - Frames: " << frameCount << "\n\n";

        const int issues = countTotalIssues();
        if (issues > 0) {
            stats << "Status: " << issues << " configuration issues detected.";
            statsLabel.setColour(juce::Label::textColourId, TeulPalette::AccentOrange());
        } else {
            stats << "Status: All systems nominal.";
            statsLabel.setColour(juce::Label::textColourId, TeulPalette::PanelTextMuted());
        }

        statsLabel.setText(stats, juce::dontSendNotification);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(20);
        titleLabel.setBounds(area.removeFromTop(40));
        area.removeFromTop(10);
        statsLabel.setBounds(area);
    }

private:
    int countTotalIssues() const {
        int count = 0;
        for (const auto& endpoint : document.controlState.inputEndpoints)
            if (hasIssueState(issueStateForEndpoint(endpoint))) count++;
        for (const auto& endpoint : document.controlState.outputEndpoints)
            if (hasIssueState(issueStateForEndpoint(endpoint))) count++;
        for (const auto& source : document.controlState.sources)
            if (hasIssueState(issueStateForControlSource(source))) count++;
        return count;
    }

    static bool hasIncompleteRailPort(const TSystemRailPort& port) {
        return port.portId.isEmpty() || port.displayName.isEmpty();
    }

    static TIssueState issueStateForEndpoint(const TSystemRailEndpoint& endpoint) {
        TIssueState state = TIssueState::none;
        if (endpoint.degraded) state = mergeIssueState(state, TIssueState::degraded);
        if (endpoint.stereo && endpoint.ports.size() == 1) state = mergeIssueState(state, TIssueState::degraded);
        if (endpoint.ports.empty() || std::any_of(endpoint.ports.begin(), endpoint.ports.end(), hasIncompleteRailPort)) {
            state = mergeIssueState(state, TIssueState::invalidConfig);
        }
        if (endpoint.missing) state = mergeIssueState(state, TIssueState::missing);
        return state;
    }

    static TIssueState issueStateForControlSource(const TControlSource& source) {
        TIssueState state = TIssueState::none;
        if (source.degraded) state = mergeIssueState(state, TIssueState::degraded);
        if (source.ports.empty() || std::any_of(source.ports.begin(), source.ports.end(), [](const TControlSourcePort& p) {
            return p.portId.isEmpty() || p.displayName.isEmpty();
        })) {
            state = mergeIssueState(state, TIssueState::invalidConfig);
        }
        if (source.missing) state = mergeIssueState(state, TIssueState::missing);
        return state;
    }

    TTeulDocument& document;
    juce::Label titleLabel;
    juce::Label statsLabel;
};

// --- ControlSourceView Implementation ---

class TInspectorPanel::ControlSourceView : public juce::Component {
public:
    explicit ControlSourceView(TTeulDocument& doc, const juce::String& sId) 
        : document(doc), sourceId(sId) 
    {
        addAndMakeVisible(headerLabel);
        headerLabel.setText("Control Source", juce::dontSendNotification);
        headerLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        headerLabel.setColour(juce::Label::textColourId, TeulPalette::AccentPurple());

        addAndMakeVisible(nameLabel);
        nameLabel.setText("Display Name:", juce::dontSendNotification);
        addAndMakeVisible(sourceNameEditor);
        configureEditableField(sourceNameEditor);
        sourceNameEditor.onReturnKey = [this] { commitName(); };
        sourceNameEditor.onFocusLost = [this] { commitName(); };
        sourceNameEditor.onEscapeKey = [this] { refresh(); };

        addAndMakeVisible(kindLabel);
        kindLabel.setText("Kind:", juce::dontSendNotification);
        addAndMakeVisible(kindCombo);
        configureCombo(kindCombo);
        kindCombo.addItem("Expression", 1);
        kindCombo.addItem("Footswitch", 2);
        kindCombo.addItem("Trigger", 3);
        kindCombo.addItem("MIDI CC", 4);
        kindCombo.addItem("MIDI Note", 5);
        kindCombo.addItem("Macro", 6);
        kindCombo.onChange = [this] { commitKind(); };

        addAndMakeVisible(portsLabel);
        portsLabel.setText("Available Ports:", juce::dontSendNotification);
        addAndMakeVisible(portsBox);
        configureReadOnlyBox(portsBox);

        refresh();
    }

    void refresh() {
        auto* source = document.controlState.findSource(sourceId);
        if (!source) return;

        juce::ScopedValueSetter<bool> setter(suppressCallbacks, true);
        sourceNameEditor.setText(source->displayName, juce::dontSendNotification);
        kindCombo.setSelectedId((int)source->kind + 1, juce::dontSendNotification);

        juce::StringArray portLines;
        for (const auto& p : source->ports) {
            portLines.add(p.displayName + " (" + p.portId + ")");
        }
        portsBox.setText(portLines.joinIntoString("\n"), juce::dontSendNotification);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(20);
        headerLabel.setBounds(area.removeFromTop(30));
        area.removeFromTop(15);

        auto row = area.removeFromTop(24);
        nameLabel.setBounds(row.removeFromLeft(100));
        sourceNameEditor.setBounds(row);
        area.removeFromTop(10);

        row = area.removeFromTop(24);
        kindLabel.setBounds(row.removeFromLeft(100));
        kindCombo.setBounds(row);
        area.removeFromTop(20);

        portsLabel.setBounds(area.removeFromTop(20));
        portsBox.setBounds(area.removeFromTop(100));
    }

private:
    void commitName() {
        if (suppressCallbacks) return;
        if (auto* source = document.controlState.findSource(sourceId)) {
            const auto newName = sourceNameEditor.getText().trim();
            if (newName != source->displayName) {
                source->displayName = newName;
                document.touch(true);
            }
        }
    }

    void commitKind() {
        if (suppressCallbacks) return;
        const auto newKind = (TControlSourceKind)(kindCombo.getSelectedId() - 1);
        document.controlState.reconfigureSourceShape(sourceId, newKind, TControlSourceMode::continuous);
        refresh();
    }

    TTeulDocument& document;
    juce::String sourceId;
    bool suppressCallbacks = false;

    juce::Label headerLabel;
    juce::Label nameLabel;
    juce::TextEditor sourceNameEditor;
    juce::Label kindLabel;
    juce::ComboBox kindCombo;
    juce::Label portsLabel;
    juce::TextEditor portsBox;
};

// --- SystemEndpointView Implementation ---

class TInspectorPanel::SystemEndpointView : public juce::Component {
public:
    explicit SystemEndpointView(TTeulDocument& doc, const juce::String& eId)
        : document(doc), endpointId(eId)
    {
        addAndMakeVisible(headerLabel);
        headerLabel.setText("System Endpoint", juce::dontSendNotification);
        headerLabel.setFont(juce::Font(18.0f, juce::Font::bold));
        headerLabel.setColour(juce::Label::textColourId, TeulPalette::AccentSky());

        addAndMakeVisible(infoLabel);
        infoLabel.setColour(juce::Label::textColourId, TeulPalette::PanelText());
        
        addAndMakeVisible(portsLabel);
        portsLabel.setText("Hardware Ports:", juce::dontSendNotification);
        addAndMakeVisible(portsBox);
        configureReadOnlyBox(portsBox);

        refresh();
    }

    void refresh() {
        auto* endpoint = document.controlState.findEndpoint(endpointId);
        if (!endpoint) return;

        juce::String info;
        info << "Name: " << endpoint->displayName << "\n"
             << "Kind: " << (endpoint->kind == TSystemRailEndpointKind::audioInput ? "Audio Input" : "Audio Output") << "\n"
             << "Stereo: " << (endpoint->stereo ? "Yes" : "No");
        infoLabel.setText(info, juce::dontSendNotification);

        juce::StringArray portLines;
        for (const auto& p : endpoint->ports) {
            portLines.add(p.displayName + " [" + p.portId + "]");
        }
        portsBox.setText(portLines.joinIntoString("\n"), juce::dontSendNotification);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(20);
        headerLabel.setBounds(area.removeFromTop(30));
        area.removeFromTop(15);
        infoLabel.setBounds(area.removeFromTop(80));
        area.removeFromTop(15);
        portsLabel.setBounds(area.removeFromTop(20));
        portsBox.setBounds(area.removeFromTop(100));
    }

private:
    TTeulDocument& document;
    juce::String endpointId;
    juce::Label headerLabel;
    juce::Label infoLabel;
    juce::Label portsLabel;
    juce::TextEditor portsBox;
};

// --- TInspectorPanel Implementation ---

TInspectorPanel::TInspectorPanel(TTeulDocument& doc, const TNodeRegistry& registry)
    : document(doc), nodeRegistry(registry)
{
    updateActiveView();
}

TInspectorPanel::~TInspectorPanel() = default;

void TInspectorPanel::setSelection(InspectorType type, const juce::String& targetId) {
    if (currentType == type && currentTargetId == targetId) return;
    
    currentType = type;
    currentTargetId = targetId;
    updateActiveView();
}

void TInspectorPanel::refreshFromDocument() {
    if (auto* p = dynamic_cast<PlaceholderView*>(activeView.get())) p->refresh();
    else if (auto* c = dynamic_cast<ControlSourceView*>(activeView.get())) c->refresh();
    else if (auto* s = dynamic_cast<SystemEndpointView*>(activeView.get())) s->refresh();
}

void TInspectorPanel::setDocumentChangedCallback(std::function<void()> callback) {
    onDocumentChanged = std::move(callback);
}

void TInspectorPanel::setLayoutChangedCallback(std::function<void()> callback) {
    onLayoutChanged = std::move(callback);
}

void TInspectorPanel::paint(juce::Graphics& g) {
    g.fillAll(TeulPalette::PanelBackground());
    g.setColour(TeulPalette::PanelBackgroundAlt());
    g.drawRect(getLocalBounds(), 1);
}

void TInspectorPanel::resized() {
    if (activeView) activeView->setBounds(getLocalBounds());
}

void TInspectorPanel::updateActiveView() {
    activeView.reset();

    switch (currentType) {
        case InspectorType::ControlSource:
            activeView = std::make_unique<ControlSourceView>(document, currentTargetId);
            break;
        case InspectorType::SystemEndpoint:
            activeView = std::make_unique<SystemEndpointView>(document, currentTargetId);
            break;
        case InspectorType::None:
        default:
            activeView = std::make_unique<PlaceholderView>(document);
            break;
    }

    addAndMakeVisible(activeView.get());
    resized();
    if (onLayoutChanged) onLayoutChanged();
}

} // namespace Teul
