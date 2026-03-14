#pragma once

#include "Teul2/Document/TDocumentTypes.h"
#include <JuceHeader.h>
#include <vector>

namespace Teul {

// =============================================================================
//  ITeulParamProvider — 외부(Ieum 등)에서 노드 파라미터에 접근하기 위한 인터페이스
// =============================================================================

class ITeulParamProvider {
public:
  class Listener {
  public:
    virtual ~Listener() = default;
    virtual void teulParamSurfaceChanged() {}
    virtual void teulParamValueChanged(const TTeulExposedParam &param) = 0;
  };

  virtual ~ITeulParamProvider() = default;

  virtual std::vector<TTeulExposedParam> listExposedParams() const = 0;
  virtual juce::var getParam(const juce::String &paramId) const = 0;
  virtual bool setParam(const juce::String &paramId, const juce::var &value) = 0;
  virtual void addListener(Listener *listener) = 0;
  virtual void removeListener(Listener *listener) = 0;
};

} // namespace Teul
