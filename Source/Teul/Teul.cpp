#include "Teul.h"

// Most Teul sources are included here because the generated project still
// compiles Teul as a single translation unit. Keep this file as an aggregator
// until the build registers these implementation units individually.
#include "Model/TGraphDocument.cpp"
#include "Registry/TNodeRegistry.cpp"
#include "Runtime/TGraphRuntime.cpp"
#include "UI/Port/TPortComponent.cpp"
#include "UI/Node/TNodeComponent.cpp"
#include "UI/Canvas/TGraphCanvas.cpp"
#include "Editor/EditorHandle.cpp"
