#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* Build the intentionally minimal Smart Hub placeholder screen. */
void ui_init(void);

/* Application-context hook for future bounded telemetry/update lanes. */
void ui_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
