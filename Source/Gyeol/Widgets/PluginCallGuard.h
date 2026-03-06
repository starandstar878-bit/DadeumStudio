#pragma once

#include <JuceHeader.h>
#include <exception>
#include <utility>

namespace Gyeol::Widgets {
template <typename Callable>
bool invokePluginBoundary(const char *context, Callable &&callable) noexcept {
  try {
    callable();
    return true;
  } catch (const std::exception &exception) {
    DBG("[Gyeol][PluginBoundary] " + juce::String(context)
        + " threw std::exception: " + juce::String(exception.what()));
    return false;
  } catch (...) {
    DBG("[Gyeol][PluginBoundary] " + juce::String(context)
        + " threw unknown exception.");
    return false;
  }
}

template <typename ResultType, typename Callable>
ResultType invokePluginBoundaryOr(const char *context,
                                  Callable &&callable,
                                  const ResultType &fallback) noexcept {
  ResultType result = fallback;
  const auto success = invokePluginBoundary(context, [&]() {
    result = callable();
  });

  return success ? result : fallback;
}
} // namespace Gyeol::Widgets