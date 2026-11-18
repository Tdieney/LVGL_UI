#include "ui_model.h"

#include <string.h>

static const uint32_t s_baud_rates[STUDIO_BAUD_COUNT] = STUDIO_BAUD_RATES;

static bool scene_matches(const ui_model_t *m)
{
    switch (m->selected_scene) {
    case STUDIO_SCENE_HOME:
        return m->light_power == 1 && m->light_bright == 80 &&
               m->ac_power == 1 && m->ac_temp == 24;
    case STUDIO_SCENE_SLEEP:
        return m->light_power == 0 && m->ac_power == 1 && m->ac_temp == 26;
    case STUDIO_SCENE_AWAY:
        return m->light_power == 0 && m->ac_power == 0;
    default:
        return false;
    }
}

static void clear_diverged_scene(ui_model_t *m)
{
    if (!scene_matches(m)) {
        m->selected_scene = STUDIO_SCENE_NONE;
    }
}

static bool command_valid(const studio_cmd_t *cmd)
{
    switch (cmd->kind) {
    case STUDIO_CMD_LIGHT:
        return (cmd->data.light.power == 0 || cmd->data.light.power == 1) &&
               cmd->data.light.bright >= 0 && cmd->data.light.bright <= 100;
    case STUDIO_CMD_AC:
        return (cmd->data.ac.power == 0 || cmd->data.ac.power == 1) &&
               cmd->data.ac.temp >= 16 && cmd->data.ac.temp <= 30;
    case STUDIO_CMD_ENV:
        return cmd->data.env.temp >= INT16_MIN &&
               cmd->data.env.temp <= INT16_MAX &&
               cmd->data.env.humid >= 0 && cmd->data.env.humid <= 100;
    case STUDIO_CMD_SCENE:
        return cmd->data.scene == STUDIO_SCENE_HOME ||
               cmd->data.scene == STUDIO_SCENE_SLEEP ||
               cmd->data.scene == STUDIO_SCENE_AWAY;
    case STUDIO_CMD_BAUD:
        return ui_model_baud_supported(cmd->data.baud);
    default:
        return false;
    }
}

void ui_model_init(ui_model_t *m)
{
    if (!m) {
        return;
    }
    memset(m, 0, sizeof(*m));
    m->light_last_nonzero_bright = 80;
    m->ac_temp = 25;
    m->applied_baud = STUDIO_BAUD_DEFAULT;
}

bool ui_model_apply(ui_model_t *m, const studio_cmd_t *cmd)
{
    if (!m || !cmd || !command_valid(cmd)) {
        return false;
    }
    switch (cmd->kind) {
    case STUDIO_CMD_LIGHT:
        m->light_power = cmd->data.light.power;
        m->light_bright = cmd->data.light.bright;
        if (m->light_bright > 0) {
            m->light_last_nonzero_bright = m->light_bright;
        }
        clear_diverged_scene(m);
        break;
    case STUDIO_CMD_AC:
        m->ac_power = cmd->data.ac.power;
        m->ac_temp = cmd->data.ac.temp;
        clear_diverged_scene(m);
        break;
    case STUDIO_CMD_ENV:
        m->env_temp = (int16_t)cmd->data.env.temp;
        m->env_humid = (uint8_t)cmd->data.env.humid;
        m->env_valid = cmd->data.env.valid;
        break;
    case STUDIO_CMD_SCENE:
        if (cmd->data.scene == STUDIO_SCENE_HOME) {
            m->light_power = 1;
            m->light_bright = 80;
            m->light_last_nonzero_bright = 80;
            m->ac_power = 1;
            m->ac_temp = 24;
        } else if (cmd->data.scene == STUDIO_SCENE_SLEEP) {
            m->light_power = 0;
            m->ac_power = 1;
            m->ac_temp = 26;
        } else {
            m->light_power = 0;
            m->ac_power = 0;
        }
        m->selected_scene = cmd->data.scene;
        break;
    case STUDIO_CMD_BAUD:
        m->applied_baud = cmd->data.baud;
        break;
    default:
        break;
    }
    return true;
}

bool ui_model_light_toggle(ui_model_t *m)
{
    if (!m) {
        return false;
    }
    if (m->light_power) {
        m->light_power = 0;
    } else {
        m->light_power = 1;
        m->light_bright = m->light_last_nonzero_bright;
    }
    clear_diverged_scene(m);
    return true;
}

bool ui_model_baud_supported(uint32_t baud)
{
    for (unsigned i = 0; i < STUDIO_BAUD_COUNT; i++) {
        if (s_baud_rates[i] == baud) {
            return true;
        }
    }
    return false;
}
