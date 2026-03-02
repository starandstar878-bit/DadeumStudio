#include "Gyeol/Runtime/RuntimeParamBridge.h"

#include <cmath>

namespace Gyeol::Runtime
{
    namespace
    {
        RuntimeParamResult makeError(RuntimeParamError error, const juce::String& message)
        {
            RuntimeParamResult result;
            result.error = error;
            result.message = message;
            return result;
        }
    }

    RuntimeParamResult RuntimeParamBridge::normalizeAndValidateValue(const juce::var& input)
    {
        if (input.isVoid() || input.isUndefined())
            return makeError(RuntimeParamError::unsupportedType, "runtime param value is empty");

        RuntimeParamResult result;
        result.error = RuntimeParamError::none;

        if (input.isBool() || input.isInt() || input.isInt64() || input.isString())
        {
            result.value = input;
            return result;
        }

        if (input.isDouble())
        {
            const auto value = static_cast<double>(input);
            if (!std::isfinite(value))
                return makeError(RuntimeParamError::nonFiniteNumber, "runtime param value must be finite");

            result.value = value;
            return result;
        }

        return makeError(RuntimeParamError::unsupportedType,
                         "runtime param value must be bool/int/int64/double/string");
    }

    RuntimeParamResult RuntimeParamBridge::set(const juce::String& key,
                                               const juce::var& requestedValue,
                                               const juce::var& eventPayload)
    {
        const auto normalizedKey = key.trim();
        if (normalizedKey.isEmpty())
            return makeError(RuntimeParamError::invalidKey, "runtime param key is empty");

        const auto effectiveValue = (requestedValue.isVoid() || requestedValue.isUndefined())
                                      ? eventPayload
                                      : requestedValue;

        auto result = normalizeAndValidateValue(effectiveValue);
        if (!result.wasOk())
            return result;

        const auto existingIt = runtimeParams.find(normalizedKey);
        if (existingIt == runtimeParams.end() || existingIt->second != result.value)
        {
            runtimeParams[normalizedKey] = result.value;
            ++valueRevision;
        }
        return result;
    }

    RuntimeParamResult RuntimeParamBridge::adjust(const juce::String& key, double delta)
    {
        const auto normalizedKey = key.trim();
        if (normalizedKey.isEmpty())
            return makeError(RuntimeParamError::invalidKey, "runtime param key is empty");
        if (!std::isfinite(delta))
            return makeError(RuntimeParamError::nonFiniteNumber, "runtime param delta must be finite");

        auto current = 0.0;
        if (const auto it = runtimeParams.find(normalizedKey); it != runtimeParams.end())
        {
            if (it->second.isInt() || it->second.isInt64() || it->second.isDouble())
            {
                current = static_cast<double>(it->second);
            }
            else
            {
                return makeError(RuntimeParamError::typeMismatch, "runtime param is not numeric");
            }
        }

        const auto next = current + delta;
        if (!std::isfinite(next))
            return makeError(RuntimeParamError::nonFiniteNumber, "runtime param result is not finite");

        RuntimeParamResult result;
        result.value = next;
        const auto existingIt = runtimeParams.find(normalizedKey);
        if (existingIt == runtimeParams.end() || existingIt->second != result.value)
        {
            runtimeParams[normalizedKey] = result.value;
            ++valueRevision;
        }
        return result;
    }

    RuntimeParamResult RuntimeParamBridge::toggle(const juce::String& key)
    {
        const auto normalizedKey = key.trim();
        if (normalizedKey.isEmpty())
            return makeError(RuntimeParamError::invalidKey, "runtime param key is empty");

        auto current = false;
        if (const auto it = runtimeParams.find(normalizedKey); it != runtimeParams.end())
        {
            if (it->second.isBool() || it->second.isInt() || it->second.isInt64() || it->second.isDouble())
            {
                current = static_cast<bool>(it->second);
            }
            else
            {
                return makeError(RuntimeParamError::typeMismatch, "runtime param is not toggleable");
            }
        }

        RuntimeParamResult result;
        result.value = !current;
        const auto existingIt = runtimeParams.find(normalizedKey);
        if (existingIt == runtimeParams.end() || existingIt->second != result.value)
        {
            runtimeParams[normalizedKey] = result.value;
            ++valueRevision;
        }
        return result;
    }

    void RuntimeParamBridge::clear()
    {
        if (runtimeParams.empty())
            return;

        runtimeParams.clear();
        ++valueRevision;
    }

    const std::map<juce::String, juce::var>& RuntimeParamBridge::values() const noexcept
    {
        return runtimeParams;
    }

    std::uint64_t RuntimeParamBridge::revision() const noexcept
    {
        return valueRevision;
    }
}
