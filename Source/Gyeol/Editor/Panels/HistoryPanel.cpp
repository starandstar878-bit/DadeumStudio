#include "Gyeol/Editor/Panels/HistoryPanel.h"

#include <algorithm>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        bool isUndoRedoLabel(const juce::String& action)
        {
            const auto normalized = action.trim().toLowerCase();
            return normalized == "undo" || normalized == "redo";
        }

        juce::String makeEventLabel(const juce::String& action, const juce::String& detail)
        {
            const auto cleanAction = action.trim();
            const auto cleanDetail = detail.trim();
            if (cleanAction.isEmpty())
                return cleanDetail;
            if (cleanDetail.isEmpty())
                return cleanAction;
            return cleanAction + " - " + cleanDetail;
        }
    }

    HistoryPanel::HistoryPanel()
    {
        titleLabel.setText("History", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(192, 200, 214));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        summaryLabel.setText("Undo 0 | Redo 0", juce::dontSendNotification);
        summaryLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 170, 186));
        summaryLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(summaryLabel);

        undoButton.onClick = [this]
        {
            if (onUndoRequested != nullptr)
                onUndoRequested();
        };
        redoButton.onClick = [this]
        {
            if (onRedoRequested != nullptr)
                onRedoRequested();
        };
        clearButton.onClick = [this]
        {
            setStackState(cachedUndoDepth, cachedRedoDepth, cachedHistorySerial + 1);
        };
        collapseToggleButton.onClick = [this]
        {
            setCollapsed(!collapsed);
        };

        addAndMakeVisible(collapseToggleButton);
        addAndMakeVisible(undoButton);
        addAndMakeVisible(redoButton);
        addAndMakeVisible(clearButton);

        listBox.setModel(this);
        listBox.setRowHeight(26);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(17, 23, 31));
        listBox.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(44, 52, 66));
        addAndMakeVisible(listBox);

        setCanUndoRedo(false, false);
        updateCollapsedVisualState();
        setStackState(0, 0, 1);
    }

    HistoryPanel::~HistoryPanel()
    {
        listBox.setModel(nullptr);
    }

    void HistoryPanel::setStackState(int undoDepth, int redoDepth, uint64_t historySerial)
    {
        undoDepth = std::max(0, undoDepth);
        redoDepth = std::max(0, redoDepth);

        if (cachedUndoDepth == undoDepth
            && cachedRedoDepth == redoDepth
            && cachedHistorySerial == historySerial)
        {
            return;
        }

        const auto previousUndoDepth = cachedUndoDepth;
        const auto previousRedoDepth = cachedRedoDepth;

        if (undoDepth == previousUndoDepth - 1 && redoDepth == previousRedoDepth + 1)
        {
            // Undo: move one label from undo top to redo top.
            juce::String movedLabel;
            if (!undoEventLabels.empty())
            {
                movedLabel = undoEventLabels.back();
                undoEventLabels.pop_back();
            }
            redoEventLabels.push_back(std::move(movedLabel));
        }
        else if (undoDepth == previousUndoDepth + 1 && redoDepth == previousRedoDepth - 1)
        {
            // Redo: move one label from redo top to undo top.
            juce::String movedLabel;
            if (!redoEventLabels.empty())
            {
                movedLabel = redoEventLabels.back();
                redoEventLabels.pop_back();
            }
            undoEventLabels.push_back(std::move(movedLabel));
        }
        else
        {
            if (redoDepth == 0)
                redoEventLabels.clear();

            if (undoEventLabels.size() > static_cast<size_t>(undoDepth))
                undoEventLabels.resize(static_cast<size_t>(undoDepth));
            else if (undoEventLabels.size() < static_cast<size_t>(undoDepth))
                undoEventLabels.resize(static_cast<size_t>(undoDepth));

            if (redoEventLabels.size() > static_cast<size_t>(redoDepth))
                redoEventLabels.resize(static_cast<size_t>(redoDepth));
            else if (redoEventLabels.size() < static_cast<size_t>(redoDepth))
                redoEventLabels.resize(static_cast<size_t>(redoDepth));
        }

        cachedUndoDepth = undoDepth;
        cachedRedoDepth = redoDepth;
        cachedHistorySerial = historySerial;

        rows.clear();
        rows.reserve(static_cast<size_t>(undoDepth + redoDepth + 1));

        int stateIndex = 0;
        for (int i = 0; i < undoDepth; ++i)
        {
            StackRow row;
            const auto defaultLabel = "Applied #" + juce::String(i + 1);
            if (i >= 0 && i < static_cast<int>(undoEventLabels.size()) && undoEventLabels[static_cast<size_t>(i)].isNotEmpty())
                row.label = undoEventLabels[static_cast<size_t>(i)];
            else
                row.label = defaultLabel;
            row.current = false;
            row.future = false;
            row.stateIndex = stateIndex++;
            rows.push_back(std::move(row));
        }

        StackRow current;
        current.label = "Current State";
        current.current = true;
        current.future = false;
        current.stateIndex = stateIndex++;
        rows.push_back(std::move(current));
        currentRow = undoDepth;

        for (int i = 0; i < redoDepth; ++i)
        {
            StackRow row;
            const auto defaultLabel = "Future #" + juce::String(i + 1);
            const auto labelIndex = redoDepth - 1 - i; // redo top appears closest to current row
            if (labelIndex >= 0
                && labelIndex < static_cast<int>(redoEventLabels.size())
                && redoEventLabels[static_cast<size_t>(labelIndex)].isNotEmpty())
            {
                row.label = redoEventLabels[static_cast<size_t>(labelIndex)];
            }
            else
            {
                row.label = defaultLabel;
            }
            row.current = false;
            row.future = true;
            row.stateIndex = stateIndex++;
            rows.push_back(std::move(row));
        }

        listBox.updateContent();
        if (currentRow >= 0 && currentRow < static_cast<int>(rows.size()))
        {
            listBox.selectRow(currentRow);
            listBox.scrollToEnsureRowIsOnscreen(currentRow);
        }

        summaryLabel.setText("Undo " + juce::String(undoDepth) + " | Redo " + juce::String(redoDepth),
                             juce::dontSendNotification);
        repaint();
    }

    void HistoryPanel::appendEntry(const juce::String& action, const juce::String& detail)
    {
        if (cachedUndoDepth <= 0)
            return;
        if (cachedHistorySerial == 0 || cachedHistorySerial == lastAnnotatedHistorySerial)
            return;
        if (isUndoRedoLabel(action))
            return;

        const auto label = makeEventLabel(action, detail);
        if (label.isEmpty())
            return;

        const auto topIndex = cachedUndoDepth - 1;
        if (topIndex < 0 || topIndex >= static_cast<int>(undoEventLabels.size()))
            return;

        undoEventLabels[static_cast<size_t>(topIndex)] = label;
        lastAnnotatedHistorySerial = cachedHistorySerial;
        setStackState(cachedUndoDepth, cachedRedoDepth, cachedHistorySerial + 1);
    }

    void HistoryPanel::clear()
    {
        // Formal mode: nothing to clear in the derived stack view.
        setStackState(cachedUndoDepth, cachedRedoDepth, cachedHistorySerial + 1);
    }

    void HistoryPanel::setCollapsed(bool shouldCollapse)
    {
        if (collapsed == shouldCollapse)
            return;

        collapsed = shouldCollapse;
        updateCollapsedVisualState();
        resized();
        repaint();

        if (onCollapseToggled != nullptr)
            onCollapseToggled(collapsed);
    }

    bool HistoryPanel::isCollapsed() const noexcept
    {
        return collapsed;
    }

    void HistoryPanel::setCanUndoRedo(bool canUndo, bool canRedo)
    {
        undoButton.setEnabled(canUndo);
        redoButton.setEnabled(canRedo);
    }

    void HistoryPanel::setMaxEntries(int)
    {
        // Formal mode does not use append-only log capacity.
    }

    void HistoryPanel::setCollapseToggledCallback(std::function<void(bool)> callback)
    {
        onCollapseToggled = std::move(callback);
    }

    void HistoryPanel::setUndoRequestedCallback(std::function<void()> callback)
    {
        onUndoRequested = std::move(callback);
    }

    void HistoryPanel::setRedoRequestedCallback(std::function<void()> callback)
    {
        onRedoRequested = std::move(callback);
    }

    void HistoryPanel::paint(juce::Graphics& g)
    {
        g.fillAll(juce::Colour::fromRGB(24, 28, 34));
        g.setColour(juce::Colour::fromRGB(40, 46, 56));
        g.drawRect(getLocalBounds(), 1);
    }

    void HistoryPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);
        auto top = area.removeFromTop(20);
        collapseToggleButton.setBounds(top.removeFromRight(24));
        titleLabel.setBounds(top.removeFromLeft(120));
        summaryLabel.setBounds(top);

        if (collapsed)
            return;

        area.removeFromTop(4);
        auto buttons = area.removeFromTop(24);
        undoButton.setBounds(buttons.removeFromLeft(64));
        buttons.removeFromLeft(6);
        redoButton.setBounds(buttons.removeFromLeft(64));
        buttons.removeFromLeft(6);
        clearButton.setBounds(buttons.removeFromLeft(64));

        area.removeFromTop(6);
        listBox.setBounds(area);
    }

    void HistoryPanel::updateCollapsedVisualState()
    {
        collapseToggleButton.setButtonText(collapsed ? ">" : "v");
        undoButton.setVisible(!collapsed);
        redoButton.setVisible(!collapsed);
        clearButton.setVisible(!collapsed);
        listBox.setVisible(!collapsed);
    }

    int HistoryPanel::getNumRows()
    {
        return static_cast<int>(rows.size());
    }

    void HistoryPanel::paintListBoxItem(int rowNumber,
                                        juce::Graphics& g,
                                        int width,
                                        int height,
                                        bool rowIsSelected)
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(rows.size()))
            return;

        const auto& row = rows[static_cast<size_t>(rowNumber)];
        const auto bounds = juce::Rectangle<int>(0, 0, width, height);

        juce::Colour fill = juce::Colour::fromRGB(25, 31, 40);
        if (row.future)
            fill = juce::Colour::fromRGB(30, 30, 34);
        if (row.current)
            fill = juce::Colour::fromRGB(54, 92, 160);
        if (rowIsSelected)
            fill = fill.brighter(0.08f);

        g.setColour(fill.withAlpha(row.current ? 0.84f : 0.60f));
        g.fillRect(bounds);

        g.setColour(juce::Colour::fromRGB(44, 52, 66));
        g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

        auto textArea = bounds.reduced(8, 4);
        auto left = textArea.removeFromLeft(84);

        if (row.current)
        {
            g.setColour(juce::Colour::fromRGB(112, 214, 156));
            g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(left.getX()),
                                                          static_cast<float>(left.getY() + 1),
                                                          66.0f,
                                                          14.0f),
                                   3.0f);
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText("CURRENT",
                       juce::Rectangle<int>(left.getX(), left.getY() + 1, 66, 14),
                       juce::Justification::centred,
                       true);
        }
        else if (row.future)
        {
            g.setColour(juce::Colour::fromRGB(128, 136, 150));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText("REDO", left.removeFromLeft(52), juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour(juce::Colour::fromRGB(130, 146, 170));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText("UNDO", left.removeFromLeft(52), juce::Justification::centredLeft, true);
        }

        g.setColour(row.future ? juce::Colour::fromRGB(146, 152, 164)
                               : juce::Colour::fromRGB(194, 202, 216));
        g.setFont(juce::FontOptions(11.0f, row.current ? juce::Font::bold : juce::Font::plain));
        g.drawText(row.label, textArea, juce::Justification::centredLeft, true);
    }
}
