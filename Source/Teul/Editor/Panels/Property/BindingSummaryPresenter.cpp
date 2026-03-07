#include "Teul/Editor/Panels/Property/BindingSummaryPresenter.h"

namespace Teul {

juce::String makeIeumBindingText(const TParamSpec &spec) {
  juce::String bindingText = spec.exposeToIeum ? "Ieum: exposed"
                                               : "Ieum: hidden";
  if (spec.exposeToIeum && spec.preferredBindingKey.isNotEmpty())
    bindingText << " / key: " << spec.preferredBindingKey;
  if (spec.isAutomatable)
    bindingText << " / auto";
  if (spec.isModulatable)
    bindingText << " / mod";
  if (spec.isReadOnly)
    bindingText << " / read-only";
  return bindingText;
}

GyeolBindingPresentation makeGyeolBindingPresentation(
    const TParamSpec &spec, const juce::String &paramId,
    ParamBindingSummaryResolver bindingSummaryResolver) {
  if (!spec.exposeToIeum)
    return {"Gyeol: hidden", juce::Colours::white.withAlpha(0.28f)};

  if (bindingSummaryResolver == nullptr)
    return {"Gyeol: source unavailable",
            juce::Colours::white.withAlpha(0.36f)};

  const auto summary = bindingSummaryResolver(paramId);
  if (summary.isNotEmpty())
    return {"Gyeol: bound / " + summary, juce::Colour(0xffd9c27c)};

  return {"Gyeol: unbound", juce::Colours::white.withAlpha(0.36f)};
}

} // namespace Teul
