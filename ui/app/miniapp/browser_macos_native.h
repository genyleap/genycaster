/*!
 * @file ui/app/miniapp/browser_macos_native.h
 * @brief C ABI between the Qt/C++26 Mini App host and Apple's Objective-C WebKit adapter.
 * @details Opaque handles are created and destroyed on the GUI thread.
 *   Callback string pointers are borrowed for the duration of each call;
 *   localization and external-URL callbacks return borrowed data that is
 *   consumed synchronously by the native adapter.
 * @author Kambiz Asadzadeh
 * @since 12 Nov 2026
 * @copyright Copyright (c) 2026 Genyleap.
 * @license See LICENSE in repository root (GPL-3.0-only).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GenyMacBrowser GenyMacBrowser;

typedef enum GenyMacBrowserMode {
    GENY_MAC_WINDOW = 0,
    GENY_MAC_BOTTOM = 1,
    GENY_MAC_LEFT = 2,
    GENY_MAC_RIGHT = 3
} GenyMacBrowserMode;

typedef enum GenyMacBrowserRole {
    GENY_MAC_MINIAPP = 0,
    GENY_MAC_MEDIA = 1
} GenyMacBrowserRole;

typedef enum GenyMacBrowserEvent {
    GENY_MAC_CLOSED,
    GENY_MAC_RETRY,
    GENY_MAC_MINIMIZE,
    GENY_MAC_ACTIVATED,
    GENY_MAC_SHARE,
    GENY_MAC_NAVIGATION_STARTED
} GenyMacBrowserEvent;

typedef struct GenyMacBrowserCallbacks {
    void* context;
    void (*event)(void* context, GenyMacBrowserEvent event);
    void (*message)(void* context, const char* json, const char* document);
    void (*failed)(void* context, const char* translation_id);
    int (*allowed_url)(void* context, const char* url);
    const char* (*localize)(void* context, const char* translation_id);
    const char* (*youtube_watch_url)(void* context, const char* url);
} GenyMacBrowserCallbacks;

/* A null return indicates native host creation failed. The handle is owned by
   the caller, who must call geny_mac_browser_destroy once on the GUI thread.
   Native callbacks must not outlive this handle. */
GenyMacBrowser* geny_mac_browser_create(
    const char* title, const char* origin, GenyMacBrowserMode mode,
    int reduce_motion, int initially_visible, GenyMacBrowserRole role,
    GenyMacBrowserCallbacks callbacks);
int geny_mac_browser_valid(GenyMacBrowser* browser);
void geny_mac_browser_destroy(GenyMacBrowser* browser);
void geny_mac_browser_load(GenyMacBrowser* browser, const char* url, const char* referrer);
void geny_mac_browser_status(GenyMacBrowser* browser, const char* message, int failed);
void geny_mac_browser_ready(GenyMacBrowser* browser);
void geny_mac_browser_set_back(GenyMacBrowser* browser, int visible);
void geny_mac_browser_evaluate(GenyMacBrowser* browser, const char* script);
void geny_mac_browser_set_visible(GenyMacBrowser* browser, int visible);

#ifdef __cplusplus
}
#endif
