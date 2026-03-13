#pragma once

#include "IeumDocument.h"

namespace Ieum {

/** 
 * Ieum 문서를 JSON으로 변환하거나 그 반대를 담당합니다.
 */
class IeumSerializer {
public:
    static int currentSchemaVersion() noexcept { return 1; }

    static juce::var toJson(const IeumDocument& doc);
    static bool fromJson(IeumDocument& doc, const juce::var& json);

private:
    static juce::var bindingToJson(const IeumBindingSpec& spec);
    static juce::var endpointToJson(const IeumEndpoint& endpoint);

    static bool jsonToBinding(IeumBindingSpec& spec, const juce::var& json);
    static bool jsonToEndpoint(IeumEndpoint& endpoint, const juce::var& json);
};

} // namespace Ieum
