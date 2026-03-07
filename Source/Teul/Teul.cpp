#include "Teul.h"

// Most Teul sources are included here because the generated project still
// compiles Teul as a single translation unit. Keep this file as an aggregator
// until the build registers these implementation units individually.
#include "Model/TGraphDocument.cpp"
#include "Registry/TNodeRegistry.cpp"
#include "Runtime/TGraphRuntime.cpp"
#include "Editor/Panels/Property/ParamValueFormatter.cpp"
#include "Editor/Panels/Property/ParamEditorFactory.cpp"
#include "Editor/Panels/Property/BindingSummaryPresenter.cpp"
#include "Editor/Node/NodePreviewRenderer.cpp"
#include "UI/Port/TPortComponent.cpp"
#include "UI/Node/TNodeComponent.cpp"
#include "UI/Canvas/TGraphCanvas.cpp"
#include "Editor/Panels/NodeLibraryPanel.cpp"
#include "Editor/Panels/NodePropertiesPanel.cpp"
#include "Editor/EditorHandleImpl.cpp"
#include "Editor/EditorHandle.cpp"
