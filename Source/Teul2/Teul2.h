#pragma once

/**
 * Teul2 Umbrella Header
 * 
 * 외부 모듈에서 Teul2 기능을 사용할 때 이 헤더 하나만 포함하면 됩니다.
 * 레이어별 핵심 인터페이스(Document, Editor, Runtime, Bridge)를 모두 제공합니다.
 */

// 1. 데이터 레이어 (Document)
#include "Teul2/Document/TTeulDocument.h"
#include "Teul2/Document/TDocumentStore.h"
#include "Teul2/Document/TDocumentTypes.h"

// 2. UI 레이어 (Editor)
#include "Teul2/Editor/TTeulEditor.h"

// 3. 실행 레이어 (Runtime)
#include "Teul2/Runtime/TTeulRuntime.h"

// 4. 연동 레이어 (Bridge)
#include "Teul2/Bridge/TTeulBridge.h"
