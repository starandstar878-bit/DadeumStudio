#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <map>

namespace Gyeol::Runtime
{
    enum class RuntimeParamError
    {
        none,
        invalidKey,
        unsupportedType,
        nonFiniteNumber,
        typeMismatch
    };

    struct RuntimeParamResult
    {
        RuntimeParamError error = RuntimeParamError::none;
        juce::String message;
        juce::var value;

        [[nodiscard]] bool wasOk() const noexcept
        {
            return error == RuntimeParamError::none;
        }
    };

    class RuntimeParamBridge
    {
    public:
        RuntimeParamResult set(const juce::String& key,
                               const juce::var& requestedValue,
                               const juce::var& eventPayload);

        RuntimeParamResult adjust(const juce::String& key, double delta);
        RuntimeParamResult toggle(const juce::String& key);

        void clear();
        [[nodiscard]] const std::map<juce::String, juce::var>& values() const noexcept;
        [[nodiscard]] std::uint64_t revision() const noexcept;

    private:
        static RuntimeParamResult normalizeAndValidateValue(const juce::var& input);

        std::map<juce::String, juce::var> runtimeParams;
        std::uint64_t valueRevision = 1;
    };
}
