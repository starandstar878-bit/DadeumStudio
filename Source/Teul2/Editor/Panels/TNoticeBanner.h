#pragma once

#include "Teul2/Document/TTeulDocument.h"
#include <JuceHeader.h>
#include <functional>

namespace Teul {

class TNoticeBanner : public juce::Component {
public:
    TNoticeBanner();
    ~TNoticeBanner() override = default;

    void setDismissHandler(std::function<void()> handler);
    void setNotice(const TDocumentNotice& notice);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    static juce::Colour accentForLevel(TDocumentNoticeLevel level);

    TDocumentNotice notice;
    juce::TextButton dismissButton;
    std::function<void()> dismissHandler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TNoticeBanner)
};

} // namespace Teul
