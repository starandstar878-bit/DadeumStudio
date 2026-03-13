#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace Teul {

class TNodeRegistry;

class NodeLibraryPanel : public juce::Component {
public:
  using InsertCallback = std::function<void(const juce::String &)>;

  ~NodeLibraryPanel() override = default;

  static std::unique_ptr<NodeLibraryPanel> create(const TNodeRegistry &registry,
                                                  InsertCallback onInsert);
};

} // namespace Teul
