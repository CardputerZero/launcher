/*
 * ui_loading.h
 *
 * Global "Loading..." overlay used to give instant visual feedback when
 * the user activates an app entry on the launcher home.
 *
 * The overlay is a lazily-created child of lv_layer_top(), so it floats
 * above any screen and survives lv_disp_load_scr() transitions. It is
 * never deleted — only toggled via LV_OBJ_FLAG_HIDDEN — which mirrors
 * the pattern established by ui_global_hint.cpp.
 *
 * Typical usage from the launcher:
 *
 *   ui_loading_show("Loading...");
 *   lv_refr_now(NULL);   // force the overlay to paint *before* the
 *                        //   (possibly slow) page construction below
 *   auto p = std::make_shared<PageT>();
 *   lv_disp_load_scr(p->get_ui());
 *   ui_loading_hide();
 */
#ifndef UI_LOADING_H
#define UI_LOADING_H

#ifdef __cplusplus
extern "C" {
#endif

/* Show the loading overlay with the given label. Idempotent: calling
 * repeatedly just updates the text. Safe to call before LVGL is fully
 * initialised (no-op if lv_layer_top() is not yet available).
 */
void ui_loading_show(const char *label);

/* Hide the loading overlay. Safe to call when not shown (no-op). */
void ui_loading_hide(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_LOADING_H */
