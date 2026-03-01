#pragma once

#include "Gyeol/Public/DocumentHandle.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <JuceHeader.h>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class ValidationPanel : public juce::Component,
                            private juce::ListBoxModel
    {
    public:
        enum class IssueSeverity
        {
            info,
            warning,
            error
        };

        struct Issue
        {
            IssueSeverity severity = IssueSeverity::info;
            juce::String title;
            juce::String message;
        };

        ValidationPanel(DocumentHandle& documentIn, const Widgets::WidgetRegistry& registryIn);
        ~ValidationPanel() override;

        void markDirty();
        void refreshValidation();
        bool autoRefreshEnabled() const noexcept;
        void setAutoRefreshEnabled(bool enabled);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        int getNumRows() override;
        void paintListBoxItem(int rowNumber,
                              juce::Graphics& g,
                              int width,
                              int height,
                              bool rowIsSelected) override;

        void rebuildIssues();
        void pushIssue(IssueSeverity severity, const juce::String& title, const juce::String& message);
        static juce::Colour colorForSeverity(IssueSeverity severity);
        static juce::String labelForSeverity(IssueSeverity severity);

        DocumentHandle& document;
        const Widgets::WidgetRegistry& registry;
        std::vector<Issue> issues;
        bool dirty = true;
        bool autoRefresh = true;

        juce::Label titleLabel;
        juce::Label summaryLabel;
        juce::ToggleButton autoRefreshToggle { "Auto" };
        juce::TextButton runButton { "Run Validation" };
        juce::ListBox listBox;
    };
}
