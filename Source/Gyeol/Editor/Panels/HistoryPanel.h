#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

namespace Gyeol::Ui::Panels
{
    class HistoryPanel : public juce::Component,
                         private juce::ListBoxModel
    {
    public:
        struct StackRow
        {
            juce::String label;
            bool current = false;
            bool future = false;
            int stateIndex = 0;
        };

        HistoryPanel();
        ~HistoryPanel() override;

        void setStackState(int undoDepth, int redoDepth, uint64_t historySerial);
        void appendEntry(const juce::String& action, const juce::String& detail = {});
        void clear();
        void setCollapsed(bool shouldCollapse);
        bool isCollapsed() const noexcept;
        void setCanUndoRedo(bool canUndo, bool canRedo);
        void setMaxEntries(int maxEntriesIn);
        void setCollapseToggledCallback(std::function<void(bool)> callback);

        void setUndoRequestedCallback(std::function<void()> callback);
        void setRedoRequestedCallback(std::function<void()> callback);

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        int getNumRows() override;
        void paintListBoxItem(int rowNumber,
                              juce::Graphics& g,
                              int width,
                              int height,
                              bool rowIsSelected) override;

        std::vector<StackRow> rows;
        std::vector<juce::String> undoEventLabels;
        std::vector<juce::String> redoEventLabels;
        int currentRow = -1;
        int cachedUndoDepth = 0;
        int cachedRedoDepth = 0;
        uint64_t cachedHistorySerial = 0;
        uint64_t lastAnnotatedHistorySerial = 0;
        bool collapsed = true;

        juce::Label titleLabel;
        juce::Label summaryLabel;
        juce::TextButton collapseToggleButton { ">" };
        juce::TextButton undoButton { "Undo" };
        juce::TextButton redoButton { "Redo" };
        juce::TextButton clearButton { "Sync" };
        juce::ListBox listBox;

        std::function<void(bool)> onCollapseToggled;
        std::function<void()> onUndoRequested;
        std::function<void()> onRedoRequested;

        void updateCollapsedVisualState();
    };
}
