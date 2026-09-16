/* Minimal syntax reproduction of the guide's two legacy-restore expressions. */
#include "ui_auto.h"
void verify_guide_api(void) {
    ui_hub_device_config_v1_t leg = {0};
    ui_hub_device_config_t cand;
    (void)offsetof(ui_hub_device_config_v1_t, schema_version);
    ui_auto_migrate_legacy_config(&leg, &cand);
}
