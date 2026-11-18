#ifndef UI_H
#define UI_H

#include "lvgl.h"
#include "ui_types.h"
#include "studio_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Every function here belongs to the GUI context (the task that runs
 * lv_timer_handler()), never an ISR or the UART worker. Time comes from
 * lv_tick_get(); the UI has no second clock. */

/* Build the UI on the active screen. cb may be NULL (local actions are then
 * shown but not sent, and a baud request fails at once). */
void ui_init(ui_action_cb_t cb, void *context);

/* Apply a validated command from the PC. Returns false and changes nothing
 * for invalid input. Never calls the action callback. */
bool ui_apply(const studio_cmd_t *cmd);

/* Report the result of an LCD baud request. Only the active request's rate
 * completes; failure keeps the previously applied rate. */
bool ui_baud_complete(uint32_t requested_baud, bool success);

/* Call after every accepted message from the PC (Connection lights its RX dot). */
void ui_note_rx(void);

const ui_model_t *ui_get_model(void);

void ui_navigate_to_page(ui_page_id_t page);
ui_page_id_t ui_get_page(void);

/* Delete the UI and its timer. ui_init() may be called again afterwards. */
void ui_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
