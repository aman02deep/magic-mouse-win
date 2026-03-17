#pragma once

/**
 * plugin_api.h — Magic Mouse stable C ABI for third-party plugins.
 *
 * All functions use `extern "C"` — plugins can be written in any language
 * that can load a DLL and call C functions.
 *
 * Compatibility policy:
 *   - The numeric value of MAGIC_MOUSE_PLUGIN_API_VERSION will increment on
 *     every breaking change.
 *   - Struct members must never be removed or reordered; new fields may be
 *     appended only if the struct carries an explicit `size` field.
 *
 * Example (C++ plugin):
 *   #include "plugin_api.h"
 *   void MAGIC_MOUSE_CALL on_gesture(const mm_gesture_event_t* e, void* ud) {
 *       printf("gesture: %s  value: %.2f\n", e->name, e->value);
 *   }
 *   extern "C" void mm_plugin_init() {
 *       mm_register_gesture_hook(&on_gesture, nullptr);
 *   }
 */

#ifdef _WIN32
#  ifdef MAGIC_MOUSE_BUILD_DLL
#    define MAGIC_MOUSE_API __declspec(dllexport)
#  else
#    define MAGIC_MOUSE_API __declspec(dllimport)
#  endif
#else
#  define MAGIC_MOUSE_API __attribute__((visibility("default")))
#endif

#define MAGIC_MOUSE_CALL    __cdecl
#define MAGIC_MOUSE_PLUGIN_API_VERSION  1

#ifdef __cplusplus
extern "C" {
#endif

// ── Types ─────────────────────────────────────────────────────────────────────

/// Fired whenever a recognised gesture completes.
typedef struct mm_gesture_event_t {
    const char* name;    ///< e.g. "swipe_left", "smart_zoom", "tap"
    float       value;   ///< Normalised magnitude [0.0, 1.0]
    int         fingers; ///< Number of contact points
    double      timestamp_ms; ///< Milliseconds since process start
} mm_gesture_event_t;

/// Callback type for gesture hooks.
typedef void (MAGIC_MOUSE_CALL *mm_gesture_callback_t)(
    const mm_gesture_event_t* event,
    void*                     userdata);

// ── Gesture hook API ──────────────────────────────────────────────────────────

/// Register a callback that is invoked on every gesture event.
/// Thread-safe; may be called from any thread.
/// @return 0 on success, non-zero on error (e.g., too many hooks registered).
MAGIC_MOUSE_API int MAGIC_MOUSE_CALL
mm_register_gesture_hook(mm_gesture_callback_t callback, void* userdata);

/// Unregister a previously registered callback.
/// No-op if the callback was never registered.
MAGIC_MOUSE_API void MAGIC_MOUSE_CALL
mm_unregister_gesture_hook(mm_gesture_callback_t callback);

// ── Versioning ────────────────────────────────────────────────────────────────

/// Returns the runtime API version of the loaded library.
/// Compare against MAGIC_MOUSE_PLUGIN_API_VERSION for compatibility checking.
MAGIC_MOUSE_API int MAGIC_MOUSE_CALL
mm_get_api_version(void);

#ifdef __cplusplus
} // extern "C"
#endif
