#pragma once

#include <stdint.h>

/*
    Gyeol Widget Plugin ABI (C boundary)

    String lifetime contract:
    - Any Utf8View received from host is valid only during the callback call.
    - Any Utf8View provided by plugin callbacks must remain valid until the callback returns.
    - Host copies plugin-provided Utf8View payloads synchronously before callback return.
*/

#if defined(_WIN32)
  #define GYEOL_WIDGET_PLUGIN_EXPORT __declspec(dllexport)
  #define GYEOL_WIDGET_PLUGIN_IMPORT __declspec(dllimport)
#else
  #define GYEOL_WIDGET_PLUGIN_EXPORT
  #define GYEOL_WIDGET_PLUGIN_IMPORT
#endif

#define GYEOL_WIDGET_PLUGIN_ABI_VERSION_MAJOR 1u
#define GYEOL_WIDGET_PLUGIN_ABI_VERSION_MINOR 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GyeolWidgetUtf8View
{
    const char* data;
    uint32_t size;
} GyeolWidgetUtf8View;

typedef struct GyeolWidgetPointF32
{
    float x;
    float y;
} GyeolWidgetPointF32;

typedef struct GyeolWidgetRectF32
{
    float x;
    float y;
    float w;
    float h;
} GyeolWidgetRectF32;

typedef struct GyeolWidgetAssetRef
{
    GyeolWidgetUtf8View asset_id;
    GyeolWidgetUtf8View display_name;
    GyeolWidgetUtf8View mime;
} GyeolWidgetAssetRef;

typedef struct GyeolWidgetDropOption
{
    GyeolWidgetUtf8View label;
    GyeolWidgetUtf8View prop_key;
    GyeolWidgetUtf8View hint;
} GyeolWidgetDropOption;

typedef struct GyeolWidgetModelView
{
    int64_t widget_id;
    int32_t widget_type; /* optional, -1 means host has no enum mapping */
    GyeolWidgetRectF32 bounds;
    const void* properties_handle;
} GyeolWidgetModelView;

typedef struct GyeolWidgetPropertyPatchView
{
    void* patch_handle;
} GyeolWidgetPropertyPatchView;

typedef struct GyeolWidgetMouseEventView
{
    GyeolWidgetPointF32 local;
    GyeolWidgetPointF32 canvas;
    GyeolWidgetPointF32 wheel_delta;
    uint32_t buttons;
    uint32_t modifiers;
    uint32_t click_count;
} GyeolWidgetMouseEventView;

typedef struct GyeolWidgetPaintContext
{
    void* native_handle;
} GyeolWidgetPaintContext;

typedef enum GyeolWidgetResultCode
{
    GYEOL_WIDGET_RESULT_OK = 0,
    GYEOL_WIDGET_RESULT_INVALID_ARGUMENT = 1,
    GYEOL_WIDGET_RESULT_UNSUPPORTED = 2,
    GYEOL_WIDGET_RESULT_FAILED = 3
} GyeolWidgetResultCode;

typedef enum GyeolWidgetConsumeEvent
{
    GYEOL_WIDGET_CONSUME_NO = 0,
    GYEOL_WIDGET_CONSUME_YES = 1
} GyeolWidgetConsumeEvent;

typedef enum GyeolWidgetCursorType
{
    GYEOL_WIDGET_CURSOR_INHERIT = 0,
    GYEOL_WIDGET_CURSOR_NORMAL = 1,
    GYEOL_WIDGET_CURSOR_POINTING_HAND = 2,
    GYEOL_WIDGET_CURSOR_DRAGGING_HAND = 3,
    GYEOL_WIDGET_CURSOR_LEFT_RIGHT_RESIZE = 4,
    GYEOL_WIDGET_CURSOR_UP_DOWN_RESIZE = 5,
    GYEOL_WIDGET_CURSOR_CROSSHAIR = 6,
    GYEOL_WIDGET_CURSOR_IBEAM = 7,
    GYEOL_WIDGET_CURSOR_WAIT = 8
} GyeolWidgetCursorType;

typedef enum GyeolWidgetLogLevel
{
    GYEOL_WIDGET_LOG_DEBUG = 0,
    GYEOL_WIDGET_LOG_INFO = 1,
    GYEOL_WIDGET_LOG_WARN = 2,
    GYEOL_WIDGET_LOG_ERROR = 3
} GyeolWidgetLogLevel;

typedef struct GyeolWidgetDrawApi
{
    uint32_t struct_size;
    uint32_t abi_version_major;
    uint32_t abi_version_minor;

    void (*set_colour_rgba8)(void* host_context, const GyeolWidgetPaintContext* paint_context, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void (*fill_rect)(void* host_context, const GyeolWidgetPaintContext* paint_context, GyeolWidgetRectF32 rect);
    void (*fill_rounded_rect)(void* host_context,
                              const GyeolWidgetPaintContext* paint_context,
                              GyeolWidgetRectF32 rect,
                              float corner_radius);
    void (*draw_line)(void* host_context,
                      const GyeolWidgetPaintContext* paint_context,
                      GyeolWidgetPointF32 from,
                      GyeolWidgetPointF32 to,
                      float thickness);
    void (*draw_text)(void* host_context,
                      const GyeolWidgetPaintContext* paint_context,
                      GyeolWidgetUtf8View text,
                      GyeolWidgetRectF32 bounds,
                      uint32_t justification_flags,
                      int32_t max_lines);
} GyeolWidgetDrawApi;

typedef GyeolWidgetResultCode (*GyeolWidgetRegisterWidgetFn)(void* host_context, const struct GyeolWidgetDescriptor* descriptor);
typedef void (*GyeolWidgetLogFn)(void* host_context, int32_t level, GyeolWidgetUtf8View message);

typedef int32_t (*GyeolWidgetPropsHasKeyFn)(void* host_context, const void* props_handle, GyeolWidgetUtf8View key);
typedef GyeolWidgetResultCode (*GyeolWidgetPropsGetStringFn)(void* host_context,
                                                             const void* props_handle,
                                                             GyeolWidgetUtf8View key,
                                                             GyeolWidgetUtf8View* out_value);
typedef GyeolWidgetResultCode (*GyeolWidgetPropsGetInt64Fn)(void* host_context,
                                                            const void* props_handle,
                                                            GyeolWidgetUtf8View key,
                                                            int64_t* out_value);
typedef GyeolWidgetResultCode (*GyeolWidgetPropsGetDoubleFn)(void* host_context,
                                                             const void* props_handle,
                                                             GyeolWidgetUtf8View key,
                                                             double* out_value);
typedef GyeolWidgetResultCode (*GyeolWidgetPropsGetBoolFn)(void* host_context,
                                                           const void* props_handle,
                                                           GyeolWidgetUtf8View key,
                                                           int32_t* out_value);

typedef GyeolWidgetResultCode (*GyeolWidgetPatchSetStringFn)(void* host_context,
                                                             void* patch_handle,
                                                             GyeolWidgetUtf8View key,
                                                             GyeolWidgetUtf8View value);
typedef GyeolWidgetResultCode (*GyeolWidgetPatchSetInt64Fn)(void* host_context,
                                                            void* patch_handle,
                                                            GyeolWidgetUtf8View key,
                                                            int64_t value);
typedef GyeolWidgetResultCode (*GyeolWidgetPatchSetDoubleFn)(void* host_context,
                                                             void* patch_handle,
                                                             GyeolWidgetUtf8View key,
                                                             double value);
typedef GyeolWidgetResultCode (*GyeolWidgetPatchSetBoolFn)(void* host_context,
                                                           void* patch_handle,
                                                           GyeolWidgetUtf8View key,
                                                           int32_t value);
typedef GyeolWidgetResultCode (*GyeolWidgetPatchSetJsonFn)(void* host_context,
                                                           void* patch_handle,
                                                           GyeolWidgetUtf8View key,
                                                           GyeolWidgetUtf8View json_value);
typedef GyeolWidgetResultCode (*GyeolWidgetPatchRemoveFn)(void* host_context, void* patch_handle, GyeolWidgetUtf8View key);

typedef struct GyeolWidgetHostApi
{
    uint32_t struct_size;
    uint32_t abi_version_major;
    uint32_t abi_version_minor;

    void* host_context;

    GyeolWidgetRegisterWidgetFn register_widget;
    GyeolWidgetLogFn log;

    GyeolWidgetPropsHasKeyFn props_has_key;
    GyeolWidgetPropsGetStringFn props_get_string;
    GyeolWidgetPropsGetInt64Fn props_get_int64;
    GyeolWidgetPropsGetDoubleFn props_get_double;
    GyeolWidgetPropsGetBoolFn props_get_bool;

    GyeolWidgetPatchSetStringFn patch_set_string;
    GyeolWidgetPatchSetInt64Fn patch_set_int64;
    GyeolWidgetPatchSetDoubleFn patch_set_double;
    GyeolWidgetPatchSetBoolFn patch_set_bool;
    GyeolWidgetPatchSetJsonFn patch_set_json;
    GyeolWidgetPatchRemoveFn patch_remove;

    const GyeolWidgetDrawApi* draw_api;
} GyeolWidgetHostApi;

typedef void (*GyeolWidgetPaintFn)(void* plugin_user_data,
                                   const GyeolWidgetPaintContext* paint_context,
                                   const GyeolWidgetModelView* widget,
                                   const GyeolWidgetRectF32* body_bounds,
                                   const GyeolWidgetDrawApi* draw_api,
                                   const GyeolWidgetHostApi* host_api);
typedef int32_t (*GyeolWidgetHitTestFn)(void* plugin_user_data, const GyeolWidgetModelView* widget, GyeolWidgetPointF32 local_point);
typedef GyeolWidgetCursorType (*GyeolWidgetCursorProviderFn)(void* plugin_user_data,
                                                             const GyeolWidgetModelView* widget,
                                                             GyeolWidgetPointF32 local_point);
typedef GyeolWidgetConsumeEvent (*GyeolWidgetInteractionFn)(void* plugin_user_data,
                                                            const GyeolWidgetModelView* widget,
                                                            const GyeolWidgetMouseEventView* mouse_event,
                                                            GyeolWidgetPropertyPatchView* patch_out,
                                                            const GyeolWidgetHostApi* host_api);
typedef GyeolWidgetResultCode (*GyeolWidgetDropOptionsFn)(void* plugin_user_data,
                                                          const GyeolWidgetModelView* widget,
                                                          const GyeolWidgetAssetRef* asset,
                                                          GyeolWidgetDropOption* out_options,
                                                          uint32_t* in_out_option_count);
typedef GyeolWidgetResultCode (*GyeolWidgetApplyDropFn)(void* plugin_user_data,
                                                        const GyeolWidgetModelView* widget,
                                                        const GyeolWidgetAssetRef* asset,
                                                        const GyeolWidgetDropOption* option,
                                                        GyeolWidgetPropertyPatchView* patch_out,
                                                        const GyeolWidgetHostApi* host_api);

typedef struct GyeolWidgetExportCodegenContext
{
    GyeolWidgetUtf8View member_name;
    GyeolWidgetUtf8View type_key;
    GyeolWidgetUtf8View export_target_type;
    const GyeolWidgetModelView* widget;
} GyeolWidgetExportCodegenContext;

typedef struct GyeolWidgetExportCodegenResult
{
    GyeolWidgetUtf8View member_type;
    GyeolWidgetUtf8View codegen_kind;
    GyeolWidgetUtf8View constructor_lines_json; /* JSON array<string> */
    GyeolWidgetUtf8View resized_lines_json;     /* JSON array<string> */
} GyeolWidgetExportCodegenResult;

typedef GyeolWidgetResultCode (*GyeolWidgetExportCodegenFn)(void* plugin_user_data,
                                                            const GyeolWidgetExportCodegenContext* context,
                                                            GyeolWidgetExportCodegenResult* out_result,
                                                            const GyeolWidgetHostApi* host_api);

typedef struct GyeolWidgetDescriptor
{
    uint32_t struct_size;
    uint32_t abi_version_major;
    uint32_t abi_version_minor;

    int32_t widget_type; /* optional, -1 recommended for plugins */
    GyeolWidgetUtf8View type_key; /* canonical unique id */
    GyeolWidgetUtf8View display_name;
    GyeolWidgetRectF32 default_bounds;
    GyeolWidgetPointF32 min_size;
    GyeolWidgetUtf8View default_properties_json;

    void* plugin_user_data;
    GyeolWidgetPaintFn paint;
    GyeolWidgetHitTestFn hit_test;
    GyeolWidgetCursorProviderFn cursor_provider;
    GyeolWidgetInteractionFn on_mouse_down;
    GyeolWidgetInteractionFn on_mouse_drag;
    GyeolWidgetInteractionFn on_mouse_up;
    GyeolWidgetDropOptionsFn get_drop_options;
    GyeolWidgetApplyDropFn apply_drop;
    GyeolWidgetExportCodegenFn export_codegen; /* optional, NULL means host fallback */
} GyeolWidgetDescriptor;

typedef GyeolWidgetResultCode (*GyeolWidgetRegisterPluginFn)(const GyeolWidgetHostApi* host_api);

/*
    Drop options contract:
    - If out_options == NULL, plugin writes required option count to in_out_option_count.
    - If out_options != NULL, plugin writes up to *in_out_option_count options and updates it with written count.

    Export codegen contract (ABI v1.1):
    - `export_codegen` is optional.
    - Host copies `GyeolWidgetExportCodegenResult` string payloads synchronously before callback return.
    - `constructor_lines_json` / `resized_lines_json` must be JSON array<string> when non-empty.
*/

static inline int32_t gyeol_widget_host_api_is_compatible(const GyeolWidgetHostApi* host_api)
{
    if (host_api == 0)
        return 0;
    if (host_api->struct_size < sizeof(GyeolWidgetHostApi))
        return 0;
    if (host_api->abi_version_major != GYEOL_WIDGET_PLUGIN_ABI_VERSION_MAJOR)
        return 0;
    if (host_api->register_widget == 0)
        return 0;

    return 1;
}

#ifdef __cplusplus
}
#endif

#define GYEOL_WIDGET_DLL_ENTRY(symbol_name) \
    extern "C" GYEOL_WIDGET_PLUGIN_EXPORT GyeolWidgetResultCode symbol_name(const GyeolWidgetHostApi* host_api)
