#ifndef UI_MODEL_H
#define UI_MODEL_H

#include "ui_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Light off at 0 (remembered 80), AC off at 25, no room reading, 115200 baud,
 * no pending baud request and no selected scene. */
void ui_model_init(ui_model_t *m);

/* Validates, then applies. Invalid input leaves every byte unchanged. A valid
 * unchanged command is accepted. LIGHT keeps both supplied fields (power 0 with
 * a stored brightness is valid). BAUD records an already applied rate; the UI
 * owns pending requests. Scenes: Home light 80 % and AC 24, Sleep light off and
 * AC 26, Away both off. A device change that no longer matches the selected
 * scene clears the selection. */
bool ui_model_apply(ui_model_t *m, const studio_cmd_t *cmd);

/* Local switch: off keeps brightness; on restores the last nonzero value. */
bool ui_model_light_toggle(ui_model_t *m);

bool ui_model_baud_supported(uint32_t baud);

#ifdef __cplusplus
}
#endif

#endif /* UI_MODEL_H */
