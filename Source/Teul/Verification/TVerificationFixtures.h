#pragma once
#include "Teul/Model/TGraphDocument.h"
#include "Teul/Registry/TNodeRegistry.h"
#include <vector>
namespace Teul {
struct TVerificationGraphFixture {
  juce::String fixtureId;
  juce::String displayName;
  TGraphDocument document;
};
TGraphDocument makeVerificationGraphG1TonePath(const TNodeRegistry &registry);
TGraphDocument makeVerificationGraphG2FilterSweep(const TNodeRegistry &registry);
TGraphDocument makeVerificationGraphG3StereoMotion(const TNodeRegistry &registry);
TGraphDocument makeVerificationGraphG4MidiVoice(const TNodeRegistry &registry);
TGraphDocument makeVerificationGraphG5TimeTail(const TNodeRegistry &registry);
std::vector<TVerificationGraphFixture>
makeRepresentativeVerificationGraphSet(const TNodeRegistry &registry);
} // namespace Teul