#pragma once

// =============================================================================
//  ⚠️ 주의: 매크로(TEUL_NODE_AUTOREGISTER)가 런타임에 실행되려면,
//  반드시 이 헤더(CoreNodes.h)가 TNodeRegistry.cpp 파일에 포함되어야 합니다.
//  여기에 선언된 모든 Core 노드군들이 런타임 팩토리에 정적 등록됩니다.
// =============================================================================

#include "Core/FXNodes.h"
#include "Core/FilterNodes.h"
#include "Core/MathLogicNodes.h"
#include "Core/MidiNodes.h"
#include "Core/MixerNodes.h"
#include "Core/ModulationNodes.h"
#include "Core/SourceNodes.h"

