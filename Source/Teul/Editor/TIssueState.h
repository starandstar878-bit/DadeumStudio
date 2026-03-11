#pragma once

#include <JuceHeader.h>

namespace Teul {

enum class TIssueState {
  none,
  degraded,
  missing,
  invalidConfig,
};

inline int issueStatePriority(TIssueState state) noexcept {
  switch (state) {
  case TIssueState::degraded:
    return 1;
  case TIssueState::missing:
    return 2;
  case TIssueState::invalidConfig:
    return 3;
  case TIssueState::none:
  default:
    return 0;
  }
}

inline TIssueState mergeIssueState(TIssueState lhs, TIssueState rhs) noexcept {
  return issueStatePriority(rhs) > issueStatePriority(lhs) ? rhs : lhs;
}

inline bool hasIssueState(TIssueState state) noexcept {
  return state != TIssueState::none;
}

inline juce::Colour issueStateAccent(TIssueState state) noexcept {
  switch (state) {
  case TIssueState::degraded:
    return juce::Colour(0xfffb923c);
  case TIssueState::missing:
    return juce::Colour(0xfff59e0b);
  case TIssueState::invalidConfig:
    return juce::Colour(0xffef4444);
  case TIssueState::none:
  default:
    return juce::Colours::transparentBlack;
  }
}

inline juce::String issueStateLabel(TIssueState state) {
  switch (state) {
  case TIssueState::degraded:
    return "Degraded";
  case TIssueState::missing:
    return "Missing";
  case TIssueState::invalidConfig:
    return "Invalid";
  case TIssueState::none:
  default:
    return {};
  }
}

} // namespace Teul
