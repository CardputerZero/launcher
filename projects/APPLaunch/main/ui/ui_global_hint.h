/*
 * ui_global_hint.h
 *
 * Global on-screen hint/toast overlay for the launcher and any sub-app
 * page. Shows a short, transient banner near the top of the active
 * screen when specific keys are pressed.
 *
 * Hooked from the cp0_lvgl keyboard dispatch after LV_EVENT_KEYBOARD
 * has been sent to the active screen.
 * The helper only READS elm — it never frees it.
 */
#ifndef UI_GLOBAL_HINT_H
#define UI_GLOBAL_HINT_H

struct key_item;

#ifdef __cplusplus
namespace ui_global_hint {
void on_key(const struct key_item *elm);
}

extern "C" {
#endif

/* Call on every key_item dequeued from the keyboard queue.
 * Decides whether to show a transient toast hint; a no-op for
 * keys that don't match the rules.
 */
void ui_global_hint_on_key(const struct key_item *elm);

#ifdef __cplusplus
}
#endif

#endif /* UI_GLOBAL_HINT_H */
