#pragma once

#include "Teul/Runtime/TGraphRuntime.h"

namespace Teul {

class TTeulRuntime : public TGraphRuntime {
public:
  using TGraphRuntime::TGraphRuntime;
  using RuntimeStats = TGraphRuntime::RuntimeStats;
};

} // namespace Teul