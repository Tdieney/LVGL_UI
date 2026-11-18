#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include "studio_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_PAGE_LIGHT = 0,
    UI_PAGE_AC = 1,
    UI_PAGE_TEMPERATURE = 2,
    UI_PAGE_HUMIDITY = 3,
    UI_PAGE_COUNT = 4
} ui_page_id_t;

typedef enum {
    UI_BAUD_IDLE = 0,
    UI_BAUD_PENDING,
    UI_BAUD_FAILED
} ui_baud_state_t;

/* Everything the UI shows. Owned by the UI; read it through ui_get_model(). */
typedef struct {
    int light_power;               /* 0/1 as last applied */
    int light_bright;              /* 0..100; effective output is 0 while off */
    int light_last_nonzero_bright; /* restored when the switch turns the light on */
    int ac_power;                  /* 0/1 */
    int ac_temp;                   /* 16..30, kept while off */
    int16_t env_temp;              /* whole deg C */
    uint8_t env_humid;             /* 0..100 whole % */
    bool env_valid;
    uint32_t applied_baud;
    uint32_t pending_baud;
    ui_baud_state_t baud_state;
    studio_scene_t selected_scene;
} ui_model_t;

/* Called synchronously in GUI context with a borrowed command. Copy or queue
 * it before returning; never wait for the UART or parse JSON here. */
typedef void (*ui_action_cb_t)(const studio_cmd_t *cmd, void *context);

#ifdef __cplusplus
}
#endif

#endif /* UI_TYPES_H */
