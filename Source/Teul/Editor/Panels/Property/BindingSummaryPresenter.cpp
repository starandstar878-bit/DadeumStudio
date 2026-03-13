#include "Teul/Editor/Panels/Property/BindingSummaryPresenter.h"
#include "Teul/Editor/Theme/TeulPalette.h"

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
    return {"Gyeol: hidden", TeulPalette::PanelTextDisabled().withAlpha(0.82f)};

  if (bindingSummaryResolver == nullptr)
    return {"Gyeol: source unavailable",
            TeulPalette::PanelTextFaint().withAlpha(0.88f)};

  const auto summary = bindingSummaryResolver(paramId);
  if (summary.isNotEmpty())
    return {"Gyeol: bound / " + summary, TeulPalette::AccentGold()};

  return {"Gyeol: unbound", TeulPalette::PanelTextFaint().withAlpha(0.88f)};
}

} // namespace Teul
