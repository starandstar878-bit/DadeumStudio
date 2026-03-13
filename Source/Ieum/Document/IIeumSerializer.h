#pragma once

#include "IIeumDocument.h"

namespace Ieum {

/** 
 * Ieum 문서를 JSON으로 변환하거나 그 반대를 담당합니다.
 */
class IIeumSerializer {
public:
    static int currentSchemaVersion() noexcept { return 1; }

    static juce::var toJson(const IIeumDocument& doc);
    static bool fromJson(IIeumDocument& doc, const juce::var& json);

private:
    static juce::var bindingToJson(const IBindingSpec& spec);
    static juce::var endpointToJson(const IEndpoint& endpoint);

    static bool jsonToBinding(IBindingSpec& spec, const juce::var& json);
    static bool jsonToEndpoint(IEndpoint& endpoint, const juce::var& json);
};

} // namespace Ieum

