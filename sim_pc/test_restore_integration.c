/* =========================================================================
 * test_restore_integration.c
 *
 * Compiles and validates the Flash/NVS restore integration example from
 * docs/user/SMART_HUB_UI_GUIDE.md section 8.4 against real project headers.
 * ========================================================================= */

#include "lvgl.h"
#include "ui.h"
#include "ui_types.h"
#include "ui_auto.h"
#include "ui_test_api.h"
#include "lora_comm.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#define NVS_MAX_REC_LEN 64

typedef enum {
    NVS_RECORD_KIND_UNKNOWN    = 0,
    NVS_RECORD_KIND_CURRENT_V2 = 1,
    NVS_RECORD_KIND_LEGACY_V1  = 2
} nvs_record_kind_t;

/* Preserves established channel defaults (Purifier, Fan, Humidifier, Desk Light) */
static uint8_t get_channel_default_preset(uint8_t channel_idx)
{
    switch (channel_idx) {
        case 0:  return UI_PRESET_PURIFIER;
        case 1:  return UI_PRESET_FAN;
        case 2:  return UI_PRESET_HUMIDIFIER;
        case 3:  return UI_PRESET_LIGHT;
        default: return UI_PRESET_GENERIC;
    }
}

/* Mock NVS storage */
static uint8_t s_mock_storage[UI_DEVICE_COUNT][NVS_MAX_REC_LEN];
static size_t s_mock_storage_len[UI_DEVICE_COUNT];
static nvs_record_kind_t s_mock_storage_kind[UI_DEVICE_COUNT];
static bool s_mock_storage_exists[UI_DEVICE_COUNT];

static bool nvs_read_device_record(uint8_t idx, uint8_t *buf, size_t max_len, size_t *out_len, nvs_record_kind_t *out_kind)
{
    if (idx >= UI_DEVICE_COUNT || !s_mock_storage_exists[idx]) return false;
    size_t len = s_mock_storage_len[idx];
    if (len > max_len) len = max_len;
    memcpy(buf, s_mock_storage[idx], len);
    *out_len = len;
    *out_kind = s_mock_storage_kind[idx];
    return true;
}

/* Exact copy of the documented integration adapter */
static void config_load_from_flash(void)
{
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        uint8_t raw_buf[NVS_MAX_REC_LEN];
        size_t rec_len = 0;
        nvs_record_kind_t kind = NVS_RECORD_KIND_UNKNOWN;

        if (!nvs_read_device_record(i, raw_buf, sizeof(raw_buf), &rec_len, &kind)) {
            ui_hub_device_config_t def_cfg;
            ui_auto_get_defaults(i, get_channel_default_preset(i), &def_cfg);
            def_cfg.mode = UI_MODE_MANUAL;
            ui_set_device_config(i, &def_cfg);
            continue;
        }

        ui_hub_device_config_t cand;
        bool candidate_ready = false;

        if (kind == NVS_RECORD_KIND_CURRENT_V2 && rec_len == sizeof(ui_hub_device_config_t)) {
            if (raw_buf[offsetof(ui_hub_device_config_t, schema_version)] == UI_CONFIG_SCHEMA_VERSION) {
                memcpy(&cand, raw_buf, sizeof(ui_hub_device_config_t));
                candidate_ready = true;
            }
        } else if (kind == NVS_RECORD_KIND_LEGACY_V1 && rec_len == sizeof(ui_hub_device_config_v1_t)) {
            ui_hub_device_config_v1_t leg;
            memcpy(&leg, raw_buf, sizeof(leg));
            if (ui_auto_migrate_legacy_config(&leg, sizeof(leg), &cand)) {
                candidate_ready = true;
            }
        }

        if (!candidate_ready || !ui_set_device_config(i, &cand)) {
            ui_hub_device_config_t def_cfg;
            ui_auto_get_defaults(i, get_channel_default_preset(i), &def_cfg);
            def_cfg.mode = UI_MODE_MANUAL;
            ui_set_device_config(i, &def_cfg);
        }
    }
}

static uint32_t s_cb_count = 0;
static void test_config_cb(uint8_t dev_idx, const ui_hub_device_config_t *cfg)
{
    (void)dev_idx;
    (void)cfg;
    s_cb_count++;
}

/* Pre-ui_init fixture: called BEFORE ui_init() is called, with s_screen == NULL */
bool test_restore_pre_init_fixture(void)
{
    ui_set_config_changed_cb(test_config_cb);
    s_cb_count = 0;

    /* Setup mock storage:
     * Ch 0: Missing
     * Ch 1: Current V2, Living Fan, Auto, 1200/800, active_low=1
     * Ch 2: Legacy V1, Bed Humid, Humidifier, Auto, active_low=0
     * Ch 3: Truncated
     */
    s_mock_storage_exists[0] = false;

    s_mock_storage_exists[1] = true;
    s_mock_storage_kind[1] = NVS_RECORD_KIND_CURRENT_V2;
    ui_hub_device_config_t cur_cfg = {
        .name = "Living Fan",
        .preset = UI_PRESET_FAN,
        .mode = UI_MODE_AUTO,
        .active_low = 1,
        .schema_version = UI_CONFIG_SCHEMA_VERSION,
        .auto_on_thresh = 1200,
        .auto_off_thresh = 800
    };
    memcpy(s_mock_storage[1], &cur_cfg, sizeof(cur_cfg));
    s_mock_storage_len[1] = sizeof(cur_cfg);

    s_mock_storage_exists[2] = true;
    s_mock_storage_kind[2] = NVS_RECORD_KIND_LEGACY_V1;
    ui_hub_device_config_v1_t leg_cfg;
    memset(&leg_cfg, 0, sizeof(leg_cfg));
    strncpy(leg_cfg.name, "Bed Humid", sizeof(leg_cfg.name) - 1);
    leg_cfg.preset = UI_PRESET_HUMIDIFIER;
    leg_cfg.mode = UI_MODE_AUTO;
    leg_cfg.active_low = 0;
    memcpy(s_mock_storage[2], &leg_cfg, sizeof(leg_cfg));
    s_mock_storage_len[2] = sizeof(leg_cfg);

    s_mock_storage_exists[3] = true;
    s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
    memcpy(s_mock_storage[3], &cur_cfg, 20);
    s_mock_storage_len[3] = 20;

    /* Call documented adapter directly before ui_init */
    config_load_from_flash();

    /* Assert 0 callbacks triggered */
    if (s_cb_count != 0) {
        fprintf(stderr, "FAIL: pre-init config_load_from_flash triggered %u callbacks\n", (unsigned)s_cb_count);
        return false;
    }

    /* Assert Ch 1 config matches Living Fan, Auto, 1200/800, active_low=1 */
    const ui_hub_device_config_t *c1 = ui_get_device_config(1);
    if (strcmp(c1->name, "Living Fan") != 0 || c1->preset != UI_PRESET_FAN ||
        c1->mode != UI_MODE_AUTO || c1->active_low != 1 ||
        c1->auto_on_thresh != 1200 || c1->auto_off_thresh != 800) {
        fprintf(stderr, "FAIL: pre-init Ch 1 config mismatch\n");
        return false;
    }

    /* Assert initial TX is blocked */
    if (ui_is_tx_ready()) {
        fprintf(stderr, "FAIL: pre-init ui_is_tx_ready was true before init/link\n");
        return false;
    }

    /* X3: Keep restored configs in place so ui_init() boots with them */
    return true;
}

/* Post-ui_init fixture assertion: called immediately AFTER ui_init() */
bool test_restore_post_init_assertion(void)
{
    /* 1. Assert 0 callbacks occurred during ui_init() */
    if (s_cb_count != 0) {
        fprintf(stderr, "FAIL: ui_init() triggered %u callbacks during initialization (expected 0)\n", (unsigned)s_cb_count);
        return false;
    }

    /* 2. Assert Channel 1 config survived ui_init() without being overwritten */
    const ui_hub_device_config_t *c1 = ui_get_device_config(1);
    if (strcmp(c1->name, "Living Fan") != 0 || c1->preset != UI_PRESET_FAN ||
        c1->mode != UI_MODE_AUTO || c1->active_low != 1 ||
        c1->auto_on_thresh != 1200 || c1->auto_off_thresh != 800) {
        fprintf(stderr, "FAIL: Ch 1 restored config did NOT survive ui_init()\n");
        return false;
    }

    /* 3. Assert Channel 2 legacy config migrated to Manual survived ui_init() */
    const ui_hub_device_config_t *c2 = ui_get_device_config(2);
    if (strcmp(c2->name, "Bed Humid") != 0 || c2->preset != UI_PRESET_HUMIDIFIER ||
        c2->mode != UI_MODE_MANUAL || c2->active_low != 0 ||
        c2->auto_on_thresh != 40 || c2->auto_off_thresh != 50) {
        fprintf(stderr, "FAIL: Ch 2 legacy migrated config did NOT survive ui_init()\n");
        return false;
    }

    /* 4. Assert Channel 0 (missing record) defaulted to Purifier, Manual */
    const ui_hub_device_config_t *c0 = ui_get_device_config(0);
    if (c0->preset != UI_PRESET_PURIFIER || c0->mode != UI_MODE_MANUAL) {
        fprintf(stderr, "FAIL: Ch 0 missing record did not yield Purifier Manual\n");
        return false;
    }

    /* 5. Assert initial TX remains blocked before first eligible report */
    sim_reset_tx_records();
    if (ui_is_tx_ready()) {
        fprintf(stderr, "FAIL: ui_is_tx_ready() was true immediately after ui_init() before first report\n");
        return false;
    }
    if (ui_is_tx_ready()) {
        sim_record_tx(lv_tick_get(), &lora_hub_cmd);
    }
    if (sim_get_tx_count() != 0) {
        fprintf(stderr, "FAIL: Outbound TX occurred before first eligible report\n");
        return false;
    }

    /* 6. Assert Auto hold is active on Auto channel */
    if (!ui_is_auto_hold_active(1)) {
        fprintf(stderr, "FAIL: Auto hold was not active on Ch 1 after boot\n");
        return false;
    }

    /* 7. Publish first eligible report: Node reports relay 1 is active */
    lora_node_status_init_header(&lora_node_status);
    lora_node_status.bits.co2_ppm = 420;
    lora_node_status.bits.co2_valid = 1;
    lora_node_status.bits.tvoc_ppb = 85;
    lora_node_status.bits.voc_valid = 1;
    lora_node_status.bits.temp_deci_c = 235;
    lora_node_status.bits.temp_valid = 1;
    lora_node_status.bits.humidity_pct = 48;
    lora_node_status.bits.humid_valid = 1;
    /* Node reports relay 1 active: for active_low=1, physical level 0 means relay energized */
    lora_node_status_set_relay_gpio(&lora_node_status, 0, 1);
    lora_node_status_set_relay_gpio(&lora_node_status, 1, 0); /* relay 1 on */
    lora_node_status_set_relay_gpio(&lora_node_status, 2, 1);
    lora_node_status_set_relay_gpio(&lora_node_status, 3, 1);
    lv_tick_inc(10u);
    uint32_t now = lv_tick_get();
    lora_last_rx_tick_ms = now;
    lora_rx_revision++;

    ui_tick();
    lv_timer_handler();

    /* Assert TX is now permitted */
    if (!ui_is_tx_ready()) {
        fprintf(stderr, "FAIL: ui_is_tx_ready() remained false after first eligible report\n");
        return false;
    }

    /* Integration loop step: Record first send through the TX loop */
    if (ui_is_tx_ready()) {
        sim_record_tx(now, &lora_hub_cmd);
    }
    if (sim_get_tx_count() != 1) {
        fprintf(stderr, "FAIL: First send was not recorded through TX loop\n");
        return false;
    }

    /* Assert first recorded packet adopts reported GPIOs without sending boot zero pulse */
    const sim_tx_record_t *first_tx = sim_get_tx_record(0);
    uint8_t adopted_mask = first_tx->relay_mask;
    uint8_t reported_mask = lora_node_status_get_relay_gpios_mask(&lora_node_status);
    if (adopted_mask != reported_mask) {
        fprintf(stderr, "FAIL: First recorded packet mask (0x%02X) did not adopt reported mask (0x%02X)\n",
                adopted_mask, reported_mask);
        return false;
    }
    if (lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd) != reported_mask) {
        fprintf(stderr, "FAIL: Outbound command mask (0x%02X) did not adopt reported mask (0x%02X)\n",
                lora_hub_cmd_get_relay_gpios_mask(&lora_hub_cmd), reported_mask);
        return false;
    }

    /* Restore clean defaults for subsequent regression checks */
    ui_test_reset_device_configs();
    ui_test_reset_device_commands();
    return true;
}

bool test_restore_integration_suite(void)
{
    ui_set_config_changed_cb(test_config_cb);
    s_cb_count = 0;

    /* Case 1: Channel 0 missing record -> defaults to Purifier, Manual */
    s_mock_storage_exists[0] = false;

    /* Case 2: Channel 1 valid current v2 record -> Fan, Auto, 1200/800 */
    s_mock_storage_exists[1] = true;
    s_mock_storage_kind[1] = NVS_RECORD_KIND_CURRENT_V2;
    ui_hub_device_config_t cur_cfg = {
        .name = "Living Fan",
        .preset = UI_PRESET_FAN,
        .mode = UI_MODE_AUTO,
        .active_low = 1,
        .schema_version = UI_CONFIG_SCHEMA_VERSION,
        .auto_on_thresh = 1200,
        .auto_off_thresh = 800
    };
    memcpy(s_mock_storage[1], &cur_cfg, sizeof(cur_cfg));
    s_mock_storage_len[1] = sizeof(cur_cfg);

    /* Case 3: Channel 2 valid legacy v1 record -> Humidifier, migrates to Manual, defaults 40/50 */
    s_mock_storage_exists[2] = true;
    s_mock_storage_kind[2] = NVS_RECORD_KIND_LEGACY_V1;
    ui_hub_device_config_v1_t leg_cfg;
    memset(&leg_cfg, 0, sizeof(leg_cfg));
    strncpy(leg_cfg.name, "Bed Humid", sizeof(leg_cfg.name) - 1);
    leg_cfg.preset = UI_PRESET_HUMIDIFIER;
    leg_cfg.mode = UI_MODE_AUTO; /* Legacy Auto must be downgraded to Manual by migration */
    leg_cfg.active_low = 0;
    memcpy(s_mock_storage[2], &leg_cfg, sizeof(leg_cfg));
    s_mock_storage_len[2] = sizeof(leg_cfg);

    /* Case 4: Channel 3 truncated current v2 record (e.g. 20 bytes instead of 36) -> rejected, defaults to Light, Manual */
    s_mock_storage_exists[3] = true;
    s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
    memcpy(s_mock_storage[3], &cur_cfg, 20);
    s_mock_storage_len[3] = 20;

    /* Run integration restore under boot staging semantics */
    ui_test_set_boot_staging(true);
    config_load_from_flash();
    ui_test_set_boot_staging(false);

    /* Assert zero callbacks triggered during restore */
    if (s_cb_count != 0) {
        fprintf(stderr, "FAIL: config_load_from_flash triggered %u callbacks (expected 0)\n", (unsigned)s_cb_count);
        return false;
    }

    /* Verify Channel 0: Purifier, Manual, defaults */
    const ui_hub_device_config_t *c0 = ui_get_device_config(0);
    if (c0->preset != UI_PRESET_PURIFIER || c0->mode != UI_MODE_MANUAL) {
        fprintf(stderr, "FAIL: Ch 0 missing record did not yield Purifier Manual\n");
        return false;
    }

    /* Verify Channel 1: Living Fan, Auto, 1200/800, active_low=1 */
    const ui_hub_device_config_t *c1 = ui_get_device_config(1);
    if (strcmp(c1->name, "Living Fan") != 0 || c1->preset != UI_PRESET_FAN ||
        c1->mode != UI_MODE_AUTO || c1->active_low != 1 ||
        c1->auto_on_thresh != 1200 || c1->auto_off_thresh != 800) {
        fprintf(stderr, "FAIL: Ch 1 valid v2 record restore mismatch\n");
        return false;
    }

    /* Verify Channel 2: Bed Humid, Humidifier, downgraded to Manual, active_low=0, canonical limits */
    const ui_hub_device_config_t *c2 = ui_get_device_config(2);
    if (strcmp(c2->name, "Bed Humid") != 0 || c2->preset != UI_PRESET_HUMIDIFIER ||
        c2->mode != UI_MODE_MANUAL || c2->active_low != 0 ||
        c2->auto_on_thresh != 40 || c2->auto_off_thresh != 50) {
        fprintf(stderr, "FAIL: Ch 2 legacy v1 migration mismatch\n");
        return false;
    }

    /* Verify Channel 3: Truncated v2 record was rejected and fell back to Desk Light, Manual */
    const ui_hub_device_config_t *c3 = ui_get_device_config(3);
    if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
        fprintf(stderr, "FAIL: Ch 3 truncated v2 record did not fall back to Desk Light Manual\n");
        return false;
    }

    /* Comprehensive Truncation Matrix for V2 (all lengths 0..sizeof(ui_hub_device_config_t)-1) */
    for (size_t len = 0; len < sizeof(ui_hub_device_config_t); len++) {
        s_mock_storage_exists[3] = true;
        s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
        if (len > 0) memcpy(s_mock_storage[3], &cur_cfg, len);
        s_mock_storage_len[3] = len;
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c3 = ui_get_device_config(3);
        if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
            fprintf(stderr, "FAIL: Truncated V2 length %u was not rejected\n", (unsigned)len);
            return false;
        }
    }

    /* Comprehensive Truncation Matrix for V1 Legacy (all lengths 0..sizeof(ui_hub_device_config_v1_t)-1) */
    for (size_t len = 0; len < sizeof(ui_hub_device_config_v1_t); len++) {
        s_mock_storage_exists[2] = true;
        s_mock_storage_kind[2] = NVS_RECORD_KIND_LEGACY_V1;
        if (len > 0) memcpy(s_mock_storage[2], &leg_cfg, len);
        s_mock_storage_len[2] = len;
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c2 = ui_get_device_config(2);
        if (c2->preset != UI_PRESET_HUMIDIFIER || c2->mode != UI_MODE_MANUAL) {
            fprintf(stderr, "FAIL: Truncated V1 length %u was not rejected\n", (unsigned)len);
            return false;
        }
    }

    /* Unknown/Wrong Record Kind & Schema Version Matrix */
    const nvs_record_kind_t wrong_kinds[] = { NVS_RECORD_KIND_UNKNOWN, (nvs_record_kind_t)99 };
    for (size_t k = 0; k < sizeof(wrong_kinds)/sizeof(wrong_kinds[0]); k++) {
        s_mock_storage_exists[3] = true;
        s_mock_storage_kind[3] = wrong_kinds[k];
        s_mock_storage_len[3] = sizeof(ui_hub_device_config_t);
        memcpy(s_mock_storage[3], &cur_cfg, sizeof(cur_cfg));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c3 = ui_get_device_config(3);
        if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
            fprintf(stderr, "FAIL: Wrong kind %u was not rejected\n", (unsigned)wrong_kinds[k]);
            return false;
        }
    }

    const uint8_t wrong_versions[] = { 0, 1, 99 };
    for (size_t v = 0; v < sizeof(wrong_versions)/sizeof(wrong_versions[0]); v++) {
        ui_hub_device_config_t bad_ver = cur_cfg;
        bad_ver.schema_version = wrong_versions[v];
        s_mock_storage_exists[3] = true;
        s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
        s_mock_storage_len[3] = sizeof(bad_ver);
        memcpy(s_mock_storage[3], &bad_ver, sizeof(bad_ver));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c3 = ui_get_device_config(3);
        if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
            fprintf(stderr, "FAIL: Wrong schema version %u was not rejected\n", (unsigned)wrong_versions[v]);
            return false;
        }
    }

    /* Malformed Presets & Modes Matrix */
    const uint8_t bad_presets[] = { UI_PRESET_COUNT, 9, 255 };
    for (size_t p = 0; p < sizeof(bad_presets)/sizeof(bad_presets[0]); p++) {
        ui_hub_device_config_t bad_p = cur_cfg;
        bad_p.preset = bad_presets[p];
        s_mock_storage_exists[3] = true;
        s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
        s_mock_storage_len[3] = sizeof(bad_p);
        memcpy(s_mock_storage[3], &bad_p, sizeof(bad_p));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c3 = ui_get_device_config(3);
        if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
            fprintf(stderr, "FAIL: Bad preset %u was not rejected\n", (unsigned)bad_presets[p]);
            return false;
        }
    }

    /* Empty Name Matrix */
    ui_hub_device_config_t bad_name = cur_cfg;
    bad_name.name[0] = '\0';
    s_mock_storage_exists[3] = true;
    s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
    s_mock_storage_len[3] = sizeof(bad_name);
    memcpy(s_mock_storage[3], &bad_name, sizeof(bad_name));
    ui_test_set_boot_staging(true);
    config_load_from_flash();
    ui_test_set_boot_staging(false);
    c3 = ui_get_device_config(3);
    if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
        fprintf(stderr, "FAIL: Empty name was not rejected\n");
        return false;
    }

    /* Unterminated Name Matrix */
    memset(bad_name.name, 'A', sizeof(bad_name.name));
    s_mock_storage_exists[3] = true;
    s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
    s_mock_storage_len[3] = sizeof(bad_name);
    memcpy(s_mock_storage[3], &bad_name, sizeof(bad_name));
    ui_test_set_boot_staging(true);
    config_load_from_flash();
    ui_test_set_boot_staging(false);
    c3 = ui_get_device_config(3);
    if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
        fprintf(stderr, "FAIL: Unterminated name was not rejected\n");
        return false;
    }

    /* Invalid Modes Matrix */
    const uint8_t bad_modes[] = { 2, 3, 99, 255 };
    for (size_t m = 0; m < sizeof(bad_modes)/sizeof(bad_modes[0]); m++) {
        ui_hub_device_config_t bad_m = cur_cfg;
        bad_m.mode = bad_modes[m];
        s_mock_storage_exists[3] = true;
        s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
        s_mock_storage_len[3] = sizeof(bad_m);
        memcpy(s_mock_storage[3], &bad_m, sizeof(bad_m));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c3 = ui_get_device_config(3);
        if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
            fprintf(stderr, "FAIL: Bad mode %u was not rejected\n", (unsigned)bad_modes[m]);
            return false;
        }
    }

    /* Invalid Polarities Matrix */
    const uint8_t bad_pols[] = { 2, 99, 255 };
    for (size_t pol = 0; pol < sizeof(bad_pols)/sizeof(bad_pols[0]); pol++) {
        ui_hub_device_config_t bad_pol = cur_cfg;
        bad_pol.active_low = bad_pols[pol];
        s_mock_storage_exists[3] = true;
        s_mock_storage_kind[3] = NVS_RECORD_KIND_CURRENT_V2;
        s_mock_storage_len[3] = sizeof(bad_pol);
        memcpy(s_mock_storage[3], &bad_pol, sizeof(bad_pol));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c3 = ui_get_device_config(3);
        if (c3->preset != UI_PRESET_LIGHT || c3->mode != UI_MODE_MANUAL) {
            fprintf(stderr, "FAIL: Bad polarity %u was not rejected\n", (unsigned)bad_pols[pol]);
            return false;
        }
    }

    /* Malformed Thresholds Matrix across all 3 Auto Rules */
    /* Fan: ON < OFF + 50 (e.g. 800/1000, 1000/1000, out-of-bounds 60050) */
    ui_hub_device_config_t bad_fan = cur_cfg;
    bad_fan.preset = UI_PRESET_FAN;
    bad_fan.mode = UI_MODE_AUTO;
    const int32_t fan_pairs[][2] = { {800, 1000}, {1000, 1000}, {60050, 800} };
    for (size_t f = 0; f < sizeof(fan_pairs)/sizeof(fan_pairs[0]); f++) {
        bad_fan.auto_on_thresh = fan_pairs[f][0];
        bad_fan.auto_off_thresh = fan_pairs[f][1];
        s_mock_storage_exists[1] = true;
        s_mock_storage_kind[1] = NVS_RECORD_KIND_CURRENT_V2;
        s_mock_storage_len[1] = sizeof(bad_fan);
        memcpy(s_mock_storage[1], &bad_fan, sizeof(bad_fan));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c1 = ui_get_device_config(1);
        if (c1->mode != UI_MODE_MANUAL || c1->auto_on_thresh != 1000 || c1->auto_off_thresh != 800) {
            fprintf(stderr, "FAIL: Bad Fan threshold pair (%d, %d) was not rejected to defaults\n",
                    (int)fan_pairs[f][0], (int)fan_pairs[f][1]);
            return false;
        }
    }

    /* Purifier: ON < OFF + 5 (e.g. 100/100, 0/100, out-of-bounds 60005) */
    ui_hub_device_config_t bad_pur;
    ui_auto_get_defaults(0, UI_PRESET_PURIFIER, &bad_pur);
    bad_pur.mode = UI_MODE_AUTO;
    const int32_t pur_pairs[][2] = { {100, 100}, {0, 100}, {60005, 100} };
    for (size_t f = 0; f < sizeof(pur_pairs)/sizeof(pur_pairs[0]); f++) {
        bad_pur.auto_on_thresh = pur_pairs[f][0];
        bad_pur.auto_off_thresh = pur_pairs[f][1];
        s_mock_storage_exists[0] = true;
        s_mock_storage_kind[0] = NVS_RECORD_KIND_CURRENT_V2;
        s_mock_storage_len[0] = sizeof(bad_pur);
        memcpy(s_mock_storage[0], &bad_pur, sizeof(bad_pur));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c0 = ui_get_device_config(0);
        if (c0->mode != UI_MODE_MANUAL || c0->auto_on_thresh != 150 || c0->auto_off_thresh != 100) {
            fprintf(stderr, "FAIL: Bad Purifier threshold pair (%d, %d) was not rejected to defaults\n",
                    (int)pur_pairs[f][0], (int)pur_pairs[f][1]);
            return false;
        }
    }

    /* Humidifier: OFF < ON + 5 (e.g. ON=50, OFF=40; OFF=105) */
    ui_hub_device_config_t bad_hum;
    ui_auto_get_defaults(2, UI_PRESET_HUMIDIFIER, &bad_hum);
    bad_hum.mode = UI_MODE_AUTO;
    const int32_t hum_pairs[][2] = { {50, 40}, {40, 40}, {40, 105} };
    for (size_t f = 0; f < sizeof(hum_pairs)/sizeof(hum_pairs[0]); f++) {
        bad_hum.auto_on_thresh = hum_pairs[f][0];
        bad_hum.auto_off_thresh = hum_pairs[f][1];
        s_mock_storage_exists[2] = true;
        s_mock_storage_kind[2] = NVS_RECORD_KIND_CURRENT_V2;
        s_mock_storage_len[2] = sizeof(bad_hum);
        memcpy(s_mock_storage[2], &bad_hum, sizeof(bad_hum));
        ui_test_set_boot_staging(true);
        config_load_from_flash();
        ui_test_set_boot_staging(false);
        c2 = ui_get_device_config(2);
        if (c2->mode != UI_MODE_MANUAL || c2->auto_on_thresh != 40 || c2->auto_off_thresh != 50) {
            fprintf(stderr, "FAIL: Bad Humidifier threshold pair (%d, %d) was not rejected to defaults\n",
                    (int)hum_pairs[f][0], (int)hum_pairs[f][1]);
            return false;
        }
    }
    /* Test runtime commit triggers exactly 1 callback */
    s_cb_count = 0;
    ui_hub_device_config_t run_cand = *ui_get_device_config(1);
    run_cand.auto_on_thresh += 50;
    if (!ui_set_device_config(1, &run_cand)) {
        fprintf(stderr, "FAIL: Valid runtime threshold commit was rejected\n");
        return false;
    }
    if (s_cb_count != 1) {
        fprintf(stderr, "FAIL: Runtime commit triggered %u callbacks (expected 1)\n", (unsigned)s_cb_count);
        return false;
    }

    /* Test no-op save triggers 0 callbacks */
    s_cb_count = 0;
    if (!ui_set_device_config(1, &run_cand)) {
        fprintf(stderr, "FAIL: No-op runtime save was rejected\n");
        return false;
    }
    if (s_cb_count != 0) {
        fprintf(stderr, "FAIL: No-op runtime save triggered %u callbacks (expected 0)\n", (unsigned)s_cb_count);
        return false;
    }

    /* Test rejected invalid edit triggers 0 callbacks */
    s_cb_count = 0;
    ui_hub_device_config_t bad_cand = run_cand;
    bad_cand.auto_on_thresh = 500; /* invalid: 500 < 800 */
    if (ui_set_device_config(1, &bad_cand)) {
        fprintf(stderr, "FAIL: Invalid threshold edit was accepted at runtime\n");
        return false;
    }
    if (s_cb_count != 0) {
        fprintf(stderr, "FAIL: Rejected edit triggered %u callbacks (expected 0)\n", (unsigned)s_cb_count);
        return false;
    }

    /* Outbound TX gating check: TX must not be ready before ui_tick reconciliation */
    uint32_t saved_rx = lora_last_rx_tick_ms;
    uint32_t now_gt = lv_tick_get();
    lora_last_rx_tick_ms = (now_gt > 6000u) ? (now_gt - 6000u) : 0;
    ui_tick();
    if (ui_is_tx_ready()) {
        fprintf(stderr, "FAIL: ui_is_tx_ready() was unexpectedly true while disconnected\n");
        return false;
    }
    lora_last_rx_tick_ms = lv_tick_get();
    lora_rx_revision++;
    ui_tick();
    if (!ui_is_tx_ready()) {
        fprintf(stderr, "FAIL: ui_is_tx_ready() was false after reconciliation\n");
        return false;
    }

    /* Reset defaults for subsequent regression tests */
    for (uint8_t i = 0; i < UI_DEVICE_COUNT; i++) {
        ui_hub_device_config_t def;
        ui_auto_get_defaults(i, (i == 3) ? UI_PRESET_LIGHT : i, &def);
        ui_set_device_config(i, &def);
    }
    lora_last_rx_tick_ms = saved_rx;
    ui_test_reset_device_commands();
    ui_set_config_changed_cb(NULL);
    return true;
}
