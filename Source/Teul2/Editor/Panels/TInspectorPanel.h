#pragma once

#include "Teul2/Document/TTeulDocument.h"
#include <JuceHeader.h>
#include <memory>

namespace Teul {

class TNodeRegistry;

/**
 * @brief 통합 인스펙터 패널
 * 선택된 대상(노드, 컨트롤 소스, 시스템 엔드포인트)에 따라 적절한 상세 편집 뷰를 표시합니다.
 */
class TInspectorPanel : public juce::Component {
public:
    enum class InspectorType {
        None,
        ControlSource,
        SystemEndpoint
    };

    explicit TInspectorPanel(TTeulDocument& doc, const TNodeRegistry& registry);
    ~TInspectorPanel() override;

    void setSelection(InspectorType type, const juce::String& targetId);
    void refreshFromDocument();

    // Callbacks
    void setDocumentChangedCallback(std::function<void()> callback);
    void setLayoutChangedCallback(std::function<void()> callback);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class PlaceholderView;
    class ControlSourceView;
    class SystemEndpointView;

    void updateActiveView();

    TTeulDocument& document;
    const TNodeRegistry& nodeRegistry;

    InspectorType currentType = InspectorType::None;
    juce::String currentTargetId;

    std::unique_ptr<juce::Component> activeView;
    std::function<void()> onDocumentChanged;
    std::function<void()> onLayoutChanged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TInspectorPanel)
};

} // namespace Teul
