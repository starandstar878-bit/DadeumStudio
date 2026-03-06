#pragma once
#include <JuceHeader.h>

namespace Teul {

struct TeulPalette {
  static inline const juce::Colour CanvasBackground =
      juce::Colour(0xff181818); // Dark charcoal
  static inline const juce::Colour GridLine = juce::Colour(0xff222222);
  static inline const juce::Colour GridDot = juce::Colour(0xff333333);

  static inline const juce::Colour PortAudio =
      juce::Colour(0xff4ade80); // Green
  static inline const juce::Colour PortCV = juce::Colour(0xfffbbf24);   // Amber
  static inline const juce::Colour PortGate =
      juce::Colour(0xfff97316); // Orange
  static inline const juce::Colour PortMIDI = juce::Colour(0xff60a5fa); // Blue
  static inline const juce::Colour PortControl =
      juce::Colour(0xffc084fc); // Purple

  static inline const juce::Colour NodeBackground = juce::Colour(0xff2a2a2a);
  static inline const juce::Colour NodeHeader = juce::Colour(0xff3a3a3a);
  static inline const juce::Colour NodeBorder = juce::Colour(0xff555555);
  static inline const juce::Colour NodeBorderSelected =
      juce::Colour(0xffffffff);

  static inline const juce::Colour WireNormal = juce::Colour(0xc0ffffff);
};

} // namespace Teul
