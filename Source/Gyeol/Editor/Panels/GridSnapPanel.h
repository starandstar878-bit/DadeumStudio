#pragma once

#include "Gyeol/Editor/Interaction/SnapEngine.h"
#include <JuceHeader.h>
#include <functional>

namespace Gyeol::Ui::Panels
{
    class GridSnapPanel : public juce::Component
    {
    public:
        GridSnapPanel();

        void setSettings(const Interaction::SnapSettings& settingsIn);
        const Interaction::SnapSettings& settings() const noexcept;

        void setSettingsChangedCallback(std::function<void(const Interaction::SnapSettings&)> callback);

        void paint(juce::Graphics& g) override;

    private:
        void notifySettingsChanged() const;

        Interaction::SnapSettings snapSettings;
        std::function<void(const Interaction::SnapSettings&)> onSettingsChanged;
    };
}
