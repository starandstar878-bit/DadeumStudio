#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class PropertyPanel : public juce::Component
    {
    public:
        explicit PropertyPanel(DocumentHandle& documentIn);

        void setSelection(std::vector<WidgetId> selectionIds);
        void refreshFromDocument();

        void paint(juce::Graphics& g) override;

    private:
        DocumentHandle& document;
        std::vector<WidgetId> selectedIds;
    };
}
