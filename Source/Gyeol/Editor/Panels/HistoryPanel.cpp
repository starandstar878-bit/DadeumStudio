#include "Gyeol/Editor/Panels/HistoryPanel.h"

#include "Gyeol/Editor/GyeolCustomLookAndFeel.h"

#include <algorithm>

namespace Gyeol::Ui::Panels
{
    namespace
    {
        using Gyeol::GyeolPalette;

        juce::Colour palette(GyeolPalette id, float alpha = 1.0f)
        {
            return Gyeol::getGyeolColor(id).withAlpha(alpha);
        }

        juce::Font makePanelFont(const juce::Component& component, float height, bool bold)
        {
            if (auto* lf = dynamic_cast<const Gyeol::GyeolCustomLookAndFeel*>(&component.getLookAndFeel());
                lf != nullptr)
                return lf->makeFont(height, bold);

            auto fallback = juce::Font(juce::FontOptions(height));
            return bold ? fallback.boldened() : fallback;
        }

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

        void drawClockIcon(juce::Graphics& g,
                           juce::Rectangle<float> area,
                           juce::Colour colour)
        {
            if (area.isEmpty())
                return;

            g.setColour(colour);
            g.drawEllipse(area, 1.2f);

            const auto center = area.getCentre();
            g.drawLine(center.x,
                       center.y,
                       center.x,
                       area.getY() + area.getHeight() * 0.30f,
                       1.2f);
            g.drawLine(center.x,
                       center.y,
                       area.getX() + area.getWidth() * 0.68f,
                       center.y,
                       1.2f);
        }
    }

    HistoryPanel::HistoryPanel()
    {
        titleLabel.setText("History", juce::dontSendNotification);
        titleLabel.setFont(makePanelFont(*this, 12.0f, true));
        titleLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextPrimary));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        summaryLabel.setColour(juce::Label::textColourId, palette(GyeolPalette::TextSecondary));
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
        listBox.setRowHeight(listRowHeight);
        listBox.setColour(juce::ListBox::backgroundColourId, palette(GyeolPalette::CanvasBackground));
        listBox.setColour(juce::ListBox::outlineColourId, palette(GyeolPalette::BorderDefault));
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
        currentRow = -1;

        const auto hasTimeline = undoDepth > 0 || redoDepth > 0;
        if (hasTimeline)
        {
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
        }

        listBox.updateContent();
        if (currentRow >= 0 && currentRow < static_cast<int>(rows.size()))
        {
            listBox.selectRow(currentRow);
            listBox.scrollToEnsureRowIsOnscreen(currentRow);
        }
        else
        {
            listBox.deselectAllRows();
        }

        updateSummaryLabel();
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
        updateSummaryLabel();
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

    void HistoryPanel::setRowHeight(int rowHeight)
    {
        const auto clamped = juce::jlimit(20, 64, rowHeight);
        if (listRowHeight == clamped)
            return;

        listRowHeight = clamped;
        listBox.setRowHeight(listRowHeight);
        resized();
        repaint();
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
        g.fillAll(palette(GyeolPalette::PanelBackground));
        g.setColour(palette(GyeolPalette::BorderDefault));
        g.drawRect(getLocalBounds(), 1);

        if (collapsed)
        {
            auto badge = summaryLabel.getBounds().toFloat().reduced(2.0f, 2.0f);
            if (!badge.isEmpty())
            {
                g.setColour(palette(GyeolPalette::HeaderBackground, 0.86f));
                g.fillRoundedRectangle(badge, 5.0f);
                g.setColour(palette(GyeolPalette::BorderDefault, 0.96f));
                g.drawRoundedRectangle(badge, 5.0f, 1.0f);
            }
        }
    }

    void HistoryPanel::paintOverChildren(juce::Graphics& g)
    {
        if (collapsed || !listBox.isVisible() || !rows.empty())
            return;

        auto area = listBox.getBounds().reduced(10);
        if (area.getHeight() < 44)
            return;

        auto iconSlot = area.removeFromTop(22).toFloat();
        const auto iconArea = juce::Rectangle<float>(18.0f, 18.0f).withCentre(iconSlot.getCentre());
        drawClockIcon(g, iconArea, palette(GyeolPalette::TextSecondary, 0.82f));

        g.setColour(palette(GyeolPalette::TextSecondary, 0.90f));
        g.setFont(makePanelFont(*this, 10.5f, true));
        g.drawFittedText("No history yet", area.removeFromTop(18), juce::Justification::centred, 1);
    }

    void HistoryPanel::resized()
    {
        auto area = getLocalBounds().reduced(8);
        auto top = area.removeFromTop(22);

        collapseToggleButton.setBounds(top.removeFromRight(24));
        titleLabel.setBounds(top.removeFromLeft(84));

        if (collapsed)
            summaryLabel.setBounds(top.removeFromRight(172));
        else
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

    void HistoryPanel::updateSummaryLabel()
    {
        const auto compactText =
            "Undo " + juce::String(cachedUndoDepth)
            + juce::String::fromUTF8(u8" \u00B7 ")
            + "Redo " + juce::String(cachedRedoDepth);
        const auto fullText = "Undo " + juce::String(cachedUndoDepth)
                            + " | Redo " + juce::String(cachedRedoDepth);

        summaryLabel.setText(collapsed ? compactText : fullText,
                             juce::dontSendNotification);
        summaryLabel.setColour(juce::Label::textColourId,
                               collapsed ? palette(GyeolPalette::TextPrimary)
                                         : palette(GyeolPalette::TextSecondary));
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
        auto card = bounds.toFloat().reduced(1.0f, 1.0f);

        juce::Colour fill = palette(GyeolPalette::ControlBase, 0.70f);
        if (row.future)
            fill = palette(GyeolPalette::ControlBase, 0.56f);
        if (row.current)
            fill = palette(GyeolPalette::ControlBase, 0.90f);
        if (rowIsSelected)
            fill = fill.interpolatedWith(palette(GyeolPalette::SelectionBackground, 0.42f), 0.35f);

        g.setColour(fill);
        g.fillRoundedRectangle(card, 4.0f);

        g.setColour(palette(GyeolPalette::BorderDefault, rowIsSelected ? 0.95f : 0.72f));
        g.drawRoundedRectangle(card, 4.0f, 1.0f);

        if (row.current)
        {
            auto glowBar = juce::Rectangle<float>(card.getX() + 1.0f,
                                                  card.getY() + 2.0f,
                                                  3.0f,
                                                  card.getHeight() - 4.0f);
            g.setColour(palette(GyeolPalette::AccentPrimary, 0.18f));
            g.fillRoundedRectangle(glowBar.expanded(2.0f, 1.0f), 2.0f);
            g.setColour(palette(GyeolPalette::AccentPrimary, 0.94f));
            g.fillRoundedRectangle(glowBar, 1.6f);
        }

        const auto timelineX = 16.0f;
        g.setColour(palette(GyeolPalette::BorderDefault, 0.86f));
        g.drawLine(timelineX, 2.0f, timelineX, static_cast<float>(height - 2), 1.0f);

        auto dotBounds = juce::Rectangle<float>(timelineX - 4.0f,
                                                bounds.getCentreY() - 4.0f,
                                                8.0f,
                                                8.0f);
        if (row.current)
        {
            g.setColour(palette(GyeolPalette::AccentPrimary, 0.20f));
            g.fillEllipse(dotBounds.expanded(2.0f));
            g.setColour(palette(GyeolPalette::AccentPrimary));
            g.fillEllipse(dotBounds);
            g.setColour(palette(GyeolPalette::BorderActive, 0.88f));
            g.drawEllipse(dotBounds, 1.0f);
        }
        else if (row.future)
        {
            g.setColour(palette(GyeolPalette::TextSecondary, 0.55f));
            g.drawEllipse(dotBounds, 1.2f);
        }
        else
        {
            g.setColour(palette(GyeolPalette::BorderDefault, 0.95f));
            g.drawEllipse(dotBounds, 1.2f);
        }

        auto textArea = bounds.reduced(28, 4);
        auto kindArea = textArea.removeFromTop(10);

        if (row.current)
        {
            g.setColour(palette(GyeolPalette::ValidSuccess, 0.96f));
            g.setFont(makePanelFont(*this, 8.8f, true));
            g.drawText("CURRENT", kindArea, juce::Justification::centredLeft, true);
        }
        else if (row.future)
        {
            g.setColour(palette(GyeolPalette::TextSecondary, 0.74f));
            g.setFont(makePanelFont(*this, 8.8f, true));
            g.drawText("REDO", kindArea, juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour(palette(GyeolPalette::TextSecondary, 0.86f));
            g.setFont(makePanelFont(*this, 8.8f, true));
            g.drawText("UNDO", kindArea, juce::Justification::centredLeft, true);
        }

        const auto textColour = row.current ? palette(GyeolPalette::ValidSuccess)
                             : row.future ? palette(GyeolPalette::TextSecondary, 0.78f)
                                          : palette(GyeolPalette::TextPrimary);

        g.setColour(textColour);
        g.setFont(makePanelFont(*this, 11.0f, row.current));
        g.drawText(row.label, textArea, juce::Justification::centredLeft, true);
    }
}

