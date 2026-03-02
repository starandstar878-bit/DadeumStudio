#pragma once

#include <JuceHeader.h>
#include <map>

class GyeolPhase6SmokeComponent : public juce::Component
{
public:
    GyeolPhase6SmokeComponent();
    ~GyeolPhase6SmokeComponent() override = default;

    void resized() override;

private:
    juce::TextButton button_1001;
    juce::Slider slider_1002;
    juce::Label label_1003;

    // Runtime bridge (Phase 6).
    void initializeRuntimeBridge();
    void dispatchRuntimeEvent(juce::int64 sourceWidgetId, const juce::String& eventKey, const juce::var& payload = {});
    void applyPropertyBindings();
    bool applyRuntimeAction(const juce::var& action, const juce::var& payload, bool& runtimeStateChanged);
    juce::Component* findRuntimeWidget(juce::int64 widgetId) const;
    bool setWidgetPropertyById(juce::int64 widgetId, const juce::String& propertyKey, const juce::var& value);
    std::map<juce::String, juce::var> runtimeParams;
    std::map<juce::String, juce::String> runtimeParamTypes;
    juce::Array<juce::var> propertyBindings;
    juce::Array<juce::var> runtimeBindings;
    std::map<juce::int64, juce::Component*> runtimeWidgetById;
    std::map<juce::int64, bool> runtimeButtonDownStates;
    bool runtimeBridgeMutating = false;
    bool runtimeBridgeLoaded = false;
    juce::String lastRuntimeBridgeError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GyeolPhase6SmokeComponent)
};
